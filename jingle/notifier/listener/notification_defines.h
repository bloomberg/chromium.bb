// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_NOTIFICATION_DEFINES_H_
#define JINGLE_NOTIFIER_LISTENER_NOTIFICATION_DEFINES_H_

#include <string>
#include <vector>

namespace notifier {

struct Subscription {
  // The name of the channel to subscribe to; usually but not always
  // a URL.
  std::string channel;
  // A sender, which could be a domain or a bare JID, from which we
  // will accept pushes.
  std::string from;
};

typedef std::vector<Subscription> SubscriptionList;

struct Notification {
  // The channel the notification is coming in on.
  std::string channel;
  // The notification data payload.
  std::string data;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_NOTIFICATION_DEFINES_H_
