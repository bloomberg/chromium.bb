// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is the interface between talk code and the client code proper
// It will manage all aspects of the connection and call back into the client
// when it needs attention (for instance if updates are available for syncing).

#ifndef JINGLE_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_
#define JINGLE_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/mediator_thread.h"
#include "jingle/notifier/listener/talk_mediator.h"
#include "talk/xmpp/xmppclientsettings.h"

class MessageLoop;

namespace notifier {

class TalkMediatorImpl
    : public TalkMediator, public MediatorThread::Observer {
 public:
  // Takes ownership of |mediator_thread|. It is guaranteed that
  // |mediator_thread| is destroyed only when this object is destroyed.
  // This means that you can store a pointer to mediator_thread separately
  // and use it until this object is destroyed.
  TalkMediatorImpl(
      MediatorThread* mediator_thread,
      const NotifierOptions& notifier_options);
  virtual ~TalkMediatorImpl();

  // TalkMediator implementation.

  // Should be called on the same thread as the constructor.
  virtual void SetDelegate(TalkMediator::Delegate* delegate) OVERRIDE;

  // All the methods below should be called on the same thread. It may or may
  // not be same as the thread on which the object was constructed.

  // |email| must be a valid email address (e.g., foo@bar.com).
  virtual void SetAuthToken(const std::string& email,
                            const std::string& token,
                            const std::string& token_service) OVERRIDE;
  virtual bool Login() OVERRIDE;
  // Users must call Logout once Login is called.
  virtual bool Logout() OVERRIDE;

  virtual void SendNotification(const Notification& data) OVERRIDE;

  virtual void AddSubscription(const Subscription& subscription) OVERRIDE;

  // MediatorThread::Delegate implementation.

  virtual void OnConnectionStateChange(bool logged_in) OVERRIDE;

  virtual void OnSubscriptionStateChange(bool subscribed) OVERRIDE;

  virtual void OnIncomingNotification(
      const Notification& notification) OVERRIDE;

  virtual void OnOutgoingNotification() OVERRIDE;

 private:
  struct TalkMediatorState {
    TalkMediatorState()
        : started(0), initialized(0), logging_in(0), logged_in(0) {
    }

    unsigned int started : 1;      // Background thread has started.
    unsigned int initialized : 1;  // Initialized with login information.
    unsigned int logging_in : 1;   // Logging in to the mediator's
                                   // authenticator.
    unsigned int logged_in : 1;    // Logged in the mediator's authenticator.
  };

  // Delegate, which we don't own.  May be NULL.
  TalkMediator::Delegate* delegate_;

  // Internal state.
  TalkMediatorState state_;

  // Cached and verfied from the SetAuthToken method.
  buzz::XmppClientSettings xmpp_settings_;

  // The worker thread through which talk events are posted and received.
  scoped_ptr<MediatorThread> mediator_thread_;

  const NotifierOptions notifier_options_;

  SubscriptionList subscriptions_;

  MessageLoop* parent_message_loop_;

  FRIEND_TEST_ALL_PREFIXES(TalkMediatorImplTest, SetAuthToken);
  FRIEND_TEST_ALL_PREFIXES(TalkMediatorImplTest, SendNotification);
  FRIEND_TEST_ALL_PREFIXES(TalkMediatorImplTest, MediatorThreadCallbacks);

  DISALLOW_COPY_AND_ASSIGN(TalkMediatorImpl);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_
