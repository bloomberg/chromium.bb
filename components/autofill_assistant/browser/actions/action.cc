// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

Action::Action(const ActionProto& proto) : proto_(proto) {}

Action::~Action() {}

void Action::ProcessAction(ActionDelegate* delegate,
                           ProcessActionCallback callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  InternalProcessAction(delegate, std::move(callback));
}

void Action::UpdateProcessedAction(ProcessedActionStatusProto status) {
  // Safety check in case process action is run twice.
  *processed_action_proto_->mutable_action() = proto_;
  processed_action_proto_->set_status(status);
}

// static
std::vector<std::string> Action::ExtractVector(
    const google::protobuf::RepeatedPtrField<std::string>& repeated_strings) {
  std::vector<std::string> vector;
  for (const auto& string : repeated_strings) {
    vector.emplace_back(string);
  }
  return vector;
}

}  // namespace autofill_assistant
