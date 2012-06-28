// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_DELEGATE_H_
#pragma once

#include "base/string16.h"
#include "chrome/common/instant_types.h"

class TabContents;

namespace gfx {
class Rect;
}

// |InstantController| calls these methods on its delegate (usually |Browser|)
// to ask for the Instant preview to be shown, hidden, etc. In the following
// methods, if a |TabContents| argument is explicitly supplied, the delegate
// MUST use it. Otherwise, the preview TabContents can be gotten by calling
// |InstantController::GetPreviewContents| (note that it may return NULL).
class InstantControllerDelegate {
 public:
  // Show the preview.
  virtual void ShowInstant(TabContents* preview_contents) = 0;

  // Hide any preview currently being shown.
  virtual void HideInstant() = 0;

  // Commit the preview by merging it into the active tab. Delegate takes
  // ownership of |preview_contents|.
  virtual void CommitInstant(TabContents* preview_contents) = 0;

  // Autocomplete the Instant suggested |text| into the omnibox, using the
  // specified |behavior| (see instant_types.h for details).
  virtual void SetSuggestedText(const string16& text,
                                InstantCompleteBehavior behavior) = 0;

  // Return the bounds that the preview is placed at, in screen coordinates.
  virtual gfx::Rect GetInstantBounds() = 0;

  // Notification that the preview gained focus, usually due to the user
  // clicking on it.
  virtual void InstantPreviewFocused() = 0;

  // Return the currently active tab, over which any preview would be shown.
  virtual TabContents* GetInstantHostTabContents() const = 0;

 protected:
  virtual ~InstantControllerDelegate() {}
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_CONTROLLER_DELEGATE_H_
