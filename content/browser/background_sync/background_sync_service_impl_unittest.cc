// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_service_impl.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kServiceWorkerPattern[] = "https://example.com/a";
const char kServiceWorkerScript[] = "https://example.com/a/script.js";

// Callbacks from SetUp methods
void RegisterServiceWorkerCallback(bool* called,
                                   int64_t* store_registration_id,
                                   ServiceWorkerStatusCode status,
                                   const std::string& status_message,
                                   int64_t registration_id) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *called = true;
  *store_registration_id = registration_id;
}

void FindServiceWorkerRegistrationCallback(
    scoped_refptr<ServiceWorkerRegistration>* out_registration,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *out_registration = registration;
}

// Callbacks from BackgroundSyncServiceImpl methods

void ErrorAndRegistrationCallback(bool* called,
                                  BackgroundSyncError* out_error,
                                  SyncRegistrationPtr* out_registration,
                                  BackgroundSyncError error,
                                  const SyncRegistrationPtr& registration) {
  *called = true;
  *out_error = error;
  *out_registration = registration.Clone();
}

void ErrorCallback(bool* called,
                   BackgroundSyncError* out_error,
                   BackgroundSyncError error) {
  *called = true;
  *out_error = error;
}

void ErrorAndRegistrationListCallback(
    bool* called,
    BackgroundSyncError* out_error,
    unsigned long* out_array_size,
    BackgroundSyncError error,
    mojo::Array<content::SyncRegistrationPtr> registrations) {
  *called = true;
  *out_error = error;
  if (error == BackgroundSyncError::NONE)
    *out_array_size = registrations.size();
}

}  // namespace

class BackgroundSyncServiceImplTest : public testing::Test {
 public:
  BackgroundSyncServiceImplTest()
      : thread_bundle_(
            new TestBrowserThreadBundle(TestBrowserThreadBundle::IO_MAINLOOP)),
        network_change_notifier_(net::NetworkChangeNotifier::CreateMock()) {
    default_sync_registration_ = SyncRegistration::New();
  }

  void SetUp() override {
    // Don't let the tests be confused by the real-world device connectivity
    background_sync_test_util::SetIgnoreNetworkChangeNotifier(true);

    CreateTestHelper();
    CreateStoragePartition();
    CreateBackgroundSyncContext();
    CreateServiceWorkerRegistration();
    CreateBackgroundSyncServiceImpl();
  }

  void TearDown() override {
    // This must be explicitly destroyed here to ensure that destruction
    // of both the BackgroundSyncContext and the BackgroundSyncManager occurs on
    // the correct thread.
    background_sync_context_->Shutdown();
    base::RunLoop().RunUntilIdle();
    background_sync_context_ = nullptr;

    // Restore the network observer functionality for subsequent tests
    background_sync_test_util::SetIgnoreNetworkChangeNotifier(false);
  }

  // SetUp helper methods
  void CreateTestHelper() {
    embedded_worker_helper_.reset(
        new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void CreateStoragePartition() {
    // Creates a StoragePartition so that the BackgroundSyncManager can
    // use it to access the BrowserContext.
    storage_partition_impl_.reset(new StoragePartitionImpl(
        embedded_worker_helper_->browser_context(), base::FilePath(), nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    embedded_worker_helper_->context_wrapper()->set_storage_partition(
        storage_partition_impl_.get());
  }

  void CreateBackgroundSyncContext() {
    background_sync_context_ = new BackgroundSyncContextImpl();
    background_sync_context_->Init(embedded_worker_helper_->context_wrapper());

    // Tests do not expect the sync event to fire immediately after
    // register (and cleanup up the sync registrations).  Prevent the sync
    // event from firing by setting the network state to have no connection.
    // NOTE: The setup of the network connection must happen after the
    //       BackgroundSyncManager has been setup, including any asynchronous
    //       initialization.
    base::RunLoop().RunUntilIdle();
    BackgroundSyncNetworkObserver* network_observer =
        background_sync_context_->background_sync_manager()
            ->GetNetworkObserverForTesting();
    network_observer->NotifyManagerIfNetworkChangedForTesting(
        net::NetworkChangeNotifier::CONNECTION_NONE);
    base::RunLoop().RunUntilIdle();
  }

  void CreateServiceWorkerRegistration() {
    bool called = false;
    embedded_worker_helper_->context()->RegisterServiceWorker(
        GURL(kServiceWorkerPattern), GURL(kServiceWorkerScript), NULL,
        base::Bind(&RegisterServiceWorkerCallback, &called,
                   &sw_registration_id_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);

    embedded_worker_helper_->context_wrapper()->FindReadyRegistrationForId(
        sw_registration_id_, GURL(kServiceWorkerPattern).GetOrigin(),
        base::Bind(FindServiceWorkerRegistrationCallback, &sw_registration_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(sw_registration_);
  }

  void CreateBackgroundSyncServiceImpl() {
    // Create a dummy mojo channel so that the BackgroundSyncServiceImpl can be
    // instantiated
    mojo::InterfaceRequest<BackgroundSyncService> service_request =
        mojo::GetProxy(&service_ptr_);
    // Create a new BackgroundSyncServiceImpl bound to the dummy channel
    background_sync_context_->CreateService(std::move(service_request));
    base::RunLoop().RunUntilIdle();

    service_impl_ = *background_sync_context_->services_.begin();
    ASSERT_TRUE(service_impl_);
  }

  // Helpers for testing BackgroundSyncServiceImpl methods
  void Register(SyncRegistrationPtr sync,
                const BackgroundSyncService::RegisterCallback& callback) {
    service_impl_->Register(std::move(sync), sw_registration_id_,
                            false /* requested_from_service_worker */,
                            callback);
    base::RunLoop().RunUntilIdle();
  }

  void Unregister(int32_t handle_id,
                  const BackgroundSyncService::UnregisterCallback& callback) {
    service_impl_->Unregister(
        handle_id, sw_registration_id_, callback);
    base::RunLoop().RunUntilIdle();
  }

  void GetRegistration(
      const mojo::String& tag,
      const BackgroundSyncService::RegisterCallback& callback) {
    service_impl_->GetRegistration(tag, sw_registration_id_, callback);
    base::RunLoop().RunUntilIdle();
  }

  void GetRegistrations(
      const BackgroundSyncService::GetRegistrationsCallback& callback) {
    service_impl_->GetRegistrations(sw_registration_id_, callback);
    base::RunLoop().RunUntilIdle();
  }

  scoped_ptr<TestBrowserThreadBundle> thread_bundle_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<EmbeddedWorkerTestHelper> embedded_worker_helper_;
  scoped_ptr<StoragePartitionImpl> storage_partition_impl_;
  scoped_refptr<BackgroundSyncContextImpl> background_sync_context_;
  int64_t sw_registration_id_;
  scoped_refptr<ServiceWorkerRegistration> sw_registration_;
  BackgroundSyncServicePtr service_ptr_;
  BackgroundSyncServiceImpl*
      service_impl_;  // Owned by background_sync_context_
  SyncRegistrationPtr default_sync_registration_;
};

// Tests

TEST_F(BackgroundSyncServiceImplTest, Register) {
  bool called = false;
  BackgroundSyncError error;
  SyncRegistrationPtr reg;
  Register(default_sync_registration_.Clone(),
           base::Bind(&ErrorAndRegistrationCallback, &called, &error, &reg));
  EXPECT_TRUE(called);
  EXPECT_EQ(BackgroundSyncError::NONE, error);
  EXPECT_EQ("", reg->tag);
}

TEST_F(BackgroundSyncServiceImplTest, Unregister) {
  bool unregister_called = false;
  BackgroundSyncError unregister_error;
  SyncRegistrationPtr reg;
  Unregister(default_sync_registration_->handle_id,
             base::Bind(&ErrorCallback, &unregister_called, &unregister_error));
  EXPECT_TRUE(unregister_called);
  EXPECT_EQ(BackgroundSyncError::NOT_ALLOWED, unregister_error);
}

TEST_F(BackgroundSyncServiceImplTest, UnregisterWithRegisteredSync) {
  bool register_called = false;
  bool unregister_called = false;
  BackgroundSyncError register_error;
  BackgroundSyncError unregister_error;
  SyncRegistrationPtr reg;
  Register(default_sync_registration_.Clone(),
           base::Bind(&ErrorAndRegistrationCallback, &register_called,
                      &register_error, &reg));
  EXPECT_TRUE(register_called);
  EXPECT_EQ(BackgroundSyncError::NONE, register_error);
  Unregister(reg->handle_id,
             base::Bind(&ErrorCallback, &unregister_called, &unregister_error));
  EXPECT_TRUE(unregister_called);
  EXPECT_EQ(BackgroundSyncError::NONE, unregister_error);
}

TEST_F(BackgroundSyncServiceImplTest, GetRegistration) {
  bool called = false;
  BackgroundSyncError error;
  SyncRegistrationPtr reg;
  GetRegistration(
      "", base::Bind(&ErrorAndRegistrationCallback, &called, &error, &reg));
  EXPECT_TRUE(called);
  EXPECT_EQ(BackgroundSyncError::NOT_FOUND, error);
}

TEST_F(BackgroundSyncServiceImplTest, GetRegistrationWithRegisteredSync) {
  bool register_called = false;
  bool getregistration_called = false;
  BackgroundSyncError register_error;
  BackgroundSyncError getregistration_error;
  SyncRegistrationPtr register_reg;
  SyncRegistrationPtr getregistration_reg;
  Register(default_sync_registration_.Clone(),
           base::Bind(&ErrorAndRegistrationCallback, &register_called,
                      &register_error, &register_reg));
  EXPECT_TRUE(register_called);
  EXPECT_EQ(BackgroundSyncError::NONE, register_error);
  GetRegistration(
      register_reg->tag,
      base::Bind(&ErrorAndRegistrationCallback, &getregistration_called,
                 &getregistration_error, &getregistration_reg));
  EXPECT_TRUE(getregistration_called);
  EXPECT_EQ(BackgroundSyncError::NONE, getregistration_error);
}

TEST_F(BackgroundSyncServiceImplTest, GetRegistrations) {
  bool called = false;
  BackgroundSyncError error;
  unsigned long array_size = 0UL;
  GetRegistrations(base::Bind(&ErrorAndRegistrationListCallback, &called,
                              &error, &array_size));
  EXPECT_TRUE(called);
  EXPECT_EQ(BackgroundSyncError::NONE, error);
  EXPECT_EQ(0UL, array_size);
}

TEST_F(BackgroundSyncServiceImplTest, GetRegistrationsWithRegisteredSync) {
  bool register_called = false;
  bool getregistrations_called = false;
  BackgroundSyncError register_error;
  BackgroundSyncError getregistrations_error;
  SyncRegistrationPtr register_reg;
  unsigned long array_size = 0UL;
  Register(default_sync_registration_.Clone(),
           base::Bind(&ErrorAndRegistrationCallback, &register_called,
                      &register_error, &register_reg));
  EXPECT_TRUE(register_called);
  EXPECT_EQ(BackgroundSyncError::NONE, register_error);
  GetRegistrations(base::Bind(&ErrorAndRegistrationListCallback,
                              &getregistrations_called, &getregistrations_error,
                              &array_size));
  EXPECT_TRUE(getregistrations_called);
  EXPECT_EQ(BackgroundSyncError::NONE, getregistrations_error);
  EXPECT_EQ(1UL, array_size);
}

}  // namespace content
