// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_subsystem.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/cloud_policy_controller.h"
#include "chrome/browser/policy/cloud_policy_identity_strategy.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

namespace {

// Default refresh rate.
const int64 kDefaultPolicyRefreshRateMs = 3 * 60 * 60 * 1000;  // 3 hours.

// Refresh rate sanity interval bounds.
const int64 kPolicyRefreshRateMinMs = 30 * 60 * 1000;  // 30 minutes
const int64 kPolicyRefreshRateMaxMs = 24 * 60 * 60 * 1000;  // 1 day

}  // namespace

namespace policy {

CloudPolicySubsystem::ObserverRegistrar::ObserverRegistrar(
    CloudPolicySubsystem* cloud_policy_subsystem,
    CloudPolicySubsystem::Observer* observer)
    : observer_(observer) {
  policy_notifier_ = cloud_policy_subsystem->notifier();
  policy_notifier_->AddObserver(observer);
}

CloudPolicySubsystem::ObserverRegistrar::~ObserverRegistrar() {
  if (policy_notifier_)
    policy_notifier_->RemoveObserver(observer_);
}

CloudPolicySubsystem::CloudPolicySubsystem(
    CloudPolicyIdentityStrategy* identity_strategy,
    CloudPolicyCacheBase* policy_cache)
    : identity_strategy_(identity_strategy) {
  net::NetworkChangeNotifier::AddIPAddressObserver(this);
  notifier_.reset(new PolicyNotifier());
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDeviceManagementUrl)) {
    device_management_service_.reset(new DeviceManagementService(
        command_line->GetSwitchValueASCII(switches::kDeviceManagementUrl)));
    cloud_policy_cache_.reset(policy_cache);
    cloud_policy_cache_->set_policy_notifier(notifier_.get());
    cloud_policy_cache_->Load();

    device_token_fetcher_.reset(
        new DeviceTokenFetcher(device_management_service_.get(),
                               cloud_policy_cache_.get(),
                               notifier_.get()));
  }
}

CloudPolicySubsystem::~CloudPolicySubsystem() {
  cloud_policy_controller_.reset();
  device_token_fetcher_.reset();
  cloud_policy_cache_.reset();
  device_management_service_.reset();
  net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
}

void CloudPolicySubsystem::OnIPAddressChanged() {
  if (state() == CloudPolicySubsystem::NETWORK_ERROR &&
      cloud_policy_controller_.get()) {
    cloud_policy_controller_->Retry();
  }
}

void CloudPolicySubsystem::Initialize(const char* refresh_pref_name,
                                      int64 delay_milliseconds) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDeviceManagementUrl)) {
    DCHECK(device_management_service_.get());
    DCHECK(cloud_policy_cache_.get());
    DCHECK(device_token_fetcher_.get());
    DCHECK(identity_strategy_);

    refresh_pref_name_ = refresh_pref_name;
    DCHECK(!cloud_policy_controller_.get());
    cloud_policy_controller_.reset(
        new CloudPolicyController(device_management_service_.get(),
                                  cloud_policy_cache_.get(),
                                  device_token_fetcher_.get(),
                                  identity_strategy_,
                                  notifier_.get()));

    device_management_service_->ScheduleInitialization(delay_milliseconds);

    PrefService* local_state = g_browser_process->local_state();
    DCHECK(pref_change_registrar_.IsEmpty());
    pref_change_registrar_.Init(local_state);
    pref_change_registrar_.Add(refresh_pref_name_, this);
    UpdatePolicyRefreshRate(local_state->GetInteger(refresh_pref_name_));
  }
}

void CloudPolicySubsystem::Shutdown() {
  if (device_management_service_.get())
    device_management_service_->Shutdown();
  cloud_policy_controller_.reset();
  cloud_policy_cache_.reset();
  pref_change_registrar_.RemoveAll();
}

CloudPolicySubsystem::PolicySubsystemState CloudPolicySubsystem::state() {
  return notifier_->state();
}

CloudPolicySubsystem::ErrorDetails CloudPolicySubsystem::error_details() {
  return notifier_->error_details();
}

void CloudPolicySubsystem::StopAutoRetry() {
  cloud_policy_controller_->StopAutoRetry();
  device_token_fetcher_->StopAutoRetry();
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

// static
void CloudPolicySubsystem::RegisterPrefs(PrefService* pref_service) {
  pref_service->RegisterIntegerPref(prefs::kDevicePolicyRefreshRate,
                                    kDefaultPolicyRefreshRateMs,
                                    PrefService::UNSYNCABLE_PREF);
  pref_service->RegisterIntegerPref(prefs::kUserPolicyRefreshRate,
                                    kDefaultPolicyRefreshRateMs,
                                    PrefService::UNSYNCABLE_PREF);
}

void CloudPolicySubsystem::UpdatePolicyRefreshRate(int64 refresh_rate) {
  if (cloud_policy_controller_.get()) {
    // Clamp to sane values.
    refresh_rate = std::max(kPolicyRefreshRateMinMs, refresh_rate);
    refresh_rate = std::min(kPolicyRefreshRateMaxMs, refresh_rate);
    cloud_policy_controller_->SetRefreshRate(refresh_rate);
  }
}

void CloudPolicySubsystem::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED) {
    DCHECK_EQ(*(Details<std::string>(details).ptr()),
              std::string(refresh_pref_name_));
    PrefService* pref_service = Source<PrefService>(source).ptr();
    // Temporarily also consider the profile preference service as a valid
    // source, since we cannot yet push user cloud policy to |local_state|.
    UpdatePolicyRefreshRate(pref_service->GetInteger(refresh_pref_name_));
  } else {
    NOTREACHED();
  }
}

void CloudPolicySubsystem::ScheduleServiceInitialization(
    int64 delay_milliseconds) {
  if (device_management_service_.get())
    device_management_service_->ScheduleInitialization(delay_milliseconds);
}

}  // namespace policy
