// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_PRECONDITION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_PRECONDITION_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/assistant_web_controller.h"

namespace autofill_assistant {
// Class represents a set of preconditions for an assistant script to be
// executed.
class AssistantScriptPrecondition {
 public:
  explicit AssistantScriptPrecondition(
      const std::vector<std::vector<std::string>>& elements_exist);
  ~AssistantScriptPrecondition();

  // Check whether the conditions satisfied and return the result through
  // |callback|.
  void Check(AssistantWebController* web_controller,
             base::OnceCallback<void(bool)> callback);

 private:
  void OnCheckElementExists(bool result);

  std::vector<std::vector<std::string>> elements_exist_;
  base::OnceCallback<void(bool)> check_preconditions_callback_;
  size_t pending_elements_exist_check_;

  base::WeakPtrFactory<AssistantScriptPrecondition> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantScriptPrecondition);
};

}  // namespace autofill_assistant.

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_PRECONDITION_H_