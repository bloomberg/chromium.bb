// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is mock for delicious testing.
// It's very primitive, and it would have been better to use gmock, except
// that gmock is only for linux.

#ifndef JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_MOCK_H_
#define JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_MOCK_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "jingle/notifier/listener/mediator_thread.h"

namespace buzz {
class XmppClientSettings;
}

namespace notifier {

class MockMediatorThread : public MediatorThread {
 public:
  MockMediatorThread();

  virtual ~MockMediatorThread();

  void Reset();

  virtual void AddObserver(Observer* observer) OVERRIDE;

  virtual void RemoveObserver(Observer* observer) OVERRIDE;

  // Overridden from MediatorThread
  virtual void Login(const buzz::XmppClientSettings& settings) OVERRIDE;

  virtual void Logout() OVERRIDE;

  virtual void Start() OVERRIDE;

  virtual void SubscribeForUpdates(
      const SubscriptionList& subscriptions) OVERRIDE;

  virtual void ListenForUpdates() OVERRIDE;

  virtual void SendNotification(const Notification &) OVERRIDE;
  virtual void UpdateXmppSettings(
      const buzz::XmppClientSettings& settings) OVERRIDE;


  void ReceiveNotification(const Notification& data);

  Observer* observer_;
  // Internal State
  int login_calls;
  int logout_calls;
  int start_calls;
  int subscribe_calls;
  int listen_calls;
  int send_calls;
  int update_settings_calls;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_MOCK_H_
