// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Interface to the code which handles talk logic.  Used to initialize SSL
// before the underlying talk login occurs.
//
// Example usage:
//
//   TalkMediator mediator();
//   mediator.SetAuthToken("email", "token", "service_id");
//   mediator.Login();
//   ...
//   mediator.Logout();

#ifndef JINGLE_NOTIFIER_LISTENER_TALK_MEDIATOR_H_
#define JINGLE_NOTIFIER_LISTENER_TALK_MEDIATOR_H_

#include <string>

#include "jingle/notifier/listener/notification_defines.h"

namespace notifier {

class TalkMediator {
 public:
  TalkMediator() {}
  virtual ~TalkMediator() {}

  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnNotificationStateChange(bool notifications_enabled) = 0;

    virtual void OnIncomingNotification(const Notification& notification) = 0;

    virtual void OnOutgoingNotification() = 0;
  };

  // |delegate| may be NULL.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // The following methods are for authorizaiton of the xmpp client.
  virtual bool SetAuthToken(const std::string& email,
                            const std::string& token,
                            const std::string& token_service) = 0;
  virtual bool Login() = 0;
  virtual bool Logout() = 0;

  // Method for the owner of this object to notify peers that an update has
  // occurred.
  virtual bool SendNotification(const Notification& data) = 0;

  // Add a subscription to subscribe to.
  virtual void AddSubscription(const Subscription& subscription) = 0;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_TALK_MEDIATOR_H_
