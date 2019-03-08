// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_UTILS_H_
#define CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_UTILS_H_

#include <deque>
#include <utility>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}

namespace memory {
namespace internal {

// An observation window consists of a series of samples. Samples in this window
// will have a maximum age (the window length), samples that exceed that age
// will be automatically trimmed when a new one is available.
//
// Concurrent calls to instances of this class aren't allowed.
template <typename T>
class ObservationWindow {
 public:
  explicit ObservationWindow(base::TimeDelta window_length);
  virtual ~ObservationWindow();

  // Function that should be called each time a new sample is available. When a
  // new sample gets added the entries that exceeds the age of this window will
  // first be removed, and then the new one will be added to it.
  void OnSample(const T sample);

 protected:
  using Observation = std::pair<const base::TimeTicks, const T>;

  // Called each time a sample gets added to the observation window.
  virtual void OnSampleAdded(const T& sample) = 0;

  // Called each time a sample gets removed from the observation window.
  virtual void OnSampleRemoved(const T& sample) = 0;

  const std::deque<Observation>& observations_for_testing() {
    return observations_;
  }

  void set_clock_for_testing(const base::TickClock* clock) { clock_ = clock; }

 private:
  // The length of the window. Samples older than |base::TimeTicks::Now() -
  // window_length_| will automatically be removed from this window when a new
  // one gets added.
  const base::TimeDelta window_length_;

  // The observations, in order of arrival.
  std::deque<Observation> observations_;

  // Allow for an injectable clock for testing.
  const base::TickClock* clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ObservationWindow);
};

}  // namespace internal
}  // namespace memory

#include "chrome/browser/memory/memory_pressure_monitor_utils_impl.h"

#endif  // CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_UTILS_H_
