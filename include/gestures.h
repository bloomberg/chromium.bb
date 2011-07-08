// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_GESTURES_H__
#define GESTURES_GESTURES_H__

#include <stdint.h>
#include <sys/time.h>

#ifdef __cplusplus
#include <base/basictypes.h>
#include <base/scoped_ptr.h>
extern "C" {
#endif

// C API:

typedef double stime_t;  // seconds

stime_t StimeFromTimeval(const struct timeval*);

struct HardwareProperties {
  float left, top, right, bottom;
  float res_x;  // pixels/mm
  float res_y;  // pixels/mm
  float screen_x_dpi;  // read from X server and passed to library
  float screen_y_dpi;  // read from X server and passed to library
  unsigned short max_finger_cnt;
  unsigned supports_t5r2:1;
  unsigned support_semi_mt:1;
  unsigned is_button_pad:1;
};

// position is the (x,y) cartesian coord of the finger on the trackpad.
// touch_major/minor are the large/small radii of the ellipse of the touching
// finger. width_major/minor are also radii, but of the finger itself,
// including a little bit that isn't touching. So, width* is generally a tad
// larger than touch*.
// tracking_id: If a finger is the same physical finger across two
// consecutive frames, it must have the same tracking id; if it's a different
// finger, it may (should) have a different tracking id.
struct FingerState {
  float touch_major, touch_minor;
  float width_major, width_minor;
  float pressure;
  float orientation;
  float position_x;
  float position_y;
  short tracking_id;
};

#define GESTURES_BUTTON_LEFT 1
#define GESTURES_BUTTON_MIDDLE 2
#define GESTURES_BUTTON_RIGHT 4

// One frame of trackpad data
struct HardwareState {
#ifdef __cplusplus
  FingerState* GetFingerState(short tracking_id);
  const FingerState* GetFingerState(short tracking_id) const;
#endif  // __cplusplus
  stime_t timestamp;  // 64-bit Wall clock time in microseconds (10^-6 s)
  int buttons_down;  // bit field, use GESTURES_BUTTON_*
  unsigned short finger_cnt;
  struct FingerState* fingers;
};

// Gesture sub-structs
typedef struct {
  float dx, dy;
} GestureMove;
typedef struct{
  float dx, dy;
} GestureScroll;
typedef struct {
  // If a bit is set in both down and up, client should process down first
  unsigned down;  // bit field, use GESTURES_BUTTON_*
  unsigned up;  // bit field, use GESTURES_BUTTON_*
} GestureButtonsChange;
enum GestureType {
#ifdef GESTURES_INTERNAL
  kGestureTypeNull = -1,  // internal to Gestures library only
#endif  // GESTURES_INTERNAL
  kGestureTypeContactInitiated = 0,
  kGestureTypeMove,
  kGestureTypeScroll,
  kGestureTypeButtonsChange,
};

#ifdef __cplusplus
// Pass these into Gesture() ctor as first arg
extern const GestureMove kGestureMove;
extern const GestureScroll kGestureScroll;
extern const GestureButtonsChange kGestureButtonsChange;
#endif  // __cplusplus

struct Gesture {
#ifdef __cplusplus
  // Create Move/Scroll gesture
#ifdef GESTURES_INTERNAL
  Gesture() : start_time(0), end_time(0), type(kGestureTypeNull) {}
#endif  // GESTURES_INTERNAL
  Gesture(const GestureMove&, stime_t start, stime_t end, float dx, float dy)
      : start_time(start), end_time(end), type(kGestureTypeMove) {
    details.move.dx = dx;
    details.move.dy = dy;
  }
  Gesture(const GestureScroll&,
          stime_t start, stime_t end, float dx, float dy)
      : start_time(start), end_time(end), type(kGestureTypeScroll) {
    details.scroll.dx = dx;
    details.scroll.dy = dy;
  }
  Gesture(const GestureButtonsChange&,
          stime_t start, stime_t end, unsigned down, unsigned up)
      : start_time(start), end_time(end), type(kGestureTypeButtonsChange) {
    details.buttons.down = down;
    details.buttons.up = up;
  }
#endif  // __cplusplus

  stime_t start_time, end_time;
  enum GestureType type;
  union {
    GestureMove move;
    GestureScroll scroll;
    GestureButtonsChange buttons;
  } details;
};

typedef void (*GestureReadyFunction)(void* client_data,
                                     const struct Gesture* gesture);

#ifdef __cplusplus
// C++ API:

namespace gestures {

class Interpreter;

struct GestureInterpreter {
 public:
  explicit GestureInterpreter(int version);
  ~GestureInterpreter();
  void PushHardwareState(HardwareState* hwstate);

  void SetHardwareProperties(const HardwareProperties& hwprops);

  void set_callback(GestureReadyFunction callback,
                    void* client_data) {
    callback_ = callback;
    callback_data_ = client_data;
  }
  void set_tap_to_click(bool tap_to_click) { tap_to_click_ = tap_to_click; }
  void set_move_speed(int speed) { move_speed_ = speed; }
  void set_scroll_speed(int speed) { scroll_speed_ = speed; }
 private:
  GestureReadyFunction callback_;
  void* callback_data_;

  scoped_ptr<Interpreter> interpreter_;

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
                                         struct HardwareState*);

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
