// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "chrome/browser/sync/notifier/registration_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/v2/invalidation-client-impl.h"

namespace {

const char kApplicationName[] = "chrome-sync";

}  // anonymous namespace

namespace sync_notifier {

ChromeInvalidationClient::Listener::~Listener() {}

ChromeInvalidationClient::ChromeInvalidationClient()
    : chrome_system_resources_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      scoped_callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      listener_(NULL),
      state_writer_(NULL),
      ticl_ready_(false) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

ChromeInvalidationClient::~ChromeInvalidationClient() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Stop();
  DCHECK(!listener_);
  DCHECK(!state_writer_);
}

void ChromeInvalidationClient::Start(
    const std::string& client_id, const std::string& client_info,
    const std::string& state,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const browser_sync::WeakHandle<InvalidationVersionTracker>&
        invalidation_version_tracker,
    Listener* listener,
    StateWriter* state_writer,
    base::WeakPtr<buzz::XmppTaskParentInterface> base_task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Stop();

  chrome_system_resources_.set_platform(client_info);
  chrome_system_resources_.Start();

  // The Storage resource is implemented as a write-through cache.  We populate
  // it with the initial state on startup, so subsequent writes go to disk and
  // update the in-memory cache, while reads just return the cached state.
  chrome_system_resources_.storage()->SetInitialState(state);

  max_invalidation_versions_ = initial_max_invalidation_versions;
  if (max_invalidation_versions_.empty()) {
    DVLOG(2) << "No initial max invalidation versions for any type";
  } else {
    for (InvalidationVersionMap::const_iterator it =
             max_invalidation_versions_.begin();
         it != max_invalidation_versions_.end(); ++it) {
      DVLOG(2) << "Initial max invalidation version for "
               << syncable::ModelTypeToString(it->first) << " is "
               << it->second;
    }
  }
  invalidation_version_tracker_ = invalidation_version_tracker;
  DCHECK(invalidation_version_tracker_.IsInitialized());

  DCHECK(!listener_);
  DCHECK(listener);
  listener_ = listener;
  DCHECK(!state_writer_);
  DCHECK(state_writer);
  state_writer_ = state_writer;

  int client_type = ipc::invalidation::ClientType::CHROME_SYNC;
  // TODO(akalin): Use InvalidationClient::Create() once it supports
  // taking a ClientConfig.
  invalidation::InvalidationClientImpl::Config client_config;
  invalidation_client_.reset(
      new invalidation::InvalidationClientImpl(
          &chrome_system_resources_, client_type, client_id, client_config,
          kApplicationName, this));
  ChangeBaseTask(base_task);
  invalidation_client_->Start();

  registration_manager_.reset(
      new RegistrationManager(invalidation_client_.get()));
}

void ChromeInvalidationClient::ChangeBaseTask(
    base::WeakPtr<buzz::XmppTaskParentInterface> base_task) {
  DCHECK(invalidation_client_.get());
  DCHECK(base_task.get());
  cache_invalidation_packet_handler_.reset(
      new CacheInvalidationPacketHandler(base_task));
  chrome_system_resources_.network()->UpdatePacketHandler(
      cache_invalidation_packet_handler_.get());
}

void ChromeInvalidationClient::Stop() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!invalidation_client_.get()) {
    DCHECK(!cache_invalidation_packet_handler_.get());
    return;
  }

  registration_manager_.reset();
  cache_invalidation_packet_handler_.reset();
  chrome_system_resources_.Stop();
  invalidation_client_->Stop();

  invalidation_client_.reset();
  state_writer_ = NULL;
  listener_ = NULL;

  invalidation_version_tracker_.Reset();
  max_invalidation_versions_.clear();
}

void ChromeInvalidationClient::RegisterTypes(
    const syncable::ModelTypeSet& types) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  registered_types_ = types;
  if (ticl_ready_ && registration_manager_.get()) {
    registration_manager_->SetRegisteredTypes(registered_types_);
  }
  // TODO(akalin): Clear invalidation versions for unregistered types.
}

void ChromeInvalidationClient::Ready(
    invalidation::InvalidationClient* client) {
  ticl_ready_ = true;
  listener_->OnSessionStatusChanged(true);
  registration_manager_->SetRegisteredTypes(registered_types_);
}

void ChromeInvalidationClient::Invalidate(
    invalidation::InvalidationClient* client,
    const invalidation::Invalidation& invalidation,
    const invalidation::AckHandle& ack_handle) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DVLOG(1) << "Invalidate: " << InvalidationToString(invalidation);
  syncable::ModelType model_type;
  if (!ObjectIdToRealModelType(invalidation.object_id(), &model_type)) {
    LOG(WARNING) << "Could not get invalidation model type; "
                 << "invalidating everything";
    EmitInvalidation(registered_types_, std::string());
    client->Acknowledge(ack_handle);
    return;
  }
  // The invalidation API spec allows for the possibility of redundant
  // invalidations, so keep track of the max versions and drop
  // invalidations with old versions.
  //
  // TODO(akalin): Now that we keep track of registered types, we
  // should drop invalidations for unregistered types.  We may also
  // have to filter it at a higher level, as invalidations for
  // newly-unregistered types may already be in flight.
  InvalidationVersionMap::const_iterator it =
      max_invalidation_versions_.find(model_type);
  if ((it != max_invalidation_versions_.end()) &&
      (invalidation.version() <= it->second)) {
    // Drop redundant invalidations.
    client->Acknowledge(ack_handle);
    return;
  }
  DVLOG(2) << "Setting max invalidation version for "
           << syncable::ModelTypeToString(model_type) << " to "
           << invalidation.version();
  max_invalidation_versions_[model_type] = invalidation.version();
  invalidation_version_tracker_.Call(
      FROM_HERE,
      &InvalidationVersionTracker::SetMaxVersion,
      model_type, invalidation.version());

  std::string payload;
  // payload() CHECK()'s has_payload(), so we must check it ourselves first.
  if (invalidation.has_payload())
    payload = invalidation.payload();

  syncable::ModelTypeSet types;
  types.insert(model_type);
  EmitInvalidation(types, payload);
  // TODO(akalin): We should really acknowledge only after we get the
  // updates from the sync server. (see http://crbug.com/78462).
  client->Acknowledge(ack_handle);
}

void ChromeInvalidationClient::InvalidateUnknownVersion(
    invalidation::InvalidationClient* client,
    const invalidation::ObjectId& object_id,
    const invalidation::AckHandle& ack_handle) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DVLOG(1) << "InvalidateUnknownVersion";

  syncable::ModelType model_type;
  if (!ObjectIdToRealModelType(object_id, &model_type)) {
    LOG(WARNING) << "Could not get invalidation model type; "
                 << "invalidating everything";
    EmitInvalidation(registered_types_, std::string());
    client->Acknowledge(ack_handle);
    return;
  }

  syncable::ModelTypeSet types;
  types.insert(model_type);
  EmitInvalidation(types, "");
  // TODO(akalin): We should really acknowledge only after we get the
  // updates from the sync server. (see http://crbug.com/78462).
  client->Acknowledge(ack_handle);
}

// This should behave as if we got an invalidation with version
// UNKNOWN_OBJECT_VERSION for all known data types.
void ChromeInvalidationClient::InvalidateAll(
    invalidation::InvalidationClient* client,
    const invalidation::AckHandle& ack_handle) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DVLOG(1) << "InvalidateAll";
  EmitInvalidation(registered_types_, std::string());
  // TODO(akalin): We should really acknowledge only after we get the
  // updates from the sync server. (see http://crbug.com/76482).
  client->Acknowledge(ack_handle);
}

void ChromeInvalidationClient::EmitInvalidation(
    const syncable::ModelTypeSet& types, const std::string& payload) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  // TODO(akalin): Move all uses of ModelTypeBitSet for invalidations
  // to ModelTypeSet.
  syncable::ModelTypePayloadMap type_payloads =
      syncable::ModelTypePayloadMapFromBitSet(
          syncable::ModelTypeBitSetFromSet(types), payload);
  listener_->OnInvalidate(type_payloads);
}

void ChromeInvalidationClient::InformRegistrationStatus(
      invalidation::InvalidationClient* client,
      const invalidation::ObjectId& object_id,
      InvalidationListener::RegistrationState new_state) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DVLOG(1) << "InformRegistrationStatus: "
           << ObjectIdToString(object_id) << " " << new_state;

  syncable::ModelType model_type;
  if (!ObjectIdToRealModelType(object_id, &model_type)) {
    LOG(WARNING) << "Could not get object id model type; ignoring";
    return;
  }

  if (new_state != InvalidationListener::REGISTERED) {
    // Let |registration_manager_| handle the registration backoff policy.
    registration_manager_->MarkRegistrationLost(model_type);
  }
}

void ChromeInvalidationClient::InformRegistrationFailure(
    invalidation::InvalidationClient* client,
    const invalidation::ObjectId& object_id,
    bool is_transient,
    const std::string& error_message) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DVLOG(1) << "InformRegistrationFailure: "
           << ObjectIdToString(object_id)
           << "is_transient=" << is_transient
           << ", message=" << error_message;

  syncable::ModelType model_type;
  if (!ObjectIdToRealModelType(object_id, &model_type)) {
    LOG(WARNING) << "Could not get object id model type; ignoring";
    return;
  }

  if (is_transient) {
    // We don't care about |unknown_hint|; we let
    // |registration_manager_| handle the registration backoff policy.
    registration_manager_->MarkRegistrationLost(model_type);
  } else {
    // Non-transient failures are permanent, so block any future
    // registration requests for |model_type|.  (This happens if the
    // server doesn't recognize the data type, which could happen for
    // brand-new data types.)
    registration_manager_->DisableType(model_type);
  }
}

void ChromeInvalidationClient::ReissueRegistrations(
    invalidation::InvalidationClient* client,
    const std::string& prefix,
    int prefix_length) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DVLOG(1) << "AllRegistrationsLost";
  registration_manager_->MarkAllRegistrationsLost();
}

void ChromeInvalidationClient::InformError(
    invalidation::InvalidationClient* client,
    const invalidation::ErrorInfo& error_info) {
  listener_->OnSessionStatusChanged(false);
  LOG(ERROR) << "Invalidation client encountered an error";
  // TODO(ghc): handle the error.
}

void ChromeInvalidationClient::WriteState(const std::string& state) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(state_writer_);
  state_writer_->WriteState(state);
}

}  // namespace sync_notifier
