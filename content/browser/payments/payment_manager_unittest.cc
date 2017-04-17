// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "components/payments/mojom/payment_app.mojom.h"
#include "content/browser/payments/payment_app_content_unittest_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

using ::payments::mojom::PaymentAppManifestError;
using ::payments::mojom::PaymentAppManifestPtr;
using ::payments::mojom::PaymentHandlerStatus;
using ::payments::mojom::PaymentInstrument;
using ::payments::mojom::PaymentInstrumentPtr;

const char kServiceWorkerPattern[] = "https://example.com/a";
const char kServiceWorkerScript[] = "https://example.com/a/script.js";

void SetManifestCallback(bool* called,
                         PaymentAppManifestError* out_error,
                         PaymentAppManifestError error) {
  *called = true;
  *out_error = error;
}

void GetManifestCallback(bool* called,
                         PaymentAppManifestPtr* out_manifest,
                         PaymentAppManifestError* out_error,
                         PaymentAppManifestPtr manifest,
                         PaymentAppManifestError error) {
  *called = true;
  *out_manifest = std::move(manifest);
  *out_error = error;
}

void SetPaymentInstrumentCallback(PaymentHandlerStatus* out_status,
                                  PaymentHandlerStatus status) {
  *out_status = status;
}

void GetPaymentInstrumentCallback(PaymentInstrumentPtr* out_instrument,
                                  PaymentHandlerStatus* out_status,
                                  PaymentInstrumentPtr instrument,
                                  PaymentHandlerStatus status) {
  *out_instrument = std::move(instrument);
  *out_status = status;
}

}  // namespace

class PaymentManagerTest : public PaymentAppContentUnitTestBase {
 public:
  PaymentManagerTest() {
    manager_ = CreatePaymentManager(GURL(kServiceWorkerPattern),
                                    GURL(kServiceWorkerScript));
    EXPECT_NE(nullptr, manager_);
  }

  PaymentManager* payment_manager() const { return manager_; }

  void SetPaymentInstrument(const std::string& instrumentKey,
                            PaymentInstrumentPtr instrument,
                            PaymentHandlerStatus* out_status) {
    manager_->SetPaymentInstrument(
        instrumentKey, std::move(instrument),
        base::Bind(&SetPaymentInstrumentCallback, out_status));
    base::RunLoop().RunUntilIdle();
  }

  void GetPaymentInstrument(const std::string& instrumentKey,
                            PaymentInstrumentPtr* out_instrument,
                            PaymentHandlerStatus* out_status) {
    manager_->GetPaymentInstrument(
        instrumentKey,
        base::Bind(&GetPaymentInstrumentCallback, out_instrument, out_status));
    base::RunLoop().RunUntilIdle();
  }

 private:
  // Owned by payment_app_context_.
  PaymentManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(PaymentManagerTest);
};

TEST_F(PaymentManagerTest, SetAndGetManifest) {
  bool called = false;
  PaymentAppManifestError error =
      PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED;
  SetManifest(payment_manager(),
              CreatePaymentAppManifestForTest(kServiceWorkerPattern),
              base::Bind(&SetManifestCallback, &called, &error));
  ASSERT_TRUE(called);

  ASSERT_EQ(PaymentAppManifestError::NONE, error);

  called = false;
  PaymentAppManifestPtr read_manifest;
  PaymentAppManifestError read_error =
      PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED;
  GetManifest(payment_manager(), base::Bind(&GetManifestCallback, &called,
                                            &read_manifest, &read_error));
  ASSERT_TRUE(called);

  ASSERT_EQ(payments::mojom::PaymentAppManifestError::NONE, read_error);
  EXPECT_EQ("payment-app-icon", read_manifest->icon.value());
  EXPECT_EQ(kServiceWorkerPattern, read_manifest->name);
  ASSERT_EQ(1U, read_manifest->options.size());
  EXPECT_EQ("payment-app-icon", read_manifest->options[0]->icon.value());
  EXPECT_EQ("Visa ****", read_manifest->options[0]->name);
  EXPECT_EQ("payment-app-id", read_manifest->options[0]->id);
  ASSERT_EQ(1U, read_manifest->options[0]->enabled_methods.size());
  EXPECT_EQ("visa", read_manifest->options[0]->enabled_methods[0]);
}

TEST_F(PaymentManagerTest, SetManifestWithoutAssociatedServiceWorker) {
  bool called = false;
  PaymentAppManifestError error = PaymentAppManifestError::NONE;
  UnregisterServiceWorker(GURL(kServiceWorkerPattern));
  SetManifest(payment_manager(),
              CreatePaymentAppManifestForTest(kServiceWorkerPattern),
              base::Bind(&SetManifestCallback, &called, &error));
  ASSERT_TRUE(called);

  EXPECT_EQ(PaymentAppManifestError::NO_ACTIVE_WORKER, error);
}

TEST_F(PaymentManagerTest, GetManifestWithoutAssociatedServiceWorker) {
  bool called = false;
  PaymentAppManifestPtr read_manifest;
  PaymentAppManifestError read_error = PaymentAppManifestError::NONE;
  UnregisterServiceWorker(GURL(kServiceWorkerPattern));
  GetManifest(payment_manager(), base::Bind(&GetManifestCallback, &called,
                                            &read_manifest, &read_error));
  ASSERT_TRUE(called);

  EXPECT_EQ(PaymentAppManifestError::NO_ACTIVE_WORKER, read_error);
}

TEST_F(PaymentManagerTest, GetManifestWithNoSavedManifest) {
  bool called = false;
  PaymentAppManifestPtr read_manifest;
  PaymentAppManifestError read_error = PaymentAppManifestError::NONE;
  GetManifest(payment_manager(), base::Bind(&GetManifestCallback, &called,
                                            &read_manifest, &read_error));
  ASSERT_TRUE(called);

  EXPECT_EQ(PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED,
            read_error);
}

TEST_F(PaymentManagerTest, SetAndGetPaymentInstrument) {
  PaymentHandlerStatus write_status = PaymentHandlerStatus::NOT_FOUND;
  PaymentInstrumentPtr write_details = PaymentInstrument::New();
  write_details->name = "Visa ending ****4756",
  write_details->enabled_methods.push_back("visa");
  write_details->stringified_capabilities = "{}";
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, write_status);
  SetPaymentInstrument("test_key", std::move(write_details), &write_status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, write_status);

  PaymentHandlerStatus read_status = PaymentHandlerStatus::NOT_FOUND;
  PaymentInstrumentPtr read_details;
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, read_status);
  GetPaymentInstrument("test_key", &read_details, &read_status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, read_status);
  EXPECT_EQ("Visa ending ****4756", read_details->name);
  ASSERT_EQ(1U, read_details->enabled_methods.size());
  EXPECT_EQ("visa", read_details->enabled_methods[0]);
  EXPECT_EQ("{}", read_details->stringified_capabilities);
}

TEST_F(PaymentManagerTest, GetUnstoredPaymentInstrument) {
  PaymentHandlerStatus read_status = PaymentHandlerStatus::SUCCESS;
  PaymentInstrumentPtr read_details;
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, read_status);
  GetPaymentInstrument("test_key", &read_details, &read_status);
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, read_status);
}

}  // namespace content
