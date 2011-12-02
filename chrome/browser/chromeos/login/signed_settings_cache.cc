// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings_cache.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"

using content::BrowserThread;

namespace chromeos {

namespace {

void OnStorePolicyCompleted(SignedSettings::ReturnCode code) {
  if (code != SignedSettings::SUCCESS)
    LOG(ERROR) << "Couldn't save temp store to the policy blob. code: " << code;
}

void FinishFinalize(PrefService* local_state,
                    SignedSettings::ReturnCode code,
                    const em::PolicyFetchResponse& policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (code != SignedSettings::SUCCESS) {
    LOG(ERROR) << "Can't finalize temp store error code:" << code;
    return;
  }

  if (local_state) {
    std::string encoded =
        local_state->GetString(prefs::kSignedSettingsCache);
    std::string policy_string;
    if (!base::Base64Decode(encoded, &policy_string)) {
      LOG(WARNING) << "Can't decode policy from base64 on finalizing.";
      return;
    }

    em::PolicyData merging_policy_data;
    if (!merging_policy_data.ParseFromString(policy_string)) {
      LOG(WARNING) << "Can't decode policy from string on finalizing.";
      return;
    }

    em::PolicyFetchResponse policy_envelope = policy;
    DCHECK(policy_envelope.has_policy_data());
    em::PolicyData base_policy_data;
    base_policy_data.ParseFromString(policy_envelope.policy_data());
    // Merge only the policy value as we should never ever rewrite the other
    // fields of the PolicyData protobuf.
    base_policy_data.set_policy_value(merging_policy_data.policy_value());
    policy_envelope.set_policy_data(base_policy_data.SerializeAsString());
    DCHECK(base_policy_data.has_username());
    policy_envelope.clear_policy_data_signature();
    SignedSettingsHelper::Get()->StartStorePolicyOp(
        policy_envelope, base::Bind(&OnStorePolicyCompleted));
  }
}

}  // namespace

namespace signed_settings_cache {

void RegisterPrefs(PrefService* local_state) {
  local_state->RegisterStringPref(prefs::kSignedSettingsCache,
                                  "invalid",
                                  PrefService::UNSYNCABLE_PREF);
}

bool Store(const em::PolicyData& policy, PrefService* local_state) {
  if (local_state) {
    std::string policy_string = policy.SerializeAsString();
    std::string encoded;
    if (!base::Base64Encode(policy_string, &encoded)) {
      LOG(WARNING) << "Can't encode policy in base64.";
      return false;
    }
    local_state->SetString(prefs::kSignedSettingsCache, encoded);
    return true;
  }
  return false;
}

bool Retrieve(em::PolicyData *policy, PrefService* local_state) {
  if (local_state) {
    std::string encoded =
        local_state->GetString(prefs::kSignedSettingsCache);
    std::string policy_string;
    if (!base::Base64Decode(encoded, &policy_string)) {
      LOG(WARNING) << "Can't decode policy from base64.";
      return false;
    }
    return policy->ParseFromString(policy_string);
  }
  return false;
}

void Finalize(PrefService* local_state) {
  // Reload the initial policy blob, apply settings from temp storage,
  // and write back the blob.
  SignedSettingsHelper::Get()->StartRetrievePolicyOp(
      base::Bind(FinishFinalize, local_state));
}

}  // namespace signed_settings_cache

}  // namespace chromeos
