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
#include "ash/wm/workspace/workspace_animations.h"
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
#include "ui/gfx/vector3d_f.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

DECLARE_WINDOW_PROPERTY_TYPE(int)
DECLARE_WINDOW_PROPERTY_TYPE(ash::WindowVisibilityAnimationType)
DECLARE_WINDOW_PROPERTY_TYPE(ash::WindowVisibilityAnimationTransition)
DECLARE_WINDOW_PROPERTY_TYPE(float)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(ASH_EXPORT, bool)

using aura::Window;
using base::TimeDelta;
using ui::Layer;

namespace ash {
namespace internal {
namespace {
const float kWindowAnimation_Vertical_TranslateY = 15.f;

}  // namespace

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
const float kCrossFadeDurationMinMs = 200.f;
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

const int kWindowAnimation_Rotate_DurationMS = 180;
const int kWindowAnimation_Rotate_OpacityDurationPercent = 90;
const float kWindowAnimation_Rotate_TranslateY = -20.f;
const float kWindowAnimation_Rotate_PerspectiveDepth = 500.f;
const float kWindowAnimation_Rotate_DegreesX = 5.f;
const float kWindowAnimation_Rotate_ScaleFactor = .99f;

const float kWindowAnimation_Bounce_Scale = 1.02f;
const int kWindowAnimation_Bounce_DurationMS = 180;
const int kWindowAnimation_Bounce_GrowShrinkDurationPercent = 40;

// Tween type when cross fading a workspace window.
const ui::Tween::Type kCrossFadeTweenType = ui::Tween::EASE_IN_OUT;

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
                             const gfx::Transform& start_transform,
                             const gfx::Transform& end_transform) {
  window->layer()->set_delegate(window);
  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetTransform(start_transform);
  window->layer()->SetVisible(true);

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    base::TimeDelta duration = GetWindowVisibilityAnimationDuration(window);
    if (duration.ToInternalValue() > 0)
      settings.SetTransitionDuration(duration);

    window->layer()->SetTransform(end_transform);
    window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
  }
}

// Hides a window using an animation, animating its opacity from 1.f to 0.f,
// its visibility to false, and its transform to |end_transform|.
void AnimateHideWindowCommon(aura::Window* window,
                             const gfx::Transform& end_transform) {
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

static gfx::Transform GetScaleForWindow(aura::Window* window) {
  gfx::Rect bounds = window->bounds();
  gfx::Transform scale = gfx::GetScaleTransform(
      gfx::Point(kWindowAnimation_TranslateFactor * bounds.width(),
                 kWindowAnimation_TranslateFactor * bounds.height()),
      kWindowAnimation_ScaleFactor);
  return scale;
}

// Show/Hide windows using a shrink animation.
void AnimateShowWindow_Drop(aura::Window* window) {
  AnimateShowWindowCommon(window, GetScaleForWindow(window), gfx::Transform());
}

void AnimateHideWindow_Drop(aura::Window* window) {
  AnimateHideWindowCommon(window, GetScaleForWindow(window));
}

// Show/Hide windows using a vertical Glenimation.
void AnimateShowWindow_Vertical(aura::Window* window) {
  gfx::Transform transform;
  transform.Translate(0, window->GetProperty(
      kWindowVisibilityAnimationVerticalPositionKey));
  AnimateShowWindowCommon(window, transform, gfx::Transform());
}

void AnimateHideWindow_Vertical(aura::Window* window) {
  gfx::Transform transform;
  transform.Translate(0, window->GetProperty(
      kWindowVisibilityAnimationVerticalPositionKey));
  AnimateHideWindowCommon(window, transform);
}

// Show/Hide windows using a fade.
void AnimateShowWindow_Fade(aura::Window* window) {
  AnimateShowWindowCommon(window, gfx::Transform(), gfx::Transform());
}

void AnimateHideWindow_Fade(aura::Window* window) {
  AnimateHideWindowCommon(window, gfx::Transform());
}

gfx::Rect GetMinimizeRectForWindow(aura::Window* window) {
  Launcher* launcher = Launcher::ForWindow(window);
  // Launcher is created lazily and can be NULL.
  if (!launcher)
    return gfx::Rect();
  gfx::Rect target_bounds = Launcher::ForWindow(window)->
      GetScreenBoundsOfItemIconForWindow(window);
  if (target_bounds.IsEmpty()) {
    // Assume the launcher is overflowed, zoom off to the bottom right of the
    // work area.
    gfx::Rect work_area =
        Shell::GetScreen()->GetDisplayNearestWindow(window).work_area();
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
      new ui::InterpolatedScale(gfx::Point3F(1, 1, 1),
                                gfx::Point3F(scale_x, scale_y, 1)));

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

  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kBrightnessGrayscaleFadeDurationMs);

  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetTransitionDuration(duration);
  if (!show)
    settings.AddObserver(new HidingWindowAnimationObserver(window));

   window->layer()->GetAnimator()->
       ScheduleTogether(
           internal::CreateBrightnessGrayscaleAnimationSequence(end_value,
                                                                duration));
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

ui::LayerAnimationElement* CreateGrowShrinkElement(
    aura::Window* window, bool grow) {
  scoped_ptr<ui::InterpolatedTransform> scale(new ui::InterpolatedScale(
      gfx::Point3F(kWindowAnimation_Bounce_Scale,
                   kWindowAnimation_Bounce_Scale,
                   1),
      gfx::Point3F(1, 1, 1)));
  scoped_ptr<ui::InterpolatedTransform> scale_about_pivot(
      new ui::InterpolatedTransformAboutPivot(
          gfx::Point(window->bounds().width() * 0.5,
                     window->bounds().height() * 0.5),
          scale.release()));
  scale_about_pivot->SetReversed(grow);
  scoped_ptr<ui::LayerAnimationElement> transition(
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          scale_about_pivot.release(),
          base::TimeDelta::FromMilliseconds(
              kWindowAnimation_Bounce_DurationMS *
                  kWindowAnimation_Bounce_GrowShrinkDurationPercent / 100)));
  transition->set_tween_type(grow ? ui::Tween::EASE_OUT : ui::Tween::EASE_IN);
  return transition.release();
}

void AnimateBounce(aura::Window* window) {
  ui::ScopedLayerAnimationSettings scoped_settings(
      window->layer()->GetAnimator());
  scoped_settings.SetPreemptionStrategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  window->layer()->set_delegate(window);
  scoped_ptr<ui::LayerAnimationSequence> sequence(
      new ui::LayerAnimationSequence);
  sequence->AddElement(CreateGrowShrinkElement(window, true));
  ui::LayerAnimationElement::AnimatableProperties paused_properties;
  paused_properties.insert(ui::LayerAnimationElement::BOUNDS);
  sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      paused_properties,
      base::TimeDelta::FromMilliseconds(
        kWindowAnimation_Bounce_DurationMS *
            (100 - 2 * kWindowAnimation_Bounce_GrowShrinkDurationPercent) /
            100)));
  sequence->AddElement(CreateGrowShrinkElement(window, false));
  window->layer()->GetAnimator()->StartAnimation(sequence.release());
}

void AddLayerAnimationsForRotate(aura::Window* window, bool show) {
  window->layer()->set_delegate(window);
  if (show)
    window->layer()->SetOpacity(kWindowAnimation_HideOpacity);

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(
      kWindowAnimation_Rotate_DurationMS);

  if (!show) {
    new HidingWindowAnimationObserver(window);
    window->layer()->GetAnimator()->SchedulePauseForProperties(
        duration * (100 - kWindowAnimation_Rotate_OpacityDurationPercent) / 100,
        ui::LayerAnimationElement::OPACITY,
        -1);
  }
  scoped_ptr<ui::LayerAnimationElement> opacity(
      ui::LayerAnimationElement::CreateOpacityElement(
          show ? kWindowAnimation_ShowOpacity : kWindowAnimation_HideOpacity,
          duration * kWindowAnimation_Rotate_OpacityDurationPercent / 100));
  opacity->set_tween_type(ui::Tween::EASE_IN_OUT);
  window->layer()->GetAnimator()->ScheduleAnimation(
      new ui::LayerAnimationSequence(opacity.release()));

  float xcenter = window->bounds().width() * 0.5;

  gfx::Transform transform;
  transform.Translate(xcenter, 0);
  transform.ApplyPerspectiveDepth(kWindowAnimation_Rotate_PerspectiveDepth);
  transform.Translate(-xcenter, 0);
  scoped_ptr<ui::InterpolatedTransform> perspective(
      new ui::InterpolatedConstantTransform(transform));

  scoped_ptr<ui::InterpolatedTransform> scale(
      new ui::InterpolatedScale(1, kWindowAnimation_Rotate_ScaleFactor));
  scoped_ptr<ui::InterpolatedTransform> scale_about_pivot(
      new ui::InterpolatedTransformAboutPivot(
          gfx::Point(xcenter, kWindowAnimation_Rotate_TranslateY),
          scale.release()));

  scoped_ptr<ui::InterpolatedTransform> translation(
      new ui::InterpolatedTranslation(gfx::Point(), gfx::Point(
          0, kWindowAnimation_Rotate_TranslateY)));

  scoped_ptr<ui::InterpolatedTransform> rotation(
      new ui::InterpolatedAxisAngleRotation(
          gfx::Vector3dF(1, 0, 0), 0, kWindowAnimation_Rotate_DegreesX));

  scale_about_pivot->SetChild(perspective.release());
  translation->SetChild(scale_about_pivot.release());
  rotation->SetChild(translation.release());
  rotation->SetReversed(show);

  scoped_ptr<ui::LayerAnimationElement> transition(
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          rotation.release(), duration));

  window->layer()->GetAnimator()->ScheduleAnimation(
      new ui::LayerAnimationSequence(transition.release()));
}

void AnimateShowWindow_Rotate(aura::Window* window) {
  AddLayerAnimationsForRotate(window, true);
}

void AnimateHideWindow_Rotate(aura::Window* window) {
  AddLayerAnimationsForRotate(window, false);
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
    case WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE:
      AnimateShowWindow_Minimize(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE:
      AnimateShowWindow_BrightnessGrayscale(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_ROTATE:
      AnimateShowWindow_Rotate(window);
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
    case WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE:
      AnimateHideWindow_Minimize(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE:
      AnimateHideWindow_BrightnessGrayscale(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_ROTATE:
      AnimateHideWindow_Rotate(window);
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
  virtual void OnCompositingStarted(ui::Compositor* compositor) OVERRIDE {
  }
  virtual void OnCompositingEnded(ui::Compositor* compositor) OVERRIDE {
  }
  virtual void OnCompositingAborted(ui::Compositor* compositor) OVERRIDE {
    // Triggers OnImplicitAnimationsCompleted() to be called and deletes us.
    layer_->GetAnimator()->StopAnimating();
  }
  virtual void OnCompositingLockStateChanged(
      ui::Compositor* compositor) OVERRIDE {
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

// Implementation of cross fading. Window is the window being cross faded. It
// should be at the target bounds. |old_layer| the previous layer from |window|.
// This takes ownership of |old_layer| and deletes when the animation is done.
// |pause_duration| is the duration to pause at the current bounds before
// animating. Returns the duration of the fade.
TimeDelta CrossFadeImpl(aura::Window* window,
                        ui::Layer* old_layer,
                        ui::Tween::Type tween_type) {
  const gfx::Rect old_bounds(old_layer->bounds());
  const gfx::Rect new_bounds(window->bounds());
  const bool old_on_top = (old_bounds.width() > new_bounds.width());

  // Shorten the animation if there's not much visual movement.
  const TimeDelta duration = GetCrossFadeDuration(old_bounds, new_bounds);

  // Scale up the old layer while translating to new position.
  {
    old_layer->GetAnimator()->StopAnimating();
    ui::ScopedLayerAnimationSettings settings(old_layer->GetAnimator());

    // Animation observer owns the old layer and deletes itself.
    settings.AddObserver(new internal::CrossFadeObserver(window, old_layer));
    settings.SetTransitionDuration(duration);
    settings.SetTweenType(tween_type);
    gfx::Transform out_transform;
    float scale_x = static_cast<float>(new_bounds.width()) /
        static_cast<float>(old_bounds.width());
    float scale_y = static_cast<float>(new_bounds.height()) /
        static_cast<float>(old_bounds.height());
    out_transform.Translate(new_bounds.x() - old_bounds.x(),
                            new_bounds.y() - old_bounds.y());
    out_transform.Scale(scale_x, scale_y);
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
  gfx::Transform in_transform;
  const float scale_x = static_cast<float>(old_bounds.width()) /
      static_cast<float>(new_bounds.width());
  const float scale_y = static_cast<float>(old_bounds.height()) /
      static_cast<float>(new_bounds.height());
  in_transform.Translate(old_bounds.x() - new_bounds.x(),
                               old_bounds.y() - new_bounds.y());
  in_transform.Scale(scale_x, scale_y);
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
    settings.SetTransitionDuration(duration);
    settings.SetTweenType(tween_type);
    window->layer()->SetTransform(gfx::Transform());
    if (!old_on_top) {
      // New layer is on top, fade it in.
      window->layer()->SetOpacity(1.f);
    }
  }
  return duration;
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

  CrossFadeImpl(window, old_layer, ui::Tween::EASE_OUT);
}

void CrossFadeWindowBetweenWorkspaces(aura::Window* new_workspace,
                                      aura::Window* window,
                                      ui::Layer* old_layer) {
  ui::Layer* layer_parent = new_workspace->layer()->parent();
  layer_parent->Add(old_layer);
  const bool restoring = old_layer->bounds().width() > window->bounds().width();
  if (restoring)
    layer_parent->StackAbove(old_layer, new_workspace->layer());
  else
    layer_parent->StackBelow(old_layer, new_workspace->layer());

  CrossFadeImpl(window, old_layer, internal::kCrossFadeTweenType);
}

TimeDelta GetCrossFadeDuration(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshWindowAnimationsDisabled))
    return base::TimeDelta();

  int old_area = old_bounds.width() * old_bounds.height();
  int new_area = new_bounds.width() * new_bounds.height();
  int max_area = std::max(old_area, new_area);
  // Avoid divide by zero.
  if (max_area == 0)
    return TimeDelta::FromMilliseconds(internal::kWorkspaceSwitchTimeMS);

  int delta_area = std::abs(old_area - new_area);
  // If the area didn't change, the animation is instantaneous.
  if (delta_area == 0)
    return TimeDelta::FromMilliseconds(internal::kWorkspaceSwitchTimeMS);

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

std::vector<ui::LayerAnimationSequence*>
CreateBrightnessGrayscaleAnimationSequence(float target_value,
                                           base::TimeDelta duration) {
  ui::Tween::Type animation_type = ui::Tween::EASE_OUT;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshBootAnimationFunction2)) {
    animation_type = ui::Tween::EASE_OUT_2;
  } else if (CommandLine::ForCurrentProcess()->HasSwitch(
                  ash::switches::kAshBootAnimationFunction3)) {
    animation_type = ui::Tween::EASE_OUT_3;
  }
  scoped_ptr<ui::LayerAnimationSequence> brightness_sequence(
      new ui::LayerAnimationSequence());
  scoped_ptr<ui::LayerAnimationSequence> grayscale_sequence(
      new ui::LayerAnimationSequence());

  scoped_ptr<ui::LayerAnimationElement> brightness_element(
      ui::LayerAnimationElement::CreateBrightnessElement(
          target_value, duration));
  brightness_element->set_tween_type(animation_type);
  brightness_sequence->AddElement(brightness_element.release());

  scoped_ptr<ui::LayerAnimationElement> grayscale_element(
      ui::LayerAnimationElement::CreateGrayscaleElement(
          target_value, duration));
  grayscale_element->set_tween_type(animation_type);
  grayscale_sequence->AddElement(grayscale_element.release());

  std::vector<ui::LayerAnimationSequence*> animations;
  animations.push_back(brightness_sequence.release());
  animations.push_back(grayscale_sequence.release());

  return animations;
}

bool AnimateWindow(aura::Window* window, WindowAnimationType type) {
  switch (type) {
    case WINDOW_ANIMATION_TYPE_BOUNCE:
      AnimateBounce(window);
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace internal
}  // namespace ash
