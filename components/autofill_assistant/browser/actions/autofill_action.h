// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace autofill_assistant {
// An action to autofill a form using a local address or credit card.
class AutofillAction : public Action {
 public:
  explicit AutofillAction(const ActionProto& proto);
  ~AutofillAction() override;

  // Overrides Action:
  void ProcessAction(ActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  struct Data {
    Data();
    ~Data();

    // Name of the address to use. Ignored if autofilling a card.
    std::string name;
    std::string prompt;
    std::vector<std::string> selectors;
    std::vector<std::vector<std::string>> required_fields_selectors;

    // True if autofilling a card, otherwise we are autofilling an address.
    bool is_autofill_card;
  };

  // Called when the user selected the data.
  void OnDataSelected(ActionDelegate* delegate,
                      ProcessActionCallback callback,
                      const std::string& guid);

  // Fill the form using data with GUID |guid|. Return whether filling succeeded
  // or not through |callback|.
  void FillFormWithData(std::string guid,
                        ActionDelegate* delegate,
                        ProcessActionCallback action_callback);

  // Called when the form has been filled.
  void OnFormFilled(ActionDelegate* delegate,
                    ProcessActionCallback action_callback,
                    bool successful);

  // Called when we get the value of the required fields.
  void OnGetRequiredFieldsValue(ProcessActionCallback action_callback,
                                const std::vector<std::string>& values);

  Data data_;
  base::WeakPtrFactory<AutofillAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillAction);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_
