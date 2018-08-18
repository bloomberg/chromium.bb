// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller_views.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, public:

BrowserNonClientFrameViewMac::BrowserNonClientFrameViewMac(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
  fullscreen_toolbar_controller_.reset([[FullscreenToolbarControllerViews alloc]
      initWithBrowserView:browser_view]);

  pref_registrar_.Init(browser_view->GetProfile()->GetPrefs());
  pref_registrar_.Add(
      prefs::kShowFullscreenToolbar,
      base::BindRepeating(&BrowserNonClientFrameViewMac::UpdateFullscreenTopUI,
                          base::Unretained(this), false));
}

BrowserNonClientFrameViewMac::~BrowserNonClientFrameViewMac() {
  if ([fullscreen_toolbar_controller_ isInFullscreen])
    [fullscreen_toolbar_controller_ exitFullscreenMode];
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, BrowserNonClientFrameView implementation:

void BrowserNonClientFrameViewMac::OnFullscreenStateChanged() {
  if (browser_view()->IsFullscreen()) {
    [fullscreen_toolbar_controller_ enterFullscreenMode];
  } else {
    [fullscreen_toolbar_controller_ exitFullscreenMode];
  }
  browser_view()->Layout();
}

bool BrowserNonClientFrameViewMac::CaptionButtonsOnLeadingEdge() const {
  return true;
}

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  // TODO(weili): In the future, we should hide the title bar, and show the
  // tab strip directly under the menu bar. For now, just lay our content
  // under the native title bar. Use the default title bar height to avoid
  // calling through private APIs.
  DCHECK(tabstrip);

  const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
  gfx::Rect bounds = gfx::Rect(0, GetTopInset(restored), width(),
                               tabstrip->GetPreferredSize().height());
  bounds.Inset(GetTabStripLeftInset(), 0,
               GetAfterTabstripItemWidth() + GetTabstripPadding(), 0);
  return bounds;
}

int BrowserNonClientFrameViewMac::GetTopInset(bool restored) const {
  if (!browser_view()->IsTabStripVisible())
    return 0;

  // Mac seems to reserve 1 DIP of the top inset as a resize handle.
  constexpr int kResizeHandleHeight = 1;
  constexpr int kTabstripTopInset = 8;
  int top_inset = kTabstripTopInset;
  if (EverHasVisibleBackgroundTabShapes()) {
    top_inset =
        std::max(top_inset, BrowserNonClientFrameView::kMinimumDragHeight +
                                kResizeHandleHeight);
  }

  // Calculate the y offset for the tab strip because in fullscreen mode the tab
  // strip may need to move under the slide down menu bar.
  CGFloat y_offset = top_inset;
  if (browser_view()->IsFullscreen()) {
    CGFloat menu_bar_height =
        [[[NSApplication sharedApplication] mainMenu] menuBarHeight];
    CGFloat title_bar_height =
        NSHeight([NSWindow frameRectForContentRect:NSZeroRect
                                         styleMask:NSWindowStyleMaskTitled]);
    CGFloat added_height =
        [[fullscreen_toolbar_controller_ menubarTracker] menubarFraction] *
        (menu_bar_height + title_bar_height);
    y_offset += added_height;

    if (added_height > 0) {
      // When menubar shows up, we need to update mouse tracking area.
      NSWindow* window = GetWidget()->GetNativeWindow();
      NSRect content_bounds = [[window contentView] bounds];
      // Backing bar tracking area uses native coordinates.
      CGFloat backing_bar_height = FullscreenBackingBarHeight();
      NSRect backing_bar_area =
          NSMakeRect(0, NSMaxY(content_bounds) - backing_bar_height - y_offset,
                     NSWidth(content_bounds), backing_bar_height + y_offset);
      [fullscreen_toolbar_controller_ updateToolbarFrame:backing_bar_area];
    }
  }

  return y_offset;
}

int BrowserNonClientFrameViewMac::GetAfterTabstripItemWidth() const {
  int item_width;
  views::View* profile_switcher_button = GetProfileSwitcherButton();
  if (profile_indicator_icon() && browser_view()->IsTabStripVisible())
    item_width = profile_indicator_icon()->width();
  else if (profile_switcher_button)
    item_width = profile_switcher_button->GetPreferredSize().width();
  else
    return 0;
  return item_width + GetAvatarIconPadding();
}

int BrowserNonClientFrameViewMac::GetThemeBackgroundXInset() const {
  return 0;
}

void BrowserNonClientFrameViewMac::UpdateFullscreenTopUI(
    bool is_exiting_fullscreen) {
  FullscreenToolbarStyle old_style =
      [fullscreen_toolbar_controller_ toolbarStyle];

  // Update to the new toolbar style if needed.
  FullscreenToolbarStyle new_style;
  FullscreenController* controller =
      browser_view()->GetExclusiveAccessManager()->fullscreen_controller();
  if ((controller->IsWindowFullscreenForTabOrPending() ||
       controller->IsExtensionFullscreenOrPending()) &&
      !is_exiting_fullscreen) {
    new_style = FullscreenToolbarStyle::TOOLBAR_NONE;
  } else {
    PrefService* prefs = browser_view()->GetProfile()->GetPrefs();
    new_style = prefs->GetBoolean(prefs::kShowFullscreenToolbar)
                    ? FullscreenToolbarStyle::TOOLBAR_PRESENT
                    : FullscreenToolbarStyle::TOOLBAR_HIDDEN;
  }
  [fullscreen_toolbar_controller_ setToolbarStyle:new_style];

  if (old_style != new_style) {
    // Notify browser that top ui state has been changed so that we can update
    // the bookmark bar state as well.
    browser_view()->browser()->FullscreenTopUIStateChanged();

    // Re-layout if toolbar style changes in fullscreen mode.
    if (frame()->IsFullscreen())
      browser_view()->Layout();

    [FullscreenToolbarController recordToolbarStyle:new_style];
  }
}

bool BrowserNonClientFrameViewMac::ShouldHideTopUIForFullscreen() const {
  if (frame()->IsFullscreen()) {
    return [fullscreen_toolbar_controller_ toolbarStyle] !=
           FullscreenToolbarStyle::TOOLBAR_PRESENT;
  }
  return false;
}

void BrowserNonClientFrameViewMac::UpdateThrobber(bool running) {
}

int BrowserNonClientFrameViewMac::GetTabStripLeftInset() const {
  constexpr int kTabstripLeftInset = 70;  // Make room for caption buttons.

  if (frame()->IsFullscreen())
    return 0;  // Do not draw caption buttons on fullscreen.
  else
    return kTabstripLeftInset;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::NonClientFrameView implementation:

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int BrowserNonClientFrameViewMac::NonClientHitTest(const gfx::Point& point) {
  views::View* profile_switcher_view = GetProfileSwitcherButton();
  if (profile_switcher_view) {
    gfx::Point point_in_switcher(point);
    views::View::ConvertPointToTarget(this, profile_switcher_view,
                                      &point_in_switcher);
    if (profile_switcher_view->HitTestPoint(point_in_switcher)) {
      return HTCLIENT;
    }
  }
  int component = frame()->client_view()->NonClientHitTest(point);

  // BrowserView::NonClientHitTest will return HTNOWHERE for points that hit
  // the native title bar. On Mac, we need to explicitly return HTCAPTION for
  // those points.
  if (component == HTNOWHERE && bounds().Contains(point))
    return HTCAPTION;

  return component;
}

void BrowserNonClientFrameViewMac::GetWindowMask(const gfx::Size& size,
                                                 gfx::Path* window_mask) {
}

void BrowserNonClientFrameViewMac::ResetWindowControls() {
}

void BrowserNonClientFrameViewMac::UpdateWindowIcon() {
}

void BrowserNonClientFrameViewMac::UpdateWindowTitle() {
}

void BrowserNonClientFrameViewMac::SizeConstraintsChanged() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::View implementation:

gfx::Size BrowserNonClientFrameViewMac::GetMinimumSize() const {
  gfx::Size size = browser_view()->GetMinimumSize();
  constexpr gfx::Size kMinTabbedWindowSize(400, 272);
  constexpr gfx::Size kMinPopupWindowSize(100, 122);
  size.SetToMax(browser_view()->browser()->is_type_tabbed()
                    ? kMinTabbedWindowSize
                    : kMinPopupWindowSize);
  return size;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, protected:

// views::View:

void BrowserNonClientFrameViewMac::Layout() {
  DCHECK(browser_view());
  views::View* profile_switcher_button = GetProfileSwitcherButton();
  if (profile_indicator_icon() && browser_view()->IsTabStripVisible()) {
    LayoutIncognitoButton();
    // Mac lays out the incognito icon on the right, as the stoplight
    // buttons live in its Windows/Linux location.
    profile_indicator_icon()->SetX(width() - GetAfterTabstripItemWidth());
  } else if (profile_switcher_button) {
    gfx::Size button_size = profile_switcher_button->GetPreferredSize();
    int button_x = width() - GetAfterTabstripItemWidth();
    int button_y = 0;
    TabStrip* tabstrip = browser_view()->tabstrip();
    if (tabstrip && browser_view()->IsTabStripVisible()) {
      int new_tab_button_bottom =
          tabstrip->bounds().y() + tabstrip->new_tab_button_bounds().height();
      // Align the switcher's bottom to bottom of the new tab button;
      button_y = new_tab_button_bottom - button_size.height();
    }
    profile_switcher_button->SetBounds(button_x, button_y, button_size.width(),
                                       button_size.height());
  }
  BrowserNonClientFrameView::Layout();
}

void BrowserNonClientFrameViewMac::OnPaint(gfx::Canvas* canvas) {
  if (!browser_view()->IsBrowserTypeNormal())
    return;

  canvas->DrawColor(GetFrameColor());

  if (!GetThemeProvider()->UsingSystemTheme())
    PaintThemedFrame(canvas);

  if (browser_view()->IsToolbarVisible())
    PaintToolbarTopStroke(canvas);
}

// BrowserNonClientFrameView:
AvatarButtonStyle BrowserNonClientFrameViewMac::GetAvatarButtonStyle() const {
  return AvatarButtonStyle::NATIVE;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, private:

void BrowserNonClientFrameViewMac::PaintThemedFrame(gfx::Canvas* canvas) {
  gfx::ImageSkia image = GetFrameImage();
  canvas->TileImageInt(image, 0, 0, width(), image.height());
  gfx::ImageSkia overlay = GetFrameOverlayImage();
  canvas->DrawImageInt(overlay, 0, 0);
}

CGFloat BrowserNonClientFrameViewMac::FullscreenBackingBarHeight() const {
  BrowserView* browser_view = this->browser_view();
  DCHECK(browser_view->IsFullscreen());

  CGFloat total_height = 0;
  if (browser_view->IsTabStripVisible())
    total_height += browser_view->GetTabStripHeight();

  if (browser_view->IsToolbarVisible())
    total_height += browser_view->GetToolbarBounds().height();

  if (browser_view->IsBookmarkBarVisible() &&
      browser_view->GetBookmarkBarView()->IsDetached()) {
    // Only when the bookmark bar is shown and not in 'detached' mode, it will
    // show up along with slide down panel.
    total_height += browser_view->GetBookmarkBarView()->height();
  }

  return total_height;
}
