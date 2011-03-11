// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_win.h"

#include <dwmapi.h>
#include <shellapi.h>

#include <set>

#include "chrome/browser/accessibility/browser_accessibility_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"
#include "grit/theme_resources.h"
#include "ui/gfx/font.h"
#include "views/screen.h"
#include "views/widget/widget_win.h"
#include "views/window/window_delegate.h"
#include "views/window/window_win.h"

// static
static const int kClientEdgeThickness = 3;
static const int kTabDragWindowAlpha = 200;
// We need to offset the DWMFrame into the toolbar so that the blackness
// doesn't show up on our rounded corners.
static const int kDWMFrameTopOffset = 3;
// If not -1, windows are shown with this state.
static int explicit_show_state = -1;

// static (Factory method.)
BrowserFrame* BrowserFrame::Create(BrowserView* browser_view,
                                   Profile* profile) {
  BrowserFrameWin* frame = new BrowserFrameWin(browser_view, profile);
  frame->InitBrowserFrame();
  return frame;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, public:

BrowserFrameWin::BrowserFrameWin(BrowserView* browser_view, Profile* profile)
    : WindowWin(browser_view),
      browser_view_(browser_view),
      root_view_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(delegate_(this)) {
  set_native_browser_frame(this);
  browser_view_->set_frame(this);
  non_client_view()->SetFrameView(CreateFrameViewForWindow());
  // Don't focus anything on creation, selecting a tab will set the focus.
  set_focus_on_creation(false);
}

BrowserFrameWin::~BrowserFrameWin() {
}

void BrowserFrameWin::InitBrowserFrame() {
  WindowWin::Init(NULL, gfx::Rect());
}

// static
void BrowserFrameWin::SetShowState(int state) {
  explicit_show_state = state;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, views::WindowWin overrides:

int BrowserFrameWin::GetShowState() const {
  if (explicit_show_state != -1)
    return explicit_show_state;

  STARTUPINFO si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  GetStartupInfo(&si);
  return si.wShowWindow;
}

gfx::Insets BrowserFrameWin::GetClientAreaInsets() const {
  // Use the default client insets for an opaque frame or a glass popup/app
  // frame.
  if (!non_client_view()->UseNativeFrame() ||
      !browser_view_->IsBrowserTypeNormal()) {
    return WindowWin::GetClientAreaInsets();
  }

  int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
  // In fullscreen mode, we have no frame. In restored mode, we draw our own
  // client edge over part of the default frame.
  if (IsFullscreen())
    border_thickness = 0;
  else if (!IsMaximized())
    border_thickness -= kClientEdgeThickness;
  return gfx::Insets(0, border_thickness, border_thickness, border_thickness);
}

bool BrowserFrameWin::GetAccelerator(int cmd_id,
                                     ui::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
}

void BrowserFrameWin::OnEndSession(BOOL ending, UINT logoff) {
  BrowserList::SessionEnding();
}

void BrowserFrameWin::OnEnterSizeMove() {
  browser_view_->WindowMoveOrResizeStarted();
}

void BrowserFrameWin::OnExitSizeMove() {
  views::WidgetWin::OnExitSizeMove();
}

void BrowserFrameWin::OnInitMenuPopup(HMENU menu, UINT position,
                                      BOOL is_system_menu) {
  browser_view_->PrepareToRunSystemMenu(menu);
}

void BrowserFrameWin::OnMove(const CPoint& point) {
  browser_view_->WindowMoved();
}

void BrowserFrameWin::OnMoving(UINT param, LPRECT new_bounds) {
  browser_view_->WindowMoved();
}

LRESULT BrowserFrameWin::OnNCHitTest(const CPoint& pt) {
  // Only do DWM hit-testing when we are using the native frame.
  if (non_client_view()->UseNativeFrame()) {
    LRESULT result;
    if (DwmDefWindowProc(GetNativeView(), WM_NCHITTEST, 0,
                         MAKELPARAM(pt.x, pt.y), &result)) {
      return result;
    }
  }
  return WindowWin::OnNCHitTest(pt);
}

void BrowserFrameWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  WindowWin::OnWindowPosChanged(window_pos);
  UpdateDWMFrame();

  // Windows lies to us about the position of the minimize button before a
  // window is visible.  We use this position to place the OTR avatar in RTL
  // mode, so when the window is shown, we need to re-layout and schedule a
  // paint for the non-client frame view so that the icon top has the correct
  // position when the window becomes visible.  This fixes bugs where the icon
  // appears to overlay the minimize button.
  // Note that we will call Layout every time SetWindowPos is called with
  // SWP_SHOWWINDOW, however callers typically are careful about not specifying
  // this flag unless necessary to avoid flicker.
  if (window_pos->flags & SWP_SHOWWINDOW) {
    non_client_view()->Layout();
    non_client_view()->SchedulePaint();
  }
}

ThemeProvider* BrowserFrameWin::GetThemeProvider() const {
  return browser_view_->browser()->profile()->GetThemeProvider();
}

void BrowserFrameWin::OnScreenReaderDetected() {
  BrowserAccessibilityState::GetInstance()->OnScreenReaderDetected();
  WindowWin::OnScreenReaderDetected();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, views::Window overrides:

void BrowserFrameWin::Activate() {
  // When running under remote desktop, if the remote desktop client is not
  // active on the users desktop, then none of the windows contained in the
  // remote desktop will be activated.  However, WindowWin::Activate will still
  // bring this browser window to the foreground.  We explicitly set ourselves
  // as the last active browser window to ensure that we get treated as such by
  // the rest of Chrome.
  BrowserList::SetLastActive(browser_view_->browser());

  WindowWin::Activate();
}

views::NonClientFrameView* BrowserFrameWin::CreateFrameViewForWindow() {
  if (AlwaysUseNativeFrame())
    browser_frame_view_ = new GlassBrowserFrameView(this, browser_view_);
  else
    browser_frame_view_ =
        browser::CreateBrowserNonClientFrameView(this, browser_view_);
  return browser_frame_view_;
}

void BrowserFrameWin::UpdateFrameAfterFrameChange() {
  // We need to update the glass region on or off before the base class adjusts
  // the window region.
  UpdateDWMFrame();
  WindowWin::UpdateFrameAfterFrameChange();
}

views::RootView* BrowserFrameWin::CreateRootView() {
  root_view_ = new BrowserRootView(browser_view_, this);
  return root_view_;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, NativeBrowserFrame implementation:

views::NativeWindow* BrowserFrameWin::AsNativeWindow() {
  return this;
}

const views::NativeWindow* BrowserFrameWin::AsNativeWindow() const {
  return this;
}

int BrowserFrameWin::GetMinimizeButtonOffset() const {
  TITLEBARINFOEX titlebar_info;
  titlebar_info.cbSize = sizeof(TITLEBARINFOEX);
  SendMessage(GetNativeView(), WM_GETTITLEBARINFOEX, 0, (WPARAM)&titlebar_info);

  CPoint minimize_button_corner(titlebar_info.rgrect[2].left,
                                titlebar_info.rgrect[2].top);
  MapWindowPoints(HWND_DESKTOP, GetNativeView(), &minimize_button_corner, 1);

  return minimize_button_corner.x;
}

gfx::Rect BrowserFrameWin::GetBoundsForTabStrip(views::View* tabstrip) const {
  return browser_frame_view_->GetBoundsForTabStrip(tabstrip);
}

int BrowserFrameWin::GetHorizontalTabStripVerticalOffset(bool restored) const {
  return browser_frame_view_->GetHorizontalTabStripVerticalOffset(restored);
}

void BrowserFrameWin::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

ui::ThemeProvider* BrowserFrameWin::GetThemeProviderForFrame() const {
  // This is implemented for a different interface than GetThemeProvider is,
  // but they mean the same things.
  return GetThemeProvider();
}

bool BrowserFrameWin::AlwaysUseNativeFrame() const {
  // App panel windows draw their own frame.
  if (browser_view_->IsBrowserTypePanel())
    return false;

  // We don't theme popup or app windows, so regardless of whether or not a
  // theme is active for normal browser windows, we don't want to use the custom
  // frame for popups/apps.
  if (!browser_view_->IsBrowserTypeNormal() &&
      views::WidgetWin::IsAeroGlassEnabled())
    return true;

  // Otherwise, we use the native frame when we're told we should by the theme
  // provider (e.g. no custom theme is active).
  return GetThemeProvider()->ShouldUseNativeFrame();
}

views::View* BrowserFrameWin::GetFrameView() const {
  return browser_frame_view_;
}

void BrowserFrameWin::TabStripDisplayModeChanged() {
  if (GetRootView()->has_children()) {
    // Make sure the child of the root view gets Layout again.
    GetRootView()->GetChildViewAt(0)->InvalidateLayout();
  }
  GetRootView()->Layout();

  UpdateDWMFrame();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin, private:

void BrowserFrameWin::UpdateDWMFrame() {
  // Nothing to do yet, or we're not showing a DWM frame.
  if (!client_view() || !AlwaysUseNativeFrame())
    return;

  MARGINS margins = { 0 };
  if (browser_view_->IsBrowserTypeNormal()) {
    // In fullscreen mode, we don't extend glass into the client area at all,
    // because the GDI-drawn text in the web content composited over it will
    // become semi-transparent over any glass area.
    if (!IsMaximized() && !IsFullscreen()) {
      margins.cxLeftWidth = kClientEdgeThickness + 1;
      margins.cxRightWidth = kClientEdgeThickness + 1;
      margins.cyBottomHeight = kClientEdgeThickness + 1;
      margins.cyTopHeight = kClientEdgeThickness + 1;
    }
    // In maximized mode, we only have a titlebar strip of glass, no side/bottom
    // borders.
    if (!browser_view_->IsFullscreen()) {
      gfx::Rect tabstrip_bounds(
          GetBoundsForTabStrip(browser_view_->tabstrip()));
      margins.cyTopHeight = (browser_view_->UseVerticalTabs() ?
          tabstrip_bounds.y() : tabstrip_bounds.bottom()) + kDWMFrameTopOffset;
    }
  } else {
    // For popup and app windows we want to use the default margins.
  }
  DwmExtendFrameIntoClientArea(GetNativeView(), &margins);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

// static
const gfx::Font& BrowserFrame::GetTitleFont() {
  static gfx::Font* title_font =
      new gfx::Font(views::WindowWin::GetWindowTitleFont());
  return *title_font;
}

