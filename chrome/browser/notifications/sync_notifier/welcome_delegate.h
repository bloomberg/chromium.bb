// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_WELCOME_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_WELCOME_DELEGATE_H_

#include "chrome/browser/notifications/notification_delegate.h"
#include "ui/message_center/notifier_settings.h"

namespace content {
class RenderViewHost;
}
class Profile;

namespace notifier {

// A class that can handle user actions for per-service welcome notifications.
// The main action will be ButtonClick which will be used to disable the
// service.
class WelcomeDelegate : public NotificationDelegate {
 public:
  explicit WelcomeDelegate(const std::string& notification_id,
                           Profile* profile,
                           const message_center::NotifierId notifier_id);

  // NotificationDelegate interface.
  virtual void Display() OVERRIDE;
  virtual void Error() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual void Click() OVERRIDE;
  virtual void ButtonClick(int button_index) OVERRIDE;
  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE;
  virtual std::string id() const OVERRIDE;

 private:
  virtual ~WelcomeDelegate();

  const std::string notification_id_;
  Profile* profile_;  // Weak.
  const message_center::NotifierId notifier_id_;

  DISALLOW_COPY_AND_ASSIGN(WelcomeDelegate);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_WELCOME_DELEGATE_H_
