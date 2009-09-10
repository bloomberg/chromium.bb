// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is mock for delicious testing.
// It's very primitive, and it would have been better to use gmock, except
// that gmock is only for linux.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_MEDIATOR_THREAD_MOCK_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_MEDIATOR_THREAD_MOCK_H_

#include "chrome/browser/sync/notifier/listener/mediator_thread.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace browser_sync {

class MockMediatorThread : public MediatorThread {
 public:
  MockMediatorThread() {
    Reset();
  }
  ~MockMediatorThread() {}

  void Reset() {
    login_calls = 0;
    logout_calls = 0;
    start_calls = 0;
    subscribe_calls = 0;
    listen_calls = 0;
    send_calls = 0;
  }

  // Overridden from MediatorThread
  void Login(const buzz::XmppClientSettings& settings) {
    login_calls++;
  }

  void Logout() {
    logout_calls++;
  }

  void Start() {
    start_calls++;
  }

  virtual void SubscribeForUpdates() {
    subscribe_calls++;
  }

  virtual void ListenForUpdates() {
    listen_calls++;
  }

  virtual void SendNotification() {
    send_calls++;
  }

  // Callback control
  void ChangeState(MediatorThread::MediatorMessage message) {
    SignalStateChange(message);
  }

  // Intneral State
  int login_calls;
  int logout_calls;
  int start_calls;
  int subscribe_calls;
  int listen_calls;
  int send_calls;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_MEDIATOR_THREAD_MOCK_H_
