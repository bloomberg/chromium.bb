// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_animations.h"

#include <math.h>

#include <algorithm>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
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
const int kLayerAnimationsForMinimizeDurationMS = 200;

// Durations for the cross-fade animation, in milliseconds.
const float kCrossFadeDurationMinMs = 100.f;
const float kCrossFadeDurationMaxMs = 400.f;

// Durations for the brightness/grayscale fade animation, in milliseconds.
const int kBrightnessGrayscaleFadeDurationMs = 1000;

// Brightness/grayscale values for hide/show window animations.
const float kWindowAnimation_HideBrightnessGrayscale = 1.f;
const float kWindowAnimation_ShowBrightnessGrayscale = 0.f;

const float kWindowAnimation_HideOpacity = 0.f;
const float kWindowAnimation_ShowOpacity = 1.f;
const float kWindowAnimation_TranslateFactor = 0.025f;
const float kWindowAnimation_ScaleFactor = .95f;
// TODO(sky): if we end up sticking with 0, nuke the code doing the rotation.
const float kWindowAnimation_MinimizeRotate = 0.f;

// Amount windows are scaled during workspace animations.
const float kWorkspaceScale = .95f;

int64 Round64(float f) {
  return static_cast<int64>(f + 0.5f);
}

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
    delete this;
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
      if (layer) {
        layer->SuppressPaint();
        layers_.push_back(layer);
      }
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
class WorkspaceHidingWindowAnimationObserver
    : public ui::ImplicitAnimationObserver {
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
    delete this;
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
ui::Transform BuildWorkspaceSwitchTransform(aura::Window* window, float scale) {
  // Animations for transitioning workspaces scale all windows. To give the
  // effect of scaling from the center of the screen the windows are translated.
  gfx::Rect bounds = window->bounds();
  gfx::Rect parent_bounds(window->parent()->bounds());

  float mid_x = static_cast<float>(parent_bounds.width()) / 2.0f;
  float initial_x =
      (static_cast<float>(bounds.x()) - mid_x) * scale + mid_x;
  float mid_y = static_cast<float>(parent_bounds.height()) / 2.0f;
  float initial_y =
      (static_cast<float>(bounds.y()) - mid_y) * scale + mid_y;

  ui::Transform transform;
  transform.ConcatTranslate(
      initial_x - static_cast<float>(bounds.x()),
      initial_y - static_cast<float>(bounds.y()));
  transform.ConcatScale(scale, scale);
  return transform;
}

void AnimateShowWindow_Workspace(aura::Window* window) {
  ui::Transform transform(
      BuildWorkspaceSwitchTransform(window, kWorkspaceScale));
  // When we call SetOpacity here, if a hide sequence is already running,
  // the default animation preemption strategy fast forwards the hide sequence
  // to completion and notifies the WorkspaceHidingWindowAnimationObserver to
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
  ui::Transform transform(
      BuildWorkspaceSwitchTransform(window, kWorkspaceScale));
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
        gfx::Screen::GetDisplayNearestWindow(window).work_area();
    target_bounds.SetRect(work_area.right(), work_area.bottom(), 0, 0);
  }
  target_bounds =
      ScreenAsh::ConvertRectFromScreen(window->parent(), target_bounds);
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

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(
      kLayerAnimationsForMinimizeDurationMS);

  scoped_ptr<ui::LayerAnimationElement> transition(
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          rotation_about_pivot.release(), duration));

  transition->set_tween_type(
      show ? ui::Tween::EASE_IN : ui::Tween::EASE_IN_OUT);

  window->layer()->GetAnimator()->ScheduleAnimation(
      new ui::LayerAnimationSequence(transition.release()));

  // When hiding a window, turn off blending until the animation is 3 / 4 done
  // to save bandwidth and reduce jank
  if (!show) {
    window->layer()->GetAnimator()->SchedulePauseForProperties(
        (duration * 3 ) / 4, ui::LayerAnimationElement::OPACITY, -1);
  }

  // Fade in and out quickly when the window is small to reduce jank
  float opacity = show ? 1.0f : 0.0f;
  window->layer()->GetAnimator()->ScheduleAnimation(
      new ui::LayerAnimationSequence(
          ui::LayerAnimationElement::CreateOpacityElement(
              opacity, duration / 4)));
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
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(
      kLayerAnimationsForMinimizeDurationMS);
  settings.SetTransitionDuration(duration);
  settings.AddObserver(new HidingWindowAnimationObserver(window));
  window->layer()->SetVisible(false);

  AddLayerAnimationsForMinimize(window, false);
}

void AnimateShowHideWindowCommon_BrightnessGrayscale(aura::Window* window,
                                                     bool show) {
  window->layer()->set_delegate(window);

  float start_value, end_value;
  if (show) {
    start_value = kWindowAnimation_HideBrightnessGrayscale;
    end_value = kWindowAnimation_ShowBrightnessGrayscale;
  } else {
    start_value = kWindowAnimation_ShowBrightnessGrayscale;
    end_value = kWindowAnimation_HideBrightnessGrayscale;
  }

  window->layer()->SetLayerBrightness(start_value);
  window->layer()->SetLayerGrayscale(start_value);
  if (show) {
    window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
    window->layer()->SetVisible(true);
  }

  int animation_duration = kBrightnessGrayscaleFadeDurationMs;
  ui::Tween::Type animation_type = ui::Tween::EASE_OUT_2;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshDisableBootAnimation2)) {
    animation_type = ui::Tween::EASE_OUT;
  }

  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(animation_duration));
  if (!show)
    settings.AddObserver(new HidingWindowAnimationObserver(window));

  scoped_ptr<ui::LayerAnimationSequence> brightness_sequence(
      new ui::LayerAnimationSequence());
  scoped_ptr<ui::LayerAnimationSequence> grayscale_sequence(
      new ui::LayerAnimationSequence());

  scoped_ptr<ui::LayerAnimationElement> brightness_element(
      ui::LayerAnimationElement::CreateBrightnessElement(
          end_value,
          base::TimeDelta::FromMilliseconds(animation_duration)));
  brightness_element->set_tween_type(animation_type);
  brightness_sequence->AddElement(brightness_element.release());

  scoped_ptr<ui::LayerAnimationElement> grayscale_element(
      ui::LayerAnimationElement::CreateGrayscaleElement(
          end_value,
          base::TimeDelta::FromMilliseconds(animation_duration)));
  grayscale_element->set_tween_type(animation_type);
  grayscale_sequence->AddElement(grayscale_element.release());

   std::vector<ui::LayerAnimationSequence*> animations;
   animations.push_back(brightness_sequence.release());
   animations.push_back(grayscale_sequence.release());
   window->layer()->GetAnimator()->ScheduleTogether(animations);
   if (!show) {
     window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
     window->layer()->SetVisible(false);
   }
}

void AnimateShowWindow_BrightnessGrayscale(aura::Window* window) {
  AnimateShowHideWindowCommon_BrightnessGrayscale(window, true);
}

void AnimateHideWindow_BrightnessGrayscale(aura::Window* window) {
  AnimateShowHideWindowCommon_BrightnessGrayscale(window, false);
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
    case WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE:
        AnimateShowWindow_BrightnessGrayscale(window);
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
    case WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE:
      AnimateHideWindow_BrightnessGrayscale(window);
      return true;
    default:
      NOTREACHED();
      return false;
  }
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
    window_->RemoveObserver(this);
    window_ = NULL;
    layer_->GetCompositor()->RemoveObserver(this);
    wm::DeepDeleteLayers(layer_);
    layer_ = NULL;
  }

  // ui::CompositorObserver overrides:
  virtual void OnCompositingDidCommit(ui::Compositor* compositor) OVERRIDE {
  }
  virtual void OnCompositingWillStart(ui::Compositor* compositor) OVERRIDE {
  }
  virtual void OnCompositingStarted(ui::Compositor* compositor) OVERRIDE {
  }
  virtual void OnCompositingEnded(ui::Compositor* compositor) OVERRIDE {
  }
  virtual void OnCompositingAborted(ui::Compositor* compositor) OVERRIDE {
    // Triggers OnImplicitAnimationsCompleted() to be called and deletes us.
    layer_->GetAnimator()->StopAnimating();
  }

  // aura::WindowObserver overrides:
  virtual void OnWindowDestroying(Window* window) OVERRIDE {
    // Triggers OnImplicitAnimationsCompleted() to be called and deletes us.
    layer_->GetAnimator()->StopAnimating();
  }

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    delete this;
  }

 private:
  Window* window_;  // not owned
  Layer* layer_;  // owned

  DISALLOW_COPY_AND_ASSIGN(CrossFadeObserver);
};

}  // namespace
}  // namespace internal

namespace {

// Scales for workspaces above/below current workspace.
const float kWorkspaceScaleAbove = 1.1f;
const float kWorkspaceScaleBelow = .9f;

// Amount of time to pause before animating anything. Only used during initial
// animation (when logging in).
const int kWorkspaceInitialPauseTimeMS = 500;

// TODO: leaving in for now since Nicholas wants to play with this, remove if we
// leave it at 0.
const int kPauseTimeMS = 0;

}  // namespace

// Amount of time for animating a workspace in or out.
const int kWorkspaceSwitchTimeMS = 200 + kPauseTimeMS;

namespace {

// Brightness for the non-active workspace.
// TODO(sky): ideally this would be -.33f, but it slows down animations by a
// factor of 2. Figure out why.
const float kWorkspaceBrightness = 0.0f;

enum WorkspaceScaleType {
  WORKSPACE_SCALE_ABOVE,
  WORKSPACE_SCALE_BELOW,
};

// Used to identify what should animate in AnimateWorkspaceIn/Out.
enum WorkspaceAnimateTypes {
  WORKSPACE_ANIMATE_OPACITY =    1 << 0,
  WORKSPACE_ANIMATE_BRIGHTNESS = 1 << 1,
};

void ApplyWorkspaceScale(ui::Layer* layer, WorkspaceScaleType type) {
  const float scale = type == WORKSPACE_SCALE_ABOVE ? kWorkspaceScaleAbove :
      kWorkspaceScaleBelow;
  ui::Transform transform;
  transform.ConcatScale(scale, scale);
  transform.ConcatTranslate(
      -layer->bounds().width() * (scale - 1.0f) / 2,
      -layer->bounds().height() * (scale - 1.0f) / 2);
  layer->SetTransform(transform);
}

// Implementation of cross fading. Window is the window being cross faded. It
// should be at the target bounds. |old_layer| the previous layer from |window|.
// This takes ownership of |old_layer| and deletes when the animation is done.
// |pause_duration| is the duration to pause at the current bounds before
// animating. Returns the duration of the fade.
TimeDelta CrossFadeImpl(aura::Window* window,
                        ui::Layer* old_layer,
                        ui::Tween::Type tween_type,
                        base::TimeDelta pause_duration) {
  const gfx::Rect old_bounds(old_layer->bounds());
  const gfx::Rect new_bounds(window->bounds());
  const bool old_on_top = (old_bounds.width() > new_bounds.width());

  // Shorten the animation if there's not much visual movement.
  const TimeDelta duration = GetCrossFadeDuration(old_bounds, new_bounds);

  // Scale up the old layer while translating to new position.
  {
    old_layer->GetAnimator()->StopAnimating();
    ui::ScopedLayerAnimationSettings settings(old_layer->GetAnimator());
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    old_layer->GetAnimator()->SchedulePauseForProperties(
        pause_duration, ui::LayerAnimationElement::TRANSFORM,
        ui::LayerAnimationElement::OPACITY, -1);

    // Animation observer owns the old layer and deletes itself.
    settings.AddObserver(new internal::CrossFadeObserver(window, old_layer));
    settings.SetTransitionDuration(duration);
    settings.SetTweenType(tween_type);
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
    old_layer = NULL;
  }

  // Set the new layer's current transform, such that the user sees a scaled
  // version of the window with the original bounds at the original position.
  ui::Transform in_transform;
  const float scale_x = static_cast<float>(old_bounds.width()) /
      static_cast<float>(new_bounds.width());
  const float scale_y = static_cast<float>(old_bounds.height()) /
      static_cast<float>(new_bounds.height());
  in_transform.ConcatScale(scale_x, scale_y);
  in_transform.ConcatTranslate(old_bounds.x() - new_bounds.x(),
                               old_bounds.y() - new_bounds.y());
  window->layer()->SetTransform(in_transform);
  if (!old_on_top) {
    // The new layer is on top and should fade in.  The old layer below will
    // stay opaque and block the desktop.
    window->layer()->SetOpacity(0.f);
  }
  {
    // Animate the new layer to the identity transform, so the window goes to
    // its newly set bounds.
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    window->layer()->GetAnimator()->SchedulePauseForProperties(
        pause_duration, ui::LayerAnimationElement::TRANSFORM,
        ui::LayerAnimationElement::OPACITY, -1);
    settings.SetTransitionDuration(duration);
    settings.SetTweenType(tween_type);
    window->layer()->SetTransform(ui::Transform());
    if (!old_on_top) {
      // New layer is on top, fade it in.
      window->layer()->SetOpacity(1.f);
    }
  }
  return duration;
}

// Returns a TimeDelta from |time_ms|. If animations are disabled this returns
// a TimeDelta of 0 (so the animation completes immediately).
base::TimeDelta AdjustAnimationTime(int time_ms) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshWindowAnimationsDisabled))
    time_ms = 0;
  return base::TimeDelta::FromMilliseconds(time_ms);
}

// Returns a TimeDelta adjusted for animations. If animations are disabled this
// returns 0. If animations are not disabled and |time_delta| is non-empty, it
// is returned. Otherwise |delta_time_ms| is returned.
base::TimeDelta AdjustAnimationTimeDelta(base::TimeDelta time_delta,
                                         int delta_time_ms) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshWindowAnimationsDisabled))
    return base::TimeDelta();
  return time_delta == base::TimeDelta() ?
      base::TimeDelta::FromMilliseconds(delta_time_ms) : time_delta;
}

void AnimateWorkspaceInImpl(aura::Window* window,
                            WorkspaceAnimationDirection direction,
                            uint32 animate_types,
                            int pause_time_ms,
                            ui::Tween::Type tween_type,
                            base::TimeDelta duration) {
  window->layer()->SetOpacity(
      (animate_types & WORKSPACE_ANIMATE_OPACITY) ? 0.0f : 1.0f);
  window->layer()->SetLayerBrightness(
      (animate_types & WORKSPACE_ANIMATE_BRIGHTNESS) ?
      kWorkspaceBrightness : 0.0f);
  window->Show();
  ApplyWorkspaceScale(window->layer(),
                      direction == WORKSPACE_ANIMATE_UP ?
                          WORKSPACE_SCALE_BELOW : WORKSPACE_SCALE_ABOVE);
  window->layer()->GetAnimator()->StopAnimating();

  {
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());

    if (pause_time_ms > 0) {
      settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      window->layer()->GetAnimator()->SchedulePauseForProperties(
          AdjustAnimationTime(pause_time_ms),
          ui::LayerAnimationElement::TRANSFORM,
          ui::LayerAnimationElement::OPACITY,
          ui::LayerAnimationElement::BRIGHTNESS,
          ui::LayerAnimationElement::VISIBILITY,
          -1);
    }

    settings.SetTweenType(tween_type);
    settings.SetTransitionDuration(duration);
    window->layer()->SetTransform(ui::Transform());
    window->layer()->SetOpacity(1.0f);
    window->layer()->SetLayerBrightness(0.0f);
  }
}

void AnimateWorkspaceOutImpl(aura::Window* window,
                             WorkspaceAnimationDirection direction,
                             uint32 animate_types,
                             int pause_time_ms,
                             ui::Tween::Type tween_type,
                             TimeDelta duration) {
  window->Show();
  window->layer()->SetTransform(ui::Transform());
  window->layer()->SetLayerBrightness(0.0f);
  window->layer()->SetOpacity(1.0f);
  window->layer()->GetAnimator()->StopAnimating();

  {
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());

    if (pause_time_ms > 0) {
      settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      window->layer()->GetAnimator()->SchedulePauseForProperties(
          AdjustAnimationTime(pause_time_ms),
          ui::LayerAnimationElement::TRANSFORM,
          ui::LayerAnimationElement::OPACITY,
          ui::LayerAnimationElement::BRIGHTNESS,
          ui::LayerAnimationElement::VISIBILITY,
          -1);
    }

    settings.SetTransitionDuration(duration);
    settings.SetTweenType(tween_type);
    ApplyWorkspaceScale(window->layer(),
                        direction == WORKSPACE_ANIMATE_UP ?
                            WORKSPACE_SCALE_ABOVE : WORKSPACE_SCALE_BELOW);
    // NOTE: Hide() must be before SetOpacity(), else
    // VisibilityController::UpdateLayerVisibility doesn't pass the false to the
    // layer so that the layer and window end up out of sync and confused.
    window->Hide();
    if (animate_types & WORKSPACE_ANIMATE_OPACITY)
      window->layer()->SetOpacity(0.0f);
    if (animate_types & WORKSPACE_ANIMATE_BRIGHTNESS)
      window->layer()->SetLayerBrightness(kWorkspaceBrightness);

    // After the animation completes snap the transform back to the identity,
    // otherwise any one that asks for screen bounds gets a slightly scaled
    // version.
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    settings.SetTransitionDuration(base::TimeDelta());
    window->layer()->SetTransform(ui::Transform());
  }
}

ui::Tween::Type TweenTypeForWorskpaceOut(WorkspaceType type) {
  return WORKSPACE_DESKTOP ? ui::Tween::LINEAR : ui::Tween::EASE_OUT;
}

}  // namespace

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

void CrossFadeToBounds(aura::Window* window, const gfx::Rect& new_bounds) {
  DCHECK(window->TargetVisibility());
  const gfx::Rect old_bounds = window->bounds();

  // Create fresh layers for the window and all its children to paint into.
  // Takes ownership of the old layer and all its children, which will be
  // cleaned up after the animation completes.
  ui::Layer* old_layer = wm::RecreateWindowLayers(window, false);
  ui::Layer* new_layer = window->layer();

  // Resize the window to the new size, which will force a layout and paint.
  window->SetBounds(new_bounds);

  // Ensure the higher-resolution layer is on top.
  bool old_on_top = (old_bounds.width() > new_bounds.width());
  if (old_on_top)
    old_layer->parent()->StackBelow(new_layer, old_layer);
  else
    old_layer->parent()->StackAbove(new_layer, old_layer);

  CrossFadeImpl(window, old_layer, ui::Tween::EASE_OUT, base::TimeDelta());
}

void CrossFadeWindowBetweenWorkspaces(aura::Window* old_workspace,
                                      aura::Window* new_workspace,
                                      aura::Window* window,
                                      ui::Layer* old_layer) {
  ui::Layer* layer_parent = new_workspace->layer()->parent();
  layer_parent->Add(old_layer);
  const bool restoring = old_layer->bounds().width() > window->bounds().width();
  ui::Tween::Type tween_type, workspace_tween_type;
  if (restoring) {
    layer_parent->StackAbove(old_layer, new_workspace->layer());
    tween_type = ui::Tween::EASE_OUT;
    workspace_tween_type = ui::Tween::EASE_OUT;
  } else {
    layer_parent->StackBelow(old_layer, new_workspace->layer());
    tween_type = ui::Tween::EASE_IN_2;
    workspace_tween_type = ui::Tween::LINEAR;
  }

  const TimeDelta duration =
      CrossFadeImpl(window, old_layer, tween_type,
                    AdjustAnimationTime(restoring ? 0 : kPauseTimeMS));

  if (restoring) {
    typedef aura::Window::Windows Windows;
    if (old_workspace)
      AnimateWorkspaceOutImpl(old_workspace, WORKSPACE_ANIMATE_UP,
                              WORKSPACE_ANIMATE_BRIGHTNESS,
                              0, workspace_tween_type, duration);

    // Ideally we would use AnimateWorkspaceIn() for |new_workspace|, but that
    // results in |window| animating with the workspace scale. We don't want
    // that, so we explicitly animate each of the children to give the effect of
    // the workspace scaling.
    new_workspace->Show();
    new_workspace->SetTransform(ui::Transform());
    new_workspace->layer()->SetOpacity(1.0f);
    new_workspace->layer()->SetLayerBrightness(0.0f);
    const Windows& children(new_workspace->children());
    for (Windows::const_iterator i = children.begin(); i != children.end();
         ++i) {
      aura::Window* child = *i;
      if (child == window)
        continue;
      child->SetTransform(ash::internal::BuildWorkspaceSwitchTransform(
                              child, kWorkspaceScaleBelow));
      child->layer()->SetLayerBrightness(kWorkspaceBrightness);
      ui::ScopedLayerAnimationSettings settings(child->layer()->GetAnimator());
      settings.SetTweenType(ui::Tween::EASE_OUT);
      settings.SetTransitionDuration(duration);
      child->SetTransform(ui::Transform());
      child->layer()->SetLayerBrightness(0.0f);
    }
  } else {
    if (old_workspace) {
      AnimateWorkspaceOutImpl(old_workspace, WORKSPACE_ANIMATE_DOWN,
                              WORKSPACE_ANIMATE_BRIGHTNESS,
                              0, workspace_tween_type, duration);
    }

    new_workspace->Show();
    new_workspace->layer()->SetOpacity(1.f);
    new_workspace->layer()->SetTransform(ui::Transform());
    new_workspace->layer()->SetLayerBrightness(0.0f);
  }
}

void AnimateBetweenWorkspaces(aura::Window* old_window,
                              WorkspaceType old_type,
                              bool animate_old,
                              aura::Window* new_window,
                              WorkspaceType new_type,
                              bool is_restoring_maximized_window) {
  uint32 common_animate_types = 0;
  if (animate_old) {
    // When switching to the desktop the old window lifts off.
    uint32 animate_types = 0;
    if (!(new_type == WORKSPACE_MAXIMIZED && old_type == WORKSPACE_MAXIMIZED))
      animate_types |= WORKSPACE_ANIMATE_BRIGHTNESS;
    if ((new_type == WORKSPACE_DESKTOP || old_type == WORKSPACE_MAXIMIZED) &&
        !is_restoring_maximized_window)
      animate_types |= WORKSPACE_ANIMATE_OPACITY;
    AnimateWorkspaceOutImpl(
        old_window,
        new_type == WORKSPACE_DESKTOP ? WORKSPACE_ANIMATE_UP :
            WORKSPACE_ANIMATE_DOWN,
        animate_types,
        0,
        TweenTypeForWorskpaceOut(old_type),
        AdjustAnimationTime(kWorkspaceSwitchTimeMS));
  }

  // Switching from the desktop to a maximized animates down.
  uint32 animate_types = common_animate_types;
  if (!(new_type == WORKSPACE_MAXIMIZED && old_type == WORKSPACE_MAXIMIZED) &&
      !is_restoring_maximized_window)
    animate_types |= WORKSPACE_ANIMATE_BRIGHTNESS;
  if (old_type == WORKSPACE_DESKTOP)
    animate_types |= WORKSPACE_ANIMATE_OPACITY;
  AnimateWorkspaceInImpl(
      new_window,
      old_type == WORKSPACE_DESKTOP ?
          WORKSPACE_ANIMATE_DOWN : WORKSPACE_ANIMATE_UP,
      animate_types,
      0,
      ui::Tween::EASE_OUT,
      AdjustAnimationTime(kWorkspaceSwitchTimeMS));
}

void AnimateWorkspaceIn(aura::Window* window,
                        WorkspaceAnimationDirection direction,
                        bool initial_animate,
                        base::TimeDelta delta) {
  AnimateWorkspaceInImpl(
      window, direction,
      WORKSPACE_ANIMATE_BRIGHTNESS |
          (initial_animate ? WORKSPACE_ANIMATE_OPACITY : 0),
      initial_animate ? kWorkspaceInitialPauseTimeMS : 0,
      ui::Tween::EASE_OUT,
      AdjustAnimationTimeDelta(delta, kWorkspaceSwitchTimeMS));
}

void AnimateWorkspaceOut(aura::Window* window,
                         WorkspaceAnimationDirection direction,
                         WorkspaceType type,
                         bool initial_animate,
                         base::TimeDelta delta) {
  AnimateWorkspaceOutImpl(window, direction, WORKSPACE_ANIMATE_BRIGHTNESS,
                          initial_animate ? kWorkspaceInitialPauseTimeMS : 0,
                          TweenTypeForWorskpaceOut(type),
                          AdjustAnimationTimeDelta(delta,
                                                   kWorkspaceSwitchTimeMS));
}

base::TimeDelta GetSystemBackgroundDestroyDuration() {
  return base::TimeDelta::FromMilliseconds(
      std::max(static_cast<int>(internal::kCrossFadeDurationMaxMs),
               kWorkspaceSwitchTimeMS));
}

TimeDelta GetCrossFadeDuration(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshWindowAnimationsDisabled))
    return base::TimeDelta();

  const int min_time_ms = internal::WorkspaceController::IsWorkspace2Enabled() ?
      kWorkspaceSwitchTimeMS : 0;
  int old_area = old_bounds.width() * old_bounds.height();
  int new_area = new_bounds.width() * new_bounds.height();
  int max_area = std::max(old_area, new_area);
  // Avoid divide by zero.
  if (max_area == 0)
    return TimeDelta::FromMilliseconds(min_time_ms);

  int delta_area = std::abs(old_area - new_area);
  // If the area didn't change, the animation is instantaneous.
  if (delta_area == 0)
    return TimeDelta::FromMilliseconds(min_time_ms);

  float factor =
      static_cast<float>(delta_area) / static_cast<float>(max_area);
  const float kRange = internal::kCrossFadeDurationMaxMs -
      internal::kCrossFadeDurationMinMs;
  return TimeDelta::FromMilliseconds(
      internal::Round64(internal::kCrossFadeDurationMinMs + (factor * kRange)));
}

namespace internal {

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
