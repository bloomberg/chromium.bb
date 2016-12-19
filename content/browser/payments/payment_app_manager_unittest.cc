// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/macros.h"
#include "components/payments/payment_app.mojom.h"
#include "content/browser/payments/payment_app_content_unittest_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

const char kServiceWorkerPattern[] = "https://example.com/a";
const char kServiceWorkerScript[] = "https://example.com/a/script.js";
const char kUnregisteredServiceWorkerPattern[] =
    "https://example.com/unregistered";

void SetManifestCallback(bool* called,
                         payments::mojom::PaymentAppManifestError* out_error,
                         payments::mojom::PaymentAppManifestError error) {
  *called = true;
  *out_error = error;
}

void GetManifestCallback(bool* called,
                         payments::mojom::PaymentAppManifestPtr* out_manifest,
                         payments::mojom::PaymentAppManifestError* out_error,
                         payments::mojom::PaymentAppManifestPtr manifest,
                         payments::mojom::PaymentAppManifestError error) {
  *called = true;
  *out_manifest = std::move(manifest);
  *out_error = error;
}

payments::mojom::PaymentAppManifestPtr CreatePaymentAppManifestForTest() {
  payments::mojom::PaymentAppOptionPtr option =
      payments::mojom::PaymentAppOption::New();
  option->name = "Visa ****";
  option->id = "payment-app-id";
  option->icon = std::string("payment-app-icon");
  option->enabled_methods.push_back("visa");

  payments::mojom::PaymentAppManifestPtr manifest =
      payments::mojom::PaymentAppManifest::New();
  manifest->icon = std::string("payment-app-icon");
  manifest->name = "Payment App";
  manifest->options.push_back(std::move(option));

  return manifest;
}

}  // namespace

class PaymentAppManagerTest : public PaymentAppContentUnitTestBase {
 public:
  PaymentAppManagerTest() {
    manager_ = CreatePaymentAppManager(GURL(kServiceWorkerPattern),
                                       GURL(kServiceWorkerScript));
    EXPECT_NE(manager_, nullptr);
  }

  PaymentAppManager* payment_app_manager() const { return manager_; }

 private:
  // Owned by payment_app_context_.
  PaymentAppManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppManagerTest);
};

TEST_F(PaymentAppManagerTest, SetAndGetManifest) {
  bool called = false;
  payments::mojom::PaymentAppManifestError error = payments::mojom::
      PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED;
  SetManifest(payment_app_manager(), kServiceWorkerPattern,
              CreatePaymentAppManifestForTest(),
              base::Bind(&SetManifestCallback, &called, &error));
  ASSERT_TRUE(called);

  ASSERT_EQ(error, payments::mojom::PaymentAppManifestError::NONE);

  called = false;
  payments::mojom::PaymentAppManifestPtr read_manifest;
  payments::mojom::PaymentAppManifestError read_error = payments::mojom::
      PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED;
  GetManifest(
      payment_app_manager(), kServiceWorkerPattern,
      base::Bind(&GetManifestCallback, &called, &read_manifest, &read_error));
  ASSERT_TRUE(called);

  ASSERT_EQ(read_error, payments::mojom::PaymentAppManifestError::NONE);
  EXPECT_EQ(read_manifest->icon.value(), "payment-app-icon");
  EXPECT_EQ(read_manifest->name, "Payment App");
  ASSERT_EQ(read_manifest->options.size(), 1U);
  EXPECT_EQ(read_manifest->options[0]->icon.value(), "payment-app-icon");
  EXPECT_EQ(read_manifest->options[0]->name, "Visa ****");
  EXPECT_EQ(read_manifest->options[0]->id, "payment-app-id");
  ASSERT_EQ(read_manifest->options[0]->enabled_methods.size(), 1U);
  EXPECT_EQ(read_manifest->options[0]->enabled_methods[0], "visa");
}

TEST_F(PaymentAppManagerTest, SetManifestWithoutAssociatedServiceWorker) {
  bool called = false;
  payments::mojom::PaymentAppManifestError error =
      payments::mojom::PaymentAppManifestError::NONE;
  SetManifest(payment_app_manager(), kUnregisteredServiceWorkerPattern,
              CreatePaymentAppManifestForTest(),
              base::Bind(&SetManifestCallback, &called, &error));
  ASSERT_TRUE(called);

  EXPECT_EQ(error, payments::mojom::PaymentAppManifestError::NO_ACTIVE_WORKER);
}

TEST_F(PaymentAppManagerTest, GetManifestWithoutAssociatedServiceWorker) {
  bool called = false;
  payments::mojom::PaymentAppManifestPtr read_manifest;
  payments::mojom::PaymentAppManifestError read_error =
      payments::mojom::PaymentAppManifestError::NONE;
  GetManifest(
      payment_app_manager(), kUnregisteredServiceWorkerPattern,
      base::Bind(&GetManifestCallback, &called, &read_manifest, &read_error));
  ASSERT_TRUE(called);

  EXPECT_EQ(read_error,
            payments::mojom::PaymentAppManifestError::NO_ACTIVE_WORKER);
}

TEST_F(PaymentAppManagerTest, GetManifestWithNoSavedManifest) {
  bool called = false;
  payments::mojom::PaymentAppManifestPtr read_manifest;
  payments::mojom::PaymentAppManifestError read_error =
      payments::mojom::PaymentAppManifestError::NONE;
  GetManifest(
      payment_app_manager(), kServiceWorkerPattern,
      base::Bind(&GetManifestCallback, &called, &read_manifest, &read_error));
  ASSERT_TRUE(called);

  EXPECT_EQ(read_error, payments::mojom::PaymentAppManifestError::
                            MANIFEST_STORAGE_OPERATION_FAILED);
}

}  // namespace content
