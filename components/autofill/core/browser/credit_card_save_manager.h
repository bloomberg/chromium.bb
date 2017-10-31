// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_SAVE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

// Manages logic for determining whether upload credit card save to Google
// Payments is available as well as actioning both local and upload credit card
// save logic.  Owned by FormDataImporter.
class CreditCardSaveManager : public payments::PaymentsClientSaveDelegate {
 public:
  // The parameters should outlive the CreditCardSaveManager.
  CreditCardSaveManager(AutofillClient* client,
                        payments::PaymentsClient* payments_client,
                        const std::string& app_locale,
                        PersonalDataManager* personal_data_manager);
  virtual ~CreditCardSaveManager();

  // Offers local credit card save to the user.
  void OfferCardLocalSave(const CreditCard& card);

  // Begins the process to offer upload credit card save to the user if the
  // imported card passes all requirements and Google Payments approves.
  void AttemptToOfferCardUploadSave(const FormStructure& submitted_form,
                                    const CreditCard& card);

  // Returns true if all the conditions for enabling the upload of credit card
  // are satisfied.
  virtual bool IsCreditCardUploadEnabled();

  // For testing.
  void SetAppLocale(std::string app_locale) { app_locale_ = app_locale; }

 protected:
  // payments::PaymentsClientSaveDelegate:
  // Exposed for testing.
  void OnDidUploadCard(AutofillClient::PaymentsRpcResult result,
                       const std::string& server_id) override;

 private:
  // payments::PaymentsClientSaveDelegate:
  void OnDidGetUploadDetails(
      AutofillClient::PaymentsRpcResult result,
      const base::string16& context_token,
      std::unique_ptr<base::DictionaryValue> legal_message) override;

  // Examines |card| and the stored profiles and if a candidate set of profiles
  // is found that matches the client-side validation rules, assigns the values
  // to |upload_request.profiles| and returns 0. If no valid set can be found,
  // returns the failure reasons. Appends any experiments that were triggered to
  // |upload_request.active_experiments|. The return value is a bitmask of
  // |AutofillMetrics::CardUploadDecisionMetric|.
  int SetProfilesForCreditCardUpload(
      const CreditCard& card,
      payments::PaymentsClient::UploadRequestDetails* upload_request) const;

  // Sets |user_did_accept_upload_prompt_| and calls SendUploadCardRequest if
  // the risk data is available.
  void OnUserDidAcceptUpload();

  // Saves risk data in |uploading_risk_data_| and calls SendUploadCardRequest
  // if the user has accepted the prompt.
  void OnDidGetUploadRiskData(const std::string& risk_data);

  // Finalizes the upload request and calls PaymentsClient::UploadCard().
  void SendUploadCardRequest();

  // Returns metric relevant to the CVC field based on values in
  // |found_cvc_field_|, |found_value_in_cvc_field_| and
  // |found_cvc_value_in_non_cvc_field_|.
  AutofillMetrics::CardUploadDecisionMetric GetCVCCardUploadDecisionMetric()
      const;

  // Logs the card upload decisions in UKM and UMA.
  // |upload_decision_metrics| is a bitmask of
  // |AutofillMetrics::CardUploadDecisionMetric|.
  void LogCardUploadDecisions(int upload_decision_metrics);

  AutofillClient* const client_;

  // Handles Payments service requests.
  // Owned by AutofillManager.
  payments::PaymentsClient* payments_client_;

  std::string app_locale_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the AutofillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  PersonalDataManager* personal_data_manager_;

  // Collected information about a pending upload request.
  payments::PaymentsClient::UploadRequestDetails upload_request_;
  // |true| if the user has opted to upload save their credit card to Google.
  bool user_did_accept_upload_prompt_ = false;

  // |should_cvc_be_requested_| is |true| if we should request CVC from the user
  // in the card upload dialog.
  bool should_cvc_be_requested_ = false;
  // |found_cvc_field_| is |true| if there exists a field that is determined to
  // be a CVC field via heuristics.
  bool found_cvc_field_ = false;
  // |found_value_in_cvc_field_| is |true| if a field that is determined to
  // be a CVC field via heuristics has non-empty |value|.
  // |value| may or may not be a valid CVC.
  bool found_value_in_cvc_field_ = false;
  // |found_cvc_value_in_non_cvc_field_| is |true| if a field that is not
  // determined to be a CVC field via heuristics has a valid CVC |value|.
  bool found_cvc_value_in_non_cvc_field_ = false;

  GURL pending_upload_request_url_;

  base::WeakPtrFactory<CreditCardSaveManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CreditCardSaveManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_SAVE_MANAGER_H_
