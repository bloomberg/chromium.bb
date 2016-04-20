// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_DELEGATE_ANDROID_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/permissions/permission_infobar_delegate.h"

class InfoBarService;

class ProtectedMediaIdentifierInfoBarDelegateAndroid
    : public PermissionInfobarDelegate {
 public:
  // Creates a protected media identifier infobar and delegate and adds the
  // infobar to |infobar_service|.  Returns the infobar if it was successfully
  // added.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   const GURL& requesting_frame,
                                   const PermissionSetCallback& callback);

 protected:
  ProtectedMediaIdentifierInfoBarDelegateAndroid(
      const GURL& requesting_frame,
      const PermissionSetCallback& callback);
  ~ProtectedMediaIdentifierInfoBarDelegateAndroid() override;

 private:
  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  int GetMessageResourceId() const override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;

  GURL requesting_frame_;

  DISALLOW_COPY_AND_ASSIGN(ProtectedMediaIdentifierInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_DELEGATE_ANDROID_H_
