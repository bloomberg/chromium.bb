// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SUGGESTED_TEXT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SUGGESTED_TEXT_VIEW_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/controls/label.h"

class AutocompleteEditModel;

// SuggestedTextView is used to show the suggest text in the LocationBar.
// Invoke |StartAnimation| to start an animation that when done invokes
// |CommitSuggestedText| on the AutocompleteEdit to commit the suggested text.
class SuggestedTextView : public views::Label,
                          public ui::AnimationDelegate {
 public:
  explicit SuggestedTextView(AutocompleteEditModel* edit_model);
  virtual ~SuggestedTextView();

  // Starts the animation. If the animation is currently running it is stopped
  // and restarted. The animation transitions the suggested text to look like
  // selected text. When the animation completes |OnCommitSuggestedText| is
  // invoked on the LocationBar.
  void StartAnimation();

  // Stops the animation.
  void StopAnimation();

  // View overrides:
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

  // AnimationDelegate overrides:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

 private:
  // Creates the animation to use.
  ui::Animation* CreateAnimation();

  // Resets the background color.
  void UpdateBackgroundColor();

  AutocompleteEditModel* edit_model_;

  scoped_ptr<ui::Animation> animation_;

  SkColor bg_color_;

  DISALLOW_COPY_AND_ASSIGN(SuggestedTextView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SUGGESTED_TEXT_VIEW_H_
