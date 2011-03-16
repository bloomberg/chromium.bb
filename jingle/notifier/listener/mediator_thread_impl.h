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
#include "base/observer_list_threadsafe.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/weak_ptr.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/communicator/login.h"
#include "jingle/notifier/listener/mediator_thread.h"
#include "jingle/notifier/listener/push_notifications_listen_task.h"
#include "jingle/notifier/listener/push_notifications_subscribe_task.h"

class MessageLoop;

namespace buzz {
class XmppClientSettings;
}  // namespace buzz

namespace net {
class HostResolver;
}  // namespace net

namespace notifier {

// Workaround for MSVS 2005 bug that fails to handle inheritance from a nested
// class properly if it comes directly on a base class list.
typedef Login::Delegate LoginDelegate;

class MediatorThreadImpl : public MediatorThread, public LoginDelegate,
                           public PushNotificationsListenTaskDelegate,
                           public PushNotificationsSubscribeTaskDelegate {
 public:
  explicit MediatorThreadImpl(const NotifierOptions& notifier_options);
  virtual ~MediatorThreadImpl();

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Start the thread.
  virtual void Start();

  // These are called from outside threads, by the talk mediator object.
  // They add messages to a queue which we poll in this thread.
  virtual void Login(const buzz::XmppClientSettings& settings);
  virtual void Logout();
  virtual void ListenForUpdates();
  virtual void SubscribeForUpdates(const SubscriptionList& subscriptions);
  virtual void SendNotification(const Notification& data);

  // Login::Delegate implementation.
  virtual void OnConnect(base::WeakPtr<talk_base::Task> base_task);
  virtual void OnDisconnect();

  // PushNotificationsListenTaskDelegate implementation.
  virtual void OnNotificationReceived(
      const Notification& notification);
  // PushNotificationsSubscribeTaskDelegate implementation.
  virtual void OnSubscribed();
  virtual void OnSubscriptionError();

 protected:
  // Should only be called after Start().
  MessageLoop* worker_message_loop();

  // These handle messages indicating an event happened in the outside
  // world.  These are all called from the worker thread. They are protected
  // so they can be used by subclasses.
  void OnIncomingNotification(
      const Notification& notification);
  void OnSubscriptionStateChange(bool success);

  scoped_refptr<ObserverListThreadSafe<Observer> > observers_;
  MessageLoop* parent_message_loop_;
  base::WeakPtr<talk_base::Task> base_task_;

 private:
  void DoLogin(const buzz::XmppClientSettings& settings);
  void DoDisconnect();

  // Helpers invoked on worker thread.
  void ListenForPushNotifications();
  void SubscribeForPushNotifications(
      const SubscriptionList& subscriptions);

  void DoSendNotification(
      const Notification& data);

  const NotifierOptions notifier_options_;

  base::Thread worker_thread_;
  scoped_ptr<net::HostResolver> host_resolver_;
  scoped_ptr<net::CertVerifier> cert_verifier_;

  scoped_ptr<notifier::Login> login_;

  DISALLOW_COPY_AND_ASSIGN(MediatorThreadImpl);
};

}  // namespace notifier

// We manage the lifetime of notifier::MediatorThreadImpl ourselves.
DISABLE_RUNNABLE_METHOD_REFCOUNT(notifier::MediatorThreadImpl);

#endif  // JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_
