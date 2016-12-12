// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "components/payments/payment_app.mojom.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const char kServiceWorkerPattern[] = "https://example.com/a";
const char kServiceWorkerScript[] = "https://example.com/a/script.js";

void RegisterServiceWorkerCallback(bool* called,
                                   int64_t* store_registration_id,
                                   ServiceWorkerStatusCode status,
                                   const std::string& status_message,
                                   int64_t registration_id) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *called = true;
  *store_registration_id = registration_id;
}

void SetManifestCallback(payments::mojom::PaymentAppManifestError* out_error,
                         payments::mojom::PaymentAppManifestError error) {
  *out_error = error;
}

void GetManifestCallback(payments::mojom::PaymentAppManifestPtr* out_manifest,
                         payments::mojom::PaymentAppManifestError* out_error,
                         payments::mojom::PaymentAppManifestPtr manifest,
                         payments::mojom::PaymentAppManifestError error) {
  *out_manifest = std::move(manifest);
  *out_error = error;
}

}  // namespace

class PaymentAppManagerTest : public testing::Test {
 public:
  PaymentAppManagerTest()
      : thread_bundle_(
            new TestBrowserThreadBundle(TestBrowserThreadBundle::IO_MAINLOOP)),
        embedded_worker_helper_(new EmbeddedWorkerTestHelper(base::FilePath())),
        storage_partition_impl_(new StoragePartitionImpl(
            embedded_worker_helper_->browser_context(), base::FilePath(),
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {

    embedded_worker_helper_->context_wrapper()->set_storage_partition(
        storage_partition_impl_.get());

    payment_app_context_ =
        new PaymentAppContext(embedded_worker_helper_->context_wrapper());

    bool called = false;
    embedded_worker_helper_->context()->RegisterServiceWorker(
        GURL(kServiceWorkerPattern), GURL(kServiceWorkerScript), NULL,
        base::Bind(&RegisterServiceWorkerCallback, &called,
                   &sw_registration_id_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);

    mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request =
        mojo::GetProxy(&service_);
    payment_app_context_->CreateService(std::move(request));
    base::RunLoop().RunUntilIdle();

    manager_ = payment_app_context_->services_.begin()->first;
    EXPECT_TRUE(manager_);
  }

  ~PaymentAppManagerTest() override {
    payment_app_context_->Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void SetManifest(const std::string& scope,
                   payments::mojom::PaymentAppManifestPtr manifest,
                   const PaymentAppManager::SetManifestCallback& callback) {
    manager_->SetManifest(scope, std::move(manifest), callback);
    base::RunLoop().RunUntilIdle();
  }

  void GetManifest(const std::string& scope,
                   const PaymentAppManager::GetManifestCallback& callback) {
    manager_->GetManifest(scope, callback);
    base::RunLoop().RunUntilIdle();
  }

 private:
  std::unique_ptr<TestBrowserThreadBundle> thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> embedded_worker_helper_;
  std::unique_ptr<StoragePartitionImpl> storage_partition_impl_;
  int64_t sw_registration_id_;
  scoped_refptr<PaymentAppContext> payment_app_context_;
  payments::mojom::PaymentAppManagerPtr service_;

  // Owned by payment_app_context_.
  PaymentAppManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppManagerTest);
};

TEST_F(PaymentAppManagerTest, SetAndGetManifest) {
  payments::mojom::PaymentAppOptionPtr option =
      payments::mojom::PaymentAppOption::New();
  option->label = "Visa ****";
  option->id = "payment-app-id";
  option->icon = std::string("payment-app-icon");
  option->enabled_methods.push_back("visa");

  payments::mojom::PaymentAppManifestPtr manifest =
      payments::mojom::PaymentAppManifest::New();
  manifest->icon = std::string("payment-app-icon");
  manifest->label = "Payment App";
  manifest->options.push_back(std::move(option));

  payments::mojom::PaymentAppManifestError error;
  SetManifest(kServiceWorkerPattern, std::move(manifest),
              base::Bind(&SetManifestCallback, &error));

  ASSERT_EQ(error, payments::mojom::PaymentAppManifestError::NONE);

  payments::mojom::PaymentAppManifestPtr read_manifest;
  payments::mojom::PaymentAppManifestError read_error;
  GetManifest(kServiceWorkerPattern,
              base::Bind(&GetManifestCallback, &read_manifest, &read_error));

  ASSERT_EQ(read_error, payments::mojom::PaymentAppManifestError::NONE);
  EXPECT_EQ(read_manifest->icon, std::string("payment-app-icon"));
  EXPECT_EQ(read_manifest->label, "Payment App");
  ASSERT_EQ(read_manifest->options.size(), 1U);
  EXPECT_EQ(read_manifest->options[0]->icon, std::string("payment-app-icon"));
  EXPECT_EQ(read_manifest->options[0]->label, "Visa ****");
  EXPECT_EQ(read_manifest->options[0]->id, "payment-app-id");
  ASSERT_EQ(read_manifest->options[0]->enabled_methods.size(), 1U);
  EXPECT_EQ(read_manifest->options[0]->enabled_methods[0], "visa");
}

TEST_F(PaymentAppManagerTest, GetManifestWithoutAssociatedServiceWorker) {
  payments::mojom::PaymentAppManifestPtr read_manifest;
  payments::mojom::PaymentAppManifestError read_error;
  GetManifest(kServiceWorkerPattern,
              base::Bind(&GetManifestCallback, &read_manifest, &read_error));

  EXPECT_EQ(read_error, payments::mojom::PaymentAppManifestError::
                            MANIFEST_STORAGE_OPERATION_FAILED);
}

}  // namespace content
