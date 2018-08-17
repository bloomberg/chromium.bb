// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_WAIT_FOR_DOM_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_WAIT_FOR_DOM_ACTION_H_

#include "components/autofill_assistant/browser/actions/assistant_action.h"

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace autofill_assistant {
// An action to ask Chrome to wait for a DOM element to process next action.
class AssistantWaitForDomAction : public AssistantAction {
 public:
  // |timeout_ms| indicates waiting timeout period. |selectors| specifies the
  // DOM element to wait. |for_absence| indicates whether waiting for absence of
  // the element.
  AssistantWaitForDomAction(int timeout_ms,
                            const std::vector<std::string>& selectors,
                            bool for_absence);
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

  int timeout_ms_;
  std::vector<std::string> target_element_selectors_;
  bool for_absence_;

  base::WeakPtrFactory<AssistantWaitForDomAction> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(AssistantWaitForDomAction);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_WAIT_FOR_DOM_ACTION_H_