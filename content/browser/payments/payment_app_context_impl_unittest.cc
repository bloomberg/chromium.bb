// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "components/payments/payment_app.mojom.h"
#include "content/browser/payments/payment_app_content_unittest_base.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/public/browser/payment_app_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class PaymentAppManager;

namespace {

void SetManifestCallback(bool* called,
                         payments::mojom::PaymentAppManifestError* out_error,
                         payments::mojom::PaymentAppManifestError error) {
  *called = true;
  *out_error = error;
}

void GetAllManifestsCallback(bool* called,
                             PaymentAppContext::Manifests* out_manifests,
                             PaymentAppContext::Manifests manifests) {
  *called = true;
  *out_manifests = std::move(manifests);
}

}  // namespace

class PaymentAppContextTest : public PaymentAppContentUnitTestBase {
 public:
  PaymentAppContextTest() {}
  ~PaymentAppContextTest() override {}

  void GetAllManifests(PaymentAppContext::GetAllManifestsCallback callback) {
    payment_app_context()->GetAllManifests(callback);
    base::RunLoop().RunUntilIdle();
  }

  void CreatePaymentApp(const GURL& scope_url, const GURL& sw_script_url) {
    PaymentAppManager* manager =
        CreatePaymentAppManager(scope_url, sw_script_url);

    payments::mojom::PaymentAppOptionPtr option =
        payments::mojom::PaymentAppOption::New();
    option->name = "Visa ****";
    option->id = "payment-app-id";
    option->icon = std::string("payment-app-icon");
    option->enabled_methods.push_back("visa");

    payments::mojom::PaymentAppManifestPtr manifest =
        payments::mojom::PaymentAppManifest::New();
    manifest->icon = std::string("payment-app-icon");
    manifest->name = scope_url.spec();
    manifest->options.push_back(std::move(option));

    payments::mojom::PaymentAppManifestError error = payments::mojom::
        PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED;
    bool called = false;
    SetManifest(manager, scope_url.spec(), std::move(manifest),
                base::Bind(&SetManifestCallback, &called, &error));
    ASSERT_TRUE(called);

    ASSERT_EQ(error, payments::mojom::PaymentAppManifestError::NONE);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentAppContextTest);
};

TEST_F(PaymentAppContextTest, Test) {
  static const struct {
    const char* scopeUrl;
    const char* scriptUrl;
  } kPaymentAppInfo[] = {
      {"https://example.com/a", "https://example.com/a/script.js"},
      {"https://example.com/b", "https://example.com/b/script.js"},
      {"https://example.com/c", "https://example.com/c/script.js"}};

  for (size_t i = 0; i < arraysize(kPaymentAppInfo); i++) {
    CreatePaymentApp(GURL(kPaymentAppInfo[i].scopeUrl),
                     GURL(kPaymentAppInfo[i].scriptUrl));
  }

  PaymentAppContext::Manifests manifests;
  bool called = false;
  GetAllManifests(base::Bind(&GetAllManifestsCallback, &called, &manifests));
  ASSERT_TRUE(called);

  ASSERT_EQ(manifests.size(), 3U);
  size_t i = 0;
  for (const auto& manifest : manifests) {
    EXPECT_EQ(manifest.second->icon.value(), "payment-app-icon");
    EXPECT_EQ(manifest.second->name, kPaymentAppInfo[i++].scopeUrl);
    ASSERT_EQ(manifest.second->options.size(), 1U);
    EXPECT_EQ(manifest.second->options[0]->icon.value(), "payment-app-icon");
    EXPECT_EQ(manifest.second->options[0]->name, "Visa ****");
    EXPECT_EQ(manifest.second->options[0]->id, "payment-app-id");
    ASSERT_EQ(manifest.second->options[0]->enabled_methods.size(), 1U);
    EXPECT_EQ(manifest.second->options[0]->enabled_methods[0], "visa");
  }
}

}  // namespace content
