// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <vector>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"

namespace metrics {

// LeakDetector is an interface layer that connects the allocator
// (base::allocator), the leak detector logic (LeakDetectorImpl), and any
// external classes interested in receiving leak reports (extend the Observer
// class).
//
// Currently it is stubbed out and only provides an interface for registering
// observers to receive leak reports.
// TODO(sque): Add the full functionality and allow only one instance.
//
// This class is not thread-safe, and it should always be called on the same
// thread that instantiated it.
class LeakDetector {
 public:
  // Contains a report of a detected memory leak.
  struct LeakReport {
    LeakReport();
    ~LeakReport();

    size_t alloc_size_bytes;

    // Unlike the CallStack struct, which consists of addresses, this call stack
    // will contain offsets in the executable binary.
    std::vector<uintptr_t> call_stack;
  };

  // Interface for receiving leak reports.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called by leak detector to report a leak.
    virtual void OnLeakFound(const LeakReport& report) = 0;
  };

  // Constructor arguments:
  // sampling_rate:
  //     Pseudorandomly sample a fraction of the incoming allocations and frees,
  //     based on hash values. Setting to 0 means no allocs/frees are sampled.
  //     Setting to 1.0 or more means all allocs/frees are sampled. Anything in
  //     between will result in an approximately that fraction of allocs/frees
  //     being sampled.
  // max_call_stack_unwind_depth:
  //     The max number of call stack frames to unwind.
  // analysis_interval_bytes:
  //     Perform a leak analysis each time this many bytes have been allocated
  //     since the previous analysis.
  // size_suspicion_threshold, call_stack_suspicion_threshold:
  //     A possible leak should be suspected this many times to take action on i
  //     For size analysis, the action is to start profiling by call stack.
  //     For call stack analysis, the action is to generate a leak report.
  LeakDetector(float sampling_rate,
               size_t max_call_stack_unwind_depth,
               uint64_t analysis_interval_bytes,
               uint32_t size_suspicion_threshold,
               uint32_t call_stack_suspicion_threshold);

  ~LeakDetector();

  // Add |observer| to the list of stored Observers, i.e. |observers_|, to which
  // the leak detector will report leaks.
  void AddObserver(Observer* observer);

  // Remove |observer| from |observers_|.
  void RemoveObserver(Observer* observer);

 private:
  FRIEND_TEST_ALL_PREFIXES(LeakDetectorTest, NotifyObservers);

  // Notifies all Observers in |observers_| with the given vector of leak
  // reports.
  void NotifyObservers(const std::vector<LeakReport>& reports);

  // List of observers to notify when there's a leak report.
  base::ObserverList<Observer> observers_;

  // For thread safety.
  base::ThreadChecker thread_checker_;

  // For generating closures containing objects of this class.
  base::WeakPtrFactory<LeakDetector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetector);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_H_
