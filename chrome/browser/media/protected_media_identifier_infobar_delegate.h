// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_DELEGATE_H_

#include <string>

#include "chrome/browser/content_settings/permission_request_id.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

class PermissionQueueController;
class InfoBarService;

// TODO(toyoshim): Much more code can be shared with GeolocationInfoBarDelegate.
// http://crbug.com/266743

class ProtectedMediaIdentifierInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a protected media identifier infobar and delegate and adds the
  // infobar to |infobar_service|.  Returns the infobar if it was successfully
  // added.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   PermissionQueueController* controller,
                                   const PermissionRequestID& id,
                                   const GURL& requesting_frame,
                                   const std::string& display_languages);

 protected:
  ProtectedMediaIdentifierInfoBarDelegate(PermissionQueueController* controller,
                                          const PermissionRequestID& id,
                                          const GURL& requesting_frame,
                                          int contents_unique_id,
                                          const std::string& display_languages);
  virtual ~ProtectedMediaIdentifierInfoBarDelegate();

  // Calls back to the controller to inform it of the user's decision.
  void SetPermission(bool update_content_setting, bool allowed);

 private:
  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual bool ShouldExpireInternal(
      const NavigationDetails& details) const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual base::string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  PermissionQueueController* controller_;
  const PermissionRequestID id_;
  GURL requesting_frame_;
  int contents_unique_id_;
  std::string display_languages_;

  DISALLOW_COPY_AND_ASSIGN(ProtectedMediaIdentifierInfoBarDelegate);
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_DELEGATE_H_
