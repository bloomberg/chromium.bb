// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CLIENT_IOS_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.h"
#include "components/autofill/ios/browser/autofill_client_ios_bridge.h"

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

namespace sync_driver {
class SyncService;
}

namespace autofill {

class PersonalDataManager;

// iOS implementation of AutofillClient.
// TODO(dconnelly): Split this up, as it collects functionality that doesn't
// necessarily fit together on iOS. http://crbug.com/432487.
class AutofillClientIOS : public AutofillClient {
 public:
  AutofillClientIOS(
      ios::ChromeBrowserState* browser_state,
      infobars::InfoBarManager* infobar_manager,
      id<AutofillClientIOSBridge> bridge,
      password_manager::PasswordGenerationManager* password_generation_manager,
      std::unique_ptr<IdentityProvider> identity_provider);
  ~AutofillClientIOS() override;

  // AutofillClient implementation.
  PersonalDataManager* GetPersonalDataManager() override;
  PrefService* GetPrefs() override;
  sync_driver::SyncService* GetSyncService() override;
  IdentityProvider* GetIdentityProvider() override;
  rappor::RapporService* GetRapporService() override;
  void HideRequestAutocompleteDialog() override;
  void ShowAutofillSettings() override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ConfirmSaveCreditCardLocally(const CreditCard& card,
                                    const base::Closure& callback) override;
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      std::unique_ptr<base::DictionaryValue> legal_message,
      const base::Closure& callback) override;
  void LoadRiskData(
      const base::Callback<void(const std::string&)>& callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(const CreditCardScanCallback& callback) override;
  void ShowRequestAutocompleteDialog(
      const FormData& form,
      content::RenderFrameHost* render_frame_host,
      const ResultCallback& callback) override;
  void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<Suggestion>& suggestions,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void HideAutofillPopup() override;
  bool IsAutocompleteEnabled() override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  void OnFirstUserGestureObserved() override;
  scoped_refptr<AutofillWebDataService> GetDatabase() override;
  bool IsContextSecure(const GURL& form_origin) override;

 private:
  ios::ChromeBrowserState* browser_state_;
  infobars::InfoBarManager* infobar_manager_;
  id<AutofillClientIOSBridge> bridge_;  // Weak
  password_manager::PasswordGenerationManager* password_generation_manager_;
  std::unique_ptr<IdentityProvider> identity_provider_;
  CardUnmaskPromptControllerImpl unmask_controller_;

  DISALLOW_COPY_AND_ASSIGN(AutofillClientIOS);
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CLIENT_IOS_H_
