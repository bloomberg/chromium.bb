// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_H_
#define CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_H_

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "chrome/browser/ui/search/search_types.h"
#include "ui/base/animation/animation_delegate.h"

class TabContents;

namespace ui {
class MultiAnimation;
}

namespace chrome {
namespace search {

class SearchModel;
class ToolbarSearchAnimatorObserver;

// ToolbarSearchAnimator is used to track the gradient background state of the
// toolbar and related classes.  To use ToolbarSearchAnimator, add a
// ToolbarSearchAnimatorObserver.  The ToolbarSearchAnimatorObserver is then
// notified appropriately.
class ToolbarSearchAnimator : public SearchModelObserver,
                              public ui::AnimationDelegate {
 public:
  explicit ToolbarSearchAnimator(SearchModel* search_model);
  virtual ~ToolbarSearchAnimator();

  // Get the gradient background opacity to paint for toolbar and active tab, a
  // value between 0f and 1f inclusive:
  // - 0f: only paint flat background
  // - < 1f: paint flat background at full opacity and gradient background at
  //   specified opacity
  // - 1f: only paint gradient background at full opacity
  double GetGradientOpacity() const;

  // Called from SearchDelegate::StopObservingTab() when a tab is deactivated or
  // closing or detached, to jump to the end state of the animation.
  // This allows a reactivated tab to show the end state of the animation,
  // rather than the transient state.
  void FinishAnimation(TabContents* tab_contents);

  // Add and remove observers.
  void AddObserver(ToolbarSearchAnimatorObserver* observer);
  void RemoveObserver(ToolbarSearchAnimatorObserver* observer);

  // Overridden from SearchModelObserver:
  virtual void ModeChanged(const Mode& old_mode, const Mode& new_mode) OVERRIDE;

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

 private:
  friend class ToolbarSearchAnimatorTest;
  friend class ToolbarSearchAnimatorTestObserver;

  // Helper function to initialize background animation with its parts.
  void InitBackgroundAnimation();

  // Helper function to start animation for gradient background change.
  void StartBackgroundChange();

  // Reset animations by stopping them.
  // If we're animating background or separator, we'll notify observers via
  // ToolbarSearchAnimatorObserver::OnToolbarBackgroundAnimatorCanceled or
  // ToolbarSearchAnimatorObserver::OnToolbarSeparatorAnimatorCanceled
  // respectively.
  // Pass in |tab_contents| if animation is canceled because of deactivating or
  // detaching or closing a tab.
  void Reset(TabContents* tab_contents);

  // Weak.  Owned by Browser.  Non-NULL.
  SearchModel* search_model_;

  // The background change animation.
  scoped_ptr<ui::MultiAnimation> background_animation_;

  // Time (in ms) of background animation delay and duration.
  int background_change_delay_ms_;
  int background_change_duration_ms_;

  // Observers.
  ObserverList<ToolbarSearchAnimatorObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarSearchAnimator);
};

}  // namespace chrome
}  // namespace search

#endif  // CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_H_
