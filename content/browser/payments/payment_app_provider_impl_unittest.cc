// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/payments/payment_app_content_unittest_base.h"
#include "content/browser/payments/payment_app_provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/payments/payment_app.mojom.h"
#include "url/gurl.h"

namespace content {

class PaymentManager;

namespace {

using ::payments::mojom::PaymentHandlerStatus;
using ::payments::mojom::PaymentInstrument;
using ::payments::mojom::PaymentInstrumentPtr;

void SetPaymentInstrumentCallback(PaymentHandlerStatus* out_status,
                                  PaymentHandlerStatus status) {
  *out_status = status;
}

void GetAllPaymentAppsCallback(PaymentAppProvider::PaymentApps* out_apps,
                               PaymentAppProvider::PaymentApps apps) {
  *out_apps = std::move(apps);
}

void InvokePaymentAppCallback(
    bool* called,
    payments::mojom::PaymentHandlerResponsePtr response) {
  *called = true;
}

void PaymentEventResultCallback(base::OnceClosure callback,
                                bool* out_payment_event_result,
                                bool payment_event_result) {
  *out_payment_event_result = payment_event_result;
  std::move(callback).Run();
}

}  // namespace

class PaymentAppProviderTest : public PaymentAppContentUnitTestBase {
 public:
  PaymentAppProviderTest() {}
  ~PaymentAppProviderTest() override {}

  void SetPaymentInstrument(
      PaymentManager* manager,
      const std::string& instrument_key,
      PaymentInstrumentPtr instrument,
      PaymentManager::SetPaymentInstrumentCallback callback) {
    ASSERT_NE(nullptr, manager);
    manager->SetPaymentInstrument(instrument_key, std::move(instrument),
                                  std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  void GetAllPaymentApps(
      PaymentAppProvider::GetAllPaymentAppsCallback callback) {
    PaymentAppProviderImpl::GetInstance()->GetAllPaymentApps(
        browser_context(), std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  void InvokePaymentApp(int64_t registration_id,
                        payments::mojom::PaymentRequestEventDataPtr event_data,
                        PaymentAppProvider::InvokePaymentAppCallback callback) {
    PaymentAppProviderImpl::GetInstance()->InvokePaymentApp(
        browser_context(), registration_id, std::move(event_data),
        std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  void CanMakePayment(int64_t registration_id,
                      payments::mojom::CanMakePaymentEventDataPtr event_data,
                      PaymentAppProvider::PaymentEventResultCallback callback) {
    PaymentAppProviderImpl::GetInstance()->CanMakePayment(
        browser_context(), registration_id, std::move(event_data),
        std::move(callback));
  }

  void AbortPayment(int64_t registration_id,
                    PaymentAppProvider::PaymentEventResultCallback callback) {
    PaymentAppProviderImpl::GetInstance()->AbortPayment(
        browser_context(), registration_id, std::move(callback));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentAppProviderTest);
};

TEST_F(PaymentAppProviderTest, AbortPaymentTest) {
  PaymentManager* manager = CreatePaymentManager(
      GURL("https://example.com"), GURL("https://example.com/script.js"));

  PaymentHandlerStatus status;
  SetPaymentInstrument(manager, "payment_instrument_key",
                       payments::mojom::PaymentInstrument::New(),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));

  PaymentAppProvider::PaymentApps apps;
  GetAllPaymentApps(base::BindOnce(&GetAllPaymentAppsCallback, &apps));
  ASSERT_EQ(1U, apps.size());

  bool payment_aborted = false;
  base::RunLoop loop;
  AbortPayment(last_sw_registration_id(),
               base::BindOnce(&PaymentEventResultCallback, loop.QuitClosure(),
                              &payment_aborted));
  loop.Run();
  ASSERT_TRUE(payment_aborted);
}

TEST_F(PaymentAppProviderTest, CanMakePaymentTest) {
  PaymentManager* manager = CreatePaymentManager(
      GURL("https://example.com"), GURL("https://example.com/script.js"));

  PaymentHandlerStatus status;
  SetPaymentInstrument(manager, "payment_instrument_key",
                       payments::mojom::PaymentInstrument::New(),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));

  PaymentAppProvider::PaymentApps apps;
  GetAllPaymentApps(base::BindOnce(&GetAllPaymentAppsCallback, &apps));
  ASSERT_EQ(1U, apps.size());

  payments::mojom::CanMakePaymentEventDataPtr event_data =
      payments::mojom::CanMakePaymentEventData::New();
  payments::mojom::PaymentMethodDataPtr methodData =
      payments::mojom::PaymentMethodData::New();
  methodData->supported_methods.push_back("test-method");
  event_data->method_data.push_back(std::move(methodData));

  bool can_make_payment = false;
  base::RunLoop loop;
  CanMakePayment(last_sw_registration_id(), std::move(event_data),
                 base::BindOnce(&PaymentEventResultCallback, loop.QuitClosure(),
                                &can_make_payment));
  loop.Run();
  ASSERT_TRUE(can_make_payment);
}

TEST_F(PaymentAppProviderTest, InvokePaymentAppTest) {
  PaymentManager* manager1 = CreatePaymentManager(
      GURL("https://hellopay.com/a"), GURL("https://hellopay.com/a/script.js"));
  PaymentManager* manager2 = CreatePaymentManager(
      GURL("https://bobpay.com/b"), GURL("https://bobpay.com/b/script.js"));

  PaymentHandlerStatus status;
  SetPaymentInstrument(manager1, "test_key1",
                       payments::mojom::PaymentInstrument::New(),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));
  SetPaymentInstrument(manager2, "test_key2",
                       payments::mojom::PaymentInstrument::New(),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));
  SetPaymentInstrument(manager2, "test_key3",
                       payments::mojom::PaymentInstrument::New(),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));

  PaymentAppProvider::PaymentApps apps;
  GetAllPaymentApps(base::BindOnce(&GetAllPaymentAppsCallback, &apps));
  ASSERT_EQ(2U, apps.size());

  int64_t bobpay_registration_id = last_sw_registration_id();
  EXPECT_EQ(apps[bobpay_registration_id]->scope.spec(), "https://bobpay.com/b");

  payments::mojom::PaymentRequestEventDataPtr event_data =
      payments::mojom::PaymentRequestEventData::New();
  event_data->method_data.push_back(payments::mojom::PaymentMethodData::New());
  event_data->total = payments::mojom::PaymentCurrencyAmount::New();

  bool called = false;
  InvokePaymentApp(bobpay_registration_id, std::move(event_data),
                   base::BindOnce(&InvokePaymentAppCallback, &called));
  ASSERT_TRUE(called);
}

TEST_F(PaymentAppProviderTest, GetAllPaymentAppsTest) {
  PaymentManager* manager1 = CreatePaymentManager(
      GURL("https://hellopay.com/a"), GURL("https://hellopay.com/a/script.js"));
  int64_t hellopay_registration_id = last_sw_registration_id();

  PaymentManager* manager2 = CreatePaymentManager(
      GURL("https://bobpay.com/b"), GURL("https://bobpay.com/b/script.js"));
  int64_t bobpay_registration_id = last_sw_registration_id();

  PaymentHandlerStatus status;
  PaymentInstrumentPtr instrument_1 = PaymentInstrument::New();
  instrument_1->enabled_methods.push_back("hellopay");
  SetPaymentInstrument(manager1, "test_key1", std::move(instrument_1),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));

  PaymentInstrumentPtr instrument_2 = PaymentInstrument::New();
  instrument_2->enabled_methods.push_back("hellopay");
  SetPaymentInstrument(manager2, "test_key2", std::move(instrument_2),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));

  PaymentInstrumentPtr instrument_3 = PaymentInstrument::New();
  instrument_3->enabled_methods.push_back("bobpay");
  SetPaymentInstrument(manager2, "test_key3", std::move(instrument_3),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));

  PaymentAppProvider::PaymentApps apps;
  GetAllPaymentApps(base::BindOnce(&GetAllPaymentAppsCallback, &apps));

  ASSERT_EQ(2U, apps.size());
  ASSERT_EQ(1U, apps[hellopay_registration_id]->enabled_methods.size());
  ASSERT_EQ(2U, apps[bobpay_registration_id]->enabled_methods.size());
}

TEST_F(PaymentAppProviderTest, GetAllPaymentAppsFromTheSameOriginTest) {
  PaymentManager* manager1 = CreatePaymentManager(
      GURL("https://bobpay.com/a"), GURL("https://bobpay.com/a/script.js"));
  int64_t bobpay_a_registration_id = last_sw_registration_id();

  PaymentManager* manager2 = CreatePaymentManager(
      GURL("https://bobpay.com/b"), GURL("https://bobpay.com/b/script.js"));
  int64_t bobpay_b_registration_id = last_sw_registration_id();

  PaymentHandlerStatus status;
  PaymentInstrumentPtr instrument_1 = PaymentInstrument::New();
  instrument_1->enabled_methods.push_back("hellopay");
  SetPaymentInstrument(manager1, "test_key1", std::move(instrument_1),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));

  PaymentInstrumentPtr instrument_2 = PaymentInstrument::New();
  instrument_2->enabled_methods.push_back("hellopay");
  SetPaymentInstrument(manager2, "test_key2", std::move(instrument_2),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));

  PaymentInstrumentPtr instrument_3 = PaymentInstrument::New();
  instrument_3->enabled_methods.push_back("bobpay");
  SetPaymentInstrument(manager2, "test_key3", std::move(instrument_3),
                       base::BindOnce(&SetPaymentInstrumentCallback, &status));

  PaymentAppProvider::PaymentApps apps;
  GetAllPaymentApps(base::BindOnce(&GetAllPaymentAppsCallback, &apps));

  ASSERT_EQ(2U, apps.size());
  ASSERT_EQ(1U, apps[bobpay_a_registration_id]->enabled_methods.size());
  ASSERT_EQ(2U, apps[bobpay_b_registration_id]->enabled_methods.size());
}

}  // namespace content
