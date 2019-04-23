// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_util.h"

#include "base/guid.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace payments {

class PaymentsUtilTest : public testing::Test {
 public:
  PaymentsUtilTest() {}
  ~PaymentsUtilTest() override {}

  CreditCard GetCreditCardWithSpecifiedCardNumber(const char* card_number) {
    CreditCard credit_card(base::GenerateGUID(), /*origin=*/"");
    test::SetCreditCardInfo(&credit_card, "Test User", card_number, "11",
                            test::NextYear().c_str(), "1");
    return credit_card;
  }

 protected:
  TestPersonalDataManager personal_data_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentsUtilTest);
};

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_Normal) {
  base::HistogramTester histogram_tester;

  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  EXPECT_EQ(123456, GetBillingCustomerId(&personal_data_manager_,
                                         /*should_log_validity=*/true));

  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsCustomerDataBillingIdStatus",
      AutofillMetrics::BillingIdStatus::VALID, 1);
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_Garbage) {
  base::HistogramTester histogram_tester;

  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"garbage"));

  EXPECT_EQ(0, GetBillingCustomerId(&personal_data_manager_,
                                    /*should_log_validity=*/true));

  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsCustomerDataBillingIdStatus",
      AutofillMetrics::BillingIdStatus::PARSE_ERROR, 1);
}

TEST_F(PaymentsUtilTest, GetBillingCustomerId_PaymentsCustomerData_NoData) {
  base::HistogramTester histogram_tester;

  // Explictly do not set PaymentsCustomerData. Nothing crashes and the returned
  // customer ID is 0.
  EXPECT_EQ(0, GetBillingCustomerId(&personal_data_manager_,
                                    /*should_log_validity=*/true));
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsCustomerDataBillingIdStatus",
      AutofillMetrics::BillingIdStatus::MISSING, 1);
}

TEST_F(PaymentsUtilTest, HasGooglePaymentsAccount_Normal) {
  personal_data_manager_.SetPaymentsCustomerData(
      std::make_unique<PaymentsCustomerData>(/*customer_id=*/"123456"));

  EXPECT_TRUE(HasGooglePaymentsAccount(&personal_data_manager_));
}

TEST_F(PaymentsUtilTest, HasGooglePaymentsAccount_NoData) {
  // Explicitly do not set Prefs data. Nothing crashes and returns false.
  EXPECT_FALSE(HasGooglePaymentsAccount(&personal_data_manager_));
}

TEST_F(PaymentsUtilTest, IsCreditCardSupported_EmptyBin) {
  // Create empty supported card bin ranges.
  std::vector<std::pair<int, int>> supported_card_bin_ranges;
  CreditCard credit_card =
      GetCreditCardWithSpecifiedCardNumber("4111111111111111");
  // Credit card is not supported since the supported bin range is empty.
  EXPECT_FALSE(IsCreditCardSupported(credit_card, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardSupported_SameStartAndEnd) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(411111, 411111)};
  CreditCard credit_card =
      GetCreditCardWithSpecifiedCardNumber("4111111111111111");
  // Credit card is supported since the card number is within the range of the
  // same start and end.
  EXPECT_TRUE(IsCreditCardSupported(credit_card, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardSupported_InsideRange) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(411110, 411112)};
  CreditCard credit_card =
      GetCreditCardWithSpecifiedCardNumber("4111111111111111");
  // Credit card is supported since the card number is inside the range.
  EXPECT_TRUE(IsCreditCardSupported(credit_card, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardSupported_StartBoundary) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(411111, 422222)};
  CreditCard credit_card =
      GetCreditCardWithSpecifiedCardNumber("4111111111111111");
  // Credit card is supported since the card number is at the start boundary.
  EXPECT_TRUE(IsCreditCardSupported(credit_card, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardSupported_EndBoundary) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(410000, 411111)};
  CreditCard credit_card =
      GetCreditCardWithSpecifiedCardNumber("4111111111111111");
  // Credit card is supported since the card number is at the end boundary.
  EXPECT_TRUE(IsCreditCardSupported(credit_card, supported_card_bin_ranges));
}

TEST_F(PaymentsUtilTest, IsCreditCardSupported_OutOfRange) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges{
      std::make_pair(2111, 2111), std::make_pair(412, 413),
      std::make_pair(300, 305)};
  CreditCard credit_card = test::GetCreditCard();
  // Credit card is not supported since the card number is out of any range.
  EXPECT_FALSE(IsCreditCardSupported(credit_card, supported_card_bin_ranges));
}

}  // namespace payments
}  // namespace autofill
