// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_subsystem.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "chrome/browser/policy/cloud_policy_cache.h"
#include "chrome/browser/policy/cloud_policy_controller.h"
#include "chrome/browser/policy/cloud_policy_identity_strategy.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/common/chrome_switches.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

namespace {

// Refresh rate sanity interval bounds.
const int64 kPolicyRefreshRateMinMs = 30 * 60 * 1000;  // 30 minutes
const int64 kPolicyRefreshRateMaxMs = 24 * 60 * 60 * 1000;  // 1 day

}  // namespace

namespace policy {

CloudPolicySubsystem::CloudPolicySubsystem(
    const FilePath& policy_cache_file,
    CloudPolicyIdentityStrategy* identity_strategy)
    : prefs_(NULL) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDeviceManagementUrl)) {
    device_management_service_.reset(new DeviceManagementService(
        command_line->GetSwitchValueASCII(switches::kDeviceManagementUrl)));
    cloud_policy_cache_.reset(new CloudPolicyCache(policy_cache_file));
    cloud_policy_cache_->LoadFromFile();

    device_token_fetcher_.reset(
        new DeviceTokenFetcher(device_management_service_.get(),
                               cloud_policy_cache_.get()));

    cloud_policy_controller_.reset(
        new CloudPolicyController(cloud_policy_cache_.get(),
                                  device_management_service_->CreateBackend(),
                                  device_token_fetcher_.get(),
                                  identity_strategy));
  }
}

CloudPolicySubsystem::~CloudPolicySubsystem() {
  DCHECK(!prefs_);
  cloud_policy_controller_.reset();
  device_token_fetcher_.reset();
  cloud_policy_cache_.reset();
  device_management_service_.reset();
}

void CloudPolicySubsystem::Initialize(
    PrefService* prefs,
    const char* refresh_rate_pref_name,
    URLRequestContextGetter* request_context) {
  DCHECK(!prefs_);
  prefs_ = prefs;

  if (device_management_service_.get())
    device_management_service_->Initialize(request_context);

  policy_refresh_rate_.Init(refresh_rate_pref_name, prefs_, this);
  UpdatePolicyRefreshRate();
}

void CloudPolicySubsystem::Shutdown() {
  if (device_management_service_.get())
    device_management_service_->Shutdown();
  cloud_policy_controller_.reset();
  cloud_policy_cache_.reset();
  policy_refresh_rate_.Destroy();
  prefs_ = NULL;
}

ConfigurationPolicyProvider* CloudPolicySubsystem::GetManagedPolicyProvider() {
  if (cloud_policy_cache_.get())
    return cloud_policy_cache_->GetManagedPolicyProvider();

  return NULL;
}

ConfigurationPolicyProvider*
    CloudPolicySubsystem::GetRecommendedPolicyProvider() {
  if (cloud_policy_cache_.get())
    return cloud_policy_cache_->GetRecommendedPolicyProvider();

  return NULL;
}

void CloudPolicySubsystem::UpdatePolicyRefreshRate() {
  if (cloud_policy_controller_.get()) {
    // Clamp to sane values.
    int64 refresh_rate = policy_refresh_rate_.GetValue();
    refresh_rate = std::max(kPolicyRefreshRateMinMs, refresh_rate);
    refresh_rate = std::min(kPolicyRefreshRateMaxMs, refresh_rate);
    cloud_policy_controller_->SetRefreshRate(refresh_rate);
  }
}

void CloudPolicySubsystem::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED &&
      policy_refresh_rate_.GetPrefName() ==
          *(Details<std::string>(details).ptr()) &&
      prefs_ == Source<PrefService>(source).ptr()) {
    UpdatePolicyRefreshRate();
  } else {
    NOTREACHED();
  }
}

}  // namespace policy
