// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "chrome/browser/sync/notifier/registration_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/invalidation-client-impl.h"

namespace sync_notifier {

ChromeInvalidationClient::Listener::~Listener() {}

ChromeInvalidationClient::ChromeInvalidationClient()
    : listener_(NULL) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

ChromeInvalidationClient::~ChromeInvalidationClient() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Stop();
  DCHECK(!listener_);
}

void ChromeInvalidationClient::Start(
    const std::string& client_id, Listener* listener,
    talk_base::Task* base_task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Stop();

  chrome_system_resources_.StartScheduler();

  DCHECK(!listener_);
  listener_ = listener;

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
          &chrome_system_resources_, client_type, client_id, this,
          client_config));
  cache_invalidation_packet_handler_.reset(
      new CacheInvalidationPacketHandler(base_task,
                                         invalidation_client_.get()));
  registration_manager_.reset(
      new RegistrationManager(invalidation_client_.get()));
  RegisterTypes();
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
  listener_ = NULL;
}

void ChromeInvalidationClient::RegisterTypes() {
  DCHECK(non_thread_safe_.CalledOnValidThread());

  // TODO(akalin): Make this configurable instead of listening to
  // notifications for all possible types.
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    registration_manager_->RegisterType(syncable::ModelTypeFromInt(i));
  }
  // TODO(akalin): This is a hack to make new sync data types work
  // with server-issued notifications.  Remove this when it's not
  // needed anymore.
  registration_manager_->RegisterType(syncable::UNSPECIFIED);
}

void ChromeInvalidationClient::Invalidate(
    const invalidation::Invalidation& invalidation,
    invalidation::Closure* callback) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(invalidation::IsCallbackRepeatable(callback));
  VLOG(1) << "Invalidate: " << InvalidationToString(invalidation);
  syncable::ModelType model_type;
  if (ObjectIdToRealModelType(invalidation.object_id(), &model_type)) {
    listener_->OnInvalidate(model_type);
  } else {
    LOG(WARNING) << "Could not get invalidation model type; "
                 << "invalidating everything";
    listener_->OnInvalidateAll();
  }
  RunAndDeleteClosure(callback);
}

void ChromeInvalidationClient::InvalidateAll(
    invalidation::Closure* callback) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(invalidation::IsCallbackRepeatable(callback));
  VLOG(1) << "InvalidateAll";
  listener_->OnInvalidateAll();
  RunAndDeleteClosure(callback);
}

void ChromeInvalidationClient::AllRegistrationsLost(
    invalidation::Closure* callback) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(invalidation::IsCallbackRepeatable(callback));
  VLOG(1) << "AllRegistrationsLost";
  registration_manager_->MarkAllRegistrationsLost();
  RunAndDeleteClosure(callback);
}

void ChromeInvalidationClient::RegistrationLost(
    const invalidation::ObjectId& object_id,
    invalidation::Closure* callback) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(invalidation::IsCallbackRepeatable(callback));
  VLOG(1) << "RegistrationLost: " << ObjectIdToString(object_id);
  syncable::ModelType model_type;
  if (ObjectIdToRealModelType(object_id, &model_type)) {
    registration_manager_->MarkRegistrationLost(model_type);
  } else {
    LOG(WARNING) << "Could not get object id model type; ignoring";
  }
  RunAndDeleteClosure(callback);
}

}  // namespace sync_notifier
