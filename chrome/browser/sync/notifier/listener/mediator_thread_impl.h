// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This object runs on a thread and knows how to interpret messages sent by the
// talk mediator. The mediator posts messages to a queue which the thread polls
// (in a super class).
//
// Example usage:
//
//   MediatorThread m = new MediatorThreadImpl(pass in stuff);
//   m.start(); // Start the thread
//   // Once the thread is started, you can do server stuff.
//   m.Login(loginInformation);
//   // Events happen, the mediator finds out through its pump more messages
//   // are dispatched to the thread eventually we want to log out.
//   m.Logout();
//   delete m; // Also stops the thread.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/notifier/communicator/login.h"
#include "chrome/browser/sync/notifier/communicator/login_failure.h"
#include "chrome/browser/sync/notifier/listener/mediator_thread.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {
class TaskPump;
}  // namespace notifier

namespace buzz {
class XmppClient;
}  // namespace buzz

namespace talk_base {
class SocketServer;
}  // namespace talk_base

namespace browser_sync {

enum MEDIATOR_CMD {
  CMD_LOGIN,
  CMD_DISCONNECT,
  CMD_LISTEN_FOR_UPDATES,
  CMD_SEND_NOTIFICATION,
  CMD_SUBSCRIBE_FOR_UPDATES
};

// Used to pass authentication information from the mediator to the thread.
// Use new to allocate it on the heap, the thread will delete it for you.
struct LoginData : public talk_base::MessageData {
  explicit LoginData(const buzz::XmppClientSettings& settings)
      : user_settings(settings) {
  }
  virtual ~LoginData() {}

  buzz::XmppClientSettings user_settings;
};

class MediatorThreadImpl
    : public MediatorThread,
      public sigslot::has_slots<>,
      public talk_base::MessageHandler,
      public talk_base::Thread {
 public:
  MediatorThreadImpl();
  virtual ~MediatorThreadImpl();

  // Start the thread.
  virtual void Start();
  virtual void Run();

  // These are called from outside threads, by the talk mediator object.
  // They add messages to a queue which we poll in this thread.
  void Login(const buzz::XmppClientSettings& settings);
  void Logout();
  void ListenForUpdates();
  void SubscribeForUpdates();
  void SendNotification();
  void LogStanzas();

 private:
  // Called from within the thread on internal events.
  void ProcessMessages(int cms);
  void OnMessage(talk_base::Message* msg);
  void DoLogin(LoginData* login_data);
  void DoDisconnect();
  void DoSubscribeForUpdates();
  void DoListenForUpdates();
  void DoSendNotification();
  void DoStanzaLogging();

  // These handle messages indicating an event happened in the outside world.
  void OnUpdateListenerMessage();
  void OnUpdateNotificationSent(bool success);
  void OnLoginFailureMessage(const notifier::LoginFailure& failure);
  void OnClientStateChangeMessage(notifier::Login::ConnectionState state);
  void OnSubscriptionStateChange(bool success);
  void OnInputDebug(const char* msg, int length);
  void OnOutputDebug(const char* msg, int length);

  buzz::XmppClient* xmpp_client();

  // All buzz::XmppClients are owned by their parent.  The root parent is the
  // SingleLoginTask created by the notifier::Login object.  This in turn is
  // owned by the TaskPump.  They are destroyed either when processing is
  // complete or the pump shuts down.
  scoped_ptr<notifier::TaskPump> pump_;
  scoped_ptr<notifier::Login> login_;
  DISALLOW_COPY_AND_ASSIGN(MediatorThreadImpl);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_
