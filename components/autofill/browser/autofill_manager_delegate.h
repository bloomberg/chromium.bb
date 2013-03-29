// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_MANAGER_DELEGATE_H_
#define COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_MANAGER_DELEGATE_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"

namespace autofill {
class PasswordGenerator;
}

namespace content {
struct PasswordForm;
struct SSLStatus;
}

namespace gfx {
class Rect;
class RectF;
}

class AutofillMetrics;
class AutofillPopupDelegate;
class CreditCard;
class FormStructure;
class GURL;
class InfoBarService;
class PersonalDataManager;
class PrefService;
class ProfileSyncServiceBase;

struct FormData;

namespace autofill {

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

  // Returns true if saving passwords is currently enabled for the
  // delegate.
  virtual bool IsSavingPasswordsEnabled() const = 0;

  // Returns true if Sync is enabled for the passwords datatype.
  virtual bool IsPasswordSyncEnabled() const = 0;

  // Sets a callback that will be called when sync state changes.
  //
  // Set the callback to an object for which |is_null()| evaluates to
  // true to stop receiving notifications
  // (e.g. SetSyncStateChangedCallback(base::Closure())).
  virtual void SetSyncStateChangedCallback(const base::Closure& callback) = 0;

  // Causes an error explaining that Autocheckout has failed to be displayed to
  // the user.
  virtual void OnAutocheckoutError() = 0;

  // Causes the Autofill settings UI to be shown.
  virtual void ShowAutofillSettings() = 0;

  // Run |save_card_callback| if the credit card should be imported as personal
  // data. |metric_logger| can be used to log user actions.
  virtual void ConfirmSaveCreditCard(
      const AutofillMetrics& metric_logger,
      const CreditCard& credit_card,
      const base::Closure& save_card_callback) = 0;

  // Causes the password generation bubble UI to be shown using the
  // specified form with the given bounds.
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& bounds,
      const content::PasswordForm& form,
      autofill::PasswordGenerator* generator) = 0;

  // Causes the Autocheckout bubble UI to be displayed. |bounding_box| is the
  // anchor for the bubble. |native_view| is the parent view of the bubble.
  // |callback| is run if the bubble is accepted.
  virtual void ShowAutocheckoutBubble(
      const gfx::RectF& bounding_box,
      const gfx::NativeView& native_view,
      const base::Callback<void(bool)>& callback) = 0;

  // Causes the dialog for request autocomplete feature to be shown.
  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const AutofillMetrics& metric_logger,
      DialogType dialog_type,
      const base::Callback<void(const FormStructure*,
                                const std::string&)>& callback) = 0;

  // Hide the Autocheckout bubble if one is currently showing.
  virtual void HideAutocheckoutBubble() = 0;

  // Called when the dialog for request autocomplete closes. (So UI code will
  // free memory, etc.)
  virtual void RequestAutocompleteDialogClosed() = 0;

  // Shows an Autofill popup with the given |values|, |labels|, |icons|, and
  // |identifiers| for the element at |element_bounds|. |delegate| will be
  // notified of popup events.
  virtual void ShowAutofillPopup(const gfx::RectF& element_bounds,
                                 const std::vector<string16>& values,
                                 const std::vector<string16>& labels,
                                 const std::vector<string16>& icons,
                                 const std::vector<int>& identifiers,
                                 AutofillPopupDelegate* delegate) = 0;

  // Hide the Autofill popup if one is currently showing.
  virtual void HideAutofillPopup() = 0;

  // Updates the Autocheckout progress bar. |value| must be in [0.0, 1.0].
  virtual void UpdateProgressBar(double value) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_AUTOFILL_MANAGER_DELEGATE_H_
