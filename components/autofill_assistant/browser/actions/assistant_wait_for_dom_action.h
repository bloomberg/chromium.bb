// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_WAIT_FOR_DOM_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_WAIT_FOR_DOM_ACTION_H_

#include "components/autofill_assistant/browser/actions/assistant_action.h"
#include "components/autofill_assistant/browser/assistant.pb.h"

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace autofill_assistant {
// An action to ask Chrome to wait for a DOM element to process next action.
class AssistantWaitForDomAction : public AssistantAction {
 public:
  explicit AssistantWaitForDomAction(const AssistantActionProto& proto);
  ~AssistantWaitForDomAction() override;

  // Overrides AssistantAction:
  void ProcessAction(AssistantActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  void CheckElementExists(AssistantActionDelegate* delegate,
                          int rounds,
                          ProcessActionCallback callback);
  void OnCheckElementExists(AssistantActionDelegate* delegate,
                            int rounds,
                            ProcessActionCallback callback,
                            bool result);

  base::WeakPtrFactory<AssistantWaitForDomAction> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(AssistantWaitForDomAction);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_WAIT_FOR_DOM_ACTION_H_
