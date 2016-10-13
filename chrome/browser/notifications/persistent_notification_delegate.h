// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/notifications/web_notification_delegate.h"

class GURL;

namespace content {
class BrowserContext;
}

// Delegate responsible for listening to the click event on persistent
// notifications, to forward them to the PlatformNotificationService so that
// JavaScript events can be fired on the associated Service Worker.
class PersistentNotificationDelegate : public WebNotificationDelegate {
 public:
  PersistentNotificationDelegate(content::BrowserContext* browser_context,
                                 const std::string& notification_id,
                                 const GURL& origin,
                                 int notification_settings_index);

  // NotificationDelegate implementation.
  void Display() override;
  void Close(bool by_user) override;
  void Click() override;
  void ButtonClick(int button_index) override;
  void ButtonClickWithReply(int button_index,
                            const base::string16& reply) override;

 protected:
  ~PersistentNotificationDelegate() override;

 private:
  int notification_settings_index_;

  DISALLOW_COPY_AND_ASSIGN(PersistentNotificationDelegate);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_DELEGATE_H_
