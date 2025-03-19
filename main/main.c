#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

#define TRIGGER_PIN   15
#define ECHO_PIN      14
#define SENSOR_TIMEOUT_US 30000

volatile bool measurement_in_progress = false;
volatile bool new_measurement_available = false;
volatile bool sensor_failure = false;
volatile uint32_t pulse_width_us = 0;
absolute_time_t echo_rise_time;
alarm_id_t sensor_alarm_id;

int64_t sensor_timeout_callback(alarm_id_t id, void *user_data) {
    if (measurement_in_progress) {
        sensor_failure = true;
        measurement_in_progress = false;
        new_measurement_available = true;
    }
    return 0;
}


void echo_gpio_callback(uint gpio, uint32_t events) {
    if (gpio_get(ECHO_PIN)) { 
        echo_rise_time = get_absolute_time();
        measurement_in_progress = true;
        sensor_failure = false;
        sensor_alarm_id = add_alarm_in_us(SENSOR_TIMEOUT_US, sensor_timeout_callback, NULL, true);
    } else { 
        if (measurement_in_progress) {
            absolute_time_t echo_fall_time = get_absolute_time();
            cancel_alarm(sensor_alarm_id);
            pulse_width_us = absolute_time_diff_us(echo_rise_time, echo_fall_time);
            measurement_in_progress = false;
            new_measurement_available = true;
        }
    }
}

int main(void) {
    stdio_init_all();
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_put(TRIGGER_PIN, 0);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_gpio_callback);

    while (1) {

        if (!measurement_in_progress && !new_measurement_available) {
            gpio_put(TRIGGER_PIN, 1);
            sleep_us(10);
            gpio_put(TRIGGER_PIN, 0);
        }

        while (!new_measurement_available) {
            sleep_ms(10);
        }

        if (sensor_failure) {
            printf("Falha\n");
        } else {
            int distance_cm = pulse_width_us / 58;
            printf("%d cm\n", distance_cm);
        }
        new_measurement_available = false;

        sleep_ms(1000);
    }

    return 0;
}
