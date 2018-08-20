// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_sync_invalidation_listener.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "components/invalidation/impl/network_channel.h"
#include "components/invalidation/impl/per_user_topic_invalidation_client.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/prefs/pref_service.h"
#include "google/cacheinvalidation/include/types.h"

namespace syncer {

namespace {

invalidation::ObjectId ConvertToObjectId(
    const invalidation::InvalidationObjectId& invalidation_object_id) {
  return invalidation::ObjectId(invalidation_object_id.source(),
                                invalidation_object_id.name());
}

invalidation::InvalidationObjectId ConvertToInvalidationObjectId(
    const invalidation::ObjectId& object_id) {
  return invalidation::InvalidationObjectId(object_id.source(),
                                            object_id.name());
}

ObjectIdSet ConvertToObjectIdSet(const InvalidationObjectIdSet& ids) {
  ObjectIdSet object_ids;
  for (const auto& id : ids)
    object_ids.insert(ConvertToObjectId(id));
  return object_ids;
}

InvalidationObjectIdSet ConvertToInvalidationObjectIdSet(
    const ObjectIdSet& ids) {
  InvalidationObjectIdSet invalidation_object_ids;
  for (const auto& id : ids)
    invalidation_object_ids.insert(ConvertToInvalidationObjectId(id));
  return invalidation_object_ids;
}

}  // namespace

FCMSyncInvalidationListener::Delegate::~Delegate() {}

FCMSyncInvalidationListener::FCMSyncInvalidationListener(
    std::unique_ptr<FCMSyncNetworkChannel> network_channel)
    : network_channel_(std::move(network_channel)),
      delegate_(nullptr),
      ticl_state_(DEFAULT_INVALIDATION_ERROR),
      fcm_network_state_(DEFAULT_INVALIDATION_ERROR),
      weak_factory_(this) {
  network_channel_->AddObserver(this);
}

FCMSyncInvalidationListener::~FCMSyncInvalidationListener() {
  network_channel_->RemoveObserver(this);
  Stop();
  DCHECK(!delegate_);
}

void FCMSyncInvalidationListener::Start(
    CreateInvalidationClientCallback create_invalidation_client_callback,
    Delegate* delegate,
    std::unique_ptr<PerUserTopicRegistrationManager>
        per_user_topic_registration_manager) {
  DCHECK(delegate);
  Stop();
  delegate_ = delegate;
  per_user_topic_registration_manager_ =
      std::move(per_user_topic_registration_manager);

  invalidation_client_ = std::move(create_invalidation_client_callback)
                             .Run(network_channel_.get(), &logger_, this);
  invalidation_client_->Start();
}

void FCMSyncInvalidationListener::UpdateRegisteredIds(const ObjectIdSet& ids) {
  registered_ids_ = ConvertToInvalidationObjectIdSet(ids);
  if (ticl_state_ == INVALIDATIONS_ENABLED &&
      per_user_topic_registration_manager_ && !token_.empty())
    DoRegistrationUpdate();
}

void FCMSyncInvalidationListener::Ready(InvalidationClient* client) {
  DCHECK_EQ(client, invalidation_client_.get());
  ticl_state_ = INVALIDATIONS_ENABLED;
  EmitStateChange();
  DoRegistrationUpdate();
}

void FCMSyncInvalidationListener::Invalidate(
    InvalidationClient* client,
    const invalidation::Invalidation& invalidation) {
  DCHECK_EQ(client, invalidation_client_.get());

  const invalidation::ObjectId& id = invalidation.object_id();

  std::string payload;
  // payload() CHECK()'s has_payload(), so we must check it ourselves first.
  if (invalidation.has_payload())
    payload = invalidation.payload();

  DVLOG(2) << "Received invalidation with version " << invalidation.version()
           << " for " << ObjectIdToString(id);

  ObjectIdInvalidationMap invalidations;
  Invalidation inv = Invalidation::Init(id, invalidation.version(), payload);
  inv.SetAckHandler(AsWeakPtr(), base::ThreadTaskRunnerHandle::Get());
  invalidations.Insert(inv);

  DispatchInvalidations(invalidations);
}

void FCMSyncInvalidationListener::InvalidateUnknownVersion(
    InvalidationClient* client,
    const invalidation::ObjectId& object_id) {
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "InvalidateUnknownVersion";

  ObjectIdInvalidationMap invalidations;
  Invalidation unknown_version = Invalidation::InitUnknownVersion(object_id);
  unknown_version.SetAckHandler(AsWeakPtr(),
                                base::ThreadTaskRunnerHandle::Get());
  invalidations.Insert(unknown_version);

  DispatchInvalidations(invalidations);
}

// This should behave as if we got an invalidation with version
// UNKNOWN_OBJECT_VERSION for all known data types.
void FCMSyncInvalidationListener::InvalidateAll(InvalidationClient* client) {
  DCHECK_EQ(client, invalidation_client_.get());
  DVLOG(1) << "InvalidateAll";

  ObjectIdInvalidationMap invalidations;
  for (const auto& registered_id : registered_ids_) {
    Invalidation unknown_version =
        Invalidation::InitUnknownVersion(ConvertToObjectId(registered_id));
    unknown_version.SetAckHandler(AsWeakPtr(),
                                  base::ThreadTaskRunnerHandle::Get());
    invalidations.Insert(unknown_version);
  }
  DispatchInvalidations(invalidations);
}

void FCMSyncInvalidationListener::DispatchInvalidations(
    const ObjectIdInvalidationMap& invalidations) {
  ObjectIdInvalidationMap to_save = invalidations;
  ObjectIdInvalidationMap to_emit = invalidations.GetSubsetWithObjectIds(
      ConvertToObjectIdSet(registered_ids_));

  SaveInvalidations(to_save);
  EmitSavedInvalidations(to_emit);
}

void FCMSyncInvalidationListener::SaveInvalidations(
    const ObjectIdInvalidationMap& to_save) {
  ObjectIdSet objects_to_save = to_save.GetObjectIds();
  for (ObjectIdSet::const_iterator it = objects_to_save.begin();
       it != objects_to_save.end(); ++it) {
    UnackedInvalidationsMap::iterator lookup =
        unacked_invalidations_map_.find(*it);
    if (lookup == unacked_invalidations_map_.end()) {
      lookup = unacked_invalidations_map_
                   .insert(std::make_pair(*it, UnackedInvalidationSet(*it)))
                   .first;
    }
    lookup->second.AddSet(to_save.ForObject(*it));
  }
}

void FCMSyncInvalidationListener::EmitSavedInvalidations(
    const ObjectIdInvalidationMap& to_emit) {
  DVLOG(2) << "Emitting invalidations: " << to_emit.ToString();
  delegate_->OnInvalidate(to_emit);
}

void FCMSyncInvalidationListener::InformError(
    InvalidationClient* client,
    const invalidation::ErrorInfo& error_info) {}

void FCMSyncInvalidationListener::InformTokenRecieved(
    InvalidationClient* client,
    const std::string& token) {
  DCHECK_EQ(client, invalidation_client_.get());
  token_ = token;
  DoRegistrationUpdate();
}

void FCMSyncInvalidationListener::Acknowledge(const invalidation::ObjectId& id,
                                              const syncer::AckHandle& handle) {
  UnackedInvalidationsMap::iterator lookup =
      unacked_invalidations_map_.find(id);
  if (lookup == unacked_invalidations_map_.end()) {
    DLOG(WARNING) << "Received acknowledgement for untracked object ID";
    return;
  }
  lookup->second.Acknowledge(handle);
}

void FCMSyncInvalidationListener::Drop(const invalidation::ObjectId& id,
                                       const syncer::AckHandle& handle) {
  UnackedInvalidationsMap::iterator lookup =
      unacked_invalidations_map_.find(id);
  if (lookup == unacked_invalidations_map_.end()) {
    DLOG(WARNING) << "Received drop for untracked object ID";
    return;
  }
  lookup->second.Drop(handle);
}

void FCMSyncInvalidationListener::DoRegistrationUpdate() {
  per_user_topic_registration_manager_->UpdateRegisteredIds(registered_ids_,
                                                            token_);

  // TODO(melandory): remove unacked invalidations for unregistered objects.
  ObjectIdInvalidationMap object_id_invalidation_map;
  for (auto& unacked : unacked_invalidations_map_) {
    if (registered_ids_.find(ConvertToInvalidationObjectId(unacked.first)) ==
        registered_ids_.end()) {
      continue;
    }
    unacked.second.ExportInvalidations(AsWeakPtr(),
                                       base::ThreadTaskRunnerHandle::Get(),
                                       &object_id_invalidation_map);
  }

  // There's no need to run these through DispatchInvalidations(); they've
  // already been saved to storage (that's where we found them) so all we need
  // to do now is emit them.
  EmitSavedInvalidations(object_id_invalidation_map);
}

void FCMSyncInvalidationListener::StopForTest() {
  Stop();
}

ObjectIdSet FCMSyncInvalidationListener::GetRegisteredIdsForTest() const {
  return ConvertToObjectIdSet(registered_ids_);
}

base::WeakPtr<FCMSyncInvalidationListener>
FCMSyncInvalidationListener::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FCMSyncInvalidationListener::Stop() {
  if (!invalidation_client_) {
    return;
  }

  invalidation_client_->Stop();

  invalidation_client_.reset();
  delegate_ = nullptr;
  per_user_topic_registration_manager_.reset();

  ticl_state_ = DEFAULT_INVALIDATION_ERROR;
  fcm_network_state_ = DEFAULT_INVALIDATION_ERROR;
}

InvalidatorState FCMSyncInvalidationListener::GetState() const {
  if (ticl_state_ == INVALIDATION_CREDENTIALS_REJECTED ||
      fcm_network_state_ == INVALIDATION_CREDENTIALS_REJECTED) {
    // If either the ticl or the push client rejected our credentials,
    // return INVALIDATION_CREDENTIALS_REJECTED.
    return INVALIDATION_CREDENTIALS_REJECTED;
  }
  if (ticl_state_ == INVALIDATIONS_ENABLED &&
      fcm_network_state_ == INVALIDATIONS_ENABLED) {
    // If the ticl is ready and the push client notifications are
    // enabled, return INVALIDATIONS_ENABLED.
    return INVALIDATIONS_ENABLED;
  }
  // Otherwise, we have a transient error.
  return TRANSIENT_INVALIDATION_ERROR;
}

void FCMSyncInvalidationListener::EmitStateChange() {
  delegate_->OnInvalidatorStateChange(GetState());
}

void FCMSyncInvalidationListener::OnFCMSyncNetworkChannelStateChanged(
    InvalidatorState invalidator_state) {
  fcm_network_state_ = invalidator_state;
  EmitStateChange();
}

}  // namespace syncer
