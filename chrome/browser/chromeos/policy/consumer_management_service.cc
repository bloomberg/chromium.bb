// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace {

// Boot atttributes ID.
const char kAttributeOwnerId[] = "consumer_management.owner_id";

// The string of Status enum.
const char* const kStatusString[] = {
  "StatusUnknown",
  "StatusEnrolled",
  "StatusEnrolling",
  "StatusUnenrolled",
  "StatusUnenrolling",
};

static_assert(
    arraysize(kStatusString) == policy::ConsumerManagementService::STATUS_LAST,
    "invalid kStatusString array size");

}  // namespace

namespace policy {

ConsumerManagementService::ConsumerManagementService(
    chromeos::CryptohomeClient* client,
    chromeos::DeviceSettingsService* device_settings_service)
    : client_(client),
      device_settings_service_(device_settings_service),
      weak_ptr_factory_(this) {
  // A NULL value may be passed in tests.
  if (device_settings_service_)
    device_settings_service_->AddObserver(this);
}

ConsumerManagementService::~ConsumerManagementService() {
  if (device_settings_service_)
    device_settings_service_->RemoveObserver(this);
}

// static
void ConsumerManagementService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kConsumerManagementStage,
      ConsumerManagementStage::None().ToInternalValue());
}

void ConsumerManagementService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ConsumerManagementService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

ConsumerManagementService::Status
ConsumerManagementService::GetStatus() const {
  if (!device_settings_service_)
    return STATUS_UNKNOWN;

  const em::PolicyData* policy_data = device_settings_service_->policy_data();
  if (!policy_data)
    return STATUS_UNKNOWN;

  const ConsumerManagementStage stage = GetStage();
  if (GetManagementMode(*policy_data) == MANAGEMENT_MODE_CONSUMER_MANAGED) {
    if (stage.IsUnenrolling())
      return STATUS_UNENROLLING;
    return STATUS_ENROLLED;
  }

  if (stage.IsEnrolling())
    return STATUS_ENROLLING;

  return STATUS_UNENROLLED;
}

std::string ConsumerManagementService::GetStatusString() const {
  return kStatusString[GetStatus()];
}

ConsumerManagementStage ConsumerManagementService::GetStage() const {
  const PrefService* prefs = g_browser_process->local_state();
  const int stage = prefs->GetInteger(prefs::kConsumerManagementStage);
  return ConsumerManagementStage::FromInternalValue(stage);
}

void ConsumerManagementService::SetStage(
    const ConsumerManagementStage& stage) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(prefs::kConsumerManagementStage, stage.ToInternalValue());

  NotifyStatusChanged();
}

void ConsumerManagementService::GetOwner(const GetOwnerCallback& callback) {
  cryptohome::GetBootAttributeRequest request;
  request.set_name(kAttributeOwnerId);
  client_->GetBootAttribute(
      request,
      base::Bind(&ConsumerManagementService::OnGetBootAttributeDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void ConsumerManagementService::SetOwner(const std::string& user_id,
                                         const SetOwnerCallback& callback) {
  cryptohome::SetBootAttributeRequest request;
  request.set_name(kAttributeOwnerId);
  request.set_value(user_id.data(), user_id.size());
  client_->SetBootAttribute(
      request,
      base::Bind(&ConsumerManagementService::OnSetBootAttributeDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void ConsumerManagementService::OwnershipStatusChanged() {
}

void ConsumerManagementService::DeviceSettingsUpdated() {
  NotifyStatusChanged();
}

void ConsumerManagementService::OnDeviceSettingsServiceShutdown() {
  device_settings_service_ = nullptr;
}

void ConsumerManagementService::NotifyStatusChanged() {
  FOR_EACH_OBSERVER(Observer, observers_, OnConsumerManagementStatusChanged());
}

void ConsumerManagementService::OnGetBootAttributeDone(
    const GetOwnerCallback& callback,
    chromeos::DBusMethodCallStatus call_status,
    bool dbus_success,
    const cryptohome::BaseReply& reply) {
  if (!dbus_success || reply.error() != 0) {
    LOG(ERROR) << "Failed to get the owner info from boot lockbox.";
    callback.Run("");
    return;
  }

  callback.Run(
      reply.GetExtension(cryptohome::GetBootAttributeReply::reply).value());
}

void ConsumerManagementService::OnSetBootAttributeDone(
    const SetOwnerCallback& callback,
    chromeos::DBusMethodCallStatus call_status,
    bool dbus_success,
    const cryptohome::BaseReply& reply) {
  if (!dbus_success || reply.error() != 0) {
    LOG(ERROR) << "Failed to set owner info in boot lockbox.";
    callback.Run(false);
    return;
  }

  cryptohome::FlushAndSignBootAttributesRequest request;
  client_->FlushAndSignBootAttributes(
      request,
      base::Bind(&ConsumerManagementService::OnFlushAndSignBootAttributesDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void ConsumerManagementService::OnFlushAndSignBootAttributesDone(
    const SetOwnerCallback& callback,
    chromeos::DBusMethodCallStatus call_status,
    bool dbus_success,
    const cryptohome::BaseReply& reply) {
  if (!dbus_success || reply.error() != 0) {
    LOG(ERROR) << "Failed to flush and sign boot lockbox.";
    callback.Run(false);
    return;
  }

  callback.Run(true);
}

}  // namespace policy
