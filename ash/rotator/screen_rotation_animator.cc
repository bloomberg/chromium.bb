// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animator.h"

#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/rotator/screen_rotation_animation.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Switch value for an animation that will rotate the initial orientation's
// layer towards the new orientation and the new orientation's layer in to
// position from the initial orientation through an arc of
// |kPartialRotationDegrees| degrees. The initial orientation's layer will be
// faded out as well.
const char kPartialRotation[] = "partial-rotation";

// Switch value for the same animation as |kPartialRotation| but with a much
// longer duration.
const char kPartialRotationSlow[] = "partial-rotation-slow";

// Switch value for an animation that will rotate both the initial and target
// orientation's layers from the initial orientation into the target
// orientation. The initial orientation's layer will be faded out as well and an
// interpolated scaling factor is applied to the animation so that the initial
// and target layers are both the same size throughout the animation.
const char kFullRotation[] = "full-rotation";

// Switch value for the same animation as |kFullRotation| but with a much longer
// duration.
const char kFullRotationSlow[] = "full-rotation-slow";

// The number of degrees the partial rotation animations animate through.
const int kPartialRotationDegrees = 20;

// The time it takes for the normal speed rotation animations to run.
const int kRotationDurationInMs = 250;

// The time it takes for the slow speed rotation animations to run.
const int kRotationDurationSlowInMs = 2500;

// Gets the current display rotation for the display with the specified
// |display_id|.
gfx::Display::Rotation GetCurrentRotation(int64 display_id) {
  return Shell::GetInstance()
      ->display_manager()
      ->GetDisplayInfo(display_id)
      .rotation();
}

// Returns true if the rotation between |initial_rotation| and |new_rotation| is
// 180 degrees.
bool Is180DegreeFlip(gfx::Display::Rotation initial_rotation,
                     gfx::Display::Rotation new_rotation) {
  return (initial_rotation + 2) % 4 == new_rotation;
}

// A LayerAnimationObserver that will destroy the contained LayerTreeOwner when
// notified that a layer animation has ended or was aborted.
class LayerCleanupObserver : public ui::LayerAnimationObserver {
 public:
  explicit LayerCleanupObserver(
      scoped_ptr<ui::LayerTreeOwner> layer_tree_owner);
  ~LayerCleanupObserver() override;

  // Get the root layer of the owned layer tree.
  ui::Layer* GetRootLayer();

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

 protected:
  // ui::LayerAnimationObserver:
  bool RequiresNotificationWhenAnimatorDestroyed() const override {
    return true;
  }
  void OnAttachedToSequence(ui::LayerAnimationSequence* sequence) override;
  void OnDetachedFromSequence(ui::LayerAnimationSequence* sequence) override;

 private:
  // The owned layer tree.
  scoped_ptr<ui::LayerTreeOwner> layer_tree_owner_;

  // The LayerAnimationSequence that |this| has been attached to. Defaults to
  // nullptr.
  ui::LayerAnimationSequence* sequence_;

  DISALLOW_COPY_AND_ASSIGN(LayerCleanupObserver);
};

LayerCleanupObserver::LayerCleanupObserver(
    scoped_ptr<ui::LayerTreeOwner> layer_tree_owner)
    : layer_tree_owner_(layer_tree_owner.Pass()), sequence_(nullptr) {
}

LayerCleanupObserver::~LayerCleanupObserver() {
  // We must eplicitly detach from |sequence_| because we return true from
  // RequiresNotificationWhenAnimatorDestroyed.
  if (sequence_)
    sequence_->RemoveObserver(this);
}

ui::Layer* LayerCleanupObserver::GetRootLayer() {
  return layer_tree_owner_->root();
}

void LayerCleanupObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  delete this;
}

void LayerCleanupObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  delete this;
}

void LayerCleanupObserver::OnAttachedToSequence(
    ui::LayerAnimationSequence* sequence) {
  sequence_ = sequence;
}

void LayerCleanupObserver::OnDetachedFromSequence(
    ui::LayerAnimationSequence* sequence) {
  DCHECK_EQ(sequence, sequence_);
  sequence_ = nullptr;
}

// Set the screen orientation for the given |display| to |new_rotation| and
// animate the change.
void RotateScreen(int64 display_id,
                  gfx::Display::Rotation new_rotation,
                  base::TimeDelta duration,
                  int rotation_degrees,
                  int rotation_degree_offset,
                  gfx::Tween::Type tween_type,
                  bool should_scale) {
  aura::Window* root_window =
      Shell::GetInstance()->display_controller()->GetRootWindowForDisplayId(
          display_id);

  const gfx::Display::Rotation initial_orientation =
      GetCurrentRotation(display_id);

  const gfx::RectF original_screen_bounds = root_window->GetTargetBounds();
  // 180 degree rotations should animate clock-wise.
  const int rotation_factor =
      (initial_orientation + 3) % 4 == new_rotation ? 1 : -1;

  scoped_ptr<ui::LayerTreeOwner> old_layer_tree =
      wm::RecreateLayers(root_window);

  // Add the cloned layer tree in to the root, so it will be rendered.
  root_window->layer()->Add(old_layer_tree->root());
  root_window->layer()->StackAtTop(old_layer_tree->root());

  scoped_ptr<LayerCleanupObserver> layer_cleanup_observer(
      new LayerCleanupObserver(old_layer_tree.Pass()));

  Shell::GetInstance()->display_manager()->SetDisplayRotation(display_id,
                                                              new_rotation);

  const gfx::RectF rotated_screen_bounds = root_window->GetTargetBounds();
  const gfx::Point pivot = gfx::Point(rotated_screen_bounds.width() / 2,
                                      rotated_screen_bounds.height() / 2);

  gfx::Point3F new_layer_initial_scale = gfx::Point3F(1.0f, 1.0f, 1.0f);
  gfx::Point3F old_layer_target_scale = gfx::Point3F(1.0f, 1.0f, 1.0f);

  if (should_scale) {
    new_layer_initial_scale = gfx::Point3F(
        original_screen_bounds.width() / rotated_screen_bounds.width(),
        original_screen_bounds.height() / rotated_screen_bounds.height(), 1.0f);

    old_layer_target_scale = gfx::Point3F(
        rotated_screen_bounds.width() / original_screen_bounds.width(),
        rotated_screen_bounds.height() / original_screen_bounds.height(), 1.0f);
  }

  // We must animate each non-cloned child layer individually because the cloned
  // layer was added as a child to |root_window|'s layer so that it will be
  // rendered.
  // TODO(bruthig): Add a NOT_DRAWN layer in between the root_window's layer and
  // its current children so that we only need to initiate two
  // LayerAnimationSequences. One for the new layers and one for the old layer.
  for (ui::Layer* child_layer : root_window->layer()->children()) {
    // Skip the cloned layer because it has a different animation.
    if (child_layer == layer_cleanup_observer->GetRootLayer())
      continue;

    scoped_ptr<ScreenRotationAnimation> screen_rotation(
        new ScreenRotationAnimation(
            child_layer, rotation_degrees * rotation_factor,
            0 /* end_degrees */, child_layer->opacity(),
            1.0f /* target_opacity */, new_layer_initial_scale,
            gfx::Point3F(1.0f, 1.0f, 1.0f), pivot, duration, tween_type));

    ui::LayerAnimator* animator = child_layer->GetAnimator();
    animator->set_preemption_strategy(
        ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
    scoped_ptr<ui::LayerAnimationSequence> animation_sequence(
        new ui::LayerAnimationSequence(screen_rotation.release()));
    animator->StartAnimation(animation_sequence.release());
  }

  // The old layer will also be transformed into the new orientation. We will
  // translate it so that the old layer's center point aligns with the new
  // orientation's center point and use that center point as the pivot for the
  // rotation animation.
  gfx::Transform translate_transform;
  translate_transform.Translate(
      (rotated_screen_bounds.width() - original_screen_bounds.width()) / 2,
      (rotated_screen_bounds.height() - original_screen_bounds.height()) / 2);
  layer_cleanup_observer->GetRootLayer()->SetTransform(translate_transform);

  scoped_ptr<ScreenRotationAnimation> screen_rotation(
      new ScreenRotationAnimation(
          layer_cleanup_observer->GetRootLayer(),
          (rotation_degrees + rotation_degree_offset) * rotation_factor,
          rotation_degree_offset * rotation_factor,
          layer_cleanup_observer->GetRootLayer()->opacity(),
          0.0f /* target_opacity */, gfx::Point3F(1.0f, 1.0f, 1.0f),
          old_layer_target_scale, pivot, duration, tween_type));

  ui::LayerAnimator* animator =
      layer_cleanup_observer->GetRootLayer()->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  scoped_ptr<ui::LayerAnimationSequence> animation_sequence(
      new ui::LayerAnimationSequence(screen_rotation.release()));
  // Add an observer so that the cloned layers can be cleaned up with the
  // animation completes/aborts.
  animation_sequence->AddObserver(layer_cleanup_observer.release());
  animator->StartAnimation(animation_sequence.release());
}

}  // namespace

ScreenRotationAnimator::ScreenRotationAnimator(int64 display_id)
    : display_id_(display_id) {
}

ScreenRotationAnimator::~ScreenRotationAnimator() {
}

void ScreenRotationAnimator::Rotate(gfx::Display::Rotation new_rotation) {
  const gfx::Display::Rotation current_rotation =
      GetCurrentRotation(display_id_);

  if (current_rotation == new_rotation)
    return;

  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAshEnableScreenRotationAnimation);

  if (switch_value.empty()) {
    Shell::GetInstance()->display_manager()->SetDisplayRotation(display_id_,
                                                                new_rotation);
    return;
  }

  if (kPartialRotation == switch_value ||
      kPartialRotationSlow == switch_value) {
    const int durationInMs = (kPartialRotation == switch_value)
                                 ? kRotationDurationInMs
                                 : kRotationDurationSlowInMs;
    const int rotation_degree_offset =
        Is180DegreeFlip(current_rotation, new_rotation)
            ? 180 - kPartialRotationDegrees
            : 90 - kPartialRotationDegrees;

    RotateScreen(display_id_, new_rotation,
                 base::TimeDelta::FromMilliseconds(durationInMs),
                 kPartialRotationDegrees, rotation_degree_offset,
                 gfx::Tween::FAST_OUT_LINEAR_IN, false /* should_scale */);
  } else if (kFullRotation == switch_value ||
             kFullRotationSlow == switch_value) {
    const int durationInMs = (kFullRotation == switch_value)
                                 ? kRotationDurationInMs
                                 : kRotationDurationSlowInMs;
    const int rotation_degrees =
        Is180DegreeFlip(current_rotation, new_rotation) ? 180 : 90;

    RotateScreen(display_id_, new_rotation,
                 base::TimeDelta::FromMilliseconds(durationInMs),
                 rotation_degrees, 0 /* rotation_degree_offset */,
                 gfx::Tween::FAST_OUT_LINEAR_IN, true /* should_scale */);
  } else {
    NOTREACHED();
  }
}

}  // namespace ash
