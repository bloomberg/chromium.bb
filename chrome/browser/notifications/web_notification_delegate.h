// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WEB_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_WEB_NOTIFICATION_DELEGATE_H_

#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace features {

extern const base::Feature kAllowFullscreenWebNotificationsFeature;

} // namespace features

// Base class for the PersistentNotificationDelegate and the
// NotificationObjectProxy. All common functionality for displaying web
// notifications is found here.
// TODO(peter, crbug.com/596161): Migrate this functionality offered by the
// delegate to the NotificationDisplayService.
class WebNotificationDelegate : public NotificationDelegate {
 public:
  // NotificationDelegate implementation.
  std::string id() const override;
  bool SettingsClick() override;
  bool ShouldDisplaySettingsButton() override;
  bool ShouldDisplayOverFullscreen() const override;

 protected:
  WebNotificationDelegate(content::BrowserContext* browser_context,
                          const std::string& notification_id,
                          const GURL& origin);

  ~WebNotificationDelegate() override;

  content::BrowserContext* browser_context() { return browser_context_; }
  const GURL& origin() { return origin_; }

 private:
  content::BrowserContext* browser_context_;
  std::string notification_id_;
  GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationDelegate);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WEB_NOTIFICATION_DELEGATE_H_
