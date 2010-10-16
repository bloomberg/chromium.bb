// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is mock for delicious testing.
// It's very primitive, and it would have been better to use gmock, except
// that gmock is only for linux.

#ifndef JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_MOCK_H_
#define JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_MOCK_H_

#include <string>
#include <vector>

#include "jingle/notifier/listener/mediator_thread.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

class MockMediatorThread : public MediatorThread {
 public:
  MockMediatorThread() : observer_(NULL) {
    Reset();
  }

  virtual ~MockMediatorThread() {}

  void Reset() {
    login_calls = 0;
    logout_calls = 0;
    start_calls = 0;
    subscribe_calls = 0;
    listen_calls = 0;
    send_calls = 0;
  }

  virtual void AddObserver(Observer* observer) {
    observer_ = observer;
  }

  virtual void RemoveObserver(Observer* observer) {
    observer_ = NULL;
  }

  // Overridden from MediatorThread
  virtual void Login(const buzz::XmppClientSettings& settings) {
    login_calls++;
    if (observer_) {
      observer_->OnConnectionStateChange(true);
    }
  }

  virtual void Logout() {
    logout_calls++;
    if (observer_) {
      observer_->OnConnectionStateChange(false);
    }
  }

  virtual void Start() {
    start_calls++;
  }

  virtual void SubscribeForUpdates(
      const std::vector<std::string>& subscribed_services_list) {
    subscribe_calls++;
    if (observer_) {
      observer_->OnSubscriptionStateChange(true);
    }
  }

  virtual void ListenForUpdates() {
    listen_calls++;
  }

  virtual void SendNotification(const OutgoingNotificationData &) {
    send_calls++;
    if (observer_) {
      observer_->OnOutgoingNotification();
    }
  }

  void ReceiveNotification(const IncomingNotificationData& data) {
    if (observer_) {
      observer_->OnIncomingNotification(data);
    }
  }

  Observer* observer_;
  // Intneral State
  int login_calls;
  int logout_calls;
  int start_calls;
  int subscribe_calls;
  int listen_calls;
  int send_calls;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_MOCK_H_
