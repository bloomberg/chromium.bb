// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/server_notifier_thread.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/sync/notifier/cache_invalidation_packet_handler.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "chrome/browser/sync/notifier/chrome_system_resources.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/common/net/notifier/listener/notification_defines.h"
#include "google/cacheinvalidation/invalidation-client-impl.h"
#include "talk/xmpp/jid.h"

namespace sync_notifier {

ServerNotifierThread::ServerNotifierThread() {}

ServerNotifierThread::~ServerNotifierThread() {}

void ServerNotifierThread::ListenForUpdates() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &ServerNotifierThread::StartInvalidationListener));
}

void ServerNotifierThread::SubscribeForUpdates(
    const std::vector<std::string>& subscribed_services_list) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this, &ServerNotifierThread::RegisterTypesAndSignalSubscribed));
}

void ServerNotifierThread::Logout() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &ServerNotifierThread::StopInvalidationListener));
  MediatorThreadImpl::Logout();
}

void ServerNotifierThread::SendNotification(
    const OutgoingNotificationData& data) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  NOTREACHED() << "Shouldn't send notifications if "
               << "ServerNotifierThread is used";
}

void ServerNotifierThread::Invalidate(
    const invalidation::Invalidation& invalidation,
    invalidation::Closure* callback) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  CHECK(invalidation::IsCallbackRepeatable(callback));
  LOG(INFO) << "Invalidate: " << InvalidationToString(invalidation);
  // TODO(akalin): Signal notification only for the invalidated types.
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &ServerNotifierThread::SignalIncomingNotification));
  RunAndDeleteClosure(callback);
}

void ServerNotifierThread::InvalidateAll(
    invalidation::Closure* callback) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  CHECK(invalidation::IsCallbackRepeatable(callback));
  LOG(INFO) << "InvalidateAll";
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &ServerNotifierThread::SignalIncomingNotification));
  RunAndDeleteClosure(callback);
}

void ServerNotifierThread::AllRegistrationsLost(
    invalidation::Closure* callback) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  CHECK(invalidation::IsCallbackRepeatable(callback));
  LOG(INFO) << "AllRegistrationsLost; reregistering";
  RegisterTypes();
  RunAndDeleteClosure(callback);
}

void ServerNotifierThread::RegistrationLost(
    const invalidation::ObjectId& object_id,
    invalidation::Closure* callback) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  CHECK(invalidation::IsCallbackRepeatable(callback));
  LOG(INFO) << "RegistrationLost; reregistering: "
            << ObjectIdToString(object_id);
  RegisterTypes();
  RunAndDeleteClosure(callback);
}

void ServerNotifierThread::StartInvalidationListener() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());

  StopInvalidationListener();
  chrome_invalidation_client_.reset(new ChromeInvalidationClient());

  // TODO(akalin): Make cache_guid() part of the client ID.  If we do
  // so and we somehow propagate it up to the server somehow, we can
  // make it so that we won't receive any notifications that were
  // generated from our own changes.
  const std::string kClientId = "server_notifier_thread";
  chrome_invalidation_client_->Start(kClientId, this, xmpp_client());
}

void ServerNotifierThread::RegisterTypesAndSignalSubscribed() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  RegisterTypes();
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &ServerNotifierThread::SignalSubscribed));
}

void ServerNotifierThread::RegisterTypes() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());

  // TODO(akalin): Make this configurable instead of listening to
  // notifications for all possible types.
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
    std::string notification_type;
    if (!syncable::RealModelTypeToNotificationType(
            model_type, &notification_type)) {
      LOG(ERROR) << "Could not get notification type for model type "
                 << syncable::ModelTypeToString(model_type);
      continue;
    }
    invalidation::ObjectId object_id;
    object_id.mutable_name()->set_string_value(notification_type);
    object_id.set_source(invalidation::ObjectId::CHROME_SYNC);
    chrome_invalidation_client_->Register(
        object_id,
        invalidation::NewPermanentCallback(
            this, &ServerNotifierThread::RegisterCallback));
  }
}

void ServerNotifierThread::RegisterCallback(
    const invalidation::RegistrationUpdateResult& result) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  // TODO(akalin): Do something meaningful here.
  LOG(INFO) << "Registered: " << RegistrationUpdateResultToString(result);
}

void ServerNotifierThread::SignalSubscribed() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  if (delegate_) {
    delegate_->OnSubscriptionStateChange(true);
  }
}

void ServerNotifierThread::SignalIncomingNotification() {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  if (delegate_) {
    // TODO(akalin): Fill this in with something meaningful.
    IncomingNotificationData notification_data;
    delegate_->OnIncomingNotification(notification_data);
  }
}

void ServerNotifierThread::StopInvalidationListener() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());

  if (chrome_invalidation_client_.get()) {
    // TODO(akalin): Need to do unregisters here?
    chrome_invalidation_client_->Stop();
  }
  chrome_invalidation_client_.reset();
}

}  // namespace sync_notifier
