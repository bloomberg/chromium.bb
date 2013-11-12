// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_POPUP_BLOCKED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_POPUP_BLOCKED_INFOBAR_DELEGATE_H_

#include "chrome/browser/infobars/confirm_infobar_delegate.h"

class InfoBarService;

class PopupBlockedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static InfoBarDelegate* Create(InfoBarService* infobar_service,
                                 int num_popups);
  virtual ~PopupBlockedInfoBarDelegate();

 private:
  PopupBlockedInfoBarDelegate(InfoBarService* infobar_service, int num_popups);

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual PopupBlockedInfoBarDelegate* AsPopupBlockedInfoBarDelegate() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  int num_popups_;

  DISALLOW_COPY_AND_ASSIGN(PopupBlockedInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_ANDROID_CONTENT_SETTINGS_POPUP_BLOCKED_INFOBAR_DELEGATE_H_
