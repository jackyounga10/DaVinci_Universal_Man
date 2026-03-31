#include <LiquidCrystal_I2C.h>
#include <string.h>
#include <math.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

LiquidCrystal_I2C lcd(0x27, 20, 4);  // Initial setup for a 20 columns x 4 rows LCD with I2C address 0x27
// the setup function runs once when you press reset or power the board

const int total_samples = 4000;
signed char samples[total_samples];
int oldest_sample_index = total_samples-1;
int loop_cnt = 0;
const int display_frequency = 200;
const int num_weighted_average_samples = 10;
const float weighted_average_divisor = num_weighted_average_samples * (num_weighted_average_samples + 1) / 2;  // This is the n-th triangle of the weighted average
const int level_point = 503;
int last_proportion_error = 0;
const int pid_display_column_width = 2;


void setup() {
  lcd.init(); // Initialize the LCD display
  lcd.backlight(); // Turn on the backlight of the LCD
  Serial.begin(9600); // Initialize serial communication
}


signed char read_error() {
  float level_sensor = analogRead(A0);  // This is on a 0 to 1023 scale
  int raw_error = level_sensor - level_point;
  return MAX(-127, MIN(127, raw_error));  // This is now on a -127 to 127 scale so it fits in 8 bits
}


void store_error(unsigned char error) {
  // We store errors in a circular buffer for efficiency
  samples[oldest_sample_index] = error;
  if(oldest_sample_index == 0) {
    oldest_sample_index = total_samples-1;
  } else {
    oldest_sample_index--;
  }
}


// void print_error_history() {
//   char debug_string[100];
//   for(int i=0; i<total_samples; i++) {
//       snprintf(debug_string, sizeof(debug_string), "[%d]: %-10f ", i, samples[i]);
//       Serial.print(debug_string); 
//   }
//   Serial.print("\n"); 
//   delay(1);  
// }

void draw_display(float proportion_error, float derivative_error, float intergal_error, float volts) {
  char first_line[21];
  char second_line[21];
  int p = (int)round(proportion_error);
  int i = (int)round(intergal_error);
  int d = (int)round(derivative_error);
  snprintf(first_line, sizeof(first_line), "P:%03d I:%03d D:%03d", p, i, d, pid_display_column_width);
  snprintf(second_line, sizeof(second_line), "Volts:%5.2f", volts);
  lcd.clear();
  lcd.print(first_line);
  lcd.setCursor(0, 1); 
  lcd.print(second_line);
}

float proportion() {
  // Calculating this as the weighted average over the last N samples
  int sum = 0;
  int initial_weight = num_weighted_average_samples + 1;
  for (int i=1; i<=num_weighted_average_samples; i++) {
    sum += (initial_weight - i) * samples[(oldest_sample_index + i)  % total_samples];
  }
  return ((float)sum) / weighted_average_divisor;
}

float intergal() {
  long sum = 0;
  for (int i=0; i<=total_samples; i++) {
    sum += samples[i];
  }
  return (float)(((double)sum) / total_samples);
}


void loop() {
  Serial.print(loop_cnt);
  Serial.println();
  store_error(read_error());
  // Serial.print(read_error());
  // Serial.println();
  // print_error_history();
  float proportion_error = proportion();
  float derivative_error = last_proportion_error - proportion_error;
  last_proportion_error = proportion_error;
  float intergal_error = intergal();

  float p_weight = 1;
  float i_weight = 3;
  float d_weight = 0.5;
  if (abs(proportion_error < 10)) {
    p_weight = 1;
    i_weight = 3;
    d_weight = 0.5;
  }

  float total_error = (proportion_error * p_weight + intergal_error * i_weight + derivative_error * d_weight) / (p_weight + i_weight + d_weight);
  float volts = MAX(3, MIN(total_error/4 + 10, 24));
  int pwm_signal = round(volts * 255 / 24);
  
  analogWrite(3, pwm_signal);

  if (loop_cnt == display_frequency) {
    draw_display(proportion_error, derivative_error, intergal_error, volts);
    loop_cnt = 0;
  } else {
    loop_cnt++;
  }
}
