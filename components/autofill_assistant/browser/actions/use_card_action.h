// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_CARD_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_CARD_ACTION_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An action to ask user to choose a local card to fill the form.
class UseCardAction : public Action {
 public:
  explicit UseCardAction(const ActionProto& proto);
  ~UseCardAction() override;

  // Overrides Action:
  void ProcessAction(ActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  void OnChooseCard(ActionDelegate* delegate,
                    ProcessActionCallback callback,
                    const std::string& guid);
  void OnFillCardForm(ProcessActionCallback callback, bool result);

  base::WeakPtrFactory<UseCardAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UseCardAction);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_CARD_ACTION_H_
