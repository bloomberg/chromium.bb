// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_UI_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_UI_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill_assistant/browser/metrics.h"
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
  MOCK_METHOD1(Shutdown, void(Metrics::DropOutReason));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(OnChipsChanged, void(const std::vector<Chip>& chips));
  MOCK_METHOD3(
      GetPaymentInformation,
      void(payments::mojom::PaymentOptionsPtr payment_options,
           base::OnceCallback<void(std::unique_ptr<PaymentInformation>)>
               callback,
           const std::vector<std::string>& supported_basic_card_networks));
  MOCK_METHOD1(OnDetailsChanged, void(const Details* details));
  MOCK_METHOD1(OnProgressChanged, void(int progress));
  MOCK_METHOD1(OnTouchableAreaChanged, void(const std::vector<RectF>& areas));
  MOCK_CONST_METHOD0(Terminate, bool());
  MOCK_CONST_METHOD0(GetDropOutReason, Metrics::DropOutReason());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_UI_CONTROLLER_H_
