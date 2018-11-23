// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockActionDelegate : public ActionDelegate {
 public:
  MockActionDelegate();
  ~MockActionDelegate() override;

  MOCK_METHOD0(CreateBatchElementChecker,
               std::unique_ptr<BatchElementChecker>());

  void ShortWaitForElementExist(
      const Selector& selector,
      base::OnceCallback<void(bool)> callback) override {
    OnShortWaitForElementExist(selector, callback);
  }

  MOCK_METHOD2(OnShortWaitForElementExist,
               void(const Selector& selector, base::OnceCallback<void(bool)>&));

  void WaitForElementVisible(base::TimeDelta max_wait_time,
                             bool allow_interrupt,
                             const Selector& selector,
                             base::OnceCallback<void(bool)> callback) override {
    OnWaitForElementVisible(max_wait_time, allow_interrupt, selector, callback);
  }

  MOCK_METHOD4(OnWaitForElementVisible,
               void(base::TimeDelta,
                    bool,
                    const Selector&,
                    base::OnceCallback<void(bool)>&));

  MOCK_METHOD1(ShowStatusMessage, void(const std::string& message));
  MOCK_METHOD2(ClickOrTapElement,
               void(const Selector& selector,
                    base::OnceCallback<void(bool)> callback));

  void Choose(const std::vector<UiController::Choice>& choices,
              base::OnceCallback<void(const std::string&)> callback) override {
    OnChoose(choices, callback);
  }

  MOCK_METHOD2(OnChoose,
               void(const std::vector<UiController::Choice>& choice,
                    base::OnceCallback<void(const std::string&)>& callback));

  MOCK_METHOD1(ForceChoose, void(const std::string&));

  void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) override {
    OnChooseAddress(callback);
  }

  MOCK_METHOD1(OnChooseAddress,
               void(base::OnceCallback<void(const std::string&)>& callback));

  void FillAddressForm(const autofill::AutofillProfile* profile,
                       const Selector& selector,
                       base::OnceCallback<void(bool)> callback) override {
    OnFillAddressForm(profile, selector, callback);
  }

  MOCK_METHOD3(OnFillAddressForm,
               void(const autofill::AutofillProfile* profile,
                    const Selector& selector,
                    base::OnceCallback<void(bool)>& callback));

  void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) override {
    OnChooseCard(callback);
  }

  MOCK_METHOD1(OnChooseCard,
               void(base::OnceCallback<void(const std::string&)>& callback));

  void FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                    const base::string16& cvc,
                    const Selector& selector,
                    base::OnceCallback<void(bool)> callback) override {
    OnFillCardForm(card->guid(), selector, callback);
  }

  MOCK_METHOD3(OnFillCardForm,
               void(const std::string& guid,
                    const Selector& selector,
                    base::OnceCallback<void(bool)>& callback));
  MOCK_METHOD3(SelectOption,
               void(const Selector& selector,
                    const std::string& selected_option,
                    base::OnceCallback<void(bool)> callback));
  MOCK_METHOD2(FocusElement,
               void(const Selector& selector,
                    base::OnceCallback<void(bool)> callback));
  MOCK_METHOD1(SetTouchableElements,
               void(const std::vector<Selector>& element_selectors));

  MOCK_METHOD2(HighlightElement,
               void(const Selector& selector,
                    base::OnceCallback<void(bool)> callback));

  MOCK_METHOD4(
      GetPaymentInformation,
      void(payments::mojom::PaymentOptionsPtr payment_options,
           base::OnceCallback<void(std::unique_ptr<PaymentInformation>)>
               callback,
           const std::string& title,
           const std::vector<std::string>& supported_basic_card_networks));

  void SetFieldValue(const Selector& selector,
                     const std::string& value,
                     bool ignored_simulate_key_presses,
                     base::OnceCallback<void(bool)> callback) {
    OnSetFieldValue(selector, value, callback);
  }

  MOCK_METHOD3(OnSetFieldValue,
               void(const Selector& selector,
                    const std::string& value,
                    base::OnceCallback<void(bool)>& callback));

  MOCK_METHOD4(SetAttribute,
               void(const Selector& selector,
                    const std::vector<std::string>& attribute,
                    const std::string& value,
                    base::OnceCallback<void(bool)> callback));
  MOCK_METHOD2(
      GetOuterHtml,
      void(const Selector& selector,
           base::OnceCallback<void(bool, const std::string&)> callback));
  MOCK_METHOD1(LoadURL, void(const GURL& url));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Restart, void());
  MOCK_METHOD0(GetClientMemory, ClientMemory*());
  MOCK_METHOD0(GetPersonalDataManager, autofill::PersonalDataManager*());
  MOCK_METHOD0(GetWebContents, content::WebContents*());
  MOCK_METHOD1(StopCurrentScriptAndShutdown, void(const std::string& message));
  MOCK_METHOD0(HideDetails, void());
  MOCK_METHOD2(ShowDetails,
               void(const DetailsProto& details,
                    base::OnceCallback<void(bool)> callback));
  MOCK_METHOD2(ShowProgressBar, void(int progress, const std::string& message));
  MOCK_METHOD0(HideProgressBar, void());
  MOCK_METHOD0(ShowOverlay, void());
  MOCK_METHOD0(HideOverlay, void());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
