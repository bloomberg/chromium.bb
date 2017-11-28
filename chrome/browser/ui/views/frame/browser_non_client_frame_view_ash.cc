// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include <algorithm>

#include "ash/ash_constants.h"
#include "ash/ash_layout_constants.h"
#include "ash/frame/caption_buttons/frame_back_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/default_frame_header.h"
#include "ash/frame/frame_border_hit_test.h"
#include "ash/frame/frame_header_util.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_frame_header_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "chrome/browser/ui/views/frame/hosted_app_frame_header_ash.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/web_applications/web_app.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// Space between right edge of tabstrip and maximize button.
constexpr int kTabstripRightSpacing = 10;
// Height of the shadow in the tab image, used to ensure clicks in the shadow
// area still drag restored windows.  This keeps the clickable area large enough
// to hit easily.
constexpr int kTabShadowHeight = 4;

constexpr SkColor kMdWebUIFrameColor =
    SkColorSetARGBMacro(0xff, 0x25, 0x4f, 0xae);

bool IsV1AppBackButtonEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshEnableV1AppBackButton);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, public:

BrowserNonClientFrameViewAsh::BrowserNonClientFrameViewAsh(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      caption_button_container_(nullptr),
      back_button_(nullptr),
      window_icon_(nullptr),
      hosted_app_button_container_(nullptr) {
  ash::wm::InstallResizeHandleWindowTargeterForWindow(frame->GetNativeWindow(),
                                                      nullptr);
  ash::Shell::Get()->AddShellObserver(this);
}

BrowserNonClientFrameViewAsh::~BrowserNonClientFrameViewAsh() {
  if (TabletModeClient::Get())
    TabletModeClient::Get()->RemoveObserver(this);
  ash::Shell::Get()->RemoveShellObserver(this);
}

void BrowserNonClientFrameViewAsh::Init() {
  caption_button_container_ = new ash::FrameCaptionButtonContainerView(frame());
  caption_button_container_->UpdateSizeButtonVisibility();
  AddChildView(caption_button_container_);

  Browser* browser = browser_view()->browser();

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this, nullptr);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }
  if (browser->is_app() && IsV1AppBackButtonEnabled()) {
    back_button_ = new ash::FrameBackButton();
    AddChildView(back_button_);
    // TODO(oshima): Update the back button state.
  }

  frame_header_ = CreateFrameHeader();

  if (browser->is_app()) {
    frame()->GetNativeWindow()->SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::CHROME_APP));
  } else {
    frame()->GetNativeWindow()->SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::BROWSER));
  }

  // TabletModeClient may not be initialized during unit tests.
  if (TabletModeClient::Get())
    TabletModeClient::Get()->AddObserver(this);
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

    // The header isn't painted for restored popup/app windows in overview mode,
    // but the inset is still calculated below, so the overview code can align
    // the window content with a fake header.
    if (!in_overview_mode_ || frame()->IsFullscreen() ||
        browser_view()->IsTabStripVisible()) {
      return 0;
    }
  }

  if (!browser_view()->IsTabStripVisible()) {
    return (UsePackagedAppHeaderStyle())
               ? frame_header_->GetHeaderHeight()
               : caption_button_container_->bounds().bottom();
  }

  const int header_height =
      restored
          ? GetAshLayoutSize(AshLayoutSize::BROWSER_RESTORED_CAPTION_BUTTON)
                .height()
          : frame_header_->GetHeaderHeight();
  return header_height - browser_view()->GetTabStripHeight();
}

int BrowserNonClientFrameViewAsh::GetThemeBackgroundXInset() const {
  return ash::FrameHeaderUtil::GetThemeBackgroundXInset();
}

void BrowserNonClientFrameViewAsh::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

void BrowserNonClientFrameViewAsh::UpdateMinimumSize() {
  gfx::Size min_size = GetMinimumSize();
  aura::Window* frame_window = frame()->GetNativeWindow();
  const gfx::Size* previous_min_size =
      frame_window->GetProperty(aura::client::kMinimumSize);
  if (!previous_min_size || *previous_min_size != min_size) {
    frame_window->SetProperty(aura::client::kMinimumSize,
                              new gfx::Size(min_size));
  }
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
  if (hosted_app_button_container_) {
    gfx::Point client_point(point);
    View::ConvertPointToTarget(this, hosted_app_button_container_,
                               &client_point);
    if (hosted_app_button_container_->HitTestPoint(client_point))
      return HTCLIENT;
  }

  const int hit_test = ash::FrameBorderNonClientHitTest(
      this, back_button_, caption_button_container_, point);

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
  if (hosted_app_button_container_)
    hosted_app_button_container_->RefreshContentSettingViews();
}

void BrowserNonClientFrameViewAsh::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void BrowserNonClientFrameViewAsh::UpdateWindowTitle() {
  if (!frame()->IsFullscreen())
    frame_header_->SchedulePaintForTitle();
}

void BrowserNonClientFrameViewAsh::SizeConstraintsChanged() {
}

///////////////////////////////////////////////////////////////////////////////
// views::View:

void BrowserNonClientFrameViewAsh::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldPaint())
    return;

  const bool should_paint_as_active = ShouldPaintAsActive();
  frame_header_->SetPaintAsActive(should_paint_as_active);

  const ash::FrameHeader::Mode header_mode =
      should_paint_as_active ? ash::FrameHeader::MODE_ACTIVE
                             : ash::FrameHeader::MODE_INACTIVE;
  frame_header_->PaintHeader(canvas, header_mode);

  if (hosted_app_button_container_)
    hosted_app_button_container_->SetPaintAsActive(should_paint_as_active);

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
  frame_header_->LayoutHeader();

  int painted_height = GetTopInset(false);
  if (browser_view()->IsTabStripVisible())
    painted_height += browser_view()->tabstrip()->GetPreferredSize().height();

  frame_header_->SetHeaderHeightForPainting(painted_height);

  if (profile_indicator_icon())
    LayoutProfileIndicatorIcon();
  BrowserNonClientFrameView::Layout();
  const bool immersive =
      browser_view()->immersive_mode_controller()->IsEnabled();
  const bool tab_strip_visible = browser_view()->IsTabStripVisible();
  // In immersive fullscreen mode, the top view inset property should be 0.
  const int inset =
      (tab_strip_visible || immersive) ? 0 : GetTopInset(/*restored=*/false);
  frame()->GetNativeWindow()->SetProperty(aura::client::kTopViewInset, inset);
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
  int min_width = std::max(frame_header_->GetMinimumHeaderWidth(),
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
  OnOverviewModeChanged(true);
}

void BrowserNonClientFrameViewAsh::OnOverviewModeEnded() {
  OnOverviewModeChanged(false);
}

///////////////////////////////////////////////////////////////////////////////
// ash::mojom::TabletModeClient:

void BrowserNonClientFrameViewAsh::OnTabletModeToggled(bool enabled) {
  caption_button_container_->UpdateSizeButtonVisibility();

  if (enabled) {
    // Enter immersive mode if the feature is enabled and the widget is not
    // already in fullscreen mode. Popups that are not activated but not
    // minimized are still put in immersive mode, since they may still be
    // visible but not activated due to something transparent and/or not
    // fullscreen (ie. fullscreen launcher).
    if (TabletModeClient::Get()->auto_hide_title_bars() &&
        !frame()->IsFullscreen() && !browser_view()->IsBrowserTypeNormal() &&
        !frame()->IsMinimized()) {
      browser_view()->immersive_mode_controller()->SetEnabled(true);
      return;
    }
  } else {
    // Exit immersive mode if the feature is enabled and the widget is not in
    // fullscreen mode.
    if (TabletModeClient::Get()->auto_hide_title_bars() &&
        !frame()->IsFullscreen() && !browser_view()->IsBrowserTypeNormal()) {
      browser_view()->immersive_mode_controller()->SetEnabled(false);
      return;
    }
  }

  InvalidateLayout();
  // Can be null in tests.
  if (frame()->client_view())
    frame()->client_view()->InvalidateLayout();
  if (frame()->GetRootView())
    frame()->GetRootView()->Layout();
}

///////////////////////////////////////////////////////////////////////////////
// TabIconViewModel:

bool BrowserNonClientFrameViewAsh::ShouldTabIconViewAnimate() const {
  // Hosted apps use their app icon and shouldn't show a throbber.
  if (extensions::HostedAppBrowserController::IsForExperimentalHostedAppBrowser(
          browser_view()->browser())) {
    return false;
  }

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
      MultiUserWindowManager::ShouldShowAvatar(
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
  // We need to paint when the top-of-window views are revealed in immersive
  // fullscreen.
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  if (immersive_mode_controller->IsEnabled())
    return immersive_mode_controller->IsRevealed();

  if (frame()->IsFullscreen())
    return false;

  // Do not paint for V1 apps in overview mode.
  return browser_view()->IsBrowserTypeNormal() || !in_overview_mode_;
}

void BrowserNonClientFrameViewAsh::OnOverviewModeChanged(bool in_overview) {
  in_overview_mode_ = in_overview;
  caption_button_container_->SetVisible(!in_overview);
  if (window_icon_)
    window_icon_->SetVisible(!in_overview);
  if (back_button_)
    back_button_->SetVisible(!in_overview);
  // Schedule a paint to show or hide the header.
  SchedulePaint();
}

std::unique_ptr<ash::FrameHeader>
BrowserNonClientFrameViewAsh::CreateFrameHeader() {
  Browser* browser = browser_view()->browser();
  if (!UsePackagedAppHeaderStyle()) {
    auto browser_frame_header = std::make_unique<BrowserFrameHeaderAsh>();
    browser_frame_header->Init(frame(), browser_view(), this, window_icon_,
                               caption_button_container_, back_button_);
    return browser_frame_header;
  }
  std::unique_ptr<ash::DefaultFrameHeader> default_frame_header = nullptr;
  if (extensions::HostedAppBrowserController::IsForExperimentalHostedAppBrowser(
          browser)) {
    default_frame_header = std::make_unique<HostedAppFrameHeaderAsh>(
        browser->hosted_app_controller(), frame(), this,
        caption_button_container_);

    // Add the container for extra hosted app buttons (e.g app menu button).
    SkColor button_color = ash::FrameCaptionButton::GetButtonColor(
        default_frame_header->ShouldUseLightImages());
    hosted_app_button_container_ = new HostedAppButtonContainer(
        browser_view(), button_color,
        SkColorSetA(button_color,
                    255 * ash::kInactiveFrameButtonIconAlphaRatio));
    caption_button_container_->AddChildViewAt(hosted_app_button_container_, 0);
  } else {
    default_frame_header = std::make_unique<ash::DefaultFrameHeader>(
        frame(), this, caption_button_container_);
    if (!browser->is_app()) {
      // For non app (i.e. WebUI) windows (e.g. Settings) use MD frame color.
      default_frame_header->SetFrameColors(kMdWebUIFrameColor,
                                           kMdWebUIFrameColor);
    }
  }

  if (back_button_)
    default_frame_header->set_back_button(back_button_);

  if (window_icon_)
    default_frame_header->set_left_header_view(window_icon_);

  return default_frame_header;
}
