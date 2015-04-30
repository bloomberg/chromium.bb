// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_DELEGATE_H_

#include "base/strings/string16.h"

// A delegate for a generic bubble that hangs off the toolbar actions bar.
class ToolbarActionsBarBubbleDelegate {
 public:
  enum CloseAction {
    CLOSE_LEARN_MORE,
    CLOSE_EXECUTE,
    CLOSE_DISMISS
  };

  virtual ~ToolbarActionsBarBubbleDelegate() {}

  // Gets the text for the bubble's heading (title).
  virtual base::string16 GetHeadingText() = 0;

  // Gets the text for the bubble's body.
  virtual base::string16 GetBodyText() = 0;

  // Gets the text for an optional item list to display. If this returns an
  // empty string, no list will be added.
  virtual base::string16 GetItemListText() = 0;

  // Gets the text for the main button on the bubble; this button will
  // correspond with ACTION_EXECUTE.
  virtual base::string16 GetActionButtonText() = 0;

  // Gets the text for a second button on the bubble; this button will
  // correspond with ACTION_DISMISS. If this returns an empty string, no
  // button will be added.
  virtual base::string16 GetDismissButtonText() = 0;

  // Gets the text for a "learn more" link-style button on the bubble; this
  // button will correspond with ACTION_LEARN_MORE. If this returns an empty
  // string, no button will be added.
  virtual base::string16 GetLearnMoreButtonText() = 0;

  // Called when the bubble is shown.
  virtual void OnBubbleShown() = 0;

  // Called when the bubble is closed with the type of action the user took.
  virtual void OnBubbleClosed(CloseAction action) = 0;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_DELEGATE_H_
