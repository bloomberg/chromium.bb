// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_cycler_animator.h"

#include <algorithm>
#include <cmath>

#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/workspace/colored_window_controller.h"
#include "ash/wm/workspace/workspace.h"
#include "ash/wm/workspace/workspace_cycler_configuration.h"
#include "base/values.h"
#include "ui/aura/window.h"
#include "ui/base/events/event_utils.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"

typedef ash::WorkspaceCyclerConfiguration Config;

namespace ash {
namespace internal {

// Class which computes the transform, brightness, and visbility of workspaces
// on behalf of the animator.
class StyleCalculator {
 public:
  StyleCalculator(const gfx::Rect& screen_bounds,
                  const gfx::Rect& maximized_bounds,
                  size_t num_workspaces);
  ~StyleCalculator();

  // Returns the transform, brightness, and visibility that the workspace at
  // |workspace_index| should be animated to when the user has stopped cycling
  // through workspaces given |selected_workspace_index|.
  // Unlike GetTargetProperties(), the method does not have a |scroll_delta|
  // argument as stopping the workspace cycler always results in complately
  // selecting a workspace.
  void GetStoppedTargetProperties(size_t selected_workspace_index,
                                  size_t workspace_index,
                                  gfx::Transform* transform,
                                  float* brightness,
                                  bool* visible) const;

  // Returns the transform, brightness, and visibility that the workspace at
  // |workspace_index| should be animated to when the user is cycling through
  // workspaces given |selected_workspace_index| and |scroll_delta|.
  // |scroll_delta| is the amount of pixels that the user has scrolled
  // vertically since completely selecting the workspace at
  // |selected_workspace_index|.
  void GetTargetProperties(size_t selected_workspace_index,
                           size_t workspace_index,
                           double scroll_delta,
                           gfx::Transform* transform,
                           float* brightness,
                           bool* visible) const;

 private:
  // Returns the target animation transform of the workspace which is at
  // |offset_from_selected| from the selected workspace when the user is not
  // cycling through workspaces.
  // This method assumes |scroll_delta| == 0.
  gfx::Transform GetStoppedTargetTransformForOffset(
      int offset_from_selected) const;

  // Returns the target animation transform of the workspace which is at
  // |offset_from_selected| from the selected workspace.
  // This method like all the methods below assumes |scroll_delta| == 0 for the
  // sake of simplicity. The transform for |scroll_delta| != 0 can be obtained
  // via interpolation.
  gfx::DecomposedTransform GetTargetTransformForOffset(
      int offset_from_selected) const;

  // Returns the target animation brightness of the workspace which is at
  // |offset_from_selected| from the selected workspace.
  // This method assumes |scroll_delta| == 0.
  float GetTargetBrightnessForOffset(int offset_from_selected) const;

  // Returns the target animation visibility of the workspace with the given
  // parameters.
  // This method assumes |scroll_delta| == 0.
  bool GetTargetVisibilityForOffset(int offset_from_selected,
                                    size_t workspace_index) const;

  // Returns the minimum offset from the selected workspace at which a
  // workspace can be visible. The offset is used to limit the amount of
  // workspaces which are simultaneously visible.
  int GetMinVisibleOffsetFromSelected() const;

  // Returns the maximum offset from the selected workspace at which a
  // workspace can be visible. The offset is used to limit the amount of
  // workspaces which are simultaneously visible.
  int GetMaxVisibleOffsetFromSelected() const;

  // The bounds of the display containing the workspaces in workspace
  // coordinates, including the shelf if any.
  const gfx::Rect screen_bounds_;

  // The bounds of a maximized window. This excludes the shelf if any.
  const gfx::Rect maximized_bounds_;

  // The combined number of visible and hidden workspaces.
  size_t num_workspaces_;

  DISALLOW_COPY_AND_ASSIGN(StyleCalculator);
};

StyleCalculator::StyleCalculator(const gfx::Rect& screen_bounds,
                                 const gfx::Rect& maximized_bounds,
                                 size_t num_workspaces)
    : screen_bounds_(screen_bounds),
      maximized_bounds_(maximized_bounds),
      num_workspaces_(num_workspaces) {
}

StyleCalculator::~StyleCalculator() {
}

void StyleCalculator::GetStoppedTargetProperties(
    size_t selected_workspace_index,
    size_t workspace_index,
    gfx::Transform* transform,
    float* brightness,
    bool* visible) const {
  DCHECK_LT(selected_workspace_index, num_workspaces_);
  DCHECK_LT(workspace_index, num_workspaces_);
  int offset_from_selected = static_cast<int>(
      workspace_index - selected_workspace_index);
  if (transform)
    *transform = GetStoppedTargetTransformForOffset(offset_from_selected);

  if (brightness)
    *brightness = GetTargetBrightnessForOffset(offset_from_selected);

  // All the workspaces other than the selected workspace are either occluded by
  // the selected workspace or off screen.
  if (visible)
    *visible = (selected_workspace_index == workspace_index);
}

void StyleCalculator::GetTargetProperties(
    size_t selected_workspace_index,
    size_t workspace_index,
    double scroll_delta,
    gfx::Transform* transform,
    float* brightness,
    bool* visible) const {
  DCHECK_LT(selected_workspace_index, num_workspaces_);
  DCHECK_LT(workspace_index, num_workspaces_);

  int offset_from_selected = static_cast<int>(
      workspace_index - selected_workspace_index);

  int first_offset_from_selected = offset_from_selected;
  int second_offset_from_selected = offset_from_selected;
  if (scroll_delta < 0) {
    // The user is part of the way to selecting the workspace at
    // |selected_workspace_index| - 1 -> |offset_from_selected| + 1.
    second_offset_from_selected = offset_from_selected + 1;
  } else if (scroll_delta > 0) {
    // The user is part of the way to selecting the workspace at
    // |selected_workspace_index| + 1 -> |offset_from_selected| - 1.
    second_offset_from_selected = offset_from_selected - 1;
  }

  double scroll_distance_to_cycle_to_next_workspace = Config::GetDouble(
      Config::SCROLL_DISTANCE_TO_CYCLE_TO_NEXT_WORKSPACE);
  double progress = fabs(
      scroll_delta / scroll_distance_to_cycle_to_next_workspace);
  DCHECK_GT(1.0, progress);

  if (transform) {
    gfx::DecomposedTransform first_transform = GetTargetTransformForOffset(
        first_offset_from_selected);

    gfx::DecomposedTransform interpolated_transform;
    if (first_offset_from_selected == second_offset_from_selected) {
      interpolated_transform = first_transform;
    } else {
      gfx::DecomposedTransform second_transform = GetTargetTransformForOffset(
          second_offset_from_selected);
      gfx::BlendDecomposedTransforms(&interpolated_transform,
                                     second_transform,
                                     first_transform,
                                     progress);
    }
    *transform = gfx::ComposeTransform(interpolated_transform);
  }

  if (brightness) {
    float first_brightness = GetTargetBrightnessForOffset(
        first_offset_from_selected);
    float second_brightness = GetTargetBrightnessForOffset(
        second_offset_from_selected);
    *brightness = first_brightness + progress *
        (second_brightness - first_brightness);
  }

  if (visible) {
    bool first_visible = GetTargetVisibilityForOffset(
        first_offset_from_selected, workspace_index);
    bool second_visible = GetTargetVisibilityForOffset(
        second_offset_from_selected, workspace_index);
    *visible = (first_visible || second_visible);
  }
}

gfx::Transform StyleCalculator::GetStoppedTargetTransformForOffset(
    int offset_from_selected) const {
  if (offset_from_selected <= 0) {
    // The selected workspace takes up the entire screen. The workspaces deeper
    // than the selected workspace are stacked exactly under the selected
    // workspace and are completely occluded by it.
    return gfx::Transform();
  }

  // The workspaces shallower than the selected workspace are stacked exactly
  // on top of each other offscreen.
  double max_scale = Config::GetDouble(Config::MAX_SCALE);

  gfx::Transform transform;
  transform.Translate(0, screen_bounds_.height());
  transform.Scale(max_scale, max_scale);
  return transform;
}

gfx::DecomposedTransform StyleCalculator::GetTargetTransformForOffset(
    int offset_from_selected) const {
  // When cycling, the workspaces are spread out from the positions computed by
  // GetStoppedTargetTransformForOffset(). The transforms are computed so that
  // on screen the workspaces look like this:
  //  ____________________________________________
  // |          ________________________          |
  // |         |    deepest workpace    |_________|_
  // |        _|________________________|_        | |
  // |       |                            |       | |
  // |      _|____________________________|_      | |_ top stack.
  // |     | selected / shallowest workspace|     | |
  // |     |                                |     | |
  // |     |                                |_____|_|
  // |     |                                |     |
  // |     |                                |     |
  // |    _|________________________________|_    |
  // |   |           deepest workspace        |___|_
  // |  _|____________________________________|_  | |_ bottom stack.
  // | |           shallowest workspace         |_|_|
  // |_|________________________________________|_|
  // The selected workspace is the most visible workspace. It is the workspace
  // which will be activated if the user does not do any more cycling.
  bool in_top_stack = (offset_from_selected <= 0);

  int min_visible_offset_from_selected = GetMinVisibleOffsetFromSelected();
  int max_visible_offset_from_selected = GetMaxVisibleOffsetFromSelected();

  // Give workspaces at hidden offsets the transform of the workspace at the
  // nearest visible offset. This is needed to produce a valid result when
  // interpolating between the values of GetTargetTransformForOffset() for a
  // visible and hidden offset.
  if (offset_from_selected < min_visible_offset_from_selected)
    offset_from_selected = min_visible_offset_from_selected;
  else if (offset_from_selected > max_visible_offset_from_selected)
    offset_from_selected = max_visible_offset_from_selected;

  double selected_scale = Config::GetDouble(Config::SELECTED_SCALE);

  double scale = selected_scale;
  if (in_top_stack) {
    double min_scale = Config::GetDouble(Config::MIN_SCALE);
    scale -= static_cast<double>(offset_from_selected) /
        min_visible_offset_from_selected * (selected_scale - min_scale);
  } else {
    double max_scale = Config::GetDouble(Config::MAX_SCALE);
    scale += static_cast<double>(offset_from_selected) /
        max_visible_offset_from_selected * (max_scale - selected_scale);
  }

  // Compute the workspace's y offset.
  double y_offset = 0;
  if (offset_from_selected == 0) {
    y_offset = Config::GetDouble(Config::SELECTED_Y_OFFSET);
  } else {
    Config::Property y_offsets_property;
    if (in_top_stack) {
      y_offsets_property = Config::DEEPER_THAN_SELECTED_Y_OFFSETS;
    } else {
      y_offsets_property = Config::SHALLOWER_THAN_SELECTED_Y_OFFSETS;
      y_offset = maximized_bounds_.height();
    }
    const base::ListValue& y_offsets = Config::GetListValue(y_offsets_property);
    size_t y_offset_index = static_cast<size_t>(
        std::abs(offset_from_selected) - 1);
    DCHECK_GT(y_offsets.GetSize(), y_offset_index);
    double y_offset_config_value = 0;
    y_offsets.GetDouble(y_offset_index, &y_offset_config_value);
    y_offset += y_offset_config_value;
  }

  // Center the workspace horizontally.
  double x_offset = maximized_bounds_.width() * (1 - scale) / 2;

  gfx::DecomposedTransform transform;
  transform.translate[0] = x_offset;
  transform.translate[1] = y_offset;
  transform.scale[0] = scale;
  transform.scale[1] = scale;
  return transform;
}

float StyleCalculator::GetTargetBrightnessForOffset(
    int offset_from_selected) const {
  int max_visible_distance_from_selected = std::max(
      std::abs(GetMinVisibleOffsetFromSelected()),
      GetMaxVisibleOffsetFromSelected());
  return Config::GetDouble(Config::MIN_BRIGHTNESS) * std::min(
      1.0,
      static_cast<double>(std::abs(offset_from_selected)) /
          max_visible_distance_from_selected);
}

bool StyleCalculator::GetTargetVisibilityForOffset(
    int offset_from_selected,
    size_t workspace_index) const {
  // The workspace at the highest possible index is the shallowest workspace
  // out of both stacks and is always visible.
  // The workspace at |GetMaxVisibleOffsetFromSelected()| is hidden because it
  // has the same transform as the shallowest workspace and is completely
  // occluded by it.
  if (workspace_index == num_workspaces_ - 1)
    return true;

  return offset_from_selected >= GetMinVisibleOffsetFromSelected() &&
      offset_from_selected < GetMaxVisibleOffsetFromSelected();
}

int StyleCalculator::GetMinVisibleOffsetFromSelected() const {
  const base::ListValue& y_offsets = Config::GetListValue(
      Config::DEEPER_THAN_SELECTED_Y_OFFSETS);
  // Hide workspaces for which there is no y offset specified.
  return -1 * static_cast<int>(y_offsets.GetSize());
}

int StyleCalculator::GetMaxVisibleOffsetFromSelected() const {
  const base::ListValue& y_offsets = Config::GetListValue(
      Config::SHALLOWER_THAN_SELECTED_Y_OFFSETS);
  // Hide workspaces for which there is no y offset specified.
  return y_offsets.GetSize();
}

WorkspaceCyclerAnimator::WorkspaceCyclerAnimator(Delegate* delegate)
    : delegate_(delegate),
      initial_active_workspace_index_(0),
      selected_workspace_index_(0),
      scroll_delta_(0),
      animation_type_(NONE),
      launcher_background_controller_(NULL),
      style_calculator_(NULL) {
}

WorkspaceCyclerAnimator::~WorkspaceCyclerAnimator() {
  StopObservingImplicitAnimations();
  animation_type_ = NONE;
}

void WorkspaceCyclerAnimator::Init(const std::vector<Workspace*>& workspaces,
                                   Workspace* initial_active_workspace) {
  workspaces_ = workspaces;

  std::vector<Workspace*>::iterator it = std::find(workspaces_.begin(),
      workspaces_.end(), initial_active_workspace);
  DCHECK(it != workspaces_.end());
  initial_active_workspace_index_ = it - workspaces_.begin();
  selected_workspace_index_ = initial_active_workspace_index_;

  screen_bounds_ = ScreenAsh::GetDisplayBoundsInParent(
      workspaces_[0]->window());
  maximized_bounds_ = ScreenAsh::GetMaximizedWindowBoundsInParent(
      workspaces_[0]->window());
  style_calculator_.reset(new StyleCalculator(
      screen_bounds_, maximized_bounds_, workspaces_.size()));
}

void WorkspaceCyclerAnimator::AnimateStartingCycler() {
  // Ensure that the workspaces are stacked with respect to their order
  // in |workspaces_|.
  aura::Window* parent = workspaces_[0]->window()->parent();
  DCHECK(parent);
  for (size_t i = 0; i < workspaces_.size() - 1; ++i) {
    parent->StackChildAbove(workspaces_[i + 1]->window(),
                            workspaces_[i]->window());
  }

  // Set the initial transform and brightness of the workspaces such that they
  // animate correctly when AnimateToUpdatedState() is called after the loop.
  // Start at index 1 as the desktop workspace is not animated.
  for (size_t i = 1; i < workspaces_.size(); ++i) {
    aura::Window* window = workspaces_[i]->window();
    ui::Layer* layer = window->layer();

    gfx::Transform transform;
    float brightness = 0.0f;
    style_calculator_->GetStoppedTargetProperties(selected_workspace_index_, i,
        &transform, &brightness, NULL);
    layer->SetTransform(transform);
    layer->SetLayerBrightness(brightness);
  }

  int start_cycler_animation_duration = static_cast<int>(Config::GetDouble(
      Config::START_CYCLER_ANIMATION_DURATION));

  scroll_delta_ = 0;
  animation_type_ = CYCLER_START;
  AnimateToUpdatedState(start_cycler_animation_duration);

  // Dim the desktop workspace and the desktop background.
  AnimateTo(0, false, start_cycler_animation_duration, gfx::Transform(),
      Config::GetDouble(Config::DESKTOP_WORKSPACE_BRIGHTNESS), true);

  aura::Window* background = GetDesktopBackground();
  if (background) {
    ui::Layer* layer = background->layer();

    if (!background->IsVisible()) {
      background->Show();
      layer->SetOpacity(0.0f);
    }

    {
      ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
      settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          start_cycler_animation_duration));

      layer->SetOpacity(Config::GetDouble(Config::BACKGROUND_OPACITY));
    }
  }

  // Create a window to simulate a fully opaque launcher. This prevents
  // workspaces from showing from behind the launcher.
  CreateLauncherBackground();
}

void WorkspaceCyclerAnimator::AnimateStoppingCycler() {
  if (scroll_delta_ != 0) {
    // Completely select the workspace at |selected_workspace_index_|.
    int animation_duration = GetAnimationDurationForChangeInScrollDelta(
        -scroll_delta_);
    scroll_delta_ = 0;
    animation_type_ = CYCLER_COMPLETELY_SELECT;
    AnimateToUpdatedState(animation_duration);
    return;
  }

  int stop_cycler_animation_duration = static_cast<int>(Config::GetDouble(
      Config::STOP_CYCLER_ANIMATION_DURATION));

  animation_type_ = CYCLER_END;
  AnimateToUpdatedState(stop_cycler_animation_duration);

  AnimateTo(0, false, stop_cycler_animation_duration, gfx::Transform(),
      0.0f, true);

  aura::Window* background = GetDesktopBackground();
  if (background) {
    ui::Layer* layer = background->layer();
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        stop_cycler_animation_duration));

    layer->SetOpacity((selected_workspace_index_ == 0) ? 1.0f : 0.0f);
  }
}

void WorkspaceCyclerAnimator::AbortAnimations() {
  StopObservingImplicitAnimations();
  animation_type_ = NONE;
  CyclerStopped(initial_active_workspace_index_);
}

void WorkspaceCyclerAnimator::AnimateCyclingByScrollDelta(double scroll_delta) {
  if (scroll_delta == 0)
    return;

  // Drop any updates received while an animation is running.
  // TODO(pkotwicz): Do something better.
  if (animation_type_ != NONE)
    return;

  if (ui::IsNaturalScrollEnabled())
    scroll_delta *= -1;

  double old_scroll_delta = scroll_delta_;
  scroll_delta_ += scroll_delta;

  double scroll_distance_to_cycle_to_next_workspace = Config::GetDouble(
      Config::SCROLL_DISTANCE_TO_CYCLE_TO_NEXT_WORKSPACE);

  double min_scroll_delta = -1 *
      static_cast<double>(selected_workspace_index_) *
      scroll_distance_to_cycle_to_next_workspace;
  double max_scroll_delta = static_cast<double>(
      workspaces_.size() - 1 - selected_workspace_index_) *
      scroll_distance_to_cycle_to_next_workspace;

  if (scroll_delta_ < min_scroll_delta)
    scroll_delta_ = min_scroll_delta;
  else if (scroll_delta_ > max_scroll_delta)
    scroll_delta_ = max_scroll_delta;

  if (scroll_delta_ == old_scroll_delta)
    return;

  // Set the selected workspace to the workspace that the user is closest to
  // selecting completely. A workspace is completely selected when the user
  // has scrolled the exact amount to select the workspace with no undershoot /
  // overshoot.
  int workspace_change = floor(scroll_delta_ /
      scroll_distance_to_cycle_to_next_workspace + .5);
  selected_workspace_index_ += workspace_change;

  // Set |scroll_delta_| to the amount of undershoot / overshoot.
  scroll_delta_ -= workspace_change *
      scroll_distance_to_cycle_to_next_workspace;

  int animation_duration = GetAnimationDurationForChangeInScrollDelta(
      scroll_delta_ - old_scroll_delta);

  animation_type_ = CYCLER_UPDATE;
  AnimateToUpdatedState(animation_duration);
}

void WorkspaceCyclerAnimator::AnimateToUpdatedState(int animation_duration) {
  DCHECK_NE(NONE, animation_type_);

  bool animator_to_wait_for_selected = false;

  // Start at index 1, as the animator does not animate the desktop workspace.
  for (size_t i = 1; i < workspaces_.size(); ++i) {
    gfx::Transform transform;
    float brightness = 0.0f;
    bool visible = false;
    if (animation_type_ == CYCLER_END) {
      DCHECK_EQ(0, scroll_delta_);
      style_calculator_->GetStoppedTargetProperties(selected_workspace_index_,
          i, &transform, &brightness, &visible);
    } else {
      style_calculator_->GetTargetProperties(selected_workspace_index_, i,
          scroll_delta_, &transform, &brightness, &visible);
    }

    aura::Window* window = workspaces_[i]->window();
    int workspace_animation_duration = animation_duration;
    if (!window->IsVisible()) {
      if (visible) {
        // The workspace's previous state is unknown, set the state immediately.
        workspace_animation_duration = 0;
      } else {
        // Don't bother animating workspaces which aren't visible.
        continue;
      }
    }
    // Hide the workspace after |animation_duration| in the case of
    // |visible| == false as the workspace to be hidden may not be completely
    // occluded till the animation has completed.

    bool wait_for_animator = false;
    if (!animator_to_wait_for_selected && workspace_animation_duration != 0) {
      // The completion of the animations for this workspace will be used to
      // indicate that the animations for all workspaces have completed.
      wait_for_animator = true;
      animator_to_wait_for_selected = true;
    }

    AnimateTo(i, wait_for_animator, workspace_animation_duration,
              transform, brightness, visible);
  }

  // All of the animations started by this method were of zero duration.
  // Call the animation callback.
  if (!animator_to_wait_for_selected)
    OnImplicitAnimationsCompleted();
}

void WorkspaceCyclerAnimator::CyclerStopped(size_t visible_workspace_index) {
  for(size_t i = 0; i < workspaces_.size(); ++i) {
    aura::Window* window = workspaces_[i]->window();
    ui::Layer* layer = window->layer();
    layer->SetLayerBrightness(0.0f);
    layer->SetTransform(gfx::Transform());

    if (i == visible_workspace_index)
      window->Show();
    else
      window->Hide();
  }

  aura::Window* background = GetDesktopBackground();
  if (background) {
    background->layer()->SetOpacity(1.0);
    if (visible_workspace_index != 0u)
      background->Hide();
  }

  launcher_background_controller_.reset();
}

void WorkspaceCyclerAnimator::AnimateTo(size_t workspace_index,
                                        bool wait_for_animation_to_complete,
                                        int animation_duration,
                                        const gfx::Transform& target_transform,
                                        float target_brightness,
                                        bool target_visibility) {
  aura::Window* window = workspaces_[workspace_index]->window();
  ui::Layer* layer = window->layer();
  ui::LayerAnimator* animator = layer->GetAnimator();
  ui::ScopedLayerAnimationSettings settings(animator);

  if (wait_for_animation_to_complete) {
    StopObservingImplicitAnimations();
    DCHECK_NE(animation_type_, NONE);
    settings.AddObserver(this);
  }

  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      animation_duration));

  if (target_visibility) {
    window->Show();

    // Set the opacity in case the layer has some weird initial state.
    layer->SetOpacity(1.0f);
  } else {
    window->Hide();
  }

  layer->SetTransform(target_transform);
  layer->SetLayerBrightness(target_brightness);
}

int WorkspaceCyclerAnimator::GetAnimationDurationForChangeInScrollDelta(
    double change) const {
  double ratio = Config::GetDouble(
      Config::CYCLER_STEP_ANIMATION_DURATION_RATIO);
  return static_cast<int>(fabs(change) * ratio);
}

void WorkspaceCyclerAnimator::CreateLauncherBackground() {
  if (screen_bounds_ == maximized_bounds_)
    return;

  aura::Window* random_workspace_window = workspaces_[0]->window();
  ash::Launcher* launcher = ash::Launcher::ForWindow(random_workspace_window);
  aura::Window* launcher_window = launcher->widget()->GetNativeWindow();

  // TODO(pkotwicz): Figure out what to do when the launcher visible state is
  // SHELF_AUTO_HIDE.
  ShelfLayoutManager* shelf_layout_manager =
      ShelfLayoutManager::ForLauncher(launcher_window);
  if (!shelf_layout_manager->IsVisible())
    return;

  gfx::Rect shelf_bounds = shelf_layout_manager->GetIdealBounds();

  launcher_background_controller_.reset(new ColoredWindowController(
      launcher_window->parent(), "LauncherBackground"));
  launcher_background_controller_->SetColor(SK_ColorBLACK);
  aura::Window* tint_window =
      launcher_background_controller_->GetWidget()->GetNativeWindow();
  tint_window->SetBounds(shelf_bounds);
  launcher_window->parent()->StackChildBelow(tint_window, launcher_window);
  tint_window->Show();
}

aura::Window* WorkspaceCyclerAnimator::GetDesktopBackground() const {
  RootWindowController* root_controller = GetRootWindowController(
      workspaces_[0]->window()->GetRootWindow());
  if (!root_controller)
    return NULL;

  return root_controller->GetContainer(
      kShellWindowId_DesktopBackgroundContainer);
}

void WorkspaceCyclerAnimator::NotifyDelegate(
    AnimationType completed_animation) {
  if (completed_animation == CYCLER_START)
    delegate_->StartWorkspaceCyclerAnimationFinished();
  else if (completed_animation == CYCLER_END)
    delegate_->StopWorkspaceCyclerAnimationFinished();
}

void WorkspaceCyclerAnimator::OnImplicitAnimationsCompleted() {
  AnimationType completed_animation = animation_type_;
  animation_type_ = NONE;

  if (completed_animation == CYCLER_COMPLETELY_SELECT)
    AnimateStoppingCycler();
  else if (completed_animation == CYCLER_END)
    CyclerStopped(selected_workspace_index_);

  if (completed_animation == CYCLER_START ||
      completed_animation == CYCLER_END) {
    // Post a task to notify the delegate of the animation completion because
    // the delegate may delete |this| as a result of getting notified.
    MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        base::Bind(&WorkspaceCyclerAnimator::NotifyDelegate,
                   AsWeakPtr(),
                   completed_animation));
  }
}

}  // namespace internal
}  // namespace ash
