// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_

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
  // Shows the User Manager or re-activates an existing one, focusing the
  // profile given by |profile_path_to_focus|. Based on the value of
  // |tutorial_mode|, a tutorial could be shown, in which case
  // |profile_path_to_focus| is ignored.
  static void Show(const base::FilePath& profile_path_to_focus,
                   profiles::UserManagerTutorialMode tutorial_mode);

  // Hide the User Manager.
  static void Hide();

  // Returns whether or not the User Manager is showing.
  static bool IsShowing();

 private:
  friend struct base::DefaultDeleter<UserManagerView>;

  UserManagerView();
  virtual ~UserManagerView();

  // Creates a new UserManagerView instance for the |guest_profile| and
  // shows the |url|.
  static void OnGuestProfileCreated(scoped_ptr<UserManagerView> instance,
                                    const base::FilePath& profile_path_to_focus,
                                    Profile* guest_profile,
                                    const std::string& url);

  // Creates dialog and initializes UI.
  void Init(const base::FilePath& profile_path_to_focus,
            Profile* guest_profile,
            const GURL& url);

  // views::View:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  // views::DialogDelegateView:
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual bool UseNewStyleForThisDialog() const OVERRIDE;

  views::WebView* web_view_;

  scoped_ptr<AutoKeepAlive> keep_alive_;
  // An open User Manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static UserManagerView* instance_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_
