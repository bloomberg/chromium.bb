// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_NETWORK_HANDLER_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_NETWORK_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/invalidation/impl/fcm_sync_network_channel.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace syncer {

/*
 * The class responsible for communication via GCM channel:
 *  - It retrieves the token required for the subscription
 *  and passes it by invoking token callback.
 *  - It receives the messages and passes them to the
 *  invalidation infrustructure, so they can be converted to the
 *  invalidations and consumed by listeners.
 */
class FCMNetworkHandler : public gcm::GCMAppHandler,
                          public FCMSyncNetworkChannel {
 public:
  FCMNetworkHandler(gcm::GCMDriver* gcm_driver,
                    instance_id::InstanceIDDriver* instance_id_driver);

  ~FCMNetworkHandler() override;

  void StartListening();
  void StopListening();
  void UpdateGcmChannelState(bool);

  // GCMAppHandler overrides.
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(const std::string& app_id,
                   const gcm::GCMClient::SendErrorDetails& details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

 private:
  // Called when a subscription token is obtained from the GCM server.
  void DidRetrieveToken(const std::string& subscription_token,
                        instance_id::InstanceID::Result result);

  gcm::GCMDriver* const gcm_driver_;
  instance_id::InstanceIDDriver* const instance_id_driver_;

  bool gcm_channel_online_ = false;

  base::WeakPtrFactory<FCMNetworkHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FCMNetworkHandler);
};
}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_NETWORK_HANDLER_H_
