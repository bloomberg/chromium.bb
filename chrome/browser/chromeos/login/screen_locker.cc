// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/singleton.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_lock_view.h"
#include "chrome/common/notification_service.h"
#include "views/screen.h"
#include "views/widget/widget_gtk.h"

namespace {
// The maxium times that the screen locker should try to grab input,
// and its interval. It has to be able to grab all inputs in 30 seconds,
// otherwise chromium process fails and the session is terminated.
const int64 kRetryGrabIntervalMs = 1000;
const int kGrabFailureLimit = 30;

// Observer to start ScreenLocker when the screen lock
class ScreenLockObserver : public chromeos::ScreenLockLibrary::Observer,
                           public NotificationObserver {
 public:
  ScreenLockObserver() {
    registrar_.Add(this, NotificationType::LOGIN_USER_CHANGED,
                   NotificationService::AllSources());
  }

  // NotificationObserver overrides:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::LOGIN_USER_CHANGED) {
      // Register Screen Lock after login screen to make sure
      // we don't show the screen lock on top of the login screen by accident.
      if (chromeos::CrosLibrary::Get()->EnsureLoaded())
        chromeos::CrosLibrary::Get()->GetScreenLockLibrary()->AddObserver(this);
    }
  }

  virtual void LockScreen(chromeos::ScreenLockLibrary* obj) {
    chromeos::ScreenLocker::Show();
  }

  virtual void UnlockScreen(chromeos::ScreenLockLibrary* obj) {
    chromeos::ScreenLocker::Hide();
  }

  virtual void UnlockScreenFailed(chromeos::ScreenLockLibrary* obj) {
    chromeos::ScreenLocker::UnlockScreenFailed();
  }

 private:
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockObserver);
};

}  // namespace

namespace chromeos {

// static
ScreenLocker* ScreenLocker::screen_locker_ = NULL;

// A child widget that grabs both keyboard and pointer input.
// TODO(oshima): catch grab-broke event and quit if it ever happenes.
class GrabWidget : public views::WidgetGtk {
 public:
  GrabWidget()
      : views::WidgetGtk(views::WidgetGtk::TYPE_CHILD),
        ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
        grab_failure_count_(0),
        kbd_grab_status_(GDK_GRAB_INVALID_TIME),
        mouse_grab_status_(GDK_GRAB_INVALID_TIME) {
  }

  virtual void Show() {
    views::WidgetGtk::Show();
    GtkWidget* current_grab_window = gtk_grab_get_current();
    if (current_grab_window)
      gtk_grab_remove(current_grab_window);

    DoGrab();

    // Now steal all inputs.
    TryGrabAllInputs();
  }

  // Try to grab all inputs. It initiates another try if it fails to
  // grab and the retry count is within a limit, or fails with CHECK.
  void TryGrabAllInputs();

 private:
  ScopedRunnableMethodFactory<GrabWidget> task_factory_;

  // The number times the widget tried to grab all focus.
  int grab_failure_count_;
  // Status of keyboard and mouse grab.
  GdkGrabStatus kbd_grab_status_;
  GdkGrabStatus mouse_grab_status_;

  DISALLOW_COPY_AND_ASSIGN(GrabWidget);
};


void GrabWidget::TryGrabAllInputs() {
  if (kbd_grab_status_ != GDK_GRAB_SUCCESS)
    kbd_grab_status_ = gdk_keyboard_grab(window_contents()->window, FALSE,
                                         GDK_CURRENT_TIME);
  if (mouse_grab_status_ != GDK_GRAB_SUCCESS) {
    mouse_grab_status_ =
        gdk_pointer_grab(window_contents()->window,
                         FALSE,
                         static_cast<GdkEventMask>(
                             GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK),
                         NULL,
                         NULL,
                         GDK_CURRENT_TIME);
  }
  if ((kbd_grab_status_ != GDK_GRAB_SUCCESS ||
       kbd_grab_status_ != GDK_GRAB_SUCCESS) &&
      grab_failure_count_++ < kGrabFailureLimit) {
    DLOG(WARNING) << "Failed to grab inputs. Trying again in 1 second: kbd="
                  << kbd_grab_status_ << ", mouse=" << mouse_grab_status_;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        task_factory_.NewRunnableMethod(&GrabWidget::TryGrabAllInputs),
        kRetryGrabIntervalMs);
  } else {
    CHECK_EQ(GDK_GRAB_SUCCESS, kbd_grab_status_)
        << "Failed to grab keyboard input:" << kbd_grab_status_;
    CHECK_EQ(GDK_GRAB_SUCCESS, mouse_grab_status_)
        << "Failed to grab pointer input:" << mouse_grab_status_;
  }
}

}  // namespace

namespace chromeos {

ScreenLocker::ScreenLocker(const UserManager::User& user)
    : lock_window_(NULL),
      lock_widget_(NULL),
      screen_lock_view_(NULL),
      user_(user) {
  DCHECK(!screen_locker_);
  screen_locker_ = this;
}

ScreenLocker::~ScreenLocker() {
  DCHECK(lock_window_);
  lock_window_->Close();
  // lock_widget_ will be deleted by gtk's destroy signal.
  screen_locker_ = NULL;
  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenUnlockCompleted();
}

void ScreenLocker::Init(const gfx::Rect& bounds) {
  // TODO(oshima): Figure out which UI to keep and remove in the background.
  views::View* screen = new BackgroundView();
  lock_window_ = new views::WidgetGtk(views::WidgetGtk::TYPE_POPUP);
  lock_window_->Init(NULL, bounds);
  lock_window_->SetContentsView(screen);
  lock_window_->Show();

  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  screen_lock_view_ = new ScreenLockView(this);
  screen_lock_view_->Init();

  gfx::Size size = screen_lock_view_->GetPreferredSize();

  lock_widget_ = new GrabWidget();
  lock_widget_->MakeTransparent();
  lock_widget_->InitWithWidget(lock_window_,
                               gfx::Rect((bounds.width() - size.width()) / 2,
                                         (bounds.height() - size.width()) / 2,
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
  EnableInput();
}

void ScreenLocker::OnLoginSuccess(const std::string& username,
                                  const std::string& credentials) {
  DLOG(INFO) << "OnLoginSuccess";

  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenUnlockRequested();
}

void ScreenLocker::Authenticate(const string16& password) {
  screen_lock_view_->SetEnabled(false);
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::AuthenticateToUnlock,
                        user_.email(),
                        UTF16ToUTF8(password)));
}

void ScreenLocker::EnableInput() {
  screen_lock_view_->SetEnabled(true);
  screen_lock_view_->ClearAndSetFocusToPassword();
}

// static
void ScreenLocker::Show() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  // TODO(oshima): Currently, PowerManager may send a lock screen event
  // even if a screen is locked. Investigate & solve the issue and
  // enable this again if it's possible.
  // DCHECK(!screen_locker_);
  if (!screen_locker_) {
    gfx::Rect bounds(views::Screen::GetMonitorWorkAreaNearestWindow(NULL));
    ScreenLocker* locker =
        new ScreenLocker(UserManager::Get()->logged_in_user());
    locker->Init(bounds);
  }
  // TODO(oshima): Wait for a message from WM to complete the process.
  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetScreenLockLibrary()->NotifyScreenLockCompleted();
}

// static
void ScreenLocker::Hide() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  DCHECK(screen_locker_);
  gdk_keyboard_ungrab(GDK_CURRENT_TIME);
  gdk_pointer_ungrab(GDK_CURRENT_TIME);
  MessageLoop::current()->DeleteSoon(FROM_HERE, screen_locker_);
}

// static
void ScreenLocker::UnlockScreenFailed() {
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  if (screen_locker_) {
    screen_locker_->EnableInput();
  } else {
    // This can happen when a user requested unlock, but PowerManager
    // rejected because the computer is closed, then PowerManager unlocked
    // because it's open again and the above failure message arrives.
    // This'd be extremely rare, but may still happen.
    LOG(INFO) << "Screen is unlocked";
  }
}

// static
void ScreenLocker::InitClass() {
  Singleton<ScreenLockObserver>::get();
}

}  // namespace chromeos
