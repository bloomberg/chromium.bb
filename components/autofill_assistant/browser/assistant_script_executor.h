// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_EXECUTOR_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/assistant_script.h"
#include "components/autofill_assistant/browser/assistant_script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
// Class to execute an assistant script.
class AssistantScriptExecutor : public ActionDelegate {
 public:
  // |script| and |delegate| should outlive this object and should not be
  // nullptr.
  AssistantScriptExecutor(AssistantScript* script,
                          AssistantScriptExecutorDelegate* delegate);
  ~AssistantScriptExecutor() override;

  using RunScriptCallback = base::OnceCallback<void(bool)>;
  void Run(RunScriptCallback callback);

  // Override ActionDelegate:
  void ShowStatusMessage(const std::string& message) override;
  void ClickElement(const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)> callback) override;
  void ElementExists(const std::vector<std::string>& selectors,
                     base::OnceCallback<void(bool)> callback) override;
  void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) override;
  void FillAddressForm(const std::string& guid,
                       const std::vector<std::string>& selectors,
                       base::OnceCallback<void(bool)> callback) override;
  void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) override;
  void FillCardForm(const std::string& guid,
                    const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)> callback) override;
  ClientMemory* GetClientMemory() override;

 private:
  void OnGetAssistantActions(bool result, const std::string& response);
  void ProcessNextAction();
  void ProcessAction(std::unique_ptr<Action> action);
  void GetNextAssistantActions();
  void OnProcessedAction(std::unique_ptr<Action> action, bool status);

  AssistantScript* script_;
  AssistantScriptExecutorDelegate* delegate_;
  RunScriptCallback callback_;

  std::deque<std::unique_ptr<Action>> actions_;
  std::vector<ProcessedActionProto> processed_actions_;
  std::string last_server_payload_;

  base::WeakPtrFactory<AssistantScriptExecutor> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(AssistantScriptExecutor);
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_EXECUTOR_H_
