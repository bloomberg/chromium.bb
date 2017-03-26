// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_spec.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_request.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequestSpecTest : public testing::Test,
                               public PaymentRequestSpec::Observer {
 protected:
  ~PaymentRequestSpecTest() override {}

  void OnInvalidSpecProvided() override {
    on_invalid_spec_provided_called_ = true;
  }

  void RecreateSpecWithMethodData(
      std::vector<payments::mojom::PaymentMethodDataPtr> method_data) {
    spec_ = base::MakeUnique<PaymentRequestSpec>(
        nullptr, nullptr, std::move(method_data), this, "en-US");
  }

  PaymentRequestSpec* spec() { return spec_.get(); }
  bool on_invalid_spec_provided_called() {
    return on_invalid_spec_provided_called_;
  }

 private:
  std::unique_ptr<PaymentRequestSpec> spec_;
  bool on_invalid_spec_provided_called_ = false;
};

// Test that empty method data notifies observers of an invalid spec.
TEST_F(PaymentRequestSpecTest, EmptyMethodData) {
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  RecreateSpecWithMethodData(std::move(method_data));
  EXPECT_TRUE(on_invalid_spec_provided_called());

  // No supported card networks.
  EXPECT_EQ(0u, spec()->supported_card_networks().size());
}

TEST_F(PaymentRequestSpecTest, IsMethodSupportedThroughBasicCard) {
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("visa");
  entry->supported_methods.push_back("mastercard");
  entry->supported_methods.push_back("invalid");
  entry->supported_methods.push_back("");
  entry->supported_methods.push_back("visa");
  mojom::PaymentMethodDataPtr entry2 = mojom::PaymentMethodData::New();
  entry2->supported_methods.push_back("basic-card");
  entry2->supported_networks.push_back(mojom::BasicCardNetwork::UNIONPAY);
  entry2->supported_networks.push_back(mojom::BasicCardNetwork::JCB);
  entry2->supported_networks.push_back(mojom::BasicCardNetwork::VISA);

  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  method_data.push_back(std::move(entry2));

  RecreateSpecWithMethodData(std::move(method_data));

  // Only unionpay and jcb are supported through basic-card.
  EXPECT_TRUE(spec()->IsMethodSupportedThroughBasicCard("unionpay"));
  EXPECT_TRUE(spec()->IsMethodSupportedThroughBasicCard("jcb"));
  // "visa" is NOT supported through basic card because it was specified
  // directly first in supportedMethods.
  EXPECT_FALSE(spec()->IsMethodSupportedThroughBasicCard("visa"));
  EXPECT_FALSE(spec()->IsMethodSupportedThroughBasicCard("mastercard"));
  EXPECT_FALSE(spec()->IsMethodSupportedThroughBasicCard("diners"));
  EXPECT_FALSE(spec()->IsMethodSupportedThroughBasicCard("garbage"));
}

// Order matters when parsing the supportedMethods and basic card networks.
TEST_F(PaymentRequestSpecTest,
       IsMethodSupportedThroughBasicCard_DifferentOrder) {
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("basic-card");
  entry->supported_networks.push_back(mojom::BasicCardNetwork::UNIONPAY);
  entry->supported_networks.push_back(mojom::BasicCardNetwork::VISA);
  mojom::PaymentMethodDataPtr entry2 = mojom::PaymentMethodData::New();
  entry2->supported_methods.push_back("visa");
  entry2->supported_methods.push_back("unionpay");
  entry2->supported_methods.push_back("jcb");

  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  method_data.push_back(std::move(entry2));

  RecreateSpecWithMethodData(std::move(method_data));

  // unionpay and visa are supported through basic-card; they were specified
  // first as basic card networks.
  EXPECT_TRUE(spec()->IsMethodSupportedThroughBasicCard("unionpay"));
  EXPECT_TRUE(spec()->IsMethodSupportedThroughBasicCard("visa"));
  // "jcb" is NOT supported through basic card; it was specified directly
  // as a supportedMethods
  EXPECT_FALSE(spec()->IsMethodSupportedThroughBasicCard("jcb"));
}

// Test that parsing supported methods (with invalid values and duplicates)
// works as expected.
TEST_F(PaymentRequestSpecTest, SupportedMethods) {
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("visa");
  entry->supported_methods.push_back("mastercard");
  entry->supported_methods.push_back("invalid");
  entry->supported_methods.push_back("");
  entry->supported_methods.push_back("visa");
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));

  RecreateSpecWithMethodData(std::move(method_data));
  EXPECT_FALSE(on_invalid_spec_provided_called());

  // Only "visa" and "mastercard" remain, in order.
  EXPECT_EQ(2u, spec()->supported_card_networks().size());
  EXPECT_EQ("visa", spec()->supported_card_networks()[0]);
  EXPECT_EQ("mastercard", spec()->supported_card_networks()[1]);
}

// Test that parsing supported methods in different method data entries (with
// invalid values and duplicates) works as expected.
TEST_F(PaymentRequestSpecTest, SupportedMethods_MultipleEntries) {
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("visa");
  mojom::PaymentMethodDataPtr entry2 = mojom::PaymentMethodData::New();
  entry2->supported_methods.push_back("mastercard");
  mojom::PaymentMethodDataPtr entry3 = mojom::PaymentMethodData::New();
  entry3->supported_methods.push_back("invalid");

  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  method_data.push_back(std::move(entry2));
  method_data.push_back(std::move(entry3));

  RecreateSpecWithMethodData(std::move(method_data));
  EXPECT_FALSE(on_invalid_spec_provided_called());

  // Only "visa" and "mastercard" remain, in order.
  EXPECT_EQ(2u, spec()->supported_card_networks().size());
  EXPECT_EQ("visa", spec()->supported_card_networks()[0]);
  EXPECT_EQ("mastercard", spec()->supported_card_networks()[1]);
}

// Test that parsing supported methods in different method data entries fails as
// soon as one entry doesn't specify anything in supported_methods.
TEST_F(PaymentRequestSpecTest, SupportedMethods_MultipleEntries_OneEmpty) {
  // First entry is valid.
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("visa");
  // Empty method data entry.
  mojom::PaymentMethodDataPtr entry2 = mojom::PaymentMethodData::New();
  // Valid one follows the empty.
  mojom::PaymentMethodDataPtr entry3 = mojom::PaymentMethodData::New();
  entry3->supported_methods.push_back("mastercard");

  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  method_data.push_back(std::move(entry2));
  method_data.push_back(std::move(entry3));

  RecreateSpecWithMethodData(std::move(method_data));
  EXPECT_TRUE(on_invalid_spec_provided_called());

  // Visa was parsed, but not mastercard.
  EXPECT_EQ(1u, spec()->supported_card_networks().size());
  EXPECT_EQ("visa", spec()->supported_card_networks()[0]);
}

// Test that only specifying basic-card means that all are supported.
TEST_F(PaymentRequestSpecTest, SupportedMethods_OnlyBasicCard) {
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("basic-card");
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));

  RecreateSpecWithMethodData(std::move(method_data));
  EXPECT_FALSE(on_invalid_spec_provided_called());

  // All of the basic card networks are supported.
  EXPECT_EQ(8u, spec()->supported_card_networks().size());
  EXPECT_EQ("amex", spec()->supported_card_networks()[0]);
  EXPECT_EQ("diners", spec()->supported_card_networks()[1]);
  EXPECT_EQ("discover", spec()->supported_card_networks()[2]);
  EXPECT_EQ("jcb", spec()->supported_card_networks()[3]);
  EXPECT_EQ("mastercard", spec()->supported_card_networks()[4]);
  EXPECT_EQ("mir", spec()->supported_card_networks()[5]);
  EXPECT_EQ("unionpay", spec()->supported_card_networks()[6]);
  EXPECT_EQ("visa", spec()->supported_card_networks()[7]);
}

// Test that specifying a method AND basic-card means that all are supported,
// but with the method as first.
TEST_F(PaymentRequestSpecTest, SupportedMethods_BasicCard_WithSpecificMethod) {
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("jcb");
  entry->supported_methods.push_back("basic-card");
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));

  RecreateSpecWithMethodData(std::move(method_data));
  EXPECT_FALSE(on_invalid_spec_provided_called());

  // All of the basic card networks are supported, but JCB is first because it
  // was specified first.
  EXPECT_EQ(8u, spec()->supported_card_networks().size());
  EXPECT_EQ("jcb", spec()->supported_card_networks()[0]);
  EXPECT_EQ("amex", spec()->supported_card_networks()[1]);
  EXPECT_EQ("diners", spec()->supported_card_networks()[2]);
  EXPECT_EQ("discover", spec()->supported_card_networks()[3]);
  EXPECT_EQ("mastercard", spec()->supported_card_networks()[4]);
  EXPECT_EQ("mir", spec()->supported_card_networks()[5]);
  EXPECT_EQ("unionpay", spec()->supported_card_networks()[6]);
  EXPECT_EQ("visa", spec()->supported_card_networks()[7]);
}

// Test that specifying basic-card with a supported network (with previous
// supported methods) will work as expected
TEST_F(PaymentRequestSpecTest, SupportedMethods_BasicCard_Overlap) {
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("mastercard");
  entry->supported_methods.push_back("visa");
  // Visa and mastercard are repeated, but in reverse order.
  mojom::PaymentMethodDataPtr entry2 = mojom::PaymentMethodData::New();
  entry2->supported_methods.push_back("basic-card");
  entry2->supported_networks.push_back(mojom::BasicCardNetwork::VISA);
  entry2->supported_networks.push_back(mojom::BasicCardNetwork::MASTERCARD);
  entry2->supported_networks.push_back(mojom::BasicCardNetwork::UNIONPAY);
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));
  method_data.push_back(std::move(entry2));

  RecreateSpecWithMethodData(std::move(method_data));
  EXPECT_FALSE(on_invalid_spec_provided_called());

  EXPECT_EQ(3u, spec()->supported_card_networks().size());
  EXPECT_EQ("mastercard", spec()->supported_card_networks()[0]);
  EXPECT_EQ("visa", spec()->supported_card_networks()[1]);
  EXPECT_EQ("unionpay", spec()->supported_card_networks()[2]);
}

// Test that specifying basic-card with supported networks after specifying
// some methods
TEST_F(PaymentRequestSpecTest,
       SupportedMethods_BasicCard_WithSupportedNetworks) {
  mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
  entry->supported_methods.push_back("basic-card");
  entry->supported_networks.push_back(mojom::BasicCardNetwork::VISA);
  entry->supported_networks.push_back(mojom::BasicCardNetwork::UNIONPAY);
  std::vector<mojom::PaymentMethodDataPtr> method_data;
  method_data.push_back(std::move(entry));

  RecreateSpecWithMethodData(std::move(method_data));
  EXPECT_FALSE(on_invalid_spec_provided_called());

  // Only the specified networks are supported.
  EXPECT_EQ(2u, spec()->supported_card_networks().size());
  EXPECT_EQ("visa", spec()->supported_card_networks()[0]);
  EXPECT_EQ("unionpay", spec()->supported_card_networks()[1]);
}

}  // namespace payments
