// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/notification_defines.h"

#include <cstddef>

namespace notifier {

Subscription::Subscription() {}
Subscription::~Subscription() {}

bool Subscription::Equals(const Subscription& other) const {
  return channel == other.channel && from == other.from;
}

namespace {

template <typename T>
bool ListsEqual(const T& t1, const T& t2) {
  if (t1.size() != t2.size()) {
    return false;
  }
  for (size_t i = 0; i < t1.size(); ++i) {
    if (!t1[i].Equals(t2[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool SubscriptionListsEqual(const SubscriptionList& subscriptions1,
                            const SubscriptionList& subscriptions2) {
  return ListsEqual(subscriptions1, subscriptions2);
}

Recipient::Recipient() {}
Recipient::~Recipient() {}

bool Recipient::Equals(const Recipient& other) const {
  return to == other.to && user_specific_data == other.user_specific_data;
}

bool RecipientListsEqual(const RecipientList& recipients1,
                         const RecipientList& recipients2) {
  return ListsEqual(recipients1, recipients2);
}

Notification::Notification() {}
Notification::~Notification() {}

bool Notification::Equals(const Notification& other) const {
  return
      channel == other.channel &&
      data == other.data &&
      RecipientListsEqual(recipients, other.recipients);
}

std::string Notification::ToString() const {
  return "{ channel: \"" + channel + "\", data: \"" + data + "\" }";
}

}  // namespace notifier
