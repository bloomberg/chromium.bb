// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_UNITTEST_BASE_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_UNITTEST_BASE_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/web/public/test/fakes/test_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

class SigninManager;

// Base class for various payment request related unit tests. This purposely
// does not inherit from PlatformTest (testing::Test) so that it can be used
// by ViewController unit tests.
class PaymentRequestUnitTestBase {
 protected:
  PaymentRequestUnitTestBase();
  ~PaymentRequestUnitTestBase();

  void SetUp();
  void TearDown();

  // Should be called after data is added to the database via AddAutofillProfile
  // and/or AddCreditCard.
  void CreateTestPaymentRequest();

  void AddAutofillProfile(autofill::AutofillProfile profile);
  void AddCreditCard(autofill::CreditCard card);

  SigninManager* GetSigninManager();

  payments::TestPaymentRequest* payment_request() {
    return payment_request_.get();
  }
  PrefService* pref_service() { return pref_service_.get(); }
  TestChromeBrowserState* browser_state() {
    return chrome_browser_state_.get();
  }
  const std::vector<std::unique_ptr<autofill::AutofillProfile>>& profiles()
      const {
    return profiles_;
  }
  const std::vector<std::unique_ptr<autofill::CreditCard>>& credit_cards()
      const {
    return cards_;
  }

 private:
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles_;
  std::vector<std::unique_ptr<autofill::CreditCard>> cards_;

  base::test::ScopedTaskEnvironment scoped_task_evironment_;
  web::TestWebState web_state_;
  std::unique_ptr<PrefService> pref_service_;
  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<payments::TestPaymentRequest> payment_request_;
};

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_PAYMENT_REQUEST_UNITTEST_BASE_H_
