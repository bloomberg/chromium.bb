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
                           content::NavigationController* navigation_controller,
                           TokenService* token_service,
                           PrefService* pref_service,
                           const std::string& username,
                           const std::string& args);
  virtual ~AutoLoginInfoBarDelegate();

 private:
  // ConfirmInfoBarDelegate overrides.
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  content::NavigationController* navigation_controller_;
  TokenService* token_service_;
  PrefService* pref_service_;
  std::string username_;
  std::string args_;

  DISALLOW_COPY_AND_ASSIGN(AutoLoginInfoBarDelegate);
};

// This is the actual infobar displayed to prompt the user to reverse
// auto-login.
class ReverseAutoLoginInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ReverseAutoLoginInfoBarDelegate(
      InfoBarTabHelper* owner,
      content::NavigationController* navigation_controller,
      PrefService* pref_service,
      const std::string& args);
  virtual ~ReverseAutoLoginInfoBarDelegate();

 private:
  // ConfirmInfoBarDelegate overrides.
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  content::NavigationController* navigation_controller_;
  TokenService* token_service_;
  PrefService* pref_service_;
  std::string username_;
  std::string args_;

  DISALLOW_COPY_AND_ASSIGN(ReverseAutoLoginInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_AUTO_LOGIN_INFO_BAR_DELEGATE_H_
