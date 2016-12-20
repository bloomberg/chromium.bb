// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_content_unittest_base.h"

#include <stdint.h>

#include <set>
#include <utility>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {

namespace {

void RegisterServiceWorkerCallback(bool* called,
                                   ServiceWorkerStatusCode status,
                                   const std::string& status_message,
                                   int64_t registration_id) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *called = true;
}

}  // namespace

PaymentAppContentUnitTestBase::PaymentAppContentUnitTestBase()
    : thread_bundle_(
          new TestBrowserThreadBundle(TestBrowserThreadBundle::IO_MAINLOOP)),
      embedded_worker_helper_(new EmbeddedWorkerTestHelper(base::FilePath())),
      storage_partition_impl_(
          new StoragePartitionImpl(embedded_worker_helper_->browser_context(),
                                   base::FilePath(),
                                   nullptr)),
      payment_app_context_(new PaymentAppContextImpl()) {
  embedded_worker_helper_->context_wrapper()->set_storage_partition(
      storage_partition_impl_.get());
  payment_app_context_->Init(embedded_worker_helper_->context_wrapper());
  base::RunLoop().RunUntilIdle();
}

PaymentAppContentUnitTestBase::~PaymentAppContentUnitTestBase() {
  payment_app_context_->Shutdown();
  base::RunLoop().RunUntilIdle();
}

PaymentAppManager* PaymentAppContentUnitTestBase::CreatePaymentAppManager(
    const GURL& scope_url,
    const GURL& sw_script_url) {
  // Register service worker for payment app manager.
  bool called = false;
  embedded_worker_helper_->context()->RegisterServiceWorker(
      scope_url, sw_script_url, nullptr,
      base::Bind(&RegisterServiceWorkerCallback, &called));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  // This function should eventually return created payment app manager
  // but there is no way to get last created payment app manager from
  // payment_app_context_->payment_app_managers_ because its type is std::map
  // and can not ensure its order. So, just make a set of existing payment app
  // managers before creating a new manager and then check what is a new thing.
  std::set<PaymentAppManager*> existing_managers;
  for (const auto& existing_manager :
       payment_app_context_->payment_app_managers_) {
    existing_managers.insert(existing_manager.first);
  }

  // Create a new payment app manager.
  payments::mojom::PaymentAppManagerPtr manager;
  mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request =
      mojo::MakeRequest(&manager);
  payment_app_managers_.push_back(std::move(manager));
  payment_app_context_->CreatePaymentAppManager(std::move(request));
  base::RunLoop().RunUntilIdle();

  // Find a last registered payment app manager.
  for (const auto& candidate_manager :
       payment_app_context_->payment_app_managers_) {
    if (!base::ContainsKey(existing_managers, candidate_manager.first))
      return candidate_manager.first;
  }

  NOTREACHED();
  return nullptr;
}

void PaymentAppContentUnitTestBase::SetManifest(
    PaymentAppManager* manager,
    const std::string& scope,
    payments::mojom::PaymentAppManifestPtr manifest,
    const PaymentAppManager::SetManifestCallback& callback) {
  ASSERT_NE(nullptr, manager);
  manager->SetManifest(scope, std::move(manifest), callback);
  base::RunLoop().RunUntilIdle();
}

void PaymentAppContentUnitTestBase::GetManifest(
    PaymentAppManager* manager,
    const std::string& scope,
    const PaymentAppManager::GetManifestCallback& callback) {
  ASSERT_NE(nullptr, manager);
  manager->GetManifest(scope, callback);
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
