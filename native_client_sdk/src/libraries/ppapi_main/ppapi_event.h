// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef PPAPI_MAIN_PPAPI_EVENT_H_
#define PPAPI_MAIN_PPAPI_EVENT_H_

#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_touch_point.h"
#include "ppapi/c/ppb_input_event.h"

#include "utils/macros.h"

EXTERN_C_BEGIN

/*
 * Event Structures
 *
 * The following event structures are based on the equivalent structs
 * defined in ppapi/c/ppb_input_event.h.
 *
 */

// Generic Event
typedef struct {
  PP_InputEvent_Type event_type;
  PP_TimeTicks time_ticks;
  uint32_t modifiers;
} PPAPIEvent;


// Key Code Up/Down
typedef struct {
  PPAPIEvent event;

  uint32_t key_code;
} PPAPIKeyEvent;


// Cooked Character Event
typedef struct {
  PPAPIEvent event;

  char text[5];
} PPAPICharEvent;


// Mouse Event
typedef struct {
  PPAPIEvent event;

  PP_InputEvent_MouseButton button;
  struct PP_Point location;
  struct PP_Point delta;
} PPAPIMouseEvent;


// Wheel Event
typedef struct {
  PPAPIEvent event;

  PP_InputEvent_MouseButton button;
  struct PP_FloatPoint delta;
  uint32_t by_page;
} PPAPIWheelEvent;


// Touch Event
#define MAX_TOUCH_POINTS 4
typedef struct {
  PPAPIEvent event;

  uint32_t point_count;
  struct PP_TouchPoint points[MAX_TOUCH_POINTS];
} PPAPITouchEvent;

EXTERN_C_END

#endif  // PPAPI_MAIN_PPAPI_EVENT_H_
