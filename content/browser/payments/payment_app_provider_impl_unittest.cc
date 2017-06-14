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

void InvokePaymentAppCallback(bool* called,
                              payments::mojom::PaymentAppResponsePtr response) {
  *called = true;
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
        browser_context(), registration_id, std::move(event_data), callback);
    base::RunLoop().RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentAppProviderTest);
};

TEST_F(PaymentAppProviderTest, InvokePaymentAppTest) {
  PaymentManager* manager1 = CreatePaymentManager(
      GURL("https://hellopay.com/a"), GURL("https://hellopay.com/a/script.js"));
  PaymentManager* manager2 = CreatePaymentManager(
      GURL("https://bobpay.com/b"), GURL("https://bobpay.com/b/script.js"));

  PaymentHandlerStatus status;
  SetPaymentInstrument(manager1, "test_key1",
                       payments::mojom::PaymentInstrument::New(),
                       base::Bind(&SetPaymentInstrumentCallback, &status));
  SetPaymentInstrument(manager2, "test_key2",
                       payments::mojom::PaymentInstrument::New(),
                       base::Bind(&SetPaymentInstrumentCallback, &status));
  SetPaymentInstrument(manager2, "test_key3",
                       payments::mojom::PaymentInstrument::New(),
                       base::Bind(&SetPaymentInstrumentCallback, &status));

  PaymentAppProvider::PaymentApps apps;
  GetAllPaymentApps(base::Bind(&GetAllPaymentAppsCallback, &apps));
  ASSERT_EQ(2U, apps.size());

  payments::mojom::PaymentRequestEventDataPtr event_data =
      payments::mojom::PaymentRequestEventData::New();
  event_data->method_data.push_back(payments::mojom::PaymentMethodData::New());
  event_data->total = payments::mojom::PaymentItem::New();
  event_data->total->amount = payments::mojom::PaymentCurrencyAmount::New();

  bool called = false;
  InvokePaymentApp(apps[GURL("https://hellopay.com/")][0]->registration_id,
                   std::move(event_data),
                   base::Bind(&InvokePaymentAppCallback, &called));
  ASSERT_TRUE(called);

  EXPECT_EQ(apps[GURL("https://hellopay.com/")][0]->registration_id,
            last_sw_registration_id());
}

TEST_F(PaymentAppProviderTest, GetAllPaymentAppsTest) {
  PaymentManager* manager1 = CreatePaymentManager(
      GURL("https://hellopay.com/a"), GURL("https://hellopay.com/a/script.js"));
  PaymentManager* manager2 = CreatePaymentManager(
      GURL("https://bobpay.com/b"), GURL("https://bobpay.com/b/script.js"));

  PaymentHandlerStatus status;
  SetPaymentInstrument(manager1, "test_key1", PaymentInstrument::New(),
                       base::Bind(&SetPaymentInstrumentCallback, &status));
  SetPaymentInstrument(manager2, "test_key2", PaymentInstrument::New(),
                       base::Bind(&SetPaymentInstrumentCallback, &status));
  SetPaymentInstrument(manager2, "test_key3", PaymentInstrument::New(),
                       base::Bind(&SetPaymentInstrumentCallback, &status));

  PaymentAppProvider::PaymentApps apps;
  GetAllPaymentApps(base::Bind(&GetAllPaymentAppsCallback, &apps));

  ASSERT_EQ(2U, apps.size());
  ASSERT_EQ(1U, apps[GURL("https://hellopay.com/")].size());
  ASSERT_EQ(2U, apps[GURL("https://bobpay.com/")].size());
}

}  // namespace content
