// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_

#include <string>

#include "base/timer.h"
#include "chrome/browser/views/dom_view.h"
#include "views/view.h"

class Profile;
class SiteContents;
class TabContentsDelegate;

namespace views {
class Label;
class Throbber;
}  // namespace views

namespace chromeos {

class AccountCreationViewDelegate {
 public:
  virtual ~AccountCreationViewDelegate() {}

  // Notify about new user name and password. This notification is sent before
  // server validates form so user may not be created. In this case this
  // this function will be called on each try.
  virtual void OnUserCreated(const std::string& username,
                             const std::string& password) = 0;

  // Notify about document load event.
  virtual void OnPageLoaded() = 0;

  // Notify about navigation errors.
  virtual void OnPageLoadFailed(const std::string& url) = 0;
};

class AccountCreationDomView : public DOMView {
 public:
  AccountCreationDomView();
  virtual ~AccountCreationDomView();

  // Set delegate that will be notified about user actions.
  void SetAccountCreationViewDelegate(AccountCreationViewDelegate* delegate);

  // Set delegate that will be notified about tab contents changes.
  void SetTabContentsDelegate(TabContentsDelegate* delegate);

 protected:
  // Overriden from DOMView:
  virtual TabContents* CreateTabContents(Profile* profile,
                                         SiteInstance* instance);

 private:
  AccountCreationViewDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AccountCreationDomView);
};

class AccountCreationView : public views::View {
 public:
  AccountCreationView();
  virtual ~AccountCreationView();

  // Initialize view layout.
  void Init();

  void InitDOM(Profile* profile, SiteInstance* site_instance);
  void LoadURL(const GURL& url);
  void SetTabContentsDelegate(TabContentsDelegate* delegate);

  // Set delegate that will be notified about user actions.
  void SetAccountCreationViewDelegate(AccountCreationViewDelegate* delegate);

  // Stops throbber and shows page content (starts renderer_timer_ for that).
  void ShowPageContent();

 private:
  // Overriden from views::View:
  virtual void Layout();

  // Called by stop_timer_. Shows rendered page.
  void ShowRenderedPage();

  // Called by start_timer_. Shows throbber and waiting label.
  void ShowWaitingControls();

  // View that renderes account creation page.
  AccountCreationDomView* dom_view_;

  // Screen border insets. Used for layout.
  gfx::Insets insets_;

  // Throbber shown during page load.
  views::Throbber* throbber_;

  // "Connecting..." label shown while waiting for the page to load/render.
  views::Label* connecting_label_;

  // Timer used when waiting for network response.
  base::OneShotTimer<AccountCreationView> start_timer_;

  // Timer used before toggling loaded page visibility.
  base::OneShotTimer<AccountCreationView> stop_timer_;

  DISALLOW_COPY_AND_ASSIGN(AccountCreationView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_
