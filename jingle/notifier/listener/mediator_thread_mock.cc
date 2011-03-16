// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/mediator_thread_mock.h"

#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

MockMediatorThread::MockMediatorThread() : observer_(NULL) {
  Reset();
}

MockMediatorThread::~MockMediatorThread() {}

void MockMediatorThread::Reset() {
  login_calls = 0;
  logout_calls = 0;
  start_calls = 0;
  subscribe_calls = 0;
  listen_calls = 0;
  send_calls = 0;
}

void MockMediatorThread::AddObserver(Observer* observer) {
  observer_ = observer;
}

void MockMediatorThread::RemoveObserver(Observer* observer) {
  observer_ = NULL;
}

// Overridden from MediatorThread
void MockMediatorThread::Login(const buzz::XmppClientSettings& settings) {
  login_calls++;
  if (observer_) {
    observer_->OnConnectionStateChange(true);
  }
}

void MockMediatorThread::Logout() {
  logout_calls++;
  if (observer_) {
    observer_->OnConnectionStateChange(false);
  }
}

void MockMediatorThread::Start() {
  start_calls++;
}

void MockMediatorThread::SubscribeForUpdates(
    const SubscriptionList& subscriptions) {
  subscribe_calls++;
  if (observer_) {
    observer_->OnSubscriptionStateChange(true);
  }
}

void MockMediatorThread::ListenForUpdates() {
  listen_calls++;
}

void MockMediatorThread::SendNotification(const Notification &) {
  send_calls++;
  if (observer_) {
    observer_->OnOutgoingNotification();
  }
}

void MockMediatorThread::ReceiveNotification(
    const Notification& data) {
  if (observer_) {
    observer_->OnIncomingNotification(data);
  }
}

}  // namespace notifier
