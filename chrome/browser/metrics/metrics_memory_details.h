// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_MEMORY_DETAILS_H_
#define CHROME_BROWSER_METRICS_METRICS_MEMORY_DETAILS_H_

#include <map>

#include "base/callback.h"
#include "chrome/browser/memory_details.h"

// MemoryGrowthTracker tracks latest metrics about record time and memory usage
// at that time per process.
class MemoryGrowthTracker {
 public:
  MemoryGrowthTracker();
  ~MemoryGrowthTracker();

  // If 30 minutes have passed since last UMA record, UpdateSample() computes
  // a difference between current memory usage |sample| of process |pid| and
  // stored memory usage at the time of last UMA record. Then, it updates the
  // stored memory usage to |sample|, stores the difference in |diff| and
  // returns true.
  // If no memory usage of |pid| has not been recorded so far or 30 minutes
  // have not passed since last record, it just returns false.
  // |sample| is memory usage in kB.
  bool UpdateSample(base::ProcessId pid, int sample, int* diff);

 private:
  // Latest metrics about record time and memory usage at that time per process.
  // The second values of |memory_sizes_| are in kB.
  std::map<base::ProcessId, base::TimeTicks> times_;
  std::map<base::ProcessId, int> memory_sizes_;

  DISALLOW_COPY_AND_ASSIGN(MemoryGrowthTracker);
};

// Handles asynchronous fetching of memory details and logging histograms about
// memory use of various processes.
// Will run the provided callback when finished.
class MetricsMemoryDetails : public MemoryDetails {
 public:
  MetricsMemoryDetails(const base::Closure& callback,
                       MemoryGrowthTracker* memory_growth_tracker);

 protected:
  ~MetricsMemoryDetails() override;

  // MemoryDetails:
  void OnDetailsAvailable() override;

 private:
  // Updates the global histograms for tracking memory usage.
  void UpdateHistograms();

#if defined(OS_CHROMEOS)
  void UpdateSwapHistograms();
#endif

  base::Closure callback_;

  // A pointer to MemoryGrowthTracker which is contained in a longer-lived
  // owner of MetricsMemoryDetails, for example, ChromeMetricsServiceClient.
  // If it is null, nothing is tracked.
  MemoryGrowthTracker* memory_growth_tracker_;

  DISALLOW_COPY_AND_ASSIGN(MetricsMemoryDetails);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_MEMORY_DETAILS_H_
