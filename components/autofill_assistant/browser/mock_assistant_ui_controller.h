// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_ASSISTANT_UI_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_ASSISTANT_UI_CONTROLLER_H_

#include "base/callback.h"
#include "components/autofill_assistant/browser/assistant_ui_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockAssistantUiController : public AssistantUiController {
 public:
  MockAssistantUiController();
  ~MockAssistantUiController() override;

  MOCK_METHOD1(SetUiDelegate, void(AssistantUiDelegate* ui_delegate));
  MOCK_METHOD1(ShowStatusMessage, void(const std::string& message));
  MOCK_METHOD0(ShowOverlay, void());
  MOCK_METHOD0(HideOverlay, void());

  void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) override {
    OnChooseAddress(callback);
  }
  MOCK_METHOD1(OnChooseAddress,
               void(base::OnceCallback<void(const std::string&)>& callback));

  void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) override {
    OnChooseCard(callback);
  }
  MOCK_METHOD1(OnChooseCard,
               void(base::OnceCallback<void(const std::string&)>& callback));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_ASSISTANT_UI_CONTROLLER_H_
