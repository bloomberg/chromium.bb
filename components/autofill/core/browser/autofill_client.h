// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CLIENT_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "ui/base/window_open_disposition.h"

class IdentityProvider;

namespace content {
class RenderFrameHost;
}

namespace gfx {
class Rect;
class RectF;
}

namespace rappor {
class RapporService;
}

class GURL;
class PrefService;

namespace autofill {

class AutofillPopupDelegate;
class AutofillWebDataService;
class CardUnmaskDelegate;
class CreditCard;
class FormStructure;
class PersonalDataManager;
struct FormData;
struct Suggestion;

// A client interface that needs to be supplied to the Autofill component by the
// embedder.
//
// Each client instance is associated with a given context within which an
// AutofillManager is used (e.g. a single tab), so when we say "for the client"
// below, we mean "in the execution context the client is associated with" (e.g.
// for the tab the AutofillManager is attached to).
class AutofillClient {
 public:
  // Copy of blink::WebFormElement::AutocompleteResult.
  enum RequestAutocompleteResult {
    AutocompleteResultSuccess,
    AutocompleteResultErrorDisabled,
    AutocompleteResultErrorCancel,
    AutocompleteResultErrorInvalid,
  };

  enum GetRealPanResult {
    // Request succeeded.
    SUCCESS,

    // Request failed; try again.
    TRY_AGAIN_FAILURE,

    // Request failed; don't try again.
    PERMANENT_FAILURE,

    // Unable to connect to Wallet servers. Prompt user to check internet
    // connection.
    NETWORK_ERROR,
  };

  typedef base::Callback<void(RequestAutocompleteResult,
                              const base::string16&,
                              const FormStructure*)> ResultCallback;

  typedef base::Callback<void(const base::string16& /* card number */,
                              int /* exp month */,
                              int /* exp year */)> CreditCardScanCallback;

  virtual ~AutofillClient() {}

  // Gets the PersonalDataManager instance associated with the client.
  virtual PersonalDataManager* GetPersonalDataManager() = 0;

  // Gets the AutofillWebDataService instance associated with the client.
  virtual scoped_refptr<AutofillWebDataService> GetDatabase() = 0;

  // Gets the preferences associated with the client.
  virtual PrefService* GetPrefs() = 0;

  // Gets the IdentityProvider associated with the client (for OAuth2).
  virtual IdentityProvider* GetIdentityProvider() = 0;

  // Gets the RapporService associated with the client (for metrics).
  virtual rappor::RapporService* GetRapporService() = 0;

  // Hides the associated request autocomplete dialog (if it exists).
  virtual void HideRequestAutocompleteDialog() = 0;

  // Causes the Autofill settings UI to be shown.
  virtual void ShowAutofillSettings() = 0;

  // A user has attempted to use a masked card. Prompt them for further
  // information to proceed.
  virtual void ShowUnmaskPrompt(const CreditCard& card,
                                base::WeakPtr<CardUnmaskDelegate> delegate) = 0;
  virtual void OnUnmaskVerificationResult(GetRealPanResult result) = 0;

  // Run |save_card_callback| if the credit card should be imported as personal
  // data. |metric_logger| can be used to log user actions.
  virtual void ConfirmSaveCreditCard(
      const base::Closure& save_card_callback) = 0;

  // Returns true if both the platform and the device support scanning credit
  // cards. Should be called before ScanCreditCard().
  virtual bool HasCreditCardScanFeature() = 0;

  // Shows the user interface for scanning a credit card. Invokes the |callback|
  // when a credit card is scanned successfully. Should be called only if
  // HasCreditCardScanFeature() returns true.
  virtual void ScanCreditCard(const CreditCardScanCallback& callback) = 0;

  // Causes the dialog for request autocomplete feature to be shown.
  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      content::RenderFrameHost* render_frame_host,
      const ResultCallback& callback) = 0;

  // Shows an Autofill popup with the given |values|, |labels|, |icons|, and
  // |identifiers| for the element at |element_bounds|. |delegate| will be
  // notified of popup events.
  virtual void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<Suggestion>& suggestions,
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
      content::RenderFrameHost* rfh,
      const std::vector<autofill::FormStructure*>& forms) = 0;

  // Inform the client that the field has been filled.
  virtual void DidFillOrPreviewField(
      const base::string16& autofilled_value,
      const base::string16& profile_full_name) = 0;

  // Informs the client that a user gesture has been observed.
  virtual void OnFirstUserGestureObserved() = 0;

  // Opens |url| with the supplied |disposition|.
  virtual void LinkClicked(const GURL& url,
                           WindowOpenDisposition disposition) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_CLIENT_H_
