// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_SETTINGS_PROVIDER_H_
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/signed_settings_migration_helper.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class Value;
}

namespace chromeos {

// CrosSettingsProvider implementation that works with SignedSettings.
class DeviceSettingsProvider : public CrosSettingsProvider,
                               public content::NotificationObserver {
 public:
  DeviceSettingsProvider(const NotifyObserversCallback& notify_cb,
                         SignedSettingsHelper* signed_settings_helper);
  virtual ~DeviceSettingsProvider();

  // CrosSettingsProvider implementation.
  virtual const base::Value* Get(const std::string& path) const OVERRIDE;
  virtual TrustedStatus PrepareTrustedValues(
      const base::Closure& callback) OVERRIDE;
  virtual bool HandlesSetting(const std::string& path) const OVERRIDE;
  virtual void Reload() OVERRIDE;

 private:
  // CrosSettingsProvider implementation:
  virtual void DoSet(const std::string& path,
                     const base::Value& value) OVERRIDE;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  const enterprise_management::PolicyData policy() const;

  // Populates in-memory cache from the local_state cache that is used to store
  // signed settings before the device is owned and to speed up policy
  // availability before the policy blob is fetched on boot.
  void RetrieveCachedData();

  // Stores a value from the |pending_changes_| queue in the signed settings.
  // If the device is not owned yet the data ends up only in the local_state
  // cache and is serialized once ownership is acquired.
  void SetInPolicy();

  // Finalizes stores to the policy file if the cache is dirty.
  void FinishSetInPolicy(
      SignedSettings::ReturnCode code,
      const enterprise_management::PolicyFetchResponse& policy);

  // Decode the various groups of policies.
  void DecodeLoginPolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PrefValueMap* new_values_cache) const;
  void DecodeKioskPolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PrefValueMap* new_values_cache) const;
  void DecodeNetworkPolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PrefValueMap* new_values_cache) const;
  void DecodeReportingPolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PrefValueMap* new_values_cache) const;
  void DecodeGenericPolicies(
      const enterprise_management::ChromeDeviceSettingsProto& policy,
      PrefValueMap* new_values_cache) const;

  // Parses the policy cache and fills the cache of base::Value objects.
  void UpdateValuesCache();

  // Applies the metrics policy and if not set migrates the legacy file.
  void ApplyMetricsSetting(bool use_file, bool new_value) const;

  // Applies the data roaming policy.
  void ApplyRoamingSetting(bool new_value) const;

  // Applies any changes of the policies that are not handled by the respective
  // subsystems.
  void ApplySideEffects() const;

  // In case of missing policy blob we should verify if this is upgrade of
  // machine owned from pre version 12 OS and the user never touched the device
  // settings. In this case revert to defaults and let people in until the owner
  // comes and changes that.
  bool MitigateMissingPolicy();

  // Called right before boolean property is changed.
  void OnBooleanPropertyChange(const std::string& path, bool new_value);

  // Checks if the current cache value can be trusted for being representative
  // for the disk cache.
  TrustedStatus RequestTrustedEntity();

  // Called right after signed value was checked.
  void OnPropertyRetrieve(const std::string& path,
                          const base::Value* value,
                          bool use_default_value);

  // Callback of StorePolicyOp for ordinary policy stores.
  void OnStorePolicyCompleted(SignedSettings::ReturnCode code);

  // Callback of RetrievePolicyOp for ordinary policy [re]loads.
  void OnRetrievePolicyCompleted(
      SignedSettings::ReturnCode code,
      const enterprise_management::PolicyFetchResponse& policy);

  // These setters are for test use only.
  void set_ownership_status(OwnershipService::Status status) {
    ownership_status_ = status;
  }
  void set_trusted_status(TrustedStatus status) {
    trusted_status_ = status;
  }
  void set_retries_left(int retries) {
    retries_left_ = retries;
  }

  // Pending callbacks that need to be invoked after settings verification.
  std::vector<base::Closure> callbacks_;

  SignedSettingsHelper* signed_settings_helper_;
  OwnershipService::Status ownership_status_;
  mutable scoped_ptr<SignedSettingsMigrationHelper> migration_helper_;

  content::NotificationRegistrar registrar_;

  // In order to guard against occasional failure to fetch a property
  // we allow for some number of retries.
  int retries_left_;

  enterprise_management::PolicyData policy_;
  TrustedStatus trusted_status_;

  PrefValueMap values_cache_;

  // This is a queue for set requests, because those need to be sequential.
  typedef std::pair<std::string, base::Value*> PendingQueueElement;
  std::vector<PendingQueueElement> pending_changes_;

  friend class DeviceSettingsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(DeviceSettingsProviderTest,
                           InitializationTestUnowned);
  FRIEND_TEST_ALL_PREFIXES(DeviceSettingsProviderTest,
                           PolicyFailedPermanentlyNotification);
  FRIEND_TEST_ALL_PREFIXES(DeviceSettingsProviderTest, PolicyLoadNotification);
  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_SETTINGS_PROVIDER_H_
