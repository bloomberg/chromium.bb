// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WEB_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_WEB_NOTIFICATION_DELEGATE_H_

#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "chrome/browser/notifications/notification_common.h"
#include "ui/message_center/notification_delegate.h"
#include "url/gurl.h"

class Profile;

namespace features {

extern const base::Feature kAllowFullscreenWebNotificationsFeature;

} // namespace features

// Delegate class for Web Notifications.
class WebNotificationDelegate : public message_center::NotificationDelegate {
 public:
  WebNotificationDelegate(NotificationCommon::Type notification_type,
                          Profile* profile,
                          const std::string& notification_id,
                          const GURL& origin);

  // NotificationDelegate implementation.
  bool SettingsClick() override;
  bool ShouldDisplaySettingsButton() override;
  void DisableNotification() override;
  bool ShouldDisplayOverFullscreen() const override;
  void Close(bool by_user) override;
  void Click() override;
  void ButtonClick(int action_index) override;
  void ButtonClickWithReply(int action_index,
                            const base::string16& reply) override;

 protected:
  ~WebNotificationDelegate() override;
  const GURL& origin() { return origin_; }

 private:
  NotificationCommon::Type notification_type_;
  Profile* profile_;
  std::string notification_id_;
  GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationDelegate);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WEB_NOTIFICATION_DELEGATE_H_
