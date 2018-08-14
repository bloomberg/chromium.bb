// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include <algorithm>

#include "ash/frame/caption_buttons/frame_back_button.h"  // mash-ok
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"  // mash-ok
#include "ash/frame/default_frame_header.h"      // mash-ok
#include "ash/frame/frame_header_util.h"         // mash-ok
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_layout_constants.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/frame_border_hit_test.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ash/shell.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/window_manager_frame_values.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// The color used for the frame when showing a non-tabbed WebUI, such as
// the Settings window.
constexpr SkColor kMdWebUiFrameColor = SkColorSetARGB(0xff, 0x25, 0x4f, 0xae);

// Color for the window title text.
constexpr SkColor kNormalWindowTitleTextColor = SkColorSetRGB(40, 40, 40);
constexpr SkColor kIncognitoWindowTitleTextColor = SK_ColorWHITE;

bool IsMash() {
  return features::IsUsingWindowService();
}

bool IsV1AppBackButtonEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kAshEnableV1AppBackButton);
}

// Returns true if |window| is currently snapped in split view mode.
bool IsSnappedInSplitView(aura::Window* window,
                          ash::mojom::SplitViewState state) {
  ash::mojom::WindowStateType type =
      window->GetProperty(ash::kWindowStateTypeKey);
  switch (state) {
    case ash::mojom::SplitViewState::NO_SNAP:
      return false;
    case ash::mojom::SplitViewState::LEFT_SNAPPED:
      return type == ash::mojom::WindowStateType::LEFT_SNAPPED;
    case ash::mojom::SplitViewState::RIGHT_SNAPPED:
      return type == ash::mojom::WindowStateType::RIGHT_SNAPPED;
    case ash::mojom::SplitViewState::BOTH_SNAPPED:
      return type == ash::mojom::WindowStateType::LEFT_SNAPPED ||
             type == ash::mojom::WindowStateType::RIGHT_SNAPPED;
    default:
      NOTREACHED();
      return false;
  }
}

const views::WindowManagerFrameValues& frame_values() {
  return views::WindowManagerFrameValues::instance();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, public:

BrowserNonClientFrameViewAsh::BrowserNonClientFrameViewAsh(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
  // In Mash, Ash worries about split view and overview state so there's no need
  // to observe such changes.
  if (IsMash())
    return;

  ash::wm::InstallResizeHandleWindowTargeterForWindow(frame->GetNativeWindow(),
                                                      nullptr);
  ash::Shell::Get()->AddShellObserver(this);

  // The ServiceManagerConnection may be nullptr in tests.
  if (content::ServiceManagerConnection::GetForProcess()) {
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindInterface(ash::mojom::kServiceName, &split_view_controller_);
    ash::mojom::SplitViewObserverPtr observer;
    observer_binding_.Bind(mojo::MakeRequest(&observer));
    split_view_controller_->AddObserver(std::move(observer));
  }
}

BrowserNonClientFrameViewAsh::~BrowserNonClientFrameViewAsh() {
  browser_view()->browser()->command_controller()->RemoveCommandObserver(
      IDC_BACK, this);

  if (TabletModeClient::Get())
    TabletModeClient::Get()->RemoveObserver(this);

  // As with Init(), some of this may need porting to Mash.
  if (IsMash())
    return;

  ImmersiveModeController* immersive_controller =
      browser_view()->immersive_mode_controller();
  if (immersive_controller)
    immersive_controller->RemoveObserver(this);

  window_observer_.RemoveAll();

  ash::Shell::Get()->RemoveShellObserver(this);
}

void BrowserNonClientFrameViewAsh::Init() {
  if (!IsMash()) {
    caption_button_container_ =
        new ash::FrameCaptionButtonContainerView(frame());
    caption_button_container_->UpdateCaptionButtonState(false /*=animate*/);
    AddChildView(caption_button_container_);
  }

  Browser* browser = browser_view()->browser();
  if (IsMash() &&
      extensions::HostedAppBrowserController::IsForExperimentalHostedAppBrowser(
          browser_view()->browser())) {
    SetUpForHostedApp(nullptr);
  }

  // Initializing the TabIconView is expensive, so only do it if we need to.
  if (browser_view()->ShouldShowWindowIcon()) {
    window_icon_ = new TabIconView(this, nullptr);
    window_icon_->set_is_light(true);
    AddChildView(window_icon_);
    window_icon_->Update();
  }

  if (browser->is_app() && IsV1AppBackButtonEnabled())
    browser->command_controller()->AddCommandObserver(IDC_BACK, this);

  aura::Window* window = frame()->GetNativeWindow();
  window->SetProperty(
      aura::client::kAppType,
      static_cast<int>(browser->is_app() ? ash::AppType::CHROME_APP
                                         : ash::AppType::BROWSER));

  window_observer_.Add(IsMash() ? window->GetRootWindow() : window);

  // To preserve privacy, tag incognito windows so that they won't be included
  // in screenshot sent to assistant server.
  if (browser->profile()->IsOffTheRecord())
    window->SetProperty(ash::kBlockedForAssistantSnapshotKey, true);

  // TabletModeClient may not be initialized during unit tests.
  if (TabletModeClient::Get())
    TabletModeClient::Get()->AddObserver(this);

  // TODO(estade): how much of the rest of this needs porting to Mash?
  if (IsMash()) {
    window->SetProperty(ash::kFrameTextColorKey, GetTitleColor());
    OnThemeChanged();
    return;
  }

  if (browser->is_app() && IsV1AppBackButtonEnabled()) {
    back_button_ = new ash::FrameBackButton();
    AddChildView(back_button_);
    // TODO(oshima): Add Tooltip, accessibility name.
  }

  frame_header_ = CreateFrameHeader();

  browser_view()->immersive_mode_controller()->AddObserver(this);

  UpdateFrameColors();
}

ash::mojom::SplitViewObserverPtr
BrowserNonClientFrameViewAsh::CreateInterfacePtrForTesting() {
  if (observer_binding_.is_bound())
    observer_binding_.Unbind();
  ash::mojom::SplitViewObserverPtr ptr;
  observer_binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView:

void BrowserNonClientFrameViewAsh::OnSingleTabModeChanged() {
  if (!IsMash()) {
    BrowserNonClientFrameView::OnSingleTabModeChanged();
    return;
  }

  UpdateFrameColors();
}

gfx::Rect BrowserNonClientFrameViewAsh::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  if (!tabstrip)
    return gfx::Rect();

  const int left_inset = GetTabStripLeftInset();
  const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
  return gfx::Rect(left_inset, GetTopInset(restored),
                   std::max(0, width() - left_inset - GetTabStripRightInset()),
                   tabstrip->GetPreferredSize().height());
}

int BrowserNonClientFrameViewAsh::GetTopInset(bool restored) const {
  // TODO(estade): why do callsites in this class hardcode false for |restored|?
  if (IsMash())
    restored = !frame()->IsMaximized() && !frame()->IsFullscreen();

  if (!ShouldPaint()) {
    // When immersive fullscreen unrevealed, tabstrip is offscreen with normal
    // tapstrip bounds, the top inset should reach this topmost edge.
    const ImmersiveModeController* const immersive_controller =
        browser_view()->immersive_mode_controller();
    if (immersive_controller->IsEnabled() &&
        !immersive_controller->IsRevealed()) {
      return (-1) * browser_view()->GetTabStripHeight();
    }

    if (IsMash())
      return 0;

    // The header isn't painted for restored popup/app windows in overview mode,
    // but the inset is still calculated below, so the overview code can align
    // the window content with a fake header.
    if (!in_overview_mode_ || frame()->IsFullscreen() ||
        browser_view()->IsTabStripVisible()) {
      return 0;
    }
  }

  Browser* browser = browser_view()->browser();
  if (IsMash() && UsePackagedAppHeaderStyle(browser))
    return GetAshLayoutSize(ash::AshLayoutSize::kNonBrowserCaption).height();

  const int header_height =
      IsMash()
          ? GetAshLayoutSize(restored
                                 ? ash::AshLayoutSize::kBrowserCaptionRestored
                                 : ash::AshLayoutSize::kBrowserCaptionMaximized)
                .height()
          : frame_header_->GetHeaderHeight();

  if (browser_view()->IsTabStripVisible())
    return header_height - browser_view()->GetTabStripHeight();

  if (IsMash())
    return header_height;

  return UsePackagedAppHeaderStyle(browser)
             ? frame_header_->GetHeaderHeight()
             : caption_button_container_->bounds().bottom();
}

int BrowserNonClientFrameViewAsh::GetThemeBackgroundXInset() const {
  return ash::FrameHeaderUtil::GetThemeBackgroundXInset();
}

void BrowserNonClientFrameViewAsh::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

void BrowserNonClientFrameViewAsh::UpdateClientArea() {
  if (!IsMash())
    return BrowserNonClientFrameView::UpdateClientArea();

  std::vector<gfx::Rect> additional_client_area;
  int top_container_offset = 0;
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  // Frame decorations (the non-client area) are visible if not in immersive
  // mode, or in immersive mode *and* the reveal widget is showing.
  const bool show_frame_decorations = !immersive_mode_controller->IsEnabled() ||
                                      immersive_mode_controller->IsRevealed();
  if (browser_view()->IsTabStripVisible() && show_frame_decorations) {
    TabStrip* tab_strip = browser_view()->tabstrip();
    gfx::Rect tab_strip_bounds(GetBoundsForTabStrip(tab_strip));

    int tab_strip_max_x = tab_strip->GetTabsMaxX();
    if (!tab_strip_bounds.IsEmpty() && tab_strip_max_x) {
      tab_strip_bounds.set_width(tab_strip_max_x);
      // The new tab button may be inside or outside |tab_strip_bounds|.  If
      // it's outside, handle it similarly to those bounds.  If it's inside,
      // the Subtract() call below will leave it empty and it will be ignored
      // later.
      gfx::Rect new_tab_button_bounds = tab_strip->new_tab_button_bounds();
      new_tab_button_bounds.Subtract(tab_strip_bounds);
      if (immersive_mode_controller->IsEnabled()) {
        top_container_offset =
            immersive_mode_controller->GetTopContainerVerticalOffset(
                browser_view()->top_container()->size());
        tab_strip_bounds.set_y(tab_strip_bounds.y() + top_container_offset);
        new_tab_button_bounds.set_y(new_tab_button_bounds.y() +
                                    top_container_offset);
        tab_strip_bounds.Intersect(GetLocalBounds());
        new_tab_button_bounds.Intersect(GetLocalBounds());
      }

      additional_client_area.push_back(tab_strip_bounds);

      if (!new_tab_button_bounds.IsEmpty())
        additional_client_area.push_back(new_tab_button_bounds);
    }
  }

  if (hosted_app_button_container_) {
    gfx::Rect bounds = hosted_app_button_container_->ConvertRectToWidget(
        hosted_app_button_container_->GetLocalBounds());
    additional_client_area.push_back(bounds);
  }

  aura::WindowTreeHostMus* window_tree_host_mus =
      static_cast<aura::WindowTreeHostMus*>(
          GetWidget()->GetNativeWindow()->GetHost());
  if (show_frame_decorations) {
    const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
    const int header_height =
        GetAshLayoutSize(restored
                             ? ash::AshLayoutSize::kBrowserCaptionRestored
                             : ash::AshLayoutSize::kBrowserCaptionMaximized)
            .height();

    gfx::Insets client_area_insets =
        views::WindowManagerFrameValues::instance().normal_insets;
    client_area_insets.Set(header_height, client_area_insets.left(),
                           client_area_insets.bottom(),
                           client_area_insets.right());
    window_tree_host_mus->SetClientArea(client_area_insets,
                                        additional_client_area);
    views::Widget* reveal_widget = immersive_mode_controller->GetRevealWidget();
    if (reveal_widget) {
      // In immersive mode the reveal widget needs the same client area as
      // the Browser widget. This way mus targets the window manager (ash) for
      // clicks in the frame decoration.
      static_cast<aura::WindowTreeHostMus*>(
          reveal_widget->GetNativeWindow()->GetHost())
          ->SetClientArea(client_area_insets, additional_client_area);
    }
  } else {
    window_tree_host_mus->SetClientArea(gfx::Insets(), additional_client_area);
  }
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

int BrowserNonClientFrameViewAsh::GetTabStripLeftInset() const {
  return BrowserNonClientFrameView::GetTabStripLeftInset() +
         frame_values().normal_insets.left();
}

void BrowserNonClientFrameViewAsh::OnTabsMaxXChanged() {
  BrowserNonClientFrameView::OnTabsMaxXChanged();
  UpdateClientArea();
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
  int hit_test = ash::FrameBorderNonClientHitTest(this, point);

  // When the window is restored we want a large click target above the tabs
  // to drag the window, so redirect clicks in the tab's shadow to caption.
  if (hit_test == HTCLIENT && !frame()->IsMaximized() &&
      !frame()->IsFullscreen()) {
    gfx::Point client_point(point);
    View::ConvertPointToTarget(this, frame()->client_view(), &client_point);
    gfx::Rect tabstrip_shadow_bounds(browser_view()->tabstrip()->bounds());
    constexpr int kTabShadowHeight = 4;
    tabstrip_shadow_bounds.set_height(kTabShadowHeight);
    if (tabstrip_shadow_bounds.Contains(client_point))
      return HTCAPTION;
  }

  return hit_test;
}

void BrowserNonClientFrameViewAsh::GetWindowMask(const gfx::Size& size,
                                                 gfx::Path* window_mask) {
  // Aura does not use window masks.
}

void BrowserNonClientFrameViewAsh::ResetWindowControls() {
  if (!IsMash()) {
    caption_button_container_->SetVisible(true);
    caption_button_container_->ResetWindowControls();
  }

  if (hosted_app_button_container_)
    hosted_app_button_container_->UpdateContentSettingViewsVisibility();
}

void BrowserNonClientFrameViewAsh::UpdateWindowIcon() {
  if (window_icon_)
    window_icon_->SchedulePaint();
}

void BrowserNonClientFrameViewAsh::UpdateWindowTitle() {
  if (!IsMash() && !frame()->IsFullscreen())
    frame_header_->SchedulePaintForTitle();
}

void BrowserNonClientFrameViewAsh::SizeConstraintsChanged() {}

void BrowserNonClientFrameViewAsh::ActivationChanged(bool active) {
  BrowserNonClientFrameView::ActivationChanged(active);

  const bool should_paint_as_active = ShouldPaintAsActive();

  if (!IsMash())
    frame_header_->SetPaintAsActive(should_paint_as_active);

  if (hosted_app_button_container_)
    hosted_app_button_container_->SetPaintAsActive(should_paint_as_active);
}

///////////////////////////////////////////////////////////////////////////////
// views::View:

void BrowserNonClientFrameViewAsh::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldPaint())
    return;

  if (frame_header_) {
    DCHECK(!IsMash());
    const ash::FrameHeader::Mode header_mode =
        ShouldPaintAsActive() ? ash::FrameHeader::MODE_ACTIVE
                              : ash::FrameHeader::MODE_INACTIVE;
    frame_header_->PaintHeader(canvas, header_mode);
  }

  if (browser_view()->IsToolbarVisible() &&
      !browser_view()->toolbar()->GetPreferredSize().IsEmpty() &&
      browser_view()->IsTabStripVisible()) {
    PaintToolbarTopStroke(canvas);
  }
}

void BrowserNonClientFrameViewAsh::Layout() {
  if (IsMash()) {
    if (profile_indicator_icon())
      LayoutIncognitoButton();

    if (hosted_app_button_container_) {
      const gfx::Rect* inverted_caption_button_bounds =
          frame()->GetNativeWindow()->GetRootWindow()->GetProperty(
              ash::kCaptionButtonBoundsKey);
      if (inverted_caption_button_bounds) {
        hosted_app_button_container_->LayoutInContainer(
            0, inverted_caption_button_bounds->x() + width(),
            inverted_caption_button_bounds->height());
      }
    }

    BrowserNonClientFrameView::Layout();

    UpdateClientArea();

    frame()->GetNativeWindow()->SetProperty(ash::kFrameImageYInsetKey,
                                            GetFrameHeaderImageYInset());
    return;
  }

  // The header must be laid out before computing |painted_height| because the
  // computation of |painted_height| for app and popup windows depends on the
  // position of the window controls.
  frame_header_->LayoutHeader();

  int painted_height = GetTopInset(false);
  if (browser_view()->IsTabStripVisible())
    painted_height += browser_view()->tabstrip()->GetPreferredSize().height();

  frame_header_->SetHeaderHeightForPainting(painted_height);

  if (profile_indicator_icon())
    LayoutIncognitoButton();
  if (hosted_app_button_container_) {
    hosted_app_button_container_->LayoutInContainer(
        0, caption_button_container_->x(), painted_height);
  }

  BrowserNonClientFrameView::Layout();
  const bool immersive =
      browser_view()->immersive_mode_controller()->IsEnabled();
  const bool tab_strip_visible = browser_view()->IsTabStripVisible();
  // In immersive fullscreen mode, the top view inset property should be 0.
  const int inset =
      (tab_strip_visible || immersive) ? 0 : GetTopInset(/*restored=*/false);
  frame()->GetNativeWindow()->SetProperty(aura::client::kTopViewInset, inset);

  // The top right corner must be occupied by a caption button for easy mouse
  // access. This check is agnostic to RTL layout.
  DCHECK_EQ(caption_button_container_->y(), 0);
  DCHECK_EQ(caption_button_container_->bounds().right(), width());
}

const char* BrowserNonClientFrameViewAsh::GetClassName() const {
  return "BrowserNonClientFrameViewAsh";
}

void BrowserNonClientFrameViewAsh::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTitleBar;
}

gfx::Size BrowserNonClientFrameViewAsh::GetMinimumSize() const {
  gfx::Size min_client_view_size(frame()->client_view()->GetMinimumSize());
  const int min_frame_width = IsMash()
                                  ? frame_values().max_title_bar_button_width +
                                        frame_values().normal_insets.width()
                                  : frame_header_->GetMinimumHeaderWidth();
  int min_width = std::max(min_frame_width, min_client_view_size.width());
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

void BrowserNonClientFrameViewAsh::OnThemeChanged() {
  if (!IsMash()) {
    UpdateFrameColors();
    return;
  }

  aura::Window* window = frame()->GetNativeWindow();
  auto update_window_image = [&window](auto property_key,
                                       const gfx::ImageSkia& image) {
    scoped_refptr<ImageRegistration> image_registration;
    if (image.isNull()) {
      window->ClearProperty(property_key);
      return image_registration;
    }

    image_registration = BrowserImageRegistrar::RegisterImage(image);
    auto* token = new base::UnguessableToken();
    *token = image_registration->token();
    window->SetProperty(property_key, token);
    return image_registration;
  };

  active_frame_image_registration_ =
      update_window_image(ash::kFrameImageActiveKey, GetFrameImage(true));
  inactive_frame_image_registration_ =
      update_window_image(ash::kFrameImageInactiveKey, GetFrameImage(false));
  active_frame_overlay_image_registration_ = update_window_image(
      ash::kFrameImageOverlayActiveKey, GetFrameOverlayImage(true));
  inactive_frame_overlay_image_registration_ = update_window_image(
      ash::kFrameImageOverlayInactiveKey, GetFrameOverlayImage(false));

  UpdateFrameColors();

  BrowserNonClientFrameView::OnThemeChanged();
}

void BrowserNonClientFrameViewAsh::ChildPreferredSizeChanged(
    views::View* child) {
  // TODO(estade): Do we need this in a world where Ash provides the header?
  if (IsMash())
    return;

  if (browser_view()->initialized()) {
    InvalidateLayout();
    frame()->GetRootView()->Layout();
  }
}

///////////////////////////////////////////////////////////////////////////////
// ash::CustomFrameHeader::AppearanceProvider:

SkColor BrowserNonClientFrameViewAsh::GetTitleColor() {
  return browser_view()->IsRegularOrGuestSession()
             ? kNormalWindowTitleTextColor
             : kIncognitoWindowTitleTextColor;
}

SkColor BrowserNonClientFrameViewAsh::GetFrameHeaderColor(bool active) {
  DCHECK(!IsMash());
  return GetFrameColor(active);
}

gfx::ImageSkia BrowserNonClientFrameViewAsh::GetFrameHeaderImage(bool active) {
  DCHECK(!IsMash());
  return GetFrameImage(active);
}

int BrowserNonClientFrameViewAsh::GetFrameHeaderImageYInset() {
  return ThemeProperties::kFrameHeightAboveTabs - GetTopInset(false);
}

gfx::ImageSkia BrowserNonClientFrameViewAsh::GetFrameHeaderOverlayImage(
    bool active) {
  DCHECK(!IsMash());
  return GetFrameOverlayImage(active);
}

bool BrowserNonClientFrameViewAsh::IsTabletMode() const {
  return TabletModeClient::Get() &&
         TabletModeClient::Get()->tablet_mode_enabled();
}

///////////////////////////////////////////////////////////////////////////////
// ash::ShellObserver:

void BrowserNonClientFrameViewAsh::OnOverviewModeStarting() {
  DCHECK(!IsMash());
  in_overview_mode_ = true;
  OnOverviewOrSplitviewModeChanged();
}

void BrowserNonClientFrameViewAsh::OnOverviewModeEnded() {
  DCHECK(!IsMash());
  in_overview_mode_ = false;
  OnOverviewOrSplitviewModeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// ash::mojom::TabletModeClient:

void BrowserNonClientFrameViewAsh::OnTabletModeToggled(bool enabled) {
  // TODO(estade): handle in Mash?
  if (!IsMash()) {
    if (!enabled && browser_view()->immersive_mode_controller()->IsRevealed()) {
      // Before updating the caption buttons state below (which triggers a
      // relayout), we want to move the caption buttons from the
      // TopContainerView back to this view.
      OnImmersiveRevealEnded();
    }

    caption_button_container_->SetVisible(ShouldShowCaptionButtons());
    caption_button_container_->UpdateCaptionButtonState(true /*=animate*/);
  }

  if (enabled) {
    // Enter immersive mode if the feature is enabled and the widget is not
    // already in fullscreen mode. Popups that are not activated but not
    // minimized are still put in immersive mode, since they may still be
    // visible but not activated due to something transparent and/or not
    // fullscreen (ie. fullscreen launcher).
    if (!frame()->IsFullscreen() && !browser_view()->IsBrowserTypeNormal() &&
        !frame()->IsMinimized()) {
      browser_view()->immersive_mode_controller()->SetEnabled(true);
      return;
    }
  } else {
    // Exit immersive mode if the feature is enabled and the widget is not in
    // fullscreen mode.
    if (!frame()->IsFullscreen() && !browser_view()->IsBrowserTypeNormal()) {
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

void BrowserNonClientFrameViewAsh::EnabledStateChangedForCommand(int id,
                                                                 bool enabled) {
  DCHECK_EQ(IDC_BACK, id);
  DCHECK(browser_view()->browser()->is_app());

  if (IsMash()) {
    frame()->GetNativeWindow()->SetProperty(
        ash::kFrameBackButtonStateKey,
        enabled ? ash::FrameBackButtonState::kEnabled
                : ash::FrameBackButtonState::kDisabled);
  } else if (back_button_) {
    back_button_->SetEnabled(enabled);
  }
}

void BrowserNonClientFrameViewAsh::OnSplitViewStateChanged(
    ash::mojom::SplitViewState current_state) {
  DCHECK(!IsMash());
  split_view_state_ = current_state;
  OnOverviewOrSplitviewModeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver:
// TODO(estade): remove this interface. Ash handles it for us with HeaderView.

void BrowserNonClientFrameViewAsh::OnWindowDestroying(aura::Window* window) {
  window_observer_.RemoveAll();
}

void BrowserNonClientFrameViewAsh::OnWindowPropertyChanged(aura::Window* window,
                                                           const void* key,
                                                           intptr_t old) {
  if (key != aura::client::kShowStateKey)
    return;

  if (IsMash()) {
    const ui::WindowShowState new_state =
        window->GetProperty(aura::client::kShowStateKey);
    if (new_state == ui::SHOW_STATE_NORMAL ||
        new_state == ui::SHOW_STATE_MAXIMIZED) {
      InvalidateLayout();
      frame()->GetRootView()->Layout();
    }
  } else {
    frame_header_->OnShowStateChanged(
        window->GetProperty(aura::client::kShowStateKey));
  }
}

///////////////////////////////////////////////////////////////////////////////
// ImmersiveModeController::Observer:

void BrowserNonClientFrameViewAsh::OnImmersiveRevealStarted() {
  // The frame caption buttons use ink drop highlights and flood fill effects.
  // They make those buttons paint_to_layer. On immersive mode, the browser's
  // TopContainerView is also converted to paint_to_layer (see
  // ImmersiveModeControllerAsh::OnImmersiveRevealStarted()). In this mode, the
  // TopContainerView is responsible for painting this
  // BrowserNonClientFrameViewAsh (see TopContainerView::PaintChildren()).
  // However, BrowserNonClientFrameViewAsh is a sibling of TopContainerView not
  // a child. As a result, when the frame caption buttons are set to
  // paint_to_layer as a result of an ink drop effect, they will disappear.
  // https://crbug.com/840242. To fix this, we'll make the caption buttons
  // temporarily children of the TopContainerView while they're all painting to
  // their layers.
  browser_view()->top_container()->AddChildViewAt(caption_button_container_, 0);
  if (back_button_)
    browser_view()->top_container()->AddChildViewAt(back_button_, 0);

  browser_view()->top_container()->Layout();
}

void BrowserNonClientFrameViewAsh::OnImmersiveRevealEnded() {
  AddChildViewAt(caption_button_container_, 0);
  if (back_button_)
    AddChildView(back_button_);
  Layout();
}

void BrowserNonClientFrameViewAsh::OnImmersiveFullscreenExited() {
  OnImmersiveRevealEnded();
}

HostedAppButtonContainer*
BrowserNonClientFrameViewAsh::GetHostedAppButtonContainerForTesting() const {
  return hosted_app_button_container_;
}

// static
bool BrowserNonClientFrameViewAsh::UsePackagedAppHeaderStyle(
    const Browser* browser) {
  // Use for non tabbed trusted source windows, e.g. Settings, as well as apps.
  return (!browser->is_type_tabbed() && browser->is_trusted_source()) ||
         browser->is_app();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, protected:

// BrowserNonClientFrameView:
AvatarButtonStyle BrowserNonClientFrameViewAsh::GetAvatarButtonStyle() const {
  // Ash doesn't support a profile switcher button.
  return AvatarButtonStyle::NONE;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewAsh, private:

bool BrowserNonClientFrameViewAsh::ShouldShowCaptionButtons() const {
  DCHECK(!IsMash());

  // In tablet mode, to prevent accidental taps of the window controls, and to
  // give more horizontal space for tabs and the new tab button especially in
  // splitscreen view, we hide the window controls. We only do this when the
  // Home Launcher feature is enabled, since it gives the user the ability to
  // minimize all windows when pressing the Launcher button on the shelf.
  if (app_list::features::IsHomeLauncherEnabled() && IsTabletMode() &&
      !browser_view()->browser()->is_app()) {
    return false;
  }

  return !in_overview_mode_ ||
         IsSnappedInSplitView(frame()->GetNativeWindow(), split_view_state_);
}

int BrowserNonClientFrameViewAsh::GetTabStripRightInset() const {
  int inset = IsMash() ? frame_values().normal_insets.right() +
                             frame_values().max_title_bar_button_width
                       : caption_button_container_->GetPreferredSize().width();

  // For Material Refresh, the end of the tabstrip contains empty space to
  // ensure the window remains draggable, which is sufficient padding to the
  // other tabstrip contents.
  constexpr int kTabstripRightSpacing = 10;
  if (!ui::MaterialDesignController::IsRefreshUi())
    inset += kTabstripRightSpacing;

  return inset;
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

void BrowserNonClientFrameViewAsh::OnOverviewOrSplitviewModeChanged() {
  DCHECK(!IsMash());

  caption_button_container_->SetVisible(ShouldShowCaptionButtons());

  // Schedule a paint to show or hide the header.
  SchedulePaint();
}

std::unique_ptr<ash::FrameHeader>
BrowserNonClientFrameViewAsh::CreateFrameHeader() {
  DCHECK(!IsMash());

  std::unique_ptr<ash::FrameHeader> header;
  Browser* browser = browser_view()->browser();
  if (!UsePackagedAppHeaderStyle(browser)) {
    header = std::make_unique<ash::CustomFrameHeader>(
        frame(), this, this, caption_button_container_);
  } else {
    auto default_frame_header = std::make_unique<ash::DefaultFrameHeader>(
        frame(), this, caption_button_container_);
    if (extensions::HostedAppBrowserController::
            IsForExperimentalHostedAppBrowser(browser)) {
      SetUpForHostedApp(default_frame_header.get());
    } else if (!browser->is_app()) {
      default_frame_header->SetFrameColors(kMdWebUiFrameColor,
                                           kMdWebUiFrameColor);
    }
    header = std::move(default_frame_header);
  }

  header->SetBackButton(back_button_);
  header->SetLeftHeaderView(window_icon_);
  return header;
}

void BrowserNonClientFrameViewAsh::SetUpForHostedApp(
    ash::DefaultFrameHeader* header) {
  SkColor active_color = ash::FrameCaptionButton::GetButtonColor(
      ash::FrameCaptionButton::ColorMode::kDefault, ash::kDefaultFrameColor);

  // Hosted apps apply a theme color if specified by the extension.
  Browser* browser = browser_view()->browser();
  base::Optional<SkColor> theme_color =
      browser->hosted_app_controller()->GetThemeColor();
  if (theme_color) {
    // Not necessary in Mash as the frame colors are set in OnThemeChanged().
    if (!IsMash()) {
      frame()->GetNativeWindow()->SetProperty(
          ash::kFrameIsThemedByHostedAppKey,
          !!browser->hosted_app_controller()->GetThemeColor());
      header->SetFrameColors(*theme_color, *theme_color);
    }

    active_color = ash::FrameCaptionButton::GetButtonColor(
        ash::FrameCaptionButton::ColorMode::kThemed, *theme_color);
  }

  // Add the container for extra hosted app buttons (e.g app menu button).
  const float inactive_alpha_ratio =
      ash::FrameCaptionButton::GetInactiveButtonColorAlphaRatio();
  SkColor inactive_color =
      SkColorSetA(active_color, 255 * inactive_alpha_ratio);
  hosted_app_button_container_ = new HostedAppButtonContainer(
      frame(), browser_view(), active_color, inactive_color);
  AddChildView(hosted_app_button_container_);
}

void BrowserNonClientFrameViewAsh::UpdateFrameColors() {
  aura::Window* window = frame()->GetNativeWindow();
  base::Optional<SkColor> active_color, inactive_color;
  if (!UsePackagedAppHeaderStyle(browser_view()->browser())) {
    active_color = GetFrameColor(true);
    inactive_color = GetFrameColor(false);
  } else if (extensions::HostedAppBrowserController::
                 IsForExperimentalHostedAppBrowser(browser_view()->browser())) {
    active_color =
        browser_view()->browser()->hosted_app_controller()->GetThemeColor();
    window->SetProperty(ash::kFrameIsThemedByHostedAppKey, !!active_color);
  } else {
    active_color = kMdWebUiFrameColor;
  }

  if (active_color) {
    window->SetProperty(ash::kFrameActiveColorKey, *active_color);
    window->SetProperty(ash::kFrameInactiveColorKey,
                        inactive_color.value_or(*active_color));
  } else {
    window->ClearProperty(ash::kFrameActiveColorKey);
    window->ClearProperty(ash::kFrameInactiveColorKey);
  }
}
