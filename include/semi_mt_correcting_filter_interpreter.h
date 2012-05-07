// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/prop_registry.h"

#ifndef GESTURES_SEMI_MT_CORRECTING_FILTER_INTERPRETER_H_
#define GESTURES_SEMI_MT_CORRECTING_FILTER_INTERPRETER_H_

namespace gestures {

static const size_t kMaxSemiMtFingers = 2;

typedef struct {
  float x, y;
} FingerPosition;

// The finger pattern of bounding box stands for the relative positions of
// fingers in the box. For example, the kBottomLeftTopRight means the finger
// 0 of the HardwareState is on the bottom left of the box, and the finger 1
// is on the top-right of the box, respectively. The value of each pattern
// is composed by two sets of LRTB bits(L: Left, R: Right, T:Top, B:Bottom).
typedef enum {
  kBottomLeftTopRight = 0x96,  // 1001 0110 (finger0:LRTB finger1:LRTB)
  kTopLeftBottomRight = 0xA5,  // 1010 0101
  kBottomRightTopLeft = 0x5A,  // 0101 1010
  kTopRightBottomLeft = 0x69,  // 0110 1001
} FingerPattern;

static const unsigned char kFinger0OnLeft = 0x80;  // (1000 0000) L bit
static const unsigned char kFinger0OnRight = 0x40;  // (0100 0000) R bit
static const unsigned char kFinger0OnTop = 0x20;  // (0010 0000) T bit
static const unsigned char kFinger0OnBottom = 0x10;  // (0001 0000) B bit

static const unsigned char kSwapPositionX = 0xCC;  // (1100 1100) LR bits on
static const unsigned char kSwapPositionY = 0x33;  // (0011 0011) TB bits on

// For the states with two-finger on the profile sensor, we can only track
// the following cases:
// 0) No finger moving
// 1) One finger moving veritically
// 2) One finger moving horizontally
// 3) Two finger moving together in parallel(i.e. vertically, horizontally or
//    diagonally)
typedef enum {
  kUnknown,
  kVerticalMove,
  kHorizontalMove
} MovingDirection;

// This interpreter processes incoming input events which required some
// tweaking due to the limitation of profile sensor, i.e. the semi-mt touchpad
// on Cr48 before they are processed by other interpreters. The tweaks mainly
// include low-pressure filtering, hysteresis, finger position correction.

class SemiMtCorrectingFilterInterpreter : public Interpreter {
  FRIEND_TEST(SemiMtCorrectingFilterInterpreterTest, LowPressureTest);
  FRIEND_TEST(SemiMtCorrectingFilterInterpreterTest, TrackingIdMappingTest);
  FRIEND_TEST(SemiMtCorrectingFilterInterpreterTest, CorrectFingerPositionTest);
  FRIEND_TEST(SemiMtCorrectingFilterInterpreterTest, FingerCrossOverTest);
  FRIEND_TEST(SemiMtCorrectingFilterInterpreterTest, ClipNonLinearAreaTest);

 public:
  SemiMtCorrectingFilterInterpreter(PropRegistry* prop_reg, Interpreter* next);
  virtual ~SemiMtCorrectingFilterInterpreter() {}
  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);
  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);
  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

 private:
  // As the Synaptics touchpad on Cr48 is very sensitive, we want to avoid the
  // hovering finger to be gone and back with the same tracking id. In addition
  // to reassign a new tracking id for the case, the function also supports
  // hysteresis and removes finger(s) with low pressure from the HardwareState.
  void LowPressureFilter(HardwareState* hwstate);

  // Update finger tracking ids.
  void AssignTrackingId(HardwareState* hwstate);

  // Decide the initial finger pattern for the transition from 0/1 finger to
  // two fingers. The pattern, i.e. top-left-bottom-right or
  // bottom-left-top-right, will be used to track and correct the finger
  // position reported with a bounding box from kernel driver.
  void InitCurrentPattern(HardwareState* hwstate, const FingerPosition& center);

  // Correct finger positions by the current pattern. If it is one-finger
  // moving gesture like click-n-select or one-finger scroll, we also need to
  // update the pattern if the moving finger crosses the stationary one.
  void UpdateAbsolutePosition(HardwareState* hwstate,
                              const FingerPosition& center,
                              float x0, float y0, float x1, float y1);


  // Swap X positions of fingers and update the FingerPattern accordingly.
  void SwapFingerPatternX(HardwareState* hwstate);

  // Swap Y positions of fingers and update the FingerPattern accordingly.
  void SwapFingerPatternY(HardwareState* hwstate);

  // For one-finger moving gesture, the center of the bounding box can be used
  // to determine if the pattern swap is required. The pattern will be flipped
  // only if the center point crosses the stationary finger in moving axis.
  void UpdateFingerPattern(HardwareState* hwstate,
                           const FingerPosition& center);

  // Change the pattern to the new state and correct the finger positions
  // with the new pattern accordingly.
  void ChangeFingerPattern(HardwareState* hwstate, unsigned char update_bits);

  // Detect the moving finger by tracking each finger's velocity.
  void DetectMovingFinger(HardwareState* hwstate);

  // Set the position variable from finger's position in HardwareState.
  void SetPosition(FingerPosition* pos, HardwareState* hwstate);

  // As the active area is not linear on the edges of Cr48, We need to clip
  // finger positions which are in those non-linear area in order to avoid
  // cursor or scroll jumps.
  void ClipNonLinearFingerPosition(HardwareState* hwstate);

  // Update all active fingers based on previous finger positions. The main
  // entry of the finger correction method. The method is to manipulate the
  // reported fingers to reflect the real finger positions as the semi_mt with
  // profile sensor like Synaptics touchpad on Cr48 only reports the pair of
  // bounding box instead of real finger positions.
  void CorrectFingerPosition(HardwareState* hwstate);

  // Starting finger positions of the two-finger gesture.
  FingerPosition start_pos_[kMaxSemiMtFingers];

  // Previous HardwareState.
  HardwareState prev_hwstate_;

  // Previous Fingers.
  FingerState prev_fingers_[kMaxSemiMtFingers];

  // The last used id number.
  unsigned short last_id_;

  // Horizontal velocities of fingers.
  float velocity_x_[kMaxSemiMtFingers];

  // Veritical velocities of fingers.
  float velocity_y_[kMaxSemiMtFingers];

  // Current moving finger index.
  size_t moving_finger_;

  // Current finger pattern for two-finger gesture, i.e. bottom-left-top-right
  // or top-left-bottom-right.
  FingerPattern current_pattern_;

  bool is_semi_mt_device_;

  // True if the interpreter is effective in gesture pipeline.
  BoolProperty interpreter_enabled_;

  // The pressure threshold of an active finger.
  DoubleProperty pressure_threshold_;

  // The hysteresis pressure of an active finger.
  DoubleProperty hysteresis_pressure_;

  // The accumulation factor for calculating the velocity of a finger.
  DoubleProperty transfer_factor_;

  // The speed ratio in decision-making between one-finger moving or both.
  DoubleProperty speed_ratio_;

  // The speed threshold for detecting if a finger is moving.
  DoubleProperty speed_threshold_;

  // True if the touchpad has non-linear edges.
  BoolProperty clip_non_linear_edge_;

  // Non-linear area boundary.
  DoubleProperty non_linear_top_;
  DoubleProperty non_linear_bottom_;
  DoubleProperty non_linear_left_;
  DoubleProperty non_linear_right_;

  scoped_ptr<Interpreter> next_;
};

}  // namespace gestures

#endif  // SEMI_MT_PREPROCESS_INTERPRETER_H_
