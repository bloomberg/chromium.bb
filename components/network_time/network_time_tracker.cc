// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include "base/basictypes.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/tick_clock.h"
#include "components/network_time/network_time_pref_names.h"

namespace network_time {

namespace {

// Clock resolution is platform dependent.
#if defined(OS_WIN)
const int64 kTicksResolutionMs = base::Time::kMinLowResolutionThresholdMs;
#else
const int64 kTicksResolutionMs = 1;  // Assume 1ms for non-windows platforms.
#endif

// Number of time measurements performed in a given network time calculation.
const int kNumTimeMeasurements = 5;

}  // namespace

// static
void NetworkTimeTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkTimeMapping,
                                   new base::DictionaryValue());
}

NetworkTimeTracker::NetworkTimeTracker(scoped_ptr<base::TickClock> tick_clock,
                                       PrefService* pref_service)
    : tick_clock_(tick_clock.Pass()),
      pref_service_(pref_service),
      received_network_time_(false) {
  const base::DictionaryValue* time_mapping =
      pref_service_->GetDictionary(prefs::kNetworkTimeMapping);
  double local_time_js = 0;
  double network_time_js = 0;
  if (time_mapping->GetDouble("local", &local_time_js) &&
      time_mapping->GetDouble("network", &network_time_js)) {
    base::Time local_time_saved = base::Time::FromJsTime(local_time_js);
    base::Time local_time_now = base::Time::Now();
    if (local_time_saved > local_time_now ||
        local_time_now - local_time_saved > base::TimeDelta::FromDays(7)) {
      // Drop saved mapping if clock skew has changed or the data is too old.
      pref_service_->ClearPref(prefs::kNetworkTimeMapping);
    } else {
      network_time_ = base::Time::FromJsTime(network_time_js) +
          (local_time_now - local_time_saved);
      network_time_ticks_ = base::TimeTicks::Now();
    }
  }
}

NetworkTimeTracker::~NetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void NetworkTimeTracker::UpdateNetworkTime(base::Time network_time,
                                           base::TimeDelta resolution,
                                           base::TimeDelta latency,
                                           base::TimeTicks post_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Network time updating to "
           << base::UTF16ToUTF8(
                  base::TimeFormatFriendlyDateAndTime(network_time));
  // Update network time on every request to limit dependency on ticks lag.
  // TODO(mad): Find a heuristic to avoid augmenting the
  // network_time_uncertainty_ too much by a particularly long latency.
  // Maybe only update when the the new time either improves in accuracy or
  // drifts too far from |network_time_|.
  network_time_ = network_time;

  // Calculate the delay since the network time was received.
  base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeDelta task_delay = now - post_time;
  // Estimate that the time was set midway through the latency time.
  network_time_ticks_ = now - task_delay - latency / 2;

  // Can't assume a better time than the resolution of the given time
  // and 5 ticks measurements are involved, each with their own uncertainty.
  // 1 & 2 are the ones used to compute the latency, 3 is the Now() from when
  // this task was posted, 4 is the Now() above and 5 will be the Now() used in
  // GetNetworkTime().
  network_time_uncertainty_ =
      resolution + latency + kNumTimeMeasurements *
      base::TimeDelta::FromMilliseconds(kTicksResolutionMs);

  received_network_time_ = true;

  base::Time network_time_now;
  if (GetNetworkTime(base::TimeTicks::Now(), &network_time_now, NULL)) {
    // Update time mapping if tracker received time update from server, i.e.
    // mapping is accurate.
    base::Time local_now = base::Time::Now();
    base::DictionaryValue time_mapping;
    time_mapping.SetDouble("local", local_now.ToJsTime());
    time_mapping.SetDouble("network", network_time_now.ToJsTime());
    pref_service_->Set(prefs::kNetworkTimeMapping, time_mapping);
  }
}

bool NetworkTimeTracker::GetNetworkTime(base::TimeTicks time_ticks,
                                        base::Time* network_time,
                                        base::TimeDelta* uncertainty) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(network_time);
  if (network_time_.is_null())
    return false;
  DCHECK(!network_time_ticks_.is_null());
  *network_time = network_time_ + (time_ticks - network_time_ticks_);
  if (uncertainty)
    *uncertainty = network_time_uncertainty_;
  return true;
}

}  // namespace network_time
