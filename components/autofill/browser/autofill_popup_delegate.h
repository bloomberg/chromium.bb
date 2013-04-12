// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_POPUP_DELEGATE_H_
#define COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_POPUP_DELEGATE_H_

#include "base/string16.h"

namespace content {
class KeyboardListener;
}

namespace autofill {

// An interface for interaction with AutofillPopupController. Will be notified
// of events by the controller.
class AutofillPopupDelegate {
 public:
  // Called when the Autofill popup is shown. |listener| may be used to pass
  // keyboard events to the popup.
  virtual void OnPopupShown(content::KeyboardListener* listener) = 0;

  // Called when the Autofill popup is hidden. |listener| must be unregistered
  // if it was registered in OnPopupShown.
  virtual void OnPopupHidden(content::KeyboardListener* listener) = 0;

  // Called when the autofill suggestion indicated by |identifier| has been
  // temporarily selected (e.g., hovered).
  virtual void DidSelectSuggestion(int identifier) = 0;

  // Inform the delegate that a row in the popup has been chosen.
  virtual void DidAcceptSuggestion(const base::string16& value,
                                   int identifier) = 0;

  // Delete the described suggestion.
  virtual void RemoveSuggestion(const base::string16& value,
                                int identifier) = 0;

  // Informs the delegate that the Autofill previewed form should be cleared.
  virtual void ClearPreviewedForm() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_POPUP_DELEGATE_H_
