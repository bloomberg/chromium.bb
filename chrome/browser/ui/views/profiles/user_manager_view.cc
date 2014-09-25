// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/user_manager_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/auto_keep_alive.h"
#include "chrome/grit/chromium_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

#if defined(OS_WIN)
#include "chrome/browser/shell_integration.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace {

// An open User Manager window. There can only be one open at a time. This
// is reset to NULL when the window is closed.
UserManagerView* instance_ = NULL;

} // namespace

// UserManager -----------------------------------------------------------------

void UserManager::Show(
    const base::FilePath& profile_path_to_focus,
    profiles::UserManagerTutorialMode tutorial_mode,
    profiles::UserManagerProfileSelected profile_open_action) {
  ProfileMetrics::LogProfileSwitchUser(ProfileMetrics::OPEN_USER_MANAGER);
  if (instance_) {
    // If we are showing the User Manager after locking a profile, change the
    // active profile to Guest.
    profiles::SetActiveProfileToGuestIfLocked();

    // If there's a user manager window open already, just activate it.
    instance_->GetWidget()->Activate();
    return;
  }

  // Create the guest profile, if necessary, and open the user manager
  // from the guest profile.
  profiles::CreateGuestProfileForUserManager(
      profile_path_to_focus,
      tutorial_mode,
      profile_open_action,
      base::Bind(&UserManagerView::OnGuestProfileCreated,
                 base::Passed(make_scoped_ptr(new UserManagerView)),
                 profile_path_to_focus));
}

void UserManager::Hide() {
  if (instance_)
    instance_->GetWidget()->Close();
}

bool UserManager::IsShowing() {
  return instance_ ? instance_->GetWidget()->IsActive() : false;
}

// UserManagerView -------------------------------------------------------------

UserManagerView::UserManagerView()
    : web_view_(NULL),
      keep_alive_(new AutoKeepAlive(NULL)) {
}

UserManagerView::~UserManagerView() {
}

// static
void UserManagerView::OnGuestProfileCreated(
    scoped_ptr<UserManagerView> instance,
    const base::FilePath& profile_path_to_focus,
    Profile* guest_profile,
    const std::string& url) {
  // If we are showing the User Manager after locking a profile, change the
  // active profile to Guest.
  profiles::SetActiveProfileToGuestIfLocked();

  DCHECK(!instance_);
  instance_ = instance.release();  // |instance_| takes over ownership.
  instance_->Init(profile_path_to_focus, guest_profile, GURL(url));
}

void UserManagerView::Init(
    const base::FilePath& profile_path_to_focus,
    Profile* guest_profile,
    const GURL& url) {
  web_view_ = new views::WebView(guest_profile);
  web_view_->set_allow_accelerators(true);
  AddChildView(web_view_);
  SetLayoutManager(new views::FillLayout);
  AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));

  // If the user manager is being displayed from an existing profile, use
  // its last active browser to determine where the user manager should be
  // placed.  This is used so that we can center the dialog on the correct
  // monitor in a multiple-monitor setup.
  //
  // If |profile_path_to_focus| is empty (for example, starting up chrome
  // when all existing profiles are locked) or we can't find an active
  // browser, bounds will remain empty and the user manager will be centered on
  // the default monitor by default.
  gfx::Rect bounds;
  if (!profile_path_to_focus.empty()) {
    ProfileManager* manager = g_browser_process->profile_manager();
    if (manager) {
      Profile* profile = manager->GetProfileByPath(profile_path_to_focus);
      DCHECK(profile);
      Browser* browser = chrome::FindLastActiveWithProfile(profile,
          chrome::GetActiveDesktop());
      if (browser) {
        gfx::NativeView native_view =
            views::Widget::GetWidgetForNativeWindow(
                browser->window()->GetNativeWindow())->GetNativeView();
        bounds = gfx::Screen::GetScreenFor(native_view)->
            GetDisplayNearestWindow(native_view).work_area();
        bounds.ClampToCenteredSize(gfx::Size(UserManager::kWindowWidth,
                                             UserManager::kWindowHeight));
      }
    }
  }

  DialogDelegate::CreateDialogWidgetWithBounds(this, NULL, NULL, bounds);

  // Since the User Manager can be the only top level window, we don't
  // want to accidentally quit all of Chrome if the user is just trying to
  // unfocus the selected pod in the WebView.
  GetDialogClientView()->RemoveAccelerator(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

#if defined(OS_WIN)
  // Set the app id for the task manager to the app id of its parent
  ui::win::SetAppIdForWindow(
      ShellIntegration::GetChromiumModelIdForProfile(
          guest_profile->GetPath()),
      views::HWNDForWidget(GetWidget()));
#endif
  GetWidget()->Show();

  web_view_->LoadInitialURL(url);
  web_view_->RequestFocus();
}

bool UserManagerView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  DCHECK_EQ(ui::VKEY_W, accelerator.key_code());
  DCHECK_EQ(ui::EF_CONTROL_DOWN, accelerator.modifiers());
  GetWidget()->Close();
  return true;
}

gfx::Size UserManagerView::GetPreferredSize() const {
  return gfx::Size(UserManager::kWindowWidth, UserManager::kWindowHeight);
}

bool UserManagerView::CanResize() const {
  return true;
}

bool UserManagerView::CanMaximize() const {
  return true;
}

bool UserManagerView::CanMinimize() const {
  return true;
}

base::string16 UserManagerView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

int UserManagerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void UserManagerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (instance_ == this)
    instance_ = NULL;
}

bool UserManagerView::UseNewStyleForThisDialog() const {
  return false;
}
