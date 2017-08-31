// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_SAVE_CARD_BUBBLE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_SAVE_CARD_BUBBLE_CONTROLLER_H_

#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/ui/save_card_bubble_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockSaveCardBubbleController : public SaveCardBubbleController {
 public:
  MockSaveCardBubbleController();
  ~MockSaveCardBubbleController() override;

  MOCK_CONST_METHOD0(GetWindowTitle, base::string16());
  MOCK_CONST_METHOD0(GetExplanatoryMessage, base::string16());
  MOCK_CONST_METHOD0(GetCard, const CreditCard());
  MOCK_CONST_METHOD0(GetCvcImageResourceId, int());
  MOCK_CONST_METHOD0(ShouldRequestCvcFromUser, bool());
  MOCK_METHOD0(OnCancelButton, void());
  MOCK_METHOD0(OnLearnMoreClicked, void());
  MOCK_METHOD1(OnLegalMessageLinkClicked, void(const GURL& url));
  MOCK_METHOD0(OnBubbleClosed, void());
  MOCK_CONST_METHOD0(GetLegalMessageLines, const LegalMessageLines&());
  MOCK_METHOD0(ContinueToRequestCvcStage, void());
  MOCK_CONST_METHOD1(InputCvcIsValid, bool(const base::string16& input_text));

  base::string16 GetCvcEnteredByUser() const override;
  void OnSaveButton(const base::string16& cvc = base::string16()) override;

 private:
  base::string16 cvc_entered_by_user_;

  DISALLOW_COPY_AND_ASSIGN(MockSaveCardBubbleController);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_SAVE_CARD_BUBBLE_CONTROLLER_H_
