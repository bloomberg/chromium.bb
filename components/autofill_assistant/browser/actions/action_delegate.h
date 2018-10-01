// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"

class GURL;

namespace autofill {
class AutofillProfile;
}

namespace autofill_assistant {
class ClientMemory;
class NodeProto;

// Action delegate called when processing actions.
class ActionDelegate {
 public:
  virtual ~ActionDelegate() = default;

  // Show status message on the bottom bar.
  virtual void ShowStatusMessage(const std::string& message) = 0;

  // Click the element given by |selectors| on the web page.
  virtual void ClickElement(const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Check whether the element given by |selectors| exists on the web page.
  virtual void ElementExists(const std::vector<std::string>& selectors,
                             base::OnceCallback<void(bool)> callback) = 0;

  // Ask user to choose an address in personal data manager. GUID of the chosen
  // address will be returned through callback, otherwise empty string if the
  // user chose to continue manually.
  virtual void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Fill the address form given by |selectors| with the given address |guid| in
  // personal data manager.
  virtual void FillAddressForm(const std::string& guid,
                               const std::vector<std::string>& selectors,
                               base::OnceCallback<void(bool)> callback) = 0;

  // Ask user to choose a card in personal data manager. GUID of the chosen card
  // will be returned through callback, otherwise empty string if the user chose
  // to continue manually.
  virtual void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Fill the card form given by |selectors| with the given card |guid| in
  // personal data manager.
  virtual void FillCardForm(const std::string& guid,
                            const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Select the option given by |selectors| and the value of the option to be
  // picked.
  virtual void SelectOption(const std::vector<std::string>& selectors,
                            const std::string& selected_option,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Focus on the element given by |selectors|.
  virtual void FocusElement(const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Get the value of |selectors| and return the result through |callback|. The
  // returned value will be the empty string in case of error or empty value.
  virtual void GetFieldValue(
      const std::vector<std::string>& selectors,
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Set the |value| of field |selectors| and return the result through
  // |callback|.
  virtual void SetFieldValue(const std::vector<std::string>& selectors,
                             const std::string& value,
                             base::OnceCallback<void(bool)> callback) = 0;

  // Get the AutofillProfile with ID |guid|, or nullptr if it doesn't exist.
  virtual const autofill::AutofillProfile* GetAutofillProfile(
      const std::string& guid) = 0;

  // Given an element |selectors| on the page as the root element, build a node
  // tree using the output parameter |node_tree_out| as a starting node.
  virtual void BuildNodeTree(const std::vector<std::string>& selectors,
                             NodeProto* node_tree_out,
                             base::OnceCallback<void(bool)> callback) = 0;

  // Load |url| in the current tab. Returns immediately, before the new page has
  // been loaded.
  virtual void LoadURL(const GURL& url) = 0;

  // Shut down Autofill Assistant at the end of the current script.
  virtual void Shutdown() = 0;

  // Restart Autofill Assistant at the end of the current script with a cleared
  // state.
  virtual void Restart() = 0;

  // Stop the current script and show |message| if it is not empty .
  virtual void StopCurrentScript(const std::string& message);

  // Return the current ClientMemory.
  virtual ClientMemory* GetClientMemory() = 0;

 protected:
  ActionDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ACTION_DELEGATE_H_
