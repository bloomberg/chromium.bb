// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_EXECUTOR_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/assistant_script.h"
#include "components/autofill_assistant/browser/assistant_script_executor_delegate.h"

namespace autofill_assistant {
// Class to execute an assistant script.
class AssistantScriptExecutor {
 public:
  // |script| and |delegate| should outlive this object and should not be
  // nullptr.
  AssistantScriptExecutor(AssistantScript* script,
                          AssistantScriptExecutorDelegate* delegate);
  ~AssistantScriptExecutor();

  using RunScriptCallback = base::OnceCallback<void(bool)>;
  void Run(RunScriptCallback callback);

 private:
  void onGetAssistantActions(bool result);

  AssistantScript* script_;
  AssistantScriptExecutorDelegate* delegate_;

  RunScriptCallback callback_;

  base::WeakPtrFactory<AssistantScriptExecutor> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(AssistantScriptExecutor);
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_EXECUTOR_H_
