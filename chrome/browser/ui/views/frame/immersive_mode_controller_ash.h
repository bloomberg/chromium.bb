// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_

#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

#include "ash/wm/immersive_fullscreen_controller.h"
#include "ash/wm/window_state_observer.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/rect.h"

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
  virtual ~ImmersiveModeControllerAsh();

  // ImmersiveModeController overrides:
  virtual void Init(BrowserView* browser_view) OVERRIDE;
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual bool IsEnabled() const OVERRIDE;
  virtual bool ShouldHideTabIndicators() const OVERRIDE;
  virtual bool ShouldHideTopViews() const OVERRIDE;
  virtual bool IsRevealed() const OVERRIDE;
  virtual int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const OVERRIDE;
  virtual ImmersiveRevealedLock* GetRevealedLock(
      AnimateReveal animate_reveal) OVERRIDE WARN_UNUSED_RESULT;
  virtual void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) OVERRIDE;
  virtual void SetupForTest() OVERRIDE;

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
  virtual void OnImmersiveRevealStarted() OVERRIDE;
  virtual void OnImmersiveRevealEnded() OVERRIDE;
  virtual void OnImmersiveFullscreenExited() OVERRIDE;
  virtual void SetVisibleFraction(double visible_fraction) OVERRIDE;
  virtual std::vector<gfx::Rect> GetVisibleBoundsInScreen() const OVERRIDE;

  // ash::wm::WindowStateObserver override:
  virtual void OnPostWindowStateTypeChange(
      ash::wm::WindowState* window_state,
      ash::wm::WindowStateType old_type) OVERRIDE;

  // content::NotificationObserver override:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  scoped_ptr<ash::ImmersiveFullscreenController> controller_;

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
