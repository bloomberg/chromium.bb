// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/web_page_view.h"
#include "views/view.h"

class Profile;
class SiteContents;

namespace chromeos {

class AccountCreationViewDelegate {
 public:
  virtual ~AccountCreationViewDelegate() {}

  // Notify about new user name and password. This notification is sent before
  // server validates form so user may not be created. In this case this
  // this function will be called on each try.
  virtual void OnUserCreated(const std::string& username,
                             const std::string& password) = 0;
};

class AccountCreationDomView : public WebPageDomView {
 public:
  AccountCreationDomView();
  virtual ~AccountCreationDomView();

  // Set delegate that will be notified about user actions.
  void SetAccountCreationViewDelegate(AccountCreationViewDelegate* delegate);

 protected:
  // Overriden from DOMView:
  virtual TabContents* CreateTabContents(Profile* profile,
                                         SiteInstance* instance);

 private:
  AccountCreationViewDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AccountCreationDomView);
};

class AccountCreationView : public WebPageView {
 public:
  AccountCreationView();
  virtual ~AccountCreationView();

  // Set delegate that will be notified about user actions.
  void SetAccountCreationViewDelegate(AccountCreationViewDelegate* delegate);

 protected:
  virtual WebPageDomView* dom_view() { return dom_view_; }

 private:
  // View that renders page.
  AccountCreationDomView* dom_view_;

  DISALLOW_COPY_AND_ASSIGN(AccountCreationView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_
