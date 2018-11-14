// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/fast_transition_observer.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chromeos/chromeos_pref_names.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

FastTransitionObserver::FastTransitionObserver(PrefService* local_state)
    : local_state_(local_state) {
  pref_change_registrar_.Init(local_state_);

  base::RepeatingCallback<void(const std::string&)> fast_transition_callback =
      base::BindRepeating(&FastTransitionObserver::OnPreferenceChanged,
                          base::Unretained(this));

  pref_change_registrar_.Add(prefs::kDeviceWiFiFastTransitionEnabled,
                             fast_transition_callback);
}

FastTransitionObserver::~FastTransitionObserver() {}

void FastTransitionObserver::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kDeviceWiFiFastTransitionEnabled);
}

void FastTransitionObserver::OnPreferenceChanged(const std::string& pref_name) {
  DCHECK(pref_name == prefs::kDeviceWiFiFastTransitionEnabled);

  const base::DictionaryValue* fast_transition_policy =
      local_state_->GetDictionary(prefs::kDeviceWiFiFastTransitionEnabled);

  // Default is to disable Fast Transition if the policy is not found.
  bool enabled = false;
  if (fast_transition_policy)
    fast_transition_policy->GetBoolean("enabled", &enabled);

  NetworkHandler::Get()->network_state_handler()->SetFastTransitionStatus(
      enabled);
}

}  // namespace chromeos
