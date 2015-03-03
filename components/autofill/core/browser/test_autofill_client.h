// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"

namespace autofill {

// This class is for easier writing of tests.
class TestAutofillClient : public AutofillClient {
 public:
  TestAutofillClient();
  ~TestAutofillClient() override;

  // AutofillClient implementation.
  PersonalDataManager* GetPersonalDataManager() override;
  scoped_refptr<AutofillWebDataService> GetDatabase() override;
  PrefService* GetPrefs() override;
  IdentityProvider* GetIdentityProvider() override;
  void HideRequestAutocompleteDialog() override;
  void ShowAutofillSettings() override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(GetRealPanResult result) override;
  void ConfirmSaveCreditCard(const base::Closure& save_card_callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(const CreditCardScanCallback& callback) override;
  void ShowRequestAutocompleteDialog(const FormData& form,
                                     content::RenderFrameHost* rfh,
                                     const ResultCallback& callback) override;
  void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<Suggestion>& suggestions,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  void HideAutofillPopup() override;
  bool IsAutocompleteEnabled() override;
  void DetectAccountCreationForms(
      content::RenderFrameHost* rfh,
      const std::vector<autofill::FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  void OnFirstUserGestureObserved() override;
  void LinkClicked(const GURL& url, WindowOpenDisposition disposition) override;

  void SetPrefs(scoped_ptr<PrefService> prefs) { prefs_ = prefs.Pass(); }

 private:
  // NULL by default.
  scoped_ptr<PrefService> prefs_;
  scoped_ptr<FakeOAuth2TokenService> token_service_;
  scoped_ptr<FakeIdentityProvider> identity_provider_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillClient);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
