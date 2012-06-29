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
  struct Params {
    Params();
    ~Params();

    // "realm" string from x-auto-login (e.g. "com.google").
    std::string realm;

    // "account" string from x-auto-login.
    std::string account;

    // "args" string from x-auto-login to be passed to MergeSession. This string
    // should be considered opaque and not be cracked open to look inside.
    std::string args;

    // Username to display in the infobar indicating user to be logged in as.
    // This is initially fetched from sign-in on non-Android platforms. Note
    // that on Android this field is not used.
    std::string username;
  };

  AutoLoginInfoBarDelegate(InfoBarTabHelper* owner, const Params& params);
  virtual ~AutoLoginInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual AutoLoginInfoBarDelegate* AsAutoLoginInfoBarDelegate() OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // All the methods below are used by the Android implementation of the
  // AutoLogin bar on the app side.
  string16 GetMessageText(const std::string& username) const;

  const std::string& realm() const { return params_.realm; }
  const std::string& account() const { return params_.account; }
  const std::string& args() const { return params_.args; }

 private:
  void RecordHistogramAction(int action);

  const Params params_;

  // Whether any UI controls in the infobar were pressed or not.
  bool button_pressed_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_AUTO_LOGIN_INFO_BAR_DELEGATE_H_
