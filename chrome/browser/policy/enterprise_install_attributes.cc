// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/enterprise_install_attributes.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace policy {

namespace {
// Constants for the possible device modes that can be stored in the lockbox.
const char kConsumerDeviceMode[] = "consumer";
const char kEnterpiseDeviceMode[] = "enterprise";
const char kKioskDeviceMode[] = "kiosk";
const char kUnknownDeviceMode[] = "unknown";

// Field names in the lockbox.
const char kAttrEnterpriseDeviceId[] = "enterprise.device_id";
const char kAttrEnterpriseDomain[] = "enterprise.domain";
const char kAttrEnterpriseMode[] = "enterprise.mode";
const char kAttrEnterpriseOwned[] = "enterprise.owned";
const char kAttrEnterpriseUser[] = "enterprise.user";

// Translates DeviceMode constants to strings used in the lockbox.
std::string GetDeviceModeString(DeviceMode mode) {
  switch (mode) {
    case DEVICE_MODE_CONSUMER:
      return kConsumerDeviceMode;
    case DEVICE_MODE_ENTERPRISE:
      return kEnterpiseDeviceMode;
    case DEVICE_MODE_KIOSK:
      return kKioskDeviceMode;
    case DEVICE_MODE_PENDING:
    case DEVICE_MODE_NOT_SET:
      break;
  }
  NOTREACHED() << "Invalid device mode: " << mode;
  return kUnknownDeviceMode;
}

// Translates strings used in the lockbox to DeviceMode values.
DeviceMode GetDeviceModeFromString(
    const std::string& mode) {
  if (mode == kConsumerDeviceMode)
    return DEVICE_MODE_CONSUMER;
  else if (mode == kEnterpiseDeviceMode)
    return DEVICE_MODE_ENTERPRISE;
  else if (mode == kKioskDeviceMode)
    return DEVICE_MODE_KIOSK;
  NOTREACHED() << "Unknown device mode string: " << mode;
  return DEVICE_MODE_NOT_SET;
}

}  // namespace

EnterpriseInstallAttributes::EnterpriseInstallAttributes(
    chromeos::CryptohomeLibrary* cryptohome)
    : cryptohome_(cryptohome),
      device_locked_(false),
      registration_mode_(DEVICE_MODE_PENDING) {}

EnterpriseInstallAttributes::LockResult EnterpriseInstallAttributes::LockDevice(
    const std::string& user,
    DeviceMode device_mode,
    const std::string& device_id) {
  CHECK_NE(device_mode, DEVICE_MODE_PENDING);
  CHECK_NE(device_mode, DEVICE_MODE_NOT_SET);

  std::string domain = gaia::ExtractDomainName(user);

  // Check for existing lock first.
  if (device_locked_) {
    return !registration_domain_.empty() && domain == registration_domain_ ?
        LOCK_SUCCESS : LOCK_WRONG_USER;
  }

  if (!cryptohome_ || !cryptohome_->InstallAttributesIsReady())
    return LOCK_NOT_READY;

  // Clearing the TPM password seems to be always a good deal.
  if (cryptohome_->TpmIsEnabled() &&
      !cryptohome_->TpmIsBeingOwned() &&
      cryptohome_->TpmIsOwned()) {
    cryptohome_->TpmClearStoredPassword();
  }

  // Make sure we really have a working InstallAttrs.
  if (cryptohome_->InstallAttributesIsInvalid()) {
    LOG(ERROR) << "Install attributes invalid.";
    return LOCK_BACKEND_ERROR;
  }

  if (!cryptohome_->InstallAttributesIsFirstInstall())
    return LOCK_WRONG_USER;

  std::string mode = GetDeviceModeString(device_mode);

  // Set values in the InstallAttrs and lock it.
  if (!cryptohome_->InstallAttributesSet(kAttrEnterpriseOwned, "true") ||
      !cryptohome_->InstallAttributesSet(kAttrEnterpriseUser, user) ||
      !cryptohome_->InstallAttributesSet(kAttrEnterpriseDomain, domain) ||
      !cryptohome_->InstallAttributesSet(kAttrEnterpriseMode, mode) ||
      !cryptohome_->InstallAttributesSet(kAttrEnterpriseDeviceId, device_id)) {
    LOG(ERROR) << "Failed writing attributes";
    return LOCK_BACKEND_ERROR;
  }

  if (!cryptohome_->InstallAttributesFinalize() ||
      cryptohome_->InstallAttributesIsFirstInstall() ||
      GetRegistrationUser() != user) {
    LOG(ERROR) << "Failed locking.";
    return LOCK_BACKEND_ERROR;
  }

  return LOCK_SUCCESS;
}

bool EnterpriseInstallAttributes::IsEnterpriseDevice() {
  ReadImmutableAttributes();
  return device_locked_ && !registration_user_.empty();
}

std::string EnterpriseInstallAttributes::GetRegistrationUser() {
  ReadImmutableAttributes();

  if (!device_locked_)
    return std::string();

  return registration_user_;
}

std::string EnterpriseInstallAttributes::GetDomain() {
  if (!IsEnterpriseDevice())
    return std::string();

  return registration_domain_;
}

std::string EnterpriseInstallAttributes::GetDeviceId() {
  if (!IsEnterpriseDevice())
    return std::string();

  return registration_device_id_;
}

DeviceMode EnterpriseInstallAttributes::GetMode() {
  ReadImmutableAttributes();
  return registration_mode_;
}

void EnterpriseInstallAttributes::ReadImmutableAttributes() {
  if (device_locked_)
    return;

  if (cryptohome_ && cryptohome_->InstallAttributesIsReady()) {
    registration_mode_ = DEVICE_MODE_NOT_SET;
    if (!cryptohome_->InstallAttributesIsInvalid() &&
        !cryptohome_->InstallAttributesIsFirstInstall()) {
      device_locked_ = true;
      std::string enterprise_owned;
      std::string enterprise_user;
      if (cryptohome_->InstallAttributesGet(kAttrEnterpriseOwned,
                                            &enterprise_owned) &&
          cryptohome_->InstallAttributesGet(kAttrEnterpriseUser,
                                            &enterprise_user) &&
          enterprise_owned == "true" &&
          !enterprise_user.empty()) {
        registration_user_ = gaia::CanonicalizeEmail(enterprise_user);

        // Initialize the mode to the legacy enterprise mode here and update
        // below if more information is present.
        registration_mode_ = DEVICE_MODE_ENTERPRISE;

        // If we could extract basic setting we should try to extract the
        // extended ones too. We try to set these to defaults as good as
        // as possible if present, which could happen for device enrolled in
        // pre 19 revisions of the code, before these new attributes were added.
        if (cryptohome_->InstallAttributesGet(kAttrEnterpriseDomain,
                                              &registration_domain_)) {
          registration_domain_ = gaia::CanonicalizeDomain(registration_domain_);
        } else {
          registration_domain_ = gaia::ExtractDomainName(registration_user_);
        }
        if (!cryptohome_->InstallAttributesGet(kAttrEnterpriseDeviceId,
                                               &registration_device_id_)) {
          registration_device_id_.clear();
        }
        std::string mode;
        if (cryptohome_->InstallAttributesGet(kAttrEnterpriseMode, &mode))
          registration_mode_ = GetDeviceModeFromString(mode);
      } else if (enterprise_user.empty() && enterprise_owned != "true") {
        // |registration_user_| is empty on consumer devices.
        registration_mode_ = DEVICE_MODE_CONSUMER;
      }
    }
  }
}

}  // namespace policy
