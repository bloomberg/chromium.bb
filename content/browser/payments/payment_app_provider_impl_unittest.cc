// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "components/payments/content/payment_app.mojom.h"
#include "content/browser/payments/payment_app_content_unittest_base.h"
#include "content/browser/payments/payment_app_provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using payments::mojom::PaymentAppManifestError;
using payments::mojom::PaymentAppManifestPtr;

namespace content {

class PaymentAppManager;

namespace {

void SetManifestCallback(bool* called,
                         PaymentAppManifestError* out_error,
                         PaymentAppManifestError error) {
  *called = true;
  *out_error = error;
}

void GetAllManifestsCallback(bool* called,
                             PaymentAppProvider::Manifests* out_manifests,
                             PaymentAppProvider::Manifests manifests) {
  *called = true;
  *out_manifests = std::move(manifests);
}

}  // namespace

class PaymentAppProviderTest : public PaymentAppContentUnitTestBase {
 public:
  PaymentAppProviderTest() {}
  ~PaymentAppProviderTest() override {}

  void GetAllManifests(PaymentAppProvider::GetAllManifestsCallback callback) {
    PaymentAppProviderImpl::GetInstance()->GetAllManifests(browser_context(),
                                                           callback);
    base::RunLoop().RunUntilIdle();
  }

  void InvokePaymentApp(int64_t registration_id,
                        payments::mojom::PaymentAppRequestPtr app_request) {
    PaymentAppProviderImpl::GetInstance()->InvokePaymentApp(
        browser_context(), registration_id, std::move(app_request));
    base::RunLoop().RunUntilIdle();
  }

  void CreatePaymentApp(const GURL& scope_url, const GURL& sw_script_url) {
    PaymentAppManager* manager =
        CreatePaymentAppManager(scope_url, sw_script_url);

    PaymentAppManifestError error =
        PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED;
    bool called = false;
    SetManifest(manager, CreatePaymentAppManifestForTest(scope_url.spec()),
                base::Bind(&SetManifestCallback, &called, &error));
    ASSERT_TRUE(called);

    ASSERT_EQ(PaymentAppManifestError::NONE, error);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentAppProviderTest);
};

TEST_F(PaymentAppProviderTest, GetAllManifestsTest) {
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

  PaymentAppProvider::Manifests manifests;
  bool called = false;
  GetAllManifests(base::Bind(&GetAllManifestsCallback, &called, &manifests));
  ASSERT_TRUE(called);

  ASSERT_EQ(3U, manifests.size());
  size_t i = 0;
  for (const auto& manifest : manifests) {
    EXPECT_EQ("payment-app-icon", manifest.second->icon.value());
    EXPECT_EQ(kPaymentAppInfo[i++].scopeUrl, manifest.second->name);
    ASSERT_EQ(1U, manifest.second->options.size());
    EXPECT_EQ("payment-app-icon", manifest.second->options[0]->icon.value());
    EXPECT_EQ("Visa ****", manifest.second->options[0]->name);
    EXPECT_EQ("payment-app-id", manifest.second->options[0]->id);
    ASSERT_EQ(1U, manifest.second->options[0]->enabled_methods.size());
    EXPECT_EQ("visa", manifest.second->options[0]->enabled_methods[0]);
  }
}

TEST_F(PaymentAppProviderTest, InvokePaymentAppTest) {
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

  PaymentAppProvider::Manifests manifests;
  bool called = false;
  GetAllManifests(base::Bind(&GetAllManifestsCallback, &called, &manifests));
  ASSERT_TRUE(called);
  ASSERT_EQ(3U, manifests.size());

  payments::mojom::PaymentAppRequestPtr app_request =
      payments::mojom::PaymentAppRequest::New();
  app_request->methodData.push_back(payments::mojom::PaymentMethodData::New());
  app_request->total = payments::mojom::PaymentItem::New();
  app_request->total->amount = payments::mojom::PaymentCurrencyAmount::New();

  EXPECT_FALSE(payment_app_invoked());
  InvokePaymentApp(manifests[1].first, std::move(app_request));

  ASSERT_TRUE(payment_app_invoked());
  EXPECT_EQ(manifests[1].first, last_sw_registration_id());
  EXPECT_EQ(GURL(kPaymentAppInfo[1].scopeUrl), last_sw_scope_url());
}

}  // namespace content
