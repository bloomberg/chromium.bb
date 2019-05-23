// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_form_action.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {

ShowFormAction::ShowFormAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_show_form() && proto_.show_form().has_form());
}

ShowFormAction::~ShowFormAction() {}

void ShowFormAction::InternalProcessAction(ActionDelegate* delegate,
                                           ProcessActionCallback callback) {
  callback_ = std::move(callback);

  // Show the form. This will call OnFormValuesChanged with the initial result,
  // which will in turn show the "Continue" chip.
  if (!delegate->SetForm(
          std::make_unique<FormProto>(proto_.show_form().form()),
          base::BindRepeating(&ShowFormAction::OnFormValuesChanged,
                              weak_ptr_factory_.GetWeakPtr(), delegate))) {
    // The form contains unsupported or invalid inputs.
    UpdateProcessedAction(UNSUPPORTED);
    std::move(callback_).Run(std::move(processed_action_proto_));
    return;
  }
}

void ShowFormAction::OnFormValuesChanged(ActionDelegate* delegate,
                                         const FormProto::Result* form_result) {
  // Copy the current values to the action result.
  *processed_action_proto_->mutable_form_result() = *form_result;

  // Check form validity.
  // TODO(crbug.com/806868): Only check validity of inputs whose value changed
  // instead of all inputs.
  bool form_is_valid = true;
  const FormProto& form = proto_.show_form().form();
  for (int i = 0; i < form.inputs_size() && form_is_valid; i++) {
    const FormInputProto& input = form.inputs(i);
    const FormInputProto::Result& input_result = form_result->input_results(i);

    switch (input.input_type_case()) {
      case FormInputProto::InputTypeCase::kCounter: {
        DCHECK(input_result.has_counter());
        DCHECK_EQ(input.counter().counters_size(),
                  input_result.counter().values_size());

        // A counter input is valid if at least one of its counters has a value
        // different than its initial value.
        bool input_is_valid = false;
        for (int j = 0; j < input.counter().counters_size(); j++) {
          if (input_result.counter().values(j) !=
              input.counter().counters(j).initial_value()) {
            input_is_valid = true;
            break;
          }
        }

        if (!input_is_valid) {
          form_is_valid = false;
        }
        break;
      }
      case FormInputProto::InputTypeCase::kSelection: {
        DCHECK(input_result.has_selection());
        DCHECK_EQ(input.selection().choices_size(),
                  input_result.selection().selected_size());

        // A selection input is valid if the number of selected choices is
        // greater or equal than |min_selected_choices|.
        int min_selected = input.selection().min_selected_choices();
        if (min_selected == 0)
          break;

        int n = 0;
        for (bool selected : input_result.selection().selected()) {
          if (selected && ++n >= min_selected) {
            break;
          }
        }

        if (n < min_selected) {
          form_is_valid = false;
        }
        break;
      }
      case FormInputProto::InputTypeCase::INPUT_TYPE_NOT_SET:
        NOTREACHED();
        break;
        // Intentionally no default case to make compilation fail if a new value
        // was added to the enum but not to this list.
    }
  }

  // Show "Continue" chip.
  // TODO(crbug.com/806868): Make this chip configurable.
  auto chips = std::make_unique<std::vector<Chip>>();

  if (proto_.show_form().has_chip()) {
    chips->emplace_back(proto_.show_form().chip());
    SetDefaultChipType(chips.get());
  } else {
    chips->emplace_back();
    chips->back().text =
        l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_PAYMENT_INFO_CONFIRM);
    chips->back().type = HIGHLIGHTED_ACTION;
  }

  chips->back().disabled = !form_is_valid;
  if (form_is_valid) {
    chips->back().callback =
        base::BindOnce(&ShowFormAction::OnButtonClicked,
                       weak_ptr_factory_.GetWeakPtr(), delegate);
  }

  delegate->Prompt(std::move(chips));
}

void ShowFormAction::OnButtonClicked(ActionDelegate* delegate) {
  DCHECK(callback_);
  delegate->SetForm(nullptr, base::DoNothing());
  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
