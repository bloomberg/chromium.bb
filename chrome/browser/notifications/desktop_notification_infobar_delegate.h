// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_INFOBAR_DELEGATE_H_

#include <string>

#include "chrome/browser/content_settings/permission_infobar_delegate.h"

class DesktopNotificationInfoBarDelegate : public PermissionInfobarDelegate {
 public:
  // Creates a Notification permission infobar and delegate and adds the
  // infobar to |infobar_service|.
  // Returns the infobar if it was successfully added.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   PermissionQueueController* controller,
                                   const PermissionRequestID& id,
                                   const GURL& requesting_frame,
                                   const std::string& display_languages);
 private:
  DesktopNotificationInfoBarDelegate(PermissionQueueController* controller,
                                     const PermissionRequestID& id,
                                     const GURL& requesting_frame,
                                     int contents_unique_id,
                                     const std::string& display_languages);
  virtual ~DesktopNotificationInfoBarDelegate();

  // PermissionInfoBarDelegate:
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetIconID() const OVERRIDE;

  GURL requesting_frame_;
  std::string display_languages_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationInfoBarDelegate);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_INFOBAR_DELEGATE_H_
