// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_SCREEN_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/account_creation_view.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/view_screen.h"
#include "chrome/browser/chromeos/login/web_page_screen.h"
#include "chrome/browser/chromeos/login/web_page_view.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"

class GURL;
class WizardScreenDelegate;

namespace chromeos {

// AccountScreen is shown when user is creating new Google Account.
class AccountScreen : public ViewScreen<AccountCreationView>,
                      public WebPageScreen,
                      public WebPageDelegate,
                      public AccountCreationViewDelegate {
 public:
  explicit AccountScreen(WizardScreenDelegate* delegate);
  virtual ~AccountScreen();

  // WebPageDelegate implementation:
  virtual void OnPageLoaded();
  virtual void OnPageLoadFailed(const std::string& url);

  // AccountCreationViewDelegate implementation:
  virtual void OnUserCreated(const std::string& username,
                             const std::string& password);

  // Sets the url for account creation. Used in tests.
  static void set_new_account_page_url(const GURL& url);
  // Sets the flag forcing to check for HTTPS. Used in tests.
  static void set_check_for_https(bool check) { check_for_https_ = check; }

 private:
  // ViewScreen implementation:
  virtual void CreateView();
  virtual void Refresh();
  virtual AccountCreationView* AllocateView();

  // TabContentsDelegate implementation:
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

  // WebPageScreen implementation:
  virtual void CloseScreen(ScreenObserver::ExitCodes code);

  // Url of account creation page. Overriden by tests.
  static scoped_ptr<GURL> new_account_page_url_;
  // Indicates if we should check for HTTPS scheme. Overriden by tests.
  static bool check_for_https_;

  DISALLOW_COPY_AND_ASSIGN(AccountScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_SCREEN_H_
