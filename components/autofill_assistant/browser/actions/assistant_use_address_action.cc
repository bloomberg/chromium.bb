// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/assistant_use_address_action.h"

#include "components/autofill_assistant/browser/actions/assistant_action_delegate.h"

namespace autofill_assistant {

AssistantUseAddressAction::AssistantUseAddressAction(
    const AssistantActionProto& proto)
    : AssistantAction(proto) {}

AssistantUseAddressAction::~AssistantUseAddressAction() {}

void AssistantUseAddressAction::ProcessAction(AssistantActionDelegate* delegate,
                                              ProcessActionCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace autofill_assistant.
