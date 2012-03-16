// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_property.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"

DECLARE_WINDOW_PROPERTY_TYPE(int)
DECLARE_WINDOW_PROPERTY_TYPE(ash::WindowVisibilityAnimationType)
DECLARE_WINDOW_PROPERTY_TYPE(ash::WindowVisibilityAnimationTransition)

using base::TimeDelta;

namespace ash {
namespace internal {
DEFINE_WINDOW_PROPERTY_KEY(WindowVisibilityAnimationType,
                           kWindowVisibilityAnimationTypeKey,
                           WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);
DEFINE_WINDOW_PROPERTY_KEY(int, kWindowVisibilityAnimationDurationKey, 0);
DEFINE_WINDOW_PROPERTY_KEY(WindowVisibilityAnimationTransition,
                           kWindowVisibilityAnimationTransitionKey,
                           ANIMATE_BOTH);
}  // namespace internal

void SetWindowVisibilityAnimationType(aura::Window* window,
                                      WindowVisibilityAnimationType type) {
  window->SetProperty(internal::kWindowVisibilityAnimationTypeKey, type);
}

void SetWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition) {
  window->SetProperty(internal::kWindowVisibilityAnimationTransitionKey,
                      transition);
}

void SetWindowVisibilityAnimationDuration(aura::Window* window,
                                          const TimeDelta& duration) {
  window->SetProperty(internal::kWindowVisibilityAnimationDurationKey,
                      static_cast<int>(duration.ToInternalValue()));
}

bool HasWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition) {
  WindowVisibilityAnimationTransition prop = window->GetProperty(
      internal::kWindowVisibilityAnimationTransitionKey);
  return (prop & transition) != 0;
}

namespace internal {
namespace {

const float kWindowAnimation_HideOpacity = 0.f;
const float kWindowAnimation_ShowOpacity = 1.f;
const float kWindowAnimation_TranslateFactor = -0.025f;
const float kWindowAnimation_ScaleFactor = 1.05f;
const float kWindowAnimation_MinimizeRotate = -90.f;

const float kWindowAnimation_Vertical_TranslateY = 15.f;

// Amount windows are scaled during workspace animations.
const float kWorkspaceScale = .95f;

// Gets/sets the WindowVisibilityAnimationType associated with a window.
WindowVisibilityAnimationType GetWindowVisibilityAnimationType(
    aura::Window* window) {
  WindowVisibilityAnimationType type =
      window->GetProperty(kWindowVisibilityAnimationTypeKey);
  if (type == WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT) {
    return (window->type() == aura::client::WINDOW_TYPE_MENU ||
            window->type() == aura::client::WINDOW_TYPE_TOOLTIP) ?
        WINDOW_VISIBILITY_ANIMATION_TYPE_FADE :
        WINDOW_VISIBILITY_ANIMATION_TYPE_DROP;
  }
  return type;
}

// Observes a hide animation.
// A window can be hidden for a variety of reasons. Sometimes, Hide() will be
// called and life is simple. Sometimes, the window is actually bound to a
// views::Widget and that Widget is closed, and life is a little more
// complicated. When a Widget is closed the aura::Window* is actually not
// destroyed immediately - it is actually just immediately hidden and then
// destroyed when the stack unwinds. To handle this case, we start the hide
// animation immediately when the window is hidden, then when the window is
// subsequently destroyed this object acquires ownership of the window's layer,
// so that it can continue animating it until the animation completes.
// Regardless of whether or not the window is destroyed, this object deletes
// itself when the animation completes.
class HidingWindowAnimationObserver : public ui::ImplicitAnimationObserver,
                                      public aura::WindowObserver {
 public:
  explicit HidingWindowAnimationObserver(aura::Window* window)
      : window_(window) {
    window_->AddObserver(this);
  }
  virtual ~HidingWindowAnimationObserver() {
    STLDeleteElements(&layers_);
  }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    // Restore the correct visibility value (overridden for the duration of the
    // animation in AnimateHideWindow()).
    layer()->SetVisible(false);
    // Window may have been destroyed by this point.
    if (window_)
      window_->RemoveObserver(this);
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    DCHECK_EQ(window, window_);
    DCHECK(layers_.empty());
    AcquireAllLayers(window_);
    window_->RemoveObserver(this);
    window_ = NULL;
  }

  void AcquireAllLayers(aura::Window* window) {
    ui::Layer* layer = window->AcquireLayer();
    DCHECK(layer);
    layers_.push_back(layer);
    for (aura::Window::Windows::const_iterator it = window->children().begin();
         it != window->children().end();
         ++it)
      AcquireAllLayers(*it);
  }

  ui::Layer* layer() { return window_ ? window_->layer() : layers_[0]; }

  aura::Window* window_;
  std::vector<ui::Layer*> layers_;

  DISALLOW_COPY_AND_ASSIGN(HidingWindowAnimationObserver);
};

// ImplicitAnimationObserver used when switching workspaces. Resets the layer
// visibility to 'false' when done. This doesn't need the complexity of
// HidingWindowAnimationObserver as the window isn't closing, and if it does a
// HidingWindowAnimationObserver will be created.
class WorkspaceHidingWindowAnimationObserver :
      public ui::ImplicitAnimationObserver {
 public:
  explicit WorkspaceHidingWindowAnimationObserver(aura::Window* window)
      : layer_(window->layer()) {
  }
  virtual ~WorkspaceHidingWindowAnimationObserver() {
  }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    // Restore the correct visibility value (overridden for the duration of the
    // animation in AnimateHideWindow()).
    layer_->SetVisible(false);
  }

  ui::Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceHidingWindowAnimationObserver);
};

// Shows a window using an animation, animating its opacity from 0.f to 1.f, and
// its transform from |start_transform| to |end_transform|.
void AnimateShowWindowCommon(aura::Window* window,
                             const ui::Transform& start_transform,
                             const ui::Transform& end_transform) {
  window->layer()->set_delegate(window);
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetTransform(start_transform);

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    int duration =
        window->GetProperty(internal::kWindowVisibilityAnimationDurationKey);
    if (duration > 0)
      settings.SetTransitionDuration(TimeDelta::FromInternalValue(duration));

    window->layer()->SetTransform(end_transform);
    window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
  }
}

// Hides a window using an animation, animating its opacity from 1.f to 0.f, and
// its transform to |end_transform|.
void AnimateHideWindowCommon(aura::Window* window,
                             const ui::Transform& end_transform) {
  window->layer()->set_delegate(NULL);

  // Property sets within this scope will be implicitly animated.
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.AddObserver(new HidingWindowAnimationObserver(window));

  int duration =
      window->GetProperty(internal::kWindowVisibilityAnimationDurationKey);
  if (duration > 0)
    settings.SetTransitionDuration(TimeDelta::FromInternalValue(duration));

  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetTransform(end_transform);
}

// Show/Hide windows using a shrink animation.
void AnimateShowWindow_Drop(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatScale(kWindowAnimation_ScaleFactor,
                        kWindowAnimation_ScaleFactor);
  transform.ConcatTranslate(
      kWindowAnimation_TranslateFactor * window->bounds().width(),
      kWindowAnimation_TranslateFactor * window->bounds().height());
  AnimateShowWindowCommon(window, transform, ui::Transform());
}

void AnimateHideWindow_Drop(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatScale(kWindowAnimation_ScaleFactor,
                        kWindowAnimation_ScaleFactor);
  transform.ConcatTranslate(
      kWindowAnimation_TranslateFactor * window->bounds().width(),
      kWindowAnimation_TranslateFactor * window->bounds().height());
  AnimateHideWindowCommon(window, transform);
}

// Show/Hide windows using a vertical Glenimation.
void AnimateShowWindow_Vertical(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatTranslate(0, kWindowAnimation_Vertical_TranslateY);
  AnimateShowWindowCommon(window, transform, ui::Transform());
}

void AnimateHideWindow_Vertical(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatTranslate(0, kWindowAnimation_Vertical_TranslateY);
  AnimateHideWindowCommon(window, transform);
}

// Show/Hide windows using a fade.
void AnimateShowWindow_Fade(aura::Window* window) {
  AnimateShowWindowCommon(window, ui::Transform(), ui::Transform());
}

void AnimateHideWindow_Fade(aura::Window* window) {
  AnimateHideWindowCommon(window, ui::Transform());
}

// Builds the transform used when switching workspaces for the specified
// window.
ui::Transform BuildWorkspaceSwitchTransform(aura::Window* window) {
  // Animations for transitioning workspaces scale all windows. To give the
  // effect of scaling from the center of the screen the windows are translated.
  gfx::Rect parent_bounds(window->parent()->bounds());
  float mid_x = static_cast<float>(parent_bounds.width()) / 2.0f;
  float initial_x =
      (static_cast<float>(window->bounds().x()) - mid_x) * kWorkspaceScale +
      mid_x;
  float mid_y = static_cast<float>(parent_bounds.height()) / 2.0f;
  float initial_y =
      (static_cast<float>(window->bounds().y()) - mid_y) * kWorkspaceScale +
      mid_y;

  ui::Transform transform;
  transform.ConcatTranslate(
      initial_x - static_cast<float>(window->bounds().x()),
      initial_y - static_cast<float>(window->bounds().y()));
  transform.ConcatScale(kWorkspaceScale, kWorkspaceScale);
  return transform;
}

void AnimateShowWindow_Workspace(aura::Window* window) {
  ui::Transform transform(BuildWorkspaceSwitchTransform(window));
  window->layer()->SetVisible(true);
  window->layer()->SetOpacity(0.0f);
  window->layer()->SetTransform(transform);

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());

    window->layer()->SetTransform(ui::Transform());
    // Opacity animates only during the first half of the animation.
    settings.SetTransitionDuration(settings.GetTransitionDuration() / 2);
    window->layer()->SetOpacity(1.0f);
  }
}

void AnimateHideWindow_Workspace(aura::Window* window) {
  ui::Transform transform(BuildWorkspaceSwitchTransform(window));
  window->layer()->SetOpacity(1.0f);
  window->layer()->SetTransform(ui::Transform());

  // Opacity animates from 1 to 0 only over the second half of the animation. To
  // get this functionality two animations are schedule for opacity, the first
  // from 1 to 1 (which effectively does nothing) the second from 1 to 0.
  // Because we're scheduling two animations of the same property we need to
  // change the preemption strategy.
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->set_preemption_strategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    // Add an observer that sets visibility of the layer to false once animation
    // completes.
    settings.AddObserver(new WorkspaceHidingWindowAnimationObserver(window));
    window->layer()->SetTransform(transform);
    settings.SetTransitionDuration(settings.GetTransitionDuration() / 2);
    window->layer()->SetOpacity(1.0f);
    window->layer()->SetOpacity(0.0f);
  }
  animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
}

gfx::Rect GetMinimizeRectForWindow(aura::Window* window) {
  gfx::Rect target_bounds = Shell::GetInstance()->launcher()->
      GetScreenBoundsOfItemIconForWindow(window);
  if (target_bounds.IsEmpty()) {
    // Assume the launcher is overflowed, zoom off to the bottom right of the
    // work area.
    gfx::Rect work_area = gfx::Screen::GetMonitorWorkAreaNearestWindow(window);
    target_bounds.SetRect(work_area.right(), work_area.bottom(), 0, 0);
  }
  return target_bounds;
}

void AnimateShowWindow_Minimize(aura::Window* window) {
  // Recalculate the transform at restore time since the launcher item may have
  // moved while the window was minimized.
  gfx::Rect target_bounds = GetMinimizeRectForWindow(window);
  ui::Transform transform;
  transform.ConcatScale(
      static_cast<float>(target_bounds.height()) / window->bounds().width(),
      static_cast<float>(target_bounds.width()) / window->bounds().height());
  transform.ConcatTranslate(target_bounds.x() - window->bounds().x(),
                            target_bounds.y() - window->bounds().y());

  AnimateShowWindowCommon(window, transform, ui::Transform());

  // Now that the window has been restored, we need to clear its animation style
  // to default so that normal animation applies.
  SetWindowVisibilityAnimationType(
      window, WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);
}

void AnimateHideWindow_Minimize(aura::Window* window) {
  gfx::Rect target_bounds = GetMinimizeRectForWindow(window);
  ui::Transform transform;
  transform.ConcatScale(
      static_cast<float>(target_bounds.height()) / window->bounds().width(),
      static_cast<float>(target_bounds.width()) / window->bounds().height());
  transform.ConcatTranslate(target_bounds.x() - window->bounds().x(),
                            target_bounds.y() - window->bounds().y());
  AnimateHideWindowCommon(window, transform);
}

bool AnimateShowWindow(aura::Window* window) {
  if (!HasWindowVisibilityAnimationTransition(window, ANIMATE_SHOW))
    return false;

  switch (GetWindowVisibilityAnimationType(window)) {
    case WINDOW_VISIBILITY_ANIMATION_TYPE_DROP:
      AnimateShowWindow_Drop(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL:
      AnimateShowWindow_Vertical(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_FADE:
      AnimateShowWindow_Fade(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_SHOW:
      AnimateShowWindow_Workspace(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE:
      AnimateShowWindow_Minimize(window);
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool AnimateHideWindow(aura::Window* window) {
  if (!HasWindowVisibilityAnimationTransition(window, ANIMATE_HIDE))
    return false;

  switch (GetWindowVisibilityAnimationType(window)) {
    case WINDOW_VISIBILITY_ANIMATION_TYPE_DROP:
      AnimateHideWindow_Drop(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL:
      AnimateHideWindow_Vertical(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_FADE:
      AnimateHideWindow_Fade(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_WORKSPACE_HIDE:
      AnimateHideWindow_Workspace(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE:
      AnimateHideWindow_Minimize(window);
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowAnimation, public:

bool AnimateOnChildWindowVisibilityChanged(aura::Window* window, bool visible) {
  if (window->GetProperty(aura::client::kAnimationsDisabledKey) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAuraWindowAnimationsDisabled)) {
    return false;
  }
  if (visible) {
    return AnimateShowWindow(window);
  } else {
    // Don't start hiding the window again if it's already being hidden.
    return window->layer()->GetTargetOpacity() != 0.0f &&
        AnimateHideWindow(window);
  }
}

}  // namespace internal
}  // namespace ash
