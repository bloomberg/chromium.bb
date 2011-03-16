// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/server_notifier_thread.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "googleurl/src/gurl.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "talk/xmpp/xmppclient.h"
#include "webkit/glue/webkit_glue.h"

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
      NewRunnableMethod(this, &ServerNotifierThread::DoListenForUpdates));
}

void ServerNotifierThread::SubscribeForUpdates(
    const notifier::SubscriptionList& subscriptions) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this, &ServerNotifierThread::RegisterTypes));

  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this, &ServerNotifierThread::SignalSubscribed));
}

void ServerNotifierThread::UpdateEnabledTypes(
    const syncable::ModelTypeSet& types) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &ServerNotifierThread::SetRegisteredTypes,
          types));

  worker_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this, &ServerNotifierThread::RegisterTypes));
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
    const notifier::Notification& notification) {
  DCHECK_EQ(MessageLoop::current(), parent_message_loop_);
  NOTREACHED() << "Shouldn't send notifications if ServerNotifierThread is "
                  "used";
}

void ServerNotifierThread::OnInvalidate(
    syncable::ModelType model_type,
    const std::string& payload) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  DCHECK_GE(model_type, syncable::FIRST_REAL_MODEL_TYPE);
  DCHECK_LT(model_type, syncable::MODEL_TYPE_COUNT);
  VLOG(1) << "OnInvalidate: " << syncable::ModelTypeToString(model_type);

  syncable::ModelTypeBitSet model_types;
  model_types[model_type] = true;
  notifier::Notification notification;
  notification.channel = model_types.to_string();
  notification.data = payload;
  observers_->Notify(&Observer::OnIncomingNotification, notification);
}

void ServerNotifierThread::OnInvalidateAll() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  VLOG(1) << "OnInvalidateAll";

  syncable::ModelTypeBitSet model_types;
  model_types.set();  // InvalidateAll, so set all datatypes to true.
  notifier::Notification notification;
  notification.channel = model_types.to_string();
  notification.data = std::string();  // No payload.
  observers_->Notify(&Observer::OnIncomingNotification, notification);
}

void ServerNotifierThread::WriteState(const std::string& state) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  VLOG(1) << "WriteState";
  state_writers_->Notify(&StateWriter::WriteState, state);
}

void ServerNotifierThread::DoListenForUpdates() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  if (!base_task_.get()) {
    return;
  }

  if (chrome_invalidation_client_.get()) {
    // If we already have an invalidation client, simply change the
    // base task.
    chrome_invalidation_client_->ChangeBaseTask(base_task_);
  } else {
    // Otherwise, create the invalidation client.
    chrome_invalidation_client_.reset(new ChromeInvalidationClient());

    // TODO(akalin): Make cache_guid() part of the client ID.  If we do
    // so and we somehow propagate it up to the server somehow, we can
    // make it so that we won't receive any notifications that were
    // generated from our own changes.
    const std::string kClientId = "server_notifier_thread";
    // Use user agent as |client_info| so we can use it for debugging
    // server-side.
    const std::string& client_info = webkit_glue::GetUserAgent(GURL());
    chrome_invalidation_client_->Start(
        kClientId, client_info, state_, this, this, base_task_);
    RegisterTypes();
    state_.clear();
  }
}

void ServerNotifierThread::RegisterTypes() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  if (!chrome_invalidation_client_.get()) {
    return;
  }
  chrome_invalidation_client_->RegisterTypes(registered_types_);
}

void ServerNotifierThread::SignalSubscribed() {
  observers_->Notify(&Observer::OnSubscriptionStateChange, true);
}

void ServerNotifierThread::StopInvalidationListener() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  chrome_invalidation_client_.reset();
}

void ServerNotifierThread::SetRegisteredTypes(
    const syncable::ModelTypeSet& types) {
  registered_types_ = types;
}

}  // namespace sync_notifier
