// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_service_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/run_loop.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kServiceWorkerPattern[] = "https://example.com/a";
const char kServiceWorkerScript[] = "https://example.com/a/script.js";
const int kRenderProcessId = 99;

// Callbacks from SetUp methods

void RegisterServiceWorkerCallback(bool* called,
                                   int64* store_registration_id,
                                   ServiceWorkerStatusCode status,
                                   const std::string& status_message,
                                   int64 registration_id) {
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
  if (error == BackgroundSyncError::BACKGROUND_SYNC_ERROR_NONE)
    *out_array_size = registrations.size();
}

class MockPowerMonitorSource : public base::PowerMonitorSource {
 private:
  // PowerMonitorSource overrides.
  bool IsOnBatteryPowerImpl() final { return false; }
};

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
    CreateTestHelper();
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
  }

  // SetUp helper methods
  void CreateTestHelper() {
    embedded_worker_helper_.reset(
        new EmbeddedWorkerTestHelper(base::FilePath(), kRenderProcessId));
  }

  void CreateBackgroundSyncContext() {
    power_monitor_.reset(
        new base::PowerMonitor(make_scoped_ptr(new MockPowerMonitorSource())));

    background_sync_context_ = new BackgroundSyncContextImpl();
    background_sync_context_->Init(embedded_worker_helper_->context_wrapper());

    // Tests do not expect the sync event to fire immediately after
    // register (and cleanup up the sync registrations).  Prevent the sync
    // event from firing by setting the network state to have no connection.
    // NOTE: The setup of the network connection must happen after the
    //       BackgroundSyncManager has been setup, including any asynchronous
    //       initialization.
    base::RunLoop().RunUntilIdle();
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
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

    // Register window client for the service worker
    provider_host_.reset(new ServiceWorkerProviderHost(
        34 /* dummy render proces id */, MSG_ROUTING_NONE /* render_frame_id */,
        1 /* dummy provider id */, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
        embedded_worker_helper_->context()->AsWeakPtr(), nullptr));
    provider_host_->SetDocumentUrl(GURL(kServiceWorkerPattern));

    embedded_worker_helper_->context_wrapper()->FindRegistrationForId(
        sw_registration_id_, GURL(kServiceWorkerPattern).GetOrigin(),
        base::Bind(FindServiceWorkerRegistrationCallback, &sw_registration_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(sw_registration_);

    sw_registration_->active_version()->AddControllee(provider_host_.get());
  }

  void RemoveWindowClient() {
    sw_registration_->active_version()->RemoveControllee(provider_host_.get());
  }

  void CreateBackgroundSyncServiceImpl() {
    // Create a dummy mojo channel so that the BackgroundSyncServiceImpl can be
    // instantiated
    mojo::InterfaceRequest<BackgroundSyncService> service_request =
        mojo::GetProxy(&service_ptr_);
    // Create a new BackgroundSyncServiceImpl bound to the dummy channel
    service_impl_.reset(new BackgroundSyncServiceImpl(
        background_sync_context_.get(), service_request.Pass()));
    base::RunLoop().RunUntilIdle();
  }

  // Helpers for testing BackgroundSyncServiceImpl methods
  void RegisterOneShot(
      SyncRegistrationPtr sync,
      const BackgroundSyncService::RegisterCallback& callback) {
    service_impl_->Register(sync.Pass(), sw_registration_id_, callback);
    base::RunLoop().RunUntilIdle();
  }

  void UnregisterOneShot(
      SyncRegistrationPtr sync,
      const BackgroundSyncService::UnregisterCallback& callback) {
    service_impl_->Unregister(
        BackgroundSyncPeriodicity::BACKGROUND_SYNC_PERIODICITY_ONE_SHOT,
        sync->id, sync->tag, sw_registration_id_, callback);
    base::RunLoop().RunUntilIdle();
  }

  void GetRegistrationOneShot(
      const mojo::String& tag,
      const BackgroundSyncService::RegisterCallback& callback) {
    service_impl_->GetRegistration(
        BackgroundSyncPeriodicity::BACKGROUND_SYNC_PERIODICITY_ONE_SHOT, tag,
        sw_registration_id_, callback);
    base::RunLoop().RunUntilIdle();
  }

  void GetRegistrationsOneShot(
      const BackgroundSyncService::GetRegistrationsCallback& callback) {
    service_impl_->GetRegistrations(
        BackgroundSyncPeriodicity::BACKGROUND_SYNC_PERIODICITY_ONE_SHOT,
        sw_registration_id_, callback);
    base::RunLoop().RunUntilIdle();
  }

  scoped_ptr<TestBrowserThreadBundle> thread_bundle_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<EmbeddedWorkerTestHelper> embedded_worker_helper_;
  scoped_ptr<base::PowerMonitor> power_monitor_;
  scoped_refptr<BackgroundSyncContextImpl> background_sync_context_;
  scoped_ptr<ServiceWorkerProviderHost> provider_host_;
  int64 sw_registration_id_;
  scoped_refptr<ServiceWorkerRegistration> sw_registration_;
  BackgroundSyncServicePtr service_ptr_;
  scoped_ptr<BackgroundSyncServiceImpl> service_impl_;
  SyncRegistrationPtr default_sync_registration_;
};

// Tests

TEST_F(BackgroundSyncServiceImplTest, Register) {
  bool called = false;
  BackgroundSyncError error;
  SyncRegistrationPtr reg;
  RegisterOneShot(
      default_sync_registration_.Clone(),
      base::Bind(&ErrorAndRegistrationCallback, &called, &error, &reg));
  EXPECT_TRUE(called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NONE, error);
  EXPECT_EQ("", reg->tag);
}

TEST_F(BackgroundSyncServiceImplTest, RegisterWithoutWindow) {
  bool called = false;
  BackgroundSyncError error;
  SyncRegistrationPtr reg;
  RemoveWindowClient();
  RegisterOneShot(
      default_sync_registration_.Clone(),
      base::Bind(&ErrorAndRegistrationCallback, &called, &error, &reg));
  EXPECT_TRUE(called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NOT_ALLOWED, error);
}

TEST_F(BackgroundSyncServiceImplTest, Unregister) {
  bool unregister_called = false;
  BackgroundSyncError unregister_error;
  SyncRegistrationPtr reg;
  UnregisterOneShot(
      default_sync_registration_.Clone(),
      base::Bind(&ErrorCallback, &unregister_called, &unregister_error));
  EXPECT_TRUE(unregister_called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NOT_FOUND,
            unregister_error);
}

TEST_F(BackgroundSyncServiceImplTest, UnregisterWithRegisteredSync) {
  bool register_called = false;
  bool unregister_called = false;
  BackgroundSyncError register_error;
  BackgroundSyncError unregister_error;
  SyncRegistrationPtr reg;
  RegisterOneShot(default_sync_registration_.Clone(),
                  base::Bind(&ErrorAndRegistrationCallback, &register_called,
                             &register_error, &reg));
  EXPECT_TRUE(register_called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NONE, register_error);
  UnregisterOneShot(reg.Pass(), base::Bind(&ErrorCallback, &unregister_called,
                                           &unregister_error));
  EXPECT_TRUE(unregister_called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NONE, unregister_error);
}

TEST_F(BackgroundSyncServiceImplTest, GetRegistration) {
  bool called = false;
  BackgroundSyncError error;
  SyncRegistrationPtr reg;
  GetRegistrationOneShot(
      "", base::Bind(&ErrorAndRegistrationCallback, &called, &error, &reg));
  EXPECT_TRUE(called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NOT_FOUND, error);
}

TEST_F(BackgroundSyncServiceImplTest, GetRegistrationWithRegisteredSync) {
  bool register_called = false;
  bool getregistration_called = false;
  BackgroundSyncError register_error;
  BackgroundSyncError getregistration_error;
  SyncRegistrationPtr register_reg;
  SyncRegistrationPtr getregistration_reg;
  RegisterOneShot(default_sync_registration_.Clone(),
                  base::Bind(&ErrorAndRegistrationCallback, &register_called,
                             &register_error, &register_reg));
  EXPECT_TRUE(register_called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NONE, register_error);
  GetRegistrationOneShot(
      register_reg->tag,
      base::Bind(&ErrorAndRegistrationCallback, &getregistration_called,
                 &getregistration_error, &getregistration_reg));
  EXPECT_TRUE(getregistration_called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NONE,
            getregistration_error);
}

TEST_F(BackgroundSyncServiceImplTest, GetRegistrations) {
  bool called = false;
  BackgroundSyncError error;
  unsigned long array_size = 0UL;
  GetRegistrationsOneShot(base::Bind(&ErrorAndRegistrationListCallback, &called,
                                     &error, &array_size));
  EXPECT_TRUE(called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NONE, error);
  EXPECT_EQ(0UL, array_size);
}

TEST_F(BackgroundSyncServiceImplTest, GetRegistrationsWithRegisteredSync) {
  bool register_called = false;
  bool getregistrations_called = false;
  BackgroundSyncError register_error;
  BackgroundSyncError getregistrations_error;
  SyncRegistrationPtr register_reg;
  unsigned long array_size = 0UL;
  RegisterOneShot(default_sync_registration_.Clone(),
                  base::Bind(&ErrorAndRegistrationCallback, &register_called,
                             &register_error, &register_reg));
  EXPECT_TRUE(register_called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NONE, register_error);
  GetRegistrationsOneShot(base::Bind(&ErrorAndRegistrationListCallback,
                                     &getregistrations_called,
                                     &getregistrations_error, &array_size));
  EXPECT_TRUE(getregistrations_called);
  EXPECT_EQ(BackgroundSyncError::BACKGROUND_SYNC_ERROR_NONE,
            getregistrations_error);
  EXPECT_EQ(1UL, array_size);
}

}  // namespace content
