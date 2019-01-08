// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/ownership/owner_settings_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kPendingPref[] = "pending.cros.metrics.reportingEnabled";

}  // namespace

namespace chromeos {

static StatsReportingController* g_stats_reporting_controller = nullptr;

// static
void StatsReportingController::Initialize(PrefService* local_state) {
  CHECK(!g_stats_reporting_controller);
  g_stats_reporting_controller = new StatsReportingController(local_state);
}

// static
bool StatsReportingController::IsInitialized() {
  return g_stats_reporting_controller;
}

// static
void StatsReportingController::Shutdown() {
  DCHECK(g_stats_reporting_controller);
  delete g_stats_reporting_controller;
  g_stats_reporting_controller = nullptr;
}

// static
StatsReportingController* StatsReportingController::Get() {
  CHECK(g_stats_reporting_controller);
  return g_stats_reporting_controller;
}

// static
void StatsReportingController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kPendingPref, false,
                                PrefRegistry::NO_REGISTRATION_FLAGS);
}

void StatsReportingController::SetEnabled(Profile* profile, bool enabled) {
  DCHECK(profile);

  if (GetOwnershipStatus() == DeviceSettingsService::OWNERSHIP_TAKEN) {
    // The device has an owner. If the current profile is that owner, we will
    // write the value on their behalf, otherwise no action is taken.
    SetWithServiceAsync(GetOwnerSettingsService(profile), enabled);
  } else {
    // The device has no owner, or we do not know yet whether the device has an
    // owner. We write a pending value that will be persisted when ownership is
    // taken (if that has not already happened).
    // We store the new value in the local state, so that even if Chrome is
    // restarted before ownership is taken, we will still persist it eventually.
    // See OnOwnershipTaken.
    local_state_->SetBoolean(kPendingPref, enabled);
  }
}

bool StatsReportingController::IsEnabled() {
  bool value = false;
  auto ownership = GetOwnershipStatus();
  if (ownership == DeviceSettingsService::OWNERSHIP_TAKEN) {
    // If ownership has been taken, return the actual value.
    GetActualValue(&value);
  } else if (ownership == DeviceSettingsService::OWNERSHIP_NONE) {
    // If ownership has not been taken, return the pending value.
    GetPendingValue(&value);
  } else {
    // If we are not sure if there whether is an owner, we just return false.
    value = false;
  }
  return value;
}

void StatsReportingController::OnOwnershipTaken(Profile* owner) {
  DCHECK_EQ(GetOwnershipStatus(), DeviceSettingsService::OWNERSHIP_TAKEN);

  bool pending_value;
  if (GetPendingValue(&pending_value)) {
    // At the time ownership is taken, there is a value waiting to be written.
    // Use the OwnerSettingsService of the new owner to write the setting.
    SetWithServiceAsync(GetOwnerSettingsService(owner), pending_value);
  }
}

StatsReportingController::StatsReportingController(PrefService* local_state)
    : local_state_(local_state), weak_factory_(this) {}

StatsReportingController::~StatsReportingController() {}

void StatsReportingController::SetWithServiceAsync(
    ownership::OwnerSettingsService* service,  // Can be null for non-owners.
    bool enabled) {
  bool not_yet_ready = service && !service->IsReady();
  if (not_yet_ready) {
    // Service is not yet ready. Listen for changes in its readiness so we can
    // write the value once it is ready. Uses weak pointers, so if everything
    // is shutdown and deleted in the meantime, this callback isn't run.
    service->IsOwnerAsync(
        base::Bind(&StatsReportingController::SetWithServiceCallback,
                   this->as_weak_ptr(), service->as_weak_ptr(), enabled));
  } else {
    // Service is either null, or ready - use it right now.
    SetWithService(service, enabled);
  }
}

void StatsReportingController::SetWithServiceCallback(
    const base::WeakPtr<ownership::OwnerSettingsService>& service,
    bool enabled,
    bool is_owner) {
  if (service)  // Make sure service wasn't deleted in the meantime.
    SetWithService(service.get(), enabled);
}

void StatsReportingController::SetWithService(
    ownership::OwnerSettingsService* service,  // Can be null for non-owners.
    bool enabled) {
  if (service && service->IsOwner()) {
    service->SetBoolean(kStatsReportingPref, enabled);
    ClearPendingValue();
  } else {
    // Do nothing since we are not the owner.
    LOG(WARNING) << "Changing settings from non-owner, setting="
                 << kStatsReportingPref;
  }
}

DeviceSettingsService::OwnershipStatus
StatsReportingController::GetOwnershipStatus() {
  return DeviceSettingsService::Get()->GetOwnershipStatus();
}

ownership::OwnerSettingsService*
StatsReportingController::GetOwnerSettingsService(Profile* profile) {
  return OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(profile);
}

bool StatsReportingController::GetPendingValue(bool* result) {
  if (local_state_->HasPrefPath(kPendingPref)) {
    *result = local_state_->GetBoolean(kPendingPref);
    return true;
  }
  return false;
}

void StatsReportingController::ClearPendingValue() {
  local_state_->ClearPref(kPendingPref);
}

bool StatsReportingController::GetActualValue(bool* result) {
  return CrosSettings::Get()->GetBoolean(kStatsReportingPref, result);
}

}  // namespace chromeos
