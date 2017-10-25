// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SwapThrashingMonitorWin defines a state-based swap thrashing monitor on
// Windows. Thrashing is continuous swap activity, caused by processes needing
// to touch more pages than fit in physical memory, over a given period of time.
//
// The systems interested in observing these signals should ensure that the
// SampleAndCalculateSwapThrashingLevel function of this detector gets called
// periodically as it is responsible for querying the state of the system.
//
// This monitor defines the following state and transitions:
//
//       ┎─────────────────────────┐
//       ┃SWAP_THRASHING_LEVEL_NONE┃
//       ┖─────────────────────────┚
//           |              ▲
//           | (T1)         | (T1')
//           ▼              |
//    ┎──────────────────────────────┒
//    ┃SWAP_THRASHING_LEVEL_SUSPECTED┃
//    ┖──────────────────────────────┚
//           |              ▲
//           | (T2)         | (T2')
//           ▼              |
//    ┎──────────────────────────────┒
//    ┃SWAP_THRASHING_LEVEL_CONFIRMED┃
//    ┖──────────────────────────────┚
//
// SWAP_THRASHING_LEVEL_NONE is the initial level, it means that there's no
// thrashing happening on the system or that it is undetermined. The monitor
// should regularly observe the hard page-fault counters and when a sustained
// thrashing activity is observed over a period of time T1 it'll enter into the
// SWAP_THRASHING_LEVEL_SUSPECTED state.
//
// If the system continues observing a sustained thrashing activity on the
// system over a second period of time T2 then it'll enter into the
// SWAP_THRASHING_LEVEL_CONFIRMED state to indicate that the system is now in
// a confirmed swap-thrashing state, otherwise it'll go back to the
// SWAP_THRASHING_LEVEL_NONE state if there's no swap-thrashing observed during
// another interval of time T1' (T1 with an hysteresis).
//
// From the SWAP_THRASHING_LEVEL_CONFIRMED level the monitor can only go back
// to the SWAP_THRASHING_LEVEL_SUSPECTED state if there's no swap-thrashing
// observed during an interval of time T2' (T2 with an hysteresis).

#ifndef CHROME_BROWSER_MEMORY_SWAP_THRASHING_MONITOR_WIN_H_
#define CHROME_BROWSER_MEMORY_SWAP_THRASHING_MONITOR_WIN_H_

#include <list>

#include "base/containers/circular_deque.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace memory {

class SwapThrashingMonitorWin {
 public:
  enum class SwapThrashingLevel {
    SWAP_THRASHING_LEVEL_NONE,
    SWAP_THRASHING_LEVEL_SUSPECTED,
    SWAP_THRASHING_LEVEL_CONFIRMED,
  };

  SwapThrashingMonitorWin();
  virtual ~SwapThrashingMonitorWin();

  // Calculates the swap-thrashing level over the interval between now and the
  // last time this function was called. This function will always return
  // SWAP_THRASHING_LEVEL_NONE when it gets called for the first time.
  //
  // This function requires sequence-affinity, through use of ThreadChecker. It
  // is also blocking and should be run on a blocking sequenced task runner.
  //
  // TODO(sebmarchand): Check if this should be posted to a task runner and run
  // asynchronously.
  SwapThrashingLevel SampleAndCalculateSwapThrashingLevel();

 protected:
  struct PageFaultObservation {
    uint64_t page_fault_count;
    base::TimeTicks timestamp;
  };

  // The average hard page-fault rate (/sec) that we expect to see when
  // switching to a higher swap-thrashing level.
  // TODO(sebmarchand): Confirm that this value is appropriate.
  static const double kPageFaultEscalationThreshold;

  // The average hard page-fault rate (/sec) that we use to go down to a lower
  // swap-thrashing level.
  // TODO(sebmarchand): Confirm that this value is appropriate.
  static const double kPageFaultCooldownThreshold;

  // Used to calculate the average page-fault rate across a specified time
  // window.
  class PageFaultAverageRate {
   public:
    // |window_length| is the minimum observation period before starting to
    // compute the average rate.
    explicit PageFaultAverageRate(base::TimeDelta window_length);
    virtual ~PageFaultAverageRate();

    // Reset the window to the value provided. Keep the most recent observation
    // if the observation list is not empty.
    void Reset(base::TimeDelta window_length);

    // Should be called when a new hard-page fault observation is made. The
    // observation will be added to the list of observations and the one that
    // are not needed anymore will be removed.
    virtual void OnObservation(PageFaultObservation observation);

    // Computes the average hard page-fault rate during this observation window.
    //
    // Returns true if the window of time has expired and store the average
    // value in |average_rate|, returns false otherwise.
    virtual base::Optional<double> AveragePageFaultRate() const;

   protected:
    base::TimeDelta window_length_;

    // The observation, in the order they have been received.
    base::circular_deque<PageFaultObservation> observations_;

   private:
    SEQUENCE_CHECKER(sequence_checker_);

    DISALLOW_COPY_AND_ASSIGN(PageFaultAverageRate);
  };

  // Record the sum of the hard page-fault count for all the Chrome processes.
  virtual bool RecordHardFaultCountForChromeProcesses();

  // Reset the escalation/cooldown windows.
  //
  // These functions are virtual for unittesting purposes.
  virtual void ResetEscalationWindow(base::TimeDelta window_length);
  virtual void ResetCooldownWindow(base::TimeDelta window_length);

  // The escalation window to detect the changes to a higher thrashing state.
  std::unique_ptr<PageFaultAverageRate> swap_thrashing_escalation_window_;

  // The cooldown window to detect the changes to a lower thrashing state.
  std::unique_ptr<PageFaultAverageRate> swap_thrashing_cooldown_window_;

  // The most recent swap-thrashing level measurement.
  SwapThrashingLevel last_thrashing_state_;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SwapThrashingMonitorWin);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_SWAP_THRASHING_MONITOR_WIN_H_
