// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_
#define COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class TickClock;
}

namespace network_time {

// A class that receives network time updates and can provide the network time
// for a corresponding local time. This class is not thread safe.
class NetworkTimeTracker {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  NetworkTimeTracker(scoped_ptr<base::TickClock> tick_clock,
                     PrefService* pref_service);
  ~NetworkTimeTracker();

  // Returns the network time corresponding to |time_ticks| if network time
  // is available. Returns false if no network time is available yet. Can also
  // return the error range if |uncertainty| isn't NULL.
  bool GetNetworkTime(base::TimeTicks time_ticks,
                      base::Time* network_time,
                      base::TimeDelta* uncertainty) const;

  // Calculates corresponding time ticks according to the given parameters.
  // The provided |network_time| is precise at the given |resolution| and
  // represent the time between now and up to |latency| + (now - |post_time|)
  // ago.
  void UpdateNetworkTime(base::Time network_time,
                         base::TimeDelta resolution,
                         base::TimeDelta latency,
                         base::TimeTicks post_time);

  bool received_network_time() const {
    return received_network_time_;
  }

 private:
  // For querying current time ticks.
  scoped_ptr<base::TickClock> tick_clock_;

  PrefService* pref_service_;

  // Network time based on last call to UpdateNetworkTime().
  base::Time network_time_;

  // The estimated local time from |tick_clock| that corresponds with
  // |network_time|. Assumes the actual network time measurement was performed
  // midway through the latency time, and does not account for suspect/resume
  // events since the network time was measured.
  // See UpdateNetworkTime(..) implementation for details.
  base::TimeTicks network_time_ticks_;

  // Uncertainty of |network_time_| based on added inaccuracies/resolution.
  // See UpdateNetworkTime(..) implementation for details.
  base::TimeDelta network_time_uncertainty_;

  base::ThreadChecker thread_checker_;

  bool received_network_time_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTimeTracker);
};

}  // namespace network_time

#endif  // COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_
