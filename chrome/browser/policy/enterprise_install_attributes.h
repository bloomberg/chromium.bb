// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ENTERPRISE_INSTALL_ATTRIBUTES_H_
#define CHROME_BROWSER_POLICY_ENTERPRISE_INSTALL_ATTRIBUTES_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

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

  explicit EnterpriseInstallAttributes(chromeos::CryptohomeLibrary* cryptohome);

  // Locks the device to be an enterprise device registered by the given user.
  // This can also be called after the lock has already been taken, in which
  // case it checks that the passed user agrees with the locked attribute.
  LockResult LockDevice(const std::string& user) WARN_UNUSED_RESULT;

  // Checks whether this is an enterprise device.
  bool IsEnterpriseDevice();

  // Gets the domain this device belongs to or an empty string if the device is
  // not an enterprise device.
  std::string GetDomain();

  // Gets the user that registered the device. Returns an empty string if the
  // device is not an enterprise device.
  std::string GetRegistrationUser();

 private:
  // Makes sure the local caches for enterprise-related install attributes are
  // up-to-date with what cryptohome has.
  void ReadImmutableAttributes();

  chromeos::CryptohomeLibrary* cryptohome_;

  bool device_locked_;
  std::string registration_user_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseInstallAttributes);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ENTERPRISE_INSTALL_ATTRIBUTES_H_
