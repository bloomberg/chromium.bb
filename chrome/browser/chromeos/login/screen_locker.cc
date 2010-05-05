// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_lock_view.h"
#include "views/screen.h"
#include "views/widget/widget_gtk.h"

namespace {

// A child widget that grabs both keyboard and pointer input.
// TODO(oshima): catch grab-broke event and quit if it ever happenes.
class GrabWidget : public views::WidgetGtk {
 public:
  GrabWidget() : views::WidgetGtk(views::WidgetGtk::TYPE_CHILD) {
  }

  virtual void Show() {
    views::WidgetGtk::Show();

    GtkWidget* current_grab_window = gtk_grab_get_current();
    if (current_grab_window)
      gtk_grab_remove(current_grab_window);

    DoGrab();
    GdkGrabStatus kbd_status =
        gdk_keyboard_grab(window_contents()->window, FALSE,
                          GDK_CURRENT_TIME);
    CHECK_EQ(kbd_status, GDK_GRAB_SUCCESS) << "Failed to grab keyboard input";
    GdkGrabStatus ptr_status =
        gdk_pointer_grab(window_contents()->window,
                         FALSE,
                         static_cast<GdkEventMask>(
                             GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK),
                         NULL,
                         NULL,
                         GDK_CURRENT_TIME);
    CHECK_EQ(ptr_status, GDK_GRAB_SUCCESS) << "Failed to grab pointer input";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GrabWidget);
};

}  // namespace

namespace chromeos {

ScreenLocker::ScreenLocker(const UserManager::User& user, Profile* profile)
    : lock_window_(NULL),
      lock_widget_(NULL),
      screen_lock_view_(NULL),
      user_(user),
      profile_(profile) {
}

ScreenLocker::~ScreenLocker() {
  DCHECK(lock_window_);
  lock_window_->Close();
  // lock_widget_ will be deleted by gtk's destroy signal.
}

void ScreenLocker::Init(const gfx::Rect& bounds) {
  // TODO(oshima): Figure out which UI to keep and remove in the background.
  views::View* screen = new chromeos::BackgroundView();
  lock_window_ = new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  lock_window_->Init(NULL, bounds);
  lock_window_->SetContentsView(screen);
  lock_window_->Show();
  lock_window_->MoveAbove(NULL);

  authenticator_ =
      LoginUtils::Get()->CreateAuthenticator(this);
  screen_lock_view_ = new ScreenLockView(this);
  screen_lock_view_->Init();

  gfx::Size size = screen_lock_view_->GetPreferredSize();

  lock_widget_ = new GrabWidget();
  lock_widget_->Init(lock_window_->window_contents(),
                     gfx::Rect((bounds.width() - size.width()) /2,
                               (bounds.height() - size.width()) /2,
                               size.width(),
                               size.height()));
  lock_widget_->SetContentsView(screen_lock_view_);
  lock_widget_->Show();
  screen_lock_view_->ClearAndSetFocusToPassword();
}

void ScreenLocker::SetAuthenticator(Authenticator* authenticator) {
  authenticator_ = authenticator;
}

void ScreenLocker::OnLoginFailure(const std::string& error) {
  DLOG(INFO) << "OnLoginFailure";
  screen_lock_view_->SetEnabled(true);
  screen_lock_view_->ClearAndSetFocusToPassword();
}

void ScreenLocker::OnLoginSuccess(const std::string& username,
                                  const std::string& credentials) {
  DLOG(INFO) << "OnLoginSuccess";
  gdk_keyboard_ungrab(GDK_CURRENT_TIME);
  gdk_pointer_ungrab(GDK_CURRENT_TIME);
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void ScreenLocker::Authenticate(const string16& password) {
  screen_lock_view_->SetEnabled(false);
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::Authenticate,
                        profile_,
                        user_.email(),
                        UTF16ToUTF8(password)));
}

}  // namespace chromeos

namespace browser {

void ShowScreenLock(Profile* profile) {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  gfx::Rect bounds(views::Screen::GetMonitorWorkAreaNearestWindow(NULL));

  chromeos::ScreenLocker* locker =
      new chromeos::ScreenLocker(chromeos::UserManager::Get()->logged_in_user(),
                                 profile);
  locker->Init(bounds);
}

}  // namespace browser
