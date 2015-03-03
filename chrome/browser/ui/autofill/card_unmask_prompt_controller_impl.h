// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_CONTROLLER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"
#include "components/autofill/core/browser/credit_card.h"

namespace autofill {

class CardUnmaskPromptView;

class CardUnmaskPromptControllerImpl : public CardUnmaskPromptController {
 public:
  explicit CardUnmaskPromptControllerImpl(content::WebContents* web_contents);
  virtual ~CardUnmaskPromptControllerImpl();

  // Functions called by ChromeAutofillClient.
  void ShowPrompt(const CreditCard& card,
                  base::WeakPtr<CardUnmaskDelegate> delegate);
  // The CVC the user entered went through validation.
  void OnVerificationResult(AutofillClient::GetRealPanResult result);

  // CardUnmaskPromptController implementation.
  void OnUnmaskDialogClosed() override;
  void OnUnmaskResponse(const base::string16& cvc,
                        const base::string16& exp_month,
                        const base::string16& exp_year,
                        bool should_store_pan) override;

  content::WebContents* GetWebContents() override;
  base::string16 GetWindowTitle() const override;
  base::string16 GetInstructionsMessage() const override;
  int GetCvcImageRid() const override;
  bool ShouldRequestExpirationDate() const override;
  bool GetStoreLocallyStartState() const override;
  bool InputTextIsValid(const base::string16& input_text) const override;

 protected:
  virtual void OnDidLoadRiskFingerprint(const std::string& risk_data);

  // Exposed for testing.
  CardUnmaskPromptView* view() { return card_unmask_view_; }

 private:
  void LoadRiskFingerprint();

  content::WebContents* web_contents_;
  CreditCard card_;
  base::WeakPtr<CardUnmaskDelegate> delegate_;
  CardUnmaskPromptView* card_unmask_view_;

  CardUnmaskDelegate::UnmaskResponse pending_response_;

  base::WeakPtrFactory<CardUnmaskPromptControllerImpl> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_CONTROLLER_IMPL_H_
