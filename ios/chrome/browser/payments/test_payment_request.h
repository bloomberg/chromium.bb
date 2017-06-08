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

namespace payments {
class PaymentsProfileComparator;
}  // namespace payments

namespace web {
class PaymentRequest;
class PaymentShippingOption;
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

  void SetProfileComparator(
      payments::PaymentsProfileComparator* profile_comparator) {
    profile_comparator_ = profile_comparator;
  }

  // Returns the web::PaymentRequest instance that was used to build this
  // object.
  web::PaymentRequest& web_payment_request() { return web_payment_request_; }

  // Removes all the shipping profiles.
  void ClearShippingProfiles();

  // Removes all the contact profiles.
  void ClearContactProfiles();

  // Removes all the credit cards.
  void ClearCreditCards();

  // Sets the currently selected shipping option for this PaymentRequest flow.
  void set_selected_shipping_option(web::PaymentShippingOption* option) {
    selected_shipping_option_ = option;
  }

  // PaymentRequest
  autofill::RegionDataLoader* GetRegionDataLoader() override;
  payments::PaymentsProfileComparator* profile_comparator() override;

 private:
  // Not owned and must outlive this object.
  autofill::RegionDataLoader* region_data_loader_;

  // Not owned and must outlive this object.
  payments::PaymentsProfileComparator* profile_comparator_;

  DISALLOW_COPY_AND_ASSIGN(TestPaymentRequest);
};

#endif  // IOS_CHROME_BROWSER_PAYMENTS_TEST_PAYMENT_REQUEST_H_
