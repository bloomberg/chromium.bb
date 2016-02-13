// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include <algorithm>

#include "ash/ash_layout_constants.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/default_header_painter.h"
#include "ash/frame/frame_border_hit_test_controller.h"
#include "ash/frame/header_painter_util.h"
#include "ash/shell.h"
#include "base/profiler/scoped_tracker.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_header_painter_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/web_app_left_header_view_ash.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// Space between right edge of tabstrip and maximize button.
const int kTabstripRightSpacing = 10;
// The content edge images have a shadow built into them.
const int kContentEdgeShadowThickness = 2;
// Height of the shadow of the content area, at the top of the toolbar.
const int kContentShadowHeight = 1;
// Space between top of window and top of tabstrip for tall headers, such as
// for restored windows, apps, etc.
const int kTabstripTopSpacingTall = 7;
// Space between top of window and top of tabstrip for short headers, such as
// for maximized windows, pop-ups, etc.
const int kTabstripTopSpacingShort = 0;
// Height of the shadow in the tab image, used to ensure clicks in the shadow
// area still drag restored windows.  This keeps the clickable area large enough
// to hit easily.
const int kTabShadowHeight = 4;

// Combines View::ConvertPointToTarget() and View::HitTest() for a given
// |point|. Converts |point| from |src| to |dst| and hit tests it against |dst|.
bool ConvertedHitTest(views::View* src,
                      views::View* dst,
                      const gfx::Point& point) {
  DCHECK(src);
  DCHECK(dst);
  gfx::Point converted_point(point);
  views::View::ConvertPointToTarget(src, dst, &converted_point);
  return dst->HitTestPoint(converted_point);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, public:

BrowserNonClientFrameViewAsh::BrowserNonClientFrameViewAsh(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      caption_button_container_(nullptr),
      web_app_left_header_view_(nullptr),
      window_icon_(nullptr),
      frame_border_hit_test_controller_(
          new ash::FrameBorderHitTestController(frame)) {
  ash::Shell::GetInstance()->AddShellObserver(this);
}

BrowserNonClientFrameViewAsh::~BrowserNonClientFrameViewAsh() {
  ash::Shell::GetInstance()->RemoveShellObserver(this);
}

void BrowserNonClientFrameViewAsh::Init() {
  caption_button_container_ = new ash::FrameCaptionButtonContainerView(frame());
  caption_button_container_->UpdateSizeButtonVisibility();
  AddChildView(caption_button_container_);

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this, nullptr);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }

  UpdateAvatar();

  if (UsePackagedAppHeaderStyle() || UseWebAppHeaderStyle()) {
    ash::DefaultHeaderPainter* header_painter = new ash::DefaultHeaderPainter;
    header_painter_.reset(header_painter);
    header_painter->Init(frame(), this, caption_button_container_);
    if (UseWebAppHeaderStyle()) {
      web_app_left_header_view_ = new WebAppLeftHeaderView(browser_view());
      AddChildView(web_app_left_header_view_);
      header_painter->UpdateLeftHeaderView(web_app_left_header_view_);
    } else if (window_icon_) {
      header_painter->UpdateLeftHeaderView(window_icon_);
    }
  } else {
    BrowserHeaderPainterAsh* header_painter = new BrowserHeaderPainterAsh;
    header_painter_.reset(header_painter);
    header_painter->Init(frame(), browser_view(), this, window_icon_,
                         caption_button_container_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView:

gfx::Rect BrowserNonClientFrameViewAsh::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  if (!tabstrip)
    return gfx::Rect();

  // When the tab strip is painted in the immersive fullscreen light bar style,
  // the caption buttons and the avatar button are not visible. However, their
  // bounds are still used to compute the tab strip bounds so that the tabs have
  // the same horizontal position when the tab strip is painted in the immersive
  // light bar style as when the top-of-window views are revealed.
  const int left_inset = GetTabStripLeftInset();
  return gfx::Rect(left_inset, GetTopInset(false),
                   std::max(0, width() - left_inset - GetTabStripRightInset()),
                   tabstrip->GetPreferredSize().height());
}

int BrowserNonClientFrameViewAsh::GetTopInset(bool restored) const {
  if (!ShouldPaint() || UseImmersiveLightbarHeaderStyle())
    return 0;

  if (!browser_view()->IsTabStripVisible()) {
    return (UsePackagedAppHeaderStyle() || UseWebAppHeaderStyle())
        ? header_painter_->GetHeaderHeight()
        : caption_button_container_->bounds().bottom();
  }

  if (!ui::MaterialDesignController::IsModeMaterial()) {
    return ((frame()->IsMaximized() || frame()->IsFullscreen()) && !restored) ?
        kTabstripTopSpacingShort : kTabstripTopSpacingTall;
  }

  const int header_height = restored
      ? GetAshLayoutSize(
            AshLayoutSize::BROWSER_RESTORED_CAPTION_BUTTON).height()
      : header_painter_->GetHeaderHeight();
  return header_height - browser_view()->GetTabStripHeight();
}

int BrowserNonClientFrameViewAsh::GetThemeBackgroundXInset() const {
  return ash::HeaderPainterUtil::GetThemeBackgroundXInset();
}

void BrowserNonClientFrameViewAsh::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

void BrowserNonClientFrameViewAsh::UpdateToolbar() {
  if (web_app_left_header_view_)
    web_app_left_header_view_->Update();
}

views::View* BrowserNonClientFrameViewAsh::GetLocationIconView() const {
  return web_app_left_header_view_ ?
      web_app_left_header_view_->GetLocationIconView() : nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// views::NonClientFrameView:

gfx::Rect BrowserNonClientFrameViewAsh::GetBoundsForClientView() const {
  // The ClientView must be flush with the top edge of the widget so that the
  // web contents can take up the entire screen in immersive fullscreen (with
  // or without the top-of-window views revealed). When in immersive fullscreen
  // and the top-of-window views are revealed, the TopContainerView paints the
  // window header by redirecting paints from its background to
  // BrowserNonClientFrameViewAsh.
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int BrowserNonClientFrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  const int hit_test = ash::FrameBorderHitTestController::NonClientHitTest(
      this, caption_button_container_, point);

  // See if the point is actually within the web app back button.
  if (hit_test == HTCAPTION && web_app_left_header_view_ &&
      ConvertedHitTest(this, web_app_left_header_view_, point)) {
    return HTCLIENT;
  }

  // When the window is restored we want a large click target above the tabs
  // to drag the window, so redirect clicks in the tab's shadow to caption.
  if (hit_test == HTCLIENT && !frame()->IsMaximized() &&
      !frame()->IsFullscreen()) {
    gfx::Point client_point(point);
    View::ConvertPointToTarget(this, frame()->client_view(), &client_point);
    gfx::Rect tabstrip_bounds(browser_view()->tabstrip()->bounds());
    if (client_point.y() < tabstrip_bounds.y() + kTabShadowHeight)
      return HTCAPTION;
  }

  return hit_test;
}

void BrowserNonClientFrameViewAsh::GetWindowMask(const gfx::Size& size,
                                                 gfx::Path* window_mask) {
  // Aura does not use window masks.
}

void BrowserNonClientFrameViewAsh::ResetWindowControls() {
  // Hide the caption buttons in immersive fullscreen when the tab light bar
  // is visible because it's confusing when the user hovers or clicks in the
  // top-right of the screen and hits one.
  caption_button_container_->SetVisible(!UseImmersiveLightbarHeaderStyle());
  caption_button_container_->ResetWindowControls();
}

void BrowserNonClientFrameViewAsh::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void BrowserNonClientFrameViewAsh::UpdateWindowTitle() {
  if (!frame()->IsFullscreen())
    header_painter_->SchedulePaintForTitle();
}

void BrowserNonClientFrameViewAsh::SizeConstraintsChanged() {
}

///////////////////////////////////////////////////////////////////////////////
// views::View:

void BrowserNonClientFrameViewAsh::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldPaint())
    return;

  if (UseImmersiveLightbarHeaderStyle()) {
    // The light bar header is not themed because theming it does not look good.
    canvas->FillRect(
        gfx::Rect(width(), header_painter_->GetHeaderHeightForPainting()),
        SK_ColorBLACK);
    return;
  }

  const bool should_paint_as_active = ShouldPaintAsActive();
  caption_button_container_->SetPaintAsActive(should_paint_as_active);
  if (web_app_left_header_view_)
    web_app_left_header_view_->SetPaintAsActive(should_paint_as_active);

  const ash::HeaderPainter::Mode header_mode = should_paint_as_active ?
      ash::HeaderPainter::MODE_ACTIVE : ash::HeaderPainter::MODE_INACTIVE;
  header_painter_->PaintHeader(canvas, header_mode);

  if (browser_view()->IsToolbarVisible() &&
      !browser_view()->toolbar()->GetPreferredSize().IsEmpty()) {
    PaintToolbarBackground(canvas);
  }
  if (!browser_view()->IsTabStripVisible())
    PaintContentEdge(canvas);
}

void BrowserNonClientFrameViewAsh::Layout() {
  // The header must be laid out before computing |painted_height| because the
  // computation of |painted_height| for app and popup windows depends on the
  // position of the window controls.
  header_painter_->LayoutHeader();

  int painted_height = GetTopInset(false);
  if (browser_view()->IsTabStripVisible())
    painted_height += browser_view()->tabstrip()->GetPreferredSize().height();

  header_painter_->SetHeaderHeightForPainting(painted_height);

  if (avatar_button())
    LayoutAvatar();
  BrowserNonClientFrameView::Layout();
}

const char* BrowserNonClientFrameViewAsh::GetClassName() const {
  return "BrowserNonClientFrameViewAsh";
}

void BrowserNonClientFrameViewAsh::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_TITLE_BAR;
}

gfx::Size BrowserNonClientFrameViewAsh::GetMinimumSize() const {
  gfx::Size min_client_view_size(frame()->client_view()->GetMinimumSize());
  int min_width = std::max(header_painter_->GetMinimumHeaderWidth(),
                           min_client_view_size.width());
  if (browser_view()->IsTabStripVisible()) {
    // Ensure that the minimum width is enough to hold a minimum width tab strip
    // at its usual insets.
    const int min_tabstrip_width =
        browser_view()->tabstrip()->GetMinimumSize().width();
    min_width = std::max(
        min_width,
        min_tabstrip_width + GetTabStripLeftInset() + GetTabStripRightInset());
  }
  return gfx::Size(min_width, min_client_view_size.height());
}

void BrowserNonClientFrameViewAsh::ChildPreferredSizeChanged(
    views::View* child) {
  // FrameCaptionButtonContainerView animates the visibility changes in
  // UpdateSizeButtonVisibility(false). Due to this a new size is not available
  // until the completion of the animation. Layout in response to the preferred
  // size changes.
  if (browser_view()->initialized() && (child == caption_button_container_)) {
    InvalidateLayout();
    frame()->GetRootView()->Layout();
  }
}

///////////////////////////////////////////////////////////////////////////////
// ash::ShellObserver:

void BrowserNonClientFrameViewAsh::OnMaximizeModeStarted() {
  caption_button_container_->UpdateSizeButtonVisibility();
  InvalidateLayout();
  frame()->client_view()->InvalidateLayout();
  frame()->GetRootView()->Layout();
}

void BrowserNonClientFrameViewAsh::OnMaximizeModeEnded() {
  OnMaximizeModeStarted();
}

///////////////////////////////////////////////////////////////////////////////
// TabIconViewModel:

bool BrowserNonClientFrameViewAsh::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to null check the selected
  // WebContents because in this condition there is not yet a selected tab.
  content::WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab && current_tab->IsLoading();
}

gfx::ImageSkia BrowserNonClientFrameViewAsh::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  return delegate ? delegate->GetWindowIcon() : gfx::ImageSkia();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, protected:

// BrowserNonClientFrameView:
void BrowserNonClientFrameViewAsh::UpdateNewAvatarButtonImpl() {
  NOTREACHED();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, private:

// views::NonClientFrameView:
bool BrowserNonClientFrameViewAsh::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  CHECK_EQ(this, target);
  if (!views::ViewTargeterDelegate::DoesIntersectRect(this, rect)) {
    // |rect| is outside BrowserNonClientFrameViewAsh's bounds.
    return false;
  }

  TabStrip* tabstrip = browser_view()->tabstrip();
  if (!tabstrip || !browser_view()->IsTabStripVisible()) {
    // Claim |rect| if it is above the top of the topmost client area view.
    return rect.y() < GetTopInset(false);
  }

  // Claim |rect| if it is above the bottom of the tabstrip in a non-tab
  // portion.
  gfx::RectF rect_in_tabstrip_coords_f(rect);
  View::ConvertRectToTarget(this, tabstrip, &rect_in_tabstrip_coords_f);
  const gfx::Rect rect_in_tabstrip_coords(
      gfx::ToEnclosingRect(rect_in_tabstrip_coords_f));
  return (rect_in_tabstrip_coords.y() <= tabstrip->height()) &&
          (!tabstrip->HitTestRect(rect_in_tabstrip_coords) ||
          tabstrip->IsRectInWindowCaption(rect_in_tabstrip_coords));
}

int BrowserNonClientFrameViewAsh::GetTabStripLeftInset() const {
  const gfx::Insets insets(GetLayoutInsets(AVATAR_ICON));
  const int avatar_right = avatar_button() ?
      (insets.left() + browser_view()->GetOTRAvatarIcon().width()) : 0;
  return avatar_right + insets.right();
}

int BrowserNonClientFrameViewAsh::GetTabStripRightInset() const {
  return kTabstripRightSpacing +
      caption_button_container_->GetPreferredSize().width();
}

bool BrowserNonClientFrameViewAsh::UseImmersiveLightbarHeaderStyle() const {
  const ImmersiveModeController* const immersive_controller =
      browser_view()->immersive_mode_controller();
  return immersive_controller->IsEnabled() &&
         !immersive_controller->IsRevealed() &&
         browser_view()->IsTabStripVisible();
}

bool BrowserNonClientFrameViewAsh::UsePackagedAppHeaderStyle() const {
  // Use for non tabbed trusted source windows, e.g. Settings, as well as apps
  // that aren't using the newer WebApp style.
  const Browser* const browser = browser_view()->browser();
  return (!browser->is_type_tabbed() && browser->is_trusted_source()) ||
         (browser->is_app() && !UseWebAppHeaderStyle());
}

bool BrowserNonClientFrameViewAsh::UseWebAppHeaderStyle() const {
  return browser_view()->browser()->SupportsWindowFeature(
      Browser::FEATURE_WEBAPPFRAME);
}

void BrowserNonClientFrameViewAsh::LayoutAvatar() {
  DCHECK(avatar_button());
#if !defined(OS_CHROMEOS)
  // ChromeOS shows avatar on V1 app.
  DCHECK(browser_view()->IsTabStripVisible());
#endif

  const gfx::ImageSkia incognito_icon = browser_view()->GetOTRAvatarIcon();
  const gfx::Insets avatar_insets = GetLayoutInsets(AVATAR_ICON);
  const int avatar_bottom = GetTopInset(false) +
      browser_view()->GetTabStripHeight() - avatar_insets.bottom();
  int avatar_y = avatar_bottom - incognito_icon.height();
  if (!ui::MaterialDesignController::IsModeMaterial() &&
      browser_view()->IsTabStripVisible() &&
      (frame()->IsMaximized() || frame()->IsFullscreen())) {
    avatar_y = GetTopInset(false) + kContentShadowHeight;
  }

  // Hide the incognito icon in immersive fullscreen when the tab light bar is
  // visible because the header is too short for the icognito icon to be
  // recognizable.
  const bool avatar_visible = !UseImmersiveLightbarHeaderStyle();
  const int avatar_height = avatar_visible ? (avatar_bottom - avatar_y) : 0;
  avatar_button()->SetBounds(avatar_insets.left(), avatar_y,
                             incognito_icon.width(), avatar_height);
  avatar_button()->SetVisible(avatar_visible);
}

bool BrowserNonClientFrameViewAsh::ShouldPaint() const {
  if (!frame()->IsFullscreen())
    return true;

  // We need to paint when in immersive fullscreen and either:
  // - The top-of-window views are revealed.
  // - The lightbar style tabstrip is visible.
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  return immersive_mode_controller->IsEnabled() &&
      (immersive_mode_controller->IsRevealed() ||
       UseImmersiveLightbarHeaderStyle());
}

void BrowserNonClientFrameViewAsh::PaintToolbarBackground(gfx::Canvas* canvas) {
  gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  if (toolbar_bounds.IsEmpty())
    return;
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToTarget(browser_view(), this, &toolbar_origin);
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
    const int w = toolbar_bounds.width();

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

      // Top stroke.
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
      // Background.
      const int split_point = kContentEdgeShadowThickness;
      const int split_y = y + split_point;
      canvas->TileImageInt(*bg, x + GetThemeBackgroundXInset(), split_y - bg_y,
                           x, split_y, w, bg->height());

      // The pre-material design content area line has a shadow that extends a
      // couple of pixels above the toolbar bounds.
      gfx::ImageSkia* toolbar_top =
          tp->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_TOP);
      canvas->TileImageInt(*toolbar_top, 0, 0, x,
                           y - kContentEdgeShadowThickness, w,
                           toolbar_top->height());

      // Draw the "lightening" shade line around the edges of the toolbar.
      gfx::ImageSkia* toolbar_left =
          tp->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_LEFT);
      canvas->TileImageInt(
          *toolbar_left, 0, 0, x + kClientEdgeThickness,
          y + kClientEdgeThickness + kContentEdgeShadowThickness,
          toolbar_left->width(), bg->height());
      gfx::ImageSkia* toolbar_right =
          tp->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_RIGHT);
      canvas->TileImageInt(
          *toolbar_right, 0, 0,
          w - toolbar_right->width() - 2 * kClientEdgeThickness,
          y + kClientEdgeThickness + kContentEdgeShadowThickness,
          toolbar_right->width(), bg->height());
    }
  }

  // Draw the toolbar/content separator.
  toolbar_bounds.Inset(kClientEdgeThickness, h - kClientEdgeThickness,
                       kClientEdgeThickness, 0);
  if (md) {
    BrowserView::Paint1pxHorizontalLine(canvas, separator_color,
                                        toolbar_bounds, true);
  } else {
    canvas->FillRect(toolbar_bounds, separator_color);
  }
}

void BrowserNonClientFrameViewAsh::PaintContentEdge(gfx::Canvas* canvas) {
  // The content separator is drawn by DefaultHeaderPainter in these cases.
  if (UsePackagedAppHeaderStyle() || UseWebAppHeaderStyle())
    return;

  gfx::Rect separator_rect(
      0, caption_button_container_->bounds().bottom(), width(), 0);
  BrowserView::Paint1pxHorizontalLine(
      canvas,
      GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR),
      separator_rect, true);
}
