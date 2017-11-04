// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mus.h"

#include <algorithm>

#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_frame_mus.h"
#include "chrome/browser/ui/views/frame/browser_header_painter_ash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/web_applications/web_app.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/window_manager_frame_values.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_CHROMEOS)
#include "ash/ash_layout_constants.h"
#endif

#if !defined(OS_CHROMEOS)
#define FRAME_AVATAR_BUTTON
#endif

namespace {

#if defined(FRAME_AVATAR_BUTTON)
// Space between the new avatar button and the minimize button.
const int kAvatarButtonOffset = 5;
#endif
// Space between right edge of tabstrip and maximize button.
const int kTabstripRightSpacing = 10;
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
#if defined(FRAME_AVATAR_BUTTON)
      profile_switcher_(this),
#endif
      tab_strip_(nullptr) {
}

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

  int left_inset = GetTabStripLeftInset();
  int right_inset = GetTabStripRightInset();
  const gfx::Rect bounds(left_inset, GetTopInset(false),
                         std::max(0, width() - left_inset - right_inset),
                         tabstrip->GetPreferredSize().height());
  return bounds;
}

int BrowserNonClientFrameViewMus::GetTopInset(bool restored) const {
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

  const int header_height = GetHeaderHeight();

  if (browser_view()->IsTabStripVisible())
    return header_height - browser_view()->GetTabStripHeight();

  return header_height;
}

int BrowserNonClientFrameViewMus::GetThemeBackgroundXInset() const {
  return 5;
}

void BrowserNonClientFrameViewMus::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

views::View* BrowserNonClientFrameViewMus::GetProfileSwitcherView() const {
#if defined(FRAME_AVATAR_BUTTON)
  return profile_switcher_.view();
#else
  return nullptr;
#endif
}

void BrowserNonClientFrameViewMus::UpdateClientArea() {
  std::vector<gfx::Rect> additional_client_area;
  int top_container_offset = 0;
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  // Frame decorations (the non-client area) are visible if not in immersive
  // mode, or in immersive mode *and* the reveal widget is showing.
  const bool show_frame_decorations = !immersive_mode_controller->IsEnabled() ||
                                      immersive_mode_controller->IsRevealed();
  if (browser_view()->IsTabStripVisible() && show_frame_decorations) {
    gfx::Rect tab_strip_bounds(GetBoundsForTabStrip(tab_strip_));

    int tab_strip_max_x = tab_strip_->GetMaxX();
    if (!tab_strip_bounds.IsEmpty() && tab_strip_max_x) {
      tab_strip_bounds.set_width(tab_strip_max_x);
      if (immersive_mode_controller->IsEnabled()) {
        top_container_offset =
            immersive_mode_controller->GetTopContainerVerticalOffset(
                browser_view()->top_container()->size());
        tab_strip_bounds.set_y(tab_strip_bounds.y() + top_container_offset);
        tab_strip_bounds.Intersect(gfx::Rect(size()));
      }
      additional_client_area.push_back(tab_strip_bounds);
    }
  }
  aura::WindowTreeHostMus* window_tree_host_mus =
      static_cast<aura::WindowTreeHostMus*>(
          GetWidget()->GetNativeWindow()->GetHost());
  if (show_frame_decorations) {
    const int header_height = GetHeaderHeight();
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

void BrowserNonClientFrameViewMus::UpdateMinimumSize() {
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
  if (hit_test == HTCAPTION && profile_switcher_.view() &&
      ConvertedHitTest(this, profile_switcher_.view(), point)) {
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

  if (browser_view()->IsToolbarVisible())
    PaintToolbarBackground(canvas);
  else if (!UsePackagedAppHeaderStyle())
    PaintContentEdge(canvas);
}

void BrowserNonClientFrameViewMus::Layout() {
  if (profile_indicator_icon())
    LayoutIncognitoButton();

#if defined(FRAME_AVATAR_BUTTON)
  if (profile_switcher_.view())
    LayoutProfileSwitcher();
#endif

  BrowserNonClientFrameView::Layout();

  UpdateClientArea();
}

const char* BrowserNonClientFrameViewMus::GetClassName() const {
  return kViewClassName;
}

void BrowserNonClientFrameViewMus::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_TITLE_BAR;
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
void BrowserNonClientFrameViewMus::UpdateProfileIcons() {
#if defined(FRAME_AVATAR_BUTTON)
  if (browser_view()->IsRegularOrGuestSession()) {
    profile_switcher_.Update(AvatarButtonStyle::NATIVE);
    return;
  }
#endif
  Browser* browser = browser_view()->browser();

  // Similar logic as in BrowserNonClientFrameViewAsh::UpdateProfileIcons (minus
  // the multi-profile part). That is, no profile indicator for non-tabbed and
  // non-app browser window, or regular/guest user browser window.
  if (!browser->is_type_tabbed() && !browser->is_app())
    return;
  if (browser_view()->IsRegularOrGuestSession())
    return;

  UpdateProfileIndicatorIcon();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMus, private:

void BrowserNonClientFrameViewMus::TabStripMaxXChanged(TabStrip* tab_strip) {
  UpdateClientArea();
}

void BrowserNonClientFrameViewMus::TabStripDeleted(TabStrip* tab_strip) {
  tab_strip_->RemoveObserver(this);
  tab_strip_ = nullptr;
}

int BrowserNonClientFrameViewMus::GetTabStripLeftInset() const {
  const int avatar_right =
      profile_indicator_icon()
          ? (kAvatarIconPadding + GetIncognitoAvatarIcon().width())
          : 0;
  return avatar_right + kAvatarIconPadding +
         frame_values().normal_insets.left();
}

int BrowserNonClientFrameViewMus::GetTabStripRightInset() const {
  const int frame_right_insets = frame_values().normal_insets.right() +
                                 frame_values().max_title_bar_button_width;
  int right_inset = kTabstripRightSpacing + frame_right_insets;

#if defined(FRAME_AVATAR_BUTTON)
  if (profile_switcher_.view()) {
    right_inset += kAvatarButtonOffset +
                   profile_switcher_.view()->GetPreferredSize().width();
  }
#endif

  return right_inset;
}

bool BrowserNonClientFrameViewMus::UsePackagedAppHeaderStyle() const {
  // Use for non tabbed trusted source windows, e.g. Settings, as well as apps.
  const Browser* const browser = browser_view()->browser();
  return (!browser->is_type_tabbed() && browser->is_trusted_source()) ||
         browser->is_app();
}

void BrowserNonClientFrameViewMus::LayoutIncognitoButton() {
  DCHECK(profile_indicator_icon());
#if !defined(OS_CHROMEOS)
  // ChromeOS shows avatar on V1 app.
  DCHECK(browser_view()->IsTabStripVisible());
#endif
  gfx::ImageSkia incognito_icon = GetIncognitoAvatarIcon();
  int avatar_bottom = GetTopInset(false) + browser_view()->GetTabStripHeight() -
                      kAvatarIconPadding;
  int avatar_y = avatar_bottom - incognito_icon.height();
  int avatar_height = incognito_icon.height();

  gfx::Rect avatar_bounds(kAvatarIconPadding, avatar_y, incognito_icon.width(),
                          avatar_height);
  profile_indicator_icon()->SetBoundsRect(avatar_bounds);
  profile_indicator_icon()->SetVisible(true);
}

void BrowserNonClientFrameViewMus::LayoutProfileSwitcher() {
#if defined(FRAME_AVATAR_BUTTON)
  gfx::Size button_size = profile_switcher_.view()->GetPreferredSize();
  int button_x = width() - GetTabStripRightInset() + kAvatarButtonOffset;
  profile_switcher_.view()->SetBounds(button_x, 0, button_size.width(),
                                      button_size.height());
#endif
}

bool BrowserNonClientFrameViewMus::ShouldPaint() const {
  if (!frame()->IsFullscreen())
    return true;

  // We need to paint when the top-of-window views are revealed in immersive
  // fullscreen.
  ImmersiveModeController* immersive_mode_controller =
      browser_view()->immersive_mode_controller();
  return immersive_mode_controller->IsEnabled() &&
         immersive_mode_controller->IsRevealed();
}

void BrowserNonClientFrameViewMus::PaintContentEdge(gfx::Canvas* canvas) {
  DCHECK(!UsePackagedAppHeaderStyle());
  const int bottom = frame_values().normal_insets.bottom();
  canvas->FillRect(
      gfx::Rect(0, bottom, width(), kClientEdgeThickness),
      GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR));
}

int BrowserNonClientFrameViewMus::GetHeaderHeight() const {
#if defined(OS_CHROMEOS)
  // TODO: move ash_layout_constants to ash/public/cpp.
  const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
  return GetAshLayoutSize(restored
                              ? AshLayoutSize::BROWSER_RESTORED_CAPTION_BUTTON
                              : AshLayoutSize::BROWSER_MAXIMIZED_CAPTION_BUTTON)
      .height();
#else
  return views::WindowManagerFrameValues::instance().normal_insets.top();
#endif
}
