// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/map.h"
#include "gestures/include/set.h"

#ifndef GESTURES_IMMEDIATE_INTERPRETER_H_
#define GESTURES_IMMEDIATE_INTERPRETER_H_

namespace gestures {

// This interpreter keeps some memory of the past and, for each incoming
// frame of hardware state, immediately determines the gestures to the best
// of its abilities.

// Currently it simply does very basic pointer movement.

static const int kMaxFingers = 5;
static const int kMaxGesturingFingers = 2;
static const int kMaxTapFingers = 5;

class TapRecord {
 public:
  void Update(const HardwareState& hwstate,
              const set<short, kMaxTapFingers>& added,
              const set<short, kMaxTapFingers>& removed,
              const set<short, kMaxFingers>& dead);
  void Clear();

  // if any gesturing fingers are moving
  bool Moving(const HardwareState& hwstate, const float dist_max) const;
  bool TapComplete() const;  // is a completed tap
  int TapType() const;  // return GESTURES_BUTTON_* value
 private:
  void NoteTouch(short the_id, const FingerState& fs);  // Adds to touched_
  void NoteRelease(short the_id);  // Adds to released_
  void Remove(short the_id);  // Removes from touched_ and released_

  map<short, FingerState, kMaxTapFingers> touched_;
  set<short, kMaxTapFingers> released_;
};

class ImmediateInterpreter : public Interpreter, public PropertyDelegate {
  FRIEND_TEST(ImmediateInterpreterTest, PalmTest);
  FRIEND_TEST(ImmediateInterpreterTest, PalmAtEdgeTest);
  FRIEND_TEST(ImmediateInterpreterTest, GetGesturingFingersTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapToClickStateMachineTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapToClickEnableTest);
  FRIEND_TEST(ImmediateInterpreterTest, ThumbRetainReevaluateTest);
  FRIEND_TEST(ImmediateInterpreterTest, ThumbRetainTest);
 public:
  enum TapToClickState {
    kTtcIdle,
    kTtcFirstTapBegan,
    kTtcTapComplete,
    kTtcSubsequentTapBegan,
    kTtcDrag,
    kTtcDragRelease,
    kTtcDragRetouch
  };

  struct ClickWiggleRec {
    float x_, y_;  // position where we started blocking wiggle
    stime_t began_press_suppression_;  // when we started blocking inc/dec press
    bool suppress_inc_press_:1;
    bool suppress_dec_press_:1;
    bool operator==(const ClickWiggleRec& that) const {
      return x_ == that.x_ &&
          y_ == that.y_ &&
          began_press_suppression_ == that.began_press_suppression_ &&
          suppress_inc_press_ == that.suppress_inc_press_ &&
          suppress_dec_press_ == that.suppress_dec_press_;
    }
    bool operator!=(const ClickWiggleRec& that) const {
      return !(*this == that);
    }
  };

  explicit ImmediateInterpreter(PropRegistry* prop_reg);
  virtual ~ImmediateInterpreter();

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  void SetHardwareProperties(const HardwareProperties& hw_props);

  TapToClickState tap_to_click_state() const { return tap_to_click_state_; }

 private:
  // Reset the member variables corresponding to same-finger state and
  // updates changed_time_ to |now|.
  void ResetSameFingersState(stime_t now);

  // Part of palm detection. Returns true if the finger indicated by
  // |finger_idx| is near another finger, which must not be a palm, in the
  // hwstate.
  bool FingerNearOtherFinger(const HardwareState& hwstate,
                             size_t finger_idx);

  // Part of palm detection. Returns true if the finger is in the ambiguous edge
  // around the outside of the touchpad.
  bool FingerInPalmEdgeZone(const FingerState& fs);

  // Returns true iff fs represents a contact that may be a palm. It's a palm
  // if it's in the edge of the pad with sufficiently large pressure. The
  // pressure required depends on exactly how close to the edge the contact is.
  bool FingerInPalmEnvelope(const FingerState& fs);

  // Updates *palm_, pointing_ below.
  void UpdatePalmState(const HardwareState& hwstate);

  // Updates the wiggle_recs_ data based on the latest hardware state
  void UpdateClickWiggle(const HardwareState& hwstate);

  // Reads the wiggle_recs_ data to see if a given finger is being suppressed
  // from moving.
  bool WiggleSuppressed(short tracking_id) const;

  // Updates thumb_ below.
  void UpdateThumbState(const HardwareState& hwstate);

  // Gets the finger or fingers we should consider for gestures.
  // Currently, it fetches the (up to) two fingers closest to the keyboard
  // that are not palms. There is one exception: for t5r2 pads with > 2
  // fingers present, we return all fingers.
  set<short, kMaxGesturingFingers> GetGesturingFingers(
      const HardwareState& hwstate) const;

  // Updates current_gesture_type_ based on passed-in hwstate and
  // considering the passed in fingers as gesturing.
  void UpdateCurrentGestureType(
      const HardwareState& hwstate,
      const set<short, kMaxGesturingFingers>& gs_fingers);

  // If the fingers are near each other in location and pressure and might
  // to be part of a 2-finger action, returns true.
  bool TwoFingersGesturing(const FingerState& finger1,
                           const FingerState& finger2) const;

  // Given that TwoFingersGesturing returns true for 2 fingers,
  // This will further look to see if it's really 2 finger scroll or not.
  // Returns the current state (move or scroll) or kGestureTypeNull if
  // unknown.
  GestureType GetTwoFingerGestureType(const FingerState& finger1,
                                      const FingerState& finger2);

  const char* TapToClickStateName(TapToClickState state);

  stime_t TimeoutForTtcState(TapToClickState state);

  void SetTapToClickState(TapToClickState state,
                          stime_t now);

  void UpdateTapGesture(const HardwareState* hwstate,
                        const set<short, kMaxGesturingFingers>& gs_fingers,
                        const bool same_fingers,
                        stime_t now,
                        stime_t* timeout);

  void UpdateTapState(const HardwareState* hwstate,
                      const set<short, kMaxGesturingFingers>& gs_fingers,
                      const bool same_fingers,
                      stime_t now,
                      unsigned* buttons_down,
                      unsigned* buttons_up,
                      stime_t* timeout);

  // Does a deep copy of hwstate into prev_state_
  void SetPrevState(const HardwareState& hwstate);

  // Returns true iff finger is in the bottom, dampened zone of the pad
  bool FingerInDampenedZone(const FingerState& finger) const;

  // Called when fingers have changed to fill start_positions_.
  void FillStartPositions(const HardwareState& hwstate);

  // Updates the internal button state based on the passed in |hwstate|.
  void UpdateButtons(const HardwareState& hwstate);

  // By looking at |hwstate| and internal state, determins if a button down
  // at this time would correspond to a left/middle/right click. Returns
  // GESTURES_BUTTON_{LEFT,MIDDLE,RIGHT}.
  int EvaluateButtonType(const HardwareState& hwstate);

  // Precondition: current_mode_ is set to the mode based on |hwstate|.
  // Computes the resulting gesture, storing it in result_.
  void FillResultGesture(const HardwareState& hwstate,
                         const set<short, kMaxGesturingFingers>& fingers);

  HardwareState prev_state_;
  set<short, kMaxGesturingFingers> prev_gs_fingers_;
  HardwareProperties hw_props_;
  Gesture result_;

  // Button data
  // Which button we are going to send/have sent for the physical btn press
  int button_type_;  // left, middle, or right

  // If we have sent button down for the currently down button
  bool sent_button_down_;

  // If we haven't sent a button down by this time, send one
  stime_t button_down_timeout_;

  // When fingers change, we record the time
  stime_t changed_time_;

  // When fingers change, we keep track of where they started.
  // Map: Finger ID -> (x, y) coordinate
  map<short, std::pair<int, int>, kMaxFingers> start_positions_;

  // Same fingers state. This state is accumulated as fingers remain the same
  // and it's reset when fingers change.
  set<short, kMaxFingers> palm_;  // tracking ids of known palms
  set<short, kMaxFingers> pending_palm_;  // tracking ids of potential palms
  // tracking ids of known non-palms
  set<short, kMaxFingers> pointing_;

  // Click suppression
  map<short, ClickWiggleRec, kMaxFingers> wiggle_recs_;

  // contacts believed to be thumbs, and when they were inserted into the map
  map<short, stime_t, kMaxFingers> thumb_;

  // Tap-to-click
  // The current state:
  TapToClickState tap_to_click_state_;

  // When we entered the state:
  stime_t tap_to_click_state_entered_;

  TapRecord tap_record_;

  // If we are currently pointing, scrolling, etc.
  GestureType current_gesture_type_;

  // Properties

  // Is Tap-To-Click enabled
  BoolProperty tap_enable_;
  // General time limit [s] for tap gestures
  DoubleProperty tap_timeout_;
  // Time [s] it takes to stop dragging when you let go of the touchpad
  DoubleProperty tap_drag_timeout_;
  // Distance [mm] a finger can move and still register a tap
  DoubleProperty tap_move_dist_;
  // Maximum pressure above which a finger is considered a palm
  DoubleProperty palm_pressure_;
  // The smaller of two widths around the edge for palm detection. Any contact
  // in this edge zone may be a palm, regardless of pressure
  DoubleProperty palm_edge_min_width_;
  // The larger of the two widths. Palms between this and the previous are
  // expected to have pressure linearly increase from 0 to palm_pressure_
  // as they approach this border.
  DoubleProperty palm_edge_width_;
  // Palms in edge are allowed to point if they move fast enough
  DoubleProperty palm_edge_point_speed_;
  // Fingers within this distance of each other aren't palms
  DoubleProperty palm_min_distance_;
  // Movements that may be tap/click wiggle larger than this are allowed
  DoubleProperty wiggle_max_dist_;
  // Wiggles (while button up) suppressed only for this time length.
  DoubleProperty wiggle_suppress_timeout_;
  // Wiggles after physical button going down are suppressed for this time
  DoubleProperty wiggle_button_down_timeout_;
  // Time [s] to block movement after number or identify of fingers change
  DoubleProperty change_timeout_;
  // Time [s] to wait before locking on to a gesture
  DoubleProperty evaluation_timeout_;
  // If two fingers have a pressure difference greater than this, we assume
  // one is a thumb.
  DoubleProperty two_finger_pressure_diff_thresh_;
  // Maximum distance [mm] two fingers may be separated and still be eligible
  // for a two-finger gesture (e.g., scroll / tap / click)
  DoubleProperty two_finger_close_distance_thresh_;
  // Consider scroll vs pointing if finger moves at least this distance [mm]
  DoubleProperty two_finger_scroll_distance_thresh_;
  // A finger must change in pressure by less than this amount to trigger motion
  DoubleProperty max_pressure_change_;
  // During a scroll one finger determines scroll speed and direction.
  // Maximum distance [mm] the other finger can move in opposite direction
  DoubleProperty scroll_stationary_finger_max_distance_;
  // Height [mm] of the bottom zone
  DoubleProperty bottom_zone_size_;
  // Time [s] to evaluate number of fingers for a click
  DoubleProperty button_evaluation_timeout_;
};

}  // namespace gestures

#endif  // GESTURES_IMMEDIATE_INTERPRETER_H_
