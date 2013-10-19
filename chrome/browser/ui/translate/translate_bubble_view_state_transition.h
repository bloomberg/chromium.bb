// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_VIEW_STATE_TRANSITION_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_VIEW_STATE_TRANSITION_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"

// The class which manages the transition of the view state of the Translate
// bubble.
class TranslateBubbleViewStateTransition {
 public:
  explicit TranslateBubbleViewStateTransition(
      TranslateBubbleModel::ViewState view_state);

  TranslateBubbleModel::ViewState view_state() const { return view_state_; }

  // Transitions the view state.
  void SetViewState(TranslateBubbleModel::ViewState view_state);

  // Goes back from the 'Advanced' view state.
  void GoBackFromAdvanced();

 private:
  // The current view type.
  TranslateBubbleModel::ViewState view_state_;

  // The view type. When the current view type is not 'Advanced' view, this is
  // equivalent to |view_state_|. Otherwise, this is the previous view type
  // before the user opens the 'Advanced' view. This is used to navigate when
  // pressing 'Cancel' button on the 'Advanced' view.
  TranslateBubbleModel::ViewState view_state_before_advanced_view_;

  DISALLOW_COPY_AND_ASSIGN(TranslateBubbleViewStateTransition);
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_VIEW_STATE_TRANSITION_H_
