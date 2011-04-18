// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/enterprise_install_attributes.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"

static const char kAttrEnterpriseOwned[] = "enterprise.owned";
static const char kAttrEnterpriseUser[] = "enterprise.user";

namespace policy {

EnterpriseInstallAttributes::EnterpriseInstallAttributes(
    chromeos::CryptohomeLibrary* cryptohome)
    : cryptohome_(cryptohome),
      device_locked_(false) {}

EnterpriseInstallAttributes::LockResult EnterpriseInstallAttributes::LockDevice(
    const std::string& user) {
  // Check for existing lock first.
  if (device_locked_) {
    return !registration_user_.empty() && user == registration_user_ ?
        LOCK_SUCCESS : LOCK_WRONG_USER;
  }

  if (!cryptohome_->InstallAttributesIsReady())
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

  // Set values in the InstallAttrs and lock it.
  if (!cryptohome_->InstallAttributesSet(kAttrEnterpriseOwned, "true") ||
      !cryptohome_->InstallAttributesSet(kAttrEnterpriseUser, user)) {
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

  std::string domain;
  size_t pos = registration_user_.find('@');
  if (pos != std::string::npos)
    domain = registration_user_.substr(pos + 1);

  return domain;
}

void EnterpriseInstallAttributes::ReadImmutableAttributes() {
  if (device_locked_)
    return;

  if (cryptohome_ &&
      cryptohome_->InstallAttributesIsReady() &&
      !cryptohome_->InstallAttributesIsInvalid() &&
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
      registration_user_ = enterprise_user;
    }
  }
}

}  // namespace policy
