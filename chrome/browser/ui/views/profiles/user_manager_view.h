// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/user_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/dialog_delegate.h"

class ScopedKeepAlive;
class UserManagerView;

class ReauthDelegate : public views::DialogDelegateView,
                       public UserManager::ReauthDialogObserver {
 public:
  ReauthDelegate(UserManagerView* parent,
                 views::WebView* web_view,
                 const std::string& email_address,
                 signin_metrics::Reason reason);
  ~ReauthDelegate() override;

  // UserManager::ReauthObserver:
  void CloseReauthDialog() override;

 private:
  ReauthDelegate();

  // Before its destruction, tells its parent container to reset its reference
  // to the ReauthDelegate.
  void OnReauthDialogDestroyed();

  // views::DialogDelegate:
  gfx::Size GetPreferredSize() const override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool ShouldUseCustomFrame() const override;
  ui::ModalType GetModalType() const override;
  void DeleteDelegate() override;
  base::string16 GetWindowTitle() const override;
  int GetDialogButtons() const override;
  views::View* GetInitiallyFocusedView() override;

  UserManagerView* parent_; // Not owned.
  views::WebView* web_view_;
  const std::string email_address_;

  DISALLOW_COPY_AND_ASSIGN(ReauthDelegate);
};

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
  static void OnSystemProfileCreated(std::unique_ptr<UserManagerView> instance,
                                     base::AutoReset<bool>* pending,
                                     Profile* system_profile,
                                     const std::string& url);

  void set_user_manager_started_showing(
      const base::Time& user_manager_started_showing) {
    user_manager_started_showing_ = user_manager_started_showing;
  }

  // Logs how long it took the UserManager to open.
  void LogTimeToOpen();

  // Shows a dialog where the user can re-authenticate the profile with the
  // given |email|. This is called in the following scenarios:
  //  -From the user manager when a profile is locked and the user's password is
  //   detected to have been changed.
  //  -From the user manager when a custodian account needs to be
  //   reauthenticated.
  // reason| can be REASON_UNLOCK or REASON_REAUTHENTICATION to indicate
  // whether this is a reauth or unlock scenario.
  void ShowReauthDialog(content::BrowserContext* browser_context,
                        const std::string& email,
                        signin_metrics::Reason reason);

  // Hides the reauth dialog if it is showing.
  void HideReauthDialog();

 private:
  friend class ReauthDelegate;
  friend std::default_delete<UserManagerView>;

  ~UserManagerView() override;

  // Resets delegate_ to nullptr when delegate_ is no longer alive.
  void OnReauthDialogDestroyed();

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
  bool ShouldUseCustomFrame() const override;

  views::WebView* web_view_;

  ReauthDelegate* delegate_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  base::Time user_manager_started_showing_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_USER_MANAGER_VIEW_H_
