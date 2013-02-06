// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_DELEGATE_H_

#include "base/string16.h"

// An interface for interaction with AutofillPopupController. Will be notified
// of events by the controller.
class AutofillPopupDelegate {
 public:
  // Called when the autofill suggestion indicated by |identifier| has been
  // temporarily selected (e.g., hovered).
  virtual void DidSelectSuggestion(int identifier) = 0;

  // Inform the delegate that a row in the popup has been chosen.
  virtual void DidAcceptSuggestion(const string16& value, int identifier) = 0;

  // Delete the described suggestion.
  virtual void RemoveSuggestion(const string16& value, int identifier) = 0;

  // Informs the delegate that the Autofill previewed form should be cleared.
  virtual void ClearPreviewedForm() = 0;

  // Called to inform the delegate the controller is experiencing destruction.
  virtual void ControllerDestroyed() = 0;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_DELEGATE_H_
