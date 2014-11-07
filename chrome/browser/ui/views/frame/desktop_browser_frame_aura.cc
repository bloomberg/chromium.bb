// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura.h"

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host.h"
#include "chrome/browser/ui/views/frame/browser_shutdown.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/font.h"
#include "ui/views/view.h"
#include "ui/wm/core/visibility_controller.h"

#if defined(OS_LINUX)
#include "chrome/browser/shell_integration_linux.h"
#endif

using aura::Window;

///////////////////////////////////////////////////////////////////////////////
// DesktopBrowserFrameAura, public:

DesktopBrowserFrameAura::DesktopBrowserFrameAura(
    BrowserFrame* browser_frame,
    BrowserView* browser_view)
    : views::DesktopNativeWidgetAura(browser_frame),
      browser_view_(browser_view),
      browser_frame_(browser_frame),
      browser_desktop_window_tree_host_(NULL) {
  GetNativeWindow()->SetName("BrowserFrameAura");
}

///////////////////////////////////////////////////////////////////////////////
// DesktopBrowserFrameAura, protected:

DesktopBrowserFrameAura::~DesktopBrowserFrameAura() {
}

///////////////////////////////////////////////////////////////////////////////
// DesktopBrowserFrameAura, views::DesktopNativeWidgetAura overrides:

void DesktopBrowserFrameAura::OnHostClosed() {
  // Destroy any remaining WebContents early on. Doing so may result in
  // calling back to one of the Views/LayoutManagers or supporting classes of
  // BrowserView. By destroying here we ensure all said classes are valid.
  DestroyBrowserWebContents(browser_view_->browser());
  aura::client::SetVisibilityClient(GetNativeView()->GetRootWindow(), NULL);
  DesktopNativeWidgetAura::OnHostClosed();
}

void DesktopBrowserFrameAura::InitNativeWidget(
    const views::Widget::InitParams& params) {
  browser_desktop_window_tree_host_ =
      BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
          browser_frame_,
          this,
          browser_view_,
          browser_frame_);
  views::Widget::InitParams modified_params = params;
  modified_params.desktop_window_tree_host =
      browser_desktop_window_tree_host_->AsDesktopWindowTreeHost();
  DesktopNativeWidgetAura::InitNativeWidget(modified_params);

  visibility_controller_.reset(new wm::VisibilityController);
  aura::client::SetVisibilityClient(GetNativeView()->GetRootWindow(),
                                    visibility_controller_.get());
  wm::SetChildWindowVisibilityChangesAnimated(
      GetNativeView()->GetRootWindow());
}

////////////////////////////////////////////////////////////////////////////////
// DesktopBrowserFrameAura, NativeBrowserFrame implementation:

views::Widget::InitParams DesktopBrowserFrameAura::GetWidgetParams() {
  views::Widget::InitParams params;
  params.native_widget = this;

#if defined(OS_LINUX)
  // Set up a custom WM_CLASS for some sorts of window types. This allows
  // task switchers in X11 environments to distinguish between main browser
  // windows and e.g app windows.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  const Browser& browser = *browser_view_->browser();
  params.wm_class_class = shell_integration_linux::GetProgramClassName();
  params.wm_class_name = params.wm_class_class;
  if (browser.is_app() && !browser.is_devtools()) {
    // This window is a hosted app or v1 packaged app.
    // NOTE: v2 packaged app windows are created by ChromeNativeAppWindowViews.
    params.wm_class_name = web_app::GetWMClassFromAppName(browser.app_name());
  } else if (command_line.HasSwitch(switches::kUserDataDir)) {
    // Set the class name to e.g. "Chrome (/tmp/my-user-data)".  The
    // class name will show up in the alt-tab list in gnome-shell if
    // you're running a binary that doesn't have a matching .desktop
    // file.
    const std::string user_data_dir =
        command_line.GetSwitchValueNative(switches::kUserDataDir);
    params.wm_class_name += " (" + user_data_dir + ")";
  }
  const char kX11WindowRoleBrowser[] = "browser";
  const char kX11WindowRolePopup[] = "pop-up";
  params.wm_role_name = browser_view_->browser()->is_type_tabbed() ?
      std::string(kX11WindowRoleBrowser) : std::string(kX11WindowRolePopup);
#endif  // defined(OS_LINUX)

  return params;
}

bool DesktopBrowserFrameAura::UsesNativeSystemMenu() const {
  return browser_desktop_window_tree_host_->UsesNativeSystemMenu();
}

int DesktopBrowserFrameAura::GetMinimizeButtonOffset() const {
  return browser_desktop_window_tree_host_->GetMinimizeButtonOffset();
}

bool DesktopBrowserFrameAura::ShouldSaveWindowPlacement() const {
  // The placement can always be stored.
  return true;
}

void DesktopBrowserFrameAura::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  *bounds = GetWidget()->GetRestoredBounds();
  if (IsMaximized())
    *show_state = ui::SHOW_STATE_MAXIMIZED;
  else if (IsMinimized())
    *show_state = ui::SHOW_STATE_MINIMIZED;
  else
    *show_state = ui::SHOW_STATE_NORMAL;
}
