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
#include "base/feature_list.h"
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
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
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
constexpr int kTabstripRightSpacing = 10;
// Height of the shadow in the tab image, used to ensure clicks in the shadow
// area still drag restored windows.  This keeps the clickable area large enough
// to hit easily.
constexpr int kTabShadowHeight = 4;

constexpr SkColor kMdWebUIFrameColor =
    SkColorSetARGBMacro(0xff, 0x25, 0x4f, 0xae);

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, public:

BrowserNonClientFrameViewAsh::BrowserNonClientFrameViewAsh(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      caption_button_container_(nullptr),
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

  if (UsePackagedAppHeaderStyle()) {
    ash::DefaultHeaderPainter* header_painter = new ash::DefaultHeaderPainter;
    header_painter_.reset(header_painter);
    header_painter->Init(frame(), this, caption_button_container_);
    if (window_icon_)
      header_painter->UpdateLeftHeaderView(window_icon_);
    if (base::FeatureList::IsEnabled(features::kMaterialDesignSettings)) {
      // For non app (i.e. WebUI) windows (e.g. Settings) use MD frame color.
      if (!browser_view()->browser()->is_app())
        header_painter->SetFrameColors(kMdWebUIFrameColor, kMdWebUIFrameColor);
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

  const int left_inset = GetTabStripLeftInset();
  return gfx::Rect(left_inset, GetTopInset(false),
                   std::max(0, width() - left_inset - GetTabStripRightInset()),
                   tabstrip->GetPreferredSize().height());
}

int BrowserNonClientFrameViewAsh::GetTopInset(bool restored) const {
  if (!ShouldPaint()) {
    // When immersive fullscreen unrevealed, tabstrip is offscreen with normal
    // tapstrip bounds, the top inset should reach this topmost edge.
    const ImmersiveModeController* const immersive_controller =
        browser_view()->immersive_mode_controller();
    if (immersive_controller->IsEnabled() &&
        !immersive_controller->IsRevealed()) {
      return (-1) * browser_view()->GetTabStripHeight();
    }
    return 0;
  }

  if (!browser_view()->IsTabStripVisible()) {
    return (UsePackagedAppHeaderStyle())
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
  caption_button_container_->SetVisible(true);
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

  const bool should_paint_as_active = ShouldPaintAsActive();
  caption_button_container_->SetPaintAsActive(should_paint_as_active);

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
  const int avatar_right =
      profile_indicator_icon()
          ? (kAvatarIconPadding + GetIncognitoAvatarIcon().width())
          : 0;
  return avatar_right + kAvatarIconPadding;
}

int BrowserNonClientFrameViewAsh::GetTabStripRightInset() const {
  return kTabstripRightSpacing +
      caption_button_container_->GetPreferredSize().width();
}

bool BrowserNonClientFrameViewAsh::UsePackagedAppHeaderStyle() const {
  // Use for non tabbed trusted source windows, e.g. Settings, as well as apps.
  const Browser* const browser = browser_view()->browser();
  return (!browser->is_type_tabbed() && browser->is_trusted_source()) ||
         browser->is_app();
}

void BrowserNonClientFrameViewAsh::LayoutProfileIndicatorIcon() {
  DCHECK(profile_indicator_icon());
#if !defined(OS_CHROMEOS)
  // ChromeOS shows avatar on V1 app.
  DCHECK(browser_view()->IsTabStripVisible());
#endif

  const gfx::ImageSkia incognito_icon = GetIncognitoAvatarIcon();
  const int avatar_bottom = GetTopInset(false) +
                            browser_view()->GetTabStripHeight() -
                            kAvatarIconPadding;
  int avatar_y = avatar_bottom - incognito_icon.height();

  const int avatar_height = avatar_bottom - avatar_y;
  profile_indicator_icon()->SetBounds(kAvatarIconPadding, avatar_y,
                                      incognito_icon.width(), avatar_height);
  profile_indicator_icon()->SetVisible(true);
}

bool BrowserNonClientFrameViewAsh::ShouldPaint() const {
  if (!frame()->IsFullscreen())
    return true;

  // We need to paint when the top-of-window views are revealed in immersive
  // fullscreen.
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  return immersive_mode_controller->IsEnabled() &&
         immersive_mode_controller->IsRevealed();
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
  canvas->ClipRect(tabstrip_bounds, SkClipOp::kDifference);
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
