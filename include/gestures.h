// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_GESTURES_H__
#define GESTURES_GESTURES_H__

#include <stdint.h>

#ifdef __cplusplus
#include <base/basictypes.h>
extern "C" {
#endif

// C API:

typedef int64_t ustime_t;  // microsecond resolution (10^-6 seconds)

struct HardwareProperties {
  int left, top, right, bottom;
  int touch_width;  // mm
  int touch_height;  // mm
  int screen_dpi;
  short supports_tripletap_hack;
  short max_finger_cnt;
};

struct FingerState {
  int touch_major, touch_minor;
  int width_major, width_minor;
  int pressure;
  int orientation;
  int position_x;
  int position_y;
  short tracking_id;
};

// One frame of trackpad data
struct HardwareState {
  ustime_t timestamp;
  short finger_cnt;
  struct FingerState* fingers;
};

#define GESTURES_BUTTON_LEFT 1
#define GESTURES_BUTTON_MIDDLE 2
#define GESTURES_BUTTON_RIGHT 4

struct Gesture {
  int dx, dy;
  int vscroll, hscroll;
  int fingers_present;
  int buttons_down;  // look at bits within, using button masks above
  const struct HardwareState* hwstate;
};

typedef void (*GestureReadyFunction)(void* client_data,
                                     const struct Gesture* gesture);

#ifdef __cplusplus
// C++ API:

namespace gestures {

struct GestureInterpreter {
 public:
  explicit GestureInterpreter(int version);
  ~GestureInterpreter();
  void PushHardwareState(const HardwareState* hwstate);

  void SetHardwareProps(const HardwareProperties* hwprops);

  void set_callback(GestureReadyFunction callback,
                    void* client_data) {
    callback_ = callback_;
    callback_data_ = client_data;
  }
  void set_tap_to_click(bool tap_to_click) { tap_to_click_ = tap_to_click; }
  void set_move_speed(int speed) { move_speed_ = speed; }
  void set_scroll_speed(int speed) { scroll_speed_ = speed; }
 private:
  GestureReadyFunction callback_;
  void* callback_data_;

  HardwareProperties hw_props_;

  // Settings
  bool tap_to_click_;
  int move_speed_;
  int scroll_speed_;

  DISALLOW_COPY_AND_ASSIGN(GestureInterpreter);
};

}  // namespace gestures

typedef gestures::GestureInterpreter GestureInterpreter;
#else
struct GestureInterpreter;
typedef struct GestureInterpreter GestureInterpreter;
#endif  // __cplusplus

#define GESTURES_VERSION 1
GestureInterpreter* NewGestureInterpreterImpl(int);
inline GestureInterpreter* NewGestureInterpreter() {
  return NewGestureInterpreterImpl(GESTURES_VERSION);  // pass current version
}

void DeleteGestureInterpreter(GestureInterpreter*);

void GestureInterpreterSetHardwareProperties(GestureInterpreter*,
                                             const struct HardwareProperties*);

void GestureInterpreterPushHardwareState(GestureInterpreter*,
                                         const struct HardwareState*);

void GestureInterpreterSetCallback(GestureInterpreter*,
                                   GestureReadyFunction,
                                   void*);

void GestureInterpreterSetTapToClickEnabled(GestureInterpreter*,
                                            int);  // 0 (disabled) or 1

void GestureInterpreterSetMovementSpeed(GestureInterpreter*,
                                        int);  // [0..100]

void GestureInterpreterSetScrollSpeed(GestureInterpreter*,
                                      int);  // [0..100]

#ifdef __cplusplus
}
#endif

#endif  // GESTURES_GESTURES_H__
