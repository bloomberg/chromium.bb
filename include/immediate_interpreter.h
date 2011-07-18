// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/set.h"

#ifndef GESTURES_IMMEDIATE_INTERPRETER_H_
#define GESTURES_IMMEDIATE_INTERPRETER_H_

namespace gestures {

// This interpreter keeps some memory of the past and, for each incoming
// frame of hardware state, immediately determines the gestures to the best
// of its abilities.

// Currently it simply does very basic pointer movement.

static const int kMaxFingers = 5;
static const int kMaxGesturingFingers = 5;

class ImmediateInterpreter : public Interpreter {
  FRIEND_TEST(ImmediateInterpreterTest, SameFingersTest);
  FRIEND_TEST(ImmediateInterpreterTest, PalmTest);
  FRIEND_TEST(ImmediateInterpreterTest, GetGesturingFingersTest);
 public:
  ImmediateInterpreter();
  virtual ~ImmediateInterpreter();

  virtual Gesture* SyncInterpret(HardwareState* hwstate);

  void SetHardwareProperties(const HardwareProperties& hw_props);

 private:
  // Returns true iff the fingers in hwstate are the same ones in prev_state_
  bool SameFingers(const HardwareState& hwstate) const;

  // Reset the member variables corresponding to same-finger state and
  // updates changed_time_ to |now|.
  void ResetSameFingersState(stime_t now);

  // Updates *palm_, pointing_ below.
  void UpdatePalmState(const HardwareState& hwstate);

  // Gets the finger or fingers we should consider for gestures.
  // Currently, it fetches the (up to) two fingers closest to the keyboard
  // that are not palms. There is one exception: for t5r2 pads with > 2
  // fingers present, we return all fingers.
  set<short, kMaxGesturingFingers> GetGesturingFingers(
      const HardwareState& hwstate) const;

  // Does a deep copy of hwstate into prev_state_
  void SetPrevState(const HardwareState& hwstate);

  HardwareState prev_state_;
  HardwareProperties hw_props_;
  Gesture result_;

  // When fingers change, we record the time
  stime_t changed_time_;

  // Same fingers state. This state is accumulated as fingers remain the same
  // and it's reset when fingers change.
  set<short, kMaxFingers> palm_;  // tracking ids of known palms
  set<short, kMaxFingers> pending_palm_;  // tracking ids of potential palms
  // tracking ids of known non-palms
  set<short, kMaxGesturingFingers> pointing_;
};

}  // namespace gestures

#endif  // GESTURES_IMMEDIATE_INTERPRETER_H_
