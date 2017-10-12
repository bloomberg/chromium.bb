// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.h"
#import "components/autofill/ios/browser/autofill_client_ios.h"
#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"

class IdentityProvider;

namespace infobars {
class InfoBarManager;
}

namespace ios {
class ChromeBrowserState;
}

namespace password_manager {
class PasswordGenerationManager;
}

namespace web {
class WebState;
}

namespace autofill {

// Chrome iOS implementation of AutofillClientIOS.
class ChromeAutofillClientIOS : public AutofillClientIOS {
 public:
  ChromeAutofillClientIOS(
      ios::ChromeBrowserState* browser_state,
      web::WebState* web_state,
      infobars::InfoBarManager* infobar_manager,
      id<AutofillClientIOSBridge> bridge,
      password_manager::PasswordGenerationManager* password_generation_manager,
      std::unique_ptr<IdentityProvider> identity_provider);
  ~ChromeAutofillClientIOS() override;

  // AutofillClientIOS implementation.
  ukm::UkmRecorder* GetUkmRecorder() override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ConfirmSaveCreditCardLocally(const CreditCard& card,
                                    const base::Closure& callback) override;
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      std::unique_ptr<base::DictionaryValue> legal_message,
      bool should_cvc_be_requested,
      const base::Closure& callback) override;
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   const base::Closure& callback) override;
  void LoadRiskData(
      const base::Callback<void(const std::string&)>& callback) override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<FormStructure*>& forms) override;

 private:
  infobars::InfoBarManager* infobar_manager_;
  password_manager::PasswordGenerationManager* password_generation_manager_;
  std::unique_ptr<IdentityProvider> identity_provider_;
  CardUnmaskPromptControllerImpl unmask_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAutofillClientIOS);
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_H_
