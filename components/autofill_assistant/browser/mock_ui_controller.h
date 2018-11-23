// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_UI_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_UI_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockUiController : public UiController {
 public:
  MockUiController();
  ~MockUiController() override;

  MOCK_METHOD1(SetUiDelegate, void(UiDelegate* ui_delegate));
  MOCK_METHOD1(ShowStatusMessage, void(const std::string& message));
  MOCK_METHOD0(GetStatusMessage, std::string());
  MOCK_METHOD0(ShowOverlay, void());
  MOCK_METHOD0(HideOverlay, void());
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(ShutdownGracefully, void());
  MOCK_METHOD0(CloseCustomTab, void());
  MOCK_METHOD1(UpdateScripts, void(const std::vector<ScriptHandle>& scripts));

  void Choose(const std::vector<UiController::Choice>& choices,
              base::OnceCallback<void(const std::string&)> callback) override {
    OnChoose(choices, callback);
  }
  MOCK_METHOD2(OnChoose,
               void(const std::vector<UiController::Choice>& choices,
                    base::OnceCallback<void(const std::string&)>& callback));

  void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) override {
    OnChooseAddress(callback);
  }
  MOCK_METHOD1(OnChooseAddress,
               void(base::OnceCallback<void(const std::string&)>& callback));

  MOCK_METHOD1(ForceChoose, void(const std::string&));

  void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) override {
    OnChooseCard(callback);
  }
  MOCK_METHOD1(OnChooseCard,
               void(base::OnceCallback<void(const std::string&)>& callback));
  MOCK_METHOD4(
      GetPaymentInformation,
      void(payments::mojom::PaymentOptionsPtr payment_options,
           base::OnceCallback<void(std::unique_ptr<PaymentInformation>)>
               callback,
           const std::string& title,
           const std::vector<std::string>& supported_basic_card_networks));
  MOCK_METHOD0(HideDetails, void());
  MOCK_METHOD2(ShowDetails,
               void(const DetailsProto& details,
                    base::OnceCallback<void(bool)> callback));
  MOCK_METHOD2(ShowProgressBar, void(int progress, const std::string& message));
  MOCK_METHOD0(HideProgressBar, void());
  MOCK_METHOD2(UpdateTouchableArea,
               void(bool enabled, const std::vector<RectF>& areas));
  MOCK_CONST_METHOD0(GetDebugContext, std::string());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_UI_CONTROLLER_H_
