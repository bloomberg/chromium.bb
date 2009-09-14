// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// These methods should post messages to a queue which a different thread will
// later come back and read from.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_MEDIATOR_THREAD_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_MEDIATOR_THREAD_H_

#include "talk/xmpp/xmppclientsettings.h"

namespace browser_sync {

class MediatorThread {
 public:
  enum MediatorMessage {
    MSG_LOGGED_IN,
    MSG_LOGGED_OUT,
    MSG_SUBSCRIPTION_SUCCESS,
    MSG_SUBSCRIPTION_FAILURE,
    MSG_NOTIFICATION_RECEIVED,
    MSG_NOTIFICATION_SENT
  };

  MediatorThread() {}
  virtual ~MediatorThread() {}

  virtual void Login(const buzz::XmppClientSettings& settings) = 0;
  virtual void Logout() = 0;
  virtual void Start() = 0;
  virtual void SubscribeForUpdates() = 0;
  virtual void ListenForUpdates() = 0;
  virtual void SendNotification() = 0;

  // Connect to this for messages about talk events.
  sigslot::signal1<MediatorMessage> SignalStateChange;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_MEDIATOR_THREAD_H_
