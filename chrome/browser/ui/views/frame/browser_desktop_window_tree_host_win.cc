// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_win.h"

#include <dwmapi.h>

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_frame_common_win.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_window_property_manager_win.h"
#include "chrome/browser/ui/views/frame/system_menu_insertion_delegate_win.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/theme_image_mapper.h"
#include "grit/theme_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/win/dpi.h"
#include "ui/views/controls/menu/native_menu_win.h"

#pragma comment(lib, "dwmapi.lib")

namespace {

const int kClientEdgeThickness = 3;
// We need to offset the DWMFrame into the toolbar so that the blackness
// doesn't show up on our rounded corners.
const int kDWMFrameTopOffset = 3;

// DesktopThemeProvider maps resource ids using MapThemeImage(). This is
// necessary for BrowserDesktopWindowTreeHostWin so that it uses the windows
// theme images rather than the ash theme images.
class DesktopThemeProvider : public ui::ThemeProvider {
 public:
  explicit DesktopThemeProvider(ui::ThemeProvider* delegate)
      : delegate_(delegate) {
  }

  virtual bool UsingSystemTheme() const OVERRIDE {
    return delegate_->UsingSystemTheme();
  }
  virtual gfx::ImageSkia* GetImageSkiaNamed(int id) const OVERRIDE {
    return delegate_->GetImageSkiaNamed(
        chrome::MapThemeImage(chrome::HOST_DESKTOP_TYPE_NATIVE, id));
  }
  virtual SkColor GetColor(int id) const OVERRIDE {
    return delegate_->GetColor(id);
  }
  virtual int GetDisplayProperty(int id) const OVERRIDE {
    return delegate_->GetDisplayProperty(id);
  }
  virtual bool ShouldUseNativeFrame() const OVERRIDE {
    return delegate_->ShouldUseNativeFrame();
  }
  virtual bool HasCustomImage(int id) const OVERRIDE {
    return delegate_->HasCustomImage(
        chrome::MapThemeImage(chrome::HOST_DESKTOP_TYPE_NATIVE, id));
  }
  virtual base::RefCountedMemory* GetRawData(
      int id,
      ui::ScaleFactor scale_factor) const OVERRIDE {
    return delegate_->GetRawData(id, scale_factor);
  }

 private:
  ui::ThemeProvider* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DesktopThemeProvider);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, public:

BrowserDesktopWindowTreeHostWin::BrowserDesktopWindowTreeHostWin(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostWin(native_widget_delegate,
                               desktop_native_widget_aura),
      browser_view_(browser_view),
      browser_frame_(browser_frame),
      did_gdi_clear_(false) {
  scoped_ptr<ui::ThemeProvider> theme_provider(
      new DesktopThemeProvider(ThemeServiceFactory::GetForProfile(
                                   browser_view->browser()->profile())));
  browser_frame->SetThemeProvider(theme_provider.Pass());
}

BrowserDesktopWindowTreeHostWin::~BrowserDesktopWindowTreeHostWin() {
}

views::NativeMenuWin* BrowserDesktopWindowTreeHostWin::GetSystemMenu() {
  if (!system_menu_.get()) {
    SystemMenuInsertionDelegateWin insertion_delegate;
    system_menu_.reset(
        new views::NativeMenuWin(browser_frame_->GetSystemMenuModel(),
                                 GetHWND()));
    system_menu_->Rebuild(&insertion_delegate);
  }
  return system_menu_.get();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, BrowserDesktopWindowTreeHost implementation:

views::DesktopWindowTreeHost*
    BrowserDesktopWindowTreeHostWin::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostWin::GetMinimizeButtonOffset() const {
  return minimize_button_metrics_.GetMinimizeButtonOffsetX();
}

bool BrowserDesktopWindowTreeHostWin::UsesNativeSystemMenu() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, views::DesktopWindowTreeHostWin overrides:

int BrowserDesktopWindowTreeHostWin::GetInitialShowState() const {
  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  return si.wShowWindow;
}

bool BrowserDesktopWindowTreeHostWin::GetClientAreaInsets(
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

void BrowserDesktopWindowTreeHostWin::HandleCreate() {
  DesktopWindowTreeHostWin::HandleCreate();
  browser_window_property_manager_ =
      BrowserWindowPropertyManager::CreateBrowserWindowPropertyManager(
          browser_view_);
  if (browser_window_property_manager_)
    browser_window_property_manager_->UpdateWindowProperties(GetHWND());
}

void BrowserDesktopWindowTreeHostWin::HandleFrameChanged() {
  // Reinitialize the status bubble, since it needs to be initialized
  // differently depending on whether or not DWM composition is enabled
  browser_view_->InitStatusBubble();

  // We need to update the glass region on or off before the base class adjusts
  // the window region.
  UpdateDWMFrame();
  DesktopWindowTreeHostWin::HandleFrameChanged();
}

bool BrowserDesktopWindowTreeHostWin::PreHandleMSG(UINT message,
                                                   WPARAM w_param,
                                                   LPARAM l_param,
                                                   LRESULT* result) {
  switch (message) {
    case WM_ACTIVATE:
      if (LOWORD(w_param) != WA_INACTIVE)
        minimize_button_metrics_.OnHWNDActivated();
      return false;
    case WM_ENDSESSION:
      chrome::SessionEnding();
      return true;
    case WM_INITMENUPOPUP:
      GetSystemMenu()->UpdateStates();
      return true;
  }
  return DesktopWindowTreeHostWin::PreHandleMSG(
      message, w_param, l_param, result);
}

void BrowserDesktopWindowTreeHostWin::PostHandleMSG(UINT message,
                                                    WPARAM w_param,
                                                    LPARAM l_param) {
  switch (message) {
  case WM_CREATE:
    minimize_button_metrics_.Init(GetHWND());
    break;
  case WM_WINDOWPOSCHANGED: {
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
  case WM_ERASEBKGND:
    if (!did_gdi_clear_ && DesktopWindowTreeHostWin::ShouldUseNativeFrame()) {
      // This is necessary to avoid white flashing in the titlebar area around
      // the minimize/maximize/close buttons.
      HDC dc = GetDC(GetHWND());
      MARGINS margins = GetDWMFrameMargins();
      RECT client_rect;
      GetClientRect(GetHWND(), &client_rect);
      HBRUSH brush = CreateSolidBrush(0);
      RECT rect = { 0, 0, client_rect.right, margins.cyTopHeight };
      FillRect(dc, &rect, brush);
      DeleteObject(brush);
      ReleaseDC(GetHWND(), dc);
      did_gdi_clear_ = true;
    }
    break;
  }
}


bool BrowserDesktopWindowTreeHostWin::IsUsingCustomFrame() const {
  // We don't theme popup or app windows, so regardless of whether or not a
  // theme is active for normal browser windows, we don't want to use the custom
  // frame for popups/apps.
  if (!browser_view_->IsBrowserTypeNormal() &&
      !DesktopWindowTreeHostWin::IsUsingCustomFrame()) {
    return false;
  }

  // Otherwise, we use the native frame when we're told we should by the theme
  // provider (e.g. no custom theme is active).
  return !GetWidget()->GetThemeProvider()->ShouldUseNativeFrame();
}

bool BrowserDesktopWindowTreeHostWin::ShouldUseNativeFrame() const {
  if (!views::DesktopWindowTreeHostWin::ShouldUseNativeFrame())
    return false;
  // This function can get called when the Browser window is closed i.e. in the
  // context of the BrowserView destructor.
  if (!browser_view_->browser())
    return false;
  return chrome::ShouldUseNativeFrame(browser_view_,
                                      GetWidget()->GetThemeProvider());
}

void BrowserDesktopWindowTreeHostWin::FrameTypeChanged() {
  views::DesktopWindowTreeHostWin::FrameTypeChanged();
  did_gdi_clear_ = false;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostWin, private:

void BrowserDesktopWindowTreeHostWin::UpdateDWMFrame() {
  // For "normal" windows on Aero, we always need to reset the glass area
  // correctly, even if we're not currently showing the native frame (e.g.
  // because a theme is showing), so we explicitly check for that case rather
  // than checking browser_frame_->ShouldUseNativeFrame() here.  Using that here
  // would mean we wouldn't reset the glass area to zero when moving from the
  // native frame to an opaque frame, leading to graphical glitches behind the
  // opaque frame.  Instead, we use that function below to tell us whether the
  // frame is currently native or opaque.
  if (!GetWidget()->client_view() || !browser_view_->IsBrowserTypeNormal() ||
      !DesktopWindowTreeHostWin::ShouldUseNativeFrame())
    return;

  MARGINS margins = GetDWMFrameMargins();

  DwmExtendFrameIntoClientArea(GetHWND(), &margins);
}

MARGINS BrowserDesktopWindowTreeHostWin::GetDWMFrameMargins() const {
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
      tabstrip_bounds = gfx::win::DIPToScreenRect(tabstrip_bounds);
      margins.cyTopHeight = tabstrip_bounds.bottom() + kDWMFrameTopOffset;
    }
  }
  return margins;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// static
BrowserDesktopWindowTreeHost*
    BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
        views::internal::NativeWidgetDelegate* native_widget_delegate,
        views::DesktopNativeWidgetAura* desktop_native_widget_aura,
        BrowserView* browser_view,
        BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostWin(native_widget_delegate,
                                             desktop_native_widget_aura,
                                             browser_view,
                                             browser_frame);
}
