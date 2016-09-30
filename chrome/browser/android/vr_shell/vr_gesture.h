// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_GESTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_GESTURE_H_

#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"

using blink::WebInputEvent;

namespace vr_shell {

typedef enum {
  GESTURE_DIRECTION_UP,
  GESTURE_DIRECTION_DOWN,
  GESTURE_DIRECTION_LEFT,
  GESTURE_DIRECTION_RIGHT
} gesture_direction;

typedef struct {
  gvr_vec3f delta;
  int type;
} GestureAngularMove;

typedef struct {
  gvr_vec2f delta;

  // If set, stop_fling means that this scroll should stop flinging, thus
  // if an interpreter suppresses it for any reason (e.g., rounds the size
  // down to 0, thus making it a noop), it will replace it with a Fling
  // TAP_DOWN gesture
  unsigned stop_fling : 1;
  gvr_vec2f initial_touch_pos;
} GestureScroll;

typedef struct {
  // If a bit is set in both down and up, client should process down first
  gvr_vec2f pos;
  unsigned down;  // bit field, use GESTURES_BUTTON_*
  unsigned up;    // bit field, use GESTURES_BUTTON_*
} GestureButtonsChange;

struct VrGesture {
  VrGesture() : type(WebInputEvent::Undefined) {}
  VrGesture(const GestureAngularMove&,
            int64_t start,
            int64_t end,
            gvr::Quatf quat)
      : start_time(start),
        end_time(end),
        type(WebInputEvent::MouseMove),
        quat(quat) {}
  VrGesture(const GestureScroll&,
            int64_t start,
            int64_t end,
            float dx,
            float dy,
            WebInputEvent::Type state,
            gvr::Quatf quat)
      : start_time(start), end_time(end), type(state), quat(quat) {
    details.scroll.delta.x = dx;
    details.scroll.delta.y = dy;
    details.scroll.stop_fling = 0;
  }
  VrGesture(const GestureButtonsChange&,
            int64_t start,
            int64_t end,
            unsigned down,
            unsigned up,
            gvr::Quatf quat)
      : start_time(start),
        end_time(end),
        type(WebInputEvent::GestureTap),
        quat(quat) {
    details.buttons.down = down;
    details.buttons.up = up;
    details.buttons.pos.x = 0;
    details.buttons.pos.y = 0;
  }

  int64_t start_time, end_time;
  enum WebInputEvent::Type type;
  union {
    GestureScroll scroll;
    GestureButtonsChange buttons;
    GestureAngularMove move;
  } details;
  gvr::Quatf quat;
  gvr::Vec2f velocity;
  gvr::Vec2f displacement;
  gesture_direction direction;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_GESTURE_H_
