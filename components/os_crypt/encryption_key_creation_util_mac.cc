// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/encryption_key_creation_util_mac.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/single_thread_task_runner.h"
#include "components/os_crypt/os_crypt_features_mac.h"
#include "components/os_crypt/os_crypt_pref_names_mac.h"
#include "components/prefs/pref_service.h"

namespace os_crypt {

using GetKeyAction = EncryptionKeyCreationUtil::GetKeyAction;

namespace {

void LogKeyOverwritingPreventionsMetric(int preventions) {
  UMA_HISTOGRAM_COUNTS_100("OSCrypt.EncryptionKeyOverwritingPreventions",
                           preventions);
}

void LogGetEncryptionKeyActionMetric(
    EncryptionKeyCreationUtil::GetKeyAction action) {
  UMA_HISTOGRAM_ENUMERATION("OSCrypt.GetEncryptionKeyAction", action);
}

}  // namespace

EncryptionKeyCreationUtilMac::EncryptionKeyCreationUtilMac(
    PrefService* local_state,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : local_state_(local_state),
      main_thread_task_runner_(main_thread_task_runner),
      key_already_created_(local_state_->GetBoolean(prefs::kKeyCreated)) {}

EncryptionKeyCreationUtilMac::~EncryptionKeyCreationUtilMac() = default;

bool EncryptionKeyCreationUtilMac::KeyAlreadyCreated() {
  return key_already_created_;
}

bool EncryptionKeyCreationUtilMac::ShouldPreventOverwriting() {
  return base::FeatureList::IsEnabled(
      os_crypt::features::kPreventEncryptionKeyOverwrites);
}

void EncryptionKeyCreationUtilMac::OnKeyWasFound() {
  DCHECK(ShouldPreventOverwriting());
  if (key_already_created_) {
    LogGetEncryptionKeyActionMetric(GetKeyAction::kKeyFound);
  } else {
    LogGetEncryptionKeyActionMetric(GetKeyAction::kKeyFoundFirstTime);
  }

  LogKeyOverwritingPreventionsMetric(0);
  UpdateKeyCreationPreference();

  // Reset the counter of preventions.
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PrefService* local_state) {
                       local_state->SetInteger(
                           prefs::kKeyOverwritingPreventions, 0);
                     },
                     local_state_));
}

void EncryptionKeyCreationUtilMac::OnKeyWasStored() {
  DCHECK(ShouldPreventOverwriting());
  DCHECK(!key_already_created_);
  UpdateKeyCreationPreference();
  LogGetEncryptionKeyActionMetric(GetKeyAction::kNewKeyAddedToKeychain);
  LogKeyOverwritingPreventionsMetric(0);
}

void EncryptionKeyCreationUtilMac::OnOverwritingPrevented() {
  DCHECK(ShouldPreventOverwriting());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](PrefService* local_state) {
            int preventions =
                local_state->GetInteger(prefs::kKeyOverwritingPreventions) + 1;
            local_state->SetInteger(prefs::kKeyOverwritingPreventions,
                                    preventions);
            LogKeyOverwritingPreventionsMetric(preventions);
          },
          local_state_));

  LogGetEncryptionKeyActionMetric(GetKeyAction::kOverwritingPrevented);
  LOG(ERROR) << "Prevented overwriting the encryption key in the Keychain";
}

void EncryptionKeyCreationUtilMac::OnKeychainLookupFailed() {
  LogGetEncryptionKeyActionMetric(GetKeyAction::kKeychainLookupFailed);
}

void EncryptionKeyCreationUtilMac::UpdateKeyCreationPreference() {
  if (key_already_created_)
    return;
  key_already_created_ = true;
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PrefService* local_state) {
                       local_state->SetBoolean(prefs::kKeyCreated, true);
                     },
                     local_state_));
}

}  // namespace os_crypt
