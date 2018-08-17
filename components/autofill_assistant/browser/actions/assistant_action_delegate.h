// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_ACTION_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"

namespace autofill_assistant {
// Assistant action delegate called when processing assistant actions.
class AssistantActionDelegate {
 public:
  virtual ~AssistantActionDelegate() = default;

  // Show status message on the assistant bottom bar.
  virtual void ShowStatusMessage(const std::string& message) = 0;

  // Click the element given by |selectors| on the web page.
  virtual void ClickElement(const std::vector<std::string>& selectors,
                            base::OnceCallback<void(bool)> callback) = 0;

  // Check whether the element given by |selectors| exists on the web page.
  virtual void ElementExists(const std::vector<std::string>& selectors,
                             base::OnceCallback<void(bool)> callback) = 0;

 protected:
  AssistantActionDelegate() = default;
};
}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_ASSISTANT_ACTION_DELEGATE_H_
