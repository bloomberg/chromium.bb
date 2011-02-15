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

namespace buzz {
class XmppClientSettings;
}

namespace notifier {

class MockMediatorThread : public MediatorThread {
 public:
  MockMediatorThread();

  virtual ~MockMediatorThread();

  void Reset();

  virtual void AddObserver(Observer* observer);

  virtual void RemoveObserver(Observer* observer);

  // Overridden from MediatorThread
  virtual void Login(const buzz::XmppClientSettings& settings);

  virtual void Logout();

  virtual void Start();

  virtual void SubscribeForUpdates(
      const std::vector<std::string>& subscribed_services_list);

  virtual void ListenForUpdates();

  virtual void SendNotification(const OutgoingNotificationData &);

  void ReceiveNotification(const IncomingNotificationData& data);

  Observer* observer_;
  // Internal State
  int login_calls;
  int logout_calls;
  int start_calls;
  int subscribe_calls;
  int listen_calls;
  int send_calls;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_MOCK_H_
