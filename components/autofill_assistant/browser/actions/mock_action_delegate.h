// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_

#include <memory>
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

  void WaitForElementVisible(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      const Selector& selector,
      base::OnceCallback<void(ProcessedActionStatusProto)> callback) override {
    OnWaitForElementVisible(max_wait_time, allow_interrupt, selector, callback);
  }

  MOCK_METHOD4(OnWaitForElementVisible,
               void(base::TimeDelta,
                    bool,
                    const Selector&,
                    base::OnceCallback<void(ProcessedActionStatusProto)>&));

  MOCK_METHOD1(SetStatusMessage, void(const std::string& message));
  MOCK_METHOD0(GetStatusMessage, std::string());
  MOCK_METHOD2(ClickOrTapElement,
               void(const Selector& selector,
                    base::OnceCallback<void(bool)> callback));

  MOCK_METHOD1(Prompt, void(std::unique_ptr<std::vector<Chip>> chips));
  MOCK_METHOD0(CancelPrompt, void());

  void FillAddressForm(const autofill::AutofillProfile* profile,
                       const Selector& selector,
                       base::OnceCallback<void(bool)> callback) override {
    OnFillAddressForm(profile, selector, callback);
  }

  MOCK_METHOD3(OnFillAddressForm,
               void(const autofill::AutofillProfile* profile,
                    const Selector& selector,
                    base::OnceCallback<void(bool)>& callback));

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
  MOCK_METHOD1(SetTouchableElementArea,
               void(const ElementAreaProto& touchable_element_area));

  MOCK_METHOD2(HighlightElement,
               void(const Selector& selector,
                    base::OnceCallback<void(bool)> callback));

  MOCK_METHOD1(GetPaymentInformation,
               void(std::unique_ptr<PaymentRequestOptions> options));

  MOCK_METHOD1(GetFullCard, void(GetFullCardCallback callback));

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

  MOCK_METHOD3(SendKeyboardInput,
               void(const Selector& selector,
                    const std::vector<std::string>& text_parts,
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
  MOCK_METHOD1(SetDetails, void(const Details& details));
  MOCK_METHOD0(ClearDetails, void());
  MOCK_METHOD1(SetInfoBox, void(const InfoBox& info_box));
  MOCK_METHOD0(ClearInfoBox, void());
  MOCK_METHOD1(SetProgress, void(int progress));
  MOCK_METHOD1(SetProgressVisible, void(bool visible));
  MOCK_METHOD1(SetChips, void(std::unique_ptr<std::vector<Chip>> chips));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
