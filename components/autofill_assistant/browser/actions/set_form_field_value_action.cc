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
      ExtractSelector(proto_.set_form_value().element()),
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
  OnSetFieldValue(delegate, std::move(callback), /* next = */ 0,
                  /* status= */ true);
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
  const auto& selector = ExtractSelector(proto_.set_form_value().element());
  switch (key_field.keypress_case()) {
    case SetFormFieldValueProto_KeyPress::kText:
      delegate->SetFieldValue(
          selector, key_field.text(),
          /* simulate_key_presses = */ false,
          base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                         weak_ptr_factory_.GetWeakPtr(), delegate,
                         std::move(callback), /* next = */ next + 1));
      break;
    case SetFormFieldValueProto_KeyPress::kKeycode:
      // DEPRECATED: the field `keycode' used to contain a single character to
      // input as text. Since there is no easy way to convert keycodes to text,
      // this field is now deprecated and only works for US-ASCII characters.
      // You should use the `keyboard_input' field instead.
      if (key_field.keycode() < 128) {  // US-ASCII
        delegate->SendKeyboardInput(
            selector, std::string(1, char(key_field.keycode())),
            base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                           weak_ptr_factory_.GetWeakPtr(), delegate,
                           std::move(callback), /* next = */ next + 1));
      } else {
        DLOG(ERROR)
            << "SetFormFieldValueProto_KeyPress: field `keycode' is deprecated "
            << "and only supports US-ASCII values (encountered "
            << key_field.keycode() << "). Use field `key' instead.";
        OnSetFieldValue(delegate, std::move(callback), next,
                        /* status= */ false);
      }
      break;
    case SetFormFieldValueProto_KeyPress::kKeyboardInput:
      delegate->SendKeyboardInput(
          selector, key_field.keyboard_input(),
          base::BindOnce(&SetFormFieldValueAction::OnSetFieldValue,
                         weak_ptr_factory_.GetWeakPtr(), delegate,
                         std::move(callback), /* next = */ next + 1));
      break;
    default:
      DLOG(ERROR) << "Unrecognized field for SetFormFieldValueProto_KeyPress";
      OnSetFieldValue(delegate, std::move(callback), next, /* status= */ false);
      break;
  }
}

}  // namespace autofill_assistant
