// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/macros.h"
#include "components/payments/content/payment_app.mojom.h"
#include "content/browser/payments/payment_app_content_unittest_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using payments::mojom::PaymentAppManifestError;
using payments::mojom::PaymentAppManifestPtr;

namespace content {
namespace {

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

}  // namespace

class PaymentAppManagerTest : public PaymentAppContentUnitTestBase {
 public:
  PaymentAppManagerTest() {
    manager_ = CreatePaymentAppManager(GURL(kServiceWorkerPattern),
                                       GURL(kServiceWorkerScript));
    EXPECT_NE(nullptr, manager_);
  }

  PaymentAppManager* payment_app_manager() const { return manager_; }

 private:
  // Owned by payment_app_context_.
  PaymentAppManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppManagerTest);
};

TEST_F(PaymentAppManagerTest, SetAndGetManifest) {
  bool called = false;
  PaymentAppManifestError error =
      PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED;
  SetManifest(payment_app_manager(),
              CreatePaymentAppManifestForTest(kServiceWorkerPattern),
              base::Bind(&SetManifestCallback, &called, &error));
  ASSERT_TRUE(called);

  ASSERT_EQ(PaymentAppManifestError::NONE, error);

  called = false;
  PaymentAppManifestPtr read_manifest;
  PaymentAppManifestError read_error =
      PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED;
  GetManifest(payment_app_manager(), base::Bind(&GetManifestCallback, &called,
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

TEST_F(PaymentAppManagerTest, SetManifestWithoutAssociatedServiceWorker) {
  bool called = false;
  PaymentAppManifestError error = PaymentAppManifestError::NONE;
  UnregisterServiceWorker(GURL(kServiceWorkerPattern));
  SetManifest(payment_app_manager(),
              CreatePaymentAppManifestForTest(kServiceWorkerPattern),
              base::Bind(&SetManifestCallback, &called, &error));
  ASSERT_TRUE(called);

  EXPECT_EQ(PaymentAppManifestError::NO_ACTIVE_WORKER, error);
}

TEST_F(PaymentAppManagerTest, GetManifestWithoutAssociatedServiceWorker) {
  bool called = false;
  PaymentAppManifestPtr read_manifest;
  PaymentAppManifestError read_error = PaymentAppManifestError::NONE;
  UnregisterServiceWorker(GURL(kServiceWorkerPattern));
  GetManifest(payment_app_manager(), base::Bind(&GetManifestCallback, &called,
                                                &read_manifest, &read_error));
  ASSERT_TRUE(called);

  EXPECT_EQ(PaymentAppManifestError::NO_ACTIVE_WORKER, read_error);
}

TEST_F(PaymentAppManagerTest, GetManifestWithNoSavedManifest) {
  bool called = false;
  PaymentAppManifestPtr read_manifest;
  PaymentAppManifestError read_error = PaymentAppManifestError::NONE;
  GetManifest(payment_app_manager(), base::Bind(&GetManifestCallback, &called,
                                                &read_manifest, &read_error));
  ASSERT_TRUE(called);

  EXPECT_EQ(PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED,
            read_error);
}

}  // namespace content
