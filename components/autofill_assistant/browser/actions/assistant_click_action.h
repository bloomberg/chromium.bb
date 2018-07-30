// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_CLICK_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_CLICK_ACTION_H_

#include "components/autofill_assistant/browser/actions/assistant_action.h"

#include <string>
#include <vector>

#include "base/macros.h"

namespace autofill_assistant {
// An action to perform a mouse left button click on a given element on Web.
class AssistantClickAction : public AssistantAction {
 public:
  // CSS selectors in |selectors| are ordered from top frame to the frame
  // contains the element and the element.
  explicit AssistantClickAction(const std::vector<std::string>& selectors);
  ~AssistantClickAction() override;

  // Overrides AssistantAction:
  void ProcessAction(AssistantActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  std::vector<std::string> target_element_selectors_;

  DISALLOW_COPY_AND_ASSIGN(AssistantClickAction);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_CLICK_ACTION_H_