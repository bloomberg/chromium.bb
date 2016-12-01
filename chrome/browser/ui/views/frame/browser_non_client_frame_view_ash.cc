// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include <algorithm>

#include "ash/common/ash_layout_constants.h"
#include "ash/common/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/common/frame/default_header_painter.h"
#include "ash/common/frame/frame_border_hit_test.h"
#include "ash/common/frame/header_painter_util.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_header_painter_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/web_app_left_header_view_ash.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_CHROMEOS)
#include "ash/shared/app_types.h"
#endif

namespace {

// Space between right edge of tabstrip and maximize button.
const int kTabstripRightSpacing = 10;
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
      window_icon_(nullptr) {
  ash::WmLookup::Get()
      ->GetWindowForWidget(frame)
      ->InstallResizeHandleWindowTargeter(nullptr);
  ash::WmShell::Get()->AddShellObserver(this);
}

BrowserNonClientFrameViewAsh::~BrowserNonClientFrameViewAsh() {
  ash::WmShell::Get()->RemoveShellObserver(this);
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

#if defined(OS_CHROMEOS)
  if (browser_view()->browser()->is_app()) {
    frame()->GetNativeWindow()->SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::CHROME_APP));
  } else {
    frame()->GetNativeWindow()->SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::BROWSER));
  }
#endif
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
  const int hit_test =
      ash::FrameBorderNonClientHitTest(this, caption_button_container_, point);

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
  // TODO(yiyix): Update |caption_button_container_|'s visibility calculation
  // when Chrome OS MD is enabled by default.
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
      !browser_view()->toolbar()->GetPreferredSize().IsEmpty() &&
      browser_view()->IsTabStripVisible()) {
    PaintToolbarBackground(canvas);
  }
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

  if (profile_indicator_icon())
    LayoutProfileIndicatorIcon();
  BrowserNonClientFrameView::Layout();
  frame()->GetNativeWindow()->SetProperty(
      aura::client::kTopViewInset,
      browser_view()->IsTabStripVisible() ? 0 : GetTopInset(true));
}

const char* BrowserNonClientFrameViewAsh::GetClassName() const {
  return "BrowserNonClientFrameViewAsh";
}

void BrowserNonClientFrameViewAsh::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_TITLE_BAR;
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

void BrowserNonClientFrameViewAsh::OnOverviewModeStarting() {
  frame()->GetNativeWindow()->SetProperty(aura::client::kTopViewColor,
                                          GetFrameColor());
  caption_button_container_->SetVisible(false);
}

void BrowserNonClientFrameViewAsh::OnOverviewModeEnded() {
  caption_button_container_->SetVisible(true);
}

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
void BrowserNonClientFrameViewAsh::UpdateProfileIcons() {
  Browser* browser = browser_view()->browser();
  if (!browser->is_type_tabbed() && !browser->is_app())
    return;
  if ((browser->profile()->GetProfileType() == Profile::INCOGNITO_PROFILE) ||
      chrome::MultiUserWindowManager::ShouldShowAvatar(
          browser_view()->GetNativeWindow())) {
    UpdateProfileIndicatorIcon();
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, private:

int BrowserNonClientFrameViewAsh::GetTabStripLeftInset() const {
  const int pad = GetLayoutConstant(AVATAR_ICON_PADDING);
  const int avatar_right =
      profile_indicator_icon() ? (pad + GetIncognitoAvatarIcon().width()) : 0;
  return avatar_right + pad;
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

void BrowserNonClientFrameViewAsh::LayoutProfileIndicatorIcon() {
  DCHECK(profile_indicator_icon());
#if !defined(OS_CHROMEOS)
  // ChromeOS shows avatar on V1 app.
  DCHECK(browser_view()->IsTabStripVisible());
#endif

  const gfx::ImageSkia incognito_icon = GetIncognitoAvatarIcon();
  const int pad = GetLayoutConstant(AVATAR_ICON_PADDING);
  const int avatar_bottom =
      GetTopInset(false) + browser_view()->GetTabStripHeight() - pad;
  int avatar_y = avatar_bottom - incognito_icon.height();

  // Hide the incognito icon in immersive fullscreen when the tab light bar is
  // visible because the header is too short for the icognito icon to be
  // recognizable.
  const bool avatar_visible = !UseImmersiveLightbarHeaderStyle();
  const int avatar_height = avatar_visible ? (avatar_bottom - avatar_y) : 0;
  profile_indicator_icon()->SetBounds(pad, avatar_y, incognito_icon.width(),
                                      avatar_height);
  profile_indicator_icon()->SetVisible(avatar_visible);
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
  const ui::ThemeProvider* tp = GetThemeProvider();

  // Background.
  if (tp->HasCustomImage(IDR_THEME_TOOLBAR)) {
    const int bg_y = GetTopInset(false) + GetLayoutInsets(TAB).top();
    const int x = toolbar_bounds.x();
    const int y = toolbar_bounds.y();
    canvas->TileImageInt(*tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR),
                         x + GetThemeBackgroundXInset(), y - bg_y, x, y,
                         toolbar_bounds.width(), toolbar_bounds.height());
  } else {
    canvas->FillRect(toolbar_bounds,
                     tp->GetColor(ThemeProperties::COLOR_TOOLBAR));
  }

  // Top stroke.
  gfx::ScopedCanvas scoped_canvas(canvas);
  gfx::Rect tabstrip_bounds(GetBoundsForTabStrip(browser_view()->tabstrip()));
  tabstrip_bounds.set_x(GetMirroredXForRect(tabstrip_bounds));
  canvas->ClipRect(tabstrip_bounds, SkRegion::kDifference_Op);
  const gfx::Rect separator_rect(toolbar_bounds.x(), tabstrip_bounds.bottom(),
                                 toolbar_bounds.width(), 0);
  BrowserView::Paint1pxHorizontalLine(canvas, GetToolbarTopSeparatorColor(),
                                      separator_rect, true);

  // Toolbar/content separator.
  toolbar_bounds.Inset(kClientEdgeThickness, 0);
  BrowserView::Paint1pxHorizontalLine(
      canvas, tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR),
      toolbar_bounds, true);
}
