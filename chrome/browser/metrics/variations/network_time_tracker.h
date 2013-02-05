// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_NETWORK_TIME_TRACKER_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_NETWORK_TIME_TRACKER_H_

#include "base/time.h"

// A class that tracks time based on a provided network time, using system's
// tick time to properly offset it, and also provide uncertainty.
class NetworkTimeTracker {
 public:
  NetworkTimeTracker();
  virtual ~NetworkTimeTracker();

  // Returns a network time based on values provided to UpdateNetworkTime and
  // CPU ticks count since then. Returns false if no network time is available
  // yet. Can also return the error range if |uncertainty| isn't NULL.
  bool GetNetworkTime(base::Time* network_time,
                      base::TimeDelta* uncertainty) const;

  // The provided |network_time| is precise at the given |resolution| and
  // represent the time between now and up to |latency| ago.
  void UpdateNetworkTime(const base::Time& network_time,
                         const base::TimeDelta& resolution,
                         const base::TimeDelta& latency);
 protected:
  // Used for a derviced test class to mock the current CPU ticks time.
  virtual base::TimeTicks GetTicksNow() const;

 private:
  // Returns true in NDEBUG, and validate time tracking when NDEBUG isn't
  // defined, returning FALSE if the network time has drifted too far.
  bool ValidateTimeTracking(const base::Time& new_network_time,
                            const base::TimeDelta& resolution,
                            const base::TimeDelta& latency);

  // Remember the network time based on last call to UpdateNetworkTime().
  base::Time network_time_;

  // The estimated ticks count when |network_time_| was set based on the
  // system's tick count.
  base::TimeTicks network_time_ticks_;

  // Uncertainty of |network_time_| based on added inaccuracies/resolution.
  base::TimeDelta network_time_uncertainty_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTimeTracker);
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_NETWORK_TIME_TRACKER_H_
