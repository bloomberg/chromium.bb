// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_root_window_host_win.h"

#include <dwmapi.h>

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/base/theme_provider.h"
#include "ui/views/controls/menu/native_menu_win.h"

#pragma comment(lib, "dwmapi.lib")

namespace {

const int kClientEdgeThickness = 3;
// We need to offset the DWMFrame into the toolbar so that the blackness
// doesn't show up on our rounded corners.
const int kDWMFrameTopOffset = 3;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHostWin, public:

BrowserDesktopRootWindowHostWin::BrowserDesktopRootWindowHostWin(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    const gfx::Rect& initial_bounds,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopRootWindowHostWin(native_widget_delegate,
                               desktop_native_widget_aura,
                               initial_bounds),
      browser_view_(browser_view),
      browser_frame_(browser_frame) {
}

BrowserDesktopRootWindowHostWin::~BrowserDesktopRootWindowHostWin() {
}

views::NativeMenuWin* BrowserDesktopRootWindowHostWin::GetSystemMenu() {
  if (!system_menu_.get()) {
    system_menu_.reset(
        new views::NativeMenuWin(browser_frame_->GetSystemMenuModel(),
                                 GetHWND()));
    system_menu_->Rebuild();
  }
  return system_menu_.get();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHostWin, BrowserDesktopRootWindowHost implementation:

views::DesktopRootWindowHost*
    BrowserDesktopRootWindowHostWin::AsDesktopRootWindowHost() {
  return this;
}

int BrowserDesktopRootWindowHostWin::GetMinimizeButtonOffset() const {
  return minimize_button_metrics_.GetMinimizeButtonOffsetX();
}

bool BrowserDesktopRootWindowHostWin::UsesNativeSystemMenu() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHostWin, views::DesktopRootWindowHostWin overrides:

int BrowserDesktopRootWindowHostWin::GetInitialShowState() const {
  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  return si.wShowWindow;
}

bool BrowserDesktopRootWindowHostWin::GetClientAreaInsets(
    gfx::Insets* insets) const {
  // Use the default client insets for an opaque frame or a glass popup/app
  // frame.
  if (!GetWidget()->ShouldUseNativeFrame() ||
      !browser_view_->IsBrowserTypeNormal()) {
    return false;
  }

  int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
  // In fullscreen mode, we have no frame. In restored mode, we draw our own
  // client edge over part of the default frame.
  if (GetWidget()->IsFullscreen())
    border_thickness = 0;
  else if (!IsMaximized())
    border_thickness -= kClientEdgeThickness;
  insets->Set(0, border_thickness, border_thickness, border_thickness);
  return true;
}

void BrowserDesktopRootWindowHostWin::HandleFrameChanged() {
  // We need to update the glass region on or off before the base class adjusts
  // the window region.
  UpdateDWMFrame();
  DesktopRootWindowHostWin::HandleFrameChanged();
}

bool BrowserDesktopRootWindowHostWin::PreHandleMSG(UINT message,
                                                   WPARAM w_param,
                                                   LPARAM l_param,
                                                   LRESULT* result) {
  switch (message) {
    case WM_ACTIVATE:
      if (LOWORD(w_param) != WA_INACTIVE)
        minimize_button_metrics_.OnHWNDActivated();
      return false;
    case WM_ENDSESSION:
      browser::SessionEnding();
      return true;
    case WM_INITMENUPOPUP:
      GetSystemMenu()->UpdateStates();
      return true;
  }
  return DesktopRootWindowHostWin::PreHandleMSG(
      message, w_param, l_param, result);
}

void BrowserDesktopRootWindowHostWin::PostHandleMSG(UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  switch (message) {
  case WM_CREATE:
    minimize_button_metrics_.Init(GetHWND());
    break;
  case WM_WINDOWPOSCHANGED:
    UpdateDWMFrame();

    // Windows lies to us about the position of the minimize button before a
    // window is visible.  We use this position to place the OTR avatar in RTL
    // mode, so when the window is shown, we need to re-layout and schedule a
    // paint for the non-client frame view so that the icon top has the correct
    // position when the window becomes visible.  This fixes bugs where the icon
    // appears to overlay the minimize button.
    // Note that we will call Layout every time SetWindowPos is called with
    // SWP_SHOWWINDOW, however callers typically are careful about not
    // specifying this flag unless necessary to avoid flicker.
    // This may be invoked during creation on XP and before the non_client_view
    // has been created.
    WINDOWPOS* window_pos = reinterpret_cast<WINDOWPOS*>(l_param);
    if (window_pos->flags & SWP_SHOWWINDOW && GetWidget()->non_client_view()) {
      GetWidget()->non_client_view()->Layout();
      GetWidget()->non_client_view()->SchedulePaint();
    }
    break;
  }
}

bool BrowserDesktopRootWindowHostWin::IsUsingCustomFrame() const {
  // App panel windows draw their own frame.
  if (browser_view_->IsPanel())
    return true;

  // We don't theme popup or app windows, so regardless of whether or not a
  // theme is active for normal browser windows, we don't want to use the custom
  // frame for popups/apps.
  if (!browser_view_->IsBrowserTypeNormal() &&
      !DesktopRootWindowHostWin::IsUsingCustomFrame()) {
    return false;
  }

  // Otherwise, we use the native frame when we're told we should by the theme
  // provider (e.g. no custom theme is active).
  return !GetWidget()->GetThemeProvider()->ShouldUseNativeFrame();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHostWin, private:

void BrowserDesktopRootWindowHostWin::UpdateDWMFrame() {
  // For "normal" windows on Aero, we always need to reset the glass area
  // correctly, even if we're not currently showing the native frame (e.g.
  // because a theme is showing), so we explicitly check for that case rather
  // than checking browser_frame_->ShouldUseNativeFrame() here.  Using that here
  // would mean we wouldn't reset the glass area to zero when moving from the
  // native frame to an opaque frame, leading to graphical glitches behind the
  // opaque frame.  Instead, we use that function below to tell us whether the
  // frame is currently native or opaque.
  if (!GetWidget()->client_view() || !browser_view_->IsBrowserTypeNormal() ||
      !DesktopRootWindowHostWin::ShouldUseNativeFrame())
    return;

  MARGINS margins = { 0 };

  // If the opaque frame is visible, we use the default (zero) margins.
  // Otherwise, we need to figure out how to extend the glass in.
  if (GetWidget()->ShouldUseNativeFrame()) {
    // In fullscreen mode, we don't extend glass into the client area at all,
    // because the GDI-drawn text in the web content composited over it will
    // become semi-transparent over any glass area.
    if (!IsMaximized() && !GetWidget()->IsFullscreen()) {
      margins.cxLeftWidth = kClientEdgeThickness + 1;
      margins.cxRightWidth = kClientEdgeThickness + 1;
      margins.cyBottomHeight = kClientEdgeThickness + 1;
      margins.cyTopHeight = kClientEdgeThickness + 1;
    }
    // In maximized mode, we only have a titlebar strip of glass, no side/bottom
    // borders.
    if (!browser_view_->IsFullscreen()) {
      gfx::Rect tabstrip_bounds(
          browser_frame_->GetBoundsForTabStrip(browser_view_->tabstrip()));
      margins.cyTopHeight = tabstrip_bounds.bottom() + kDWMFrameTopOffset;
    }
  }

  DwmExtendFrameIntoClientArea(GetHWND(), &margins);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHost, public:

// static
BrowserDesktopRootWindowHost*
    BrowserDesktopRootWindowHost::CreateBrowserDesktopRootWindowHost(
        views::internal::NativeWidgetDelegate* native_widget_delegate,
        views::DesktopNativeWidgetAura* desktop_native_widget_aura,
        const gfx::Rect& initial_bounds,
        BrowserView* browser_view,
        BrowserFrame* browser_frame) {
  return new BrowserDesktopRootWindowHostWin(native_widget_delegate,
                                             desktop_native_widget_aura,
                                             initial_bounds,
                                             browser_view,
                                             browser_frame);
}
