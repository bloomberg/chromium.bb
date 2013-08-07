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
#include "components/autofill/content/browser/autocheckout_steps.h"
#include "components/autofill/core/browser/autocheckout_bubble_state.h"

namespace content {
struct PasswordForm;
}

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
class CreditCard;
class FormStructure;
class PasswordGenerator;
class PersonalDataManager;
struct FormData;

namespace autocheckout {
class WhitelistManager;
}

enum DialogType {
  // Autofill dialog for the Autocheckout feature.
  DIALOG_TYPE_AUTOCHECKOUT,
  // Autofill dialog for the requestAutocomplete feature.
  DIALOG_TYPE_REQUEST_AUTOCOMPLETE,
};

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
  virtual ~AutofillManagerDelegate() {}

  // Gets the PersonalDataManager instance associated with the delegate.
  virtual PersonalDataManager* GetPersonalDataManager() = 0;

  // Gets the preferences associated with the delegate.
  virtual PrefService* GetPrefs() = 0;

  // Gets the autocheckout::WhitelistManager instance associated with the
  // delegate.
  virtual autocheckout::WhitelistManager*
      GetAutocheckoutWhitelistManager() const = 0;

  // Hides the associated request autocomplete dialog (if it exists).
  virtual void HideRequestAutocompleteDialog() = 0;

  // Causes an error explaining that Autocheckout has failed to be displayed to
  // the user.
  virtual void OnAutocheckoutError() = 0;

  // Called when an Autocheckout flow has succeeded. Causes a notification
  // explaining that they must confirm their purchase to be displayed to the
  // user.
  virtual void OnAutocheckoutSuccess() = 0;

  // Causes the Autofill settings UI to be shown.
  virtual void ShowAutofillSettings() = 0;

  // Run |save_card_callback| if the credit card should be imported as personal
  // data. |metric_logger| can be used to log user actions.
  virtual void ConfirmSaveCreditCard(
      const AutofillMetrics& metric_logger,
      const CreditCard& credit_card,
      const base::Closure& save_card_callback) = 0;

  // Causes the Autocheckout bubble UI to be displayed. |bounding_box| is the
  // anchor for the bubble. |is_google_user| is whether or not the user is
  // logged into or has been logged into accounts.google.com. |callback| is run
  // if the bubble is accepted. The returned boolean informs the caller whether
  // or not the bubble is successfully shown.
  virtual bool ShowAutocheckoutBubble(
      const gfx::RectF& bounding_box,
      bool is_google_user,
      const base::Callback<void(AutocheckoutBubbleState)>& callback) = 0;

  // Causes the dialog for request autocomplete feature to be shown.
  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      DialogType dialog_type,
      const base::Callback<void(const FormStructure*,
                                const std::string&)>& callback) = 0;

  // Hide the Autocheckout bubble if one is currently showing.
  virtual void HideAutocheckoutBubble() = 0;

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

  // Update progress of the Autocheckout flow as displayed to the user.
  virtual void AddAutocheckoutStep(AutocheckoutStepType step_type) = 0;
  virtual void UpdateAutocheckoutStep(
      AutocheckoutStepType step_type,
      AutocheckoutStepStatus step_status) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_DELEGATE_H_
