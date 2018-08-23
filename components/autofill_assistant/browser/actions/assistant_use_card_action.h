// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_USE_CARD_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_USE_CARD_ACTION_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/assistant_action.h"

namespace autofill_assistant {
// An action to ask user to choose a local card to fill the form.
class AssistantUseCardAction : public AssistantAction {
 public:
  explicit AssistantUseCardAction(const ActionProto& proto);
  ~AssistantUseCardAction() override;

  // Overrides AssistantAction:
  void ProcessAction(AssistantActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  void OnChooseCard(AssistantActionDelegate* delegate,
                    ProcessActionCallback callback,
                    const std::string& guid);
  void OnFillCardForm(ProcessActionCallback callback, bool result);

  base::WeakPtrFactory<AssistantUseCardAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantUseCardAction);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_USE_CARD_ACTION_H_
