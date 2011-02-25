// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OBJECT_PROXY_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OBJECT_PROXY_H_
#pragma once

#include <string>

#include "chrome/browser/notifications/notification_delegate.h"

class MessageLoop;
namespace IPC {
class Message;
}

// A NotificationObjectProxy stands in for the JavaScript Notification object
// which corresponds to a notification toast on the desktop.  It can be signaled
// when various events occur regarding the desktop notification, and the
// attached JS listeners will be invoked in the renderer or worker process.
class NotificationObjectProxy
    : public NotificationDelegate {
 public:
  // Creates a Proxy object with the necessary callback information.
  NotificationObjectProxy(int process_id, int route_id,
                          int notification_id, bool worker);

  // NotificationDelegate implementation.
  virtual void Display();
  virtual void Error();
  virtual void Close(bool by_user);
  virtual void Click();
  virtual std::string id() const;

 protected:
  friend class base::RefCountedThreadSafe<NotificationObjectProxy>;

  virtual ~NotificationObjectProxy() {}

 private:
  // Called on UI thread to send a message.
  void Send(IPC::Message* message);

  // Callback information to find the JS Notification object where it lives.
  int process_id_;
  int route_id_;
  int notification_id_;
  bool worker_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OBJECT_PROXY_H_
