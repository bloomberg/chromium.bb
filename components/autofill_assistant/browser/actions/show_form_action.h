// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_FORM_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_FORM_ACTION_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An action to show a form than can be filled by the user.
class ShowFormAction : public Action {
 public:
  explicit ShowFormAction(const ActionProto& proto);
  ~ShowFormAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) override;

  void OnFormValuesChanged(ActionDelegate* delegate,
                           const FormProto::Result* form_result);
  void OnButtonClicked(ActionDelegate* delegate);
  bool IsFormValid(const FormProto& form, const FormProto::Result& result);
  bool IsCounterInputValid(const CounterInputProto& input,
                           const CounterInputProto::Result& result);
  bool IsCounterValidationRuleSatisfied(
      const CounterInputProto::ValidationRule& rule,
      const CounterInputProto& input,
      const CounterInputProto::Result& result);
  bool IsSelectionInputValid(const SelectionInputProto& input,
                             const SelectionInputProto::Result& result);

  ProcessActionCallback callback_;
  base::WeakPtrFactory<ShowFormAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShowFormAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_FORM_ACTION_H_
