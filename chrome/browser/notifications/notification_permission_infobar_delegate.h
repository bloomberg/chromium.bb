// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_INFOBAR_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/permissions/permission_infobar_delegate.h"

class NotificationPermissionInfoBarDelegate : public PermissionInfoBarDelegate {
 public:
  NotificationPermissionInfoBarDelegate(
      ContentSettingsType type,
      const GURL& requesting_frame,
      bool user_gesture,
      Profile* profile,
      const PermissionSetCallback& callback);

 private:
  ~NotificationPermissionInfoBarDelegate() override;

  // PermissionInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  int GetMessageResourceId() const override;

  DISALLOW_COPY_AND_ASSIGN(NotificationPermissionInfoBarDelegate);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PERMISSION_INFOBAR_DELEGATE_H_
