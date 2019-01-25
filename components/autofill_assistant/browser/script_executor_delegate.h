// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_

#include <map>
#include <string>

#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/state.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace content {
class WebContents;
}  // namespace content

namespace autofill_assistant {

class Service;
class UiController;
class WebController;
class ClientMemory;

class ScriptExecutorDelegate {
 public:
  virtual Service* GetService() = 0;

  virtual UiController* GetUiController() = 0;

  virtual WebController* GetWebController() = 0;

  virtual ClientMemory* GetClientMemory() = 0;

  virtual const std::map<std::string, std::string>& GetParameters() = 0;

  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  virtual content::WebContents* GetWebContents() = 0;

  virtual void EnterState(AutofillAssistantState state) = 0;

  // Make the area of the screen that correspond to the given elements
  // touchable.
  virtual void SetTouchableElementArea(const ElementAreaProto& element) = 0;
  virtual void SetStatusMessage(const std::string& message) = 0;
  virtual std::string GetStatusMessage() const = 0;
  virtual void SetDetails(const Details& details) = 0;
  virtual void ClearDetails() = 0;

  // Makes no area of the screen touchable.
  void ClearTouchableElementArea() {
    SetTouchableElementArea(ElementAreaProto::default_instance());
  }

 protected:
  virtual ~ScriptExecutorDelegate() {}
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_EXECUTOR_DELEGATE_H_
