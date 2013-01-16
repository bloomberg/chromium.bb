// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_PROVIDER_H_

#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_value_map.h"
#include "chrome/browser/chromeos/settings/cros_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"

namespace base {
class Value;
}

namespace enterprise_management {
class ChromeDeviceSettingsProto;
}  // namespace enterprise_management

namespace chromeos {

// CrosSettingsProvider implementation that works with device settings.
class DeviceSettingsProvider : public CrosSettingsProvider,
                               public DeviceSettingsService::Observer {
 public:
  DeviceSettingsProvider(const NotifyObserversCallback& notify_cb,
                         DeviceSettingsService* device_settings_service);
  virtual ~DeviceSettingsProvider();

  // Returns true if |path| is handled by this provider.
  static bool IsDeviceSetting(const std::string& name);

  // CrosSettingsProvider implementation.
  virtual const base::Value* Get(const std::string& path) const OVERRIDE;
  virtual TrustedStatus PrepareTrustedValues(
      const base::Closure& callback) OVERRIDE;
  virtual bool HandlesSetting(const std::string& path) const OVERRIDE;

 private:
  // CrosSettingsProvider implementation:
  virtual void DoSet(const std::string& path,
                     const base::Value& value) OVERRIDE;

  // DeviceSettingsService::Observer implementation:
  virtual void OwnershipStatusChanged() OVERRIDE;
  virtual void DeviceSettingsUpdated() OVERRIDE;

  // Populates in-memory cache from the local_state cache that is used to store
  // device settings before the device is owned and to speed up policy
  // availability before the policy blob is fetched on boot.
  void RetrieveCachedData();

  // Stores a value from the |pending_changes_| queue in the device settings.
  // If the device is not owned yet the data ends up only in the local_state
  // cache and is serialized once ownership is acquired.
  void SetInPolicy();

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

  // Parses the policy data and fills in |values_cache_|.
  void UpdateValuesCache(
      const enterprise_management::PolicyData& policy_data,
      const enterprise_management::ChromeDeviceSettingsProto& settings,
      TrustedStatus trusted_status);

  // Applies the metrics policy and if not set migrates the legacy file.
  void ApplyMetricsSetting(bool use_file, bool new_value);

  // Applies the data roaming policy.
  void ApplyRoamingSetting(bool new_value);

  // Applies any changes of the policies that are not handled by the respective
  // subsystems.
  void ApplySideEffects(
      const enterprise_management::ChromeDeviceSettingsProto& settings);

  // In case of missing policy blob we should verify if this is upgrade of
  // machine owned from pre version 12 OS and the user never touched the device
  // settings. In this case revert to defaults and let people in until the owner
  // comes and changes that.
  bool MitigateMissingPolicy();

  // Checks if the current cache value can be trusted for being representative
  // for the disk cache.
  TrustedStatus RequestTrustedEntity();

  // Invokes UpdateFromService() to synchronize with |device_settings_service_|,
  // then triggers the next store operation if applicable.
  void UpdateAndProceedStoring();

  // Re-reads state from |device_settings_service_|, adjusts
  // |trusted_status_| and calls UpdateValuesCache() if applicable. Returns true
  // if new settings have been loaded.
  bool UpdateFromService();

  // Sends |device_settings_| to |device_settings_service_| for signing and
  // storage in session_manager.
  void StoreDeviceSettings();

  // Checks the current ownership status to see whether the device owner is
  // logged in and writes the data accumulated in |migration_values_| to proper
  // device settings.
  void AttemptMigration();

  // Pending callbacks that need to be invoked after settings verification.
  std::vector<base::Closure> callbacks_;

  DeviceSettingsService* device_settings_service_;
  mutable PrefValueMap migration_values_;

  TrustedStatus trusted_status_;
  DeviceSettingsService::OwnershipStatus ownership_status_;

  // The device settings as currently reported through the CrosSettingsProvider
  // interface. This may be different from the actual current device settings
  // (which can be obtained from |device_settings_service_|) in case the device
  // does not have an owner yet or there are pending changes that have not yet
  // been written to session_manager.
  enterprise_management::ChromeDeviceSettingsProto device_settings_;

  // A cache of values, indexed by the settings keys served through the
  // CrosSettingsProvider interface. This is always kept in sync with the raw
  // data found in |device_settings_|.
  PrefValueMap values_cache_;

  // This is a queue for set requests, because those need to be sequential.
  typedef std::pair<std::string, base::Value*> PendingQueueElement;
  std::deque<PendingQueueElement> pending_changes_;

  // Weak pointer factory for creating store operation callbacks.
  base::WeakPtrFactory<DeviceSettingsProvider> store_callback_factory_;

  friend class DeviceSettingsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(DeviceSettingsProviderTest,
                           InitializationTestUnowned);
  FRIEND_TEST_ALL_PREFIXES(DeviceSettingsProviderTest,
                           PolicyFailedPermanentlyNotification);
  FRIEND_TEST_ALL_PREFIXES(DeviceSettingsProviderTest, PolicyLoadNotification);
  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_PROVIDER_H_
