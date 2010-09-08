// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

#ifndef JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_
#define JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/communicator/login.h"
#include "jingle/notifier/communicator/login_connection_state.h"
#include "jingle/notifier/communicator/login_failure.h"
#include "jingle/notifier/listener/mediator_thread.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppclientsettings.h"

class MessageLoop;

namespace buzz {
class XmppClient;
}  // namespace buzz

namespace net {
class HostResolver;
}  // namespace net

namespace notifier {
class TaskPump;
}  // namespace notifier

namespace talk_base {
class SocketServer;
class Thread;
}  // namespace talk_base

namespace notifier {

class MediatorThreadImpl
    : public MediatorThread,
      public sigslot::has_slots<> {
 public:
  explicit MediatorThreadImpl(const NotifierOptions& notifier_options);
  virtual ~MediatorThreadImpl();

  virtual void SetDelegate(Delegate* delegate);

  // Start the thread.
  virtual void Start();

  // These are called from outside threads, by the talk mediator object.
  // They add messages to a queue which we poll in this thread.
  virtual void Login(const buzz::XmppClientSettings& settings);
  virtual void Logout();
  virtual void ListenForUpdates();
  virtual void SubscribeForUpdates(
      const std::vector<std::string>& subscribed_services_list);
  virtual void SendNotification(const OutgoingNotificationData& data);

 protected:
  // Should only be called after Start().
  MessageLoop* worker_message_loop();

  // Should only be called after OnConnectionStateChange() is called
  // on the delegate with true and before it is called with false.
  buzz::XmppClient* xmpp_client();

  // This is virtual so that subclasses can also know when the
  // connection state changes.
  virtual void OnClientStateChangeMessage(LoginConnectionState state);

  Delegate* delegate_;
  MessageLoop* parent_message_loop_;

 private:
  void StartLibjingleThread();
  void PumpLibjingleLoop();
  void StopLibjingleThread();

  // Called from within the thread on internal events.
  void DoLogin(const buzz::XmppClientSettings& settings);
  void DoDisconnect();
  void DoSubscribeForUpdates(
      const std::vector<std::string>& subscribed_services_list);
  void DoListenForUpdates();
  void DoSendNotification(
      const OutgoingNotificationData& data);

  // These handle messages indicating an event happened in the outside
  // world.  These are all called from the worker thread.
  void OnIncomingNotification(
      const IncomingNotificationData& notification_data);
  void OnOutgoingNotification(bool success);
  void OnLoginFailureMessage(const notifier::LoginFailure& failure);
  void OnSubscriptionStateChange(bool success);

  // Equivalents of the above functions called from the parent thread.
  void OnIncomingNotificationOnParentThread(
      const IncomingNotificationData& notification_data);
  void OnOutgoingNotificationOnParentThread(bool success);
  void OnLoginFailureMessageOnParentThread(
      const notifier::LoginFailure& failure);
  void OnClientStateChangeMessageOnParentThread(
      LoginConnectionState state);
  void OnSubscriptionStateChangeOnParentThread(
      bool success);

  const NotifierOptions notifier_options_;

  base::Thread worker_thread_;
  scoped_refptr<net::HostResolver> host_resolver_;

  // All buzz::XmppClients are owned by their parent.  The root parent is the
  // SingleLoginTask created by the notifier::Login object.  This in turn is
  // owned by the TaskPump.  They are destroyed either when processing is
  // complete or the pump shuts down.
  scoped_ptr<notifier::TaskPump> pump_;
  scoped_ptr<notifier::Login> login_;

  // Used only when |use_chrome_async_socket_| is false.
  scoped_ptr<talk_base::SocketServer> socket_server_;
  scoped_ptr<talk_base::Thread> libjingle_thread_;

  DISALLOW_COPY_AND_ASSIGN(MediatorThreadImpl);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_
