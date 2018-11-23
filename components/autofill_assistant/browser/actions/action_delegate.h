// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/selector.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

class GURL;

namespace autofill {
class AutofillProfile;
class CreditCard;
class PersonalDataManager;
}  // namespace autofill

namespace content {
class WebContents;
}  // namespace content

namespace autofill_assistant {
class ClientMemory;
class DetailsProto;
struct PaymentInformation;

// Action delegate called when processing actions.
class ActionDelegate {
 public:
  virtual ~ActionDelegate() = default;

  // Show status message on the bottom bar.
  virtual void ShowStatusMessage(const std::string& message) = 0;

  // Create a helper for checking for multiple element existence or field
  // values.
  virtual std::unique_ptr<BatchElementChecker> CreateBatchElementChecker() = 0;

  // Wait for a short time for a given selector to appear.
  //
  // Most actions should call this before issuing a command on an element, to
  // account for timing issues with needed elements not showing up right away.
  //
  // Longer-time waiting should still be controlled explicitly, using
  // WaitForDom.
  //
  // TODO(crbug.com/806868): Consider embedding that wait right into
  // WebController and eliminate double-lookup.
  virtual void ShortWaitForElementExist(
      const Selector& selector,
      base::OnceCallback<void(bool)> callback) = 0;

  // Wait for up to |max_wait_time| for the element |selectors| to be visible on
  // the page, then call |callback| with true if the element was visible, false
  // otherwise.
  //
  // If |allow_interrupt| interrupts can run while waiting.
  virtual void WaitForElementVisible(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      const Selector& selector,
      base::OnceCallback<void(bool)> callback) = 0;

  // Click or tap the element given by |selector| on the web page.
  virtual void ClickOrTapElement(const Selector& selector,
                                 base::OnceCallback<void(bool)> callback) = 0;

  // Ask user to select one of the given suggestions.
  //
  // While Choose is in progress, the UI looks the same as it does between
  // scripts, even though we're in the middle of a script. This includes
  // allowing access to the touchable elements set previously, in the same
  // script.
  virtual void Choose(
      const std::vector<std::string>& suggestions,
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Cancels a choose action in progress and pass the given result to the
  // callback.
  virtual void ForceChoose(const std::string& result) = 0;

  // Ask user to choose an address in personal data manager. GUID of the chosen
  // address will be returned through callback, otherwise empty string if the
  // user chose to continue manually.
  virtual void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Asks the user to provide the data used by UseAddressAction and
  // UseCreditCardAction.
  virtual void GetPaymentInformation(
      payments::mojom::PaymentOptionsPtr payment_options,
      base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
      const std::string& title,
      const std::vector<std::string>& supported_basic_card_networks) = 0;

  // Fill the address form given by |selector| with the given address
  // |profile|. |profile| cannot be nullptr.
  virtual void FillAddressForm(const autofill::AutofillProfile* profile,
                               const Selector& selector,
                               base::OnceCallback<void(bool)> callback) = 0;

  // Ask user to choose a card in personal data manager. GUID of the chosen card
  // will be returned through callback, otherwise empty string if the user chose
  // to continue manually.
  virtual void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Fill the card form given by |selector| with the given |card| and its
  // |cvc|. Return result asynchronously through |callback|.
  virtual void FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                            const base::string16& cvc,
                            const Selector& selector,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Select the option given by |selector| and the value of the option to be
  // picked.
  virtual void SelectOption(const Selector& selector,
                            const std::string& selected_option,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Focus on the element given by |selector|.
  virtual void FocusElement(const Selector& selector,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Sets selector of elements that can be manipulated:
  // - after the end of the script and before the beginning of the next script.
  // - during the next call to Choose()
  // whichever comes first.
  virtual void SetTouchableElements(
      const std::vector<Selector>& element_selectors) = 0;

  // Highlight the element given by |selector|.
  virtual void HighlightElement(const Selector& selector,
                                base::OnceCallback<void(bool)> callback) = 0;

  // Set the |value| of field |selector| and return the result through
  // |callback|. If |simulate_key_presses| is true, the value will be set by
  // clicking the field and then simulating key presses, otherwise the `value`
  // attribute will be set directly.
  virtual void SetFieldValue(const Selector& selector,
                             const std::string& value,
                             bool simulate_key_presses,
                             base::OnceCallback<void(bool)> callback) = 0;

  // Set the |value| of the |attribute| of the element given by |selector|.
  virtual void SetAttribute(const Selector& selector,
                            const std::vector<std::string>& attribute,
                            const std::string& value,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Return the outerHTML of an element given by |selector|.
  virtual void GetOuterHtml(
      const Selector& selector,
      base::OnceCallback<void(bool, const std::string&)> callback) = 0;

  // Load |url| in the current tab. Returns immediately, before the new page has
  // been loaded.
  virtual void LoadURL(const GURL& url) = 0;

  // Shut down Autofill Assistant at the end of the current script.
  virtual void Shutdown() = 0;

  // Shut down Autofill Assistant and close the CCT.
  virtual void CloseCustomTab() = 0;

  // Restart Autofill Assistant at the end of the current script with a cleared
  // state.
  virtual void Restart() = 0;

  // Stop the current script, shutdown autofill assistant and show |message| if
  // it is not empty or a default message otherwise.
  virtual void StopCurrentScriptAndShutdown(const std::string& message) = 0;

  // Return the current ClientMemory.
  virtual ClientMemory* GetClientMemory() = 0;

  // Get current personal data manager.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  // Get associated web contents.
  virtual content::WebContents* GetWebContents() = 0;

  // Hide contextual information.
  virtual void HideDetails() = 0;

  // Show contextual information.
  virtual void ShowDetails(const DetailsProto& details,
                           base::OnceCallback<void(bool)> callback) = 0;

  // Show the progress bar with |message| and set it at |progress|%.
  virtual void ShowProgressBar(int progress, const std::string& message) = 0;

  // Hide the progress bar.
  virtual void HideProgressBar() = 0;

  // Show the overlay.
  virtual void ShowOverlay() = 0;

  // Hide the overlay.
  virtual void HideOverlay() = 0;

 protected:
  ActionDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
