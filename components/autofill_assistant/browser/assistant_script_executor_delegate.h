// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_EXECUTOR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_EXECUTOR_DELEGATE_H_

namespace autofill_assistant {

class AssistantService;
class AssistantUiController;

class AssistantScriptExecutorDelegate {
 public:
  virtual AssistantService* GetAssistantService() = 0;

  virtual AssistantUiController* GetAssistantUiController() = 0;

 protected:
  virtual ~AssistantScriptExecutorDelegate() {}
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_SCRIPT_EXECUTOR_DELEGATE_H_
