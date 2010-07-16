// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/normal_browser_frame_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "app/x11_util.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/common/chrome_switches.h"
#include "gfx/canvas.h"
#include "gfx/font.h"
#include "gfx/path.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/window/hit_test.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"
#include "views/window/window_resources.h"

namespace {

// The frame border is usually 0 as chromeos has no border. The border
// can be enabled (4px fixed) with the command line option
// "--chromeos-frame" so that one can resize the window on dev machine.
const int kFrameBorderThicknessForDev = 4;

// While resize areas on Windows are normally the same size as the window
// borders, our top area is shrunk by 1 px to make it easier to move the window
// around with our thinner top grabbable strip.  (Incidentally, our side and
// bottom resize areas don't match the frame border thickness either -- they
// span the whole nonclient area, so there's no "dead zone" for the mouse.)
const int kTopResizeAdjust = 1;

// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;

// The icon is inset 2 px from the left frame border.
const int kIconLeftSpacing = 2;

// The top 1 px of the tabstrip is shadow; in maximized mode we push this off
// the top of the screen so the tabs appear flush against the screen edge.
const int kTabstripTopShadowThickness = 1;

const int kCustomFrameBackgroundVerticalOffset = 15;

}  // namespace

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// NormalBrowserFrameView, public:

NormalBrowserFrameView::NormalBrowserFrameView(BrowserFrame* frame,
                                               BrowserView* browser_view)
    : BrowserNonClientFrameView(),
      frame_(frame),
      browser_view_(browser_view) {
  // Normal window does not have the window title/icon.
  DCHECK(!browser_view_->ShouldShowWindowIcon());
  DCHECK(!browser_view_->ShouldShowWindowTitle());
  DCHECK(!frame_->GetWindow()->GetDelegate()->ShouldShowWindowTitle());
  DCHECK(!frame_->GetWindow()->GetDelegate()->ShouldShowWindowIcon());
}

NormalBrowserFrameView::~NormalBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// NormalBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect NormalBrowserFrameView::GetBoundsForTabStrip(
    BaseTabStrip* tabstrip) const {
  int border_thickness = FrameBorderThickness();
  if (browser_view_->UseVerticalTabs()) {
    // BrowserViewLayout adjusts the height/width based on the status area and
    // otr icon.
    gfx::Size ps = tabstrip->GetPreferredSize();
    return gfx::Rect(border_thickness, NonClientTopBorderHeight(),
                     ps.width(), browser_view_->height());
  }
  return gfx::Rect(border_thickness, NonClientTopBorderHeight(),
                   std::max(0, width() - (2 * border_thickness)),
                   tabstrip->GetPreferredHeight());
}

void NormalBrowserFrameView::UpdateThrobber(bool running) {
  // No window icon.
}

gfx::Size NormalBrowserFrameView::GetMinimumSize() {
  gfx::Size min_size(browser_view_->GetMinimumSize());
  int border_thickness = FrameBorderThickness();
  min_size.Enlarge(2 * border_thickness,
                   NonClientTopBorderHeight() + border_thickness);

  int min_titlebar_width = (2 * border_thickness) + kIconLeftSpacing;
  min_size.set_width(std::max(min_size.width(), min_titlebar_width));
  return min_size;
}

///////////////////////////////////////////////////////////////////////////////
// NormalBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect NormalBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

bool NormalBrowserFrameView::AlwaysUseNativeFrame() const {
  return frame_->AlwaysUseNativeFrame();
}

gfx::Rect NormalBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = FrameBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

int NormalBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;
  int frame_component =
      frame_->GetWindow()->GetClientView()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;
  int border_thickness = FrameBorderThickness();
  int window_component = GetHTComponentForFrame(point,
      std::max(0, border_thickness - kTopResizeAdjust), border_thickness,
      kResizeAreaCornerSize, kResizeAreaCornerSize,
      frame_->GetWindow()->GetDelegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void NormalBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                           gfx::Path* window_mask) {
  DCHECK(window_mask);
  // Always maximized.
}

void NormalBrowserFrameView::EnableClose(bool enable) {
  // No close button
}

void NormalBrowserFrameView::ResetWindowControls() {
}

///////////////////////////////////////////////////////////////////////////////
// NormalBrowserFrameView, views::View overrides:

void NormalBrowserFrameView::Paint(gfx::Canvas* canvas) {
  views::Window* window = frame_->GetWindow();
  if (window->IsFullscreen())
    return;  // Nothing is visible, so don't bother to paint.

  PaintMaximizedFrameBorder(canvas);
  PaintToolbarBackground(canvas);
}

void NormalBrowserFrameView::Layout() {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = FrameBorderThickness();
  client_view_bounds_ = gfx::Rect(border_thickness, top_height,
      std::max(0, width() - (2 * border_thickness)),
      std::max(0, height() - top_height - border_thickness));
}

bool NormalBrowserFrameView::HitTest(const gfx::Point& l) const {
  // If the point is outside the bounds of the client area, claim it.
  if (NonClientFrameView::HitTest(l))
    return true;

  // Otherwise claim it only if it's in a non-tab portion of the tabstrip.
  bool vertical_tabs = browser_view_->UseVerticalTabs();
  const gfx::Rect& tabstrip_bounds = browser_view_->tabstrip()->bounds();
  if ((!vertical_tabs && l.y() > tabstrip_bounds.bottom()) ||
      (vertical_tabs && (l.x() > tabstrip_bounds.right() ||
                         l.y() > tabstrip_bounds.bottom()))) {
    return false;
  }

  // We convert from our parent's coordinates since we assume we fill its bounds
  // completely. We need to do this since we're not a parent of the tabstrip,
  // meaning ConvertPointToView would otherwise return something bogus.
  gfx::Point browser_view_point(l);
  View::ConvertPointToView(GetParent(), browser_view_, &browser_view_point);
  return browser_view_->IsPositionInWindowCaption(browser_view_point);
}

void NormalBrowserFrameView::ViewHierarchyChanged(bool is_add,
                                                  views::View* parent,
                                                  views::View* child) {
  if (is_add && child == this) {
    // The Accessibility glue looks for the product name on these two views to
    // determine if this is in fact a Chrome window.
    GetRootView()->SetAccessibleName(l10n_util::GetString(IDS_PRODUCT_NAME));
  }
}

bool NormalBrowserFrameView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_TITLEBAR;
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// NormalBrowserFrameView, TabIconView::TabContentsProvider implementation:

bool NormalBrowserFrameView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // TabContents because in this condition there is not yet a selected tab.
  TabContents* current_tab = browser_view_->GetSelectedTabContents();
  return current_tab ? current_tab->is_loading() : false;
}

SkBitmap NormalBrowserFrameView::GetFavIconForTabIconView() {
  return frame_->GetWindow()->GetDelegate()->GetWindowIcon();
}

///////////////////////////////////////////////////////////////////////////////
// NormalBrowserFrameView, private:

int NormalBrowserFrameView::FrameBorderThickness() const {
  static int border_thickness_ =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeosFrame) ?
      kFrameBorderThicknessForDev : 0;
  return border_thickness_;
}

int NormalBrowserFrameView::NonClientTopBorderHeight() const {
  return std::max(0, FrameBorderThickness() -
      (browser_view_->IsTabStripVisible() ? kTabstripTopShadowThickness : 0));
}

void NormalBrowserFrameView::PaintMaximizedFrameBorder(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();
  views::Window* window = frame_->GetWindow();

  // Window frame mode and color
  SkBitmap* theme_frame;
  int y = 0;
  // Never theme app and popup windows.
  if (!browser_view_->IsBrowserTypeNormal()) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    theme_frame = rb.GetBitmapNamed(ShouldPaintAsActive() ?
        IDR_FRAME : IDR_FRAME_INACTIVE);
  } else if (!browser_view_->IsOffTheRecord()) {
    theme_frame = tp->GetBitmapNamed(ShouldPaintAsActive() ?
        IDR_THEME_FRAME : IDR_THEME_FRAME_INACTIVE);
    // TODO(oshima): gtk based CHROMEOS is using non custom frame
    // mode which does this adjustment. This should be removed
    // once it's fully migrated to views. -1 is due to the layout
    // difference between views and gtk and will be removed.
    // See http://crbug.com/28580.
    y = -kCustomFrameBackgroundVerticalOffset - 1;
  } else {
    theme_frame = tp->GetBitmapNamed(ShouldPaintAsActive() ?
        IDR_THEME_FRAME_INCOGNITO: IDR_THEME_FRAME_INCOGNITO_INACTIVE);
    y = -kCustomFrameBackgroundVerticalOffset - 1;
  }
  // Draw the theme frame.
  canvas->TileImageInt(*theme_frame, 0, y, width(), theme_frame->height());

  // Draw the theme frame overlay
  if (tp->HasCustomImage(IDR_THEME_FRAME_OVERLAY) &&
      browser_view_->IsBrowserTypeNormal()) {
    SkBitmap* theme_overlay = tp->GetBitmapNamed(ShouldPaintAsActive() ?
        IDR_THEME_FRAME_OVERLAY : IDR_THEME_FRAME_OVERLAY_INACTIVE);
    canvas->DrawBitmapInt(*theme_overlay, 0, 0);
  }

  if (!browser_view_->IsToolbarVisible()) {
    // There's no toolbar to edge the frame border, so we need to draw a bottom
    // edge.  The graphic we use for this has a built in client edge, so we clip
    // it off the bottom.
    SkBitmap* top_center =
        tp->GetBitmapNamed(IDR_APP_TOP_CENTER);
    int edge_height = top_center->height() - kClientEdgeThickness;
    canvas->TileImageInt(*top_center, 0,
        window->GetClientView()->y() - edge_height, width(), edge_height);
  }
}

void NormalBrowserFrameView::PaintToolbarBackground(gfx::Canvas* canvas) {
  if (!browser_view_->IsToolbarVisible())
    return;

  gfx::Rect toolbar_bounds(browser_view_->GetToolbarBounds());
  if (toolbar_bounds.IsEmpty())
    return;

  ThemeProvider* tp = GetThemeProvider();
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToView(frame_->GetWindow()->GetClientView(),
                           this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);

  // Gross hack: We split the toolbar images into two pieces, since sometimes
  // (popup mode) the toolbar isn't tall enough to show the whole image.  The
  // split happens between the top shadow section and the bottom gradient
  // section so that we never break the gradient.
  int split_point = kFrameShadowThickness * 2;
  int bottom_y = toolbar_bounds.y() + split_point;
  SkBitmap* toolbar_left =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER);
  int bottom_edge_height =
      std::min(toolbar_left->height(), toolbar_bounds.height()) - split_point;

  SkColor theme_toolbar_color =
      tp->GetColor(BrowserThemeProvider::COLOR_TOOLBAR);
  canvas->FillRectInt(theme_toolbar_color, toolbar_bounds.x(), bottom_y,
                      toolbar_bounds.width(), bottom_edge_height);

  int strip_height = browser_view_->GetTabStripHeight();
  SkBitmap* theme_toolbar = tp->GetBitmapNamed(IDR_THEME_TOOLBAR);

  canvas->TileImageInt(*theme_toolbar,
      toolbar_bounds.x() - kClientEdgeThickness,
      strip_height - kFrameShadowThickness,
      toolbar_bounds.x() - kClientEdgeThickness, bottom_y,
      toolbar_bounds.width() + (2 * kClientEdgeThickness),
      theme_toolbar->height());

  canvas->DrawBitmapInt(*toolbar_left, 0, 0, toolbar_left->width(), split_point,
      toolbar_bounds.x() - toolbar_left->width(), toolbar_bounds.y(),
      toolbar_left->width(), split_point, false);
  canvas->DrawBitmapInt(*toolbar_left, 0,
      toolbar_left->height() - bottom_edge_height, toolbar_left->width(),
      bottom_edge_height, toolbar_bounds.x() - toolbar_left->width(), bottom_y,
      toolbar_left->width(), bottom_edge_height, false);

  SkBitmap* toolbar_center =
      tp->GetBitmapNamed(IDR_CONTENT_TOP_CENTER);
  canvas->TileImageInt(*toolbar_center, 0, 0, toolbar_bounds.x(),
      toolbar_bounds.y(), toolbar_bounds.width(), split_point);

  SkBitmap* toolbar_right = tp->GetBitmapNamed(IDR_CONTENT_TOP_RIGHT_CORNER);
  canvas->DrawBitmapInt(*toolbar_right, 0, 0, toolbar_right->width(),
      split_point, toolbar_bounds.right(), toolbar_bounds.y(),
      toolbar_right->width(), split_point, false);
  canvas->DrawBitmapInt(*toolbar_right, 0,
      toolbar_right->height() - bottom_edge_height, toolbar_right->width(),
      bottom_edge_height, toolbar_bounds.right(), bottom_y,
      toolbar_right->width(), bottom_edge_height, false);

  // Draw the content/toolbar separator.
  canvas->DrawLineInt(ResourceBundle::toolbar_separator_color,
      toolbar_bounds.x(), toolbar_bounds.bottom() - 1,
      toolbar_bounds.right() - 1, toolbar_bounds.bottom() - 1);
}

}  // namespace chromeos
