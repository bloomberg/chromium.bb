// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>
#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model_observer.h"

@class FullscreenAnimator;
class FullscreenController;
class FullscreenControllerObserver;
@class FullscreenResetAnimator;
@class FullscreenScrollEndAnimator;
@class FullscreenScrollToTopAnimator;
@class ToolbarRevealAnimator;

// A helper object that listens to FullscreenModel changes and forwards this
// information to FullscreenControllerObservers.
class FullscreenMediator : public FullscreenModelObserver {
 public:
  FullscreenMediator(FullscreenController* controller, FullscreenModel* model);
  ~FullscreenMediator() override;

  // Adds and removes FullscreenControllerObservers.
  void AddObserver(FullscreenControllerObserver* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(FullscreenControllerObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // Instructs the mediator that a scroll-to-top animation has been triggered.
  void ScrollToTop();

  // Instructs the mediator that the app will be foregrounded.
  void WillEnterForeground();

  // Resets the model while animating changes.
  void AnimateModelReset();

  // Instructs the mediator to stop observing its model.
  void Disconnect();

 private:
  // FullscreenModelObserver:
  void FullscreenModelProgressUpdated(FullscreenModel* model) override;
  void FullscreenModelEnabledStateChanged(FullscreenModel* model) override;
  void FullscreenModelScrollEventStarted(FullscreenModel* model) override;
  void FullscreenModelScrollEventEnded(FullscreenModel* model) override;
  void FullscreenModelWasReset(FullscreenModel* model) override;

  // Sets up |animator|.  |animator| is expected to be a pointer to a data
  // member of this object, and is used to reset the variable in the animation
  // completion block.
  void SetUpAnimator(__strong FullscreenAnimator** animator);

  // Starts |animator| if it has animations to run.  |animator| is expected to
  // be a pointer to a data member of this object, and will be reset when
  // attempting to start an animator without any animation blocks.
  void StartAnimator(__strong FullscreenAnimator** animator);

  // Stops the current scroll end animation if one is in progress.  If
  // |update_model| is true, the FullscreenModel will be updated with the active
  // animator's current progress value.
  void StopAnimating(bool update_model);

  // Stops |animator|.  |animator| is expected to be a pointer to a data member
  // of this object, and is used to reset the variable.  If |update_model| is
  // true, the FullscreenModel will be updated with the current progress of the
  // animator before deallocation.
  void StopAnimator(__strong FullscreenAnimator** animator, bool update_model);

  // Checks whether |animator| is a valid pointer to one of the three animator
  // data members of this class.  No-op for non-debug builds.
  void VerifyAnimatorPointer(__strong FullscreenAnimator** animator) const;

  // The controller.
  FullscreenController* controller_ = nullptr;
  // The model.
  FullscreenModel* model_ = nullptr;
  // The scroll end animator passed to observers.
  __strong FullscreenScrollEndAnimator* scroll_end_animator_ = nil;
  // The scroll to top animator passed to observers.
  __strong FullscreenScrollToTopAnimator* scroll_to_top_animator_ = nil;
  // The toolbar reveal animator.
  __strong ToolbarRevealAnimator* toolbar_reveal_animator_ = nil;
  // The FullscreenControllerObservers that need to get notified of model
  // changes.
  base::ObserverList<FullscreenControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenMediator);
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MEDIATOR_H_
