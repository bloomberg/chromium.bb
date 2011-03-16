// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// These methods should post messages to a queue which a different thread will
// later come back and read from.

#ifndef JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_H_
#define JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_H_

#include <string>
#include <vector>

#include "jingle/notifier/listener/notification_defines.h"

namespace buzz {
class XmppClientSettings;
}  // namespace buzz

namespace notifier {

class MediatorThread {
 public:
  virtual ~MediatorThread() {}

  class Observer {
   public:
    virtual ~Observer() {}

    virtual void OnConnectionStateChange(bool logged_in) = 0;

    virtual void OnSubscriptionStateChange(bool subscribed) = 0;

    virtual void OnIncomingNotification(const Notification& notification) = 0;

    virtual void OnOutgoingNotification() = 0;
  };

  // Must be thread-safe (i.e., callable on any thread).
  virtual void AddObserver(Observer* observer) = 0;

  // Must be called on the same thread that AddObserver() was called
  // with the given observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual void Login(const buzz::XmppClientSettings& settings) = 0;
  virtual void Logout() = 0;
  virtual void Start() = 0;
  virtual void SubscribeForUpdates(const SubscriptionList& subscriptions) = 0;
  virtual void ListenForUpdates() = 0;
  virtual void SendNotification(const Notification& data) = 0;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_H_
