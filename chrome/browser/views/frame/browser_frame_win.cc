// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_frame_win.h"

#include <dwmapi.h>
#include <shellapi.h>

#include <set>

#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "app/win_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/dock_info.h"
#include "chrome/browser/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/views/frame/browser_root_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/glass_browser_frame_view.h"
#include "chrome/browser/views/frame/opaque_browser_frame_view.h"
#include "chrome/browser/views/tabs/browser_tab_strip.h"
#include "grit/theme_resources.h"
#include "views/screen.h"
#include "views/window/window_delegate.h"

// static
static const int kClientEdgeThickness = 3;
static const int kTabDragWindowAlpha = 200;

// static (Factory method.)
BrowserFrame* BrowserFrame::Create(BrowserView* browser_view,
                                   Profile* profile) {
  BrowserFrameWin* frame = new BrowserFrameWin(browser_view, profile);
  frame->Init();
  return frame;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

BrowserFrameWin::BrowserFrameWin(BrowserView* browser_view, Profile* profile)
    : WindowWin(browser_view),
      browser_view_(browser_view),
      saved_window_style_(0),
      saved_window_ex_style_(0),
      detached_drag_mode_(false),
      drop_tabstrip_(NULL),
      root_view_(NULL),
      frame_initialized_(false),
      profile_(profile) {
  browser_view_->set_frame(this);
  GetNonClientView()->SetFrameView(CreateFrameViewForWindow());
  // Don't focus anything on creation, selecting a tab will set the focus.
  set_focus_on_creation(false);
}

void BrowserFrameWin::Init() {
  WindowWin::Init(NULL, gfx::Rect());
}

BrowserFrameWin::~BrowserFrameWin() {
}

views::Window* BrowserFrameWin::GetWindow() {
  return this;
}

void BrowserFrameWin::TabStripCreated(TabStripWrapper* tabstrip) {
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

gfx::Rect BrowserFrameWin::GetBoundsForTabStrip(
    TabStripWrapper* tabstrip) const {
  return browser_frame_view_->GetBoundsForTabStrip(tabstrip);
}

void BrowserFrameWin::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

void BrowserFrameWin::ContinueDraggingDetachedTab() {
  detached_drag_mode_ = true;

  // Set the frame to partially transparent.
  UpdateWindowAlphaForTabDragging(detached_drag_mode_);

  // Send the message directly, so that the window is positioned appropriately.
  SendMessage(GetNativeWindow(), WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(0, 0));
}

ThemeProvider* BrowserFrameWin::GetThemeProviderForFrame() const {
  // This is implemented for a different interface than GetThemeProvider is,
  // but they mean the same things.
  return GetThemeProvider();
}

bool BrowserFrameWin::AlwaysUseNativeFrame() const {
  // We use the native frame when we're told we should by the theme provider
  // (e.g. no custom theme is active), or when we're a popup or app window. We
  // don't theme popup or app windows, so regardless of whether or not a theme
  // is active for normal browser windows, we don't want to use the custom frame
  // for popups/apps.
  return GetThemeProvider()->ShouldUseNativeFrame() ||
      (!browser_view_->IsBrowserTypeNormal() &&
      win_util::ShouldUseVistaFrame());
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, views::WidgetWin overrides:

bool BrowserFrameWin::GetAccelerator(int cmd_id,
                                     views::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
}

void BrowserFrameWin::OnEndSession(BOOL ending, UINT logoff) {
  BrowserList::WindowsSessionEnding();
}

void BrowserFrameWin::OnEnterSizeMove() {
  drop_tabstrip_ = NULL;
  browser_view_->WindowMoveOrResizeStarted();
}

void BrowserFrameWin::OnExitSizeMove() {
  if (TabStrip2::Enabled()) {
    if (detached_drag_mode_) {
      detached_drag_mode_ = false;
      if (drop_tabstrip_) {
        gfx::Point screen_point = views::Screen::GetCursorScreenPoint();
        BrowserTabStrip* tabstrip =
            browser_view_->tabstrip()->AsBrowserTabStrip();
        gfx::Rect tsb = tabstrip->GetDraggedTabScreenBounds(screen_point);
        drop_tabstrip_->AttachTab(tabstrip->DetachTab(0), screen_point, tsb);
      } else {
        UpdateWindowAlphaForTabDragging(detached_drag_mode_);
        browser_view_->tabstrip()->AsBrowserTabStrip()->SendDraggedTabHome();
      }
    }
  }
  WidgetWin::OnExitSizeMove();
}

void BrowserFrameWin::OnInitMenuPopup(HMENU menu, UINT position,
                                      BOOL is_system_menu) {
  browser_view_->PrepareToRunSystemMenu(menu);
}

LRESULT BrowserFrameWin::OnMouseActivate(HWND window, UINT hittest_code,
                                         UINT message) {
  return browser_view_->ActivateAppModalDialog() ? MA_NOACTIVATEANDEAT
                                                 : MA_ACTIVATE;
}

void BrowserFrameWin::OnMove(const CPoint& point) {
  browser_view_->WindowMoved();
}

void BrowserFrameWin::OnMoving(UINT param, LPRECT new_bounds) {
  browser_view_->WindowMoved();
}

LRESULT BrowserFrameWin::OnNCActivate(BOOL active) {
  if (browser_view_->ActivateAppModalDialog())
    return TRUE;

  browser_view_->ActivationChanged(!!active);
  return WindowWin::OnNCActivate(active);
}

LRESULT BrowserFrameWin::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // This class' client rect calculations are specific to the tabbed browser
  // window. When the glass frame is active, the client area is reported to be
  // a rectangle that touches the top of the window and is inset from the left,
  // right and bottom edges. The client rect touches the top because the
  // tabstrip is painted over the caption at a custom offset.
  // When the glass frame is not active, the client area is reported to be the
  // entire window rect, except for the cases noted below.
  // For non-tabbed browser windows, we use the default handling from the
  // views system.
  if (!browser_view_->IsBrowserTypeNormal())
    return WindowWin::OnNCCalcSize(mode, l_param);

  RECT* client_rect = mode ?
      &reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param)->rgrc[0] :
      reinterpret_cast<RECT*>(l_param);
  int border_thickness = 0;
  if (browser_view_->IsMaximized()) {
    // Make the maximized mode client rect fit the screen exactly, by
    // subtracting the border Windows automatically adds for maximized mode.
    border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
    // Find all auto-hide taskbars along the screen edges and adjust in by the
    // thickness of the auto-hide taskbar on each such edge, so the window isn't
    // treated as a "fullscreen app", which would cause the taskbars to
    // disappear.
    HMONITOR monitor = MonitorFromWindow(GetNativeView(),
                                         MONITOR_DEFAULTTONULL);
    if (win_util::EdgeHasTopmostAutoHideTaskbar(ABE_LEFT, monitor))
      client_rect->left += win_util::kAutoHideTaskbarThicknessPx;
    if (win_util::EdgeHasTopmostAutoHideTaskbar(ABE_RIGHT, monitor))
      client_rect->right -= win_util::kAutoHideTaskbarThicknessPx;
    if (win_util::EdgeHasTopmostAutoHideTaskbar(ABE_BOTTOM, monitor)) {
      client_rect->bottom -= win_util::kAutoHideTaskbarThicknessPx;
    } else if (win_util::EdgeHasTopmostAutoHideTaskbar(ABE_TOP, monitor)) {
      // Tricky bit.  Due to a bug in DwmDefWindowProc()'s handling of
      // WM_NCHITTEST, having any nonclient area atop the window causes the
      // caption buttons to draw onscreen but not respond to mouse hover/clicks.
      // So for a taskbar at the screen top, we can't push the client_rect->top
      // down; instead, we move the bottom up by one pixel, which is the
      // smallest change we can make and still get a client area less than the
      // screen size. This is visibly ugly, but there seems to be no better
      // solution.
      --client_rect->bottom;
    }
  } else if (!browser_view_->IsFullscreen()) {
    if (GetNonClientView()->UseNativeFrame()) {
      // We draw our own client edge over part of the default frame.
      border_thickness =
          GetSystemMetrics(SM_CXSIZEFRAME) - kClientEdgeThickness;
    } else {
      // This is weird, but highly essential. If we don't offset the bottom edge
      // of the client rect, the window client area and window area will match,
      // and when returning to glass rendering mode from non-glass, the client
      // area will not paint black as transparent. This is because (and I don't
      // know why) the client area goes from matching the window rect to being
      // something else. If the client area is not the window rect in both
      // modes, the blackness doesn't occur. Because of this, we need to tell
      // the RootView to lay out to fit the window rect, rather than the client
      // rect when using the opaque frame. See SizeRootViewToWindowRect.
      --client_rect->bottom;
    }
  }
  client_rect->left += border_thickness;
  client_rect->right -= border_thickness;
  client_rect->bottom -= border_thickness;

  // We'd like to return WVR_REDRAW in some cases here, but because we almost
  // always have nonclient area (except in fullscreen mode, where it doesn't
  // matter), we can't.  See comments in window.cc:OnNCCalcSize() for more info.
  return 0;
}

LRESULT BrowserFrameWin::OnNCHitTest(const CPoint& pt) {
  // Only do DWM hit-testing when we are using the native frame.
  if (GetNonClientView()->UseNativeFrame()) {
    LRESULT result;
    if (DwmDefWindowProc(GetNativeView(), WM_NCHITTEST, 0,
                         MAKELPARAM(pt.x, pt.y), &result)) {
      return result;
    }
  }
  return WindowWin::OnNCHitTest(pt);
}

void BrowserFrameWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  if (TabStrip2::Enabled()) {
    if (detached_drag_mode_) {
      // TODO(beng): move all to BrowserTabStrip...

      // We check to see if the mouse cursor is in the magnetism zone of another
      // visible TabStrip. If so, we should dock to it.
      std::set<HWND> ignore_windows;
      ignore_windows.insert(GetNativeWindow());

      gfx::Point screen_point = views::Screen::GetCursorScreenPoint();
      HWND local_window =
          DockInfo::GetLocalProcessWindowAtPoint(screen_point, ignore_windows);
      if (local_window) {
        BrowserView* browser_view =
            BrowserView::GetBrowserViewForNativeWindow(local_window);
        drop_tabstrip_ = browser_view->tabstrip()->AsBrowserTabStrip();
        if (TabStrip2::IsDragRearrange(drop_tabstrip_, screen_point)) {
          ReleaseCapture();
          return;
        }
      }
      drop_tabstrip_ = NULL;
    }
  }

  // Windows lies to us about the position of the minimize button before a
  // window is visible. We use the position of the minimize button to place the
  // distributor logo in official builds. When the window is shown, we need to
  // re-layout and schedule a paint for the non-client frame view so that the
  // distributor logo has the correct position when the window becomes visible.
  // This fixes bugs where the distributor logo appears to overlay the minimize
  // button. http://crbug.com/15520
  // Note that we will call Layout every time SetWindowPos is called with
  // SWP_SHOWWINDOW, however callers typically are careful about not specifying
  // this flag unless necessary to avoid flicker.
  if (window_pos->flags & SWP_SHOWWINDOW) {
    GetNonClientView()->Layout();
    GetNonClientView()->SchedulePaint();
  }

  UpdateDWMFrame();

  // Let the default window procedure handle - IMPORTANT!
  WindowWin::OnWindowPosChanged(window_pos);
}

ThemeProvider* BrowserFrameWin::GetThemeProvider() const {
  return profile_->GetThemeProvider();
}

ThemeProvider* BrowserFrameWin::GetDefaultThemeProvider() const {
  return profile_->GetThemeProvider();
}

bool BrowserFrameWin::SizeRootViewToWindowRect() const {
  return !GetNonClientView()->UseNativeFrame();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, views::CustomFrameWindow overrides:

int BrowserFrameWin::GetShowState() const {
  return browser_view_->GetShowState();
}

views::NonClientFrameView* BrowserFrameWin::CreateFrameViewForWindow() {
  if (AlwaysUseNativeFrame())
    browser_frame_view_ = new GlassBrowserFrameView(this, browser_view_);
  else
    browser_frame_view_ = new OpaqueBrowserFrameView(this, browser_view_);
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

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, private:

void BrowserFrameWin::UpdateDWMFrame() {
  // Nothing to do yet, or we're not showing a DWM frame.
  if (!GetClientView() || !AlwaysUseNativeFrame())
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
    }
    // In maximized mode, we only have a titlebar strip of glass, no side/bottom
    // borders.
    if (!browser_view_->IsFullscreen()) {
      margins.cyTopHeight =
          GetBoundsForTabStrip(browser_view_->tabstrip()).bottom();
    }
  } else {
    // For popup and app windows we want to use the default margins.
  }
  DwmExtendFrameIntoClientArea(GetNativeView(), &margins);
}

void BrowserFrameWin::UpdateWindowAlphaForTabDragging(bool dragging) {
  HWND frame_hwnd = GetNativeWindow();
  if (dragging) {
    // Make the frame slightly transparent during the drag operation.
    saved_window_style_ = ::GetWindowLong(frame_hwnd, GWL_STYLE);
    saved_window_ex_style_ = ::GetWindowLong(frame_hwnd, GWL_EXSTYLE);
    ::SetWindowLong(frame_hwnd, GWL_EXSTYLE,
                    saved_window_ex_style_ | WS_EX_LAYERED);
    // Remove the captions tyle so the window doesn't have window controls for a
    // more "transparent" look.
    ::SetWindowLong(frame_hwnd, GWL_STYLE,
                    saved_window_style_ & ~WS_CAPTION);
    SetLayeredWindowAttributes(frame_hwnd, RGB(0xFF, 0xFF, 0xFF),
                               kTabDragWindowAlpha, LWA_ALPHA);
  } else {
    ::SetWindowLong(frame_hwnd, GWL_STYLE, saved_window_style_);
    ::SetWindowLong(frame_hwnd, GWL_EXSTYLE, saved_window_ex_style_);
  }
}
