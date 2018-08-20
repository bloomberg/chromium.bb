// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidator.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "components/invalidation/impl/fcm_sync_network_channel.h"
#include "components/invalidation/impl/per_user_topic_invalidation_client.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/invalidation/public/object_id_invalidation_map.h"

namespace syncer {

FCMInvalidator::FCMInvalidator(
    std::unique_ptr<FCMSyncNetworkChannel> network_channel,
    invalidation::IdentityProvider* identity_provider,
    PrefService* pref_service,
    network::mojom::URLLoaderFactory* loader_factory,
    const ParseJSONCallback& parse_json)
    : invalidation_listener_(std::move(network_channel)) {
  auto registration_manager = std::make_unique<PerUserTopicRegistrationManager>(
      identity_provider, pref_service, loader_factory, parse_json);
  invalidation_listener_.Start(
      base::BindOnce(&CreatePerUserTopicInvalidationClient), this,
      std::move(registration_manager));
}

FCMInvalidator::~FCMInvalidator() {}

void FCMInvalidator::RegisterHandler(InvalidationHandler* handler) {
  registrar_.RegisterHandler(handler);
}

bool FCMInvalidator::UpdateRegisteredIds(InvalidationHandler* handler,
                                         const ObjectIdSet& ids) {
  if (!registrar_.UpdateRegisteredIds(handler, ids))
    return false;

  invalidation_listener_.UpdateRegisteredIds(registrar_.GetAllRegisteredIds());
  return true;
}

void FCMInvalidator::UpdateCredentials(const std::string& email,
                                       const std::string& token) {
  // TODO(melandory): remove during cleanup.
}

void FCMInvalidator::UnregisterHandler(InvalidationHandler* handler) {
  registrar_.UnregisterHandler(handler);
}

InvalidatorState FCMInvalidator::GetInvalidatorState() const {
  return registrar_.GetInvalidatorState();
}

void FCMInvalidator::OnInvalidate(
    const ObjectIdInvalidationMap& invalidation_map) {
  registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

void FCMInvalidator::RequestDetailedStatus(
    base::RepeatingCallback<void(const base::DictionaryValue&)> callback)
    const {}

void FCMInvalidator::OnInvalidatorStateChange(InvalidatorState state) {
  registrar_.UpdateInvalidatorState(state);
}

}  // namespace syncer
