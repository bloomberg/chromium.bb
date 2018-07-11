// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_invalidation_client.h"

#include "base/bind.h"
#include "components/invalidation/impl/invalidation_listener.h"
#include "components/invalidation/impl/logger.h"
#include "components/invalidation/impl/network_channel.h"

namespace syncer {

PerUserTopicInvalidationClient::PerUserTopicInvalidationClient(
    NetworkChannel* network,
    Logger* logger,
    InvalidationListener* listener)
    : network_(network),
      logger_(logger),
      listener_(listener),
      weak_factory_(this) {
  RegisterWithNetwork();
  TLOG(logger_, INFO, "Created client");
}

PerUserTopicInvalidationClient::~PerUserTopicInvalidationClient() {}

void PerUserTopicInvalidationClient::RegisterWithNetwork() {
  // Install ourselves as a receiver for server messages.
  network_->SetMessageReceiver(
      base::BindRepeating(&PerUserTopicInvalidationClient::MessageReceiver,
                          weak_factory_.GetWeakPtr()));
}

void PerUserTopicInvalidationClient::Start() {
  if (ticl_protocol_started_) {
    TLOG(logger_, SEVERE, "Ignoring start call since already started");

    return;
  }

  FinishStartingTiclAndInformListener();
}

void PerUserTopicInvalidationClient::Stop() {
  TLOG(logger_, INFO, "Ticl being stopped");
  ticl_protocol_started_ = false;
}

void PerUserTopicInvalidationClient::FinishStartingTiclAndInformListener() {
  DCHECK(!ticl_protocol_started_);
  ticl_protocol_started_ = true;
  GetListener()->Ready(this);
  GetListener()->ReissueRegistrations(this, "", 0);
  TLOG(logger_, INFO, "Ticl started");
}

void PerUserTopicInvalidationClient::MessageReceiver(
    const std::string& message) {
  // TODO(melandory): Here message should be passed to the protocol handler,
  // converted to the invalidation and passed to the listener afterwards.
}

}  // namespace invalidation
