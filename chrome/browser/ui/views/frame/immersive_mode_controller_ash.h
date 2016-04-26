// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_

#include <memory>

#include "ash/wm/common/window_state_observer.h"
#include "ash/wm/immersive_fullscreen_controller.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

class ImmersiveModeControllerAsh
    : public ImmersiveModeController,
      public ash::ImmersiveFullscreenController::Delegate,
      public ash::wm::WindowStateObserver,
      public content::NotificationObserver {
 public:
  ImmersiveModeControllerAsh();
  ~ImmersiveModeControllerAsh() override;

  // ImmersiveModeController overrides:
  void Init(BrowserView* browser_view) override;
  void SetEnabled(bool enabled) override;
  bool IsEnabled() const override;
  bool ShouldHideTabIndicators() const override;
  bool ShouldHideTopViews() const override;
  bool IsRevealed() const override;
  int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const override;
  ImmersiveRevealedLock* GetRevealedLock(AnimateReveal animate_reveal) override
      WARN_UNUSED_RESULT;
  void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) override;
  void SetupForTest() override;

 private:
  // Enables or disables observers for window restore and entering / exiting
  // tab fullscreen.
  void EnableWindowObservers(bool enable);

  // Updates the browser root view's layout including window caption controls.
  void LayoutBrowserRootView();

  // Updates whether the tab strip is painted in a short "light bar" style.
  // Returns true if the visibility of the tab indicators has changed.
  bool UpdateTabIndicators();

  // ImmersiveFullscreenController::Delegate overrides:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenExited() override;
  void SetVisibleFraction(double visible_fraction) override;
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override;

  // ash::wm::WindowStateObserver override:
  void OnPostWindowStateTypeChange(ash::wm::WindowState* window_state,
                                   ash::wm::WindowStateType old_type) override;

  // content::NotificationObserver override:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  std::unique_ptr<ash::ImmersiveFullscreenController> controller_;

  // Not owned.
  BrowserView* browser_view_;
  aura::Window* native_window_;

  // True if the observers for window restore and entering / exiting tab
  // fullscreen are enabled.
  bool observers_enabled_;

  // Whether a short "light bar" version of the tab strip should be painted when
  // the top-of-window views are closed. If |use_tab_indicators_| is false, the
  // tab strip is not painted at all when the top-of-window views are closed.
  bool use_tab_indicators_;

  // The current visible bounds of the find bar, in screen coordinates. This is
  // an empty rect if the find bar is not visible.
  gfx::Rect find_bar_visible_bounds_in_screen_;

  // The fraction of the TopContainerView's height which is visible. Zero when
  // the top-of-window views are not revealed regardless of
  // |use_tab_indicators_|.
  double visible_fraction_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
