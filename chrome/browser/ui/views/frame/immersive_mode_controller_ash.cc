// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {

// Revealing the TopContainerView looks better if the animation starts and ends
// just a few pixels before the view goes offscreen, which reduces the visual
// "pop" as the 3-pixel tall "light bar" style tab strip becomes visible.
const int kAnimationOffsetY = 3;

// The height of the region in pixels at the top edge of the screen in which to
// steal touch events targetted at the web contents while in immersive
// fullscreen. This region is used to allow us to get edge gestures even if the
// web contents consumes all touch events.
const int kStealTouchEventsFromWebContentsRegionHeightPx = 8;

// Converts from ImmersiveModeController::AnimateReveal to
// ash::ImmersiveFullscreenController::AnimateReveal.
ash::ImmersiveFullscreenController::AnimateReveal
ToImmersiveFullscreenControllerAnimateReveal(
    ImmersiveModeController::AnimateReveal animate_reveal) {
  switch (animate_reveal) {
    case ImmersiveModeController::ANIMATE_REVEAL_YES:
      return ash::ImmersiveFullscreenController::ANIMATE_REVEAL_YES;
    case ImmersiveModeController::ANIMATE_REVEAL_NO:
      return ash::ImmersiveFullscreenController::ANIMATE_REVEAL_NO;
  }
  NOTREACHED();
  return ash::ImmersiveFullscreenController::ANIMATE_REVEAL_NO;
}

}  // namespace

ImmersiveModeControllerAsh::ImmersiveModeControllerAsh()
    : controller_(new ash::ImmersiveFullscreenController),
      browser_view_(NULL),
      native_window_(NULL),
      observers_enabled_(false),
      use_tab_indicators_(false),
      visible_fraction_(1) {
}

ImmersiveModeControllerAsh::~ImmersiveModeControllerAsh() {
  EnableWindowObservers(false);
}

void ImmersiveModeControllerAsh::Init(BrowserView* browser_view) {
  browser_view_ = browser_view;
  native_window_ = browser_view_->GetNativeWindow();
  controller_->Init(this, browser_view_->frame(),
      browser_view_->top_container());
}

void ImmersiveModeControllerAsh::SetEnabled(bool enabled) {
  if (controller_->IsEnabled() == enabled)
    return;

  EnableWindowObservers(enabled);

  // Use a short "light bar" version of the tab strip when the top-of-window
  // views are closed. If the user additionally enters into tab fullscreen,
  // the tab indicators will be hidden.
  use_tab_indicators_ = enabled;

  controller_->SetEnabled(enabled);
}

bool ImmersiveModeControllerAsh::IsEnabled() const {
  return controller_->IsEnabled();
}

bool ImmersiveModeControllerAsh::ShouldHideTabIndicators() const {
  return !use_tab_indicators_;
}

bool ImmersiveModeControllerAsh::ShouldHideTopViews() const {
  return controller_->IsEnabled() && !controller_->IsRevealed();
}

bool ImmersiveModeControllerAsh::IsRevealed() const {
  return controller_->IsRevealed();
}

int ImmersiveModeControllerAsh::GetTopContainerVerticalOffset(
    const gfx::Size& top_container_size) const {
  if (!IsEnabled())
    return 0;

  // The TopContainerView is flush with the top of |browser_view_| when the
  // top-of-window views are fully closed so that when the tab indicators are
  // used, the "light bar" style tab strip is flush with the top of
  // |browser_view_|.
  if (!IsRevealed())
    return 0;

  int height = top_container_size.height() - kAnimationOffsetY;
  return static_cast<int>(height * (visible_fraction_ - 1));
}

ImmersiveRevealedLock* ImmersiveModeControllerAsh::GetRevealedLock(
    AnimateReveal animate_reveal) {
  return controller_->GetRevealedLock(
      ToImmersiveFullscreenControllerAnimateReveal(animate_reveal));
}

void ImmersiveModeControllerAsh::OnFindBarVisibleBoundsChanged(
    const gfx::Rect& new_visible_bounds_in_screen) {
  find_bar_visible_bounds_in_screen_ = new_visible_bounds_in_screen;
}

void ImmersiveModeControllerAsh::SetupForTest() {
  controller_->SetupForTest();
}

void ImmersiveModeControllerAsh::EnableWindowObservers(bool enable) {
  if (observers_enabled_ == enable)
    return;
  observers_enabled_ = enable;

  content::Source<FullscreenController> source(
      browser_view_->browser()->fullscreen_controller());
  if (enable) {
    ash::wm::GetWindowState(native_window_)->AddObserver(this);
    registrar_.Add(this, chrome::NOTIFICATION_FULLSCREEN_CHANGED, source);
  } else {
    ash::wm::GetWindowState(native_window_)->RemoveObserver(this);
    registrar_.Remove(this, chrome::NOTIFICATION_FULLSCREEN_CHANGED, source);
  }
}

void ImmersiveModeControllerAsh::LayoutBrowserRootView() {
  views::Widget* widget = browser_view_->frame();
  // Update the window caption buttons.
  widget->non_client_view()->frame_view()->ResetWindowControls();
  // Layout all views, including BrowserView.
  widget->GetRootView()->Layout();
}

void ImmersiveModeControllerAsh::SetRenderWindowTopInsetsForTouch(
    int top_inset) {
  content::WebContents* contents = browser_view_->GetActiveWebContents();
  if (contents) {
    aura::Window* window = contents->GetView()->GetContentNativeView();
    // |window| is NULL if the renderer crashed.
    if (window) {
      gfx::Insets inset(top_inset, 0, 0, 0);
      window->SetHitTestBoundsOverrideOuter(
          window->hit_test_bounds_override_outer_mouse(),
          inset);
    }
  }
}

void ImmersiveModeControllerAsh::SetTabIndicatorsVisible(bool visible) {
  DCHECK(!visible || use_tab_indicators_);
  if (browser_view_->tabstrip())
    browser_view_->tabstrip()->SetImmersiveStyle(visible);
}

void ImmersiveModeControllerAsh::OnImmersiveRevealStarted() {
  visible_fraction_ = 0;
  browser_view_->top_container()->SetPaintToLayer(true);
  SetTabIndicatorsVisible(false);
  SetRenderWindowTopInsetsForTouch(0);
  LayoutBrowserRootView();
  FOR_EACH_OBSERVER(Observer, observers_, OnImmersiveRevealStarted());
}

void ImmersiveModeControllerAsh::OnImmersiveRevealEnded() {
  visible_fraction_ = 0;
  browser_view_->top_container()->SetPaintToLayer(false);
  SetTabIndicatorsVisible(use_tab_indicators_);
  SetRenderWindowTopInsetsForTouch(
      kStealTouchEventsFromWebContentsRegionHeightPx);
  LayoutBrowserRootView();
}

void ImmersiveModeControllerAsh::OnImmersiveFullscreenExited() {
  browser_view_->top_container()->SetPaintToLayer(false);
  SetTabIndicatorsVisible(false);
  SetRenderWindowTopInsetsForTouch(0);
  LayoutBrowserRootView();
}

void ImmersiveModeControllerAsh::SetVisibleFraction(double visible_fraction) {
  if (visible_fraction_ != visible_fraction) {
    visible_fraction_ = visible_fraction;
    browser_view_->Layout();
  }
}

std::vector<gfx::Rect>
ImmersiveModeControllerAsh::GetVisibleBoundsInScreen() {
  views::View* top_container_view = browser_view_->top_container();
  gfx::Rect top_container_view_bounds = top_container_view->GetVisibleBounds();
  // TODO(tdanderson): Implement View::ConvertRectToScreen().
  gfx::Point top_container_view_bounds_in_screen_origin(
      top_container_view_bounds.origin());
  views::View::ConvertPointToScreen(top_container_view,
      &top_container_view_bounds_in_screen_origin);
  gfx::Rect top_container_view_bounds_in_screen(
      top_container_view_bounds_in_screen_origin,
      top_container_view_bounds.size());

  std::vector<gfx::Rect> bounds_in_screen;
  bounds_in_screen.push_back(top_container_view_bounds_in_screen);
  bounds_in_screen.push_back(find_bar_visible_bounds_in_screen_);
  return bounds_in_screen;
}

void ImmersiveModeControllerAsh::OnWindowShowTypeChanged(
    ash::wm::WindowState* window_state,
    ash::wm::WindowShowType old_type) {
  // Disable immersive fullscreen when the user exits fullscreen without going
  // through FullscreenController::ToggleFullscreenMode(). This is the case if
  // the user exits fullscreen via the restore button.
  if (controller_->IsEnabled() &&
      !window_state->IsFullscreen() &&
      !window_state->IsMinimized()) {
    browser_view_->FullscreenStateChanged();
  }
}

void ImmersiveModeControllerAsh::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_FULLSCREEN_CHANGED, type);
  if (!controller_->IsEnabled())
    return;

  bool in_tab_fullscreen = content::Source<FullscreenController>(source)->
      IsFullscreenForTabOrPending();

  bool used_tab_indicators = use_tab_indicators_;
  use_tab_indicators_ = !in_tab_fullscreen;
  SetTabIndicatorsVisible(use_tab_indicators_ && !controller_->IsRevealed());

  // Auto hide the shelf in immersive browser fullscreen. When auto hidden, the
  // shelf displays a 3px 'light bar' when it is closed. When in immersive
  // browser fullscreen and tab fullscreen, hide the shelf completely and
  // prevent it from being revealed.
  ash::wm::GetWindowState(native_window_)->set_hide_shelf_when_fullscreen(
      in_tab_fullscreen);
  ash::Shell::GetInstance()->UpdateShelfVisibility();

  if (use_tab_indicators_ != used_tab_indicators)
    LayoutBrowserRootView();
}
