// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/fake_push_client_observer.h"

namespace notifier {

FakePushClientObserver::FakePushClientObserver()
    : notifications_enabled_(false) {}

FakePushClientObserver::~FakePushClientObserver() {}

void FakePushClientObserver::OnNotificationStateChange(
    bool notifications_enabled) {
  notifications_enabled_ = notifications_enabled;
}

void FakePushClientObserver::OnIncomingNotification(
    const Notification& notification) {
  last_incoming_notification_ = notification;
}

bool FakePushClientObserver::notifications_enabled() const {
  return notifications_enabled_;
}

const Notification&
FakePushClientObserver::last_incoming_notification() const {
  return last_incoming_notification_;
}

}  // namespace notifier

