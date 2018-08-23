// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/assistant_tell_action.h"
#include "components/autofill_assistant/browser/actions/assistant_action_delegate.h"

#include <utility>

namespace autofill_assistant {

AssistantTellAction::AssistantTellAction(const AssistantActionProto& proto)
    : AssistantAction(proto) {
  DCHECK(proto_.has_tell());
}

AssistantTellAction::~AssistantTellAction() {}

void AssistantTellAction::ProcessAction(AssistantActionDelegate* delegate,
                                        ProcessActionCallback callback) {
  // tell.message in the proto is localized.
  delegate->ShowStatusMessage(proto_.tell().message());
  std::move(callback).Run(true);
}

}  // namespace autofill_assistant.
