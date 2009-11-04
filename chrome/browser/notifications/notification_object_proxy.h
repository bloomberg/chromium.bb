// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OBJECT_PROXY_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OBJECT_PROXY_H_

#include "base/ref_counted.h"

class MessageLoop;
namespace IPC {
class Message;
}

// A NotificationObjectProxy stands in for the JavaScript Notification object
// which corresponds to a notification toast on the desktop.  It can be signaled
// when various events occur regarding the desktop notification, and the
// attached JS listeners will be invoked in the renderer or worker process.
class NotificationObjectProxy :
    public base::RefCountedThreadSafe<NotificationObjectProxy> {
 public:
  // Creates a Proxy object with the necessary callback information.
  NotificationObjectProxy(int process_id, int route_id,
                          int notification_id, bool worker);

  // To be called when the desktop notification is actually shown.
  void Display();

  // To be called when the desktop notification cannot be shown due to an
  // error.
  void Error();

  // To be called when the desktop notification is closed.  If closed by a
  // user explicitly (as opposed to timeout/script), |by_user| should be true.
  void Close(bool by_user);

  // Compares two proxies by ids to decide if they are equal.
  bool IsSame(const NotificationObjectProxy& other) const {
    return (notification_id_ == other.notification_id_ &&
            route_id_ == other.route_id_ &&
            process_id_ == other.process_id_ &&
            worker_ == other.worker_);
  }

 private:
  // Called on UI thread to schedule a message for sending.
  void DeliverMessage(IPC::Message* message);

  // Called via Task on IO thread to actually send a message to a notification.
  void Send(IPC::Message* message);

  // Callback information to find the JS Notification object where it lives.
  int process_id_;
  int route_id_;
  int notification_id_;
  bool worker_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OBJECT_PROXY_H_
