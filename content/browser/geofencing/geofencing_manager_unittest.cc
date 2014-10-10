// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/geofencing/geofencing_manager.h"
#include "content/browser/geofencing/geofencing_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"
#include "url/gurl.h"

using blink::WebCircularGeofencingRegion;
typedef std::map<std::string, WebCircularGeofencingRegion> RegionMap;

namespace {

static const char* kTestRegionId = "region-id";
static const int64 kTestServiceWorkerRegistrationId = 123;
static const int64 kTestServiceWorkerRegistrationId2 = 456;

bool RegionsMatch(const WebCircularGeofencingRegion& expected,
                  const WebCircularGeofencingRegion& arg) {
  return testing::Matches(expected.latitude)(arg.latitude) &&
         testing::Matches(expected.longitude)(arg.longitude) &&
         testing::Matches(expected.radius)(arg.radius);
}
}

namespace content {

class TestGeofencingProvider : public GeofencingProvider {
 public:
  MOCK_METHOD2(RegisterRegion,
               void(const WebCircularGeofencingRegion& region,
                    const RegisterCallback& callback));
  MOCK_METHOD1(UnregisterRegion, void(int registration_id));
};

ACTION_P2(CallRegisterCallback, status, id) {
  arg1.Run(status, id);
}

ACTION_P(SaveRegisterCallback, callback) {
  *callback = arg1;
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
  GeofencingManagerTest()
      : message_loop_(),
        io_thread_(BrowserThread::IO, &message_loop_),
        provider_(0),
        manager_(0),
        test_origin_("https://example.com/") {
    test_region_.latitude = 37.421999;
    test_region_.longitude = -122.084015;
    test_region_.radius = 100;
    expected_regions_[kTestRegionId] = test_region_;
  }

  virtual void SetUp() { manager_ = new GeofencingManager(); }

  virtual void TearDown() { delete manager_; }

  void SetProviderForTests() {
    provider_ = new TestGeofencingProvider();
    manager_->SetProviderForTests(scoped_ptr<GeofencingProvider>(provider_));
  }

  GeofencingStatus RegisterRegionSync(
      int64 service_worker_registration_id,
      const std::string& id,
      const WebCircularGeofencingRegion& region) {
    StatusCatcher result;
    manager_->RegisterRegion(
        nullptr, /* browser_context */
        service_worker_registration_id,
        test_origin_,
        id,
        region,
        base::Bind(&StatusCatcher::Done, base::Unretained(&result)));
    return result.Wait();
  }

  GeofencingStatus RegisterRegionSyncWithProviderResult(
      int64 service_worker_registration_id,
      const std::string& id,
      const WebCircularGeofencingRegion& region,
      GeofencingStatus provider_status,
      int provider_result) {
    StatusCatcher result;
    EXPECT_CALL(
        *provider_,
        RegisterRegion(WebCircularGeofencingRegionEq(region), testing::_))
        .WillOnce(CallRegisterCallback(provider_status, provider_result));
    manager_->RegisterRegion(
        nullptr, /* browser_context */
        service_worker_registration_id,
        test_origin_,
        id,
        region,
        base::Bind(&StatusCatcher::Done, base::Unretained(&result)));
    return result.Wait();
  }

  GeofencingStatus UnregisterRegionSync(int64 service_worker_registration_id,
                                        const std::string& id,
                                        bool should_call_provider,
                                        int provider_id = 0) {
    StatusCatcher result;
    if (should_call_provider) {
      EXPECT_CALL(*provider_, UnregisterRegion(provider_id));
    }
    manager_->UnregisterRegion(
        nullptr, /* browser_context */
        service_worker_registration_id,
        test_origin_,
        id,
        base::Bind(&StatusCatcher::Done, base::Unretained(&result)));
    return result.Wait();
  }

  void VerifyRegions(int64 service_worker_registration_id,
                     const RegionMap& expected_regions) {
    RegionMap regions;
    EXPECT_EQ(GeofencingStatus::GEOFENCING_STATUS_OK,
              manager_->GetRegisteredRegions(nullptr, /* browser_context */
                                             service_worker_registration_id,
                                             test_origin_,
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
  base::MessageLoop message_loop_;
  TestBrowserThread io_thread_;
  TestGeofencingProvider* provider_;
  GeofencingManager* manager_;

  WebCircularGeofencingRegion test_region_;
  RegionMap expected_regions_;
  GURL test_origin_;
};

TEST_F(GeofencingManagerTest, RegisterRegion_NoProvider) {
  EXPECT_EQ(GeofencingStatus::
                GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            RegisterRegionSync(
                kTestServiceWorkerRegistrationId, kTestRegionId, test_region_));
}

TEST_F(GeofencingManagerTest, UnregisterRegion_NoProvider) {
  EXPECT_EQ(GeofencingStatus::
                GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            UnregisterRegionSync(
                kTestServiceWorkerRegistrationId, kTestRegionId, false));
}

TEST_F(GeofencingManagerTest, GetRegisteredRegions_NoProvider) {
  RegionMap regions;
  EXPECT_EQ(GeofencingStatus::
                GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
            manager_->GetRegisteredRegions(nullptr, /* browser_context */
                                           kTestServiceWorkerRegistrationId,
                                           test_origin_,
                                           &regions));
  EXPECT_TRUE(regions.empty());
}

TEST_F(GeofencingManagerTest, RegisterRegion_FailsInProvider) {
  SetProviderForTests();
  EXPECT_EQ(
      GeofencingStatus::GEOFENCING_STATUS_ERROR,
      RegisterRegionSyncWithProviderResult(kTestServiceWorkerRegistrationId,
                                           kTestRegionId,
                                           test_region_,
                                           GEOFENCING_STATUS_ERROR,
                                           -1));
}

TEST_F(GeofencingManagerTest, RegisterRegion_SucceedsInProvider) {
  SetProviderForTests();
  EXPECT_EQ(
      GeofencingStatus::GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithProviderResult(kTestServiceWorkerRegistrationId,
                                           kTestRegionId,
                                           test_region_,
                                           GEOFENCING_STATUS_OK,
                                           0));
  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);
}

TEST_F(GeofencingManagerTest, RegisterRegion_AlreadyRegistered) {
  SetProviderForTests();
  EXPECT_EQ(
      GeofencingStatus::GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithProviderResult(kTestServiceWorkerRegistrationId,
                                           kTestRegionId,
                                           test_region_,
                                           GEOFENCING_STATUS_OK,
                                           0));
  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);

  WebCircularGeofencingRegion region2;
  region2.latitude = 43.2;
  region2.longitude = 1.45;
  region2.radius = 8.5;
  EXPECT_EQ(GeofencingStatus::GEOFENCING_STATUS_ERROR,
            RegisterRegionSync(
                kTestServiceWorkerRegistrationId, kTestRegionId, region2));
  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);
}

TEST_F(GeofencingManagerTest, UnregisterRegion_NotRegistered) {
  SetProviderForTests();
  EXPECT_EQ(GeofencingStatus::GEOFENCING_STATUS_ERROR,
            UnregisterRegionSync(
                kTestServiceWorkerRegistrationId, kTestRegionId, false));
}

TEST_F(GeofencingManagerTest, UnregisterRegion_Success) {
  SetProviderForTests();
  int provider_id = 123;

  EXPECT_EQ(
      GeofencingStatus::GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithProviderResult(kTestServiceWorkerRegistrationId,
                                           kTestRegionId,
                                           test_region_,
                                           GEOFENCING_STATUS_OK,
                                           provider_id));

  EXPECT_EQ(
      GeofencingStatus::GEOFENCING_STATUS_OK,
      UnregisterRegionSync(
          kTestServiceWorkerRegistrationId, kTestRegionId, true, provider_id));
  VerifyRegions(kTestServiceWorkerRegistrationId, RegionMap());
}

TEST_F(GeofencingManagerTest, GetRegisteredRegions_RegistrationInProgress) {
  SetProviderForTests();
  StatusCatcher result;
  GeofencingProvider::RegisterCallback callback;

  EXPECT_CALL(
      *provider_,
      RegisterRegion(WebCircularGeofencingRegionEq(test_region_), testing::_))
      .WillOnce(SaveRegisterCallback(&callback));
  manager_->RegisterRegion(
      nullptr, /* browser_context */
      kTestServiceWorkerRegistrationId,
      test_origin_,
      kTestRegionId,
      test_region_,
      base::Bind(&StatusCatcher::Done, base::Unretained(&result)));

  // At this point the manager should have tried registering the region with
  // the provider, resulting in |callback| being set. Until the callback is
  // called the registration is not complete though.
  EXPECT_FALSE(callback.is_null());
  VerifyRegions(kTestServiceWorkerRegistrationId, RegionMap());

  // Now call the callback, and verify the registration completed succesfully.
  callback.Run(GEOFENCING_STATUS_OK, 123);
  EXPECT_EQ(GeofencingStatus::GEOFENCING_STATUS_OK, result.Wait());
  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);
}

TEST_F(GeofencingManagerTest, UnregisterRegion_RegistrationInProgress) {
  SetProviderForTests();
  StatusCatcher result;
  GeofencingProvider::RegisterCallback callback;

  EXPECT_CALL(
      *provider_,
      RegisterRegion(WebCircularGeofencingRegionEq(test_region_), testing::_))
      .WillOnce(SaveRegisterCallback(&callback));
  manager_->RegisterRegion(
      nullptr, /* browser_context */
      kTestServiceWorkerRegistrationId,
      test_origin_,
      kTestRegionId,
      test_region_,
      base::Bind(&StatusCatcher::Done, base::Unretained(&result)));

  // At this point the manager should have tried registering the region with
  // the provider, resulting in |callback| being set. Until the callback is
  // called the registration is not complete though.
  EXPECT_FALSE(callback.is_null());

  EXPECT_EQ(GeofencingStatus::GEOFENCING_STATUS_ERROR,
            UnregisterRegionSync(
                kTestServiceWorkerRegistrationId, kTestRegionId, false));
}

TEST_F(GeofencingManagerTest, GetRegisteredRegions_NoRegions) {
  SetProviderForTests();
  VerifyRegions(kTestServiceWorkerRegistrationId, RegionMap());
}

TEST_F(GeofencingManagerTest, RegisterRegion_SeparateServiceWorkers) {
  SetProviderForTests();
  int provider_id1 = 12;
  int provider_id2 = 34;

  EXPECT_EQ(
      GeofencingStatus::GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithProviderResult(kTestServiceWorkerRegistrationId,
                                           kTestRegionId,
                                           test_region_,
                                           GEOFENCING_STATUS_OK,
                                           provider_id1));

  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);
  VerifyRegions(kTestServiceWorkerRegistrationId2, RegionMap());

  EXPECT_EQ(
      GeofencingStatus::GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithProviderResult(kTestServiceWorkerRegistrationId2,
                                           kTestRegionId,
                                           test_region_,
                                           GEOFENCING_STATUS_OK,
                                           provider_id2));

  VerifyRegions(kTestServiceWorkerRegistrationId, expected_regions_);
  VerifyRegions(kTestServiceWorkerRegistrationId2, expected_regions_);
}

TEST_F(GeofencingManagerTest, UnregisterRegion_SeparateServiceWorkers) {
  SetProviderForTests();
  int provider_id1 = 12;
  int provider_id2 = 34;

  EXPECT_EQ(
      GeofencingStatus::GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithProviderResult(kTestServiceWorkerRegistrationId,
                                           kTestRegionId,
                                           test_region_,
                                           GEOFENCING_STATUS_OK,
                                           provider_id1));
  EXPECT_EQ(
      GeofencingStatus::GEOFENCING_STATUS_OK,
      RegisterRegionSyncWithProviderResult(kTestServiceWorkerRegistrationId2,
                                           kTestRegionId,
                                           test_region_,
                                           GEOFENCING_STATUS_OK,
                                           provider_id2));

  EXPECT_EQ(
      GeofencingStatus::GEOFENCING_STATUS_OK,
      UnregisterRegionSync(
          kTestServiceWorkerRegistrationId, kTestRegionId, true, provider_id1));

  VerifyRegions(kTestServiceWorkerRegistrationId, RegionMap());
  VerifyRegions(kTestServiceWorkerRegistrationId2, expected_regions_);

  EXPECT_EQ(GeofencingStatus::GEOFENCING_STATUS_OK,
            UnregisterRegionSync(kTestServiceWorkerRegistrationId2,
                                 kTestRegionId,
                                 true,
                                 provider_id2));

  VerifyRegions(kTestServiceWorkerRegistrationId, RegionMap());
  VerifyRegions(kTestServiceWorkerRegistrationId2, RegionMap());
}

}  // namespace content
