// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

Action::Action(const ActionProto& proto) : proto_(proto) {}

Action::~Action() {}

void Action::UpdateProcessedAction(ProcessedActionStatusProto status) {
  // Safety check in case process action is run twice.
  *processed_action_proto_->mutable_action() = proto_;
  processed_action_proto_->set_status(status);
}

}  // namespace autofill_assistant
