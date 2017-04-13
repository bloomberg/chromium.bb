// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_
#define ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/display/display.h"

namespace cc {
class CopyOutputResult;
}  // namespace cc

namespace ui {
class AnimationMetricsReporter;
class Layer;
class LayerTreeOwner;
}  // namespace ui

namespace ash {
namespace test {
class ScreenRotationAnimatorTestApi;
}  // namespace test

class ScreenRotationAnimatorObserver;

// Utility to perform a screen rotation with an animation.
class ASH_EXPORT ScreenRotationAnimator {
 public:
  explicit ScreenRotationAnimator(int64_t display_id);
  virtual ~ScreenRotationAnimator();

  // Rotates the display::Display specified by |display_id_| to the
  // |new_rotation| orientation, for the given |source|. The rotation will also
  // become active. Should only be called when |display_id_| references a valid
  // display::Display. |screen_rotation_animator_observer_| will be notified
  // when rotation is finished and there is no more pending rotation request.
  // Otherwise, any ongoing animation will be stopped and progressed to the
  // target position, followed by a new |Rotate()| call with the pending
  // rotation request.
  void Rotate(display::Display::Rotation new_rotation,
              display::Display::RotationSource source);

  int64_t display_id() const { return display_id_; }

  void AddScreenRotationAnimatorObserver(
      ScreenRotationAnimatorObserver* observer);
  void RemoveScreenRotationAnimatorObserver(
      ScreenRotationAnimatorObserver* observer);

  // When screen rotation animation is ended or aborted, calls |Rotate()| with
  // the pending rotation request if the request queue is not empty. Otherwise
  // notifies |screen_rotation_animator_observer_|.
  void ProcessAnimationQueue();

 protected:
  using CopyCallback =
      base::Callback<void(std::unique_ptr<cc::CopyOutputResult> result)>;
  struct ScreenRotationRequest {
    ScreenRotationRequest(int64_t id,
                          display::Display::Rotation to_rotation,
                          display::Display::RotationSource from_source)
        : id(id), new_rotation(to_rotation), source(from_source) {}
    int64_t id;
    display::Display::Rotation new_rotation;
    display::Display::RotationSource source;
  };

  // This function can be overridden in unit test to test removing external
  // display.
  virtual CopyCallback CreateAfterCopyCallback(
      std::unique_ptr<ScreenRotationRequest> rotation_request);

 private:
  friend class ash::test::ScreenRotationAnimatorTestApi;

  void StartRotationAnimation(
      std::unique_ptr<ScreenRotationRequest> rotation_request);

  // This is an asynchronous call to request copy output of root layer.
  void RequestCopyRootLayerAndAnimateRotation(
      std::unique_ptr<ScreenRotationRequest> rotation_request);

  // The callback in |RequestCopyRootLayerAndAnimateRotation()|.
  void OnRootLayerCopiedBeforeRotation(
      std::unique_ptr<ScreenRotationRequest> rotation_request,
      std::unique_ptr<cc::CopyOutputResult> result);

  // Recreates all |root_window| layers.
  void CreateOldLayerTree();

  // Requests a copy of |root_window| root layer output.
  void CopyOldLayerTree(std::unique_ptr<cc::CopyOutputResult> result);

  // Note: Only call this function when the |old_layer_tree_owner_| is set up
  // properly.
  // Sets the screen orientation to |new_rotation| and animate the change. The
  // animation will rotate the initial orientation's layer towards the new
  // orientation through |rotation_degrees| while fading out, and the new
  // orientation's layer will be rotated in to the |new_orientation| through
  // |rotation_degrees| arc.
  void AnimateRotation(std::unique_ptr<ScreenRotationRequest> rotation_request);

  void set_disable_animation_timers_for_test(bool disable_timers);

  void StopAnimating();

  // The id of the display to rotate.
  int64_t display_id_;

  // For current slow rotation animation, there are two states |ROTATING| and
  // |IDLE|. For the smooth rotation animation, we need to send copy request
  // and get copy result before animating.
  enum ScreenRotationState {
    COPY_REQUESTED,
    ROTATING,
    IDLE,
  };
  ScreenRotationState screen_rotation_state_;

  // Rotation request id, used to ignore copy request callback if we decide to
  // cancel the previous rotation request.
  int64_t rotation_request_id_;

  std::unique_ptr<ui::AnimationMetricsReporter> metrics_reporter_;
  // Only set in unittest to disable animation timers.
  bool disable_animation_timers_for_test_;
  base::ObserverList<ScreenRotationAnimatorObserver>
      screen_rotation_animator_observers_;
  std::unique_ptr<ui::LayerTreeOwner> old_layer_tree_owner_;
  std::unique_ptr<ScreenRotationRequest> last_pending_request_;
  base::WeakPtrFactory<ScreenRotationAnimator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationAnimator);
};

}  // namespace ash

#endif  // ASH_ROTATOR_SCREEN_ROTATION_ANIMATOR_H_
