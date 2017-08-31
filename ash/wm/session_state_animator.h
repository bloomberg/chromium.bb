// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SESSION_STATE_ANIMATOR_H_
#define ASH_WM_SESSION_STATE_ANIMATOR_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace ash {

// Displays onscreen animations for session state changes (lock/unlock, sign
// out, shut down).
class ASH_EXPORT SessionStateAnimator {
 public:
  // Animations that can be applied to groups of containers.
  enum AnimationType {
    ANIMATION_PARTIAL_CLOSE = 0,
    ANIMATION_UNDO_PARTIAL_CLOSE,
    ANIMATION_FULL_CLOSE,
    ANIMATION_FADE_IN,
    ANIMATION_FADE_OUT,
    ANIMATION_HIDE_IMMEDIATELY,
    ANIMATION_RESTORE,
    // Animations that raise/lower windows to/from area "in front" of the
    // screen.
    ANIMATION_LIFT,
    ANIMATION_UNDO_LIFT,
    ANIMATION_DROP,
    // Animations that raise/lower windows from/to area "behind" of the screen.
    ANIMATION_RAISE_TO_SCREEN,
    ANIMATION_LOWER_BELOW_SCREEN,
    ANIMATION_PARTIAL_FADE_IN,
    ANIMATION_UNDO_PARTIAL_FADE_IN,
    ANIMATION_FULL_FADE_IN,
    ANIMATION_GRAYSCALE_BRIGHTNESS,
    ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS,
  };

  // Constants for determining animation speed.
  enum AnimationSpeed {
    // Immediately change state.
    ANIMATION_SPEED_IMMEDIATE = 0,
    // Speed for animations associated with user action that can be undone.
    // Used for pre-lock and pre-shutdown animations.
    ANIMATION_SPEED_UNDOABLE,
    // Speed for animation that reverts undoable action. Used for aborting
    // pre-lock and pre-shutdown animations.
    ANIMATION_SPEED_REVERT,
    // Speed for user action that can not be undone, Used for lock and shutdown
    // animations requested via menus/shortcuts and for animating remaining
    // parts of partial lock/shutdown animations.
    ANIMATION_SPEED_FAST,
    // Speed for lock screen appearance in "old" animation set.
    ANIMATION_SPEED_SHOW_LOCK_SCREEN,
    // Speed for workspace-like animations in "new" animation set.
    ANIMATION_SPEED_MOVE_WINDOWS,
    // Speed for undoing workspace-like animations in "new" animation set.
    ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
    // Speed for shutdown in "new" animation set.
    ANIMATION_SPEED_SHUTDOWN,
    // Speed for reverting shutdown in "new" animation set.
    ANIMATION_SPEED_REVERT_SHUTDOWN,
  };

  // Specific containers or groups of containers that can be animated.
  enum Container {
    WALLPAPER = 1 << 0,
    SHELF = 1 << 1,

    // All user session related containers including the system wallpaper but
    // not including the user's wallpaper.
    NON_LOCK_SCREEN_CONTAINERS = 1 << 2,

    // Desktop wallpaper is moved to this layer when screen is locked.
    // This layer is excluded from lock animation so that wallpaper stays as is,
    // user session windows are hidden and lock UI is shown on top of it.
    // This layer is included in shutdown animation.
    LOCK_SCREEN_WALLPAPER = 1 << 3,

    // Lock screen and lock screen modal containers.
    LOCK_SCREEN_CONTAINERS = 1 << 4,

    // Multiple system layers belong here like status, menu, tooltip
    // and overlay layers.
    LOCK_SCREEN_RELATED_CONTAINERS = 1 << 5,

    // The primary root window.
    ROOT_CONTAINER = 1 << 6,
  };

  // A bitfield mask including LOCK_SCREEN_WALLPAPER,
  // LOCK_SCREEN_CONTAINERS, and LOCK_SCREEN_RELATED_CONTAINERS.
  static const int kAllLockScreenContainersMask;

  // A bitfield mask of all containers except the ROOT_CONTAINER.
  static const int kAllNonRootContainersMask;

  // The AnimationSequence groups together multiple animations and invokes a
  // callback once all contained animations are completed successfully.
  // Subclasses of AnimationSequence should call one of OnAnimationCompleted or
  // OnAnimationAborted once and behaviour is undefined if called multiple
  // times.
  // AnimationSequences will destroy themselves once EndSquence and one of
  // OnAnimationCompleted or OnAnimationAborted has been called.
  //
  // Typical usage:
  //  AnimationSequence* animation_sequence =
  //      session_state_animator->BeginAnimationSequence(some_callback);
  //  animation_sequence->StartAnimation(
  //      SessionStateAnimator::LAUNCHER,
  //      SessionStateAnimator::ANIMATION_FADE_IN,
  //      SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  //  animation_sequence->StartAnimation(
  //      SessionStateAnimator::LAUNCHER,
  //      SessionStateAnimator::ANIMATION_FADE_IN,
  //      SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  //  animation_sequence->EndSequence();
  //  // some_callback won't be called until here even if the animations
  //  // were completed before the EndSequence call.
  //
  class ASH_EXPORT AnimationSequence {
   public:
    virtual ~AnimationSequence();

    // Apply animation |type| to all containers included in |container_mask|
    // with specified |speed|.
    virtual void StartAnimation(int container_mask,
                                AnimationType type,
                                AnimationSpeed speed) = 0;

    // Ends the animation sequence and enables the callback to be invoked
    // when the animation sequence has completed.  No more animations should be
    // started after EndSequence is called because the AnimationSequenceObserver
    // may have destroyed itself.
    // NOTE: Clients of AnimationSequence should not access it after EndSequence
    // has been called.
    virtual void EndSequence();

   protected:
    // AnimationSequence should not be instantiated directly, only through
    // subclasses.
    explicit AnimationSequence(base::OnceClosure callback);

    // Subclasses should call this when the contained animations completed
    // successfully.
    // NOTE: This should NOT be accessed after OnAnimationCompleted has been
    // called.
    virtual void OnAnimationCompleted();

    // Subclasses should call this when the contained animations did NOT
    // complete successfully.
    // NOTE: This should NOT be accessed after OnAnimationAborted has been
    // called.
    virtual void OnAnimationAborted();

   private:
    // Destroys this and calls the callback if the contained animations
    // completed successfully.
    void CleanupIfSequenceCompleted();

    // Tracks whether the sequence has ended.
    bool sequence_ended_;

    // Track whether the contained animations have completed or not, both
    // successfully and unsuccessfully.
    bool animation_completed_;

    // Flag to specify whether the callback should be invoked once the sequence
    // has completed.
    bool invoke_callback_;

    // Callback to be called.
    base::OnceClosure callback_;

    DISALLOW_COPY_AND_ASSIGN(AnimationSequence);
  };

  SessionStateAnimator();
  virtual ~SessionStateAnimator();

  // Reports animation duration for |speed|.
  virtual base::TimeDelta GetDuration(AnimationSpeed speed);

  // Apply animation |type| to all containers included in |container_mask| with
  // specified |speed|.
  virtual void StartAnimation(int container_mask,
                              AnimationType type,
                              AnimationSpeed speed) = 0;

  // Apply animation |type| to all containers included in |container_mask| with
  // specified |speed| and call a |callback| once at the end of the animations,
  // if it is not null.
  virtual void StartAnimationWithCallback(int container_mask,
                                          AnimationType type,
                                          AnimationSpeed speed,
                                          base::OnceClosure callback) = 0;

  // Begins an animation sequence.  Use this when you need to be notified when
  // a group of animations are completed.  See AnimationSequence documentation
  // for more details.
  virtual AnimationSequence* BeginAnimationSequence(
      base::OnceClosure callback) = 0;

  // Retruns true if the wallpaper is hidden.
  virtual bool IsWallpaperHidden() const = 0;

  // Shows the wallpaper immediately.
  virtual void ShowWallpaper() = 0;

  // Hides the wallpaper immediately.
  virtual void HideWallpaper() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateAnimator);
};

}  // namespace ash

#endif  // ASH_WM_SESSION_STATE_ANIMATOR_H_
