// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_network_handler.h"

#include "base/base64url.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/public/invalidator_state.h"

using instance_id::InstanceID;

namespace syncer {

namespace {

const char kInvalidationsAppId[] = "com.google.chrome.fcm.invalidations";
const char kInvalidationGCMSenderId[] = "8181035976";
const char kContentKey[] = "data";

// OAuth2 Scope passed to getToken to obtain GCM registration tokens.
// Must match Java GoogleCloudMessaging.INSTANCE_ID_SCOPE.
const char kGCMScope[] = "GCM";

}  // namespace

FCMNetworkHandler::FCMNetworkHandler(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver)
    : gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      weak_ptr_factory_(this) {}

FCMNetworkHandler::~FCMNetworkHandler() {
  StopListening();
}

void FCMNetworkHandler::StartListening() {
  instance_id_driver_->GetInstanceID(kInvalidationsAppId)
      ->GetToken(kInvalidationGCMSenderId, kGCMScope,
                 /*options=*/std::map<std::string, std::string>(),
                 base::BindRepeating(&FCMNetworkHandler::DidRetrieveToken,
                                     weak_ptr_factory_.GetWeakPtr()));

  gcm_driver_->AddAppHandler(kInvalidationsAppId, this);
}

void FCMNetworkHandler::StopListening() {
  if (gcm_driver_->GetAppHandler(kInvalidationsAppId))
    gcm_driver_->RemoveAppHandler(kInvalidationsAppId);
}

void FCMNetworkHandler::DidRetrieveToken(const std::string& subscription_token,
                                         InstanceID::Result result) {
  switch (result) {
    case InstanceID::SUCCESS:
      DeliverToken(subscription_token);
      return;
    case InstanceID::INVALID_PARAMETER:
    case InstanceID::DISABLED:
    case InstanceID::ASYNC_OPERATION_PENDING:
    case InstanceID::SERVER_ERROR:
    case InstanceID::UNKNOWN_ERROR:
    case InstanceID::NETWORK_ERROR:
      DLOG(WARNING) << "Messaging subscription failed; InstanceID::Result = "
                    << result;
      UpdateGcmChannelState(false);
      break;
  }
}

void FCMNetworkHandler::UpdateGcmChannelState(bool online) {
  if (gcm_channel_online_ == online)
    return;
  gcm_channel_online_ = online;
  NotifyChannelStateChange(gcm_channel_online_ ? INVALIDATIONS_ENABLED
                                               : TRANSIENT_INVALIDATION_ERROR);
}

void FCMNetworkHandler::ShutdownHandler() {}

void FCMNetworkHandler::OnStoreReset() {}

void FCMNetworkHandler::OnMessage(const std::string& app_id,
                                  const gcm::IncomingMessage& message) {
  DCHECK_EQ(app_id, kInvalidationsAppId);
  std::string content;
  auto it = message.data.find(kContentKey);
  if (it != message.data.end())
    content = it->second;
  if (content.empty()) {
    return;
  }

  // TODO(melandory): check if content is empty and report.
  // TODO(melandory): decode base64 and report in case it is invalid.
  // TODO(melandory): parse proto and record histogram in case of invalid proto.
  // TODO(melandory): report histogram in case of success.

  UpdateGcmChannelState(true);
  DeliverIncomingMessage(content);
}

void FCMNetworkHandler::OnMessagesDeleted(const std::string& app_id) {
  // TODO(melandory): consider notifyint the client that messages were
  // deleted. So the client can act on it, e.g. in case of sync request
  // GetUpdates from the server.
}

void FCMNetworkHandler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  // Should never be called because we don't send GCM messages to
  // the server.
  NOTREACHED() << "FCMNetworkHandler doesn't send GCM messages.";
}

void FCMNetworkHandler::OnSendAcknowledged(const std::string& app_id,
                                           const std::string& message_id) {
  // Should never be called because we don't send GCM messages to
  // the server.
  NOTREACHED() << "FCMNetworkHandler doesn't send GCM messages.";
}

}  // namespace syncer
