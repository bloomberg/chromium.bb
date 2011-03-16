// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "jingle/notifier/listener/mediator_thread.h"
#include "jingle/notifier/listener/talk_mediator.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

class TalkMediatorImpl
    : public TalkMediator, public MediatorThread::Observer {
 public:
  // Takes ownership of |mediator_thread|. It is guaranteed that
  // |mediator_thread| is destroyed only when this object is destroyed.
  // This means that you can store a pointer to mediator_thread separately
  // and use it until this object is destroyed.
  TalkMediatorImpl(
      MediatorThread* mediator_thread, bool invalidate_xmpp_auth_token,
      bool allow_insecure_connection);
  virtual ~TalkMediatorImpl();

  // TalkMediator implementation.

  virtual void SetDelegate(TalkMediator::Delegate* delegate);

  virtual bool SetAuthToken(const std::string& email,
                            const std::string& token,
                            const std::string& token_service);
  virtual bool Login();
  virtual bool Logout();

  virtual bool SendNotification(const Notification& data);

  virtual void AddSubscription(const Subscription& subscription);

  // MediatorThread::Delegate implementation.

  virtual void OnConnectionStateChange(bool logged_in);

  virtual void OnSubscriptionStateChange(bool subscribed);

  virtual void OnIncomingNotification(const Notification& notification);

  virtual void OnOutgoingNotification();

 private:
  struct TalkMediatorState {
    TalkMediatorState()
        : started(0), initialized(0), logging_in(0),
          logged_in(0), subscribed(0) {
    }

    unsigned int started : 1;      // Background thread has started.
    unsigned int initialized : 1;  // Initialized with login information.
    unsigned int logging_in : 1;   // Logging in to the mediator's
                                   // authenticator.
    unsigned int logged_in : 1;    // Logged in the mediator's authenticator.
    unsigned int subscribed : 1;   // Subscribed to the xmpp receiving channel.
  };

  base::NonThreadSafe non_thread_safe_;

  // Delegate, which we don't own.  May be NULL.
  TalkMediator::Delegate* delegate_;

  // Internal state.
  TalkMediatorState state_;

  // Cached and verfied from the SetAuthToken method.
  buzz::XmppClientSettings xmpp_settings_;

  // The worker thread through which talk events are posted and received.
  scoped_ptr<MediatorThread> mediator_thread_;

  const bool invalidate_xmpp_auth_token_;
  const bool allow_insecure_connection_;

  SubscriptionList subscriptions_;

  FRIEND_TEST_ALL_PREFIXES(TalkMediatorImplTest, SetAuthTokenWithBadInput);
  FRIEND_TEST_ALL_PREFIXES(TalkMediatorImplTest, SetAuthTokenWithGoodInput);
  FRIEND_TEST_ALL_PREFIXES(TalkMediatorImplTest, SendNotification);
  FRIEND_TEST_ALL_PREFIXES(TalkMediatorImplTest, MediatorThreadCallbacks);

  DISALLOW_COPY_AND_ASSIGN(TalkMediatorImpl);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_
