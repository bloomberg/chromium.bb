// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/glass_browser_frame_view.h"

#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/side_tab_strip.h"
#include "chrome/browser/views/tabs/tab.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "gfx/canvas_skia.h"
#include "gfx/icon_util.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "views/window/client_view.h"
#include "views/window/window_resources.h"

HICON GlassBrowserFrameView::throbber_icons_[
    GlassBrowserFrameView::kThrobberIconCount];

namespace {
// There are 3 px of client edge drawn inside the outer frame borders.
const int kNonClientBorderThickness = 3;
// Vertical tabs have 4 px border.
const int kNonClientVerticalTabStripBorderThickness = 4;
// Besides the frame border, there's another 11 px of empty space atop the
// window in restored mode, to use to drag the window around.
const int kNonClientRestoredExtraThickness = 11;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of the top and bottom edges triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// In maximized mode, the OTR avatar starts 2 px below the top of the screen, so
// that it doesn't extend into the "3D edge" portion of the titlebar.
const int kOTRMaximizedTopSpacing = 2;
// The OTR avatar ends 2 px above the bottom of the tabstrip (which, given the
// way the tabstrip draws its bottom edge, will appear like a 1 px gap to the
// user).
const int kOTRBottomSpacing = 2;
// There are 2 px on each side of the OTR avatar (between the frame border and
// it on the left, and between it and the tabstrip on the right).
const int kOTRSideSpacing = 2;
// The content left/right images have a shadow built into them.
const int kContentEdgeShadowThickness = 2;
// The top 1 px of the tabstrip is shadow; in maximized mode we push this off
// the top of the screen so the tabs appear flush against the screen edge.
const int kTabstripTopShadowThickness = 1;
// In restored mode, the New Tab button isn't at the same height as the caption
// buttons, but the space will look cluttered if it actually slides under them,
// so we stop it when the gap between the two is down to 5 px.
const int kNewTabCaptionRestoredSpacing = 5;
// In maximized mode, where the New Tab button and the caption buttons are at
// similar vertical coordinates, we need to reserve a larger, 16 px gap to avoid
// looking too cluttered.
const int kNewTabCaptionMaximizedSpacing = 16;
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, public:

GlassBrowserFrameView::GlassBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : BrowserNonClientFrameView(),
      frame_(frame),
      browser_view_(browser_view),
      throbber_running_(false),
      throbber_frame_(0) {
  if (frame_->GetWindow()->GetDelegate()->ShouldShowWindowIcon())
    InitThrobberIcons();
}

GlassBrowserFrameView::~GlassBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForTabStrip(
    BaseTabStrip* tabstrip) const {
  if (browser_view_->UseVerticalTabs()) {
    gfx::Size ps = tabstrip->GetPreferredSize();
    return gfx::Rect(NonClientBorderThickness(), NonClientTopBorderHeight(),
                     ps.width(), browser_view_->height());
  }
  int minimize_button_offset = frame_->GetMinimizeButtonOffset();
  int tabstrip_x = browser_view_->ShouldShowOffTheRecordAvatar() ?
      (otr_avatar_bounds_.right() + kOTRSideSpacing) :
      NonClientBorderThickness();
  // In RTL languages, we have moved an avatar icon left by the size of window
  // controls to prevent it from being rendered over them. So, we use its x
  // position to move this tab strip left when maximized. Also, we can render
  // a tab strip until the left end of this window without considering the size
  // of window controls in RTL languages.
  if (base::i18n::IsRTL()) {
    if (!browser_view_->ShouldShowOffTheRecordAvatar() &&
        frame_->GetWindow()->IsMaximized())
      tabstrip_x += otr_avatar_bounds_.x();
    minimize_button_offset = width();
  }
  int tabstrip_width = minimize_button_offset - tabstrip_x -
      (frame_->GetWindow()->IsMaximized() ?
          kNewTabCaptionMaximizedSpacing : kNewTabCaptionRestoredSpacing);
  return gfx::Rect(tabstrip_x, NonClientTopBorderHeight(),
                   std::max(0, tabstrip_width),
                   tabstrip->GetPreferredHeight());
}

void GlassBrowserFrameView::UpdateThrobber(bool running) {
  if (throbber_running_) {
    if (running) {
      DisplayNextThrobberFrame();
    } else {
      StopThrobber();
    }
  } else if (running) {
    StartThrobber();
  }
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

bool GlassBrowserFrameView::AlwaysUseNativeFrame() const {
  return frame_->AlwaysUseNativeFrame();
}

gfx::Rect GlassBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  HWND hwnd = frame_->GetWindow()->GetNativeWindow();
  if (!browser_view_->IsTabStripVisible() && hwnd) {
    // If we don't have a tabstrip, we're either a popup or an app window, in
    // which case we have a standard size non-client area and can just use
    // AdjustWindowRectEx to obtain it. We check for a non-NULL window handle in
    // case this gets called before the window is actually created.
    RECT rect = client_bounds.ToRECT();
    AdjustWindowRectEx(&rect, GetWindowLong(hwnd, GWL_STYLE), FALSE,
                       GetWindowLong(hwnd, GWL_EXSTYLE));
    return gfx::Rect(rect);
  }

  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

int GlassBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  // If the browser isn't in normal mode, we haven't customized the frame, so
  // Windows can figure this out.  If the point isn't within our bounds, then
  // it's in the native portion of the frame, so again Windows can figure it
  // out.
  if (!browser_view_->IsBrowserTypeNormal() || !bounds().Contains(point))
    return HTNOWHERE;

  int frame_component =
      frame_->GetWindow()->GetClientView()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  We still have to check the tabstrip
  // first so that clicks in a tab don't get treated as sysmenu clicks.
  int nonclient_border_thickness = NonClientBorderThickness();
  if (gfx::Rect(nonclient_border_thickness, GetSystemMetrics(SM_CXSIZEFRAME),
                GetSystemMetrics(SM_CXSMICON),
                GetSystemMetrics(SM_CYSMICON)).Contains(point))
    return (frame_component == HTCLIENT) ? HTCLIENT : HTSYSMENU;

  if (frame_component != HTNOWHERE)
    return frame_component;

  int frame_border_thickness = FrameBorderThickness();
  int window_component = GetHTComponentForFrame(point, frame_border_thickness,
      nonclient_border_thickness, frame_border_thickness,
      kResizeAreaCornerSize - frame_border_thickness,
      frame_->GetWindow()->GetDelegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::View overrides:

void GlassBrowserFrameView::Paint(gfx::Canvas* canvas) {
  if (!browser_view_->IsTabStripVisible())
    return;  // Nothing is visible, so don't bother to paint.

  PaintToolbarBackground(canvas);
  PaintOTRAvatar(canvas);
  if (!frame_->GetWindow()->IsMaximized())
    PaintRestoredClientEdge(canvas);
}

void GlassBrowserFrameView::Layout() {
  LayoutOTRAvatar();
  LayoutClientView();
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, private:

int GlassBrowserFrameView::FrameBorderThickness() const {
  views::Window* window = frame_->GetWindow();
  return (window->IsMaximized() || window->IsFullscreen()) ?
      0 : GetSystemMetrics(SM_CXSIZEFRAME);
}

int GlassBrowserFrameView::NonClientBorderThickness() const {
  views::Window* window = frame_->GetWindow();
  if (window->IsMaximized() || window->IsFullscreen())
    return 0;

  return browser_view_->UseVerticalTabs() ?
      kNonClientVerticalTabStripBorderThickness :
      kNonClientBorderThickness;
}

int GlassBrowserFrameView::NonClientTopBorderHeight() const {
  if (frame_->GetWindow()->IsFullscreen())
    return 0;
  if (browser_view_->UseVerticalTabs())
    return static_cast<BrowserFrameWin*>(frame_)->GetTitleBarHeight();
  // We'd like to use FrameBorderThickness() here, but the maximized Aero glass
  // frame has a 0 frame border around most edges and a CXSIZEFRAME-thick border
  // at the top (see AeroGlassFrame::OnGetMinMaxInfo()).
  return GetSystemMetrics(SM_CXSIZEFRAME) + (browser_view_->IsMaximized() ?
      -kTabstripTopShadowThickness : kNonClientRestoredExtraThickness);
}

void GlassBrowserFrameView::PaintToolbarBackground(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();

  gfx::Rect toolbar_bounds(browser_view_->GetToolbarBounds());
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToView(frame_->GetWindow()->GetClientView(),
                           this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);

  SkBitmap* theme_toolbar = tp->GetBitmapNamed(IDR_THEME_TOOLBAR);
  SkBitmap* toolbar_left = tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER);
  SkBitmap* toolbar_center = tp->GetBitmapNamed(IDR_CONTENT_TOP_CENTER);

  if (browser_view_->UseVerticalTabs()) {
    gfx::Rect tabstrip_bounds(browser_view_->tabstrip()->bounds());
    gfx::Point tabstrip_origin(tabstrip_bounds.origin());
    View::ConvertPointToView(frame_->GetWindow()->GetClientView(),
                             this, &tabstrip_origin);
    tabstrip_bounds.set_origin(tabstrip_origin);

    int x = tabstrip_bounds.x();
    int y = tabstrip_bounds.y();
    int w = toolbar_bounds.right() - x;

    int src_y = Tab::GetMinimumUnselectedSize().height();
    canvas->TileImageInt(*theme_toolbar, 0, src_y,
                         MirroredLeftPointForRect(toolbar_bounds), y,
                         toolbar_bounds.width(), theme_toolbar->height());

    // Draw left edge. We explicitly set a clip as the image is bigger than just
    // the corner.
    canvas->Save();
    canvas->ClipRectInt(x - kNonClientBorderThickness,
                        y - kNonClientBorderThickness,
                        kNonClientBorderThickness,
                        kNonClientBorderThickness);
    canvas->DrawBitmapInt(*toolbar_left, x - kNonClientBorderThickness,
                          y - kNonClientBorderThickness);
    canvas->Restore();

    // Draw center edge. We need to draw a while line above the toolbar for the
    // image to overlay nicely.
    canvas->FillRectInt(SK_ColorWHITE, x, y - 1, w, 1);
    canvas->TileImageInt(*toolbar_center, x, y - kNonClientBorderThickness, w,
                         toolbar_center->height());
    // Right edge. Again, we have to clip because of image size.
    canvas->Save();
    canvas->ClipRectInt(x + w - kNonClientBorderThickness,
                        y - kNonClientBorderThickness,
                        kNonClientBorderThickness,
                        kNonClientBorderThickness);
    canvas->DrawBitmapInt(*tp->GetBitmapNamed(IDR_CONTENT_TOP_RIGHT_CORNER),
                          x + w, y);
    canvas->Restore();
  } else {
    // Draw the toolbar background, setting src_y of the paint to the tab
    // strip height as the toolbar background begins at the top of the tabs.
    int src_y = browser_view_->GetTabStripHeight() - 1;
    canvas->TileImageInt(*theme_toolbar, 0, src_y,
                         toolbar_bounds.x() - 1, toolbar_bounds.y() + 2,
                         toolbar_bounds.width() + 2, theme_toolbar->height());
    // Draw rounded corners for the tab.
    SkBitmap* toolbar_left_mask =
        tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER_MASK);
    SkBitmap* toolbar_right_mask =
        tp->GetBitmapNamed(IDR_CONTENT_TOP_RIGHT_CORNER_MASK);

    // We mask out the corners by using the DestinationIn transfer mode,
    // which keeps the RGB pixels from the destination and the alpha from
    // the source.
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);

    // Mask out the top left corner.
    int left_x = toolbar_bounds.x() - kContentEdgeShadowThickness -
        kClientEdgeThickness;
    canvas->DrawBitmapInt(*toolbar_left_mask,
                          left_x, toolbar_bounds.y(), paint);

    // Mask out the top right corner.
    int right_x = toolbar_bounds.right() - toolbar_right_mask->width() +
        kContentEdgeShadowThickness + kClientEdgeThickness;
    canvas->DrawBitmapInt(*toolbar_right_mask,
                          right_x, toolbar_bounds.y(),
                          paint);

    // Draw left edge.
    canvas->DrawBitmapInt(*toolbar_left, left_x, toolbar_bounds.y());

    // Draw center edge.
    canvas->TileImageInt(*toolbar_center, left_x + toolbar_left->width(),
                         toolbar_bounds.y(),
                         right_x - (left_x + toolbar_left->width()),
                         toolbar_center->height());

    // Right edge.
    canvas->DrawBitmapInt(*tp->GetBitmapNamed(IDR_CONTENT_TOP_RIGHT_CORNER),
                          right_x, toolbar_bounds.y());
  }

  // Draw the content/toolbar separator.
  canvas->DrawLineInt(ResourceBundle::toolbar_separator_color,
                      toolbar_bounds.x(), toolbar_bounds.bottom() - 1,
                      toolbar_bounds.right() - 1, toolbar_bounds.bottom() - 1);
}

void GlassBrowserFrameView::PaintOTRAvatar(gfx::Canvas* canvas) {
  if (!browser_view_->ShouldShowOffTheRecordAvatar())
    return;

  SkBitmap otr_avatar_icon = browser_view_->GetOTRAvatarIcon();
  int src_x = 0;
  int src_y = (otr_avatar_icon.height() - otr_avatar_bounds_.height()) / 2;
  int dst_x = MirroredLeftPointForRect(otr_avatar_bounds_);
  int dst_y = otr_avatar_bounds_.y();
  int w = otr_avatar_bounds_.width();
  int h = otr_avatar_bounds_.height();
  if (browser_view_->UseVerticalTabs()) {
    // Only a portion of the otr icon is visible for vertical tabs. Clip it
    // so that it doesn't overlap shadows.
    gfx::Point tabstrip_origin(browser_view_->tabstrip()->bounds().origin());
    View::ConvertPointToView(frame_->GetWindow()->GetClientView(), this,
                             &tabstrip_origin);
    canvas->Save();
    canvas->ClipRectInt(dst_x, 2, w, tabstrip_origin.y() - 4);
    canvas->DrawBitmapInt(otr_avatar_icon, src_x, src_y, w, h, dst_x, dst_y,
                          w, h, false);
    canvas->Restore();
  } else {
    canvas->DrawBitmapInt(otr_avatar_icon, src_x, src_y, w, h, dst_x, dst_y,
                          w, h, false);
  }
}

void GlassBrowserFrameView::PaintRestoredClientEdge(gfx::Canvas* canvas) {
  ThemeProvider* tp = GetThemeProvider();

  gfx::Rect client_area_bounds = CalculateClientAreaBounds(width(), height());

  // The client edges start below the toolbar upper corner images regardless
  // of how tall the toolbar itself is.
  int client_area_top = browser_view_->UseVerticalTabs() ?
      client_area_bounds.y() :
      frame_->GetWindow()->GetClientView()->y() +
      browser_view_->GetToolbarBounds().y() +
      tp->GetBitmapNamed(IDR_CONTENT_TOP_LEFT_CORNER)->height();

  int client_area_bottom =
      std::max(client_area_top, height() - NonClientBorderThickness());
  int client_area_height = client_area_bottom - client_area_top;
  SkBitmap* right = tp->GetBitmapNamed(IDR_CONTENT_RIGHT_SIDE);
  canvas->TileImageInt(*right, client_area_bounds.right(), client_area_top,
                       right->width(), client_area_height);

  // Draw the toolbar color so that the one pixel areas down the sides
  // show the right color even if not covered by the toolbar image.
  SkColor toolbar_color = tp->GetColor(BrowserThemeProvider::COLOR_TOOLBAR);
  canvas->DrawLineInt(toolbar_color,
      client_area_bounds.x() - kClientEdgeThickness,
      client_area_top,
      client_area_bounds.x() - kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness);
  canvas->DrawLineInt(toolbar_color,
      client_area_bounds.x() - kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness,
      client_area_bounds.right() + kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness);
  canvas->DrawLineInt(toolbar_color,
      client_area_bounds.right() - 1 + kClientEdgeThickness,
      client_area_bottom - 1 + kClientEdgeThickness,
      client_area_bounds.right() - 1 + kClientEdgeThickness,
      client_area_top);

  canvas->DrawBitmapInt(
      *tp->GetBitmapNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER),
      client_area_bounds.right(), client_area_bottom);

  SkBitmap* bottom = tp->GetBitmapNamed(IDR_CONTENT_BOTTOM_CENTER);
  canvas->TileImageInt(*bottom, client_area_bounds.x(),
      client_area_bottom, client_area_bounds.width(),
      bottom->height());

  SkBitmap* bottom_left =
      tp->GetBitmapNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER);
  canvas->DrawBitmapInt(*bottom_left,
      client_area_bounds.x() - bottom_left->width(), client_area_bottom);

  SkBitmap* left = tp->GetBitmapNamed(IDR_CONTENT_LEFT_SIDE);
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
      client_area_top, left->width(), client_area_height);
}

void GlassBrowserFrameView::LayoutOTRAvatar() {
  int otr_x = NonClientBorderThickness() + kOTRSideSpacing;
  // Move this avatar icon by the size of window controls to prevent it from
  // being rendered over them in RTL languages. This code also needs to adjust
  // the width of a tab strip to avoid decreasing this size twice. (See the
  // comment in GetBoundsForTabStrip().)
  if (base::i18n::IsRTL())
    otr_x += width() - frame_->GetMinimizeButtonOffset();
  SkBitmap otr_avatar_icon = browser_view_->GetOTRAvatarIcon();
  int otr_height = browser_view_->IsTabStripVisible() ?
      otr_avatar_icon.height() : 0;
  int otr_y = 0;
  if (browser_view_->UseVerticalTabs()) {
    otr_y = NonClientTopBorderHeight() - otr_avatar_icon.height();
  } else if (browser_view_->IsTabStripVisible()) {
    int top_height = NonClientTopBorderHeight();
    int tabstrip_height =
        browser_view_->GetTabStripHeight() - kOTRBottomSpacing;
    otr_height = frame_->GetWindow()->IsMaximized() ?
        (tabstrip_height - kOTRMaximizedTopSpacing) : otr_avatar_icon.height();
    otr_y = top_height + tabstrip_height - otr_height;
  }
  otr_avatar_bounds_.SetRect(otr_x, otr_y, otr_avatar_icon.width(), otr_height);
}

void GlassBrowserFrameView::LayoutClientView() {
  client_view_bounds_ = CalculateClientAreaBounds(width(), height());
}

gfx::Rect GlassBrowserFrameView::CalculateClientAreaBounds(int width,
                                                           int height) const {
  if (!browser_view_->IsTabStripVisible())
    return gfx::Rect(0, 0, this->width(), this->height());

  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}

void GlassBrowserFrameView::StartThrobber() {
  if (!throbber_running_) {
    throbber_running_ = true;
    throbber_frame_ = 0;
    InitThrobberIcons();
    SendMessage(frame_->GetWindow()->GetNativeWindow(), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
  }
}

void GlassBrowserFrameView::StopThrobber() {
  if (throbber_running_) {
    throbber_running_ = false;

    HICON frame_icon = NULL;

    // Check if hosted BrowserView has a window icon to use.
    if (browser_view_->ShouldShowWindowIcon()) {
      SkBitmap icon = browser_view_->GetWindowIcon();
      if (!icon.isNull())
        frame_icon = IconUtil::CreateHICONFromSkBitmap(icon);
    }

    // Fallback to class icon.
    if (!frame_icon) {
      frame_icon = reinterpret_cast<HICON>(GetClassLongPtr(
          frame_->GetWindow()->GetNativeWindow(), GCLP_HICONSM));
    }

    // This will reset the small icon which we set in the throbber code.
    // WM_SETICON with NULL icon restores the icon for title bar but not
    // for taskbar. See http://crbug.com/29996
    SendMessage(frame_->GetWindow()->GetNativeWindow(), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(frame_icon));
  }
}

void GlassBrowserFrameView::DisplayNextThrobberFrame() {
  throbber_frame_ = (throbber_frame_ + 1) % kThrobberIconCount;
  SendMessage(frame_->GetWindow()->GetNativeWindow(), WM_SETICON,
              static_cast<WPARAM>(ICON_SMALL),
              reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
}

// static
void GlassBrowserFrameView::InitThrobberIcons() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle &rb = ResourceBundle::GetSharedInstance();
    for (int i = 0; i < kThrobberIconCount; ++i) {
      throbber_icons_[i] = rb.LoadThemeIcon(IDI_THROBBER_01 + i);
      DCHECK(throbber_icons_[i]);
    }
    initialized = true;
  }
}
