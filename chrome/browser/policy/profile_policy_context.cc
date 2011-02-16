// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/device_management_policy_provider.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/profile_policy_context.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/pref_names.h"

namespace {

// Refresh rate sanity interval bounds.
const int64 kPolicyRefreshRateMinMs = 30 * 60 * 1000;  // 30 minutes
const int64 kPolicyRefreshRateMaxMs = 24 * 60 * 60 * 1000;  // 1 day

}

namespace policy {

ProfilePolicyContext::ProfilePolicyContext(Profile* profile)
    : profile_(profile) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDeviceManagementUrl)) {
    device_management_service_.reset(new DeviceManagementService(
        command_line->GetSwitchValueASCII(switches::kDeviceManagementUrl)));
    device_management_policy_provider_.reset(
        new policy::DeviceManagementPolicyProvider(
            ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
            device_management_service_->CreateBackend(),
            profile_));
  }
}

ProfilePolicyContext::~ProfilePolicyContext() {
  device_management_policy_provider_.reset();
  device_management_service_.reset();
}

void ProfilePolicyContext::Initialize() {
  if (device_management_service_.get())
    device_management_service_->Initialize(profile_->GetRequestContext());

  policy_refresh_rate_.Init(prefs::kPolicyRefreshRate, profile_->GetPrefs(),
                            this);
  UpdatePolicyRefreshRate();
}

void ProfilePolicyContext::Shutdown() {
  if (device_management_service_.get())
    device_management_service_->Shutdown();
}

DeviceManagementPolicyProvider*
ProfilePolicyContext::GetDeviceManagementPolicyProvider() {
  return device_management_policy_provider_.get();
}

// static
void ProfilePolicyContext::RegisterUserPrefs(PrefService* user_prefs) {
  user_prefs->RegisterIntegerPref(prefs::kPolicyRefreshRate,
                                  kDefaultPolicyRefreshRateInMilliseconds);
}

void ProfilePolicyContext::UpdatePolicyRefreshRate() {
  if (device_management_policy_provider_.get()) {
    // Clamp to sane values.
    int64 refresh_rate = policy_refresh_rate_.GetValue();
    refresh_rate = std::max(kPolicyRefreshRateMinMs, refresh_rate);
    refresh_rate = std::min(kPolicyRefreshRateMaxMs, refresh_rate);
    device_management_policy_provider_->SetRefreshRate(refresh_rate);
  }
}

void ProfilePolicyContext::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED &&
      prefs::kPolicyRefreshRate == *(Details<std::string>(details).ptr()) &&
      profile_->GetPrefs() == Source<PrefService>(source).ptr()) {
    UpdatePolicyRefreshRate();
  } else {
    NOTREACHED();
  }
}

}  // namespace policy
