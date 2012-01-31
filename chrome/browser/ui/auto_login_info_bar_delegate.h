// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTO_LOGIN_INFO_BAR_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTO_LOGIN_INFO_BAR_DELEGATE_H_

#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"

class InfoBarTabHelper;
class PrefService;
class TokenService;

namespace content {
class NavigationController;
}  // namespace content

// This is the actual infobar displayed to prompt the user to auto-login.
class AutoLoginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  AutoLoginInfoBarDelegate(InfoBarTabHelper* owner,
                           const std::string& username,
                           const std::string& args);
  virtual ~AutoLoginInfoBarDelegate();

 private:
  // ConfirmInfoBarDelegate overrides.
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  void RecordHistogramAction(int action);

  // Username to display in the infobar indicating user to be logged in as.
  std::string username_;

  // "args" string from x-auto-login to be passed to MergeSession.  This
  // string should be considered opaque and not be cracked open to look inside.
  std::string args_;

  // Whether any UI controls in the infobar were pressed or not.
  bool button_pressed_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginInfoBarDelegate);
};

// This is the actual infobar displayed to prompt the user to reverse
// auto-login.
class ReverseAutoLoginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ReverseAutoLoginInfoBarDelegate(InfoBarTabHelper* owner,
                                  const std::string& continue_url);
  virtual ~ReverseAutoLoginInfoBarDelegate();

 private:
  // ConfirmInfoBarDelegate overrides.
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  void RecordHistogramAction(int action);

  // The URL to continue from after the user logs in via the syncpromo.
  const std::string continue_url_;

  // Whether any UI controls in the infobar were pressed or not.
  bool button_pressed_;

  DISALLOW_COPY_AND_ASSIGN(ReverseAutoLoginInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_AUTO_LOGIN_INFO_BAR_DELEGATE_H_
