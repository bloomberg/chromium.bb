// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker_webui.h"

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
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/screen.h"
#include "views/widget/native_widget_gtk.h"

namespace {

chromeos::ScreenLockerWebUI* screen_locker_webui_ = NULL;

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

// A event observer used to unlock the screen upon user's action
// without asking password. Used in BWSI and auto login mode.
class InputEventObserver : public MessageLoopForUI::Observer {
 public:
  explicit InputEventObserver(ScreenLocker* screen_locker)
      : screen_locker_(screen_locker),
        activated_(false) {
  }

#if defined(TOUCH_UI)
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    return base::EVENT_CONTINUE;
  }

  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
  }
#else
  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE {
    if ((event->type == GDK_KEY_PRESS ||
         event->type == GDK_BUTTON_PRESS ||
         event->type == GDK_MOTION_NOTIFY) &&
        !activated_) {
      activated_ = true;
      std::string not_used_string;
      GaiaAuthConsumer::ClientLoginResult not_used;
      screen_locker_->OnLoginSuccess(not_used_string,
                                     not_used_string,
                                     not_used,
                                     false,
                                     false);
    }
  }

  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE {
  }
#endif

 private:
  chromeos::ScreenLocker* screen_locker_;

  bool activated_;

  DISALLOW_COPY_AND_ASSIGN(InputEventObserver);
};

// ScreenLockWebUI creates components necessary to authenticate a user to
// unlock the screen.
class ScreenLockWebUI : public WebUILoginView {
 public:
  explicit ScreenLockWebUI(ScreenLocker* screen_locker);
  virtual ~ScreenLockWebUI();

  // WebUILoginView overrides:
  virtual void Init() OVERRIDE;

  // Clears and sets the focus to the password field.
  void ClearAndSetFocusToPassword();

  // Enable/Disable signout button.
  void SetSignoutEnabled(bool enabled);

  // Display error message.
  void DisplayErrorMessage(string16 message);

  void SubmitPassword();

  // Overridden from views::Views:
  virtual bool AcceleratorPressed(
      const views::Accelerator& accelerator) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;

 protected:
  // WebUILoginView overrides:
  virtual views::Widget* GetLoginWindow() OVERRIDE;

 private:
  friend class test::ScreenLockerTester;

  // ScreenLocker is owned by itself.
  ScreenLocker* screen_locker_;

  std::string password_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockWebUI);
};

////////////////////////////////////////////////////////////////////////////////
// ScreenLockerWebUI implementation.

ScreenLockerWebUI::ScreenLockerWebUI(ScreenLocker* screen_locker)
    : ScreenLockerDelegate(screen_locker),
      lock_window_(NULL),
      screen_lock_webui_(NULL) {
  screen_locker_webui_ = this;
}

void ScreenLockerWebUI::Init(bool unlock_on_input) {
  static const GdkColor kGdkBlack = {0, 0, 0, 0};

  gfx::Point left_top(1, 1);
  gfx::Rect init_bounds(gfx::Screen::GetMonitorAreaNearestPoint(left_top));

  LockWindow* lock_window = new LockWindow();
  lock_window_ = lock_window->GetWidget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = init_bounds;
  params.native_widget = lock_window;
  lock_window_->Init(params);
  gtk_widget_modify_bg(
      lock_window_->GetNativeView(), GTK_STATE_NORMAL, &kGdkBlack);

  g_signal_connect(lock_window_->GetNativeView(), "client-event",
                   G_CALLBACK(OnClientEventThunk), this);

  // GTK does not like zero width/height.
  if (!unlock_on_input) {
    screen_lock_webui_ = new ScreenLockWebUI(screen_locker_);
    screen_lock_webui_->Init();
    screen_lock_webui_->SetEnabled(false);
  } else {
    input_event_observer_.reset(new InputEventObserver(screen_locker_));
    MessageLoopForUI::current()->AddObserver(input_event_observer_.get());
  }

  DCHECK(GTK_WIDGET_REALIZED(lock_window_->GetNativeView()));
  WmIpc::instance()->SetWindowType(
      lock_window_->GetNativeView(),
      WM_IPC_WINDOW_CHROME_SCREEN_LOCKER,
      NULL);

  if (screen_lock_webui_)
    lock_window_->SetContentsView(screen_lock_webui_);
  lock_window_->Show();

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

void ScreenLockerWebUI::ScreenLockReady() {
  ScreenLockerDelegate::ScreenLockReady();
  SetInputEnabled(true);
}

void ScreenLockerWebUI::OnAuthenticate() {
}

void ScreenLockerWebUI::SetInputEnabled(bool enabled) {
  // TODO(flackr): Implement enabling / disabling WebUI input.
}

void ScreenLockerWebUI::SetSignoutEnabled(bool enabled) {
  // TODO(flackr): Implement enabling / disabling signout.
}

void ScreenLockerWebUI::ShowErrorMessage(const string16& message,
                                        bool sign_out_only) {
  if (screen_lock_webui_)
    screen_lock_webui_->DisplayErrorMessage(message);
}

void ScreenLockerWebUI::ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                                  const string16& message) {
  ShowErrorMessage(message, true);
}

void ScreenLockerWebUI::ClearErrors() {
  if (screen_lock_webui_)
    screen_lock_webui_->DisplayErrorMessage(string16());
}

ScreenLockerWebUI::~ScreenLockerWebUI() {
  screen_locker_webui_ = NULL;
  if (input_event_observer_.get())
    MessageLoopForUI::current()->RemoveObserver(input_event_observer_.get());

  DCHECK(lock_window_);
  VLOG(1) << "~ScreenLocker(): Closing ScreenLocker window.";
  lock_window_->Close();
}

void ScreenLockerWebUI::OnWindowManagerReady() {
  DVLOG(1) << "OnClientEvent: drawn for lock";
  ScreenLockReady();
}

void ScreenLockerWebUI::OnClientEvent(GtkWidget* widge, GdkEventClient* event) {
  WmIpc::Message msg;
  WmIpc::instance()->DecodeMessage(*event, &msg);
  if (msg.type() == WM_IPC_MESSAGE_CHROME_NOTIFY_SCREEN_REDRAWN_FOR_LOCK)
    OnWindowManagerReady();
}

////////////////////////////////////////////////////////////////////////////////
// ScreenLockWebUI implementation.

ScreenLockWebUI::ScreenLockWebUI(ScreenLocker* screen_locker)
    : screen_locker_(screen_locker) {
  DCHECK(screen_locker_);
}

ScreenLockWebUI::~ScreenLockWebUI() {
}

void ScreenLockWebUI::Init() {
  WebUILoginView::Init();
  LoadURL(GURL(chrome::kChromeUILockScreenURL));
}

void ScreenLockWebUI::ClearAndSetFocusToPassword() {
  // TODO(flackr): Send a javascript message to clear and focus the password
  // field.
  NOTIMPLEMENTED();
}

void ScreenLockWebUI::SetSignoutEnabled(bool enabled) {
  // TODO(flackr): Send a Javascript message to enable / disable the signout
  // link.
  NOTIMPLEMENTED();
}

void ScreenLockWebUI::DisplayErrorMessage(string16 message) {
  WebUI* webui = GetWebUI();
  if (webui) {
    base::StringValue errorMessage(message);
    webui->CallJavascriptFunction("lockScreen.displayError", errorMessage);
  }
}

void ScreenLockWebUI::SubmitPassword() {
  screen_locker_->Authenticate(ASCIIToUTF16(password_.c_str()));
}

bool ScreenLockWebUI::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  // Override WebUILoginView::AcceleratorPressed to prevent call to WebUI
  // cr.ui.Oobe functions.
  // TODO(flackr): This might be able to be removed once merging with the login
  //     screen WebUI is complete.
  return true;
}

void ScreenLockWebUI::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  // Override WebUILoginView::HandleKeyboardEvent to prevent call to WebUI
  // cr.ui.Oobe functions.
  // TODO(flackr): This might be able to be removed once merging with the login
  //     screen WebUI is complete.
}

views::Widget* ScreenLockWebUI::GetLoginWindow() {
  DCHECK(screen_locker_webui_);
  return screen_locker_webui_->lock_window_;
}

}  // namespace chromeos
