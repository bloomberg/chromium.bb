// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/payments/core/address_normalizer_impl.h"
#include "components/payments/core/payment_request_delegate.h"

namespace content {
class WebContents;
}

namespace payments {

class PaymentRequestDialog;

class ChromePaymentRequestDelegate : public PaymentRequestDelegate {
 public:
  explicit ChromePaymentRequestDelegate(content::WebContents* web_contents);
  ~ChromePaymentRequestDelegate() override;

  // PaymentRequestDelegate:
  void ShowDialog(PaymentRequest* request) override;
  void CloseDialog() override;
  void ShowErrorMessage() override;
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
  ukm::UkmRecorder* GetUkmRecorder() override;
  std::string GetAuthenticatedEmail() const override;
  PrefService* GetPrefService() override;

 protected:
  // Reference to the dialog so that we can satisfy calls to CloseDialog(). This
  // reference is invalid once CloseDialog() has been called on it, because the
  // dialog will be destroyed. Protected for testing.
  PaymentRequestDialog* dialog_;

 private:
  // Not owned but outlives the PaymentRequest object that owns this.
  content::WebContents* web_contents_;

  // The address normalizer to use for the duration of the Payment Request.
  AddressNormalizerImpl address_normalizer_;

  DISALLOW_COPY_AND_ASSIGN(ChromePaymentRequestDelegate);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_CHROME_PAYMENT_REQUEST_DELEGATE_H_
