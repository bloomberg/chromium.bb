// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_screen_locker.h"

#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>

// Evil hack to undo X11 evil #define.
#undef None
#undef Status

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/native_widget_gtk.h"

namespace {

// URL which corresponds to the login WebUI.
const char kLoginURL[] = "chrome://oobe/login";

// A ScreenLock window that covers entire screen to keep the keyboard
// focus/events inside the grab widget.
class LockWindow : public views::NativeWidgetGtk {
 public:
  LockWindow()
      : views::NativeWidgetGtk(new views::Widget),
        toplevel_focus_widget_(NULL) {
    EnableDoubleBuffer(true);
  }

  // GTK propagates key events from parents to children.
  // Make sure LockWindow will never handle key events.
  virtual gboolean OnEventKey(GtkWidget* widget, GdkEventKey* event) OVERRIDE {
    // Don't handle key event in the lock window.
    return false;
  }

  virtual gboolean OnButtonPress(GtkWidget* widget,
                                 GdkEventButton* event) OVERRIDE {
    // Don't handle mouse event in the lock wnidow and
    // nor propagate to child.
    return true;
  }

  virtual void OnDestroy(GtkWidget* object) OVERRIDE {
    VLOG(1) << "OnDestroy: LockWindow destroyed";
    views::NativeWidgetGtk::OnDestroy(object);
  }

  virtual void ClearNativeFocus() OVERRIDE {
    DCHECK(toplevel_focus_widget_);
    gtk_widget_grab_focus(toplevel_focus_widget_);
  }

  // Sets the widget to move the focus to when clearning the native
  // widget's focus.
  void set_toplevel_focus_widget(GtkWidget* widget) {
    gtk_widget_set_can_focus(widget, TRUE);
    toplevel_focus_widget_ = widget;
  }

 private:
  // The widget we set focus to when clearning the focus on native
  // widget.  In screen locker, gdk input is grabbed in GrabWidget,
  // and resetting the focus by using gtk_window_set_focus seems to
  // confuse gtk and doesn't let focus move to native widget under
  // GrabWidget.
  GtkWidget* toplevel_focus_widget_;

  DISALLOW_COPY_AND_ASSIGN(LockWindow);
};

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// WebUIScreenLocker implementation.

WebUIScreenLocker::WebUIScreenLocker(ScreenLocker* screen_locker)
    : ScreenLockerDelegate(screen_locker) {
}

void WebUIScreenLocker::LockScreen(bool unlock_on_input) {
  static const GdkColor kGdkBlack = {0, 0, 0, 0};

  gfx::Rect bounds(gfx::Screen::GetMonitorAreaNearestWindow(NULL));

  LockWindow* lock_window = new LockWindow();
  lock_window_ = lock_window->GetWidget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = bounds;
  params.native_widget = lock_window;
  lock_window_->Init(params);
  gtk_widget_modify_bg(
      lock_window_->GetNativeView(), GTK_STATE_NORMAL, &kGdkBlack);

  g_signal_connect(lock_window_->GetNativeView(), "client-event",
                   G_CALLBACK(OnClientEventThunk), this);

  // TODO(flackr): Obscure the screen with black / a screensaver if
  // unlock_on_input.
  DCHECK(GTK_WIDGET_REALIZED(lock_window_->GetNativeView()));
  WmIpc::instance()->SetWindowType(
      lock_window_->GetNativeView(),
      WM_IPC_WINDOW_CHROME_SCREEN_LOCKER,
      NULL);

  WebUILoginView::Init(lock_window_);
  lock_window_->SetContentsView(this);
  lock_window_->Show();
  OnWindowCreated();
  LoadURL(GURL(kLoginURL));
  // User list consisting of a single logged-in user.
  UserList users(1, &chromeos::UserManager::Get()->logged_in_user());
  login_display_.reset(new WebUILoginDisplay(this));
  login_display_->set_background_bounds(bounds);
  login_display_->set_parent_window(
      GTK_WINDOW(lock_window_->GetNativeView()));
  login_display_->Init(users, false, false);

  static_cast<OobeUI*>(GetWebUI())->ShowSigninScreen(login_display_.get());

  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                 content::NotificationService::AllSources());

  // Add the window to its own group so that its grab won't be stolen if
  // gtk_grab_add() gets called on behalf on a non-screen-locker widget (e.g.
  // a modal dialog) -- see http://crosbug.com/8999.  We intentionally do this
  // after calling ClearGtkGrab(), as want to be in the default window group
  // then so we can break any existing GTK grabs.
  GtkWindowGroup* window_group = gtk_window_group_new();
  gtk_window_group_add_window(window_group,
                              GTK_WINDOW(lock_window_->GetNativeView()));
  g_object_unref(window_group);

  lock_window->set_toplevel_focus_widget(lock_window->window_contents());
}

void WebUIScreenLocker::ScreenLockReady() {
  ScreenLockerDelegate::ScreenLockReady();
  SetInputEnabled(true);
}

void WebUIScreenLocker::OnAuthenticate() {
}

void WebUIScreenLocker::SetInputEnabled(bool enabled) {
  login_display_->SetUIEnabled(enabled);
  SetStatusAreaEnabled(enabled);
}

void WebUIScreenLocker::SetSignoutEnabled(bool enabled) {
  // TODO(flackr): Implement.
  NOTIMPLEMENTED();
}

void WebUIScreenLocker::ShowErrorMessage(const string16& message,
                                         bool sign_out_only) {
  // TODO(flackr): Use login_display_ to show error message (requires either
  // adding a method to display error strings or strictly passing error ids).
  base::FundamentalValue login_attempts_value(0);
  base::StringValue error_message(message);
  base::StringValue help_link("");
  base::FundamentalValue help_id(0);
  GetWebUI()->CallJavascriptFunction("cr.ui.Oobe.showSignInError",
                                     login_attempts_value,
                                     error_message,
                                     help_link,
                                     help_id);
}

void WebUIScreenLocker::ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                                   const string16& message) {
  ShowErrorMessage(message, true);
}

void WebUIScreenLocker::ClearErrors() {
  GetWebUI()->CallJavascriptFunction("cr.ui.Oobe.clearErrors");
}

WebUIScreenLocker::~WebUIScreenLocker() {
  DCHECK(lock_window_);
  lock_window_->Close();
}

void WebUIScreenLocker::OnClientEvent(GtkWidget* widge, GdkEventClient* event) {
  WmIpc::Message msg;
  WmIpc::instance()->DecodeMessage(*event, &msg);
  if (msg.type() == WM_IPC_MESSAGE_CHROME_NOTIFY_SCREEN_REDRAWN_FOR_LOCK)
    ScreenLockReady();
}

////////////////////////////////////////////////////////////////////////////////
// WebUIScreenLocker, content::NotificationObserver implementation:

void WebUIScreenLocker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED)
    return;

  const User& user = *content::Details<User>(details).ptr();
  login_display_->OnUserImageChanged(user);
}

////////////////////////////////////////////////////////////////////////////////
// WebUIScreenLocker, LoginDisplay::Delegate implementation:

void WebUIScreenLocker::CreateAccount() {
  NOTREACHED();
}

string16 WebUIScreenLocker::GetConnectedNetworkName() {
  return GetCurrentNetworkName(CrosLibrary::Get()->GetNetworkLibrary());
}

void WebUIScreenLocker::FixCaptivePortal() {
  NOTREACHED();
}

void WebUIScreenLocker::CompleteLogin(const std::string& username,
                                      const std::string& password) {
  NOTREACHED();
}

void WebUIScreenLocker::Login(const std::string& username,
                              const std::string& password) {
  DCHECK(username == chromeos::UserManager::Get()->logged_in_user().email());

  chromeos::ScreenLocker::default_screen_locker()->Authenticate(
      ASCIIToUTF16(password));
}

void WebUIScreenLocker::LoginAsGuest() {
  NOTREACHED();
}

void WebUIScreenLocker::OnUserSelected(const std::string& username) {
}

void WebUIScreenLocker::OnStartEnterpriseEnrollment() {
  NOTREACHED();
}

}  // namespace chromeos
