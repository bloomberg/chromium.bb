// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_REQUEST_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_REQUEST_DELEGATE_H_

#include <string>

#include "components/payments/core/payment_request_delegate.h"
#include "components/payments/core/test_address_normalizer.h"

namespace payments {

class TestPaymentRequestDelegate : public PaymentRequestDelegate {
 public:
  TestPaymentRequestDelegate(
      autofill::PersonalDataManager* personal_data_manager);
  ~TestPaymentRequestDelegate() override;

  // PaymentRequestDelegate
  void ShowDialog(PaymentRequest* request) override {}
  void CloseDialog() override {}
  void ShowErrorMessage() override {}
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  const std::string& GetApplicationLocale() const override;
  bool IsIncognito() const override;
  bool IsSslCertificateValid() override;
  const GURL& GetLastCommittedURL() const override;
  void DoFullCardRequest(
      const autofill::CreditCard& credit_card,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate) override;
  AddressNormalizer* GetAddressNormalizer() override;
  autofill::RegionDataLoader* GetRegionDataLoader() override;
  ukm::UkmService* GetUkmService() override;
  std::string GetAuthenticatedEmail() const override;
  PrefService* GetPrefService() override;

  TestAddressNormalizer* test_address_normalizer();
  void DelayFullCardRequestCompletion();
  void CompleteFullCardRequest();

 private:
  autofill::PersonalDataManager* personal_data_manager_;
  std::string locale_;
  const GURL last_committed_url_;
  TestAddressNormalizer address_normalizer_;

  bool instantaneous_full_card_request_result_ = true;
  autofill::CreditCard full_card_request_card_;
  base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
      full_card_result_delegate_;
  DISALLOW_COPY_AND_ASSIGN(TestPaymentRequestDelegate);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_TEST_PAYMENT_REQUEST_DELEGATE_H_
