// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_TELL_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_TELL_ACTION_H_

#include "components/autofill_assistant/browser/actions/assistant_action.h"

#include <string>

#include "base/macros.h"

namespace autofill_assistant {
// An action to display a message.
class AssistantTellAction : public AssistantAction {
 public:
  // The |message| is a localized text message from the server to show user.
  explicit AssistantTellAction(const std::string& message);
  ~AssistantTellAction() override;

  // Overrides AssistantAction:
  void ProcessAction(AssistantActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  std::string message_;

  DISALLOW_COPY_AND_ASSIGN(AssistantTellAction);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_TELL_ACTION_H_