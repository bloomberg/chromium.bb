// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_instrument.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/payments/payment_request.mojom.h"

namespace payments {

class ServiceWorkerPaymentInstrumentTest : public testing::Test,
                                           public PaymentRequestSpec::Observer {
 public:
  ServiceWorkerPaymentInstrumentTest() {}
  ~ServiceWorkerPaymentInstrumentTest() override {}

 protected:
  void OnSpecUpdated() override {}

  void SetUp() override {
    mojom::PaymentDetailsPtr details = mojom::PaymentDetails::New();
    mojom::PaymentItemPtr total = mojom::PaymentItem::New();
    mojom::PaymentCurrencyAmountPtr amount =
        mojom::PaymentCurrencyAmount::New();
    amount->value = "5.00";
    amount->currency = "USD";
    total->amount = std::move(amount);
    details->total = std::move(total);

    mojom::PaymentDetailsModifierPtr modifier_1 =
        mojom::PaymentDetailsModifier::New();
    modifier_1->total = mojom::PaymentItem::New();
    modifier_1->total->amount = mojom::PaymentCurrencyAmount::New();
    modifier_1->total->amount->currency = "USD";
    modifier_1->total->amount->value = "4.00";
    modifier_1->method_data = mojom::PaymentMethodData::New();
    modifier_1->method_data->supported_methods = {"basic-card"};
    details->modifiers.push_back(std::move(modifier_1));

    mojom::PaymentDetailsModifierPtr modifier_2 =
        mojom::PaymentDetailsModifier::New();
    modifier_2->total = mojom::PaymentItem::New();
    modifier_2->total->amount = mojom::PaymentCurrencyAmount::New();
    modifier_2->total->amount->currency = "USD";
    modifier_2->total->amount->value = "3.00";
    modifier_2->method_data = mojom::PaymentMethodData::New();
    modifier_2->method_data->supported_methods = {"https://bobpay.com"};
    details->modifiers.push_back(std::move(modifier_2));

    std::vector<mojom::PaymentMethodDataPtr> method_data;
    mojom::PaymentMethodDataPtr entry = mojom::PaymentMethodData::New();
    entry->supported_methods.push_back("basic-card");
    entry->supported_networks.push_back(mojom::BasicCardNetwork::UNIONPAY);
    entry->supported_networks.push_back(mojom::BasicCardNetwork::JCB);
    entry->supported_networks.push_back(mojom::BasicCardNetwork::VISA);
    method_data.push_back(std::move(entry));

    spec_ = base::MakeUnique<PaymentRequestSpec>(
        mojom::PaymentOptions::New(), std::move(details),
        std::move(method_data), this, "en-US");

    std::unique_ptr<content::StoredPaymentApp> stored_app =
        std::make_unique<content::StoredPaymentApp>();
    stored_app->registration_id = 123456;
    stored_app->scope = GURL("https://bobpay.com");
    stored_app->name = "bobpay";
    stored_app->icon.reset(new SkBitmap());
    stored_app->enabled_methods.emplace_back("basic-card");
    stored_app->user_hint = "Visa 4012 ... 1881";
    stored_app->prefer_related_applications = false;

    instrument_ = std::make_unique<ServiceWorkerPaymentInstrument>(
        &browser_context_, GURL("https://testmerchant.com"),
        GURL("https://testmerchant.com/bobpay"), spec_.get(),
        std::move(stored_app));
  }

  void TearDown() override {}

  ServiceWorkerPaymentInstrument* GetInstrument() { return instrument_.get(); }

  mojom::PaymentRequestEventDataPtr CreatePaymentRequestEventData() {
    return instrument_->CreatePaymentRequestEventData();
  }

 private:
  content::TestBrowserContext browser_context_;

  std::unique_ptr<PaymentRequestSpec> spec_;
  std::unique_ptr<ServiceWorkerPaymentInstrument> instrument_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerPaymentInstrumentTest);
};

// Test instrument info and status are correct.
TEST_F(ServiceWorkerPaymentInstrumentTest, InstrumentInfo) {
  EXPECT_TRUE(GetInstrument()->IsCompleteForPayment());
  EXPECT_TRUE(GetInstrument()->IsExactlyMatchingMerchantRequest());

  EXPECT_EQ(base::UTF16ToUTF8(GetInstrument()->GetLabel()), "bobpay");
  EXPECT_EQ(base::UTF16ToUTF8(GetInstrument()->GetSublabel()),
            "https://bobpay.com/");
  EXPECT_NE(GetInstrument()->icon_image_skia(), nullptr);

  EXPECT_TRUE(
      GetInstrument()->IsValidForModifier({"basic-card"}, {}, {}, false));
  // Note that supported networks and types have not been implemented, so
  // here returns true.
  EXPECT_TRUE(GetInstrument()->IsValidForModifier(
      {"basic-card", "https://bobpay.com"}, {"Mastercard"}, {}, true));
  EXPECT_FALSE(GetInstrument()->IsValidForModifier({"https://bobpay.com"}, {},
                                                   {}, false));
}

// Test payment request event can be correctly constructed for invoking
// InvokePaymentApp.
TEST_F(ServiceWorkerPaymentInstrumentTest, CreatePaymentRequestEventData) {
  mojom::PaymentRequestEventDataPtr event_data =
      CreatePaymentRequestEventData();

  EXPECT_EQ(event_data->top_level_origin.spec(), "https://testmerchant.com/");
  EXPECT_EQ(event_data->payment_request_origin.spec(),
            "https://testmerchant.com/bobpay");

  EXPECT_EQ(event_data->method_data.size(), 1U);
  EXPECT_EQ(event_data->method_data[0]->supported_methods.size(), 1U);
  EXPECT_EQ(event_data->method_data[0]->supported_methods[0], "basic-card");
  EXPECT_EQ(event_data->method_data[0]->supported_networks.size(), 3U);

  EXPECT_EQ(event_data->total->currency, "USD");
  EXPECT_EQ(event_data->total->value, "5.00");

  EXPECT_EQ(event_data->modifiers.size(), 1U);
  EXPECT_EQ(event_data->modifiers[0]->total->amount->value, "4.00");
  EXPECT_EQ(event_data->modifiers[0]->total->amount->currency, "USD");
  EXPECT_EQ(event_data->modifiers[0]->method_data->supported_methods[0],
            "basic-card");
}

}  // namespace payments
