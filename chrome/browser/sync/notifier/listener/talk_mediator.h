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

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_TALK_MEDIATOR_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_TALK_MEDIATOR_H_

#include <string>

#include "chrome/browser/sync/notifier/listener/notification_defines.h"
#include "chrome/common/deprecated/event_sys.h"

namespace notifier {

struct TalkMediatorEvent {
  enum WhatHappened {
    LOGIN_SUCCEEDED,
    LOGOUT_SUCCEEDED,
    SUBSCRIPTIONS_ON,
    SUBSCRIPTIONS_OFF,
    NOTIFICATION_RECEIVED,
    NOTIFICATION_SENT,
    TALKMEDIATOR_DESTROYED,
  };

  // Required by EventChannel.
  typedef TalkMediatorEvent EventType;

  static inline bool IsChannelShutdownEvent(const TalkMediatorEvent& event) {
    return event.what_happened == TALKMEDIATOR_DESTROYED;
  }

  WhatHappened what_happened;
  // Data in the case of a NOTIFICATION_RECEIVED event
  IncomingNotificationData notification_data;
};

typedef EventChannel<TalkMediatorEvent, Lock> TalkMediatorChannel;

class TalkMediator {
 public:
  TalkMediator() {}
  virtual ~TalkMediator() {}

  // The following methods are for authorizaiton of the xmpp client.
  virtual bool SetAuthToken(const std::string& email,
                            const std::string& token,
                            const std::string& token_service) = 0;
  virtual bool Login() = 0;
  virtual bool Logout() = 0;

  // Method for the owner of this object to notify peers that an update has
  // occurred.
  virtual bool SendNotification(const OutgoingNotificationData& data) = 0;

  // Channel by which talk mediator events are signaled.
  virtual TalkMediatorChannel* channel() const = 0;

  // Add a URL to subscribe to for notifications.
  virtual void AddSubscribedServiceUrl(const std::string& service_url) = 0;
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_TALK_MEDIATOR_H_
