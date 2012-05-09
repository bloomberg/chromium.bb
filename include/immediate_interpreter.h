// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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

static const size_t kMaxFingers = 5;
static const size_t kMaxGesturingFingers = 3;
static const size_t kMaxTapFingers = 5;

class ImmediateInterpreter;

class TapRecord {
 public:
  explicit TapRecord(const ImmediateInterpreter* immediate_interpreter)
      : immediate_interpreter_(immediate_interpreter),
        t5r2_(false),
        t5r2_touched_size_(0),
        t5r2_released_size_(0) {}
  void Update(const HardwareState& hwstate,
              const HardwareState& prev_hwstate,
              const set<short, kMaxTapFingers>& added,
              const set<short, kMaxTapFingers>& removed,
              const set<short, kMaxFingers>& dead);
  void Clear();

  // if any gesturing fingers are moving
  bool Moving(const HardwareState& hwstate, const float dist_max) const;
  bool TapBegan() const;  // if a tap has begun
  bool TapComplete() const;  // is a completed tap
  // return GESTURES_BUTTON_* value or 0, if tap was too light
  int TapType() const;
  // If any contact has met the minimum pressure threshold
  bool MinTapPressureMet() const;
 private:
  void NoteTouch(short the_id, const FingerState& fs);  // Adds to touched_
  void NoteRelease(short the_id);  // Adds to released_
  void Remove(short the_id);  // Removes from touched_ and released_

  float CotapMinPressure() const;

  map<short, FingerState, kMaxTapFingers> touched_;
  set<short, kMaxTapFingers> released_;
  // At least one finger must meet the minimum pressure requirement during a
  // tap. This set contains the fingers that have.
  set<short, kMaxTapFingers> min_tap_pressure_met_;
  // All fingers must meet the cotap pressure, which is half of the min tap
  // pressure.
  set<short, kMaxTapFingers> min_cotap_pressure_met_;
  // Used to fetch properties
  const ImmediateInterpreter* immediate_interpreter_;
  // T5R2: For these pads, we try to track individual IDs, but if we get an
  // input event with insufficient data, we switch into T5R2 mode, where we
  // just track the number of contacts. We still maintain the non-T5R2 records
  // which are useful for tracking if contacts move a lot.
  // The following are for T5R2 mode:
  bool t5r2_;  // if set, use T5R2 hacks
  unsigned short t5r2_touched_size_;  // number of contacts that have arrived
  unsigned short t5r2_released_size_;  // number of contacts that have left
};

struct ScrollEvent {
  float dx, dy, dt;
  static ScrollEvent Add(const ScrollEvent& evt_a, const ScrollEvent& evt_b);
};
class ScrollEventBuffer {
 public:
  explicit ScrollEventBuffer(size_t size)
      : buf_(new ScrollEvent[size]), max_size_(size), size_(0), head_(0) {}
  void Insert(float dx, float dy, float dt);
  void Clear();
  size_t Size() const { return size_; }
  // 0 is newest, 1 is next newest, ..., size_ - 1 is oldest.
  const ScrollEvent& Get(size_t offset) const;

 private:
  scoped_array<ScrollEvent> buf_;
  size_t max_size_;
  size_t size_;
  size_t head_;
  DISALLOW_COPY_AND_ASSIGN(ScrollEventBuffer);
};

class ImmediateInterpreter : public Interpreter, public PropertyDelegate {
  FRIEND_TEST(ImmediateInterpreterTest, AvoidAccidentalPinchTest);
  FRIEND_TEST(ImmediateInterpreterTest, ChangeTimeoutTest);
  FRIEND_TEST(ImmediateInterpreterTest, ClickTest);
  FRIEND_TEST(ImmediateInterpreterTest, GetGesturingFingersTest);
  FRIEND_TEST(ImmediateInterpreterTest, PalmAtEdgeTest);
  FRIEND_TEST(ImmediateInterpreterTest, PalmTest);
  FRIEND_TEST(ImmediateInterpreterTest, PinchTests);
  FRIEND_TEST(ImmediateInterpreterTest, ScrollThenFalseTapTest);
  FRIEND_TEST(ImmediateInterpreterTest, SemiMtActiveAreaTest);
  FRIEND_TEST(ImmediateInterpreterTest, StationaryPalmTest);
  FRIEND_TEST(ImmediateInterpreterTest, SwipeTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapRecordTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapToClickEnableTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapToClickKeyboardTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapToClickLowPressureBeginOrEndTest);
  FRIEND_TEST(ImmediateInterpreterTest, TapToClickStateMachineTest);
  FRIEND_TEST(ImmediateInterpreterTest, ThumbRetainReevaluateTest);
  FRIEND_TEST(ImmediateInterpreterTest, ThumbRetainTest);
  friend class TapRecord;

 public:
  struct Point {
    Point() : x_(0.0), y_(0.0) {}
    Point(float x, float y) : x_(x), y_(y) {}
    bool operator==(const Point& that) const {
      return x_ == that.x_ && y_ == that.y_;
    }
    bool operator!=(const Point& that) const { return !((*this) == that); }
    float x_, y_;
  };
  enum TapToClickState {
    kTtcIdle,
    kTtcFirstTapBegan,
    kTtcTapComplete,
    kTtcSubsequentTapBegan,
    kTtcDrag,
    kTtcDragRelease,
    kTtcDragRetouch
  };

  explicit ImmediateInterpreter(PropRegistry* prop_reg);
  virtual ~ImmediateInterpreter();

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  void SetHardwareProperties(const HardwareProperties& hw_props);

  TapToClickState tap_to_click_state() const { return tap_to_click_state_; }

  float tap_min_pressure() const { return tap_min_pressure_.val_; }

 private:
  // Reset the member variables corresponding to same-finger state and
  // updates changed_time_ to |now|.
  void ResetSameFingersState(stime_t now);

  // Returns true if the two fingers are near each other enough to do a multi
  // finger gesture together.
  bool FingersCloseEnoughToGesture(const FingerState& finger_a,
                                   const FingerState& finger_b) const;

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

  // Returns the length of time the contact has been on the pad. Returns -1
  // on error.
  stime_t FingerAge(short finger_id, stime_t now) const;

  // Returns the square of the distance that this contact has travelled since
  // fingers changed.
  float DistanceTravelledSq(const FingerState& fs) const;

  // Returns a vector describing the movement of the finger since the
  // fingers changed.
  Point FingerTraveledVector(const FingerState& fs) const;

  // Returns the square of distance between two fingers.
  // Returns -1 if not exactly two fingers are present.
  float TwoFingerDistanceSq(const HardwareState& hwstate) const;

  // Updates thumb_ below.
  void UpdateThumbState(const HardwareState& hwstate);

  // Returns true iff the keyboard has been recently used.
  bool KeyboardRecentlyUsed(stime_t now) const;

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

  // Check for a pinch gesture and update the state machine for detection.
  // If a pinch was detected it will return true. False otherwise.
  // To reset the state machine call with reset=true
  bool UpdatePinchState(const HardwareState& hwstate, bool reset);

  // Returns the current three-finger gesture, or kGestureTypeNull if no gesture
  // should be produced.
  GestureType GetThreeFingerGestureType(const FingerState* const fingers[3]);

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

  // Returns true iff the given finger is too close to any other finger to
  // realistically be doing a tap gesture.
  bool FingerTooCloseToTap(const HardwareState& hwstate, const FingerState& fs);

  // Does a deep copy of hwstate into prev_state_
  void SetPrevState(const HardwareState& hwstate);

  // Returns true iff finger is in the bottom, dampened zone of the pad
  bool FingerInDampenedZone(const FingerState& finger) const;

  // Called when fingers have changed to fill start_positions_.
  void FillStartPositions(const HardwareState& hwstate);

  // Fills the origin_* member variables.
  void FillOriginInfo(const HardwareState& hwstate);

  // Called to detect if fingers have started moving.
  void UpdateStartedMovingTime(
      const HardwareState& hwstate,
      const set<short, kMaxGesturingFingers>& gs_fingers);

  // Updates the internal button state based on the passed in |hwstate|.
  void UpdateButtons(const HardwareState& hwstate);

  // By looking at |hwstate| and internal state, determins if a button down
  // at this time would correspond to a left/middle/right click. Returns
  // GESTURES_BUTTON_{LEFT,MIDDLE,RIGHT}.
  int EvaluateButtonType(const HardwareState& hwstate);

  // Looking at this finger and the previous, returns true iff the pressure
  // is changing so quickly that we expect it's arriving on the pad or
  // departing.
  bool PressureChangingSignificantly(const HardwareState& hwstate,
                                     const FingerState& current,
                                     const FingerState& prev) const;

  // Returns the number of most recent event events in the scroll_buffer_ that
  // should be considered for fling. If it returns 0, there should be no fling.
  size_t ScrollEventsForFlingCount() const;

  // Returns a ScrollEvent that contains velocity estimates for x and y based
  // on an N-point linear regression.
  void RegressScrollVelocity(int count, ScrollEvent* out) const;

  // Returns a ScrollEvent that can be turned directly into a fling.
  void ComputeFling(ScrollEvent* out) const;

  // Precondition: current_mode_ is set to the mode based on |hwstate|.
  // Computes the resulting gesture, storing it in result_.
  void FillResultGesture(const HardwareState& hwstate,
                         const set<short, kMaxGesturingFingers>& fingers);

  virtual void IntWasWritten(IntProperty* prop);

  HardwareState prev_state_;
  set<short, kMaxGesturingFingers> prev_gs_fingers_;
  set<short, kMaxGesturingFingers> prev_tap_gs_fingers_;
  HardwareProperties hw_props_;
  Gesture result_;
  Gesture prev_result_;

  // Button data
  // Which button we are going to send/have sent for the physical btn press
  int button_type_;  // left, middle, or right

  // If we have sent button down for the currently down button
  bool sent_button_down_;

  // If we haven't sent a button down by this time, send one
  stime_t button_down_timeout_;

  // When fingers change, we record the time
  stime_t changed_time_;

  // When gesturing fingers move after change, we record the time
  stime_t started_moving_time_;

  // When fingers leave, we record the time
  stime_t finger_leave_time_;

  // When fingers change, we keep track of where they started.
  // Map: Finger ID -> (x, y) coordinate
  map<short, Point, kMaxFingers> start_positions_;

  // Time when a contact arrived. Persists even when fingers change.
  map<short, stime_t, kMaxFingers> origin_timestamps_;
  // FingerStates from when a contact first arrived. Persists even when fingers
  // change.
  map<short, FingerState, kMaxFingers> origin_fingerstates_;

  // Same fingers state. This state is accumulated as fingers remain the same
  // and it's reset when fingers change.
  set<short, kMaxFingers> palm_;  // tracking ids of known palms
  // These contacts have moved significantly and shouldn't be considered
  // stationary palms:
  set<short, kMaxFingers> non_stationary_palm_;
  // tracking ids of known fingers that are not palms, nor thumbs.
  set<short, kMaxFingers> pointing_;
  // tracking ids of known non-palms. But might be thumbs.
  set<short, kMaxFingers> fingers_;
  // contacts believed to be thumbs, and when they were inserted into the map
  map<short, stime_t, kMaxFingers> thumb_;

  // Tap-to-click
  // The current state:
  TapToClickState tap_to_click_state_;

  // When we entered the state:
  stime_t tap_to_click_state_entered_;

  TapRecord tap_record_;

  // Time when the last motion (scroll, movement) occurred
  stime_t last_movement_timestamp_;

  // Time when the last swipe gesture was generated
  stime_t last_swipe_timestamp_;

  // If we are currently pointing, scrolling, etc.
  GestureType current_gesture_type_;

  // Cache for distance between fingers at start of pinch gesture
  float two_finger_start_distance_;

  // If the last time we were called, we did a scroll, it contains the ids
  // of the scrolling fingers. Otherwise it's empty.
  set<short, kMaxGesturingFingers> prev_scroll_fingers_;
  ScrollEventBuffer scroll_buffer_;

  // When guessing a pinch gesture. Do we guess pinch (true) or no-pinch?
  bool pinch_guess_;
  // Time when pinch guess was made. -1 if no guess has been made yet.
  stime_t pinch_guess_start_;
  // True when the pinch decision has been locked.
  bool pinch_locked_;

  // Properties

  // Is Tap-To-Click enabled
  BoolProperty tap_enable_;
  // General time limit [s] for tap gestures
  DoubleProperty tap_timeout_;
  // General time limit [s] for time between taps.
  DoubleProperty inter_tap_timeout_;
  // Time [s] before a tap gets recognized as a drag.
  DoubleProperty tap_drag_delay_;
  // Time [s] it takes to stop dragging when you let go of the touchpad
  DoubleProperty tap_drag_timeout_;
  // True if drag lock is enabled
  BoolProperty drag_lock_enable_;
  // Distance [mm] a finger can move and still register a tap
  DoubleProperty tap_move_dist_;
  // Minimum pressure a finger must have for it to click when tap to click is on
  DoubleProperty tap_min_pressure_;
  // If three finger click should be enabled. This is a temporary flag so that
  // we can deploy this feature behind a file while we work out the bugs.
  BoolProperty three_finger_click_enable_;
  // If T5R2 should support three-finger click/tap, which can in some situations
  // be unreliable.
  BoolProperty t5r2_three_finger_click_enable_;
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
  // A potential palm (ambiguous contact in the edge of the pad) will be marked
  // a palm if it travels less than palm_stationary_distance_ mm after it's
  // been on the pad for palm_stationary_time_ seconds.
  DoubleProperty palm_stationary_time_;
  DoubleProperty palm_stationary_distance_;
  // Distance [mm] a finger must move after fingers change to count as real
  // motion
  DoubleProperty change_move_distance_;
  // Time [s] to block movement after number or identify of fingers change
  DoubleProperty change_timeout_;
  // Time [s] to wait before locking on to a gesture
  DoubleProperty evaluation_timeout_;
  // A finger in the damp zone must move at least this much as much as
  // the other finger to count toward a gesture. Should be between 0 and 1.
  DoubleProperty damp_scroll_min_movement_factor_;
  // If two fingers have a pressure difference greater than diff thresh and
  // the larger is more than diff factor times the smaller, we assume the
  // larger is a thumb.
  DoubleProperty two_finger_pressure_diff_thresh_;
  DoubleProperty two_finger_pressure_diff_factor_;
  // If a large contact moves more than this much times the lowest-pressure
  // contact, consider it not to be a thumb.
  DoubleProperty thumb_movement_factor_;
  // This much time after fingers change, stop allowing contacts classified
  // as thumb to be classified as non-thumb.
  DoubleProperty thumb_eval_timeout_;
  // Maximum distance [mm] two fingers may be separated and still be eligible
  // for a two-finger gesture (e.g., scroll / tap / click). These define an
  // ellipse with horizontal and vertical axes lengths (think: radii).
  DoubleProperty two_finger_close_horizontal_distance_thresh_;
  DoubleProperty two_finger_close_vertical_distance_thresh_;
  // Consider scroll vs pointing if finger moves at least this distance [mm]
  DoubleProperty two_finger_scroll_distance_thresh_;
  // Maximum distance [mm] between the outermost fingers while performing a
  // three-finger gesture.
  DoubleProperty three_finger_close_distance_thresh_;
  // Minimum distance [mm] each of the three fingers must move to perform a
  // swipe gesture.
  DoubleProperty three_finger_swipe_distance_thresh_;
  // A finger must change in pressure by less than this per second to trigger
  // motion.
  DoubleProperty max_pressure_change_;
  // During a scroll one finger determines scroll speed and direction.
  // Maximum distance [mm] the other finger can move in opposite direction
  DoubleProperty scroll_stationary_finger_max_distance_;
  // Height [mm] of the bottom zone
  DoubleProperty bottom_zone_size_;
  // Time [s] to evaluate number of fingers for a click
  DoubleProperty button_evaluation_timeout_;
  // Timeval of time when keyboard was last touched. After the low one is set,
  // the two are converted into an stime_t and stored in keyboard_touched_.
  IntProperty keyboard_touched_timeval_high_;  // seconds
  IntProperty keyboard_touched_timeval_low_;  // microseconds
  stime_t keyboard_touched_;
  // During this timeout, which is time [s] since the keyboard has been used,
  // we are extra aggressive in palm detection. If this time is > 10s apart
  // from now (either before or after), it's disregarded. We disregard old
  // values b/c they no longer apply. Because of delays in other interpreters
  // (LooaheadInterpreter), it's possible to get "future" keyboard used times.
  // We wouldn't want a single bad future value to stop all tap-to-click, so
  // we sanity check.
  DoubleProperty keyboard_palm_prevent_timeout_;
  // Motion (pointer movement, scroll) must halt for this length of time [s]
  // before a tap can generate a click.
  DoubleProperty motion_tap_prevent_timeout_;
  // A finger must be at least this far from other fingers when it taps [mm].
  DoubleProperty tapping_finger_min_separation_;

  // y| V  /
  //  |   /  D   _-
  //  |  /    _-'
  //  | /  _-'
  //  |/_-'   H
  //  |'____________x
  // The above quadrant of a cartesian plane shows the angles where we snap
  // scrolling to vertical or horizontal. Very Vertical or Horizontal scrolls
  // are snapped, while Diagonal scrolls are not. The two properties below
  // are the slopes for the two lines.
  DoubleProperty vertical_scroll_snap_slope_;
  DoubleProperty horizontal_scroll_snap_slope_;

  // Ratio between finger movement that indicates not-a-pinch gesture
  DoubleProperty no_pinch_guess_ratio_;
  // Ratio between finger movement that certainly indicates not-a-pinch gesture
  DoubleProperty no_pinch_certain_ratio_;
  // Movement [mm] that is considered as noise during pinch detection
  DoubleProperty pinch_noise_level_;
  // Minimal distance [mm] fingers have to move to indicate a pinch gesture.
  DoubleProperty pinch_guess_min_movement_;
  // Minimal distance [mm] fingers have to move to lock a pinch gesture.
  DoubleProperty pinch_certain_min_movement_;
  // Temporary flag to turn pinch on/off while we tune it.
  BoolProperty pinch_enable_;
};

}  // namespace gestures

#endif  // GESTURES_IMMEDIATE_INTERPRETER_H_
