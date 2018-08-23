// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_USE_ADDRESS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_USE_ADDRESS_ACTION_H_

#include "components/autofill_assistant/browser/actions/assistant_action.h"

#include <string>
#include <vector>

#include "base/macros.h"

namespace autofill_assistant {
// An action to ask user to choose a local address to fill the form.
class AssistantUseAddressAction : public AssistantAction {
 public:
  explicit AssistantUseAddressAction(const AssistantActionProto& proto);

  ~AssistantUseAddressAction() override;

  // Overrides AssistantAction:
  void ProcessAction(AssistantActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantUseAddressAction);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_USE_ADDRESS_ACTION_H_
