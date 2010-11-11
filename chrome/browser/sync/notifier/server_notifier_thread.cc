// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/server_notifier_thread.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "talk/xmpp/xmppclient.h"

namespace sync_notifier {

ServerNotifierThread::ServerNotifierThread(
    const notifier::NotifierOptions& notifier_options,
    const std::string& state, StateWriter* state_writer)
    : notifier::MediatorThreadImpl(notifier_options),
      state_(state),
      state_writers_(new ObserverListThreadSafe<StateWriter>()),
      state_writer_(state_writer) {
  DCHECK_EQ(notifier::NOTIFICATION_SERVER,
            notifier_options.notification_method);
  DCHECK(state_writer_);
  state_writers_->AddObserver(state_writer_);
}

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
  state_writers_->RemoveObserver(state_writer_);
  state_writer_ = NULL;
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &ServerNotifierThread::StopInvalidationListener));
  MediatorThreadImpl::Logout();
}

void ServerNotifierThread::SendNotification(
    const OutgoingNotificationData& data) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  NOTREACHED() << "Shouldn't send notifications if ServerNotifierThread is "
                  "used";
}

void ServerNotifierThread::OnInvalidate(syncable::ModelType model_type) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  // TODO(akalin): This is a hack to make new sync data types work
  // with server-issued notifications.  Remove this when it's not
  // needed anymore.
  VLOG(1) << "OnInvalidate: " << ((model_type == syncable::UNSPECIFIED) ?
      "UNKNOWN" : syncable::ModelTypeToString(model_type));

  syncable::ModelTypeBitSet model_types;
  model_types[model_type] = true;
  IncomingNotificationData notification_data;
  notification_data.service_specific_data = model_types.to_string();
  observers_->Notify(&Observer::OnIncomingNotification, notification_data);
}

void ServerNotifierThread::OnInvalidateAll() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  VLOG(1) << "OnInvalidateAll";

  syncable::ModelTypeBitSet model_types;
  model_types.set();  // InvalidateAll, so set all datatypes to true.
  IncomingNotificationData notification_data;
  notification_data.service_specific_data = model_types.to_string();
  observers_->Notify(&Observer::OnIncomingNotification, notification_data);
}

void ServerNotifierThread::WriteState(const std::string& state) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  VLOG(1) << "WriteState";
  state_writers_->Notify(&StateWriter::WriteState, state);
}

void ServerNotifierThread::OnDisconnect() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  StopInvalidationListener();
  MediatorThreadImpl::OnDisconnect();
}

void ServerNotifierThread::StartInvalidationListener() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  if (!base_task_.get()) {
    return;
  }

  StopInvalidationListener();
  chrome_invalidation_client_.reset(new ChromeInvalidationClient());

  // TODO(akalin): Make cache_guid() part of the client ID.  If we do
  // so and we somehow propagate it up to the server somehow, we can
  // make it so that we won't receive any notifications that were
  // generated from our own changes.
  const std::string kClientId = "server_notifier_thread";
  chrome_invalidation_client_->Start(
      kClientId, state_, this, this, base_task_);
  state_.clear();
}

void ServerNotifierThread::RegisterTypesAndSignalSubscribed() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  // |chrome_invalidation_client_| can be NULL if we receive an
  // OnDisconnect() event after we gets posted but before we run.
  if (!chrome_invalidation_client_.get()) {
    return;
  }
  chrome_invalidation_client_->RegisterTypes();
  observers_->Notify(&Observer::OnSubscriptionStateChange, true);
}

void ServerNotifierThread::StopInvalidationListener() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  chrome_invalidation_client_.reset();
}

}  // namespace sync_notifier
