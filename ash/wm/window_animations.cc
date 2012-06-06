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
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

DECLARE_WINDOW_PROPERTY_TYPE(int)
DECLARE_WINDOW_PROPERTY_TYPE(ash::WindowVisibilityAnimationType)
DECLARE_WINDOW_PROPERTY_TYPE(ash::WindowVisibilityAnimationTransition)
DECLARE_WINDOW_PROPERTY_TYPE(float)

using aura::Window;
using base::TimeDelta;
using ui::Layer;

namespace ash {
namespace internal {
namespace {
const float kWindowAnimation_Vertical_TranslateY = 15.f;
}

DEFINE_WINDOW_PROPERTY_KEY(WindowVisibilityAnimationType,
                           kWindowVisibilityAnimationTypeKey,
                           WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);
DEFINE_WINDOW_PROPERTY_KEY(int, kWindowVisibilityAnimationDurationKey, 0);
DEFINE_WINDOW_PROPERTY_KEY(WindowVisibilityAnimationTransition,
                           kWindowVisibilityAnimationTransitionKey,
                           ANIMATE_BOTH);
DEFINE_WINDOW_PROPERTY_KEY(float,
                           kWindowVisibilityAnimationVerticalPositionKey,
                           kWindowAnimation_Vertical_TranslateY);

namespace {

const int kDefaultAnimationDurationForMenuMS = 150;

// TODO(jamescook): Shorten the duration if the window doesn't move much.
const int kCrossFadeAnimationDurationMs = 400;

const float kWindowAnimation_HideOpacity = 0.f;
const float kWindowAnimation_ShowOpacity = 1.f;
const float kWindowAnimation_TranslateFactor = -0.025f;
const float kWindowAnimation_ScaleFactor = 1.05f;
const float kWindowAnimation_MinimizeRotate = -5.f;

// Amount windows are scaled during workspace animations.
const float kWorkspaceScale = .95f;

base::TimeDelta GetWindowVisibilityAnimationDuration(aura::Window* window) {
  int duration =
      window->GetProperty(kWindowVisibilityAnimationDurationKey);
  if (duration == 0 && window->type() == aura::client::WINDOW_TYPE_MENU) {
    return base::TimeDelta::FromMilliseconds(
        kDefaultAnimationDurationForMenuMS);
  }
  return TimeDelta::FromInternalValue(duration);
}

bool HasWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition) {
  WindowVisibilityAnimationTransition prop = window->GetProperty(
      kWindowVisibilityAnimationTransitionKey);
  return (prop & transition) != 0;
}

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

    // If the Widget has views with layers, then it is necessary to take
    // ownership of those layers too.
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window_);
    const views::Widget* const_widget = widget;
    if (widget && const_widget->GetRootView() && widget->GetContentsView())
      AcquireAllViewLayers(widget->GetContentsView());

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

  void AcquireAllViewLayers(views::View* view) {
    for (int i = 0; i < view->child_count(); ++i)
      AcquireAllViewLayers(view->child_at(i));
    if (view->layer()) {
      ui::Layer* layer = view->RecreateLayer();
      layer->SuppressPaint();
      layers_.push_back(layer);
    }
  }

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

// Shows a window using an animation, animating its opacity from 0.f to 1.f,
// its visibility to true, and its transform from |start_transform| to
// |end_transform|.
void AnimateShowWindowCommon(aura::Window* window,
                             const ui::Transform& start_transform,
                             const ui::Transform& end_transform) {
  window->layer()->set_delegate(window);
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetTransform(start_transform);

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    base::TimeDelta duration = GetWindowVisibilityAnimationDuration(window);
    if (duration.ToInternalValue() > 0)
      settings.SetTransitionDuration(duration);

    window->layer()->SetVisible(true);
    window->layer()->SetTransform(end_transform);
    window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
  }
}

// Hides a window using an animation, animating its opacity from 1.f to 0.f,
// its visibility to false, and its transform to |end_transform|.
void AnimateHideWindowCommon(aura::Window* window,
                             const ui::Transform& end_transform) {
  window->layer()->set_delegate(NULL);

  // Property sets within this scope will be implicitly animated.
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.AddObserver(new HidingWindowAnimationObserver(window));

  base::TimeDelta duration = GetWindowVisibilityAnimationDuration(window);
  if (duration.ToInternalValue() > 0)
    settings.SetTransitionDuration(duration);

  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetTransform(end_transform);
  window->layer()->SetVisible(false);
}

// Show/Hide windows using a shrink animation.
void AnimateShowWindow_Drop(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatScale(kWindowAnimation_ScaleFactor,
                        kWindowAnimation_ScaleFactor);
  gfx::Rect bounds = window->bounds();
  transform.ConcatTranslate(
      kWindowAnimation_TranslateFactor * bounds.width(),
      kWindowAnimation_TranslateFactor * bounds.height());
  AnimateShowWindowCommon(window, transform, ui::Transform());
}

void AnimateHideWindow_Drop(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatScale(kWindowAnimation_ScaleFactor,
                        kWindowAnimation_ScaleFactor);
  gfx::Rect bounds = window->bounds();
  transform.ConcatTranslate(
      kWindowAnimation_TranslateFactor * bounds.width(),
      kWindowAnimation_TranslateFactor * bounds.height());
  AnimateHideWindowCommon(window, transform);
}

// Show/Hide windows using a vertical Glenimation.
void AnimateShowWindow_Vertical(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatTranslate(0, window->GetProperty(
      kWindowVisibilityAnimationVerticalPositionKey));
  AnimateShowWindowCommon(window, transform, ui::Transform());
}

void AnimateHideWindow_Vertical(aura::Window* window) {
  ui::Transform transform;
  transform.ConcatTranslate(0, window->GetProperty(
      kWindowVisibilityAnimationVerticalPositionKey));
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
  gfx::Rect bounds = window->bounds();
  gfx::Rect parent_bounds(window->parent()->bounds());

  float mid_x = static_cast<float>(parent_bounds.width()) / 2.0f;
  float initial_x =
      (static_cast<float>(bounds.x()) - mid_x) * kWorkspaceScale +
      mid_x;
  float mid_y = static_cast<float>(parent_bounds.height()) / 2.0f;
  float initial_y =
      (static_cast<float>(bounds.y()) - mid_y) * kWorkspaceScale +
      mid_y;

  ui::Transform transform;
  transform.ConcatTranslate(
      initial_x - static_cast<float>(bounds.x()),
      initial_y - static_cast<float>(bounds.y()));
  transform.ConcatScale(kWorkspaceScale, kWorkspaceScale);
  return transform;
}

void AnimateShowWindow_Workspace(aura::Window* window) {
  ui::Transform transform(BuildWorkspaceSwitchTransform(window));
  // When we call SetOpacity here, if a hide sequence is already running,
  // the default animation preemption strategy fast forwards the hide sequence
  // to completion and notify the WorkspaceHidingWindowAnimationObserver to
  // set the layer to be invisible. We should call SetVisible after SetOpacity
  // to ensure our layer is visible again.
  window->layer()->SetOpacity(0.0f);
  window->layer()->SetTransform(transform);
  window->layer()->SetVisible(true);

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
    gfx::Rect work_area =
        gfx::Screen::GetMonitorNearestWindow(window).work_area();
    target_bounds.SetRect(work_area.right(), work_area.bottom(), 0, 0);
  }
  return target_bounds;
}

void AddLayerAnimationsForMinimize(aura::Window* window, bool show) {
  // Recalculate the transform at restore time since the launcher item may have
  // moved while the window was minimized.
  gfx::Rect bounds = window->bounds();
  gfx::Rect target_bounds = GetMinimizeRectForWindow(window);

  float scale_x = static_cast<float>(target_bounds.height()) / bounds.width();
  float scale_y = static_cast<float>(target_bounds.width()) / bounds.height();

  scoped_ptr<ui::InterpolatedTransform> scale(
      new ui::InterpolatedScale(gfx::Point3f(1, 1, 1),
                                gfx::Point3f(scale_x, scale_y, 1)));

  scoped_ptr<ui::InterpolatedTransform> translation(
      new ui::InterpolatedTranslation(
          gfx::Point(),
          gfx::Point(target_bounds.x() - bounds.x(),
                     target_bounds.y() - bounds.y())));

  scoped_ptr<ui::InterpolatedTransform> rotation(
      new ui::InterpolatedRotation(0, kWindowAnimation_MinimizeRotate));

  scoped_ptr<ui::InterpolatedTransform> rotation_about_pivot(
      new ui::InterpolatedTransformAboutPivot(
          gfx::Point(bounds.width() * 0.5, bounds.height() * 0.5),
          rotation.release()));

  scale->SetChild(translation.release());
  rotation_about_pivot->SetChild(scale.release());

  rotation_about_pivot->SetReversed(show);

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(350);

  scoped_ptr<ui::LayerAnimationElement> transition(
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          rotation_about_pivot.release(), duration));

  transition->set_tween_type(
      show ? ui::Tween::EASE_IN : ui::Tween::EASE_IN_OUT);

  window->layer()->GetAnimator()->ScheduleAnimation(
      new ui::LayerAnimationSequence(transition.release()));

  float opacity = show ? 1.0f : 0.0f;
  window->layer()->GetAnimator()->ScheduleAnimation(
      new ui::LayerAnimationSequence(
          ui::LayerAnimationElement::CreateOpacityElement(opacity, duration)));
}

void AnimateShowWindow_Minimize(aura::Window* window) {
  window->layer()->set_delegate(window);
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  AddLayerAnimationsForMinimize(window, true);

  // Now that the window has been restored, we need to clear its animation style
  // to default so that normal animation applies.
  SetWindowVisibilityAnimationType(
      window, WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);
}

void AnimateHideWindow_Minimize(aura::Window* window) {
  window->layer()->set_delegate(NULL);

  // Property sets within this scope will be implicitly animated.
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.AddObserver(new HidingWindowAnimationObserver(window));
  window->layer()->SetVisible(false);

  AddLayerAnimationsForMinimize(window, false);
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

// Recreates a fresh layer for |window| and all its child windows. Does not
// recreate shadows or other non-window layers. Returns the old layer and its
// children, maintaining the hierarchy.
Layer* RecreateWindowLayers(Window* window) {
  Layer* old_layer = window->RecreateLayer();
  for (Window::Windows::const_iterator it = window->children().begin();
       it != window->children().end();
       ++it) {
    aura::Window* child = *it;
    Layer* old_child_layer = RecreateWindowLayers(child);
    // Maintain the hierarchy of the detached layers.
    old_layer->Add(old_child_layer);
  }
  return old_layer;
}

// Deletes |layer| and all its child layers.
void DeepDelete(Layer* layer) {
  std::vector<Layer*> children = layer->children();
  for (std::vector<Layer*>::const_iterator it = children.begin();
       it != children.end();
       ++it) {
    Layer* child = *it;
    DeepDelete(child);
  }
  delete layer;
}

// Observer for a window cross-fade animation. If either the window closes or
// the layer's animation completes or compositing is aborted due to GPU crash,
// it deletes the layer and removes itself as an observer.
class CrossFadeObserver : public ui::CompositorObserver,
                          public aura::WindowObserver,
                          public ui::ImplicitAnimationObserver {
 public:
  // Observes |window| for destruction, but does not take ownership.
  // Takes ownership of |layer| and its child layers.
  CrossFadeObserver(Window* window, Layer* layer)
      : window_(window),
        layer_(layer) {
    window_->AddObserver(this);
    layer_->GetCompositor()->AddObserver(this);
  }
  virtual ~CrossFadeObserver() {
    Cleanup();
  }

  // ui::CompositorObserver overrides:
  virtual void OnCompositingStarted(ui::Compositor* compositor) OVERRIDE {
  }
  virtual void OnCompositingEnded(ui::Compositor* compositor) OVERRIDE {
  }
  virtual void OnCompositingAborted(ui::Compositor* compositor) OVERRIDE {
    // Something went wrong with compositing and our layers are now invalid.
    if (layer_)
      layer_->GetAnimator()->StopAnimating();
    // Delete is scheduled in OnImplicitAnimationsCompleted().
    Cleanup();
  }

  // aura::WindowObserver overrides:
  virtual void OnWindowDestroying(Window* window) OVERRIDE {
    if (layer_)
      layer_->GetAnimator()->StopAnimating();
    // Delete is scheduled in OnImplicitAnimationsCompleted().
    Cleanup();
  }

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    // ImplicitAnimationObserver's base class uses the object after calling
    // this function, so we cannot delete |this|.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }

 private:
  // Can be called multiple times if the window is closed or the compositor
  // fails in the middle of the animation.
  void Cleanup() {
    if (window_) {
      window_->RemoveObserver(this);
      window_ = NULL;
    }
    if (layer_) {
      layer_->GetCompositor()->RemoveObserver(this);
      DeepDelete(layer_);
      layer_ = NULL;
    }
  }

  Window* window_;  // not owned
  Layer* layer_;  // owned

  DISALLOW_COPY_AND_ASSIGN(CrossFadeObserver);
};

}  // namespace
}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// External interface

void SetWindowVisibilityAnimationType(aura::Window* window,
                                      WindowVisibilityAnimationType type) {
  window->SetProperty(internal::kWindowVisibilityAnimationTypeKey, type);
}

WindowVisibilityAnimationType GetWindowVisibilityAnimationType(
    aura::Window* window) {
  return window->GetProperty(internal::kWindowVisibilityAnimationTypeKey);
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

void SetWindowVisibilityAnimationVerticalPosition(aura::Window* window,
                                                  float position) {
  window->SetProperty(internal::kWindowVisibilityAnimationVerticalPositionKey,
                      position);
}

ui::ImplicitAnimationObserver* CreateHidingWindowAnimationObserver(
    aura::Window* window) {
  return new internal::HidingWindowAnimationObserver(window);
}

namespace internal {

void CrossFadeToBounds(aura::Window* window, const gfx::Rect& new_bounds) {
  DCHECK(window->TargetVisibility());
  gfx::Rect old_bounds = window->bounds();

  // Create fresh layers for the window and all its children to paint into.
  // Takes ownership of the old layer and all its children, which will be
  // cleaned up after the animation completes.
  ui::Layer* old_layer = RecreateWindowLayers(window);
  ui::Layer* new_layer = window->layer();

  // Ensure the higher-resolution layer is on top.
  bool old_on_top = (old_bounds.width() > new_bounds.width());
  if (old_on_top)
    old_layer->parent()->StackBelow(new_layer, old_layer);
  else
    old_layer->parent()->StackAbove(new_layer, old_layer);

  // Tween types for transform animations must match to keep the window edges
  // aligned during the animation.
  const ui::Tween::Type kTransformTween = ui::Tween::EASE_OUT;
  {
    // Scale up the old layer while translating to new position.
    ui::ScopedLayerAnimationSettings settings(old_layer->GetAnimator());
    // Animation observer owns the old layer and deletes itself.
    settings.AddObserver(new CrossFadeObserver(window, old_layer));
    settings.SetTransitionDuration(
        TimeDelta::FromMilliseconds(kCrossFadeAnimationDurationMs));
    settings.SetTweenType(kTransformTween);
    ui::Transform out_transform;
    float scale_x = static_cast<float>(new_bounds.width()) /
        static_cast<float>(old_bounds.width());
    float scale_y = static_cast<float>(new_bounds.height()) /
        static_cast<float>(old_bounds.height());
    out_transform.ConcatScale(scale_x, scale_y);
    out_transform.ConcatTranslate(new_bounds.x() - old_bounds.x(),
                                  new_bounds.y() - old_bounds.y());
    old_layer->SetTransform(out_transform);
    if (old_on_top) {
      // The old layer is on top, and should fade out.  The new layer below will
      // stay opaque to block the desktop.
      old_layer->SetOpacity(0.f);
    }
    // In tests |old_layer| is deleted here, as animations have zero duration.
  }

  // Resize the window to the new size, which will force a layout and paint.
  window->SetBounds(new_bounds);

  // Set the new layer's current transform, such that the user sees a scaled
  // version of the window with the original bounds at the original position.
  ui::Transform in_transform;
  float scale_x = static_cast<float>(old_bounds.width()) /
      static_cast<float>(new_bounds.width());
  float scale_y = static_cast<float>(old_bounds.height()) /
      static_cast<float>(new_bounds.height());
  in_transform.ConcatScale(scale_x, scale_y);
  in_transform.ConcatTranslate(old_bounds.x() - new_bounds.x(),
                               old_bounds.y() - new_bounds.y());
  new_layer->SetTransform(in_transform);
  if (!old_on_top) {
    // The new layer is on top and should fade in.  The old layer below will
    // stay opaque and block the desktop.
    new_layer->SetOpacity(0.f);
  }
  {
    // Animate the new layer to the identity transform, so the window goes to
    // its newly set bounds.
    ui::ScopedLayerAnimationSettings settings(new_layer->GetAnimator());
    settings.SetTransitionDuration(
        TimeDelta::FromMilliseconds(kCrossFadeAnimationDurationMs));
    settings.SetTweenType(kTransformTween);
    new_layer->SetTransform(ui::Transform());
    if (!old_on_top) {
      // New layer is on top, fade it in.
      new_layer->SetOpacity(1.f);
    }
  }
}

bool AnimateOnChildWindowVisibilityChanged(aura::Window* window, bool visible) {
  if (window->GetProperty(aura::client::kAnimationsDisabledKey) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshWindowAnimationsDisabled)) {
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
