// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_wizard_view.h"

#include <gdk/gdk.h>
#include <signal.h>
#include <sys/types.h>
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/login/image_background.h"
#include "chrome/browser/chromeos/login/login_manager_view.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/common/x11_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/window/hit_test.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

using views::Background;
using views::View;
using views::Widget;

namespace {

const int kLoginWidth = 700;
const int kLoginHeight = 350;

// Names of screens to start login wizard with.
const char kLoginManager[] = "login";

}  // namespace

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

  // There is no non client area.
  virtual int NonClientHitTest(const gfx::Point& point) {
    return bounds().Contains(point) ? HTCLIENT : HTNOWHERE;
  }

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
  static LoginWizardWindow* CreateLoginWizardWindow(
      const std::string start_screen_name) {
    LoginWizardView* login_wizard = new LoginWizardView();
    LoginWizardWindow* login_wizard_window =
        new LoginWizardWindow(login_wizard);
    login_wizard_window->GetNonClientView()->SetFrameView(
        new LoginWizardNonClientFrameView());
    login_wizard->Init(start_screen_name);
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
void ShowLoginWizard(const std::string& start_screen_name) {
  views::WindowGtk* window =
      LoginWizardWindow::CreateLoginWizardWindow(start_screen_name);
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

///////////////////////////////////////////////////////////////////////////////
// LoginWizardView, public:
LoginWizardView::LoginWizardView()
    : background_pixbuf_(NULL),
      status_area_(NULL),
      current_(NULL),
      login_manager_(NULL) {
}

LoginWizardView::~LoginWizardView() {
  MessageLoop::current()->Quit();
}

void LoginWizardView::Init(const std::string& start_view_name) {
  // Load and set the background.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  background_pixbuf_ = rb.GetPixbufNamed(IDR_LOGIN_BACKGROUND);
  set_background(new views::ImageBackground(background_pixbuf_));

  // Store the dimensions of the background image to use it later.
  int width = gdk_pixbuf_get_width(background_pixbuf_);
  int height = gdk_pixbuf_get_height(background_pixbuf_);
  dimensions_.SetSize(width, height);

  InitStatusArea();

  // Create and initialize all views, hidden.
  InitLoginManager();

  // Select the view to start with and show it.
  if (start_view_name == kLoginManager) {
    current_ = login_manager_;
  } else {
    // Default to login manager.
    current_ = login_manager_;
  }
  current_->SetVisible(true);
}

///////////////////////////////////////////////////////////////////////////////
// LoginWizardView, ExitHandlers:
void LoginWizardView::OnLoginSignInSelected() {
  if (window()) {
    window()->Close();
  }
}

///////////////////////////////////////////////////////////////////////////////
// LoginWizardView, private:
void LoginWizardView::InitStatusArea() {
  status_area_ = new chromeos::StatusAreaView(this);
  status_area_->Init();
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  // TODO(avayvod): Check this on RTL interface.
  status_area_->SetBounds(dimensions_.width() - status_area_size.width(), 0,
                          status_area_size.width(),
                          status_area_size.height());
  AddChildView(status_area_);
}

void LoginWizardView::InitLoginManager() {
  login_manager_ = new LoginManagerView(this);
  login_manager_->Init();
  login_manager_->SetBounds((dimensions_.width() - kLoginWidth) / 2,
                            (dimensions_.height() - kLoginHeight) / 2,
                            kLoginWidth,
                            kLoginHeight);
  login_manager_->SetVisible(false);
  AddChildView(login_manager_);
}

///////////////////////////////////////////////////////////////////////////////
// LoginWizardView, chromeos::ScreenObserver overrides:
void LoginWizardView::OnExit(ExitCodes exit_code) {
  switch (exit_code) {
    case LOGIN_SIGN_IN_SELECTED:
      OnLoginSignInSelected();
      break;
    default:
      NOTREACHED();
  }
}

///////////////////////////////////////////////////////////////////////////////
// LoginWizardView, views::View overrides:
gfx::Size LoginWizardView::GetPreferredSize() {
  return dimensions_;
}

///////////////////////////////////////////////////////////////////////////////
// LoginWizardView, views::WindowDelegate overrides:
views::View* LoginWizardView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// LoginWizardView, StatusAreaHost overrides.
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

