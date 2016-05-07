// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_
#define COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class TickClock;
}

namespace network_time {

// Clock resolution is platform dependent.
#if defined(OS_WIN)
const int64_t kTicksResolutionMs = base::Time::kMinLowResolutionThresholdMs;
#else
const int64_t kTicksResolutionMs = 1;  // Assume 1ms for non-windows platforms.
#endif

// A class that receives network time updates and can provide the network time
// for a corresponding local time. This class is not thread safe.
class NetworkTimeTracker {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  NetworkTimeTracker(std::unique_ptr<base::Clock> clock,
                     std::unique_ptr<base::TickClock> tick_clock,
                     PrefService* pref_service);
  ~NetworkTimeTracker();

  // Sets |network_time| to an estimate of the true time.  Returns true if time
  // is available, and false otherwise.  If |uncertainty| is non-NULL, it will
  // be set to an estimate of the error range.
  //
  // Network time may be available on startup if deserialized from a pref.
  // Failing that, a call to |UpdateNetworkTime| is required to make time
  // available to callers of |GetNetworkTime|.  Subsequently, network time may
  // become unavailable if |NetworkTimeTracker| has reason to believe it is no
  // longer accurate.  Consumers should even be prepared to handle the case
  // where calls to |GetNetworkTime| never once succeeds.
  bool GetNetworkTime(base::Time* network_time,
                      base::TimeDelta* uncertainty) const;

  // Calculates corresponding time ticks according to the given parameters.
  // The provided |network_time| is precise at the given |resolution| and
  // represent the time between now and up to |latency| + (now - |post_time|)
  // ago.
  void UpdateNetworkTime(base::Time network_time,
                         base::TimeDelta resolution,
                         base::TimeDelta latency,
                         base::TimeTicks post_time);

 private:
  // The |Clock| and |TickClock| are used to sanity-check one another, allowing
  // the NetworkTimeTracker to notice e.g. suspend/resume events and clock
  // resets.
  std::unique_ptr<base::Clock> clock_;
  std::unique_ptr<base::TickClock> tick_clock_;

  PrefService* pref_service_;

  // Network time based on last call to UpdateNetworkTime().
  mutable base::Time network_time_at_last_measurement_;

  // The estimated local times that correspond with |network_time_|. Assumes
  // the actual network time measurement was performed midway through the
  // latency time.  See UpdateNetworkTime(...) implementation for details.  The
  // tick clock is the one actually used to return values to callers, but both
  // clocks must agree to within some tolerance.
  base::Time time_at_last_measurement_;
  base::TimeTicks ticks_at_last_measurement_;

  // Uncertainty of |network_time_| based on added inaccuracies/resolution.  See
  // UpdateNetworkTime(...) implementation for details.
  base::TimeDelta network_time_uncertainty_;

  base::ThreadChecker thread_checker_;

  bool received_network_time_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTimeTracker);
};

}  // namespace network_time

#endif  // COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_
