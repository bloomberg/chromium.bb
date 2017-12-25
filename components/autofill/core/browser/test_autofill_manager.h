// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/run_loop.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"

using base::TimeTicks;

namespace net {
class URLRequestContextGetter;
}

namespace autofill {

class AutofillClient;
class AutofillDriver;
class FormStructure;

class TestAutofillManager : public AutofillManager {
 public:
  // Called by AutofillManagerTest and AutofillMetricsTest.
  TestAutofillManager(AutofillDriver* driver,
                      AutofillClient* client,
                      TestPersonalDataManager* personal_data);
  // Called by CreditCardSaveManagerTest.
  TestAutofillManager(AutofillDriver* driver,
                      AutofillClient* client,
                      TestPersonalDataManager* personal_data,
                      CreditCardSaveManager* credit_card_save_manager,
                      payments::TestPaymentsClient* payments_client);
  ~TestAutofillManager() override;

  // AutofillManager overrides.
  bool IsAutofillEnabled() const override;
  bool IsCreditCardAutofillEnabled() override;
  void UploadFormData(const FormStructure& submitted_form,
                      bool observed_submission) override;
  void UploadFormDataAsyncCallback(const FormStructure* submitted_form,
                                   const base::TimeTicks& load_time,
                                   const base::TimeTicks& interaction_time,
                                   const base::TimeTicks& submission_time,
                                   bool observed_submission) override;

  // Unique to TestAutofillManager:

  // Resets the run loop so that it can wait for an asynchronous form
  // submission to complete.
  void ResetRunLoop();
  void RunRunLoop();

  // Wait for the asynchronous calls within StartUploadProcess() to complete.
  void WaitForAsyncUploadProcess();

  int GetPackedCreditCardID(int credit_card_id);

  void AddSeenForm(const FormData& form,
                   const std::vector<ServerFieldType>& heuristic_types,
                   const std::vector<ServerFieldType>& server_types);

  void AddSeenFormStructure(std::unique_ptr<FormStructure> form_structure);

  // Calls AutofillManager::OnWillSubmitForm and waits for it to complete.
  void WillSubmitForm(const FormData& form, const TimeTicks& timestamp);

  // Calls both AutofillManager::OnWillSubmitForm and
  // AutofillManager::OnFormSubmitted.
  void SubmitForm(const FormData& form, const TimeTicks& timestamp);

  void ClearFormStructures();

  const std::string GetSubmittedFormSignature();

  void SetAutofillEnabled(bool autofill_enabled);

  void SetCreditCardEnabled(bool credit_card_enabled);

  void SetExpectedSubmittedFieldTypes(
      const std::vector<ServerFieldTypeSet>& expected_types);

  void SetExpectedObservedSubmission(bool expected);

  void SetCallParentUploadFormData(bool value);

 private:
  TestPersonalDataManager* personal_data_;                  // Weak reference.
  net::URLRequestContextGetter* context_getter_ = nullptr;  // Weak reference.
  TestFormDataImporter* test_form_data_importer_ = nullptr;
  bool autofill_enabled_ = true;
  bool credit_card_enabled_ = true;
  bool call_parent_upload_form_data_ = false;
  base::Optional<bool> expected_observed_submission_;

  std::unique_ptr<base::RunLoop> run_loop_;

  std::string submitted_form_signature_;
  std::vector<ServerFieldTypeSet> expected_submitted_field_types_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_MANAGER_H_
