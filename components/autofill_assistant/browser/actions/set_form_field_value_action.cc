// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/set_form_field_value_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

SetFormFieldValueAction::SetFormFieldValueAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  DCHECK(proto_.has_set_form_value());
  DCHECK_GT(proto_.set_form_value().element().selectors_size(), 0);
  DCHECK_GT(proto_.set_form_value().value_size(), 0);
  DCHECK(proto_.set_form_value().value(0).has_text());
}

SetFormFieldValueAction::~SetFormFieldValueAction() {}

void SetFormFieldValueAction::InternalProcessAction(
    ActionDelegate* delegate,
    ProcessActionCallback callback) {
  delegate->ShortWaitForElementExist(
      ExtractVector(proto_.set_form_value().element().selectors()),
      base::BindOnce(&SetFormFieldValueAction::OnWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), base::Unretained(delegate),
                     std::move(callback)));
}

void SetFormFieldValueAction::OnWaitForElement(ActionDelegate* delegate,
                                               ProcessActionCallback callback,
                                               bool element_found) {
  if (!element_found) {
    UpdateProcessedAction(ELEMENT_RESOLUTION_FAILED);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  // TODO(crbug.com/806868): Add flag to allow simulating key presses to set
  // field value.

  // Start with first value, then call OnSetFieldValue() recursively until done.
  OnSetFieldValue(delegate, std::move(callback), /* next = */ 0, true);
}

void SetFormFieldValueAction::OnSetFieldValue(ActionDelegate* delegate,
                                              ProcessActionCallback callback,
                                              int next,
                                              bool status) {
  // If something went wrong or we are out of values: finish
  if (!status || next >= proto_.set_form_value().value_size()) {
    UpdateProcessedAction(status ? ACTION_APPLIED : OTHER_ACTION_STATUS);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  const auto& key_field = proto_.set_form_value().value(next);
  const auto& selectors =
      ExtractVector(proto_.set_form_value().element().selectors());
  switch (key_field.keypress_case()) {
    case SetFormFieldValueProto_KeyPress::kText:
      delegate->SetFieldValue(
          selectors, key_field.text(),
          /* simulate_key_presses = */ false,
          base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                         weak_ptr_factory_.GetWeakPtr(), delegate,
                         std::move(callback), /* next = */ next + 1));
      break;
    case SetFormFieldValueProto_KeyPress::kKeycode:
    // TODO(arbesser): handle keyboard presses
    default:
      DLOG(ERROR) << "Unrecognized field for SetFormFieldValueProto_KeyPress";
      OnSetFieldValue(delegate, std::move(callback), next, false);
      break;
  }
}

}  // namespace autofill_assistant
