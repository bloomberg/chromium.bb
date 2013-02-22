// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ENTERPRISE_INSTALL_ATTRIBUTES_H_
#define CHROME_BROWSER_POLICY_ENTERPRISE_INSTALL_ATTRIBUTES_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "chrome/browser/policy/cloud_policy_constants.h"

namespace chromeos {
class CryptohomeLibrary;
}

namespace policy {

// Brokers access to the enterprise-related installation-time attributes on
// ChromeOS.
class EnterpriseInstallAttributes {
 public:
  // Return codes for LockDevice().
  enum LockResult {
    LOCK_SUCCESS,
    LOCK_NOT_READY,
    LOCK_BACKEND_ERROR,
    LOCK_WRONG_USER,
  };

  // Constants for the possible device modes that can be stored in the lockbox.
  static const char kConsumerDeviceMode[];
  static const char kEnterpiseDeviceMode[];
  static const char kKioskDeviceMode[];
  static const char kUnknownDeviceMode[];

  // Field names in the lockbox.
  static const char kAttrEnterpriseDeviceId[];
  static const char kAttrEnterpriseDomain[];
  static const char kAttrEnterpriseMode[];
  static const char kAttrEnterpriseOwned[];
  static const char kAttrEnterpriseUser[];

  explicit EnterpriseInstallAttributes(chromeos::CryptohomeLibrary* cryptohome);

  // Reads data from the cache file. The cache file is used to work around slow
  // cryptohome startup, which takes a while to register its DBus interface.
  // See http://crosbug.com/37367 for background on this.
  void ReadCacheFile(const base::FilePath& cache_file);

  // Makes sure the local caches for enterprise-related install attributes are
  // up-to-date with what cryptohome has.
  void ReadImmutableAttributes();

  // Locks the device to be an enterprise device registered by the given user.
  // This can also be called after the lock has already been taken, in which
  // case it checks that the passed user agrees with the locked attribute.
  LockResult LockDevice(const std::string& user,
                        DeviceMode device_mode,
                        const std::string& device_id) WARN_UNUSED_RESULT;

  // Checks whether this is an enterprise device.
  bool IsEnterpriseDevice();

  // Gets the domain this device belongs to or an empty string if the device is
  // not an enterprise device.
  std::string GetDomain();

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

 private:
  // Decodes the install attributes provided in |attr_map|.
  void DecodeInstallAttributes(
      const std::map<std::string, std::string>& attr_map);

  chromeos::CryptohomeLibrary* cryptohome_;

  bool device_locked_;
  std::string registration_user_;
  std::string registration_domain_;
  std::string registration_device_id_;
  DeviceMode registration_mode_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseInstallAttributes);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ENTERPRISE_INSTALL_ATTRIBUTES_H_
