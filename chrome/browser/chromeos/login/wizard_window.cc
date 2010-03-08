// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <signal.h>
#include <sys/types.h>

#include <string>

#include "base/process_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/common/x11_util.h"
#include "views/screen.h"
#include "views/window/hit_test.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

// X Windows headers have "#define Status int". That interferes with
// NetworkLibrary header which defines enum "Status".
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>

namespace {

// Acts as a frame view with no UI.
class WizardNonClientFrameView : public views::NonClientFrameView {
 public:
  WizardNonClientFrameView() : views::NonClientFrameView() {}

  // Returns just the bounds of the window.
  virtual gfx::Rect GetBoundsForClientView() const { return bounds(); }

  // Doesn't add any size to the client bounds.
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const {
    return client_bounds;
  }

  // There is no non client area.
  virtual int NonClientHitTest(const gfx::Point& point) {
    return bounds().Contains(point) ? HTCLIENT : HTNOWHERE;
  }

  // There is no non client area.
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) {}
  virtual void EnableClose(bool enable) {}
  virtual void ResetWindowControls() {}

  DISALLOW_COPY_AND_ASSIGN(WizardNonClientFrameView);
};

// Subclass of WindowGtk, for use as the top level window.
class WizardWindow : public views::WindowGtk {
 public:
  static WizardWindow* Create(const std::string& first_screen_name,
                              const gfx::Size& size) {
    WizardController* controller = new WizardController();

    WizardWindow* wizard_window = new WizardWindow(controller);
    wizard_window->GetNonClientView()->SetFrameView(
        new WizardNonClientFrameView());

    gfx::Rect wizard_window_rect(0, 0, size.width(), size.height());
    if (size.width() <= 0 || size.height() <= 0) {
      // TODO(dpolukhin): add support for multiple monitors.
      gfx::Rect monitor_bounds =
          views::Screen::GetMonitorWorkAreaNearestWindow(NULL);
      wizard_window_rect.SetRect(monitor_bounds.x(),
                                 monitor_bounds.y(),
                                 monitor_bounds.width(),
                                 monitor_bounds.height());
    }
    wizard_window->Init(NULL, wizard_window_rect);
    controller->ShowFirstScreen(first_screen_name);

    // This keeps the window from flashing at startup.
    GdkWindow* gdk_window = GTK_WIDGET(
        wizard_window->GetNativeWindow())->window;
    gdk_window_set_back_pixmap(gdk_window, NULL, false);

    // This gets rid of the ugly X default cursor.
    Display* display = x11_util::GetXDisplay();
    Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
    XID root_window = x11_util::GetX11RootWindow();
    XSetWindowAttributes attr;
    attr.cursor = cursor;
    XChangeWindowAttributes(display, root_window, CWCursor, &attr);

    return wizard_window;
  }

 private:
  explicit WizardWindow(WizardController* controller)
      : views::WindowGtk(controller) {
  }

  DISALLOW_COPY_AND_ASSIGN(WizardWindow);
};

}  // namespace

namespace browser {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
void ShowLoginWizard(const std::string& first_screen_name,
                     const gfx::Size& size) {
  views::WindowGtk* window = WizardWindow::Create(first_screen_name, size);
  window->Show();
  if (chromeos::CrosLibrary::EnsureLoaded()) {
    chromeos::LoginLibrary::Get()->EmitLoginPromptReady();
  }
}

}  // namespace browser

