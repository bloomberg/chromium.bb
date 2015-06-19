// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_

#include "base/auto_reset.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "ui/views/window/dialog_delegate.h"

class AutoKeepAlive;

namespace views {
class WebView;
}

// Dialog widget that contains the Desktop User Manager webui.
class UserManagerView : public views::DialogDelegateView {
 public:
  // Do not call directly. To display the User Manager, use UserManager::Show().
  UserManagerView();

  // Creates a new UserManagerView instance for the |system_profile| and shows
  // the |url|.
  static void OnSystemProfileCreated(scoped_ptr<UserManagerView> instance,
                                     base::AutoReset<bool>* pending,
                                     Profile* system_profile,
                                     const std::string& url);

  void set_user_manager_started_showing(
      const base::Time& user_manager_started_showing) {
    user_manager_started_showing_ = user_manager_started_showing;
  }

  // Logs how long it took the UserManager to open.
  void LogTimeToOpen();

 private:
  ~UserManagerView() override;

  friend struct base::DefaultDeleter<UserManagerView>;

  // Creates dialog and initializes UI.
  void Init(Profile* guest_profile, const GURL& url);

  // views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  gfx::Size GetPreferredSize() const override;

  // views::DialogDelegateView:
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  base::string16 GetWindowTitle() const override;
  int GetDialogButtons() const override;
  void WindowClosing() override;
  bool UseNewStyleForThisDialog() const override;

  views::WebView* web_view_;

  scoped_ptr<AutoKeepAlive> keep_alive_;
  base::Time user_manager_started_showing_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_
