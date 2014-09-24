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
  // Do not call directly. To display the User Manager, use UserManager::Show().
  UserManagerView();

  // Creates a new UserManagerView instance for the |guest_profile| and
  // shows the |url|.
  static void OnGuestProfileCreated(scoped_ptr<UserManagerView> instance,
                                    const base::FilePath& profile_path_to_focus,
                                    Profile* guest_profile,
                                    const std::string& url);

 private:
  virtual ~UserManagerView();

  friend struct base::DefaultDeleter<UserManagerView>;

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
  virtual bool CanMinimize() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual bool UseNewStyleForThisDialog() const OVERRIDE;

  views::WebView* web_view_;

  scoped_ptr<AutoKeepAlive> keep_alive_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_
