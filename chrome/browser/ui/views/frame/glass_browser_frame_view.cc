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
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle_win.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_canvas.h"
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
// The content edge images have a shadow built into them.
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
base::win::ScopedHICON CreateHICONFromSkBitmapSizedTo(
    const gfx::ImageSkia& image,
    int width,
    int height) {
  return IconUtil::CreateHICONFromSkBitmap(
      width == image.width() && height == image.height()
          ? *image.bitmap()
          : skia::ImageOperations::Resize(*image.bitmap(),
                                          skia::ImageOperations::RESIZE_BEST,
                                          width, height));
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
        end_x = std::min(end_x + GetLayoutSize(NEW_TAB_BUTTON).width() +
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
  if (IsToolbarVisible())
    PaintToolbarBackground(canvas);
  PaintClientEdge(canvas);
}

void GlassBrowserFrameView::Layout() {
  if (browser_view()->IsRegularOrGuestSession())
    LayoutNewStyleAvatar();
  LayoutIncognitoIcon();
  LayoutClientView();
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, protected:

// BrowserNonClientFrameView:
void GlassBrowserFrameView::UpdateNewAvatarButtonImpl() {
  UpdateNewAvatarButton(AvatarButtonStyle::NATIVE);
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

bool GlassBrowserFrameView::IsToolbarVisible() const {
  return browser_view()->IsToolbarVisible() &&
      !browser_view()->toolbar()->GetPreferredSize().IsEmpty();
}

void GlassBrowserFrameView::PaintToolbarBackground(gfx::Canvas* canvas) {
  gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  if (toolbar_bounds.IsEmpty())
    return;
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  ConvertPointToTarget(browser_view(), this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);
  const int h = toolbar_bounds.height();
  const bool md = ui::MaterialDesignController::IsModeMaterial();
  const ui::ThemeProvider* tp = GetThemeProvider();
  const SkColor separator_color =
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR);

  if (browser_view()->IsTabStripVisible()) {
    gfx::ImageSkia* bg = tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR);
    int x = toolbar_bounds.x();
    const int y = toolbar_bounds.y();
    const int bg_y =
        GetTopInset(false) + Tab::GetYInsetForActiveTabBackground();
    int w = toolbar_bounds.width();

    if (md) {
      // Background.  The top stroke is drawn above the toolbar bounds, so
      // unlike in the non-Material Design code below, we don't need to exclude
      // any region from having the background image drawn over it.
      if (tp->HasCustomImage(IDR_THEME_TOOLBAR)) {
        canvas->TileImageInt(*bg, x + GetThemeBackgroundXInset(), y - bg_y, x,
                             y, w, h);
      } else {
        canvas->FillRect(toolbar_bounds,
                         tp->GetColor(ThemeProperties::COLOR_TOOLBAR));
      }

      // Material Design has no corners to mask out.

      // Top stroke.  For Material Design, the toolbar has no side strokes.
      gfx::Rect separator_rect(x, y, w, 0);
      gfx::ScopedCanvas scoped_canvas(canvas);
      gfx::Rect tabstrip_bounds(
          GetBoundsForTabStrip(browser_view()->tabstrip()));
      tabstrip_bounds.set_x(GetMirroredXForRect(tabstrip_bounds));
      canvas->sk_canvas()->clipRect(gfx::RectToSkRect(tabstrip_bounds),
                                    SkRegion::kDifference_Op);
      separator_rect.set_y(tabstrip_bounds.bottom());
      BrowserView::Paint1pxHorizontalLine(
          canvas, tp->GetColor(ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR),
          separator_rect, true);
    } else {
      // Background.  The top stroke is drawn using the IDR_CONTENT_TOP_XXX
      // images, which overlay the toolbar.  The top 2 px of these images is the
      // actual top stroke + shadow, and is partly transparent, so the toolbar
      // background shouldn't be drawn over it.
      const int split_point = std::min(kContentEdgeShadowThickness, h);
      if (h > split_point) {
        const int split_y = y + split_point;
        canvas->TileImageInt(*bg, x + GetThemeBackgroundXInset(),
                             split_y - bg_y, x, split_y, w, h - split_point);
      }

      // On Windows 10+ where we don't draw our own window border but rather go
      // right to the system border, the toolbar has no corners or side strokes.
      if (base::win::GetVersion() < base::win::VERSION_WIN10) {
        // Mask out the corners.
        gfx::ImageSkia* left =
            tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER);
        const int img_w = left->width();
        x -= kContentEdgeShadowThickness;
        SkPaint paint;
        paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
        canvas->DrawImageInt(
            *tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER_MASK), 0, 0,
            img_w, h, x, y, img_w, h, false, paint);
        const int right_x =
            toolbar_bounds.right() + kContentEdgeShadowThickness - img_w;
        canvas->DrawImageInt(
            *tp->GetImageSkiaNamed(IDR_CONTENT_TOP_RIGHT_CORNER_MASK), 0, 0,
            img_w, h, right_x, y, img_w, h, false, paint);

        // Corner and side strokes.
        canvas->DrawImageInt(*left, 0, 0, img_w, h, x, y, img_w, h, false);
        canvas->DrawImageInt(
            *tp->GetImageSkiaNamed(IDR_CONTENT_TOP_RIGHT_CORNER), 0, 0, img_w,
            h, right_x, y, img_w, h, false);

        x += img_w;
        w = right_x - x;
      }

      // Top stroke.
      canvas->TileImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_TOP_CENTER), x, y,
                           w, split_point);
    }
  }

  // Toolbar/content separator.
  toolbar_bounds.Inset(kClientEdgeThickness, h - kClientEdgeThickness,
                        kClientEdgeThickness, 0);
  if (md) {
    BrowserView::Paint1pxHorizontalLine(canvas, separator_color,
                                        toolbar_bounds, true);
  } else {
    canvas->FillRect(toolbar_bounds, separator_color);
  }
}

void GlassBrowserFrameView::PaintClientEdge(gfx::Canvas* canvas) {
  gfx::Rect client_bounds = CalculateClientAreaBounds();
  int y = client_bounds.y();
  const bool normal_mode = browser_view()->IsTabStripVisible();
  const ui::ThemeProvider* tp = GetThemeProvider();
  const SkColor toolbar_color =
      normal_mode
          ? tp->GetColor(ThemeProperties::COLOR_TOOLBAR)
          : ThemeProperties::GetDefaultColor(ThemeProperties::COLOR_TOOLBAR,
                                             browser_view()->IsOffTheRecord());

  const gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  if (!normal_mode) {
    // The toolbar isn't going to draw a top edge for us, so draw one ourselves.
    if (IsToolbarVisible())
      y += toolbar_bounds.y() + kClientEdgeThickness;
    client_bounds.set_y(y);
    client_bounds.Inset(-kClientEdgeThickness, -kClientEdgeThickness,
                        -kClientEdgeThickness, client_bounds.height());
    canvas->FillRect(client_bounds, toolbar_color);

    // Popup and app windows don't custom-draw any other edges, so we're done.
    return;
  }

  // In maximized mode, the only edge to draw is the top one, so we're done.
  if (frame()->IsMaximized())
    return;

  const int x = client_bounds.x();
  // Pre-Material Design, the client edge images start below the toolbar.  In MD
  // the client edge images start at the top of the toolbar.
  const bool md = ui::MaterialDesignController::IsModeMaterial();
  y += md ? toolbar_bounds.y() : toolbar_bounds.bottom();
  const int w = client_bounds.width();
  const int right = client_bounds.right();
  const int bottom = std::max(y, height() - NonClientBorderThickness(false));
  const int height = bottom - y;

  // Draw the client edge images.  For non-MD, we fill the toolbar color
  // underneath these images so they will lighten/darken it appropriately to
  // create a "3D shaded" effect.  For MD, where we want a flatter appearance,
  // we do the filling afterwards so the user sees the unmodified toolbar color.
  if (!md)
    FillClientEdgeRects(x, y, right, bottom, toolbar_color, canvas);
  gfx::ImageSkia* right_image = tp->GetImageSkiaNamed(IDR_CONTENT_RIGHT_SIDE);
  const int img_w = right_image->width();
  canvas->TileImageInt(*right_image, right, y, img_w, height);
  canvas->DrawImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER),
                       right, bottom);
  gfx::ImageSkia* bottom_image =
      tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_CENTER);
  canvas->TileImageInt(*bottom_image, x, bottom, w, bottom_image->height());
  canvas->DrawImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER),
                       x - img_w, bottom);
  canvas->TileImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_LEFT_SIDE), x - img_w,
                       y, img_w, height);
  if (md)
    FillClientEdgeRects(x, y, right, bottom, toolbar_color, canvas);
}

void GlassBrowserFrameView::FillClientEdgeRects(int x,
                                                int y,
                                                int right,
                                                int bottom,
                                                SkColor color,
                                                gfx::Canvas* canvas) const {
  gfx::Rect side(x - kClientEdgeThickness, y, kClientEdgeThickness,
                 bottom + kClientEdgeThickness - y);
  canvas->FillRect(side, color);
  canvas->FillRect(gfx::Rect(x, bottom, right - x, kClientEdgeThickness),
                   color);
  side.set_x(right);
  canvas->FillRect(side, color);
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
  const bool md = ui::MaterialDesignController::IsModeMaterial();
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
  } else if (!md && !avatar_button() &&
             (base::win::GetVersion() < base::win::VERSION_WIN10)) {
    // In non-MD before Win 10, the toolbar has a rounded corner that we don't
    // want the tabstrip to overlap.
    x += browser_view()->GetToolbarBounds().x() - kContentEdgeShadowThickness +
        GetThemeProvider()->GetImageSkiaNamed(
            IDR_CONTENT_TOP_LEFT_CORNER)->width();
  }
  const int bottom = GetTopInset(false) + browser_view()->GetTabStripHeight() -
      insets.bottom();
  const int y = (md || !frame()->IsMaximized()) ?
      (bottom - size.height()) : FrameTopBorderHeight(false);
  incognito_bounds_.SetRect(x + (avatar_button() ? insets.left() : 0), y,
                            avatar_button() ? size.width() : 0, bottom - y);
  if (avatar_button())
    avatar_button()->SetBoundsRect(incognito_bounds_);
}

void GlassBrowserFrameView::LayoutClientView() {
  client_view_bounds_ = CalculateClientAreaBounds();
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

gfx::Rect GlassBrowserFrameView::CalculateClientAreaBounds() const {
  gfx::Rect bounds(GetLocalBounds());
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

    base::win::ScopedHICON previous_small_icon;
    base::win::ScopedHICON previous_big_icon;
    HICON small_icon = nullptr;
    HICON big_icon = nullptr;

    // Check if hosted BrowserView has a window icon to use.
    if (browser_view()->ShouldShowWindowIcon()) {
      gfx::ImageSkia icon = browser_view()->GetWindowIcon();
      if (!icon.isNull()) {
        // Keep previous icons alive as long as they are referenced by the HWND.
        previous_small_icon = small_window_icon_.Pass();
        previous_big_icon = big_window_icon_.Pass();

        // Take responsibility for eventually destroying the created icons.
        small_window_icon_ =
            CreateHICONFromSkBitmapSizedTo(icon, GetSystemMetrics(SM_CXSMICON),
                                           GetSystemMetrics(SM_CYSMICON))
                .Pass();
        big_window_icon_ =
            CreateHICONFromSkBitmapSizedTo(icon, GetSystemMetrics(SM_CXICON),
                                           GetSystemMetrics(SM_CYICON))
                .Pass();

        small_icon = small_window_icon_.get();
        big_icon = big_window_icon_.get();
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
    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(small_icon));

    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_BIG),
                reinterpret_cast<LPARAM>(big_icon));
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
