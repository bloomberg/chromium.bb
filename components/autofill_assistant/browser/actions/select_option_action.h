// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SELECT_OPTION_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SELECT_OPTION_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace autofill_assistant {
// An action to select an option on a given element on Web.
class SelectOptionAction : public Action {
 public:
  explicit SelectOptionAction(const ActionProto& proto);
  ~SelectOptionAction() override;

  // Overrides Action:
  void ProcessAction(ActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  void OnSelectOption(ProcessActionCallback callback, bool status);

  base::WeakPtrFactory<SelectOptionAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SelectOptionAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SELECT_OPTION_ACTION_H_
