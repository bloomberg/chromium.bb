// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is the interface between talk code and the client code proper
// It will manage all aspects of the connection and call back into the client
// when it needs attention (for instance if updates are available for syncing).

#ifndef CHROME_COMMON_NET_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_
#define CHROME_COMMON_NET_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_

#include <string>
#include <vector>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "chrome/common/net/notifier/listener/mediator_thread.h"
#include "chrome/common/net/notifier/listener/talk_mediator.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

namespace chrome_common_net {
class NetworkChangeNotifierThread;
}  // namespace chrome_common_net

namespace notifier {

class TalkMediatorImpl
    : public TalkMediator, public MediatorThread::Delegate {
 public:
  TalkMediatorImpl(
      chrome_common_net::NetworkChangeNotifierThread*
          network_change_notifier_thread,
      bool invalidate_xmpp_auth_token);
  explicit TalkMediatorImpl(MediatorThread* thread);
  virtual ~TalkMediatorImpl();

  // TalkMediator implementation.

  virtual void SetDelegate(TalkMediator::Delegate* delegate);

  virtual bool SetAuthToken(const std::string& email,
                            const std::string& token,
                            const std::string& token_service);
  virtual bool Login();
  virtual bool Logout();

  virtual bool SendNotification(const OutgoingNotificationData& data);

  virtual void AddSubscribedServiceUrl(const std::string& service_url);

  // MediatorThread::Delegate implementation.

  virtual void OnConnectionStateChange(bool logged_in);

  virtual void OnSubscriptionStateChange(bool subscribed);

  virtual void OnIncomingNotification(
      const IncomingNotificationData& notification_data);

  virtual void OnOutgoingNotification();

 private:
  struct TalkMediatorState {
    TalkMediatorState()
        : started(0), connected(0), initialized(0), logging_in(0),
          logged_in(0), subscribed(0) {
    }

    unsigned int started : 1;      // Background thread has started.
    unsigned int connected : 1;    // Connected to the mediator thread signal.
    unsigned int initialized : 1;  // Initialized with login information.
    unsigned int logging_in : 1;   // Logging in to the mediator's
                                   // authenticator.
    unsigned int logged_in : 1;    // Logged in the mediator's authenticator.
    unsigned int subscribed : 1;   // Subscribed to the xmpp receiving channel.
  };

  // Completes common initialization between the constructors.  Set should
  // connect to true if the talk mediator should connect to the controlled
  // mediator thread's SignalStateChange object.
  void TalkMediatorInitialization(bool should_connect);

  // Protects state_, xmpp_settings_, and subscribed_services_list_.
  //
  // TODO(akalin): Remove this once we use this class from one thread
  // only.
  Lock mutex_;

  // Delegate, which we don't own.  May be NULL.
  TalkMediator::Delegate* delegate_;

  // Internal state.
  TalkMediatorState state_;

  // Cached and verfied from the SetAuthToken method.
  buzz::XmppClientSettings xmpp_settings_;

  // The worker thread through which talk events are posted and received.
  scoped_ptr<MediatorThread> mediator_thread_;

  const bool invalidate_xmpp_auth_token_;

  std::vector<std::string> subscribed_services_list_;

  FRIEND_TEST(TalkMediatorImplTest, SetAuthTokenWithBadInput);
  FRIEND_TEST(TalkMediatorImplTest, SetAuthTokenWithGoodInput);
  FRIEND_TEST(TalkMediatorImplTest, SendNotification);
  FRIEND_TEST(TalkMediatorImplTest, MediatorThreadCallbacks);
  DISALLOW_COPY_AND_ASSIGN(TalkMediatorImpl);
};

}  // namespace notifier

#endif  // CHROME_COMMON_NET_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_
