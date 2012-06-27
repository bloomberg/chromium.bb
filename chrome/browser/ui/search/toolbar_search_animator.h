// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_H_
#define CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/timer.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "ui/base/animation/animation_delegate.h"

class TabContents;

namespace ui {
class SlideAnimation;
}

namespace chrome {
namespace search {

class SearchModel;
class ToolbarSearchAnimatorObserver;

// ToolbarSearchAnimator is used to track the background state of the toolbar
// and related classes.  To use ToolbarSearchAnimator, add a
// ToolbarSearchAnimatorObserver.  The ToolbarSearchAnimatorObserver is then
// notified appropriately.
class ToolbarSearchAnimator : public SearchModelObserver,
                              public ui::AnimationDelegate {
 public:
  // State of background to paint by observers, only applicable for
  // |MODE_SEARCH|.
  enum BackgroundState {
    // Background state is not applicable.
    BACKGROUND_STATE_DEFAULT = 0,
    // Show background for |MODE_NTP|.
    BACKGROUND_STATE_NTP = 0x01,
    // Show background for |MODE_SEARCH|.
    BACKGROUND_STATE_SEARCH = 0x02,
    // Show backgrounds for both |MODE_NTP| and |MODE_SEARCH|.
    BACKGROUND_STATE_NTP_SEARCH = BACKGROUND_STATE_NTP |
                                  BACKGROUND_STATE_SEARCH,
  };

  explicit ToolbarSearchAnimator(SearchModel* search_model);
  virtual ~ToolbarSearchAnimator();

  // Get the current background state to paint.
  // |search_background_opacity| contains a valid opacity value only if
  // background for |MODE_SEARCH| needs to be shown i.e. |background_state| is
  // BACKGROUND_STATE_SEARCH or BACKGROUND_STATE_NTP_SEARCH.
  // Only call this for |MODE_SEARCH|.
  void GetCurrentBackgroundState(BackgroundState* background_state,
                                 double* search_background_opacity) const;

  // Called from SearchDelegate::StopObservingTab() when a tab is deactivated or
  // closing or detached, to jump to the end state of the animation.
  // This allows a reactivated tab to show the end state of the animation,
  // rather than the transient state.
  void FinishAnimation(TabContents* tab_contents);

  // Add and remove observers.
  void AddObserver(ToolbarSearchAnimatorObserver* observer);
  void RemoveObserver(ToolbarSearchAnimatorObserver* observer);

  // Overridden from SearchModelObserver:
  virtual void ModeChanged(const Mode& mode) OVERRIDE;

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

 private:
  // State of animation.
  enum AnimateState {
    ANIMATE_STATE_NONE,     // Doing nothing.
    ANIMATE_STATE_WAITING,  // Waiting to run background animation.
    ANIMATE_STATE_RUNNING,  // Running background animation.
  };

  // Callback for |background_change_timer_| to actually start the background
  // change animation.
  void StartBackgroundChange();

  // Reset state of animator: reset animate_state_, stop timer or animation,
  // If we're waiting to animate or animating, i.e. |animate_state| is not
  // ANIMATE_STAET_NONE, wwe'll notify observers via
  // ToolbarSearchAnimatorObserver::BackgroundChangeCanceled.
  // Pass in |tab_contents| if animation is canceled because of deactivating or
  // detaching or closing a tab.
  void Reset(TabContents* tab_contents);

  // Weak.  Owned by Browser.  Non-NULL.
  SearchModel* search_model_;

  // State of animation.
  AnimateState animate_state_;

  // The background fade animation.
  scoped_ptr<ui::SlideAnimation> background_animation_;

  // The timer to delay start of animation after mode changes from |MODE_NTP| to
  // |MODE_SEARCH|.
  base::OneShotTimer<ToolbarSearchAnimator> background_change_timer_;

  // Observers.
  ObserverList<ToolbarSearchAnimatorObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarSearchAnimator);
};

}  // namespace chrome
}  // namespace search

#endif  // CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_H_
