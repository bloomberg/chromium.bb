// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_ACTION_H_

#include "base/callback.h"

namespace autofill_assistant {

class AssistantActionDelegate;

// An action that performs a single step of a script on the website.
class AssistantAction {
 public:
  virtual ~AssistantAction() = default;

  // Callback returns whether process action is succeed or not.
  // Delegate should outlive this object.
  using ProcessActionCallback = base::OnceCallback<void(bool)>;
  virtual void ProcessAction(AssistantActionDelegate* delegate,
                             ProcessActionCallback callback) = 0;

 protected:
  AssistantAction() = default;
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_ACTION_H_
