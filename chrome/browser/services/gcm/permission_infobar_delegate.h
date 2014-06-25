// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PERMISSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_SERVICES_GCM_PERMISSION_INFOBAR_DELEGATE_H_

#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/web_contents.h"

class NavigationDetails;
class PermissionQueueController;

namespace gcm {

// Base class for permission infobars, it implements the default behaviour
// so that the accept/deny buttons grant/deny the relevant permission.
// A basic implementor only needs to implement the methods that
// provide an icon and a message text to the infobar.
class PermissionInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  virtual ~PermissionInfobarDelegate();

 protected:
  PermissionInfobarDelegate(PermissionQueueController* controller,
                            const PermissionRequestID& id,
                            const GURL& requesting_frame);

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual infobars::InfoBarDelegate::Type GetInfoBarType() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

 private:
  void SetPermission(bool update_content_setting, bool allowed);

  PermissionQueueController* controller_; // not owned by us
  const PermissionRequestID id_;
  GURL requesting_frame_;

  DISALLOW_COPY_AND_ASSIGN(PermissionInfobarDelegate);
};

}  // namespace gcm
#endif  // CHROME_BROWSER_SERVICES_GCM_PERMISSION_INFOBAR_DELEGATE_H_
