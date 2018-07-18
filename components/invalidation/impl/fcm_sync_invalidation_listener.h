// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_SYNC_INVALIDATION_LISTENER_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_SYNC_INVALIDATION_LISTENER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/impl/fcm_sync_network_channel.h"
#include "components/invalidation/impl/invalidation_client.h"
#include "components/invalidation/impl/invalidation_listener.h"
#include "components/invalidation/impl/logger.h"
#include "components/invalidation/impl/per_user_topic_registration_manager.h"
#include "components/invalidation/impl/unacked_invalidation_set.h"
#include "components/invalidation/public/ack_handler.h"
#include "components/invalidation/public/invalidation_object_id.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace syncer {

class ObjectIdInvalidationMap;

// A simple wrapper around PerUserTopicInvalidationClient that
// handles all the startup/shutdown details and hookups.
// By implementing the AckHandler interface it tracks the messages
// which were passed to InvalidationHandlers.
class FCMSyncInvalidationListener : public InvalidationListener,
                                    public AckHandler,
                                    FCMSyncNetworkChannel::Observer {
 public:
  typedef base::OnceCallback<std::unique_ptr<InvalidationClient>(
      NetworkChannel* network_channel,
      Logger* logger,
      InvalidationListener*)>
      CreateInvalidationClientCallback;

  class Delegate {
   public:
    virtual ~Delegate();

    virtual void OnInvalidate(const ObjectIdInvalidationMap& invalidations) = 0;

    virtual void OnInvalidatorStateChange(InvalidatorState state) = 0;
  };

  explicit FCMSyncInvalidationListener(
      std::unique_ptr<FCMSyncNetworkChannel> network_channel);

  ~FCMSyncInvalidationListener() override;

  void Start(
      CreateInvalidationClientCallback create_invalidation_client_callback,
      Delegate* delegate,
      std::unique_ptr<PerUserTopicRegistrationManager>
          per_user_topic_registration_manager);

  // Update the set of object IDs that we're interested in getting
  // notifications for. May be called at any time.
  void UpdateRegisteredIds(const ObjectIdSet& ids);

  // InvalidationListener implementation.
  void Ready(InvalidationClient* client) override;
  void Invalidate(InvalidationClient* client,
                  const invalidation::Invalidation& invalidation) override;
  void InvalidateUnknownVersion(
      InvalidationClient* client,
      const invalidation::ObjectId& object_id) override;
  void InvalidateAll(InvalidationClient* client) override;
  void InformError(InvalidationClient* client,
                   const invalidation::ErrorInfo& error_info) override;
  void InformTokenRecieved(InvalidationClient* client,
                           const std::string& token) override;

  // AckHandler implementation.
  void Acknowledge(const invalidation::ObjectId& id,
                   const syncer::AckHandle& handle) override;
  void Drop(const invalidation::ObjectId& id,
            const syncer::AckHandle& handle) override;

  // FCMSyncNetworkChannel::Observer implementation.
  void OnFCMSyncNetworkChannelStateChanged(
      InvalidatorState invalidator_state) override;

  void DoRegistrationUpdate();

  void StopForTest();

  ObjectIdSet GetRegisteredIdsForTest() const;

  base::WeakPtr<FCMSyncInvalidationListener> AsWeakPtr();

 private:
  void Stop();

  InvalidatorState GetState() const;

  void EmitStateChange();

  // Sends invalidations to their appropriate destination.
  //
  // If there are no observers registered for them, they will be saved for
  // later.
  //
  // If there are observers registered, they will be saved (to make sure we
  // don't drop them until they've been acted on) and emitted to the observers.
  void DispatchInvalidations(const ObjectIdInvalidationMap& invalidations);

  // Saves invalidations.
  //
  // This call isn't synchronous so we can't guarantee these invalidations will
  // be safely on disk by the end of the call, but it should ensure that the
  // data makes it to disk eventually.
  void SaveInvalidations(const ObjectIdInvalidationMap& to_save);
  // Emits previously saved invalidations to their registered observers.
  void EmitSavedInvalidations(const ObjectIdInvalidationMap& to_emit);

  // Generate a Dictionary with all the debugging information.
  std::unique_ptr<base::DictionaryValue> CollectDebugData() const;

  std::unique_ptr<FCMSyncNetworkChannel> network_channel_;
  UnackedInvalidationsMap unacked_invalidations_map_;
  Delegate* delegate_;
  Logger logger_;
  std::unique_ptr<InvalidationClient> invalidation_client_;

  // Stored to pass to |per_user_topic_registration_manager_| on start.
  InvalidationObjectIdSet registered_ids_;

  // The states of the ticl and FCN channel.
  InvalidatorState ticl_state_;
  InvalidatorState fcm_network_state_;

  std::unique_ptr<PerUserTopicRegistrationManager>
      per_user_topic_registration_manager_;
  std::string token_;

  base::WeakPtrFactory<FCMSyncInvalidationListener> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FCMSyncInvalidationListener);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_SYNC_INVALIDATION_LISTENER_H_
