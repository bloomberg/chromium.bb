// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>  // for FRIEND_TEST

#include "gestures/include/filter_interpreter.h"
#include "gestures/include/finger_metrics.h"
#include "gestures/include/gestures.h"
#include "gestures/include/list.h"
#include "gestures/include/map.h"
#include "gestures/include/memory_manager.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/tracer.h"

#ifndef GESTURES_METRICS_FILTER_INTERPRETER_H_
#define GESTURES_METRICS_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter catches scenarios which we want to collect UMA stats for
// and generate GestureMetrics gestures for them. Those gestures would picked
// up in Chrome to log the desired UMA stats. We chose not to call out to the
// metrics lib here because it might introduce the deadlock problem.

class MetricsFilterInterpreter : public FilterInterpreter {
 public:
  // Takes ownership of |next|:
  MetricsFilterInterpreter(PropRegistry* prop_reg, Interpreter* next,
                           Tracer* tracer);
  virtual ~MetricsFilterInterpreter() {}

 protected:
  virtual void SyncInterpretImpl(HardwareState* hwstate, stime_t* timeout);

 private:
  // struct for one finger's data of one frame
  struct MState: public MemoryManaged<MState> {
    MState() {}
    MState(const FingerState& fs, const HardwareState& hwstate) {
      Init(fs, hwstate);
    }

    void Init(const FingerState& fs, const HardwareState& hwstate) {
      timestamp = hwstate.timestamp;
      data = fs;
      next_ = NULL, prev_ = NULL;
    }

    static const size_t kHistorySize = 3;

    stime_t timestamp;
    FingerState data;
    MState* next_;
    MState* prev_;
  };

  // Extend the List class with memory management capability.
  template<typename T>
  class ManagedList: public List<T, MemoryManagedDeallocator<T>>,
                     public MemoryManaged<ManagedList<T>> {
   public:
    void Init() { List<T, MemoryManagedDeallocator<T>>::Init(); }
  };

  typedef ManagedList<MState> FingerHistory;

  // Update the class with new finger data, check if there is any interesting
  // pattern
  void UpdateFingerState(const HardwareState& hwstate);

  // Push new finger data into the buffer and update values
  void AddNewStateToBuffer(FingerHistory* history,
                           const FingerState& fs,
                           const HardwareState& hwstate);

  // Detect the noisy ground pattern and send GestureMetrics
  bool DetectNoisyGround(const FingerHistory* history);

  // A map to store each finger's past data
  typedef map<short, FingerHistory*, kMaxFingers> FingerHistoryMap;
  FingerHistoryMap histories_;

  // Threshold values for detecting movements caused by the noisy ground
  DoubleProperty noisy_ground_distance_threshold_;
  DoubleProperty noisy_ground_time_threshold_;
};

}  // namespace gestures

#endif  // GESTURES_METRICS_FILTER_INTERPRETER_H_
