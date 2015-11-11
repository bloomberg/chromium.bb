// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/layout_constants.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/profiles/new_avatar_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle_win.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/win/dpi.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/views/window/client_view.h"

HICON GlassBrowserFrameView::throbber_icons_[
    GlassBrowserFrameView::kThrobberIconCount];

namespace {
// Size of client edge drawn inside the outer frame borders.
const int kNonClientBorderThicknessPreWin10 = 3;
const int kNonClientBorderThicknessWin10 = 1;
// Besides the frame border, there's empty space atop the window in restored
// mode, to use to drag the window around.
const int kNonClientRestoredExtraThickness = 11;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of the top and bottom edges triggers diagonal resizing.
const int kResizeCornerWidth = 16;
// How far the new avatar button is from the left of the minimize button.
const int kNewAvatarButtonOffset = 5;
// The content left/right images have a shadow built into them.
const int kContentEdgeShadowThickness = 2;
// In restored mode, the New Tab button isn't at the same height as the caption
// buttons, but the space will look cluttered if it actually slides under them,
// so we stop it when the gap between the two is down to 5 px.
const int kNewTabCaptionRestoredSpacing = 5;
// In maximized mode, where the New Tab button and the caption buttons are at
// similar vertical coordinates, we need to reserve a larger, 16 px gap to avoid
// looking too cluttered.
const int kNewTabCaptionMaximizedSpacing = 16;

// Converts the |image| to a Windows icon and returns the corresponding HICON
// handle. |image| is resized to desired |width| and |height| if needed.
HICON CreateHICONFromSkBitmapSizedTo(const gfx::ImageSkia& image,
                                     int width,
                                     int height) {
  if (width == image.width() && height == image.height())
    return IconUtil::CreateHICONFromSkBitmap(*image.bitmap());
  return IconUtil::CreateHICONFromSkBitmap(skia::ImageOperations::Resize(
      *image.bitmap(), skia::ImageOperations::RESIZE_BEST, width, height));
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, public:

GlassBrowserFrameView::GlassBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      throbber_running_(false),
      throbber_frame_(0) {
  if (browser_view->ShouldShowWindowIcon())
    InitThrobberIcons();

  UpdateAvatar();
}

GlassBrowserFrameView::~GlassBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  // In maximized RTL windows, don't let the tabstrip overlap the caption area,
  // or the alpha-blending it does will make things like the new avatar button
  // look glitchy.
  const int offset =
    (ui::MaterialDesignController::IsModeMaterial() || !base::i18n::IsRTL() ||
     !frame()->IsMaximized()) ?
        GetLayoutInsets(AVATAR_ICON).right() : 0;
  const int x = incognito_bounds_.right() + offset;
  int end_x = width() - NonClientBorderThickness(false);
  if (!base::i18n::IsRTL()) {
    end_x = std::min(frame()->GetMinimizeButtonOffset(), end_x) -
        (frame()->IsMaximized() ?
            kNewTabCaptionMaximizedSpacing : kNewTabCaptionRestoredSpacing);

    // The new avatar button is optionally displayed to the left of the
    // minimize button.
    if (new_avatar_button()) {
      const int old_end_x = end_x;
      end_x -= new_avatar_button()->width() + kNewAvatarButtonOffset;

      // In non-maximized mode, allow the new tab button to slide completely
      // under the avatar button.
      if (!frame()->IsMaximized()) {
        end_x = std::min(end_x + GetLayoutConstant(NEW_TAB_BUTTON_WIDTH) +
                             kNewTabCaptionRestoredSpacing,
                         old_end_x);
      }
    }
  }
  return gfx::Rect(x, NonClientTopBorderHeight(false), std::max(0, end_x - x),
                   tabstrip->GetPreferredSize().height());
}

int GlassBrowserFrameView::GetTopInset(bool restored) const {
  return GetClientAreaInsets(restored).top();
}

int GlassBrowserFrameView::GetThemeBackgroundXInset() const {
  return 0;
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

gfx::Size GlassBrowserFrameView::GetMinimumSize() const {
  gfx::Size min_size(browser_view()->GetMinimumSize());

  // Account for the client area insets.
  gfx::Insets insets = GetClientAreaInsets(false);
  min_size.Enlarge(insets.width(), insets.height());
  // Client area insets do not include the shadow thickness.
  min_size.Enlarge(2 * kContentEdgeShadowThickness, 0);

  // Ensure that the minimum width is enough to hold a tab strip with minimum
  // width at its usual insets.
  if (browser_view()->IsTabStripVisible()) {
    TabStrip* tabstrip = browser_view()->tabstrip();
    int min_tabstrip_width = tabstrip->GetMinimumSize().width();
    int min_tabstrip_area_width =
        width() - GetBoundsForTabStrip(tabstrip).width() + min_tabstrip_width;
    min_size.set_width(std::max(min_tabstrip_area_width, min_size.width()));
  }

  return min_size;
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect GlassBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  HWND hwnd = views::HWNDForWidget(frame());
  if (!browser_view()->IsTabStripVisible() && hwnd) {
    // If we don't have a tabstrip, we're either a popup or an app window, in
    // which case we have a standard size non-client area and can just use
    // AdjustWindowRectEx to obtain it. We check for a non-null window handle in
    // case this gets called before the window is actually created.
    RECT rect = client_bounds.ToRECT();
    AdjustWindowRectEx(&rect, GetWindowLong(hwnd, GWL_STYLE), FALSE,
                       GetWindowLong(hwnd, GWL_EXSTYLE));
    return gfx::Rect(rect);
  }

  gfx::Insets insets = GetClientAreaInsets(false);
  return gfx::Rect(std::max(0, client_bounds.x() - insets.left()),
                   std::max(0, client_bounds.y() - insets.top()),
                   client_bounds.width() + insets.width(),
                   client_bounds.height() + insets.height());
}

int GlassBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  // If the browser isn't in normal mode, we haven't customized the frame, so
  // Windows can figure this out.  If the point isn't within our bounds, then
  // it's in the native portion of the frame, so again Windows can figure it
  // out.
  if (!browser_view()->IsBrowserTypeNormal() || !bounds().Contains(point))
    return HTNOWHERE;

  // See if the point is within the incognito icon or the new avatar menu.
  if ((avatar_button() &&
       avatar_button()->GetMirroredBounds().Contains(point)) ||
      (new_avatar_button() &&
       new_avatar_button()->GetMirroredBounds().Contains(point)))
   return HTCLIENT;

  int frame_component = frame()->client_view()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  We still have to check the tabstrip
  // first so that clicks in a tab don't get treated as sysmenu clicks.
  int nonclient_border_thickness = NonClientBorderThickness(false);
  if (gfx::Rect(nonclient_border_thickness,
                gfx::win::GetSystemMetricsInDIP(SM_CYSIZEFRAME),
                gfx::win::GetSystemMetricsInDIP(SM_CXSMICON),
                gfx::win::GetSystemMetricsInDIP(SM_CYSMICON)).Contains(point))
    return (frame_component == HTCLIENT) ? HTCLIENT : HTSYSMENU;

  if (frame_component != HTNOWHERE)
    return frame_component;

  int frame_top_border_height = FrameTopBorderHeight(false);
  // We want the resize corner behavior to apply to the kResizeCornerWidth
  // pixels at each end of the top and bottom edges.  Because |point|'s x
  // coordinate is based on the DWM-inset portion of the window (so, it's 0 at
  // the first pixel inside the left DWM margin), we need to subtract the DWM
  // margin thickness, which we calculate as the total frame border thickness
  // minus the nonclient border thickness.
  const int dwm_margin = FrameBorderThickness() - nonclient_border_thickness;
  int window_component = GetHTComponentForFrame(point, frame_top_border_height,
      nonclient_border_thickness, frame_top_border_height,
      kResizeCornerWidth - dwm_margin, frame()->widget_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::View overrides:

void GlassBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
  if (browser_view()->IsToolbarVisible() &&
      browser_view()->toolbar()->ShouldPaintBackground())
    PaintToolbarBackground(canvas);
  if (!frame()->IsMaximized())
    PaintRestoredClientEdge(canvas);
}

void GlassBrowserFrameView::Layout() {
  if (browser_view()->IsRegularOrGuestSession())
    LayoutNewStyleAvatar();
  LayoutIncognitoIcon();
  LayoutClientView();
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, protected:

// views::ButtonListener:
void GlassBrowserFrameView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == new_avatar_button()) {
    BrowserWindow::AvatarBubbleMode mode =
        BrowserWindow::AVATAR_BUBBLE_MODE_DEFAULT;
    if ((event.IsMouseEvent() &&
         static_cast<const ui::MouseEvent&>(event).IsRightMouseButton()) ||
        (event.type() == ui::ET_GESTURE_LONG_PRESS)) {
      mode = BrowserWindow::AVATAR_BUBBLE_MODE_FAST_USER_SWITCH;
    }
    browser_view()->ShowAvatarBubbleFromAvatarButton(
        mode,
        signin::ManageAccountsParams());
  }
}

// BrowserNonClientFrameView:
void GlassBrowserFrameView::UpdateNewAvatarButtonImpl() {
  UpdateNewAvatarButton(this, NewAvatarButton::NATIVE_BUTTON);
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, private:

// views::NonClientFrameView:
bool GlassBrowserFrameView::DoesIntersectRect(const views::View* target,
                                              const gfx::Rect& rect) const {
  CHECK_EQ(target, this);
  bool hit_incognito_icon = avatar_button() &&
      avatar_button()->GetMirroredBounds().Intersects(rect);
  bool hit_new_avatar_button = new_avatar_button() &&
      new_avatar_button()->GetMirroredBounds().Intersects(rect);
  return hit_incognito_icon || hit_new_avatar_button ||
         !frame()->client_view()->bounds().Intersects(rect);
}

int GlassBrowserFrameView::FrameBorderThickness() const {
  return (frame()->IsMaximized() || frame()->IsFullscreen()) ?
      0 : gfx::win::GetSystemMetricsInDIP(SM_CXSIZEFRAME);
}

int GlassBrowserFrameView::FrameTopBorderHeight(bool restored) const {
  // We'd like to use FrameBorderThickness() here, but the maximized Aero glass
  // frame has a 0 frame border around most edges and a CYSIZEFRAME-thick border
  // at the top (see AeroGlassFrame::OnGetMinMaxInfo()).
  return (frame()->IsFullscreen() && !restored) ?
      0 : gfx::win::GetSystemMetricsInDIP(SM_CYSIZEFRAME);
}

int GlassBrowserFrameView::NonClientBorderThickness(bool restored) const {
  if ((frame()->IsMaximized() || frame()->IsFullscreen()) && !restored)
    return 0;

  return (base::win::GetVersion() <= base::win::VERSION_WIN8_1)
             ? kNonClientBorderThicknessPreWin10
             : kNonClientBorderThicknessWin10;
}

int GlassBrowserFrameView::NonClientTopBorderHeight(bool restored) const {
  if (frame()->IsFullscreen() && !restored)
    return 0;

  const int top = FrameTopBorderHeight(restored);
  // The tab top inset is equal to the height of any shadow region above the
  // tabs, plus a 1 px top stroke.  In maximized mode, we want to push the
  // shadow region off the top of the screen but leave the top stroke.
  // Annoyingly, the pre-MD layout uses different heights for the hit-test
  // exclusion region (which we want here, since we're trying to size the border
  // so that the region above the tab's hit-test zone matches) versus the shadow
  // thickness.
  const int exclusion = GetLayoutConstant(TAB_TOP_EXCLUSION_HEIGHT);
  return (frame()->IsMaximized() && !restored) ?
      (top - GetLayoutInsets(TAB).top() + 1) :
      (top + kNonClientRestoredExtraThickness - exclusion);
}

void GlassBrowserFrameView::PaintToolbarBackground(gfx::Canvas* canvas) {
  ui::ThemeProvider* tp = GetThemeProvider();

  gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToTarget(browser_view(), this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);
  int x = toolbar_bounds.x();
  int w = toolbar_bounds.width();

  // Toolbar background.
  int y = toolbar_bounds.y();
  // Tile the toolbar image starting at the frame edge on the left and where
  // the tabstrip is on the top.
  gfx::ImageSkia* theme_toolbar = tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR);
  int dest_y = y;
  // In the pre-MD world, the toolbar top edge is drawn using the
  // IDR_CONTENT_TOP_XXX images, which overlay the toolbar.  The top 2 px of
  // these images is the actual top edge, and is partly transparent, so the
  // toolbar background shouldn't be drawn over it.
  const int kPreMDToolbarTopEdgeExclusion = 2;
  if (browser_view()->IsTabStripVisible())
    dest_y += kPreMDToolbarTopEdgeExclusion;
  canvas->TileImageInt(
      *theme_toolbar, x + GetThemeBackgroundXInset(),
      dest_y - GetTopInset(false) - Tab::GetYInsetForActiveTabBackground(),
      x, dest_y, w, theme_toolbar->height());

  // Toolbar edges.
  if (browser_view()->IsTabStripVisible()) {
    // Pre-Windows 10, we draw toolbar left and right edges and top corners,
    // partly atop the window border.  In Windows 10+, we don't draw our own
    // window border but rather go right to the system border, so we need only
    // draw the toolbar top edge.
    int center_x = x;
    int center_w = w;
    if (base::win::GetVersion() < base::win::VERSION_WIN10) {
      // Mask out the top left corner and draw left corner and edge.
      const int left_x = center_x - kContentEdgeShadowThickness;
      SkPaint paint;
      paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
      canvas->DrawImageInt(
          *tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER_MASK), left_x, y,
          paint);
      gfx::ImageSkia* toolbar_left =
          tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER);
      canvas->DrawImageInt(*toolbar_left, left_x, y);
      center_x = left_x + toolbar_left->width();

      // Mask out the top right corner and draw right corner and edge.
      gfx::ImageSkia* toolbar_right_mask =
          tp->GetImageSkiaNamed(IDR_CONTENT_TOP_RIGHT_CORNER_MASK);
      const int right_x = toolbar_bounds.right() +
          kContentEdgeShadowThickness - toolbar_right_mask->width();
      canvas->DrawImageInt(*toolbar_right_mask, right_x, y, paint);
      canvas->DrawImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_TOP_RIGHT_CORNER),
                           right_x, y);
      center_w = right_x - center_x;
    }

    // Top edge.
    gfx::ImageSkia* toolbar_center =
        tp->GetImageSkiaNamed(IDR_CONTENT_TOP_CENTER);
    canvas->TileImageInt(*toolbar_center, center_x, y, center_w,
                         toolbar_center->height());
  }

  // Toolbar/content separator.
  if (ui::MaterialDesignController::IsModeMaterial()) {
    toolbar_bounds.Inset(kClientEdgeThickness, 0);
    BrowserView::Paint1pxHorizontalLine(
        canvas,
        ThemeProperties::GetDefaultColor(
            ThemeProperties::COLOR_TOOLBAR_SEPARATOR),
        toolbar_bounds);
  } else {
    canvas->FillRect(
        gfx::Rect(x + kClientEdgeThickness,
                  toolbar_bounds.bottom() - kClientEdgeThickness,
                  w - (2 * kClientEdgeThickness), kClientEdgeThickness),
        ThemeProperties::GetDefaultColor(
            ThemeProperties::COLOR_TOOLBAR_SEPARATOR));
  }
}

void GlassBrowserFrameView::PaintRestoredClientEdge(gfx::Canvas* canvas) {
  ui::ThemeProvider* tp = GetThemeProvider();
  gfx::Rect client_area_bounds = CalculateClientAreaBounds(width(), height());

  // The client edges start below the toolbar upper corner images regardless
  // of how tall the toolbar itself is.
  int client_area_top = frame()->client_view()->y() +
      browser_view()->GetToolbarBounds().y() +
      tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER)->height();
  int client_area_bottom =
      std::max(client_area_top, height() - NonClientBorderThickness(false));
  int client_area_height = client_area_bottom - client_area_top;

  // Draw the client edge images.
  gfx::ImageSkia* right = tp->GetImageSkiaNamed(IDR_CONTENT_RIGHT_SIDE);
  canvas->TileImageInt(*right, client_area_bounds.right(), client_area_top,
                       right->width(), client_area_height);
  canvas->DrawImageInt(
      *tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER),
      client_area_bounds.right(), client_area_bottom);
  gfx::ImageSkia* bottom = tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_CENTER);
  canvas->TileImageInt(*bottom, client_area_bounds.x(),
      client_area_bottom, client_area_bounds.width(),
      bottom->height());
  gfx::ImageSkia* bottom_left =
      tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER);
  canvas->DrawImageInt(*bottom_left,
      client_area_bounds.x() - bottom_left->width(), client_area_bottom);
  gfx::ImageSkia* left = tp->GetImageSkiaNamed(IDR_CONTENT_LEFT_SIDE);
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
      client_area_top, left->width(), client_area_height);

  // Draw the toolbar color so that the client edges show the right color even
  // where not covered by the toolbar image.  NOTE: We do this after drawing the
  // images because the images are meant to alpha-blend atop the frame whereas
  // these rects are meant to be fully opaque, without anything overlaid.
  SkColor toolbar_color = tp->GetColor(ThemeProperties::COLOR_TOOLBAR);
  canvas->FillRect(gfx::Rect(client_area_bounds.x() - kClientEdgeThickness,
      client_area_top, kClientEdgeThickness,
      client_area_bottom + kClientEdgeThickness - client_area_top),
      toolbar_color);
  canvas->FillRect(gfx::Rect(client_area_bounds.x(), client_area_bottom,
                             client_area_bounds.width(), kClientEdgeThickness),
                   toolbar_color);
  canvas->FillRect(gfx::Rect(client_area_bounds.right(), client_area_top,
       kClientEdgeThickness,
       client_area_bottom + kClientEdgeThickness - client_area_top),
       toolbar_color);
}

void GlassBrowserFrameView::LayoutNewStyleAvatar() {
  DCHECK(browser_view()->IsRegularOrGuestSession());
  if (!new_avatar_button())
    return;

  gfx::Size label_size = new_avatar_button()->GetPreferredSize();

  int button_x = frame()->GetMinimizeButtonOffset() -
      kNewAvatarButtonOffset - label_size.width();
  if (base::i18n::IsRTL())
    button_x = width() - frame()->GetMinimizeButtonOffset() +
        kNewAvatarButtonOffset;

  // The caption button position and size is confusing.  In maximized mode, the
  // caption buttons are SM_CYMENUSIZE pixels high and are placed
  // FrameTopBorderHeight() pixels from the top of the window; all those top
  // border pixels are offscreen, so this result in caption buttons flush with
  // the top of the screen.  In restored mode, the caption buttons are first
  // placed just below a 2 px border at the top of the window (which is the
  // first two pixels' worth of FrameTopBorderHeight()), then extended upwards
  // one extra pixel to overlap part of this border.
  //
  // To match both of these, we size the button as if it's always the extra one
  // pixel in height, then we place it at the correct position in restored mode,
  // or one pixel above the top of the screen in maximized mode.
  int button_y = frame()->IsMaximized() ? (FrameTopBorderHeight(false) - 1) : 1;
  new_avatar_button()->SetBounds(
      button_x,
      button_y,
      label_size.width(),
      gfx::win::GetSystemMetricsInDIP(SM_CYMENUSIZE) + 1);
}

void GlassBrowserFrameView::LayoutIncognitoIcon() {
  const gfx::Insets insets(GetLayoutInsets(AVATAR_ICON));
  gfx::Size size;
  // During startup it's possible to reach here before the browser view has been
  // added to the view hierarchy.  In this case it won't have a widget and thus
  // can't access the theme provider, which is required to get the incognito
  // icon.  Use an empty size in this case, which will still place the tabstrip
  // at the correct coordinates for a non-incognito window.  We should get
  // another layout call after the browser view has a widget anyway.
  if (browser_view()->GetWidget())
    size = browser_view()->GetOTRAvatarIcon().size();
  int x = NonClientBorderThickness(false);
  // In RTL, the icon needs to start after the caption buttons.
  if (base::i18n::IsRTL()) {
    x = width() - frame()->GetMinimizeButtonOffset() +
        (new_avatar_button() ?
            (new_avatar_button()->width() + kNewAvatarButtonOffset) : 0);
  }
  const int bottom = GetTopInset(false) + browser_view()->GetTabStripHeight() -
      insets.bottom();
  const int y = (ui::MaterialDesignController::IsModeMaterial() ||
                 !frame()->IsMaximized()) ?
      (bottom - size.height()) : FrameTopBorderHeight(false);
  incognito_bounds_.SetRect(x + (avatar_button() ? insets.left() : 0), y,
                            avatar_button() ? size.width() : 0, bottom - y);
  if (avatar_button())
    avatar_button()->SetBoundsRect(incognito_bounds_);
}

void GlassBrowserFrameView::LayoutClientView() {
  client_view_bounds_ = CalculateClientAreaBounds(width(), height());
}

gfx::Insets GlassBrowserFrameView::GetClientAreaInsets(bool restored) const {
  if (!browser_view()->IsTabStripVisible())
    return gfx::Insets();

  const int top_height = NonClientTopBorderHeight(restored);
  const int border_thickness = NonClientBorderThickness(restored);
  return gfx::Insets(top_height,
                     border_thickness,
                     border_thickness,
                     border_thickness);
}

gfx::Rect GlassBrowserFrameView::CalculateClientAreaBounds(int width,
                                                           int height) const {
  gfx::Rect bounds(0, 0, width, height);
  bounds.Inset(GetClientAreaInsets(false));
  return bounds;
}

void GlassBrowserFrameView::StartThrobber() {
  if (!throbber_running_) {
    throbber_running_ = true;
    throbber_frame_ = 0;
    InitThrobberIcons();
    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
  }
}

void GlassBrowserFrameView::StopThrobber() {
  if (throbber_running_) {
    throbber_running_ = false;

    HICON small_icon = nullptr;
    HICON big_icon = nullptr;

    // Check if hosted BrowserView has a window icon to use.
    if (browser_view()->ShouldShowWindowIcon()) {
      gfx::ImageSkia icon = browser_view()->GetWindowIcon();
      if (!icon.isNull()) {
        small_icon = CreateHICONFromSkBitmapSizedTo(
            icon, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
        big_icon = CreateHICONFromSkBitmapSizedTo(
            icon, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
      }
    }

    // Fallback to class icon.
    if (!small_icon) {
      small_icon = reinterpret_cast<HICON>(
          GetClassLongPtr(views::HWNDForWidget(frame()), GCLP_HICONSM));
    }
    if (!big_icon) {
      big_icon = reinterpret_cast<HICON>(
          GetClassLongPtr(views::HWNDForWidget(frame()), GCLP_HICON));
    }

    // This will reset the icon which we set in the throbber code.
    // WM_SETICON with null icon restores the icon for title bar but not
    // for taskbar. See http://crbug.com/29996
    HICON previous_small_icon = reinterpret_cast<HICON>(
        SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                    static_cast<WPARAM>(ICON_SMALL),
                    reinterpret_cast<LPARAM>(small_icon)));

    HICON previous_big_icon = reinterpret_cast<HICON>(
        SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                    static_cast<WPARAM>(ICON_BIG),
                    reinterpret_cast<LPARAM>(big_icon)));

    if (previous_small_icon)
      ::DestroyIcon(previous_small_icon);

    if (previous_big_icon)
      ::DestroyIcon(previous_big_icon);
  }
}

void GlassBrowserFrameView::DisplayNextThrobberFrame() {
  throbber_frame_ = (throbber_frame_ + 1) % kThrobberIconCount;
  SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
              static_cast<WPARAM>(ICON_SMALL),
              reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
}

// static
void GlassBrowserFrameView::InitThrobberIcons() {
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < kThrobberIconCount; ++i) {
      throbber_icons_[i] =
          ui::LoadThemeIconFromResourcesDataDLL(IDI_THROBBER_01 + i);
      DCHECK(throbber_icons_[i]);
    }
    initialized = true;
  }
}
