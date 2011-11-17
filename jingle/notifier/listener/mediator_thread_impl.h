// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/mediator_thread.h"

namespace base {
class MessageLoopProxy;
}

namespace buzz {
class XmppClientSettings;
class XmppTaskParentInterface;
}  // namespace buzz

namespace talk_base {
class Task;
}  // namespace talk_base

namespace notifier {

class MediatorThreadImpl : public MediatorThread {
 public:
  explicit MediatorThreadImpl(const NotifierOptions& notifier_options);
  virtual ~MediatorThreadImpl();

  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;

  // Start the thread.
  virtual void Start() OVERRIDE;

  // These are called from outside threads, by the talk mediator object.
  // They add messages to a queue which we poll in this thread.
  virtual void Login(const buzz::XmppClientSettings& settings) OVERRIDE;
  virtual void Logout() OVERRIDE;
  virtual void ListenForUpdates() OVERRIDE;
  virtual void SubscribeForUpdates(
      const SubscriptionList& subscriptions) OVERRIDE;
  virtual void SendNotification(const Notification& data) OVERRIDE;
  virtual void UpdateXmppSettings(
      const buzz::XmppClientSettings& settings) OVERRIDE;

  // Used by unit tests.  Make sure that tests that use this have the
  // IO message loop proxy passed in via |notifier_options| pointing
  // to the current thread.
  void TriggerOnConnectForTest(
      base::WeakPtr<buzz::XmppTaskParentInterface> base_task);

 private:
  // The logic of Logout without the thread check so it can be called in the
  // d'tor.
  void LogoutImpl();
  // The real guts of MediatorThreadImpl, which allows this class to not be
  // refcounted.
  class Core;
  scoped_refptr<Core> core_;
  scoped_refptr<base::MessageLoopProxy> parent_message_loop_proxy_;
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;
  DISALLOW_COPY_AND_ASSIGN(MediatorThreadImpl);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_MEDIATOR_THREAD_IMPL_H_
