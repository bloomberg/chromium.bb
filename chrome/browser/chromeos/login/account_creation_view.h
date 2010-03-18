// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_

#include <string>

#include "chrome/browser/views/dom_view.h"

class Profile;
class SiteContents;
class TabContentsDelegate;

namespace chromeos {

class AccountCreationViewDelegate {
 public:
  virtual ~AccountCreationViewDelegate() {}

  // Notify about new user name and password. This notification is sent before
  // server validates form so user may not be created. In this case this
  // this function will be called on each try.
  virtual void OnUserCreated(const std::string& username,
                             const std::string& password) = 0;

  // Notify about navigation errors.
  virtual void OnPageLoadFailed(const std::string& url) = 0;
};

class AccountCreationView : public DOMView {
 public:
  AccountCreationView();
  virtual ~AccountCreationView();

  // Initialize view layout.
  void Init();

  // Refresh view state.
  void Refresh() {}
  void InitDOM(Profile* profile, SiteInstance* site_instance);
  void SetTabContentsDelegate(TabContentsDelegate* delegate);

  // Set delegate that will be notified about user actions.
  void SetAccountCreationViewDelegate(AccountCreationViewDelegate* delegate);

 protected:
  // Overriden from DOMView:
  virtual TabContents* CreateTabContents(Profile* profile,
                                         SiteInstance* instance);

 private:
  // Overriden from views::View:
  virtual void Paint(gfx::Canvas* canvas);

  AccountCreationViewDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AccountCreationView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_
