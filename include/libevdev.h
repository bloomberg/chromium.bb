/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef _LIBEVDEV_H_
#define _LIBEVDEV_H_

#include <linux/input.h>
#include "libevdev_event.h"
#include "libevdev_mt.h"

#ifndef Success
// from X.h
#define Success 0
#endif

#define LONG_BITS (sizeof(long) * 8)
#define NLONGS(x) (((x) + LONG_BITS - 1) / LONG_BITS)

typedef void (*syn_report_callback)(void*, EventStatePtr, struct timeval*);
typedef void (*log_callback)(void*, int level, const char*, ...);

struct EvdevInfo_ {
  struct input_id id;
  char name[1024];

  unsigned long bitmask[NLONGS(EV_CNT)];
  unsigned long key_bitmask[NLONGS(KEY_CNT)];
  unsigned long rel_bitmask[NLONGS(REL_CNT)];
  unsigned long abs_bitmask[NLONGS(ABS_CNT)];
  unsigned long led_bitmask[NLONGS(LED_CNT)];
  struct input_absinfo absinfo[ABS_CNT];
  unsigned long prop_bitmask[NLONGS(INPUT_PROP_CNT)];
  int is_monotonic:1;
};
typedef struct EvdevInfo_ EvdevInfo, *EvdevInfoPtr;

struct Evdev_ {
  syn_report_callback syn_report;
  void* syn_report_udata;

  log_callback log;
  void* log_udata;

  EventStatePtr evstate;
  int fd;

  unsigned long key_state_bitmask[NLONGS(KEY_CNT)];
  EvdevInfo info;

  struct timeval before_sync_time;
  struct timeval after_sync_time;
};
typedef struct Evdev_ Evdev, *EvdevPtr;

int EvdevOpen(EvdevPtr, const char*);
int EvdevClose(EvdevPtr);
int EvdevRead(EvdevPtr);
int EvdevProbe(EvdevPtr);
int EvdevProbeAbsinfo(EvdevPtr device, size_t key);
int EvdevProbeMTSlot(EvdevPtr device, MTSlotInfoPtr req);
int EvdevProbeKeyState(EvdevPtr device);
int EvdevEnableMonotonic(EvdevPtr device);

#endif
