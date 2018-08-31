// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_precondition.h"

#include "base/bind.h"

namespace autofill_assistant {

ScriptPrecondition::ScriptPrecondition(
    const std::vector<std::vector<std::string>>& elements_exist)
    : elements_exist_(elements_exist), weak_ptr_factory_(this) {}

ScriptPrecondition::~ScriptPrecondition() {}

void ScriptPrecondition::Check(WebController* web_controller,
                               base::OnceCallback<void(bool)> callback) {
  DCHECK(!elements_exist_.empty());

  check_preconditions_callback_ = std::move(callback);
  pending_elements_exist_check_ = 0;
  for (const auto& element : elements_exist_) {
    pending_elements_exist_check_++;
    web_controller->ElementExists(
        element, base::BindOnce(&ScriptPrecondition::OnCheckElementExists,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

void ScriptPrecondition::OnCheckElementExists(bool result) {
  pending_elements_exist_check_--;
  // Return false early if there is a check failed.
  if (!result) {
    std::move(check_preconditions_callback_).Run(false);
    return;
  }

  // Return true if all checks have been completed and there is no failed check.
  if (!pending_elements_exist_check_ && check_preconditions_callback_) {
    std::move(check_preconditions_callback_).Run(true);
  }
}

}  // namespace autofill_assistant.
