// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_BACKGROUND_ANIMATOR_H_
#define ASH_COMMON_SHELF_SHELF_BACKGROUND_ANIMATOR_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_types.h"
#include "ash/common/shelf/wm_shelf_observer.h"
#include "ash/common/wm/background_animator.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class ShelfBackgroundAnimatorObserver;
class ShelfBackgroundAnimatorTestApi;
class WmShelf;

// Central controller for the Shelf and Dock opacity animations.
//
// The ShelfBackgroundAnimator is capable of observing a WmShelf instance for
// background type changes or clients can call PaintBackground() directly.
//
// The Shelf uses 3 surfaces for the animations:
//
//  Non-Material Design:
//    1. Shelf button backgrounds
//    2. Opaque overlay for the SHELF_BACKGROUND_MAXIMIZED state.
//    3. Shelf and Dock assets for the SHELF_BACKGROUND_OVERLAP state.
//  Material Design:
//    1. Shelf button backgrounds
//    2. Opaque overlay for the SHELF_BACKGROUND_OVERLAP and
//       SHELF_BACKGROUND_MAXIMIZED states.
class ASH_EXPORT ShelfBackgroundAnimator : public WmShelfObserver,
                                           public BackgroundAnimatorDelegate {
 public:
  // Initializes this with the given |background_type|. This will observe the
  // |wm_shelf| for background type changes if |wm_shelf| is not null.
  ShelfBackgroundAnimator(ShelfBackgroundType background_type,
                          WmShelf* wm_shelf);
  ~ShelfBackgroundAnimator() override;

  ShelfBackgroundType target_background_type() const {
    return target_background_type_;
  }

  // Initializes |observer| with current values using the Initialize() function.
  void AddObserver(ShelfBackgroundAnimatorObserver* observer);
  void RemoveObserver(ShelfBackgroundAnimatorObserver* observer);

  // Updates |observer| with current values.
  void Initialize(ShelfBackgroundAnimatorObserver* observer) const;

  // Conditionally animates the background to the specified |background_type|
  // and notifies observers of the new background parameters (e.g. alpha).
  // If |change_type| is BACKGROUND_CHANGE_IMMEDIATE then the
  // observers will only receive one notification with the final background
  // state, otherwise the observers will be notified multiple times in order to
  // animate the changes to the backgrounds.
  //
  // NOTE: If a second request to paint the same |background_type| using the
  // BACKGROUND_CHANGE_ANIMATE change type is received it will be ignored and
  // observers will NOT be notified.
  void PaintBackground(ShelfBackgroundType background_type,
                       BackgroundAnimatorChangeType change_type);

 protected:
  // WmShelfObserver:
  void OnBackgroundTypeChanged(
      ShelfBackgroundType background_type,
      BackgroundAnimatorChangeType change_type) override;

  // BackgroundAnimatorDelegate:
  void UpdateBackground(BackgroundAnimator* animator, int alpha) override;
  void BackgroundAnimationEnded(BackgroundAnimator* animator) override;

 private:
  friend class ShelfBackgroundAnimatorTestApi;

  // Helper function used by PaintBackground() to animate the background.
  void AnimateBackground(ShelfBackgroundType background_type,
                         BackgroundAnimatorChangeType change_type);

  // Creates new BackgroundAnimators configured with the correct durations and
  // initial/target alpha values. If any BackgroundAnimators are currently
  // animating they will be stopped.
  void CreateAnimators(ShelfBackgroundType background_type,
                       BackgroundAnimatorChangeType change_type);

  // Stops all existing animators owned by this.
  void StopAnimators();

  // Called when an alpha value changes and observers need to be notified.
  void OnAlphaChanged(BackgroundAnimator* animator, int alpha);

  // The background type that this is animating towards or has reached.
  ShelfBackgroundType target_background_type_ = SHELF_BACKGROUND_DEFAULT;

  // The last background type this is animating away from.
  ShelfBackgroundType previous_background_type_ = SHELF_BACKGROUND_MAXIMIZED;

  // Animates the solid color background of the Shelf.
  // TODO(bruthig): Replace all BackgroundAnimators with a single
  // gfx::SlideAnimation.
  std::unique_ptr<BackgroundAnimator> opaque_background_animator_;

  // Animates the asset/image based background of the Shelf.
  // TODO(bruthig): Remove when non-md is no longer needed (crbug.com/614453).
  std::unique_ptr<BackgroundAnimator> asset_background_animator_;

  // Animates the backgrounds of Shelf child Views.
  std::unique_ptr<BackgroundAnimator> item_background_animator_;

  base::ObserverList<ShelfBackgroundAnimatorObserver> observers_;

  // True if the existing BackgroundAnimators can be re-used to animate to the
  // |previous_background_type_|. This allows for pre-empted animations to take
  // the same amount of time to reverse to the |previous_background_type_|.
  bool can_reuse_animators_ = false;

  // Tracks how many animators completed successfully since the last
  // CreateAnimators() call. Used to properly set |can_reuse_animators_|.
  int successful_animator_count_ = 0;

  // The shelf to observe for changes to the shelf background type, can be null.
  WmShelf* wm_shelf_;

  DISALLOW_COPY_AND_ASSIGN(ShelfBackgroundAnimator);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_BACKGROUND_ANIMATOR_H_
