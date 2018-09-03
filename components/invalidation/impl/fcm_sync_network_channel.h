// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Simple system resources class that uses the current thread for scheduling.
// Assumes the current thread is already running tasks.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_SYNC_NETWORK_CHANNEL_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_SYNC_NETWORK_CHANNEL_H_

#include "base/callback.h"
#include "base/observer_list.h"
#include "components/invalidation/impl/network_channel.h"
#include "components/invalidation/public/invalidator_state.h"

namespace syncer {

// FCMSyncNetworkChannel implements common tasks needed from the network by
// client:
//  - registering message callbacks
//  - notifying on network problems
class FCMSyncNetworkChannel : public NetworkChannel {
 public:
  class Observer {
   public:
    virtual void OnFCMSyncNetworkChannelStateChanged(
        InvalidatorState invalidator_state) = 0;
  };

  FCMSyncNetworkChannel();
  ~FCMSyncNetworkChannel() override;

  void SetMessageReceiver(MessageCallback incoming_receiver) override;
  void SetTokenReceiver(TokenCallback token_receiver) override;

  // Classes interested in network channel state changes should implement
  // FCMSyncNetworkChannel::Observer and register here.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Get the count of how many valid received messages were received.
  int GetReceivedMessagesCount() const;

  // Subclass should call NotifyNetworkStatusChange to notify about network
  // changes. This triggers cacheinvalidation to try resending failed message
  // ahead of schedule when client comes online or IP address changes.
  void NotifyNetworkStatusChange(bool online);

  // Subclass should notify about connection state through
  // NotifyChannelStateChange. If communication doesn't work and it is possible
  // that invalidations from server will not reach this client then channel
  // should call this function with TRANSIENT_INVALIDATION_ERROR.
  void NotifyChannelStateChange(InvalidatorState invalidator_state);

  // Subclass should call DeliverIncomingMessage for message to reach
  // invalidations library.
  bool DeliverIncomingMessage(const std::string& payload,
                              const std::string& private_topic,
                              const std::string& public_topic,
                              const std::string& version);

  // Subclass should call DeliverToken for token to reach registration
  // manager.
  bool DeliverToken(const std::string& token);

 private:
  // Callbacks into invalidation library
  MessageCallback incoming_receiver_;
  TokenCallback token_receiver_;

  int received_messages_count_;
  std::string token_;

  base::ObserverList<Observer>::Unchecked observers_;
};
}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_SYNC_NETWORK_CHANNEL_H_
