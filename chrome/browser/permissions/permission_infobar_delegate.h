// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"

class NavigationDetails;

// Base class for permission infobars, it implements the default behavior
// so that the accept/deny buttons grant/deny the relevant permission.
// A basic implementor only needs to implement the methods that
// provide an icon and a message text to the infobar.
class PermissionInfobarDelegate : public ConfirmInfoBarDelegate {

 public:
  using PermissionSetCallback = base::Callback<void(bool, bool)>;
  ContentSettingsType content_setting() const { return content_settings_type_; }

 protected:
  PermissionInfobarDelegate(const GURL& requesting_origin,
                            content::PermissionType permission_type,
                            ContentSettingsType content_settings_type,
                            const PermissionSetCallback& callback);
  ~PermissionInfobarDelegate() override;

  virtual int GetMessageResourceId() const = 0;

 private:
  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  void InfoBarDismissed() override;
  PermissionInfobarDelegate* AsPermissionInfobarDelegate() override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  base::string16 GetMessageText() const override;
  bool Accept() override;
  bool Cancel() override;

  void SetPermission(bool update_content_setting, bool allowed);

  GURL requesting_origin_;
  bool action_taken_;
  content::PermissionType permission_type_;
  ContentSettingsType content_settings_type_;
  const PermissionSetCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PermissionInfobarDelegate);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_H_
