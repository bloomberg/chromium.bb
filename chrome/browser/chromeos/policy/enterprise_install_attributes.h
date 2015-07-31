// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ENTERPRISE_INSTALL_ATTRIBUTES_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ENTERPRISE_INSTALL_ATTRIBUTES_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

// Brokers access to the enterprise-related installation-time attributes on
// ChromeOS.
// TODO(zelidrag, mnissler): Rename + move this class - http://crbug.com/249513.
class EnterpriseInstallAttributes {
 public:
  // EnterpriseInstallAttributes status codes.  Do not change the numeric ids or
  // the meaning of the existing codes to preserve the interpretability of old
  // logfiles.
  enum LockResult {
    LOCK_SUCCESS = 0,          // Success.
    LOCK_NOT_READY = 1,        // Backend/TPM still initializing.
    LOCK_TIMEOUT = 2,          // Backend/TPM timed out.
    LOCK_BACKEND_INVALID = 3,  // Backend failed to initialize.
    LOCK_ALREADY_LOCKED = 4,   // TPM has already been locked.
    LOCK_SET_ERROR = 5,        // Failed to set attributes.
    LOCK_FINALIZE_ERROR = 6,   // Backend failed to lock.
    LOCK_READBACK_ERROR = 7,   // Inconsistency reading back registration data.
    LOCK_WRONG_DOMAIN = 8,     // Device already registered to another domain.
    LOCK_WRONG_MODE = 9,       // Device already locked to a different mode.
  };

  // A callback to handle responses of methods returning a LockResult value.
  typedef base::Callback<void(LockResult lock_result)> LockResultCallback;

  // Return serialized InstallAttributes of an enterprise-owned configuration.
  static std::string GetEnterpriseOwnedInstallAttributesBlobForTesting(
      const std::string& user_name);

  explicit EnterpriseInstallAttributes(
      chromeos::CryptohomeClient* cryptohome_client);
  ~EnterpriseInstallAttributes();

  // Tries to read install attributes from the cache file which is created early
  // during the boot process.  The cache file is used to work around slow
  // cryptohome startup, which takes a while to register its DBus interface.
  // (See http://crosbug.com/37367 for background on this.)
  void Init(const base::FilePath& cache_file);

  // Makes sure the local caches for enterprise-related install attributes are
  // up-to-date with what cryptohome has. This method checks the readiness of
  // attributes and read them if ready. Actual read will be performed in
  // ReadAttributesIfReady().
  void ReadImmutableAttributes(const base::Closure& callback);

  // Locks the device to be an enterprise device registered by the given user.
  // This can also be called after the lock has already been taken, in which
  // case it checks that the passed user agrees with the locked attribute.
  // |callback| must not be null and is called with the result.  Must not be
  // called while a previous LockDevice() invocation is still pending.
  void LockDevice(const std::string& user,
                  DeviceMode device_mode,
                  const std::string& device_id,
                  const LockResultCallback& callback);

  // Checks whether this is an enterprise device.
  bool IsEnterpriseDevice() const;

  // Checks whether this is a consumer kiosk enabled device.
  bool IsConsumerKioskDeviceWithAutoLaunch();

  // Gets the domain this device belongs to or an empty string if the device is
  // not an enterprise device.
  std::string GetDomain() const;

  // Gets the user that registered the device. Returns an empty string if the
  // device is not an enterprise device.
  std::string GetRegistrationUser();

  // Gets the device id that was generated when the device was registered.
  // Returns an empty string if the device is not an enterprise device or the
  // device id was not stored in the lockbox (prior to R19).
  std::string GetDeviceId();

  // Gets the mode the device was enrolled to. The return value for devices that
  // are not locked yet will be DEVICE_MODE_UNKNOWN.
  DeviceMode GetMode();

 protected:
  // True if install attributes have been read successfully.  False if read
  // failed or no read attempt was made.
  bool device_locked_;

  // Whether the TPM / install attributes consistency check is running.
  bool consistency_check_running_;

  // To be run after the consistency check has finished.
  base::Closure post_check_action_;

  // Wether the LockDevice() initiated TPM calls are running.
  bool device_lock_running_;

  std::string registration_user_;
  std::string registration_domain_;
  std::string registration_device_id_;
  DeviceMode registration_mode_;

 private:
  FRIEND_TEST_ALL_PREFIXES(EnterpriseInstallAttributesTest,
                           DeviceLockedFromOlderVersion);
  FRIEND_TEST_ALL_PREFIXES(EnterpriseInstallAttributesTest, Init);
  FRIEND_TEST_ALL_PREFIXES(EnterpriseInstallAttributesTest,
                           InitForConsumerKiosk);
  FRIEND_TEST_ALL_PREFIXES(EnterpriseInstallAttributesTest,
                           VerifyFakeInstallAttributesCache);

  // Constants for the possible device modes that can be stored in the lockbox.
  static const char kConsumerDeviceMode[];
  static const char kEnterpriseDeviceMode[];
  static const char kLegacyRetailDeviceMode[];
  static const char kConsumerKioskDeviceMode[];
  static const char kUnknownDeviceMode[];

  // Field names in the lockbox.
  static const char kAttrEnterpriseDeviceId[];
  static const char kAttrEnterpriseDomain[];
  static const char kAttrEnterpriseMode[];
  static const char kAttrEnterpriseOwned[];
  static const char kAttrEnterpriseUser[];
  static const char kAttrConsumerKioskEnabled[];

  // Translates DeviceMode constants to strings used in the lockbox.
  std::string GetDeviceModeString(DeviceMode mode);

  // Translates strings used in the lockbox to DeviceMode values.
  DeviceMode GetDeviceModeFromString(const std::string& mode);

  // Decodes the install attributes provided in |attr_map|.
  void DecodeInstallAttributes(
      const std::map<std::string, std::string>& attr_map);

  // Helper for ReadImmutableAttributes.
  void ReadAttributesIfReady(
      const base::Closure& callback,
      chromeos::DBusMethodCallStatus call_status,
      bool result);

  // Helper for LockDevice(). Handles the result of InstallAttributesIsReady()
  // and continue processing LockDevice if the result is true.
  void LockDeviceIfAttributesIsReady(
      const std::string& user,
      DeviceMode device_mode,
      const std::string& device_id,
      const LockResultCallback& callback,
      chromeos::DBusMethodCallStatus call_status,
      bool result);

  // Confirms the registered user and invoke the callback.
  void OnReadImmutableAttributes(const std::string& user,
                                 const LockResultCallback& callback);

  // Check state of install attributes against TPM lock state and generate UMA
  // for the result.  Asynchronously retry |dbus_retries| times in case of DBUS
  // errors (cryptohomed startup is slow).
  void TriggerConsistencyCheck(int dbus_retries);

  // Callback for TpmIsOwned() DBUS call.  Generates UMA or schedules retry in
  // case of DBUS error.
  void OnTpmOwnerCheckCompleted(int dbus_retries_remaining,
                                chromeos::DBusMethodCallStatus call_status,
                                bool result);

  chromeos::CryptohomeClient* cryptohome_client_;

  base::WeakPtrFactory<EnterpriseInstallAttributes> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseInstallAttributes);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ENTERPRISE_INSTALL_ATTRIBUTES_H_
