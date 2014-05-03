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
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {

// Revealing the TopContainerView looks better if the animation starts and ends
// just a few pixels before the view goes offscreen, which reduces the visual
// "pop" as the 3-pixel tall "light bar" style tab strip becomes visible.
const int kAnimationOffsetY = 3;

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

  controller_->SetEnabled(browser_view_->browser()->is_app() ?
          ash::ImmersiveFullscreenController::WINDOW_TYPE_HOSTED_APP :
          ash::ImmersiveFullscreenController::WINDOW_TYPE_BROWSER
      , enabled);
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
  widget->non_client_view()->frame_view()->InvalidateLayout();
  browser_view_->InvalidateLayout();
  widget->GetRootView()->Layout();
}

bool ImmersiveModeControllerAsh::UpdateTabIndicators() {
  bool has_tabstrip = browser_view_->IsBrowserTypeNormal();
  if (!IsEnabled() || !has_tabstrip) {
    use_tab_indicators_ = false;
  } else {
    bool in_tab_fullscreen = browser_view_->browser()->fullscreen_controller()->
        IsWindowFullscreenForTabOrPending();
    use_tab_indicators_ = !in_tab_fullscreen;
  }

  bool show_tab_indicators = use_tab_indicators_ && !IsRevealed();
  if (show_tab_indicators != browser_view_->tabstrip()->IsImmersiveStyle()) {
    browser_view_->tabstrip()->SetImmersiveStyle(show_tab_indicators);
    return true;
  }
  return false;
}

void ImmersiveModeControllerAsh::OnImmersiveRevealStarted() {
  visible_fraction_ = 0;
  browser_view_->top_container()->SetPaintToLayer(true);
  browser_view_->top_container()->SetFillsBoundsOpaquely(false);
  UpdateTabIndicators();
  LayoutBrowserRootView();
  FOR_EACH_OBSERVER(Observer, observers_, OnImmersiveRevealStarted());
}

void ImmersiveModeControllerAsh::OnImmersiveRevealEnded() {
  visible_fraction_ = 0;
  browser_view_->top_container()->SetPaintToLayer(false);
  UpdateTabIndicators();
  LayoutBrowserRootView();
}

void ImmersiveModeControllerAsh::OnImmersiveFullscreenExited() {
  browser_view_->top_container()->SetPaintToLayer(false);
  UpdateTabIndicators();
  LayoutBrowserRootView();
}

void ImmersiveModeControllerAsh::SetVisibleFraction(double visible_fraction) {
  if (visible_fraction_ != visible_fraction) {
    visible_fraction_ = visible_fraction;
    browser_view_->Layout();
  }
}

std::vector<gfx::Rect>
ImmersiveModeControllerAsh::GetVisibleBoundsInScreen() const {
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

void ImmersiveModeControllerAsh::OnPostWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::wm::WindowStateType old_type) {
  // Disable immersive fullscreen when the user exits fullscreen without going
  // through FullscreenController::ToggleBrowserFullscreenMode(). This is the
  // case if the user exits fullscreen via the restore button.
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

  bool tab_indicator_visibility_changed = UpdateTabIndicators();

  // Auto hide the shelf in immersive browser fullscreen. When auto hidden, the
  // shelf displays a 3px 'light bar' when it is closed. When in immersive
  // browser fullscreen and tab fullscreen, hide the shelf completely and
  // prevent it from being revealed.
  bool in_tab_fullscreen = content::Source<FullscreenController>(source)->
      IsWindowFullscreenForTabOrPending();
  ash::wm::GetWindowState(native_window_)->set_hide_shelf_when_fullscreen(
      in_tab_fullscreen);
  ash::Shell::GetInstance()->UpdateShelfVisibility();

  if (tab_indicator_visibility_changed)
    LayoutBrowserRootView();
}
