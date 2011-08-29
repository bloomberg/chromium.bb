// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include <base/scoped_ptr.h>
#include <gtest/gtest.h>  // For FRIEND_TEST

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/list.h"

#ifndef GESTURES_LOOKAHEAD_FILTER_INTERPRETER_H_
#define GESTURES_LOOKAHEAD_FILTER_INTERPRETER_H_

namespace gestures {

class LookaheadFilterInterpreter : public Interpreter {
  FRIEND_TEST(LookaheadFilterInterpreterTest, SimpleTest);
 public:
  explicit LookaheadFilterInterpreter(Interpreter* next);
  virtual ~LookaheadFilterInterpreter();

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

  virtual void Configure(GesturesPropProvider* pp, void* data);

  virtual void Deconfigure(GesturesPropProvider* pp, void* data);

 private:
  struct QState {
    QState();
    explicit QState(unsigned short max_fingers);

    // Deep copy of new_state to state_
    void set_state(const HardwareState& new_state);

    HardwareState state_;
    unsigned short max_fingers_;
    scoped_array<FingerState> fs_;

    stime_t due_;
    bool completed_;

    QState* next_;
    QState* prev_;
  };

  void UpdateInterpreterDue(stime_t new_interpreter_due,
                            stime_t now,
                            stime_t* timeout);
  bool ShouldSuppressResult(const Gesture* gesture, QState* node);

  List<QState> queue_;
  List<QState> free_list_;

  unsigned short max_fingers_per_hwstate_;

  scoped_ptr<Interpreter> next_;
  stime_t interpreter_due_;

  double min_nonsuppress_speed_;
  GesturesProp* min_nonsuppress_speed_prop_;
  double delay_;
  GesturesProp* delay_prop_;
};

}  // namespace gestures

#endif  // GESTURES_LOOKAHEAD_FILTER_INTERPRETER_H_
