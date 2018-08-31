// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_

namespace autofill_assistant {

class Service;
class UiController;
class WebController;
struct ClientMemory;

class ScriptExecutorDelegate {
 public:
  virtual Service* GetService() = 0;

  virtual UiController* GetUiController() = 0;

  virtual WebController* GetWebController() = 0;

  virtual ClientMemory* GetClientMemory() = 0;

 protected:
  virtual ~ScriptExecutorDelegate() {}
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_
