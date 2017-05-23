// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_TEST_PAYMENT_REQUEST_H_
#define IOS_CHROME_BROWSER_PAYMENTS_TEST_PAYMENT_REQUEST_H_

#include "base/macros.h"
#include "ios/chrome/browser/payments/payment_request.h"

namespace autofill {
class PersonalDataManager;
class RegionDataLoader;
}  // namespace autofill

namespace web {
class PaymentRequest;
}  // namespace web

// PaymentRequest for use in tests.
class TestPaymentRequest : public PaymentRequest {
 public:
  // |personal_data_manager| should not be null and should outlive this object.
  TestPaymentRequest(const web::PaymentRequest& web_payment_request,
                     autofill::PersonalDataManager* personal_data_manager)
      : PaymentRequest(web_payment_request, personal_data_manager) {}

  ~TestPaymentRequest() override{};

  void SetRegionDataLoader(autofill::RegionDataLoader* region_data_loader) {
    region_data_loader_ = region_data_loader;
  }

  // PaymentRequest
  autofill::RegionDataLoader* GetRegionDataLoader() override;

 private:
  // Not owned and must outlive this object.
  autofill::RegionDataLoader* region_data_loader_;

  DISALLOW_COPY_AND_ASSIGN(TestPaymentRequest);
};

#endif  // IOS_CHROME_BROWSER_PAYMENTS_TEST_PAYMENT_REQUEST_H_
