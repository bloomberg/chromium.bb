// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation_animator.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/display/window_tree_host_manager.h"
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

// The number of degrees that the rotation animations animate through.
const int kRotationDegrees = 20;

// The time it takes for the rotation animations to run.
const int kRotationDurationInMs = 250;

// Gets the current display rotation for the display with the specified
// |display_id|.
gfx::Display::Rotation GetCurrentRotation(int64_t display_id) {
  return Shell::GetInstance()
      ->display_manager()
      ->GetDisplayInfo(display_id)
      .GetActiveRotation();
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
      std::unique_ptr<ui::LayerTreeOwner> layer_tree_owner);
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
  // Aborts the active animations of the layer, and recurses upon its child
  // layers.
  void AbortAnimations(ui::Layer* layer);

  // The owned layer tree.
  std::unique_ptr<ui::LayerTreeOwner> layer_tree_owner_;

  // The LayerAnimationSequence that |this| has been attached to. Defaults to
  // nullptr.
  ui::LayerAnimationSequence* sequence_;

  DISALLOW_COPY_AND_ASSIGN(LayerCleanupObserver);
};

LayerCleanupObserver::LayerCleanupObserver(
    std::unique_ptr<ui::LayerTreeOwner> layer_tree_owner)
    : layer_tree_owner_(std::move(layer_tree_owner)), sequence_(nullptr) {}

LayerCleanupObserver::~LayerCleanupObserver() {
  // We must eplicitly detach from |sequence_| because we return true from
  // RequiresNotificationWhenAnimatorDestroyed.
  if (sequence_)
    sequence_->RemoveObserver(this);
  AbortAnimations(layer_tree_owner_->root());
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

void LayerCleanupObserver::AbortAnimations(ui::Layer* layer) {
  for (ui::Layer* child_layer : layer->children())
    AbortAnimations(child_layer);
  layer->GetAnimator()->AbortAllAnimations();
}

// Set the screen orientation for the given |display_id| to |new_rotation| and
// animate the change. The animation will rotate the initial orientation's
// layer towards the new orientation through |rotation_degrees| while fading
// out, and the new orientation's layer will be rotated in to the
// |new_orientation| through |rotation_degrees| arc.
void RotateScreen(int64_t display_id,
                  gfx::Display::Rotation new_rotation,
                  gfx::Display::RotationSource source) {
  aura::Window* root_window = Shell::GetInstance()
                                  ->window_tree_host_manager()
                                  ->GetRootWindowForDisplayId(display_id);

  const gfx::Display::Rotation initial_orientation =
      GetCurrentRotation(display_id);

  const gfx::Rect original_screen_bounds = root_window->GetTargetBounds();
  // 180 degree rotations should animate clock-wise.
  const int rotation_factor =
      (initial_orientation + 3) % 4 == new_rotation ? 1 : -1;

  const int old_layer_initial_rotation_degrees =
      (Is180DegreeFlip(initial_orientation, new_rotation) ? 180 : 90);

  const base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(kRotationDurationInMs);

  const gfx::Tween::Type tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;

  std::unique_ptr<ui::LayerTreeOwner> old_layer_tree =
      ::wm::RecreateLayers(root_window);

  // Add the cloned layer tree in to the root, so it will be rendered.
  root_window->layer()->Add(old_layer_tree->root());
  root_window->layer()->StackAtTop(old_layer_tree->root());

  std::unique_ptr<LayerCleanupObserver> layer_cleanup_observer(
      new LayerCleanupObserver(std::move(old_layer_tree)));

  Shell::GetInstance()->display_manager()->SetDisplayRotation(
      display_id, new_rotation, source);

  const gfx::Rect rotated_screen_bounds = root_window->GetTargetBounds();
  const gfx::Point pivot = gfx::Point(rotated_screen_bounds.width() / 2,
                                      rotated_screen_bounds.height() / 2);

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

    std::unique_ptr<ScreenRotationAnimation> screen_rotation(
        new ScreenRotationAnimation(
            child_layer, kRotationDegrees * rotation_factor,
            0 /* end_degrees */, child_layer->opacity(),
            1.0f /* target_opacity */, pivot, duration, tween_type));

    ui::LayerAnimator* animator = child_layer->GetAnimator();
    animator->set_preemption_strategy(
        ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
    std::unique_ptr<ui::LayerAnimationSequence> animation_sequence(
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

  std::unique_ptr<ScreenRotationAnimation> screen_rotation(
      new ScreenRotationAnimation(
          layer_cleanup_observer->GetRootLayer(),
          old_layer_initial_rotation_degrees * rotation_factor,
          (old_layer_initial_rotation_degrees - kRotationDegrees) *
              rotation_factor,
          layer_cleanup_observer->GetRootLayer()->opacity(),
          0.0f /* target_opacity */, pivot, duration, tween_type));

  ui::LayerAnimator* animator =
      layer_cleanup_observer->GetRootLayer()->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  std::unique_ptr<ui::LayerAnimationSequence> animation_sequence(
      new ui::LayerAnimationSequence(screen_rotation.release()));
  // Add an observer so that the cloned layers can be cleaned up with the
  // animation completes/aborts.
  animation_sequence->AddObserver(layer_cleanup_observer.release());
  animator->StartAnimation(animation_sequence.release());
}

}  // namespace

ScreenRotationAnimator::ScreenRotationAnimator(int64_t display_id)
    : display_id_(display_id) {}

ScreenRotationAnimator::~ScreenRotationAnimator() {
}

bool ScreenRotationAnimator::CanAnimate() const {
  return Shell::GetInstance()
      ->display_manager()
      ->GetDisplayForId(display_id_)
      .is_valid();
}

void ScreenRotationAnimator::Rotate(gfx::Display::Rotation new_rotation,
                                    gfx::Display::RotationSource source) {
  const gfx::Display::Rotation current_rotation =
      GetCurrentRotation(display_id_);

  if (current_rotation == new_rotation)
    return;

  RotateScreen(display_id_, new_rotation, source);
}

}  // namespace ash
