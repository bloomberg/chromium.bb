// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
// Class to execute an assistant script.
class ScriptExecutor : public ActionDelegate {
 public:
  // |delegate| should outlive this object and should not be nullptr.
  ScriptExecutor(const std::string& script_path,
                 ScriptExecutorDelegate* delegate);
  ~ScriptExecutor() override;

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
  void SelectOption(const std::vector<std::string>& selectors,
                    const std::string& selected_option,
                    base::OnceCallback<void(bool)> callback) override;
  void FocusElement(const std::vector<std::string>& selectors,
                    base::OnceCallback<void(bool)> callback) override;
  void GetFieldsValue(
      const std::vector<std::vector<std::string>>& selectors_list,
      base::OnceCallback<void(const std::vector<std::string>&)> callback)
      override;
  void SetFieldsValue(
      const std::vector<std::vector<std::string>>& selectors_list,
      const std::vector<std::string>& values,
      base::OnceCallback<void(bool)> callback) override;
  const autofill::AutofillProfile* GetAutofillProfile(
      const std::string& guid) override;
  ClientMemory* GetClientMemory() override;

 private:
  void OnGetActions(bool result, const std::string& response);
  void ProcessNextAction();
  void ProcessAction(std::unique_ptr<Action> action);
  void GetNextActions();
  void OnProcessedAction(std::unique_ptr<ProcessedActionProto> action);

  std::string script_path_;
  ScriptExecutorDelegate* delegate_;
  RunScriptCallback callback_;

  std::deque<std::unique_ptr<Action>> actions_;
  std::vector<ProcessedActionProto> processed_actions_;
  std::string last_server_payload_;

  base::WeakPtrFactory<ScriptExecutor> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ScriptExecutor);
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_H_
