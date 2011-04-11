// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "chrome/browser/sync/notifier/registration_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/invalidation-client-impl.h"

namespace sync_notifier {

ChromeInvalidationClient::Listener::~Listener() {}

ChromeInvalidationClient::ChromeInvalidationClient()
    : chrome_system_resources_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      scoped_callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      handle_outbound_packet_callback_(
          scoped_callback_factory_.NewCallback(
              &ChromeInvalidationClient::HandleOutboundPacket)),
      listener_(NULL),
      state_writer_(NULL) {
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
    const std::string& state, Listener* listener,
    StateWriter* state_writer, base::WeakPtr<talk_base::Task> base_task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Stop();

  chrome_system_resources_.StartScheduler();

  DCHECK(!listener_);
  DCHECK(listener);
  listener_ = listener;
  DCHECK(!state_writer_);
  DCHECK(state_writer);
  state_writer_ = state_writer;

  invalidation::ClientType client_type;
  client_type.set_type(invalidation::ClientType::CHROME_SYNC);
  // TODO(akalin): Use InvalidationClient::Create() once it supports
  // taking a ClientConfig.
  invalidation::ClientConfig client_config;
  // Bump up limits so that we reduce the number of registration
  // replies we get.
  client_config.max_registrations_per_message = 20;
  client_config.max_ops_per_message = 40;
  invalidation_client_.reset(
      new invalidation::InvalidationClientImpl(
          &chrome_system_resources_, client_type, client_id, client_info,
          client_config, this));
  invalidation_client_->Start(state);
  invalidation::NetworkEndpoint* network_endpoint =
      invalidation_client_->network_endpoint();
  CHECK(network_endpoint);
  network_endpoint->RegisterOutboundListener(
      handle_outbound_packet_callback_.get());
  ChangeBaseTask(base_task);
  registration_manager_.reset(
      new RegistrationManager(invalidation_client_.get()));
  registration_manager_->SetRegisteredTypes(registered_types_);
}

void ChromeInvalidationClient::ChangeBaseTask(
    base::WeakPtr<talk_base::Task> base_task) {
  DCHECK(invalidation_client_.get());
  DCHECK(base_task.get());
  cache_invalidation_packet_handler_.reset(
      new CacheInvalidationPacketHandler(base_task,
                                         invalidation_client_.get()));
}

void ChromeInvalidationClient::Stop() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  if (!invalidation_client_.get()) {
    DCHECK(!cache_invalidation_packet_handler_.get());
    return;
  }

  chrome_system_resources_.StopScheduler();

  registration_manager_.reset();
  cache_invalidation_packet_handler_.reset();
  invalidation_client_.reset();
  state_writer_ = NULL;
  listener_ = NULL;
}

void ChromeInvalidationClient::RegisterTypes(
    const syncable::ModelTypeSet& types) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  registered_types_ = types;
  if (registration_manager_.get()) {
    registration_manager_->SetRegisteredTypes(registered_types_);
  }
  // TODO(akalin): Clear invalidation versions for unregistered types.
}

void ChromeInvalidationClient::Invalidate(
    const invalidation::Invalidation& invalidation,
    invalidation::Closure* callback) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(invalidation::IsCallbackRepeatable(callback));
  VLOG(1) << "Invalidate: " << InvalidationToString(invalidation);
  syncable::ModelType model_type;
  if (!ObjectIdToRealModelType(invalidation.object_id(), &model_type)) {
    LOG(WARNING) << "Could not get invalidation model type; "
                 << "invalidating everything";
    EmitInvalidation(registered_types_, std::string());
    RunAndDeleteClosure(callback);
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
  //
  // TODO(akalin): Persist |max_invalidation_versions_| somehow.
  if (invalidation.version() != UNKNOWN_OBJECT_VERSION) {
    std::map<syncable::ModelType, int64>::const_iterator it =
        max_invalidation_versions_.find(model_type);
    if ((it != max_invalidation_versions_.end()) &&
        (invalidation.version() <= it->second)) {
      // Drop redundant invalidations.
      RunAndDeleteClosure(callback);
      return;
    }
    max_invalidation_versions_[model_type] = invalidation.version();
  }

  std::string payload;
  // payload() CHECK()'s has_payload(), so we must check it ourselves first.
  if (invalidation.has_payload())
    payload = invalidation.payload();

  syncable::ModelTypeSet types;
  types.insert(model_type);
  EmitInvalidation(types, payload);
  // TODO(akalin): We should really |callback| only after we get the
  // updates from the sync server. (see http://crbug.com/78462).
  RunAndDeleteClosure(callback);
}

// This should behave as if we got an invalidation with version
// UNKNOWN_OBJECT_VERSION for all known data types.
void ChromeInvalidationClient::InvalidateAll(
    invalidation::Closure* callback) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(invalidation::IsCallbackRepeatable(callback));
  VLOG(1) << "InvalidateAll";
  EmitInvalidation(registered_types_, std::string());
  // TODO(akalin): We should really |callback| only after we get the
  // updates from the sync server. (see http://crbug.com/76482).
  RunAndDeleteClosure(callback);
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

void ChromeInvalidationClient::RegistrationStateChanged(
    const invalidation::ObjectId& object_id,
    invalidation::RegistrationState new_state,
    const invalidation::UnknownHint& unknown_hint) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  VLOG(1) << "RegistrationStateChanged: "
          << ObjectIdToString(object_id) << " " << new_state;
  if (new_state == invalidation::RegistrationState_UNKNOWN) {
    VLOG(1) << "is_transient=" << unknown_hint.is_transient()
            << ", message=" << unknown_hint.message();
  }

  syncable::ModelType model_type;
  if (!ObjectIdToRealModelType(object_id, &model_type)) {
    LOG(WARNING) << "Could not get object id model type; ignoring";
    return;
  }

  if (new_state != invalidation::RegistrationState_REGISTERED) {
    // We don't care about |unknown_hint|; we let
    // |registration_manager_| handle the registration backoff policy.
    registration_manager_->MarkRegistrationLost(model_type);
  }
}

void ChromeInvalidationClient::AllRegistrationsLost(
    invalidation::Closure* callback) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(invalidation::IsCallbackRepeatable(callback));
  VLOG(1) << "AllRegistrationsLost";
  registration_manager_->MarkAllRegistrationsLost();
  RunAndDeleteClosure(callback);
}

void ChromeInvalidationClient::SessionStatusChanged(bool has_session) {
  VLOG(1) << "SessionStatusChanged: " << has_session;
  listener_->OnSessionStatusChanged(has_session);
}

void ChromeInvalidationClient::WriteState(const std::string& state) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(state_writer_);
  state_writer_->WriteState(state);
}

void ChromeInvalidationClient::HandleOutboundPacket(
    invalidation::NetworkEndpoint* const& network_endpoint) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  CHECK(cache_invalidation_packet_handler_.get());
  cache_invalidation_packet_handler_->
      HandleOutboundPacket(network_endpoint);
}

}  // namespace sync_notifier
