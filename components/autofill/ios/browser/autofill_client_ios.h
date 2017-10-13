// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.h"
#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"

class IdentityProvider;

namespace syncer {
class SyncService;
}

namespace web {
class WebState;
}

class PrefService;

namespace autofill {

class PersonalDataManager;

// iOS implementation of AutofillClient.
class AutofillClientIOS : public AutofillClient {
 public:
  AutofillClientIOS(
      PrefService* pref_service,
      PersonalDataManager* personal_data_manager,
      web::WebState* web_state,
      id<AutofillClientIOSBridge> bridge,
      std::unique_ptr<IdentityProvider> identity_provider,
      scoped_refptr<AutofillWebDataService> autofill_web_data_service);
  ~AutofillClientIOS() override;

  // AutofillClient implementation.
  PersonalDataManager* GetPersonalDataManager() override;
  PrefService* GetPrefs() override;
  syncer::SyncService* GetSyncService() override;
  IdentityProvider* GetIdentityProvider() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  SaveCardBubbleController* GetSaveCardBubbleController() override;
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
      bool should_cvc_be_requested,
      const base::Closure& callback) override;
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   const base::Closure& callback) override;
  void LoadRiskData(
      const base::Callback<void(const std::string&)>& callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(const CreditCardScanCallback& callback) override;
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
  scoped_refptr<AutofillWebDataService> GetDatabase() override;
  void DidInteractWithNonsecureCreditCardInput(
      content::RenderFrameHost* rfh) override;
  bool IsContextSecure() override;
  bool ShouldShowSigninPromo() override;
  bool IsAutofillSupported() override;
  void ExecuteCommand(int id) override;

 private:
  PrefService* pref_service_;
  PersonalDataManager* personal_data_manager_;
  web::WebState* web_state_;
  __weak id<AutofillClientIOSBridge> bridge_;
  std::unique_ptr<IdentityProvider> identity_provider_;
  scoped_refptr<AutofillWebDataService> autofill_web_data_service_;

  DISALLOW_COPY_AND_ASSIGN(AutofillClientIOS);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_H_
