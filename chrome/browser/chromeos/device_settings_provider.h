// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_SETTINGS_PROVIDER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/hash_tables.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "chrome/browser/chromeos/signed_settings_migration_helper.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/notification_registrar.h"

namespace em = enterprise_management;

class PrefService;

namespace base {
class ListValue;
}

namespace chromeos {

class OwnershipService;

// CrosSettingsProvider implementation that works with SignedSettings.
class DeviceSettingsProvider : public CrosSettingsProvider,
                               public content::NotificationObserver {
 public:
  DeviceSettingsProvider();
  virtual ~DeviceSettingsProvider();

  // CrosSettingsProvider implementation.
  virtual const base::Value* Get(const std::string& path) const OVERRIDE;
  virtual bool GetTrusted(const std::string& path,
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

  const em::PolicyData policy() const;

  // Populates in-memory cache from the local_state cache that is used to store
  // signed settings before the device is owned and to speed up policy
  // availability before the policy blob is fetched on boot.
  void RetrieveCachedData();

  // Stores a value in the signed settings. If the device is not owned yet the
  // data ends up only in the local_state cache and is serialized once ownership
  // is acquired.
  void SetInPolicy(const std::string& prop, const base::Value& value);

  // Finalizes stores to the policy file if the cache is dirty.
  void FinishSetInPolicy(const std::string& prop,
                         const base::Value* value,
                         SignedSettings::ReturnCode code,
                         const em::PolicyFetchResponse& policy);

  // Parses the policy cache and fills the cache of base::Value objects.
  void UpdateValuesCache();

  // Applies the metrics policy and if not set migrates the legacy file.
  void ApplyMetricsSetting(bool use_file, bool new_value) const;

  // Applies the data roaming policy.
  void ApplyRoamingSetting(bool new_value) const;

  // Applies any changes of the policies that are not handled by the respective
  // subsystms.
  void ApplySideEffects() const;

  // Called right before boolean property is changed.
  void OnBooleanPropertyChange(const std::string& path, bool new_value);

  // Checks if the current cache value can be trusted for being representative
  // for the disk cache.
  bool RequestTrustedEntity();

  // Called right after signed value was checked.
  void OnPropertyRetrieve(const std::string& path,
                          const base::Value* value,
                          bool use_default_value);

  // Callback of StorePolicyOp for ordinary policy stores.
  void OnStorePolicyCompleted(SignedSettings::ReturnCode code);

  // Callback of RetrievePolicyOp for ordinary policy [re]loads.
  void OnRetrievePolicyCompleted(SignedSettings::ReturnCode code,
                                 const em::PolicyFetchResponse& policy);

  // Pending callbacks that need to be invoked after settings verification.
  std::vector<base::Closure> callbacks_;

  OwnershipService::Status ownership_status_;
  mutable scoped_ptr<SignedSettingsMigrationHelper> migration_helper_;

  content::NotificationRegistrar registrar_;

  // In order to guard against occasional failure to fetch a property
  // we allow for some number of retries.
  int retries_left_;

  em::PolicyData policy_;
  bool trusted_;

  PrefValueMap values_cache_;

  friend class SignedSettingsHelper;

  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_SETTINGS_PROVIDER_H_
