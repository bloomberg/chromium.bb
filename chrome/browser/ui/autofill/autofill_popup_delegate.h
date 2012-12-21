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
  // Called when the autofill suggestion indicated by |unique_id| has been
  // temporarily selected (e.g., hovered).
  virtual void SelectAutofillSuggestion(int unique_id) = 0;

  // Inform the delegate that a row in the popup has been chosen.
  // TODO(estade): does this really need to return a value?
  virtual bool DidAcceptAutofillSuggestion(const string16& value,
                                           int unique_id,
                                           unsigned index) = 0;

  // Remove the given Autocomplete entry from the DB.
  virtual void RemoveAutocompleteEntry(const string16& value) = 0;

  // Remove the given Autofill profile or credit credit.
  virtual void RemoveAutofillProfileOrCreditCard(int unique_id) = 0;

  // Informs the delegate that the Autofill previewed form should be cleared.
  virtual void ClearPreviewedForm() = 0;

  // Called to inform the delegate the controller is experiencing destruction.
  virtual void ControllerDestroyed() = 0;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_DELEGATE_H_
