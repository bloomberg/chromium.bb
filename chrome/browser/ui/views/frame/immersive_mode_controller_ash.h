// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_

#include <memory>

#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_delegate.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

// See ash/mus/frame/README.md for description of how immersive mode works in
// mash. This code works with both classic ash, and mash.
class ImmersiveModeControllerAsh
    : public ImmersiveModeController,
      public ash::ImmersiveFullscreenControllerDelegate,
      public content::NotificationObserver,
      public aura::WindowObserver {
 public:
  ImmersiveModeControllerAsh();
  ~ImmersiveModeControllerAsh() override;

  ash::ImmersiveFullscreenController* controller() { return controller_.get(); }

  // ImmersiveModeController overrides:
  void Init(BrowserView* browser_view) override;
  void SetEnabled(bool enabled) override;
  bool IsEnabled() const override;
  bool ShouldHideTopViews() const override;
  bool IsRevealed() const override;
  int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const override;
  ImmersiveRevealedLock* GetRevealedLock(AnimateReveal animate_reveal) override
      WARN_UNUSED_RESULT;
  void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) override;
  bool ShouldStayImmersiveAfterExitingFullscreen() override;
  views::Widget* GetRevealWidget() override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

 private:
  // Enables or disables observers for window restore and entering / exiting
  // tab fullscreen.
  void EnableWindowObservers(bool enable);

  // Updates the browser root view's layout including window caption controls.
  void LayoutBrowserRootView();

  // Used when running in mash to create |mash_reveal_widget_|. Does nothing
  // if already null.
  void CreateMashRevealWidget();

  // Destroys |mash_reveal_widget_| if valid, does nothing otherwise.
  void DestroyMashRevealWidget();

  // ImmersiveFullscreenController::Delegate overrides:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenExited() override;
  void SetVisibleFraction(double visible_fraction) override;
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override;

  // content::NotificationObserver override:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // aura::WindowObserver override:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  std::unique_ptr<ash::ImmersiveFullscreenController> controller_;

  // Not owned.
  BrowserView* browser_view_;
  aura::Window* native_window_;

  // True if the observers for window restore and entering / exiting tab
  // fullscreen are enabled.
  bool observers_enabled_;

  // The current visible bounds of the find bar, in screen coordinates. This is
  // an empty rect if the find bar is not visible.
  gfx::Rect find_bar_visible_bounds_in_screen_;

  // The fraction of the TopContainerView's height which is visible. Zero when
  // the top-of-window views are not revealed.
  double visible_fraction_;

  // When running in mash a widget is created to draw the top container. This
  // widget does not actually contain the top container, it just renders it.
  std::unique_ptr<views::Widget> mash_reveal_widget_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_ASH_H_
