// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/proto/install_attributes.pb.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace policy {

namespace cryptohome_util = chromeos::cryptohome_util;

namespace {

// Number of TPM lock state query retries during consistency check.
int kDbusRetryCount = 12;

// Interval of TPM lock state query retries during consistency check.
int kDbusRetryIntervalInSeconds = 5;

bool ReadMapKey(const std::map<std::string, std::string>& map,
                const std::string& key,
                std::string* value) {
  std::map<std::string, std::string>::const_iterator entry = map.find(key);
  if (entry == map.end())
    return false;

  *value = entry->second;
  return true;
}

}  // namespace

// static
std::string
EnterpriseInstallAttributes::GetEnterpriseOwnedInstallAttributesBlobForTesting(
    const std::string& user_name) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  cryptohome::SerializedInstallAttributes::Attribute* attribute = NULL;

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(EnterpriseInstallAttributes::kAttrEnterpriseOwned);
  attribute->set_value("true");

  attribute = install_attrs_proto.add_attributes();
  attribute->set_name(EnterpriseInstallAttributes::kAttrEnterpriseUser);
  attribute->set_value(user_name);

  return install_attrs_proto.SerializeAsString();
}

EnterpriseInstallAttributes::EnterpriseInstallAttributes(
    chromeos::CryptohomeClient* cryptohome_client)
    : device_locked_(false),
      consistency_check_running_(false),
      device_lock_running_(false),
      registration_mode_(DEVICE_MODE_PENDING),
      cryptohome_client_(cryptohome_client),
      weak_ptr_factory_(this) {
}

EnterpriseInstallAttributes::~EnterpriseInstallAttributes() {}

void EnterpriseInstallAttributes::Init(const base::FilePath& cache_file) {
  DCHECK_EQ(false, device_locked_);

  // The actual check happens asynchronously, thus it is ok to trigger it before
  // Init() has completed.
  TriggerConsistencyCheck(kDbusRetryCount * kDbusRetryIntervalInSeconds);

  if (!base::PathExists(cache_file))
    return;

  device_locked_ = true;

  char buf[16384];
  int len = base::ReadFile(cache_file, buf, sizeof(buf));
  if (len == -1 || len >= static_cast<int>(sizeof(buf))) {
    PLOG(ERROR) << "Failed to read " << cache_file.value();
    return;
  }

  cryptohome::SerializedInstallAttributes install_attrs_proto;
  if (!install_attrs_proto.ParseFromArray(buf, len)) {
    LOG(ERROR) << "Failed to parse install attributes cache";
    return;
  }

  google::protobuf::RepeatedPtrField<
      const cryptohome::SerializedInstallAttributes::Attribute>::iterator entry;
  std::map<std::string, std::string> attr_map;
  for (entry = install_attrs_proto.attributes().begin();
       entry != install_attrs_proto.attributes().end();
       ++entry) {
    // The protobuf values unfortunately contain terminating null characters, so
    // we have to sanitize the value here.
    attr_map.insert(std::make_pair(entry->name(),
                                   std::string(entry->value().c_str())));
  }

  DecodeInstallAttributes(attr_map);
}

void EnterpriseInstallAttributes::ReadImmutableAttributes(
    const base::Closure& callback) {
  if (device_locked_) {
    callback.Run();
    return;
  }

  cryptohome_client_->InstallAttributesIsReady(
      base::Bind(&EnterpriseInstallAttributes::ReadAttributesIfReady,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void EnterpriseInstallAttributes::ReadAttributesIfReady(
    const base::Closure& callback,
    chromeos::DBusMethodCallStatus call_status,
    bool result) {
  if (call_status == chromeos::DBUS_METHOD_CALL_SUCCESS && result) {
    registration_mode_ = DEVICE_MODE_NOT_SET;
    if (!cryptohome_util::InstallAttributesIsInvalid() &&
        !cryptohome_util::InstallAttributesIsFirstInstall()) {
      device_locked_ = true;

      static const char* const kEnterpriseAttributes[] = {
        kAttrEnterpriseDeviceId,
        kAttrEnterpriseDomain,
        kAttrEnterpriseMode,
        kAttrEnterpriseOwned,
        kAttrEnterpriseUser,
        kAttrConsumerKioskEnabled,
      };
      std::map<std::string, std::string> attr_map;
      for (size_t i = 0; i < arraysize(kEnterpriseAttributes); ++i) {
        std::string value;
        if (cryptohome_util::InstallAttributesGet(kEnterpriseAttributes[i],
                                                  &value))
          attr_map[kEnterpriseAttributes[i]] = value;
      }

      DecodeInstallAttributes(attr_map);
    }
  }
  callback.Run();
}

void EnterpriseInstallAttributes::LockDevice(
    const std::string& user,
    DeviceMode device_mode,
    const std::string& device_id,
    const LockResultCallback& callback) {
  DCHECK(!callback.is_null());
  CHECK_EQ(device_lock_running_, false);
  CHECK_NE(device_mode, DEVICE_MODE_PENDING);
  CHECK_NE(device_mode, DEVICE_MODE_NOT_SET);

  // Check for existing lock first.
  if (device_locked_) {
    if (device_mode != registration_mode_) {
      callback.Run(LOCK_WRONG_MODE);
      return;
    }

    switch (registration_mode_) {
      case DEVICE_MODE_ENTERPRISE:
      case DEVICE_MODE_LEGACY_RETAIL_MODE: {
        // Check domain match for enterprise devices.
        std::string domain = gaia::ExtractDomainName(user);
        if (registration_domain_.empty() || domain != registration_domain_) {
          callback.Run(LOCK_WRONG_DOMAIN);
          return;
        }
        break;
      }
      case DEVICE_MODE_NOT_SET:
      case DEVICE_MODE_PENDING:
        // This case can't happen due to the CHECK_NE asserts above.
        NOTREACHED();
        break;
      case DEVICE_MODE_CONSUMER:
      case DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH:
        // The user parameter is ignored for consumer devices.
        break;
    }

    // Already locked in the right mode, signal success.
    callback.Run(LOCK_SUCCESS);
    return;
  }

  // In case the consistency check is still running, postpone the device locking
  // until it has finished.  This should not introduce additional delay since
  // device locking must wait for TPM initialization anyways.
  if (consistency_check_running_) {
    CHECK(post_check_action_.is_null());
    post_check_action_ = base::Bind(&EnterpriseInstallAttributes::LockDevice,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    user,
                                    device_mode,
                                    device_id,
                                    callback);
    return;
  }

  device_lock_running_ = true;
  cryptohome_client_->InstallAttributesIsReady(
      base::Bind(&EnterpriseInstallAttributes::LockDeviceIfAttributesIsReady,
                 weak_ptr_factory_.GetWeakPtr(),
                 user,
                 device_mode,
                 device_id,
                 callback));
}

void EnterpriseInstallAttributes::LockDeviceIfAttributesIsReady(
    const std::string& user,
    DeviceMode device_mode,
    const std::string& device_id,
    const LockResultCallback& callback,
    chromeos::DBusMethodCallStatus call_status,
    bool result) {
  if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS || !result) {
    device_lock_running_ = false;
    callback.Run(LOCK_NOT_READY);
    return;
  }

  // Clearing the TPM password seems to be always a good deal.
  if (cryptohome_util::TpmIsEnabled() &&
      !cryptohome_util::TpmIsBeingOwned() &&
      cryptohome_util::TpmIsOwned()) {
    cryptohome_client_->CallTpmClearStoredPasswordAndBlock();
  }

  // Make sure we really have a working InstallAttrs.
  if (cryptohome_util::InstallAttributesIsInvalid()) {
    LOG(ERROR) << "Install attributes invalid.";
    device_lock_running_ = false;
    callback.Run(LOCK_BACKEND_INVALID);
    return;
  }

  if (!cryptohome_util::InstallAttributesIsFirstInstall()) {
    LOG(ERROR) << "Install attributes already installed.";
    device_lock_running_ = false;
    callback.Run(LOCK_ALREADY_LOCKED);
    return;
  }

  std::string mode = GetDeviceModeString(device_mode);
  std::string registration_user;
  if (!user.empty())
    registration_user = gaia::CanonicalizeEmail(user);

  if (device_mode == DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH) {
    // Set values in the InstallAttrs and lock it.
    if (!cryptohome_util::InstallAttributesSet(kAttrConsumerKioskEnabled,
                                               "true")) {
      LOG(ERROR) << "Failed writing attributes.";
      device_lock_running_ = false;
      callback.Run(LOCK_SET_ERROR);
      return;
    }
  } else {
    std::string domain = gaia::ExtractDomainName(registration_user);
    // Set values in the InstallAttrs and lock it.
    if (!cryptohome_util::InstallAttributesSet(kAttrEnterpriseOwned, "true") ||
        !cryptohome_util::InstallAttributesSet(kAttrEnterpriseUser,
                                               registration_user) ||
        !cryptohome_util::InstallAttributesSet(kAttrEnterpriseDomain,
                                               domain) ||
        !cryptohome_util::InstallAttributesSet(kAttrEnterpriseMode, mode) ||
        !cryptohome_util::InstallAttributesSet(kAttrEnterpriseDeviceId,
                                               device_id)) {
      LOG(ERROR) << "Failed writing attributes.";
      device_lock_running_ = false;
      callback.Run(LOCK_SET_ERROR);
      return;
    }
  }

  if (!cryptohome_util::InstallAttributesFinalize() ||
      cryptohome_util::InstallAttributesIsFirstInstall()) {
    LOG(ERROR) << "Failed locking.";
    device_lock_running_ = false;
    callback.Run(LOCK_FINALIZE_ERROR);
    return;
  }

  ReadImmutableAttributes(
      base::Bind(&EnterpriseInstallAttributes::OnReadImmutableAttributes,
                 weak_ptr_factory_.GetWeakPtr(),
                 registration_user,
                 callback));
}

void EnterpriseInstallAttributes::OnReadImmutableAttributes(
    const std::string& registration_user,
    const LockResultCallback& callback) {

  if (GetRegistrationUser() != registration_user) {
    LOG(ERROR) << "Locked data doesn't match.";
    device_lock_running_ = false;
    callback.Run(LOCK_READBACK_ERROR);
    return;
  }

  device_lock_running_ = false;
  callback.Run(LOCK_SUCCESS);
}

bool EnterpriseInstallAttributes::IsEnterpriseDevice() const {
  return device_locked_ && !registration_user_.empty();
}

bool EnterpriseInstallAttributes::IsConsumerKioskDeviceWithAutoLaunch() {
  return device_locked_ &&
         registration_mode_ == DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH;
}

std::string EnterpriseInstallAttributes::GetRegistrationUser() {
  if (!device_locked_)
    return std::string();

  return registration_user_;
}

std::string EnterpriseInstallAttributes::GetDomain() const {
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
  return registration_mode_;
}

void EnterpriseInstallAttributes::TriggerConsistencyCheck(int dbus_retries) {
  consistency_check_running_ = true;
  cryptohome_client_->TpmIsOwned(
      base::Bind(&EnterpriseInstallAttributes::OnTpmOwnerCheckCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 dbus_retries));
}

void EnterpriseInstallAttributes::OnTpmOwnerCheckCompleted(
    int dbus_retries_remaining,
    chromeos::DBusMethodCallStatus call_status,
    bool result) {
  if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS &&
      dbus_retries_remaining) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&EnterpriseInstallAttributes::TriggerConsistencyCheck,
                   weak_ptr_factory_.GetWeakPtr(),
                   dbus_retries_remaining - 1),
        base::TimeDelta::FromSeconds(kDbusRetryIntervalInSeconds));
    return;
  }

  base::HistogramBase::Sample state = device_locked_;
  state |= 0x2 * (registration_mode_ == DEVICE_MODE_ENTERPRISE);
  if (call_status == chromeos::DBUS_METHOD_CALL_SUCCESS)
    state |= 0x4 * result;
  else
    state = 0x8;  // This case is not a bit mask.
  UMA_HISTOGRAM_ENUMERATION("Enterprise.AttributesTPMConsistency", state, 9);

  // Run any action (LockDevice call) that might have queued behind the
  // consistency check.
  consistency_check_running_ = false;
  if (!post_check_action_.is_null()) {
    post_check_action_.Run();
    post_check_action_.Reset();
  }
}

// Warning: The values for these keys (but not the keys themselves) are stored
// in the protobuf with a trailing zero.  Also note that some of these constants
// have been copied to login_manager/device_policy_service.cc.  Please make sure
// that all changes to the constants are reflected there as well.
const char EnterpriseInstallAttributes::kConsumerDeviceMode[] = "consumer";
const char EnterpriseInstallAttributes::kEnterpriseDeviceMode[] = "enterprise";
const char EnterpriseInstallAttributes::kLegacyRetailDeviceMode[] = "kiosk";
const char EnterpriseInstallAttributes::kConsumerKioskDeviceMode[] =
    "consumer_kiosk";
const char EnterpriseInstallAttributes::kUnknownDeviceMode[] = "unknown";

const char EnterpriseInstallAttributes::kAttrEnterpriseDeviceId[] =
    "enterprise.device_id";
const char EnterpriseInstallAttributes::kAttrEnterpriseDomain[] =
    "enterprise.domain";
const char EnterpriseInstallAttributes::kAttrEnterpriseMode[] =
    "enterprise.mode";
const char EnterpriseInstallAttributes::kAttrEnterpriseOwned[] =
    "enterprise.owned";
const char EnterpriseInstallAttributes::kAttrEnterpriseUser[] =
    "enterprise.user";
const char EnterpriseInstallAttributes::kAttrConsumerKioskEnabled[] =
    "consumer.app_kiosk_enabled";

std::string EnterpriseInstallAttributes::GetDeviceModeString(DeviceMode mode) {
  switch (mode) {
    case DEVICE_MODE_CONSUMER:
      return EnterpriseInstallAttributes::kConsumerDeviceMode;
    case DEVICE_MODE_ENTERPRISE:
      return EnterpriseInstallAttributes::kEnterpriseDeviceMode;
    case DEVICE_MODE_LEGACY_RETAIL_MODE:
      return EnterpriseInstallAttributes::kLegacyRetailDeviceMode;
    case DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH:
      return EnterpriseInstallAttributes::kConsumerKioskDeviceMode;
    case DEVICE_MODE_PENDING:
    case DEVICE_MODE_NOT_SET:
      break;
  }
  NOTREACHED() << "Invalid device mode: " << mode;
  return EnterpriseInstallAttributes::kUnknownDeviceMode;
}

DeviceMode EnterpriseInstallAttributes::GetDeviceModeFromString(
    const std::string& mode) {
  if (mode == EnterpriseInstallAttributes::kConsumerDeviceMode)
    return DEVICE_MODE_CONSUMER;
  else if (mode == EnterpriseInstallAttributes::kEnterpriseDeviceMode)
    return DEVICE_MODE_ENTERPRISE;
  else if (mode == EnterpriseInstallAttributes::kLegacyRetailDeviceMode)
    return DEVICE_MODE_LEGACY_RETAIL_MODE;
  else if (mode == EnterpriseInstallAttributes::kConsumerKioskDeviceMode)
    return DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH;
  NOTREACHED() << "Unknown device mode string: " << mode;
  return DEVICE_MODE_NOT_SET;
}

void EnterpriseInstallAttributes::DecodeInstallAttributes(
    const std::map<std::string, std::string>& attr_map) {
  std::string enterprise_owned;
  std::string enterprise_user;
  std::string consumer_kiosk_enabled;
  if (ReadMapKey(attr_map, kAttrEnterpriseOwned, &enterprise_owned) &&
      ReadMapKey(attr_map, kAttrEnterpriseUser, &enterprise_user) &&
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
    if (ReadMapKey(attr_map, kAttrEnterpriseDomain, &registration_domain_))
      registration_domain_ = gaia::CanonicalizeDomain(registration_domain_);
    else
      registration_domain_ = gaia::ExtractDomainName(registration_user_);

    ReadMapKey(attr_map, kAttrEnterpriseDeviceId, &registration_device_id_);

    std::string mode;
    if (ReadMapKey(attr_map, kAttrEnterpriseMode, &mode))
      registration_mode_ = GetDeviceModeFromString(mode);
  } else if (ReadMapKey(attr_map,
                        kAttrConsumerKioskEnabled,
                        &consumer_kiosk_enabled) &&
             consumer_kiosk_enabled == "true") {
    registration_mode_ = DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH;
  } else if (enterprise_user.empty() && enterprise_owned != "true") {
    // |registration_user_| is empty on consumer devices.
    registration_mode_ = DEVICE_MODE_CONSUMER;
  }
}

}  // namespace policy
