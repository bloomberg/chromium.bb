// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace autofill {
class AutofillProfile;
}

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
  enum FieldValueStatus { UNKNOWN, EMPTY, NOT_EMPTY };

  // Called when the user selected the data.
  void OnDataSelected(ActionDelegate* delegate,
                      const std::string& guid);

  // Fill the form using data with GUID |guid|. Return whether filling succeeded
  // or not through |callback|.
  void FillFormWithData(const std::string& guid, ActionDelegate* delegate);

  // Called when the form has been filled.
  void OnFormFilled(const std::string& guid,
                    ActionDelegate* delegate,
                    bool successful);

  // Check whether all required fields have a non-empty value. If it is the
  // case, finish the action successfully. If it's not and |allow_fallback|
  // false, fail the action. If |allow_fallback| is true, try again by filling
  // the failed fields without Autofill.
  void CheckRequiredFields(const std::string& guid,
                           ActionDelegate* delegate,
                           bool allow_fallback);

  // Called when we get the value of the required fields.
  void OnGetRequiredFieldValue(const std::string& guid,
                               ActionDelegate* delegate,
                               bool allow_fallback,
                               int index,
                               const std::string& value);

  // Get the value of |address_field| associated to profile |profile|. Return
  // empty string if there is no data available.
  base::string16 GetAddressFieldValue(
      const autofill::AutofillProfile* profile,
      const UseAddressProto::RequiredField::AddressField& address_field);

  // Called after trying to set form values without Autofill in case of fallback
  // after failed validation.
  void OnSetFieldValue(const std::string& guid,
                       ActionDelegate* delegate,
                       bool successful);

  void EndAction(bool successful);

  // Usage of the autofilled address. Ignored if autofilling a card.
  std::string name_;
  std::string prompt_;
  std::vector<std::string> selectors_;
  std::string fill_form_message_;
  std::string check_form_message_;

  // True if autofilling a card, otherwise we are autofilling an address.
  bool is_autofill_card_;
  std::vector<FieldValueStatus> required_fields_value_status_;
  size_t pending_set_field_value_;
  ProcessActionCallback process_action_callback_;
  base::WeakPtrFactory<AutofillAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_
