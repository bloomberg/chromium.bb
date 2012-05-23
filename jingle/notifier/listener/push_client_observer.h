// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_LISTENER_NON_BLOCKING_PUSH_CLIENT_OBSERVER_H_
#define JINGLE_NOTIFIER_LISTENER_NON_BLOCKING_PUSH_CLIENT_OBSERVER_H_

#include "jingle/notifier/listener/notification_defines.h"

namespace notifier {

// A PushClientObserver is notified whenever an incoming notification
// is received or when the state of the push client changes.
class PushClientObserver {
 protected:
  virtual ~PushClientObserver();

 public:
  // Called when the state of the push client changes.  If
  // |notifications_enabled| is true, that means notifications can be
  // sent and received freely.  If it is false, that means no
  // notifications can be sent or received.
  virtual void OnNotificationStateChange(bool notifications_enabled) = 0;

  // Called when a notification is received.  The details of the
  // notification are in |notification|.
  virtual void OnIncomingNotification(const Notification& notification) = 0;
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_NON_BLOCKING_PUSH_CLIENT_OBSERVER_H_
