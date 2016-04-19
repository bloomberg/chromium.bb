// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OBJECT_PROXY_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OBJECT_PROXY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/notifications/notification_delegate.h"

namespace content {
class BrowserContext;
class DesktopNotificationDelegate;
}

// A NotificationObjectProxy stands in for the JavaScript Notification object
// which corresponds to a notification toast on the desktop.  It can be signaled
// when various events occur regarding the desktop notification, and the
// attached JS listeners will be invoked in the renderer or worker process.
class NotificationObjectProxy : public NotificationDelegate {
 public:
  // Creates a Proxy object with the necessary callback information. The Proxy
  // will take ownership of |delegate|.
  NotificationObjectProxy(
      content::BrowserContext* browser_context,
      std::unique_ptr<content::DesktopNotificationDelegate> delegate);

  // NotificationDelegate implementation.
  void Display() override;
  void Close(bool by_user) override;
  void Click() override;
  void ButtonClick(int button_index) override;
  void SettingsClick() override;
  bool ShouldDisplaySettingsButton() override;
  std::string id() const override;

 protected:
  ~NotificationObjectProxy() override;

 private:
  content::BrowserContext* browser_context_;
  std::unique_ptr<content::DesktopNotificationDelegate> delegate_;
  bool displayed_;
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(NotificationObjectProxy);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_OBJECT_PROXY_H_
