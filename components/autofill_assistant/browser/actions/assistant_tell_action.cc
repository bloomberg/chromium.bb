// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/assistant_tell_action.h"

namespace autofill_assistant {

AssistantTellAction::AssistantTellAction(const std::string& message)
    : message_(message) {}

AssistantTellAction::~AssistantTellAction() {}

void AssistantTellAction::ProcessAction(ProcessActionCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace autofill_assistant.