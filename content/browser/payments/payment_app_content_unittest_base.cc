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
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
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

void UnregisterServiceWorkerCallback(bool* called,
                                     ServiceWorkerStatusCode status) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *called = true;
}

}  // namespace

class PaymentAppContentUnitTestBase::PaymentAppForWorkerTestHelper
    : public EmbeddedWorkerTestHelper {
 public:
  PaymentAppForWorkerTestHelper()
      : EmbeddedWorkerTestHelper(base::FilePath()),
        was_dispatched_(false),
        last_sw_registration_id_(kInvalidServiceWorkerRegistrationId) {}
  ~PaymentAppForWorkerTestHelper() override {}

  void OnStartWorker(
      int embedded_worker_id,
      int64_t service_worker_version_id,
      const GURL& scope,
      const GURL& script_url,
      bool pause_after_download,
      mojom::ServiceWorkerEventDispatcherRequest request) override {
    ServiceWorkerVersion* version =
        context()->GetLiveVersion(service_worker_version_id);
    version->SetMainScriptHttpResponseInfo(
        EmbeddedWorkerTestHelper::CreateHttpResponseInfo());
    last_sw_registration_id_ = version->registration_id();
    last_sw_scope_ = scope;
    EmbeddedWorkerTestHelper::OnStartWorker(
        embedded_worker_id, service_worker_version_id, scope, script_url,
        pause_after_download, std::move(request));
  }

  void OnPaymentRequestEvent(
      payments::mojom::PaymentAppRequestPtr app_request,
      const mojom::ServiceWorkerEventDispatcher::
          DispatchPaymentRequestEventCallback& callback) override {
    ASSERT_FALSE(was_dispatched_);
    EmbeddedWorkerTestHelper::OnPaymentRequestEvent(std::move(app_request),
                                                    callback);
    was_dispatched_ = true;
  }

  bool was_dispatched_;
  int64_t last_sw_registration_id_;
  GURL last_sw_scope_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentAppForWorkerTestHelper);
};

PaymentAppContentUnitTestBase::PaymentAppContentUnitTestBase()
    : thread_bundle_(
          new TestBrowserThreadBundle(TestBrowserThreadBundle::IO_MAINLOOP)),
      worker_helper_(new PaymentAppForWorkerTestHelper()) {
  worker_helper_->context_wrapper()->set_storage_partition(storage_partition());
  storage_partition()->service_worker_context_->Shutdown();
  base::RunLoop().RunUntilIdle();

  storage_partition()->service_worker_context_ =
      worker_helper_->context_wrapper();
  payment_app_context()->Init(worker_helper_->context_wrapper());
  base::RunLoop().RunUntilIdle();
}

PaymentAppContentUnitTestBase::~PaymentAppContentUnitTestBase() {}

BrowserContext* PaymentAppContentUnitTestBase::browser_context() {
  DCHECK(worker_helper_);
  return worker_helper_->browser_context();
}

PaymentAppManager* PaymentAppContentUnitTestBase::CreatePaymentAppManager(
    const GURL& scope_url,
    const GURL& sw_script_url) {
  // Register service worker for payment app manager.
  bool called = false;
  worker_helper_->context()->RegisterServiceWorker(
      scope_url, sw_script_url, nullptr,
      base::Bind(&RegisterServiceWorkerCallback, &called));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  // This function should eventually return created payment app manager
  // but there is no way to get last created payment app manager from
  // payment_app_context()->payment_app_managers_ because its type is std::map
  // and can not ensure its order. So, just make a set of existing payment app
  // managers before creating a new manager and then check what is a new thing.
  std::set<PaymentAppManager*> existing_managers;
  for (const auto& existing_manager :
       payment_app_context()->payment_app_managers_) {
    existing_managers.insert(existing_manager.first);
  }

  // Create a new payment app manager.
  payments::mojom::PaymentAppManagerPtr manager;
  mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request =
      mojo::MakeRequest(&manager);
  payment_app_managers_.push_back(std::move(manager));
  payment_app_context()->CreatePaymentAppManager(std::move(request));
  base::RunLoop().RunUntilIdle();

  // Find a last registered payment app manager.
  for (const auto& candidate_manager :
       payment_app_context()->payment_app_managers_) {
    if (!base::ContainsKey(existing_managers, candidate_manager.first)) {
      candidate_manager.first->Init(scope_url.spec());
      base::RunLoop().RunUntilIdle();
      return candidate_manager.first;
    }
  }

  NOTREACHED();
  return nullptr;
}

void PaymentAppContentUnitTestBase::SetManifest(
    PaymentAppManager* manager,
    payments::mojom::PaymentAppManifestPtr manifest,
    const PaymentAppManager::SetManifestCallback& callback) {
  ASSERT_NE(nullptr, manager);
  manager->SetManifest(std::move(manifest), callback);
  base::RunLoop().RunUntilIdle();
}

void PaymentAppContentUnitTestBase::GetManifest(
    PaymentAppManager* manager,
    const PaymentAppManager::GetManifestCallback& callback) {
  ASSERT_NE(nullptr, manager);
  manager->GetManifest(callback);
  base::RunLoop().RunUntilIdle();
}

payments::mojom::PaymentAppManifestPtr
PaymentAppContentUnitTestBase::CreatePaymentAppManifestForTest(
    const std::string& name) {
  payments::mojom::PaymentAppOptionPtr option =
      payments::mojom::PaymentAppOption::New();
  option->name = "Visa ****";
  option->id = "payment-app-id";
  option->icon = std::string("payment-app-icon");
  option->enabled_methods.push_back("visa");

  payments::mojom::PaymentAppManifestPtr manifest =
      payments::mojom::PaymentAppManifest::New();
  manifest->icon = std::string("payment-app-icon");
  manifest->name = name;
  manifest->options.push_back(std::move(option));

  return manifest;
}

void PaymentAppContentUnitTestBase::UnregisterServiceWorker(
    const GURL& scope_url) {
  // Unregister service worker.
  bool called = false;
  worker_helper_->context()->UnregisterServiceWorker(
      scope_url, base::Bind(&UnregisterServiceWorkerCallback, &called));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

void PaymentAppContentUnitTestBase::ResetPaymentAppInvoked() const {
  worker_helper_->was_dispatched_ = false;
}

bool PaymentAppContentUnitTestBase::payment_app_invoked() const {
  return worker_helper_->was_dispatched_;
}

int64_t PaymentAppContentUnitTestBase::last_sw_registration_id() const {
  return worker_helper_->last_sw_registration_id_;
}

const GURL& PaymentAppContentUnitTestBase::last_sw_scope_url() const {
  return worker_helper_->last_sw_scope_;
}

StoragePartitionImpl* PaymentAppContentUnitTestBase::storage_partition() {
  return static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
}

PaymentAppContextImpl* PaymentAppContentUnitTestBase::payment_app_context() {
  return storage_partition()->GetPaymentAppContext();
}

}  // namespace content
