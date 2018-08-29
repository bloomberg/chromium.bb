// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_ADDRESS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_ADDRESS_ACTION_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {
// An action to ask user to choose a local address to fill the form.
class UseAddressAction : public Action {
 public:
  explicit UseAddressAction(const ActionProto& proto);

  ~UseAddressAction() override;

  // Overrides Action:
  void ProcessAction(ActionDelegate* delegate,
                     ProcessActionCallback callback) override;

 private:
  void OnChooseAddress(ActionDelegate* delegate,
                       ProcessActionCallback callback,
                       const std::string& guid);
  void OnFillAddressForm(ProcessActionCallback callback, bool result);

  base::WeakPtrFactory<UseAddressAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UseAddressAction);
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_USE_ADDRESS_ACTION_H_
