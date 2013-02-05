// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/network_time_tracker.h"

#include <math.h>

#include "base/logging.h"

namespace {

// base::TimeTicks::Now() is documented to have a resolution of ~1-15ms.
const int64 kTicksResolutionMs = 15;

}  // namespace

NetworkTimeTracker::NetworkTimeTracker() {
}

NetworkTimeTracker::~NetworkTimeTracker() {
}

bool NetworkTimeTracker::GetNetworkTime(base::Time* network_time,
                                        base::TimeDelta* uncertainty) const {
  if (network_time_.is_null())
    return false;
  DCHECK(!network_time_ticks_.is_null());
  DCHECK(network_time);
  *network_time = network_time_ + (GetTicksNow() - network_time_ticks_);
  if (uncertainty)
    *uncertainty = network_time_uncertainty_;
  return true;
}

void NetworkTimeTracker::UpdateNetworkTime(const base::Time& network_time,
                                           const base::TimeDelta& resolution,
                                           const base::TimeDelta& latency) {
  DCHECK(ValidateTimeTracking(network_time, resolution, latency));

  // Update network time on every request to limit dependency on ticks lag.
  // TODO(mad): Find a heuristic to avoid augmenting the
  // network_time_uncertainty_ too much by a particularly long latency.
  // Maybe only update when we either improve accuracy or drifted too far
  // from |network_time|.
  network_time_ = network_time;
  // Estimate that the time was set midway through the latency time.
  network_time_ticks_ = GetTicksNow() - latency / 2;

  // We can't assume a better time than the resolution of the given time
  // and we involve 4 ticks value, each with their own uncertainty, 1 & 2
  // are the ones used to compute the latency, 3 is the Now() above and 4
  // will be the Now() used in GetNetworkTime().
  network_time_uncertainty_ =
      resolution + latency +
      4 * base::TimeDelta::FromMilliseconds(kTicksResolutionMs);
}

base::TimeTicks NetworkTimeTracker::GetTicksNow() const {
  return base::TimeTicks::Now();
}

bool NetworkTimeTracker::ValidateTimeTracking(
    const base::Time& new_network_time,
    const base::TimeDelta& resolution,
    const base::TimeDelta& latency) {
#ifndef NDEBUG
  base::Time current_network_time;
  base::TimeDelta uncertainty;
  if (GetNetworkTime(&current_network_time, &uncertainty)) {
    // Account for new_network_time's own innaccuracy.
    uncertainty +=
        resolution + latency +
        2 * base::TimeDelta::FromMilliseconds(kTicksResolutionMs);
    DVLOG(1) << current_network_time.ToInternalValue() << " VS "
             << new_network_time.ToInternalValue() << ", should be within: "
             << uncertainty.ToInternalValue();
    return fabs(static_cast<double>(current_network_time.ToInternalValue() -
                                    new_network_time.ToInternalValue())) <=
           static_cast<double>(uncertainty.ToInternalValue());
  }
#endif  // ifndef NDEBUG
  return true;  // No time to track so no tracking drift, or in an NDEBUG build.
}
