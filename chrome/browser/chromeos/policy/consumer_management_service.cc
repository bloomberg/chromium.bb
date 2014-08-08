// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"

namespace {

const char* kAttributeOwnerId = "consumer_management.owner_id";

}  // namespace

namespace policy {

ConsumerManagementService::ConsumerManagementService(
    chromeos::CryptohomeClient* client) : client_(client),
                                          weak_ptr_factory_(this) {
}

ConsumerManagementService::~ConsumerManagementService() {
}

// static
void ConsumerManagementService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kConsumerManagementEnrollmentState, ENROLLMENT_NONE);
}

ConsumerManagementService::EnrollmentState
ConsumerManagementService::GetEnrollmentState() const {
  const PrefService* prefs = g_browser_process->local_state();
  int state = prefs->GetInteger(prefs::kConsumerManagementEnrollmentState);
  if (state < 0 || state >= ENROLLMENT_LAST) {
    LOG(ERROR) << "Unknown enrollment state: " << state;
    state = 0;
  }
  return static_cast<EnrollmentState>(state);
}

void ConsumerManagementService::SetEnrollmentState(EnrollmentState state) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(prefs::kConsumerManagementEnrollmentState, state);
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
