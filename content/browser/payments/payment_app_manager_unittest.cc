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
const char kUnregisteredServiceWorkerPattern[] =
    "https://example.com/unregistered";

void RegisterServiceWorkerCallback(bool* called,
                                   int64_t* store_registration_id,
                                   ServiceWorkerStatusCode status,
                                   const std::string& status_message,
                                   int64_t registration_id) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *called = true;
  *store_registration_id = registration_id;
}

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

class PaymentAppManagerTest : public testing::Test {
 public:
  PaymentAppManagerTest()
      : thread_bundle_(
            new TestBrowserThreadBundle(TestBrowserThreadBundle::IO_MAINLOOP)),
        embedded_worker_helper_(new EmbeddedWorkerTestHelper(base::FilePath())),
        storage_partition_impl_(new StoragePartitionImpl(
            embedded_worker_helper_->browser_context(),
            base::FilePath(), nullptr)),
        sw_registration_id_(0) {
    embedded_worker_helper_->context_wrapper()->set_storage_partition(
        storage_partition_impl_.get());

    payment_app_context_ = new PaymentAppContextImpl();
    payment_app_context_->Init(embedded_worker_helper_->context_wrapper());

    bool called = false;
    embedded_worker_helper_->context()->RegisterServiceWorker(
        GURL(kServiceWorkerPattern), GURL(kServiceWorkerScript), NULL,
        base::Bind(&RegisterServiceWorkerCallback, &called,
                   &sw_registration_id_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);

    mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request =
        mojo::GetProxy(&service_);
    payment_app_context_->CreatePaymentAppManager(std::move(request));
    base::RunLoop().RunUntilIdle();

    manager_ = payment_app_context_->payment_app_managers_.begin()->first;
    EXPECT_NE(nullptr, manager_);
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
  scoped_refptr<PaymentAppContextImpl> payment_app_context_;
  payments::mojom::PaymentAppManagerPtr service_;

  // Owned by payment_app_context_.
  PaymentAppManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppManagerTest);
};

TEST_F(PaymentAppManagerTest, SetAndGetManifest) {
  bool called = false;
  payments::mojom::PaymentAppManifestError error = payments::mojom::
      PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED;
  SetManifest(kServiceWorkerPattern, CreatePaymentAppManifestForTest(),
              base::Bind(&SetManifestCallback, &called, &error));

  ASSERT_TRUE(called);
  ASSERT_EQ(error, payments::mojom::PaymentAppManifestError::NONE);

  called = false;
  payments::mojom::PaymentAppManifestPtr read_manifest;
  payments::mojom::PaymentAppManifestError read_error = payments::mojom::
      PaymentAppManifestError::MANIFEST_STORAGE_OPERATION_FAILED;
  GetManifest(kServiceWorkerPattern, base::Bind(&GetManifestCallback, &called,
                                                &read_manifest, &read_error));

  ASSERT_TRUE(called);
  ASSERT_EQ(read_error, payments::mojom::PaymentAppManifestError::NONE);
  EXPECT_EQ(read_manifest->icon, std::string("payment-app-icon"));
  EXPECT_EQ(read_manifest->name, "Payment App");
  ASSERT_EQ(read_manifest->options.size(), 1U);
  EXPECT_EQ(read_manifest->options[0]->icon, std::string("payment-app-icon"));
  EXPECT_EQ(read_manifest->options[0]->name, "Visa ****");
  EXPECT_EQ(read_manifest->options[0]->id, "payment-app-id");
  ASSERT_EQ(read_manifest->options[0]->enabled_methods.size(), 1U);
  EXPECT_EQ(read_manifest->options[0]->enabled_methods[0], "visa");
}

TEST_F(PaymentAppManagerTest, SetManifestWithoutAssociatedServiceWorker) {
  bool called = false;
  payments::mojom::PaymentAppManifestError error =
      payments::mojom::PaymentAppManifestError::NONE;
  SetManifest(kUnregisteredServiceWorkerPattern,
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
      kUnregisteredServiceWorkerPattern,
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
  GetManifest(kServiceWorkerPattern, base::Bind(&GetManifestCallback, &called,
                                                &read_manifest, &read_error));

  ASSERT_TRUE(called);
  EXPECT_EQ(read_error, payments::mojom::PaymentAppManifestError::
                            MANIFEST_STORAGE_OPERATION_FAILED);
}

}  // namespace content
