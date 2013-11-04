// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_USER_MANAGER_VIEW_H_

#include "chrome/browser/profiles/profile.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class WebView;
}

// Dialog widget that contains the Desktop User Manager webui.
class UserManagerView : public views::DialogDelegateView {
 public:
  // Shows the User Manager or re-activates an existing one.
  static void Show(const base::FilePath& profile_path_to_focus);

  // Hide the User Manager.
  static void Hide();

  // Returns whether or not the User Manager is showing.
  static bool IsShowing();

 private:
  explicit UserManagerView(Profile* profile);
  virtual ~UserManagerView();

  // If the |guest_profile| has been initialized succesfully (according to
  // |status|), creates a new UserManagerView instance with the user with path
  // |profile_path_to_focus| focused.
  static void OnGuestProfileCreated(const base::FilePath& profile_path_to_focus,
                                    Profile* guest_profile,
                                    Profile::CreateStatus status);

  // views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::DialogDelegateView:
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual bool UseNewStyleForThisDialog() const OVERRIDE;

  views::WebView* web_view_;

  // An open User Manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static UserManagerView* instance_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_MANAGER_VIEW_H_
