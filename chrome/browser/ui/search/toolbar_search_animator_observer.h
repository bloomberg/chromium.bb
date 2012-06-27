// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_OBSERVER_H_
#define CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_OBSERVER_H_
#pragma once

class TabContents;

namespace chrome {
namespace search {

// This class defines the observer interface for |ToolbarSearchAnimator|.
class ToolbarSearchAnimatorObserver {
 public:
  // Called from ui::AnimationDelegate::AnimationProgressed.
  virtual void OnToolbarBackgroundAnimatorProgressed() = 0;

  // Called when animation is canceled and jumps to the end state.
  // If animation is canceled because the active tab is deactivated or detached
  // or closing, |tab_contents| contains the tab's contents.
  // Otherwise, if animation is canceled because of mode change, |tab_contents|
  // is NULL.
  virtual void OnToolbarBackgroundAnimatorCanceled(
      TabContents* tab_contents) = 0;

 protected:
  virtual ~ToolbarSearchAnimatorObserver() {}
};

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_TOOLBAR_SEARCH_ANIMATOR_OBSERVER_H_
