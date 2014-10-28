// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/geofencing/geofencing_manager.h"
#include "content/browser/geofencing/geofencing_service.h"
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
static const int64 kTestServiceWorkerRegistrationId = 123;
static const int64 kTestServiceWorkerRegistrationId2 = 456;
static const int64 kTestGeofencingRegistrationId = 42;
static const int64 kTestGeofencingRegistrationId2 = 43;

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
  MOCK_METHOD0(IsServiceAvailable, bool());
  MOCK_METHOD2(RegisterRegion,
               int64(const WebCircularGeofencingRegion& region,
                     GeofencingRegistrationDelegate* delegate));
  MOCK_METHOD1(UnregisterRegion, void(int64 geofencing_registration_id));
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

class GeofencingManagerTest : public testing::Test {
 public:
  GeofencingManagerTest() : service_(nullptr) {
    test_region_.latitude = 37.421999;
    test_region_.longitude = -122.084015;
    test_region_.radius = 100;
    expected_regions_[kTestRegionId] = test_region_;
  }

  void SetUp() override {
    service_ = new TestGeofencingService();
    ON_CALL(*service_, IsServiceAvailable())
        .WillByDefault(testing::Return(false));
    manager_ = new GeofencingManager(nullptr /* ServiceWorkerContextWrapper */);
    manager_->SetServiceForTesting(service_);
  }

  void TearDown() override {
    manager_ = nullptr;
    delete service_;
    service_ = nullptr;
  }

  void SetHasProviderForTests() {
    ON_CALL(*service_, IsServiceAvailable())
        .WillByDefault(testing::Return(true));
  }

  GeofencingStatus RegisterRegionSync(
      int64 service_worker_registration_id,
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
      int64 service_worker_registration_id,
      const std::string& id,
      const WebCircularGeofencingRegion& region,
      GeofencingStatus service_status,
      int64 geofencing_registration_id) {
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

  GeofencingStatus UnregisterRegionSync(int64 service_worker_registration_id,
                                        const std::string& id,
                                        bool should_call_service,
                                        int64 geofencing_registration_id = 0) {
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

  void VerifyRegions(int64 service_worker_registration_id,
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
  TestGeofencingService* service_;
  scoped_refptr<GeofencingManager> manager_;

  WebCircularGeofencingRegion test_region_;
  RegionMap expected_regions_;
};

TEST_F(GeofencingManagerTest, RegisterRegion_NoService) {
  EXPECT_EQ(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            RegisterRegionSync(
                kTestServiceWorkerRegistrationId, kTestRegionId, test_region_));
}

TEST_F(GeofencingManagerTest, UnregisterRegion_NoService) {
  EXPECT_EQ(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            UnregisterRegionSync(
                kTestServiceWorkerRegistrationId, kTestRegionId, false));
}

TEST_F(GeofencingManagerTest, GetRegisteredRegions_NoService) {
  RegionMap regions;
  EXPECT_EQ(GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            manager_->GetRegisteredRegions(kTestServiceWorkerRegistrationId,
                                           &regions));
  EXPECT_TRUE(regions.empty());
}

TEST_F(GeofencingManagerTest, RegisterRegion_FailsInService) {
  SetHasProviderForTests();
  EXPECT_EQ(
      GEOFENCING_STATUS_ERROR,
      RegisterRegionSyncWithServiceResult(kTestServiceWorkerRegistrationId,
                                          kTestRegionId,
                                          test_region_,
                                          GEOFENCING_STATUS_ERROR,
                                          -1));
}

TEST_F(GeofencingManagerTest, RegisterRegion_SucceedsInService) {
  SetHasProviderForTests();
  EXPECT_EQ(
      GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithServiceResult(kTestServiceWorkerRegistrationId,
                                          kTestRegionId,
                                          test_region_,
                                          GEOFENCING_STATUS_OK,
                                          kTestGeofencingRegistrationId));
  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);
}

TEST_F(GeofencingManagerTest, RegisterRegion_AlreadyRegistered) {
  SetHasProviderForTests();
  EXPECT_EQ(
      GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithServiceResult(kTestServiceWorkerRegistrationId,
                                          kTestRegionId,
                                          test_region_,
                                          GEOFENCING_STATUS_OK,
                                          kTestGeofencingRegistrationId));
  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);

  WebCircularGeofencingRegion region2;
  region2.latitude = 43.2;
  region2.longitude = 1.45;
  region2.radius = 8.5;
  EXPECT_EQ(GEOFENCING_STATUS_ERROR,
            RegisterRegionSync(
                kTestServiceWorkerRegistrationId, kTestRegionId, region2));
  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);
}

TEST_F(GeofencingManagerTest, UnregisterRegion_NotRegistered) {
  SetHasProviderForTests();
  EXPECT_EQ(GEOFENCING_STATUS_UNREGISTRATION_FAILED_NOT_REGISTERED,
            UnregisterRegionSync(
                kTestServiceWorkerRegistrationId, kTestRegionId, false));
}

TEST_F(GeofencingManagerTest, UnregisterRegion_Success) {
  SetHasProviderForTests();

  EXPECT_EQ(
      GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithServiceResult(kTestServiceWorkerRegistrationId,
                                          kTestRegionId,
                                          test_region_,
                                          GEOFENCING_STATUS_OK,
                                          kTestGeofencingRegistrationId));

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            UnregisterRegionSync(kTestServiceWorkerRegistrationId,
                                 kTestRegionId,
                                 true,
                                 kTestGeofencingRegistrationId));
  VerifyRegions(kTestServiceWorkerRegistrationId, RegionMap());
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
      kTestServiceWorkerRegistrationId,
      kTestRegionId,
      test_region_,
      base::Bind(&StatusCatcher::Done, base::Unretained(&result)));

  // At this point the manager should have tried registering the region with
  // the service, resulting in |delegate| being set. Until the callback is
  // called the registration is not complete though.
  EXPECT_NE(delegate, nullptr);
  VerifyRegions(kTestServiceWorkerRegistrationId, RegionMap());

  // Now call the callback, and verify the registration completed succesfully.
  delegate->RegistrationFinished(kTestGeofencingRegistrationId,
                                 GEOFENCING_STATUS_OK);
  EXPECT_EQ(GEOFENCING_STATUS_OK, result.Wait());
  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);
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
      kTestServiceWorkerRegistrationId,
      kTestRegionId,
      test_region_,
      base::Bind(&StatusCatcher::Done, base::Unretained(&result)));

  // At this point the manager should have tried registering the region with
  // the service, resulting in |delegate| being set. Until the callback is
  // called the registration is not complete though.
  EXPECT_NE(delegate, nullptr);

  EXPECT_EQ(GEOFENCING_STATUS_UNREGISTRATION_FAILED_NOT_REGISTERED,
            UnregisterRegionSync(
                kTestServiceWorkerRegistrationId, kTestRegionId, false));
}

TEST_F(GeofencingManagerTest, GetRegisteredRegions_NoRegions) {
  SetHasProviderForTests();
  VerifyRegions(kTestServiceWorkerRegistrationId, RegionMap());
}

TEST_F(GeofencingManagerTest, RegisterRegion_SeparateServiceWorkers) {
  SetHasProviderForTests();

  EXPECT_EQ(
      GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithServiceResult(kTestServiceWorkerRegistrationId,
                                          kTestRegionId,
                                          test_region_,
                                          GEOFENCING_STATUS_OK,
                                          kTestGeofencingRegistrationId));

  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);
  VerifyRegions(kTestServiceWorkerRegistrationId2, RegionMap());

  EXPECT_EQ(
      GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithServiceResult(kTestServiceWorkerRegistrationId2,
                                          kTestRegionId,
                                          test_region_,
                                          GEOFENCING_STATUS_OK,
                                          kTestGeofencingRegistrationId2));

  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);
  VerifyRegions(kTestServiceWorkerRegistrationId2, expected_regions_);
}

TEST_F(GeofencingManagerTest, UnregisterRegion_SeparateServiceWorkers) {
  SetHasProviderForTests();

  EXPECT_EQ(
      GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithServiceResult(kTestServiceWorkerRegistrationId,
                                          kTestRegionId,
                                          test_region_,
                                          GEOFENCING_STATUS_OK,
                                          kTestGeofencingRegistrationId));
  EXPECT_EQ(
      GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithServiceResult(kTestServiceWorkerRegistrationId2,
                                          kTestRegionId,
                                          test_region_,
                                          GEOFENCING_STATUS_OK,
                                          kTestGeofencingRegistrationId2));

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            UnregisterRegionSync(kTestServiceWorkerRegistrationId,
                                 kTestRegionId,
                                 true,
                                 kTestGeofencingRegistrationId));

  VerifyRegions(kTestServiceWorkerRegistrationId, RegionMap());
  VerifyRegions(kTestServiceWorkerRegistrationId2, expected_regions_);

  EXPECT_EQ(GEOFENCING_STATUS_OK,
            UnregisterRegionSync(kTestServiceWorkerRegistrationId2,
                                 kTestRegionId,
                                 true,
                                 kTestGeofencingRegistrationId2));

  VerifyRegions(kTestServiceWorkerRegistrationId, RegionMap());
  VerifyRegions(kTestServiceWorkerRegistrationId2, RegionMap());
}

TEST_F(GeofencingManagerTest, ShutdownCleansRegistrations) {
  SetHasProviderForTests();
  scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner());
  EXPECT_EQ(
      GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithServiceResult(kTestServiceWorkerRegistrationId,
                                          kTestRegionId,
                                          test_region_,
                                          GEOFENCING_STATUS_OK,
                                          kTestGeofencingRegistrationId));

  EXPECT_CALL(*service_, UnregisterRegion(kTestGeofencingRegistrationId))
      .WillOnce(QuitRunner(runner));
  manager_->Shutdown();
  runner->Run();
}

}  // namespace content
