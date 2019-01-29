// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_ACTION_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/chip.h"

namespace autofill_assistant {

// Allow the selection of one or more suggestions.
class PromptAction : public Action {
 public:
  explicit PromptAction(const ActionProto& proto);
  ~PromptAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ActionDelegate* delegate,
                             ProcessActionCallback callback) override;

  std::unique_ptr<std::vector<Chip>> CreateChips();
  void OnElementExist(const std::string& payload, bool exists);
  void OnRequiredElementExists(ActionDelegate* delegate,
                               int choice_index,
                               bool exists);
  void OnElementChecksDone(ActionDelegate* delegate);
  void OnSuggestionChosen(const std::string& payload);

  ProcessActionCallback callback_;

  std::string forced_payload_;
  std::unique_ptr<BatchElementChecker> batch_element_checker_;

  // Index of an element in PromptActionProto::choices that has a
  // show_only_if_element_exists condition that is met.
  std::set<int> required_element_found_;
  base::WeakPtrFactory<PromptAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PromptAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_ACTION_H_
