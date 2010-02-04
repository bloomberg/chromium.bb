// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login_wizard_view.h"

#include <gdk/gdk.h>
#include <signal.h>
#include <sys/types.h>
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/browser/chromeos/clock_menu_button.h"
#include "chrome/browser/chromeos/image_background.h"
#include "chrome/browser/chromeos/login_library.h"
#include "chrome/browser/chromeos/status_area_view.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/common/x11_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

using views::Background;
using views::View;
using views::Widget;

namespace browser {

// Acts as a frame view with no UI.
class LoginWizardNonClientFrameView : public views::NonClientFrameView {
 public:
  explicit LoginWizardNonClientFrameView() : views::NonClientFrameView() {}

  // Returns just the bounds of the window.
  virtual gfx::Rect GetBoundsForClientView() const { return bounds(); }

  // Doesn't add any size to the client bounds.
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const {
    return client_bounds;
  }

  // There is no system menu.
  virtual gfx::Point GetSystemMenuPoint() const { return gfx::Point(); }
  // There is no non client area.
  virtual int NonClientHitTest(const gfx::Point& point) { return 0; }

  // There is no non client area.
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) {}
  virtual void EnableClose(bool enable) {}
  virtual void ResetWindowControls() {}

  DISALLOW_COPY_AND_ASSIGN(LoginWizardNonClientFrameView);
};

// Subclass of WindowGtk, for use as the top level login window.
class LoginWizardWindow : public views::WindowGtk {
 public:
  static LoginWizardWindow* CreateLoginWizardWindow() {
    LoginWizardView* login_wizard = new LoginWizardView();
    LoginWizardWindow* login_wizard_window =
        new LoginWizardWindow(login_wizard);
    login_wizard_window->GetNonClientView()->SetFrameView(
        new LoginWizardNonClientFrameView());
    login_wizard->Init();
    login_wizard_window->Init(NULL, gfx::Rect());

    // This keeps the window from flashing at startup.
    GdkWindow* gdk_window =
        GTK_WIDGET(login_wizard_window->GetNativeWindow())->window;
    gdk_window_set_back_pixmap(gdk_window, NULL, false);

    // This gets rid of the ugly X default cursor.
    Display* display = x11_util::GetXDisplay();
    Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
    XID root_window = x11_util::GetX11RootWindow();
    XSetWindowAttributes attr;
    attr.cursor = cursor;
    XChangeWindowAttributes(display, root_window, CWCursor, &attr);
    return login_wizard_window;
  }

 private:
  explicit LoginWizardWindow(LoginWizardView* login_wizard)
      : views::WindowGtk(login_wizard) {
  }

  DISALLOW_COPY_AND_ASSIGN(LoginWizardWindow);
};

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
void ShowLoginWizard() {
  views::WindowGtk* window =
      LoginWizardWindow::CreateLoginWizardWindow();
  window->Show();
  if (chromeos::LoginLibrary::EnsureLoaded()) {
    chromeos::LoginLibrary::Get()->EmitLoginPromptReady();
  }
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

}  // namespace browser

LoginWizardView::LoginWizardView() {
}

LoginWizardView::~LoginWizardView() {
  MessageLoop::current()->Quit();
}

void LoginWizardView::Init() {
  InitWizardWindow();
  // TODO(nkostylev): Select initial wizard view based on OOBE switch.
  InitLoginWindow();
}

gfx::Size LoginWizardView::GetPreferredSize() {
  return dimensions_;
}

void LoginWizardView::OnLogin() {
  if (window()) {
    window()->Close();
  }
}

void LoginWizardView::InitLoginWindow() {
  int login_width = dimensions_.width() / 2;
  int login_height = dimensions_.height() / 2;
  LoginManagerView* login_view = new LoginManagerView(login_width,
                                                      login_height);
  login_view->set_observer(this);
  login_view->SetBounds(dimensions_.width() / 4,
                        dimensions_.height() / 4,
                        login_width,
                        login_height);
  AddChildView(login_view);
}

void LoginWizardView::InitWizardWindow() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  background_pixbuf_ = rb.GetPixbufNamed(IDR_LOGIN_BACKGROUND);
  int width = gdk_pixbuf_get_width(background_pixbuf_);
  int height = gdk_pixbuf_get_height(background_pixbuf_);
  dimensions_.SetSize(width, height);
  set_background(new views::ImageBackground(background_pixbuf_));

  status_area_ = new chromeos::StatusAreaView(this);
  status_area_->Init();
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(dimensions_.width() - status_area_size.width(), 0,
                          status_area_size.width(),
                          status_area_size.height());
  AddChildView(status_area_);
}

views::View* LoginWizardView::GetContentsView() {
  return this;
}

// StatusAreaHost overrides.
gfx::NativeWindow LoginWizardView::GetNativeWindow() const {
  return window()->GetNativeWindow();
}

bool LoginWizardView::ShouldOpenButtonOptions(
    const views::View* button_view) const {
  if (button_view == status_area_->clock_view()) {
    return false;
  }
  return true;
}

void LoginWizardView::OpenButtonOptions(const views::View* button_view) const {
  // TODO(avayvod): Add some dialog for options or remove them completely.
}

bool LoginWizardView::IsButtonVisible(const views::View* button_view) const {
  return true;
}

