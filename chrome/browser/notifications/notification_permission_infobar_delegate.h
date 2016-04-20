// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_INFOBAR_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/permissions/permission_infobar_delegate.h"

class NotificationPermissionInfobarDelegate : public PermissionInfobarDelegate {
 public:
  // Creates a Notification permission infobar and delegate and adds the
  // infobar to |infobar_service|.  Returns the infobar if it was successfully
  // added.
  static infobars::InfoBar* Create(
      InfoBarService* infobar_service,
      const GURL& requesting_frame,
      const PermissionSetCallback& callback);
 private:
  NotificationPermissionInfobarDelegate(
      const GURL& requesting_frame,
      const PermissionSetCallback& callback);
  ~NotificationPermissionInfobarDelegate() override;

  // PermissionInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  int GetMessageResourceId() const override;

  GURL requesting_frame_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPermissionInfobarDelegate);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_INFOBAR_DELEGATE_H_
