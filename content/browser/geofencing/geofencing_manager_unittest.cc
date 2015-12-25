// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/geofencing/geofencing_manager.h"
#include "content/browser/geofencing/geofencing_service.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"

using blink::WebCircularGeofencingRegion;
typedef std::map<std::string, WebCircularGeofencingRegion> RegionMap;

namespace {

static const char* kTestRegionId = "region-id";
static const int64_t kTestGeofencingRegistrationId = 42;
static const int64_t kTestGeofencingRegistrationId2 = 43;

bool RegionsMatch(const WebCircularGeofencingRegion& expected,
                  const WebCircularGeofencingRegion& arg) {
  return testing::Matches(expected.latitude)(arg.latitude) &&
         testing::Matches(expected.longitude)(arg.longitude) &&
         testing::Matches(expected.radius)(arg.radius);
}
}

namespace content {

class TestGeofencingService : public GeofencingService {
 public:
  TestGeofencingService() : is_available_(false) {}

  bool IsServiceAvailable() override { return is_available_; }

  void SetIsServiceAvailable(bool is_available) {
    is_available_ = is_available;
  }

  MOCK_METHOD2(RegisterRegion,
               int64_t(const WebCircularGeofencingRegion& region,
                       GeofencingRegistrationDelegate* delegate));
  MOCK_METHOD1(UnregisterRegion, void(int64_t geofencing_registration_id));

 private:
  bool is_available_;
};

ACTION_P(SaveDelegate, delegate) {
  *delegate = arg1;
}

ACTION_P(QuitRunner, runner) {
  runner->Quit();
}

MATCHER_P(WebCircularGeofencingRegionEq, expected, "") {
  return RegionsMatch(expected, arg);
}

class StatusCatcher {
 public:
  StatusCatcher() : was_called_(false), runner_(new MessageLoopRunner()) {}

  void Done(GeofencingStatus status) {
    CHECK(!was_called_);
    result_ = status;
    was_called_ = true;
    runner_->Quit();
  }

  GeofencingStatus Wait() {
    runner_->Run();
    CHECK(was_called_);
    return result_;
  }

 private:
  bool was_called_;
  GeofencingStatus result_;
  scoped_refptr<MessageLoopRunner> runner_;
};

void SaveResponseCallback(bool* called,
                          int64_t* store_registration_id,
                          ServiceWorkerStatusCode status,
                          const std::string& status_message,
                          int64_t registration_id) {
  EXPECT_EQ(SERVICE_WORKER_OK, status) << ServiceWorkerStatusToString(status);
  *called = true;
  *store_registration_id = registration_id;
}

ServiceWorkerContextCore::RegistrationCallback MakeRegisteredCallback(
    bool* called,
    int64_t* store_registration_id) {
  return base::Bind(&SaveResponseCallback, called, store_registration_id);
}

void CallCompletedCallback(bool* called, ServiceWorkerStatusCode) {
  *called = true;
}

ServiceWorkerContextCore::UnregistrationCallback MakeUnregisteredCallback(
    bool* called) {
  return base::Bind(&CallCompletedCallback, called);
}

class GeofencingManagerTest : public testing::Test {
 public:
  GeofencingManagerTest() : service_(nullptr) {
    test_region_.latitude = 37.421999;
    test_region_.longitude = -122.084015;
    test_region_.radius = 100;
    expected_regions_[kTestRegionId] = test_region_;
  }

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
    service_ = new TestGeofencingService();
    manager_ = new GeofencingManager(helper_->context_wrapper());
    manager_->SetServiceForTesting(service_);
    manager_->Init();

    worker1_ = RegisterServiceWorker("1");
    worker2_ = RegisterServiceWorker("2");
  }

  void TearDown() override {
    worker1_ = nullptr;
    worker2_ = nullptr;
    manager_ = nullptr;
    delete service_;
    service_ = nullptr;
    helper_.reset();
  }

  void SetHasProviderForTests() { service_->SetIsServiceAvailable(true); }

  scoped_refptr<ServiceWorkerRegistration> RegisterServiceWorker(
      const std::string& name) {
    GURL pattern("http://www.example.com/" + name);
    GURL script_url("http://www.example.com/service_worker.js");
    int64_t registration_id = kInvalidServiceWorkerRegistrationId;
    bool called = false;
    helper_->context()->RegisterServiceWorker(
        pattern, script_url, nullptr,
        MakeRegisteredCallback(&called, &registration_id));

    EXPECT_FALSE(called);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
    scoped_refptr<ServiceWorkerRegistration> worker(
        new ServiceWorkerRegistration(pattern, registration_id,
                                      helper_->context()->AsWeakPtr()));
    // ServiceWorkerRegistration posts a notification task on construction.
    base::RunLoop().RunUntilIdle();
    return worker;
  }

  void UnregisterServiceWorker(
      const scoped_refptr<ServiceWorkerRegistration>& registration) {
    bool called = false;
    helper_->context()->UnregisterServiceWorker(
        registration->pattern(), MakeUnregisteredCallback(&called));

    EXPECT_FALSE(called);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
  }

  GeofencingStatus RegisterRegionSync(
      int64_t service_worker_registration_id,
      const std::string& id,
      const WebCircularGeofencingRegion& region) {
    StatusCatcher result;
    manager_->RegisterRegion(
        service_worker_registration_id,
        id,
        region,
        base::Bind(&StatusCatcher::Done, base::Unretained(&result)));
    return result.Wait();
  }

  GeofencingStatus RegisterRegionSyncWithServiceResult(
      int64_t service_worker_registration_id,
      const std::string& id,
      const WebCircularGeofencingRegion& region,
      GeofencingStatus service_status,
      int64_t geofencing_registration_id) {
    StatusCatcher result;
    GeofencingRegistrationDelegate* delegate = 0;
    EXPECT_CALL(
        *service_,
        RegisterRegion(WebCircularGeofencingRegionEq(region), testing::_))
        .WillOnce(testing::DoAll(SaveDelegate(&delegate),
                                 testing::Return(geofencing_registration_id)));
    manager_->RegisterRegion(
        service_worker_registration_id,
        id,
        region,
        base::Bind(&StatusCatcher::Done, base::Unretained(&result)));
    CHECK(delegate);
    delegate->RegistrationFinished(geofencing_registration_id, service_status);
    return result.Wait();
  }

  GeofencingStatus UnregisterRegionSync(
      int64_t service_worker_registration_id,
      const std::string& id,
      bool should_call_service,
      int64_t geofencing_registration_id = 0) {
    StatusCatcher result;
    if (should_call_service) {
      EXPECT_CALL(*service_, UnregisterRegion(geofencing_registration_id));
    }
    manager_->UnregisterRegion(
        service_worker_registration_id,
        id,
        base::Bind(&StatusCatcher::Done, base::Unretained(&result)));
    return result.Wait();
  }

  void VerifyRegions(int64_t service_worker_registration_id,
                     const RegionMap& expected_regions) {
    RegionMap regions;
    EXPECT_EQ(GEOFENCING_STATUS_OK,
              manager_->GetRegisteredRegions(service_worker_registration_id,
                                             &regions));
    EXPECT_EQ(expected_regions.size(), regions.size());
    for (RegionMap::const_iterator it = expected_regions.begin();
         it != expected_regions.end();
         ++it) {
      EXPECT_THAT(regions[it->first],
                  WebCircularGeofencingRegionEq(it->second));
    }
  }

 protected:
  TestBrowserThreadBundle threads_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  TestGeofencingService* service_;
  scoped_refptr<GeofencingManager> manager_;

  WebCircularGeofencingRegion test_region_;
  RegionMap expected_regions_;

  scoped_refptr<ServiceWorkerRegistration> worker1_;
  scoped_refptr<ServiceWorkerRegistration> worker2_;
};

TEST_F(GeofencingManagerTest, RegisterRegion_NoService) {
  EXPECT_EQ(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            RegisterRegionSync(worker1_->id(), kTestRegionId, test_region_));
}

TEST_F(GeofencingManagerTest, UnregisterRegion_NoService) {
  EXPECT_EQ(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            UnregisterRegionSync(worker1_->id(), kTestRegionId, false));
}

TEST_F(GeofencingManagerTest, GetRegisteredRegions_NoService) {
  RegionMap regions;
  EXPECT_EQ(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            manager_->GetRegisteredRegions(worker1_->id(), &regions));
  EXPECT_TRUE(regions.empty());
}

TEST_F(GeofencingManagerTest, RegisterRegion_FailsInService) {
  SetHasProviderForTests();
  EXPECT_EQ(GEOFENCING_STATUS_ERROR,
            RegisterRegionSyncWithServiceResult(worker1_->id(), kTestRegionId,
                                                test_region_,
                                                GEOFENCING_STATUS_ERROR, -1));
}

TEST_F(GeofencingManagerTest, RegisterRegion_SucceedsInService) {
  SetHasProviderForTests();
  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker1_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId));
  VerifyRegions(worker1_->id(), expected_regions_);
}

TEST_F(GeofencingManagerTest, RegisterRegion_AlreadyRegistered) {
  SetHasProviderForTests();
  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker1_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId));
  VerifyRegions(worker1_->id(), expected_regions_);

  WebCircularGeofencingRegion region2;
  region2.latitude = 43.2;
  region2.longitude = 1.45;
  region2.radius = 8.5;
  EXPECT_EQ(GEOFENCING_STATUS_ERROR,
            RegisterRegionSync(worker1_->id(), kTestRegionId, region2));
  VerifyRegions(worker1_->id(), expected_regions_);
}

TEST_F(GeofencingManagerTest, UnregisterRegion_NotRegistered) {
  SetHasProviderForTests();
  EXPECT_EQ(GEOFENCING_STATUS_UNREGISTRATION_FAILED_NOT_REGISTERED,
            UnregisterRegionSync(worker1_->id(), kTestRegionId, false));
}

TEST_F(GeofencingManagerTest, UnregisterRegion_Success) {
  SetHasProviderForTests();

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker1_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId));

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            UnregisterRegionSync(worker1_->id(), kTestRegionId, true,
                                 kTestGeofencingRegistrationId));
  VerifyRegions(worker1_->id(), RegionMap());
}

TEST_F(GeofencingManagerTest, GetRegisteredRegions_RegistrationInProgress) {
  SetHasProviderForTests();
  StatusCatcher result;
  GeofencingRegistrationDelegate* delegate = nullptr;

  EXPECT_CALL(
      *service_,
      RegisterRegion(WebCircularGeofencingRegionEq(test_region_), testing::_))
      .WillOnce(testing::DoAll(SaveDelegate(&delegate),
                               testing::Return(kTestGeofencingRegistrationId)));
  manager_->RegisterRegion(
      worker1_->id(), kTestRegionId, test_region_,
      base::Bind(&StatusCatcher::Done, base::Unretained(&result)));

  // At this point the manager should have tried registering the region with
  // the service, resulting in |delegate| being set. Until the callback is
  // called the registration is not complete though.
  EXPECT_NE(delegate, nullptr);
  VerifyRegions(worker1_->id(), RegionMap());

  // Now call the callback, and verify the registration completed succesfully.
  delegate->RegistrationFinished(kTestGeofencingRegistrationId,
                                 GEOFENCING_STATUS_OK);
  EXPECT_EQ(GEOFENCING_STATUS_OK, result.Wait());
  VerifyRegions(worker1_->id(), expected_regions_);
}

TEST_F(GeofencingManagerTest, UnregisterRegion_RegistrationInProgress) {
  SetHasProviderForTests();
  StatusCatcher result;
  GeofencingRegistrationDelegate* delegate = nullptr;

  EXPECT_CALL(
      *service_,
      RegisterRegion(WebCircularGeofencingRegionEq(test_region_), testing::_))
      .WillOnce(testing::DoAll(SaveDelegate(&delegate),
                               testing::Return(kTestGeofencingRegistrationId)));
  manager_->RegisterRegion(
      worker1_->id(), kTestRegionId, test_region_,
      base::Bind(&StatusCatcher::Done, base::Unretained(&result)));

  // At this point the manager should have tried registering the region with
  // the service, resulting in |delegate| being set. Until the callback is
  // called the registration is not complete though.
  EXPECT_NE(delegate, nullptr);

  EXPECT_EQ(GEOFENCING_STATUS_UNREGISTRATION_FAILED_NOT_REGISTERED,
            UnregisterRegionSync(worker1_->id(), kTestRegionId, false));
}

TEST_F(GeofencingManagerTest, GetRegisteredRegions_NoRegions) {
  SetHasProviderForTests();
  VerifyRegions(worker1_->id(), RegionMap());
}

TEST_F(GeofencingManagerTest, RegisterRegion_SeparateServiceWorkers) {
  SetHasProviderForTests();

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker1_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId));

  VerifyRegions(worker1_->id(), expected_regions_);
  VerifyRegions(worker2_->id(), RegionMap());

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker2_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId2));

  VerifyRegions(worker1_->id(), expected_regions_);
  VerifyRegions(worker2_->id(), expected_regions_);
}

TEST_F(GeofencingManagerTest, UnregisterRegion_SeparateServiceWorkers) {
  SetHasProviderForTests();

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker1_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId));
  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker2_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId2));

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            UnregisterRegionSync(worker1_->id(), kTestRegionId, true,
                                 kTestGeofencingRegistrationId));

  VerifyRegions(worker1_->id(), RegionMap());
  VerifyRegions(worker2_->id(), expected_regions_);

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            UnregisterRegionSync(worker2_->id(), kTestRegionId, true,
                                 kTestGeofencingRegistrationId2));

  VerifyRegions(worker1_->id(), RegionMap());
  VerifyRegions(worker2_->id(), RegionMap());
}

TEST_F(GeofencingManagerTest, ShutdownCleansRegistrations) {
  SetHasProviderForTests();
  scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner());
  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker1_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId));

  EXPECT_CALL(*service_, UnregisterRegion(kTestGeofencingRegistrationId))
      .WillOnce(QuitRunner(runner));
  manager_->Shutdown();
  runner->Run();
}

TEST_F(GeofencingManagerTest, OnRegistrationDeleted) {
  SetHasProviderForTests();

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker1_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId));
  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker2_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId2));

  EXPECT_CALL(*service_, UnregisterRegion(kTestGeofencingRegistrationId));
  UnregisterServiceWorker(worker1_);
  VerifyRegions(worker1_->id(), RegionMap());
  VerifyRegions(worker2_->id(), expected_regions_);

  EXPECT_CALL(*service_, UnregisterRegion(kTestGeofencingRegistrationId2));
  UnregisterServiceWorker(worker2_);
  VerifyRegions(worker1_->id(), RegionMap());
  VerifyRegions(worker2_->id(), RegionMap());
}

TEST_F(GeofencingManagerTest, RegisterRegion_MockedNoService) {
  manager_->SetMockProvider(GeofencingMockState::SERVICE_UNAVAILABLE);

  EXPECT_EQ(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            RegisterRegionSync(worker1_->id(), kTestRegionId, test_region_));
}

TEST_F(GeofencingManagerTest, UnregisterRegion_MockedNoService) {
  manager_->SetMockProvider(GeofencingMockState::SERVICE_UNAVAILABLE);

  EXPECT_EQ(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            UnregisterRegionSync(worker1_->id(), kTestRegionId, false));
}

TEST_F(GeofencingManagerTest, GetRegisteredRegions_MockedNoService) {
  manager_->SetMockProvider(GeofencingMockState::SERVICE_UNAVAILABLE);

  RegionMap regions;
  EXPECT_EQ(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            manager_->GetRegisteredRegions(worker1_->id(), &regions));
  EXPECT_TRUE(regions.empty());
}

TEST_F(GeofencingManagerTest, RegisterRegion_MockedService) {
  manager_->SetMockProvider(GeofencingMockState::SERVICE_AVAILABLE);

  // Make sure real service doesn't get called.
  EXPECT_CALL(*service_, RegisterRegion(testing::_, testing::_)).Times(0);

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSync(worker1_->id(), kTestRegionId, test_region_));
  VerifyRegions(worker1_->id(), expected_regions_);
}

TEST_F(GeofencingManagerTest, SetMockProviderClearsRegistrations) {
  SetHasProviderForTests();
  EXPECT_EQ(GEOFENCING_STATUS_OK,
            RegisterRegionSyncWithServiceResult(
                worker1_->id(), kTestRegionId, test_region_,
                GEOFENCING_STATUS_OK, kTestGeofencingRegistrationId));
  VerifyRegions(worker1_->id(), expected_regions_);

  EXPECT_CALL(*service_, UnregisterRegion(kTestGeofencingRegistrationId));

  manager_->SetMockProvider(GeofencingMockState::SERVICE_AVAILABLE);
  VerifyRegions(worker1_->id(), RegionMap());
}

}  // namespace content
