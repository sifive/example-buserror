/* Copyright 2020 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */

#include <metal/cpu.h>
#include <metal/drivers/sifive_buserror0.h>
#include <metal/platform.h>
#include <stdio.h>

/* The sifive,error0 device is used to trigger a TileLink bus error for
 * testing the sifive,buserror0 device. If one isn't present, this example
 * will fail to compile because we have no way to trigger a bus error. */
#ifndef METAL_SIFIVE_ERROR0
#error Example requires a sifive,error0 device to drive bus errors
#endif

#define BADADDR METAL_SIFIVE_ERROR0_0_BASE_ADDRESS

volatile int local_int_handled = 0;

void metal_sifive_buserror0_source_0_handler(void) {
  struct metal_cpu cpu = metal_cpu_get(metal_cpu_get_current_hartid());
  sifive_buserror_event_t event = sifive_buserror_get_cause(cpu);

  if (event & METAL_BUSERROR_EVENT_ANY) {
    printf("Handled TileLink bus error\n");
    local_int_handled = 1;
    sifive_buserror_clear_event_accrued(cpu, METAL_BUSERROR_EVENT_ALL);
  }

  sifive_buserror_clear_cause(cpu);
}

int main() {
  struct metal_cpu cpu = metal_cpu_get(metal_cpu_get_current_hartid());

  /* Clear any accrued events and the cause register */
  sifive_buserror_clear_event_accrued(cpu, METAL_BUSERROR_EVENT_ALL);
  sifive_buserror_clear_cause(cpu);

  /* Enable all bus error events */
  sifive_buserror_enable_events(cpu, METAL_BUSERROR_EVENT_ALL);

  /* Trigger an error event */
  uint8_t bad = *((volatile uint8_t *)BADADDR);

  /* Fence above error event before check below */
  __asm__ ("fence");

  /* Check if the event is accrued and clear it*/
  int accrued = 0;
  if (sifive_buserror_is_event_accrued(cpu, METAL_BUSERROR_EVENT_ANY)) {
    printf("Detected accrued bus error\n");
    accrued = 1;
    sifive_buserror_clear_event_accrued(cpu, METAL_BUSERROR_EVENT_ALL);
    sifive_buserror_clear_cause(cpu);
  }
  if (!sifive_buserror_is_event_accrued(cpu, METAL_BUSERROR_EVENT_ANY)) {
    printf("Cleared accrued bus error\n");
  } else {
    printf("Failed to clear accrued bus error event\n");
    return 4;
  }

  /* Enable hart-local interrupt for error events */
  sifive_buserror_enable_local_interrupt(cpu, METAL_BUSERROR_EVENT_ALL);
  metal_cpu_enable_interrupts();

  /* Fence above error clear before trigger below */
  /* Fence above interrupt enable before trigger below */
  __asm__ ("fence");

  /* Trigger an error event */
  bad = *((volatile uint8_t *)BADADDR);

  /* Fence above error event before interrupt check below */
  __asm__ ("fence");
  __asm__ ("fence.i");

  if (!accrued) {
    return 5;
  }
  if (!local_int_handled) {
    return 6;
  }
  return 0;
}
