// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/server_notifier_thread.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "jingle/notifier/listener/notification_defines.h"

namespace sync_notifier {

ServerNotifierThread::ServerNotifierThread(bool use_chrome_async_socket)
    : notifier::MediatorThreadImpl(use_chrome_async_socket),
      state_(notifier::STATE_CLOSED) {}

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

void ServerNotifierThread::OnInvalidate(syncable::ModelType model_type) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  LOG(INFO) << "OnInvalidate: " << syncable::ModelTypeToString(model_type);
  // TODO(akalin): Signal notification only for the invalidated types.
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &ServerNotifierThread::SignalIncomingNotification));
}

void ServerNotifierThread::OnInvalidateAll() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  LOG(INFO) << "OnInvalidateAll";
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &ServerNotifierThread::SignalIncomingNotification));
}

void ServerNotifierThread::OnClientStateChangeMessage(
    notifier::LoginConnectionState state) {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  state_ = state;
  if (state_ != notifier::STATE_OPENED) {
    // Assume anything but an opened state invalidates xmpp_client().
    StopInvalidationListener();
  }
  MediatorThreadImpl::OnClientStateChangeMessage(state);
}

void ServerNotifierThread::StartInvalidationListener() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  if (state_ != notifier::STATE_OPENED) {
    return;
  }
  buzz::XmppClient* client = xmpp_client();
  if (client == NULL) {
    LOG(DFATAL) << "xmpp_client() unexpectedly NULL";
    return;
  }

  StopInvalidationListener();
  chrome_invalidation_client_.reset(new ChromeInvalidationClient());

  // TODO(akalin): Make cache_guid() part of the client ID.  If we do
  // so and we somehow propagate it up to the server somehow, we can
  // make it so that we won't receive any notifications that were
  // generated from our own changes.
  const std::string kClientId = "server_notifier_thread";
  chrome_invalidation_client_->Start(kClientId, this, client);
}

void ServerNotifierThread::RegisterTypesAndSignalSubscribed() {
  DCHECK_EQ(MessageLoop::current(), worker_message_loop());
  if (state_ != notifier::STATE_OPENED) {
    return;
  }

  chrome_invalidation_client_->RegisterTypes();
  parent_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &ServerNotifierThread::SignalSubscribed));
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
  chrome_invalidation_client_.reset();
}

}  // namespace sync_notifier
