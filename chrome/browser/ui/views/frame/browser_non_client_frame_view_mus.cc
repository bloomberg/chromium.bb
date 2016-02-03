// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mus.h"

#include <algorithm>

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
#include "chrome/browser/ui/views/frame/browser_frame_mus.h"
#include "chrome/browser/ui/views/frame/browser_header_painter_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/web_app_left_header_view_ash.h"
#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/web_applications/web_app.h"
#include "components/mus/public/cpp/window.h"
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
#include "ui/views/mus/window_manager_frame_values.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

#if defined(FRAME_AVATAR_BUTTON)
// Space between the new avatar button and the minimize button.
const int kNewAvatarButtonOffset = 5;
#endif
// Space between right edge of tabstrip and maximize button.
const int kTabstripRightSpacing = 10;
// Height of the shadow of the content area, at the top of the toolbar.
const int kContentShadowHeight = 1;
// Space between top of window and top of tabstrip for tall headers, such as
// for restored windows, apps, etc.
const int kTabstripTopSpacingTall = 4;
// Space between top of window and top of tabstrip for short headers, such as
// for maximized windows, pop-ups, etc.
const int kTabstripTopSpacingShort = 0;
// Height of the shadow in the tab image, used to ensure clicks in the shadow
// area still drag restored windows.  This keeps the clickable area large enough
// to hit easily.
const int kTabShadowHeight = 4;

#if defined(FRAME_AVATAR_BUTTON)
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
#endif

const views::WindowManagerFrameValues& frame_values() {
  return views::WindowManagerFrameValues::instance();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMus, public:

// static
const char BrowserNonClientFrameViewMus::kViewClassName[] =
    "BrowserNonClientFrameViewMus";

BrowserNonClientFrameViewMus::BrowserNonClientFrameViewMus(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      window_icon_(nullptr),
      tab_strip_(nullptr) {}

BrowserNonClientFrameViewMus::~BrowserNonClientFrameViewMus() {
  if (tab_strip_) {
    tab_strip_->RemoveObserver(this);
    tab_strip_ = nullptr;
  }
}

void BrowserNonClientFrameViewMus::Init() {
  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this, nullptr);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }

  UpdateAvatar();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView:

void BrowserNonClientFrameViewMus::OnBrowserViewInitViewsComplete() {
  DCHECK(browser_view()->tabstrip());
  DCHECK(!tab_strip_);
  tab_strip_ = browser_view()->tabstrip();
  tab_strip_->AddObserver(this);
}

gfx::Rect BrowserNonClientFrameViewMus::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  if (!tabstrip)
    return gfx::Rect();

  // When the tab strip is painted in the immersive fullscreen light bar style,
  // the caption buttons and the avatar button are not visible. However, their
  // bounds are still used to compute the tab strip bounds so that the tabs have
  // the same horizontal position when the tab strip is painted in the immersive
  // light bar style as when the top-of-window views are revealed.
  int left_inset = GetTabStripLeftInset();
  int right_inset = GetTabStripRightInset();
  return gfx::Rect(left_inset, GetTopInset(false),
                   std::max(0, width() - left_inset - right_inset),
                   tabstrip->GetPreferredSize().height());
}

int BrowserNonClientFrameViewMus::GetTopInset(bool restored) const {
  if (!ShouldPaint() || UseImmersiveLightbarHeaderStyle())
    return 0;

  if (browser_view()->IsTabStripVisible()) {
    return ((frame()->IsMaximized() || frame()->IsFullscreen()) && !restored)
               ? kTabstripTopSpacingShort
               : kTabstripTopSpacingTall;
  }

  int caption_buttons_bottom = frame_values().normal_insets.top();

  // The toolbar partially overlaps the caption buttons.
  if (browser_view()->IsToolbarVisible())
    return caption_buttons_bottom - kContentShadowHeight;

  return caption_buttons_bottom + kClientEdgeThickness;
}

int BrowserNonClientFrameViewMus::GetThemeBackgroundXInset() const {
  return 5;
}

void BrowserNonClientFrameViewMus::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

void BrowserNonClientFrameViewMus::UpdateToolbar() {
}

views::View* BrowserNonClientFrameViewMus::GetLocationIconView() const {
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// views::NonClientFrameView:

gfx::Rect BrowserNonClientFrameViewMus::GetBoundsForClientView() const {
  // The ClientView must be flush with the top edge of the widget so that the
  // web contents can take up the entire screen in immersive fullscreen (with
  // or without the top-of-window views revealed). When in immersive fullscreen
  // and the top-of-window views are revealed, the TopContainerView paints the
  // window header by redirecting paints from its background to
  // BrowserNonClientFrameViewMus.
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewMus::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int BrowserNonClientFrameViewMus::NonClientHitTest(const gfx::Point& point) {
  // TODO(sky): figure out how this interaction should work.
  int hit_test = HTCLIENT;

#if defined(FRAME_AVATAR_BUTTON)
  if (hit_test == HTCAPTION && new_avatar_button() &&
      ConvertedHitTest(this, new_avatar_button(), point)) {
    return HTCLIENT;
  }
#endif

  // When the window is restored we want a large click target above the tabs
  // to drag the window, so redirect clicks in the tab's shadow to caption.
  if (hit_test == HTCLIENT &&
      !(frame()->IsMaximized() || frame()->IsFullscreen())) {
    // Convert point to client coordinates.
    gfx::Point client_point(point);
    View::ConvertPointToTarget(this, frame()->client_view(), &client_point);
    // Report hits in shadow at top of tabstrip as caption.
    gfx::Rect tabstrip_bounds(browser_view()->tabstrip()->bounds());
    if (client_point.y() < tabstrip_bounds.y() + kTabShadowHeight)
      hit_test = HTCAPTION;
  }
  return hit_test;
}

void BrowserNonClientFrameViewMus::GetWindowMask(const gfx::Size& size,
                                                 gfx::Path* window_mask) {
  // Aura does not use window masks.
}

void BrowserNonClientFrameViewMus::ResetWindowControls() {}

void BrowserNonClientFrameViewMus::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void BrowserNonClientFrameViewMus::UpdateWindowTitle() {}

void BrowserNonClientFrameViewMus::SizeConstraintsChanged() {}

///////////////////////////////////////////////////////////////////////////////
// views::View:

void BrowserNonClientFrameViewMus::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldPaint())
    return;

  // TODO(sky): get immersive mode working.

  if (UseImmersiveLightbarHeaderStyle()) {
    PaintImmersiveLightbarStyleHeader(canvas);
    return;
  }

  if (browser_view()->IsToolbarVisible())
    PaintToolbarBackground(canvas);
  else if (!UsePackagedAppHeaderStyle() && !UseWebAppHeaderStyle())
    PaintContentEdge(canvas);
}

void BrowserNonClientFrameViewMus::Layout() {
  if (avatar_button())
    LayoutAvatar();

#if defined(FRAME_AVATAR_BUTTON)
  if (new_avatar_button())
    LayoutNewStyleAvatar();
#endif

  BrowserNonClientFrameView::Layout();

  UpdateClientArea();
}

const char* BrowserNonClientFrameViewMus::GetClassName() const {
  return kViewClassName;
}

void BrowserNonClientFrameViewMus::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_TITLE_BAR;
}

gfx::Size BrowserNonClientFrameViewMus::GetMinimumSize() const {
  gfx::Size min_client_view_size(frame()->client_view()->GetMinimumSize());
  const int min_frame_width = frame_values().max_title_bar_button_width +
                              frame_values().normal_insets.width();
  int min_width = std::max(min_frame_width, min_client_view_size.width());
  if (browser_view()->IsTabStripVisible()) {
    // Ensure that the minimum width is enough to hold a minimum width tab strip
    // at its usual insets.
    int min_tabstrip_width =
        browser_view()->tabstrip()->GetMinimumSize().width();
    min_width =
        std::max(min_width, min_tabstrip_width + GetTabStripLeftInset() +
                                GetTabStripRightInset());
  }
  return gfx::Size(min_width, min_client_view_size.height());
}

void BrowserNonClientFrameViewMus::ChildPreferredSizeChanged(
    views::View* child) {
  // FrameCaptionButtonContainerView animates the visibility changes in
  // UpdateSizeButtonVisibility(false). Due to this a new size is not available
  // until the completion of the animation. Layout in response to the preferred
  // size changes.
  if (!browser_view()->initialized())
    return;
  bool needs_layout = false;
#if defined(FRAME_AVATAR_BUTTON)
  needs_layout = needs_layout || child == new_avatar_button();
#endif
  if (needs_layout) {
    InvalidateLayout();
    frame()->GetRootView()->Layout();
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabIconViewModel:

bool BrowserNonClientFrameViewMus::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to null check the selected
  // WebContents because in this condition there is not yet a selected tab.
  content::WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab ? current_tab->IsLoading() : false;
}

gfx::ImageSkia BrowserNonClientFrameViewMus::GetFaviconForTabIconView() {
  views::WidgetDelegate* delegate = frame()->widget_delegate();
  if (!delegate)
    return gfx::ImageSkia();
  return delegate->GetWindowIcon();
}
///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMus, protected:

// BrowserNonClientFrameView:
void BrowserNonClientFrameViewMus::UpdateNewAvatarButtonImpl() {
#if defined(FRAME_AVATAR_BUTTON)
  UpdateNewAvatarButton(AvatarButtonStyle::NATIVE);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMus, private:

mus::Window* BrowserNonClientFrameViewMus::mus_window() {
  return static_cast<BrowserFrameMus*>(frame()->native_widget())->window();
}

void BrowserNonClientFrameViewMus::UpdateClientArea() {
  std::vector<gfx::Rect> additional_client_area;
  if (tab_strip_) {
    gfx::Rect tab_strip_bounds(GetBoundsForTabStrip(tab_strip_));
    if (!tab_strip_bounds.IsEmpty() && tab_strip_->max_x()) {
      tab_strip_bounds.set_width(tab_strip_->max_x());
      additional_client_area.push_back(tab_strip_bounds);
    }
  }
  mus_window()->SetClientArea(
      views::WindowManagerFrameValues::instance().normal_insets,
      additional_client_area);
}

void BrowserNonClientFrameViewMus::TabStripMaxXChanged(TabStrip* tab_strip) {
  UpdateClientArea();
}

void BrowserNonClientFrameViewMus::TabStripDeleted(TabStrip* tab_strip) {
  tab_strip_->RemoveObserver(this);
  tab_strip_ = nullptr;
}

bool BrowserNonClientFrameViewMus::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  CHECK_EQ(target, this);
  if (!views::ViewTargeterDelegate::DoesIntersectRect(this, rect)) {
    // |rect| is outside BrowserNonClientFrameViewMus's bounds.
    return false;
  }

  TabStrip* tabstrip = browser_view()->tabstrip();
  if (tabstrip && browser_view()->IsTabStripVisible()) {
    // Claim |rect| only if it is above the bottom of the tabstrip in a non-tab
    // portion.
    gfx::RectF rect_in_tabstrip_coords_f(rect);
    View::ConvertRectToTarget(this, tabstrip, &rect_in_tabstrip_coords_f);
    gfx::Rect rect_in_tabstrip_coords =
        gfx::ToEnclosingRect(rect_in_tabstrip_coords_f);

    if (rect_in_tabstrip_coords.y() > tabstrip->height())
      return false;

    return !tabstrip->HitTestRect(rect_in_tabstrip_coords) ||
           tabstrip->IsRectInWindowCaption(rect_in_tabstrip_coords);
  }

  // Claim |rect| if it is above the top of the topmost view in the client area.
  return rect.y() < GetTopInset(false);
}

int BrowserNonClientFrameViewMus::GetTabStripLeftInset() const {
  const gfx::Insets insets(GetLayoutInsets(AVATAR_ICON));
  const int avatar_right =
      avatar_button()
          ? (insets.left() + browser_view()->GetOTRAvatarIcon().width())
          : 0;
  return avatar_right + insets.right() + frame_values().normal_insets.left();
}

int BrowserNonClientFrameViewMus::GetTabStripRightInset() const {
  const int frame_right_insets = frame_values().normal_insets.right() +
                                 frame_values().max_title_bar_button_width;
  int right_inset = kTabstripRightSpacing + frame_right_insets;

#if defined(FRAME_AVATAR_BUTTON)
  if (new_avatar_button()) {
    right_inset += kNewAvatarButtonOffset +
                   new_avatar_button()->GetPreferredSize().width();
  }
#endif

  return right_inset;
}

bool BrowserNonClientFrameViewMus::UseImmersiveLightbarHeaderStyle() const {
  ImmersiveModeController* immersive_controller =
      browser_view()->immersive_mode_controller();
  return immersive_controller->IsEnabled() &&
         !immersive_controller->IsRevealed() &&
         browser_view()->IsTabStripVisible();
}

bool BrowserNonClientFrameViewMus::UsePackagedAppHeaderStyle() const {
  Browser* browser = browser_view()->browser();
  // For non tabbed trusted source windows, e.g. Settings, use the packaged
  // app style frame.
  if (!browser->is_type_tabbed() && browser->is_trusted_source())
    return true;
  // Use the packaged app style for apps that aren't using the newer WebApp
  // style.
  return browser->is_app() && !UseWebAppHeaderStyle();
}

bool BrowserNonClientFrameViewMus::UseWebAppHeaderStyle() const {
  return browser_view()->browser()->SupportsWindowFeature(
      Browser::FEATURE_WEBAPPFRAME);
}

void BrowserNonClientFrameViewMus::LayoutAvatar() {
  DCHECK(avatar_button());
#if !defined(OS_CHROMEOS)
  // ChromeOS shows avatar on V1 app.
  DCHECK(browser_view()->IsTabStripVisible());
#endif
  gfx::ImageSkia incognito_icon = browser_view()->GetOTRAvatarIcon();
  gfx::Insets avatar_insets = GetLayoutInsets(AVATAR_ICON);
  int avatar_bottom = GetTopInset(false) + browser_view()->GetTabStripHeight() -
                      avatar_insets.bottom();
  int avatar_y = avatar_bottom - incognito_icon.height();
  if (!ui::MaterialDesignController::IsModeMaterial() &&
      browser_view()->IsTabStripVisible() &&
      (frame()->IsMaximized() || frame()->IsFullscreen())) {
    avatar_y = GetTopInset(false) + kContentShadowHeight;
  }

  // Hide the incognito icon in immersive fullscreen when the tab light bar is
  // visible because the header is too short for the icognito icon to be
  // recognizable.
  bool avatar_visible = !UseImmersiveLightbarHeaderStyle();
  int avatar_height = avatar_visible ? avatar_bottom - avatar_y : 0;

  gfx::Rect avatar_bounds(avatar_insets.left(), avatar_y,
                          incognito_icon.width(), avatar_height);
  avatar_button()->SetBoundsRect(avatar_bounds);
  avatar_button()->SetVisible(avatar_visible);
}

#if defined(FRAME_AVATAR_BUTTON)
void BrowserNonClientFrameViewMus::LayoutNewStyleAvatar() {
  NOTIMPLEMENTED();
}
#endif

bool BrowserNonClientFrameViewMus::ShouldPaint() const {
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

void BrowserNonClientFrameViewMus::PaintImmersiveLightbarStyleHeader(
    gfx::Canvas* canvas) {}

void BrowserNonClientFrameViewMus::PaintToolbarBackground(gfx::Canvas* canvas) {
  gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  if (toolbar_bounds.IsEmpty())
    return;
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  View::ConvertPointToTarget(browser_view(), this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);

  int x = toolbar_bounds.x();
  int w = toolbar_bounds.width();
  int y = toolbar_bounds.y();
  const ui::ThemeProvider* tp = GetThemeProvider();

  if (ui::MaterialDesignController::IsModeMaterial()) {
    if (tp->HasCustomImage(IDR_THEME_TOOLBAR)) {
      // Paint the main toolbar image.  Since this image is also used to draw
      // the tab background, we must use the tab strip offset to compute the
      // image source y position.  If you have to debug this code use an image
      // editor to paint a diagonal line through the toolbar image and ensure it
      // lines up across the tab and toolbar.
      gfx::ImageSkia* theme_toolbar = tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR);
      canvas->TileImageInt(*theme_toolbar, x + GetThemeBackgroundXInset(),
                           y - GetTopInset(false), x, y, w,
                           theme_toolbar->height());
    } else {
      canvas->FillRect(toolbar_bounds,
                       tp->GetColor(ThemeProperties::COLOR_TOOLBAR));
    }

    // Draw the separator line atop the toolbar, on the left and right of the
    // tabstrip.
    // TODO(tdanderson): Draw the separator line for non-tabbed windows.
    if (browser_view()->IsTabStripVisible()) {
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
    }

    // Draw the content/toolbar separator.
    toolbar_bounds.Inset(kClientEdgeThickness, 0);
    BrowserView::Paint1pxHorizontalLine(
        canvas, tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR),
        toolbar_bounds, true);
  } else {
    // NOTE: this ifdef can't be OS_CHROMEOS as we want to see how it looks on
    // windows as well.
#if defined(USE_ASH)
    int h = toolbar_bounds.height();
    // Gross hack: We split the toolbar images into two pieces, since sometimes
    // (popup mode) the toolbar isn't tall enough to show the whole image.  The
    // split happens between the top shadow section and the bottom gradient
    // section so that we never break the gradient.
    // NOTE(pkotwicz): If the computation for |bottom_y| is changed, Layout()
    // must be changed as well.
    int split_point = kFrameShadowThickness * 2;
    int bottom_y = y + split_point;
    int bottom_edge_height = h - split_point;

    canvas->FillRect(gfx::Rect(x, bottom_y, w, bottom_edge_height),
                     tp->GetColor(ThemeProperties::COLOR_TOOLBAR));

    // Paint the main toolbar image.  Since this image is also used to draw the
    // tab background, we must use the tab strip offset to compute the image
    // source y position.  If you have to debug this code use an image editor
    // to paint a diagonal line through the toolbar image and ensure it lines up
    // across the tab and toolbar.
    gfx::ImageSkia* theme_toolbar = tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR);
    canvas->TileImageInt(*theme_toolbar, x + GetThemeBackgroundXInset(),
                         bottom_y - GetTopInset(false), x, bottom_y, w,
                         theme_toolbar->height());

    // The pre-material design content area line has a shadow that extends a
    // couple of pixels above the toolbar bounds.
    const int kContentShadowHeight = 2;
    gfx::ImageSkia* toolbar_top = tp->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_TOP);
    canvas->TileImageInt(*toolbar_top, 0, 0, x, y - kContentShadowHeight, w,
                         split_point + kContentShadowHeight + 1);

    // Draw the "lightening" shade line around the edges of the toolbar.
    gfx::ImageSkia* toolbar_left =
        tp->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_LEFT);
    canvas->TileImageInt(*toolbar_left, 0, 0, x + kClientEdgeThickness,
                         y + kClientEdgeThickness + kContentShadowHeight,
                         toolbar_left->width(), theme_toolbar->height());
    gfx::ImageSkia* toolbar_right =
        tp->GetImageSkiaNamed(IDR_TOOLBAR_SHADE_RIGHT);
    canvas->TileImageInt(*toolbar_right, 0, 0,
                         w - toolbar_right->width() - 2 * kClientEdgeThickness,
                         y + kClientEdgeThickness + kContentShadowHeight,
                         toolbar_right->width(), theme_toolbar->height());

    // Draw the content/toolbar separator.
    canvas->FillRect(
        gfx::Rect(x + kClientEdgeThickness,
                  toolbar_bounds.bottom() - kClientEdgeThickness,
                  w - (2 * kClientEdgeThickness), kClientEdgeThickness),
        tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR));
#else
    // This is the case for running on non-chromeos. Decide how we want this to
    // look.
#endif
  }
}

void BrowserNonClientFrameViewMus::PaintContentEdge(gfx::Canvas* canvas) {
  DCHECK(!UsePackagedAppHeaderStyle() && !UseWebAppHeaderStyle());
  const int bottom = frame_values().normal_insets.bottom();
  canvas->FillRect(
      gfx::Rect(0, bottom, width(), kClientEdgeThickness),
      GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR));
}
