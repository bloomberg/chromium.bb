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

  MOCK_METHOD1(OnStatusMessageChanged, void(const std::string& message));
  MOCK_METHOD1(OnStateChanged, void(AutofillAssistantState));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(SetChips, void(std::unique_ptr<std::vector<Chip>> chips));
  MOCK_METHOD0(ClearChips, void());
  MOCK_METHOD4(
      GetPaymentInformation,
      void(payments::mojom::PaymentOptionsPtr payment_options,
           base::OnceCallback<void(std::unique_ptr<PaymentInformation>)>
               callback,
           const std::string& title,
           const std::vector<std::string>& supported_basic_card_networks));
  MOCK_METHOD1(OnDetailsChanged, void(const Details* details));
  MOCK_METHOD1(ShowProgressBar, void(int progress));
  MOCK_METHOD0(HideProgressBar, void());
  MOCK_METHOD2(UpdateTouchableArea,
               void(bool enabled, const std::vector<RectF>& areas));
  MOCK_CONST_METHOD0(Terminate, bool());
  MOCK_METHOD0(ExpandBottomSheet, void());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_UI_CONTROLLER_H_
