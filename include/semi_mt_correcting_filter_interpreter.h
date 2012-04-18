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

static const size_t kInvalidIndex = static_cast<size_t>(-1);

typedef struct {
  int origin_id;
  int new_id;
} TrackingIdMapping;

// This interpreter processes incoming input events which required some
// tweaking due to the limitation of profile sensor, i.e. the semi-mt touchpad
// on Cr48 before they are processed by other interpreters. The tweaks mainly
// include low-pressure filtering, hysterisis, finger position correction.

class SemiMtCorrectingFilterInterpreter : public Interpreter {
  FRIEND_TEST(SemiMtCorrectingFilterInterpreterTest, SimpleTest);
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
  void LowPressureFilter(HardwareState* s);

  // Return the index of the given original tracking id in the id mapping array.
  size_t GetTrackingIdIndex(int tid) const;

  // Issue a new tracking id by giving its original tracking id from evdev
  // driver.
  int GetNewTrackingId(int original_tracking_id);

  // Reissue a new tracking id by giving its current assigned tracking id.
  int RenewTrackingId(int current_tracking_id);

  // Remove a finger with slot_index from array of slots in a HardwareState, and
  // reset its entry in ID mapping array.
  void RemoveFinger(HardwareState* s, size_t slot_index, size_t mapping_index);

  unsigned short next_id_;
  TrackingIdMapping id_mapping_[2];
  bool is_semi_mt_device_;
  BoolProperty interpreter_enabled_;
  DoubleProperty pressure_threshold_;
  DoubleProperty hysteresis_pressure_;
  scoped_ptr<Interpreter> next_;
};

}  // namespace gestures

#endif  // SEMI_MT_PREPROCESS_INTERPRETER_H_
