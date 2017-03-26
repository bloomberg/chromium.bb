// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_state.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/content/payment_request.mojom.h"
#include "components/payments/content/payment_request_spec.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestStateTest : public testing::Test,
                                public PaymentRequestState::Observer,
                                public PaymentRequestState::Delegate {
 protected:
  PaymentRequestStateTest()
      : num_on_selected_information_changed_called_(0),
        address_(autofill::test::GetFullProfile()),
        credit_card_visa_(autofill::test::GetCreditCard()),
        credit_card_amex_(autofill::test::GetCreditCard2()) {
    test_personal_data_manager_.AddTestingProfile(&address_);
    credit_card_visa_.set_billing_address_id(address_.guid());
    credit_card_visa_.set_use_count(5u);
    test_personal_data_manager_.AddTestingCreditCard(&credit_card_visa_);
    credit_card_amex_.set_billing_address_id(address_.guid());
    credit_card_amex_.set_use_count(1u);
    test_personal_data_manager_.AddTestingCreditCard(&credit_card_amex_);
    // Add an expired JCB card here.
    credit_card_jcb_ = autofill::test::GetCreditCard();
    credit_card_jcb_.SetNumber(base::ASCIIToUTF16("3530111333300000"));
    credit_card_jcb_.set_billing_address_id(address_.guid());
    credit_card_jcb_.set_use_count(1u);
    credit_card_jcb_.SetExpirationDateFromString(base::ASCIIToUTF16("01/17"));
  }
  ~PaymentRequestStateTest() override {}

  // PaymentRequestState::Observer:
  void OnSelectedInformationChanged() override {
    num_on_selected_information_changed_called_++;
  }

  // PaymentRequestState::Delegate:
  void OnPaymentResponseAvailable(mojom::PaymentResponsePtr response) override {
    payment_response_ = std::move(response);
  };

  void RecreateStateWithOptionsAndDetails(
      mojom::PaymentOptionsPtr options,
      mojom::PaymentDetailsPtr details,
      std::vector<mojom::PaymentMethodDataPtr> method_data) {
    // The spec will be based on the |options| and |details| passed in.
    spec_ = base::MakeUnique<PaymentRequestSpec>(
        std::move(options), std::move(details), std::move(method_data), nullptr,
        "en-US");
    state_ = base::MakeUnique<PaymentRequestState>(
        spec_.get(), this, "en-US", &test_personal_data_manager_);
    state_->AddObserver(this);
  }

  // Convenience method to create a PaymentRequestState with default details
  // (one shipping option) and method data (only supports visa).
  void RecreateStateWithOptions(mojom::PaymentOptionsPtr options) {
    // Create dummy PaymentDetails with a single shipping option.
    std::vector<mojom::PaymentShippingOptionPtr> shipping_options;
    mojom::PaymentShippingOptionPtr option =
        mojom::PaymentShippingOption::New();
    option->id = "option:1";
    shipping_options.push_back(std::move(option));
    mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
    details->shipping_options = std::move(shipping_options);

    RecreateStateWithOptionsAndDetails(std::move(options), std::move(details),
                                       GetMethodDataForVisa());
  }

  // Convenience method that returns MethodData that supports Visa.
  std::vector<mojom::PaymentMethodDataPtr> GetMethodDataForVisa() {
    std::vector<mojom::PaymentMethodDataPtr> method_data;
    mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
    entry->supported_methods.push_back("visa");
    method_data.push_back(std::move(entry));
    return method_data;
  }

  PaymentRequestState* state() { return state_.get(); }
  const mojom::PaymentResponsePtr& response() { return payment_response_; }
  int num_on_selected_information_changed_called() {
    return num_on_selected_information_changed_called_;
  }

  autofill::AutofillProfile* test_address() { return &address_; }

 private:
  std::unique_ptr<PaymentRequestState> state_;
  std::unique_ptr<PaymentRequestSpec> spec_;
  int num_on_selected_information_changed_called_;
  mojom::PaymentResponsePtr payment_response_;
  autofill::TestPersonalDataManager test_personal_data_manager_;

  // Test data.
  autofill::AutofillProfile address_;
  autofill::CreditCard credit_card_visa_;
  autofill::CreditCard credit_card_amex_;
  autofill::CreditCard credit_card_jcb_;
};

TEST_F(PaymentRequestStateTest, CanMakePayment) {
  // Default options.
  RecreateStateWithOptions(mojom::PaymentOptions::New());

  // CanMakePayment returns true because the method data requires Visa, and the
  // user has a Visa card on file.
  EXPECT_TRUE(state()->CanMakePayment());
}

TEST_F(PaymentRequestStateTest, CanMakePayment_CannotMakePayment) {
  // The method data requires MasterCard.
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("mastercard");
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  // CanMakePayment returns false because the method data requires MasterCard,
  // and the user doesn't have such an instrument.
  EXPECT_FALSE(state()->CanMakePayment());
}

TEST_F(PaymentRequestStateTest, CanMakePayment_OnlyBasicCard) {
  // The method data supports everything in basic-card.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("basic-card");
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  // CanMakePayment returns true because the method data supports everything,
  // and the user has at least one instrument.
  EXPECT_TRUE(state()->CanMakePayment());
}

TEST_F(PaymentRequestStateTest, CanMakePayment_BasicCard_SpecificAvailable) {
  // The method data supports visa through basic-card.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("basic-card");
  entry->supported_networks.push_back(mojom::BasicCardNetwork::VISA);
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  // CanMakePayment returns true because the method data supports visa, and the
  // user has a Visa instrument.
  EXPECT_TRUE(state()->CanMakePayment());
}

TEST_F(PaymentRequestStateTest,
       CanMakePayment_BasicCard_SpecificAvailableButInvalid) {
  // The method data supports jcb through basic-card.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("basic-card");
  entry->supported_networks.push_back(mojom::BasicCardNetwork::JCB);
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  // CanMakePayment returns false because the method data supports jcb, and the
  // user has a JCB instrument, but it's invalid.
  EXPECT_FALSE(state()->CanMakePayment());
}

TEST_F(PaymentRequestStateTest, CanMakePayment_BasicCard_SpecificUnavailable) {
  // The method data supports mastercard through basic-card.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("basic-card");
  entry->supported_networks.push_back(mojom::BasicCardNetwork::MASTERCARD);
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  // CanMakePayment returns false because the method data supports mastercard,
  // and the user doesn't have such an instrument.
  EXPECT_FALSE(state()->CanMakePayment());
}

// Test that the last shipping option is selected.
TEST_F(PaymentRequestStateTest, ShippingOptionsSelection) {
  std::vector<mojom::PaymentShippingOptionPtr> shipping_options;
  mojom::PaymentShippingOptionPtr option = mojom::PaymentShippingOption::New();
  option->id = "option:1";
  option->selected = false;
  shipping_options.push_back(std::move(option));
  mojom::PaymentShippingOptionPtr option2 = mojom::PaymentShippingOption::New();
  option2->id = "option:2";
  option2->selected = true;
  shipping_options.push_back(std::move(option2));
  mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
  details->shipping_options = std::move(shipping_options);

  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  RecreateStateWithOptionsAndDetails(std::move(options), std::move(details),
                                     GetMethodDataForVisa());

  EXPECT_EQ("option:2", state()->selected_shipping_option()->id);
}

TEST_F(PaymentRequestStateTest, ReadyToPay_DefaultSelections) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_shipping = true;
  options->request_payer_name = true;
  options->request_payer_phone = true;
  options->request_payer_email = true;
  RecreateStateWithOptions(std::move(options));

  EXPECT_TRUE(state()->is_ready_to_pay());
}

// Testing that only supported intruments are shown. In this test the merchant
// only supports Visa.
TEST_F(PaymentRequestStateTest, UnsupportedCardAreNotAvailable) {
  // Default options.
  RecreateStateWithOptions(mojom::PaymentOptions::New());

  // Ready to pay because the default instrument is selected and supported.
  EXPECT_TRUE(state()->is_ready_to_pay());

  // There's only one instrument available, even though there's an Amex in
  // PersonalDataManager.
  EXPECT_EQ(1u, state()->available_instruments().size());
}

// Test selecting a contact info profile will make the user ready to pay.
TEST_F(PaymentRequestStateTest, ReadyToPay_ContactInfo) {
  mojom::PaymentOptionsPtr options = mojom::PaymentOptions::New();
  options->request_payer_name = true;
  options->request_payer_phone = true;
  options->request_payer_email = true;
  RecreateStateWithOptions(std::move(options));

  // Ready to pay because of default selections.
  EXPECT_TRUE(state()->is_ready_to_pay());

  // Unselecting contact profile.
  state()->SetSelectedContactProfile(nullptr);
  EXPECT_EQ(1, num_on_selected_information_changed_called());

  EXPECT_FALSE(state()->is_ready_to_pay());

  state()->SetSelectedContactProfile(test_address());
  EXPECT_EQ(2, num_on_selected_information_changed_called());

  // Ready to pay!
  EXPECT_TRUE(state()->is_ready_to_pay());
}

// Test generating a PaymentResponse.
TEST_F(PaymentRequestStateTest, GeneratePaymentResponse_SupportedMethod) {
  // Default options (no shipping, no contact info).
  RecreateStateWithOptions(mojom::PaymentOptions::New());
  state()->SetSelectedInstrument(state()->available_instruments()[0].get());
  EXPECT_EQ(1, num_on_selected_information_changed_called());
  EXPECT_TRUE(state()->is_ready_to_pay());

  // TODO(mathp): Currently synchronous, when async will need a RunLoop.
  // "visa" is specified directly in the supportedMethods so it is returned
  // as the method name.
  state()->GeneratePaymentResponse();
  EXPECT_EQ("visa", response()->method_name);
  EXPECT_EQ(
      "{\"billingAddress\":"
      "{\"addressLine\":[\"666 Erebus St.\",\"Apt 8\"],"
      "\"city\":\"Elysium\","
      "\"country\":\"US\","
      "\"organization\":\"Underworld\","
      "\"phone\":\"16502111111\","
      "\"postalCode\":\"91111\","
      "\"recipient\":\"John H. Doe\","
      "\"region\":\"CA\"},"
      "\"cardNumber\":\"4111111111111111\","
      "\"cardSecurityCode\":\"123\","
      "\"cardholderName\":\"Test User\","
      "\"expiryMonth\":\"11\","
      "\"expiryYear\":\"2017\"}",
      response()->stringified_details);
}

// Test generating a PaymentResponse when the method is specified through
// "basic-card".
TEST_F(PaymentRequestStateTest, GeneratePaymentResponse_BasicCard) {
  // The method data supports visa through basic-card.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("basic-card");
  entry->supported_networks.push_back(mojom::BasicCardNetwork::VISA);
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  RecreateStateWithOptionsAndDetails(mojom::PaymentOptions::New(),
                                     mojom::PaymentDetails::New(),
                                     std::move(method_data));

  EXPECT_TRUE(state()->is_ready_to_pay());

  // TODO(mathp): Currently synchronous, when async will need a RunLoop.
  // "basic-card" is specified so it is returned as the method name.
  state()->GeneratePaymentResponse();
  EXPECT_EQ("basic-card", response()->method_name);
  EXPECT_EQ(
      "{\"billingAddress\":"
      "{\"addressLine\":[\"666 Erebus St.\",\"Apt 8\"],"
      "\"city\":\"Elysium\","
      "\"country\":\"US\","
      "\"organization\":\"Underworld\","
      "\"phone\":\"16502111111\","
      "\"postalCode\":\"91111\","
      "\"recipient\":\"John H. Doe\","
      "\"region\":\"CA\"},"
      "\"cardNumber\":\"4111111111111111\","
      "\"cardSecurityCode\":\"123\","
      "\"cardholderName\":\"Test User\","
      "\"expiryMonth\":\"11\","
      "\"expiryYear\":\"2017\"}",
      response()->stringified_details);
}

}  // namespace payments
