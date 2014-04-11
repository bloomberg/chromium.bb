// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_DELEGATE_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"

namespace gfx {
class Rect;
class RectF;
}

class GURL;
class InfoBarService;
class PrefService;

namespace autofill {

class AutofillMetrics;
class AutofillPopupDelegate;
class AutofillWebDataService;
class CreditCard;
class FormStructure;
class PasswordGenerator;
class PersonalDataManager;
struct FormData;
struct PasswordForm;

// A delegate interface that needs to be supplied to AutofillManager
// by the embedder.
//
// Each delegate instance is associated with a given context within
// which an AutofillManager is used (e.g. a single tab), so when we
// say "for the delegate" below, we mean "in the execution context the
// delegate is associated with" (e.g. for the tab the AutofillManager is
// attached to).
class AutofillManagerDelegate {
 public:
  // Copy of blink::WebFormElement::AutocompleteResult.
  enum RequestAutocompleteResult {
      AutocompleteResultSuccess,
      AutocompleteResultErrorDisabled,
      AutocompleteResultErrorCancel,
      AutocompleteResultErrorInvalid,
      // TODO(estade): add this one to WebFormElement::AutocompleteResult.
      AutocompleteResultErrorUnsupported,
  };

  typedef base::Callback<
      void(RequestAutocompleteResult, const FormStructure*)> ResultCallback;

  virtual ~AutofillManagerDelegate() {}

  // Gets the PersonalDataManager instance associated with the delegate.
  virtual PersonalDataManager* GetPersonalDataManager() = 0;

  // Gets the AutofillWebDataService instance associated with the delegate.
  virtual scoped_refptr<AutofillWebDataService> GetDatabase() = 0;

  // Gets the preferences associated with the delegate.
  virtual PrefService* GetPrefs() = 0;

  // Hides the associated request autocomplete dialog (if it exists).
  virtual void HideRequestAutocompleteDialog() = 0;

  // Causes the Autofill settings UI to be shown.
  virtual void ShowAutofillSettings() = 0;

  // Run |save_card_callback| if the credit card should be imported as personal
  // data. |metric_logger| can be used to log user actions.
  virtual void ConfirmSaveCreditCard(
      const AutofillMetrics& metric_logger,
      const base::Closure& save_card_callback) = 0;

  // Causes the dialog for request autocomplete feature to be shown.
  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const ResultCallback& callback) = 0;

  // Shows an Autofill popup with the given |values|, |labels|, |icons|, and
  // |identifiers| for the element at |element_bounds|. |delegate| will be
  // notified of popup events.
  virtual void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels,
      const std::vector<base::string16>& icons,
      const std::vector<int>& identifiers,
      base::WeakPtr<AutofillPopupDelegate> delegate) = 0;

  // Update the data list values shown by the Autofill popup, if visible.
  virtual void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) = 0;

  // Hide the Autofill popup if one is currently showing.
  virtual void HideAutofillPopup() = 0;

  // Whether the Autocomplete feature of Autofill should be enabled.
  virtual bool IsAutocompleteEnabled() = 0;

  // Pass the form structures to the password generation manager to detect
  // account creation forms.
  virtual void DetectAccountCreationForms(
      const std::vector<autofill::FormStructure*>& forms) = 0;

  // Inform the delegate that the field has been filled.
  virtual void DidFillOrPreviewField(
      const base::string16& autofilled_value,
      const base::string16& profile_full_name) = 0;

};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_DELEGATE_H_
