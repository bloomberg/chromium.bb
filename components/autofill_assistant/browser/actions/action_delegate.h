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
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/info_box.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/ui_controller.h"
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

// Action delegate called when processing actions.
class ActionDelegate {
 public:
  virtual ~ActionDelegate() = default;

  // Show status message on the bottom bar.
  virtual void SetStatusMessage(const std::string& message) = 0;

  // Returns the current status message. Usually used to restore a message after
  // the action.
  virtual std::string GetStatusMessage() = 0;

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
      base::OnceCallback<void(ProcessedActionStatusProto)> callback) = 0;

  // Click or tap the element given by |selector| on the web page.
  virtual void ClickOrTapElement(const Selector& selector,
                                 base::OnceCallback<void(bool)> callback) = 0;

  // Ask user to select one of the given suggestions.
  //
  // While a prompt is in progress, the UI looks the same as it does between
  // scripts, even though we're in the middle of a script. This includes
  // allowing access to the touchable elements set previously, in the same
  // script.
  virtual void Prompt(std::unique_ptr<std::vector<Chip>> chips) = 0;

  // Remove all chips from the UI.
  virtual void CancelPrompt() = 0;

  // Asks the user to provide the data used by UseAddressAction and
  // UseCreditCardAction.
  virtual void GetPaymentInformation(
      std::unique_ptr<PaymentRequestOptions> options) = 0;

  using GetFullCardCallback =
      base::OnceCallback<void(std::unique_ptr<autofill::CreditCard> card,
                              const base::string16& cvc)>;

  // Asks for the full card information for the selected card. Might require the
  // user entering CVC.
  virtual void GetFullCard(GetFullCardCallback callback) = 0;

  // Fill the address form given by |selector| with the given address
  // |profile|. |profile| cannot be nullptr.
  virtual void FillAddressForm(const autofill::AutofillProfile* profile,
                               const Selector& selector,
                               base::OnceCallback<void(bool)> callback) = 0;

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

  // Sets selector of areas that can be manipulated:
  // - after the end of the script and before the beginning of the next script.
  // - during the next call to SetChips()
  // whichever comes first.
  virtual void SetTouchableElementArea(
      const ElementAreaProto& touchable_element_area) = 0;

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

  // Sets the keyboard focus to |selector| and inputs the specified text parts.
  // Returns the result through |callback|.
  virtual void SendKeyboardInput(const Selector& selector,
                                 const std::vector<std::string>& text_parts,
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

  // Shut down Autofill Assistant and closes Chrome.
  virtual void Close() = 0;

  // Restart Autofill Assistant at the end of the current script with a cleared
  // state.
  virtual void Restart() = 0;

  // Return the current ClientMemory.
  virtual ClientMemory* GetClientMemory() = 0;

  // Get current personal data manager.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  // Get associated web contents.
  virtual content::WebContents* GetWebContents() = 0;

  // Clears contextual information.
  virtual void ClearDetails() = 0;

  // Sets or updates contextual information.
  virtual void SetDetails(const Details& details) = 0;

  // Clears the info box.
  virtual void ClearInfoBox() = 0;

  // Sets or updates info box.
  virtual void SetInfoBox(const InfoBox& infoBox) = 0;

  // Show the progress bar and set it at |progress|%.
  virtual void SetProgress(int progress) = 0;

  // Shows the progress bar when |visible| is true. Hides it when false.
  virtual void SetProgressVisible(bool visible) = 0;

 protected:
  ActionDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
