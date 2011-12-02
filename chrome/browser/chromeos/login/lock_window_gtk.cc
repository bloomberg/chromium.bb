// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock_window_gtk.h"

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
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/native_widget_gtk.h"
#include "ui/views/widget/widget.h"

namespace {

// The maximum duration for which locker should try to grab the keyboard and
// mouse and its interval for regrabbing on failure.
const int kMaxGrabFailureSec = 30;
const int64 kRetryGrabIntervalMs = 500;

// Maximum number of times we'll try to grab the keyboard and mouse before
// giving up.  If we hit the limit, Chrome exits and the session is terminated.
const int kMaxGrabFailures = kMaxGrabFailureSec * 1000 / kRetryGrabIntervalMs;

// Define separate methods for each error code so that stack trace
// will tell which error the grab failed with.
void FailedWithGrabAlreadyGrabbed() {
  LOG(FATAL) << "Grab already grabbed";
}
void FailedWithGrabInvalidTime() {
  LOG(FATAL) << "Grab invalid time";
}
void FailedWithGrabNotViewable() {
  LOG(FATAL) << "Grab not viewable";
}
void FailedWithGrabFrozen() {
  LOG(FATAL) << "Grab frozen";
}
void FailedWithUnknownError() {
  LOG(FATAL) << "Grab uknown";
}

}  // namespace

namespace chromeos {

LockWindow* LockWindow::Create() {
  return new LockWindowGtk();
}

////////////////////////////////////////////////////////////////////////////////
// LockWindowGtk implementation.

void LockWindowGtk::Grab(DOMView* dom_view) {
  // Grab on the RenderWidgetHostView hosting the WebUI login screen.
  grab_widget_ = dom_view->dom_contents()->tab_contents()->
      GetRenderWidgetHostView()->GetNativeView();
  ClearGtkGrab();

  // Call this after lock_window_->Show(); otherwise the 1st invocation
  // of gdk_xxx_grab() will always fail.
  TryGrabAllInputs();

  // Add the window to its own group so that its grab won't be stolen if
  // gtk_grab_add() gets called on behalf on a non-screen-locker widget (e.g.
  // a modal dialog) -- see http://crosbug.com/8999.  We intentionally do this
  // after calling ClearGtkGrab(), as want to be in the default window group
  // then so we can break any existing GTK grabs.
  GtkWindowGroup* window_group = gtk_window_group_new();
  gtk_window_group_add_window(window_group,
                              GTK_WINDOW(lock_window_->GetNativeView()));
  g_object_unref(window_group);
}

views::Widget* LockWindowGtk::GetWidget() {
  return views::NativeWidgetGtk::GetWidget();
}

gboolean LockWindowGtk::OnButtonPress(GtkWidget* widget,
                                      GdkEventButton* event) {
  // Never propagate mouse events to parent.
  return true;
}

void LockWindowGtk::OnDestroy(GtkWidget* object) {
  VLOG(1) << "OnDestroy: LockWindow destroyed";
  views::NativeWidgetGtk::OnDestroy(object);
}

void LockWindowGtk::ClearNativeFocus() {
  gtk_widget_grab_focus(window_contents());
}

////////////////////////////////////////////////////////////////////////////////
// LockWindowGtk private:

LockWindowGtk::LockWindowGtk()
    : views::NativeWidgetGtk(new views::Widget),
      lock_window_(NULL),
      grab_widget_(NULL),
      drawn_(false),
      input_grabbed_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      grab_failure_count_(0),
      kbd_grab_status_(GDK_GRAB_INVALID_TIME),
      mouse_grab_status_(GDK_GRAB_INVALID_TIME) {
  Init();
}

LockWindowGtk::~LockWindowGtk() {
}

void LockWindowGtk::Init() {
  static const GdkColor kGdkBlack = {0, 0, 0, 0};
  EnableDoubleBuffer(true);

  gfx::Rect bounds(gfx::Screen::GetMonitorAreaNearestWindow(NULL));

  lock_window_ = GetWidget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = bounds;
  params.native_widget = this;
  lock_window_->Init(params);
  gtk_widget_modify_bg(
      lock_window_->GetNativeView(), GTK_STATE_NORMAL, &kGdkBlack);

  g_signal_connect(lock_window_->GetNativeView(), "client-event",
                   G_CALLBACK(OnClientEventThunk), this);

  DCHECK(GTK_WIDGET_REALIZED(lock_window_->GetNativeView()));
  WmIpc::instance()->SetWindowType(
      lock_window_->GetNativeView(),
      WM_IPC_WINDOW_CHROME_SCREEN_LOCKER,
      NULL);
}

void LockWindowGtk::OnGrabInputs() {
  DVLOG(1) << "OnGrabInputs";
  input_grabbed_ = true;
  if (drawn_ && observer_)
    observer_->OnLockWindowReady();
}

void LockWindowGtk::OnWindowManagerReady() {
  DVLOG(1) << "OnClientEvent: drawn for lock";
  drawn_ = true;
  if (input_grabbed_ && observer_)
    observer_->OnLockWindowReady();
}

void LockWindowGtk::ClearGtkGrab() {
  GtkWidget* current_grab_window;
  // Grab gtk input first so that the menu holding gtk grab will
  // close itself.
  gtk_grab_add(grab_widget_);

  // Make sure there is no gtk grab widget so that gtk simply propagates
  // an event.  GTK maintains grab widgets in a linked-list, so we need to
  // remove until it's empty.
  while ((current_grab_window = gtk_grab_get_current()) != NULL)
    gtk_grab_remove(current_grab_window);
}

void LockWindowGtk::TryGrabAllInputs() {
  // Grab x server so that we can atomically grab and take
  // action when grab fails.
  gdk_x11_grab_server();
  gtk_grab_add(grab_widget_);
  if (kbd_grab_status_ != GDK_GRAB_SUCCESS)
    kbd_grab_status_ = gdk_keyboard_grab(grab_widget_->window,
                                         FALSE,
                                         GDK_CURRENT_TIME);
  if (mouse_grab_status_ != GDK_GRAB_SUCCESS) {
    mouse_grab_status_ =
        gdk_pointer_grab(grab_widget_->window,
                         FALSE,
                         static_cast<GdkEventMask>(
                             GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK),
                         NULL,
                         NULL,
                         GDK_CURRENT_TIME);
  }
  if ((kbd_grab_status_ != GDK_GRAB_SUCCESS ||
       mouse_grab_status_ != GDK_GRAB_SUCCESS) &&
      grab_failure_count_++ < kMaxGrabFailures) {
    LOG(WARNING) << "Failed to grab inputs. Trying again in "
                 << kRetryGrabIntervalMs << " ms: kbd="
                 << kbd_grab_status_ << ", mouse=" << mouse_grab_status_;
    TryUngrabOtherClients();
    gdk_x11_ungrab_server();
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&LockWindowGtk::TryGrabAllInputs,
                   weak_factory_.GetWeakPtr()),
        kRetryGrabIntervalMs);
  } else {
    gdk_x11_ungrab_server();
    GdkGrabStatus status = kbd_grab_status_;
    if (status == GDK_GRAB_SUCCESS) {
      status = mouse_grab_status_;
    }
    switch (status) {
      case GDK_GRAB_SUCCESS:
        break;
      case GDK_GRAB_ALREADY_GRABBED:
        FailedWithGrabAlreadyGrabbed();
        break;
      case GDK_GRAB_INVALID_TIME:
        FailedWithGrabInvalidTime();
        break;
      case GDK_GRAB_NOT_VIEWABLE:
        FailedWithGrabNotViewable();
        break;
      case GDK_GRAB_FROZEN:
        FailedWithGrabFrozen();
        break;
      default:
        FailedWithUnknownError();
        break;
    }
    DVLOG(1) << "Grab Success";
    OnGrabInputs();
  }
}

void LockWindowGtk::TryUngrabOtherClients() {
#if !defined(NDEBUG)
  {
    int event_base, error_base;
    int major, minor;
    // Make sure we have XTest extension.
    DCHECK(XTestQueryExtension(ui::GetXDisplay(),
                               &event_base, &error_base,
                               &major, &minor));
  }
#endif

  // The following code is an attempt to grab inputs by closing
  // supposedly opened menu. This happens when a plugin has a menu
  // opened.
  if (mouse_grab_status_ == GDK_GRAB_ALREADY_GRABBED ||
      mouse_grab_status_ == GDK_GRAB_FROZEN) {
    // Successfully grabbed the keyboard, but pointer is still
    // grabbed by other client. Another attempt to close supposedly
    // opened menu by emulating keypress at the left top corner.
    Display* display = ui::GetXDisplay();
    Window root, child;
    int root_x, root_y, win_x, winy;
    unsigned int mask;
    XQueryPointer(display,
                  ui::GetX11WindowFromGtkWidget(
                      window_contents()),
                  &root, &child, &root_x, &root_y,
                  &win_x, &winy, &mask);
    XTestFakeMotionEvent(display, -1, -10000, -10000, CurrentTime);
    XTestFakeButtonEvent(display, 1, True, CurrentTime);
    XTestFakeButtonEvent(display, 1, False, CurrentTime);
    // Move the pointer back.
    XTestFakeMotionEvent(display, -1, root_x, root_y, CurrentTime);
    XFlush(display);
  } else if (kbd_grab_status_ == GDK_GRAB_ALREADY_GRABBED ||
             kbd_grab_status_ == GDK_GRAB_FROZEN) {
    // Successfully grabbed the pointer, but keyboard is still grabbed
    // by other client. Another attempt to close supposedly opened
    // menu by emulating escape key.  Such situation must be very
    // rare, but handling this just in case
    Display* display = ui::GetXDisplay();
    KeyCode escape = XKeysymToKeycode(display, XK_Escape);
    XTestFakeKeyEvent(display, escape, True, CurrentTime);
    XTestFakeKeyEvent(display, escape, False, CurrentTime);
    XFlush(display);
  }
}

void LockWindowGtk::HandleGtkGrabBroke() {
  // Input should never be stolen from ScreenLocker once it's
  // grabbed.  If this happens, it's a bug and has to be fixed. We
  // let chrome crash to get a crash report and dump, and
  // SessionManager will terminate the session to logout.
  CHECK_NE(GDK_GRAB_SUCCESS, kbd_grab_status_);
  CHECK_NE(GDK_GRAB_SUCCESS, mouse_grab_status_);
}

void LockWindowGtk::OnClientEvent(GtkWidget* widge, GdkEventClient* event) {
  WmIpc::Message msg;
  WmIpc::instance()->DecodeMessage(*event, &msg);
  if (msg.type() == WM_IPC_MESSAGE_CHROME_NOTIFY_SCREEN_REDRAWN_FOR_LOCK)
    OnWindowManagerReady();
}

}  // namespace chromeos
