// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/unsupported_action.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"

namespace autofill_assistant {

UnsupportedAction::UnsupportedAction(const ActionProto& proto)
    : Action(proto) {}

UnsupportedAction::~UnsupportedAction() {}

void UnsupportedAction::ProcessAction(ActionDelegate* delegate,
                                      ProcessActionCallback callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  // TODO(crbug.com/806868): Add 'unsupported action' status to the protocol.
  UpdateProcessedAction(UNKNOWN_ACTION_STATUS);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
