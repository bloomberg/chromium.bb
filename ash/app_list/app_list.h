// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_H_
#define ASH_APP_LIST_APP_LIST_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/root_window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AppListView;

namespace internal {

// AppList is a controller that manages app list UI for shell. To show the UI,
// it requests app list widget from ShellDelegate and shows it when ready.
// While the UI is visible, it monitors things such as app list widget's
// activation state and desktop mouse click to auto dismiss the UI.
class AppList : public aura::EventFilter,
                public aura::RootWindowObserver,
                public ui::ImplicitAnimationObserver,
                public views::Widget::Observer {
 public:
  AppList();
  virtual ~AppList();

  // Returns true if AppListV2 is enabled.
  static bool UseAppListV2();

  // Show/hide app list window.
  void SetVisible(bool visible);

  // Whether app list window is visible (shown or being shown).
  bool IsVisible();

  // Returns target visibility. This differs from IsVisible() if an animation
  // is ongoing.
  bool GetTargetVisibility() const { return is_visible_; }

  // Returns app list window or NULL if it is not visible.
  aura::Window* GetWindow();

 private:
  // Sets app list view. If we are in visible mode, start showing animation.
  // Otherwise, we just close it.
  void SetView(AppListView* view);

  // Forgets the view.
  void ResetView();

  // Starts show/hide animation. ScheduleAnimation is the master who manages
  // when to call sub animations. There are three sub animations: background
  // dimming, browser windows scale/fade and app list scale/fade. The background
  // dimming runs in parallel with the other two and spans the whole animation
  // time. The rest sub animations run in two steps. On showing, the first step
  // is browser windows scale-out and fade-out and the 2nd step is app list
  // scale-in and fade-in. The 2nd step animation is started via a timer and
  // there is is a little overlap between the two animations. Hiding animation
  // is the reverse of the showing animation.
  void ScheduleAnimation();

  void ScheduleBrowserWindowsAnimationForContainer(aura::Window* container);
  void ScheduleBrowserWindowsAnimation();
  void ScheduleDimmingAnimation();
  void ScheduleAppListAnimation();

  // aura::EventFilter overrides:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

  // aura::RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& old_size) OVERRIDE;
  virtual void OnWindowFocused(aura::Window* window) OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // views::Widget::Observer overrides:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // Whether we should show or hide app list widget.
  bool is_visible_;

  // The AppListView this class manages, owned by its widget.
  AppListView* view_;

  // Timer to schedule the 2nd step animation, started when the first step
  // animation is scheduled in ScheduleAnimation.
  base::OneShotTimer<AppList> second_animation_timer_;

  DISALLOW_COPY_AND_ASSIGN(AppList);
};

}  // namespace internal
}  // namespace ash

#endif  //  ASH_APP_LIST_APP_LIST_H_
