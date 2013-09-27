// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/network_time/network_time_service.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/network_time/network_time_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

// static
void NetworkTimeService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kNetworkTimeMapping,
      new base::DictionaryValue(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

NetworkTimeService::NetworkTimeService(Profile* profile)
    : profile_(profile) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNetworkTime)) {
    return;
  }

  network_time_tracker_.reset(new NetworkTimeTracker);
  network_time_tracker_->Start();

  const base::DictionaryValue* time_mapping =
      profile_->GetPrefs()->GetDictionary(prefs::kNetworkTimeMapping);
  double local_time_js;
  double network_time_js;
  if (time_mapping->GetDouble("local", &local_time_js) &&
      time_mapping->GetDouble("network", &network_time_js)) {
    base::Time local_time_saved = base::Time::FromJsTime(local_time_js);
    if (local_time_saved > base::Time::Now() ||
        base::Time::Now() - local_time_saved > base::TimeDelta::FromDays(7)) {
      // Drop saved mapping if clock skew has changed or the data is too old.
      profile_->GetPrefs()->ClearPref(prefs::kNetworkTimeMapping);
    } else {
      network_time_tracker_->InitFromSavedTime(
          NetworkTimeTracker::TimeMapping(
              local_time_saved, base::Time::FromJsTime(network_time_js)));
    }
  }
}

NetworkTimeService::~NetworkTimeService() {}

void NetworkTimeService::Shutdown() {
  if (!network_time_tracker_)
    return;

  if (network_time_tracker_->received_network_time()) {
    // Update time mapping if tracker received time update from server, i.e.
    // mapping is accurate.
    base::Time local_now = base::Time::Now();
    base::Time network_now = GetNetworkTime(local_now);
    DictionaryValue time_mapping;
    time_mapping.SetDouble("local", local_now.ToJsTime());
    time_mapping.SetDouble("network", network_now.ToJsTime());
    profile_->GetPrefs()->Set(prefs::kNetworkTimeMapping, time_mapping);
  }
}

base::Time NetworkTimeService::GetNetworkTime(
    const base::Time& local_time) {
  if (!network_time_tracker_.get() || local_time.is_null() ||
      local_time.is_max())
    return local_time;

  base::Time network_time_now;
  if (!network_time_tracker_->GetNetworkTime(base::TimeTicks::Now(),
                                             &network_time_now, NULL)) {
    return local_time;
  }
  return local_time + (network_time_now - base::Time::Now());
}

base::Time NetworkTimeService::GetNetworkNow() {
  return GetNetworkTime(base::Time::Now());
}
