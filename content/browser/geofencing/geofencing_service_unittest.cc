// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "content/browser/geofencing/geofencing_provider.h"
#include "content/browser/geofencing/geofencing_registration_delegate.h"
#include "content/browser/geofencing/geofencing_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"

using blink::WebCircularGeofencingRegion;

namespace {

bool RegionsMatch(const WebCircularGeofencingRegion& expected,
                  const WebCircularGeofencingRegion& arg) {
  return testing::Matches(expected.latitude)(arg.latitude) &&
         testing::Matches(expected.longitude)(arg.longitude) &&
         testing::Matches(expected.radius)(arg.radius);
}

}  // namespace

namespace content {

class MockGeofencingRegistrationDelegate
    : public GeofencingRegistrationDelegate {
 public:
  MOCK_METHOD2(RegistrationFinished,
               void(int64_t geofencing_registration_id,
                    GeofencingStatus status));
  MOCK_METHOD1(RegionEntered, void(int64_t geofencing_registration_id));
  MOCK_METHOD1(RegionExited, void(int64_t geofencing_registration_id));
};

class MockGeofencingProvider : public GeofencingProvider {
 public:
  MOCK_METHOD3(RegisterRegion,
               void(int64_t geofencing_registration_id,
                    const blink::WebCircularGeofencingRegion& region,
                    const StatusCallback& callback));
  MOCK_METHOD1(UnregisterRegion, void(int64_t geofencing_registration_id));
};

ACTION_P(QuitRunner, runner) {
  runner->Quit();
}

ACTION_P(SaveRegistrationId, geofencing_registration_id) {
  *geofencing_registration_id = arg0;
}

ACTION_P(SaveStatusCallback, callback) {
  *callback = arg2;
}

MATCHER_P(WebCircularGeofencingRegionEq, expected, "") {
  return RegionsMatch(expected, arg);
}

class GeofencingServiceTest : public testing::Test {
 public:
  GeofencingServiceTest() : service_(nullptr) {
    test_region_.latitude = 37.421999;
    test_region_.longitude = -122.084015;
    test_region_.radius = 100;
  }

  void SetUp() override { service_ = new GeofencingServiceImpl(); }

  void TearDown() override { delete service_; }

  void SetProviderForTests() {
    provider_ = new MockGeofencingProvider();
    service_->SetProviderForTesting(make_scoped_ptr(provider_));
  }

  int RegistrationCount() { return service_->RegistrationCountForTesting(); }

  int64_t RegisterRegionSync(const WebCircularGeofencingRegion& region,
                             GeofencingStatus provider_status) {
    scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner());

    // The registration ID that is passed to the provider.
    int64_t provider_registration_id = -1;
    // The callback that is passed to the provider.
    GeofencingProvider::StatusCallback callback;

    EXPECT_CALL(
        *provider_,
        RegisterRegion(
            testing::_, WebCircularGeofencingRegionEq(region), testing::_))
        .WillOnce(testing::DoAll(SaveRegistrationId(&provider_registration_id),
                                 SaveStatusCallback(&callback)));

    int64_t geofencing_registration_id =
        service_->RegisterRegion(region, &delegate_);

    // Service should have synchronously called the provider.
    CHECK(!callback.is_null());
    CHECK(provider_registration_id == geofencing_registration_id);

    // Finish up registration by calling the callback and waiting for the
    // delegate to be called.
    EXPECT_CALL(
        delegate_,
        RegistrationFinished(geofencing_registration_id, provider_status))
        .WillOnce(QuitRunner(runner));
    callback.Run(provider_status);
    runner->Run();
    return geofencing_registration_id;
  }

 protected:
  TestBrowserThreadBundle threads_;
  GeofencingServiceImpl* service_;
  MockGeofencingProvider* provider_;
  MockGeofencingRegistrationDelegate delegate_;

  WebCircularGeofencingRegion test_region_;
};

TEST_F(GeofencingServiceTest, RegisterRegion_NoProvider) {
  scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner());
  int64_t geofencing_registration_id =
      service_->RegisterRegion(test_region_, &delegate_);
  EXPECT_CALL(delegate_,
              RegistrationFinished(
                  geofencing_registration_id,
                  GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE))
      .WillOnce(QuitRunner(runner));
  runner->Run();
  EXPECT_EQ(0, RegistrationCount());
}

TEST_F(GeofencingServiceTest, RegisterRegion_FailsInProvider) {
  SetProviderForTests();
  RegisterRegionSync(test_region_, GEOFENCING_STATUS_ERROR);
  EXPECT_EQ(0, RegistrationCount());
}

TEST_F(GeofencingServiceTest, RegisterRegion_SucceedsInProvider) {
  SetProviderForTests();
  RegisterRegionSync(test_region_, GEOFENCING_STATUS_OK);
  EXPECT_EQ(1, RegistrationCount());
}

TEST_F(GeofencingServiceTest, UnregisterRegion_AfterRegistration) {
  SetProviderForTests();
  int geofencing_registration_id =
      RegisterRegionSync(test_region_, GEOFENCING_STATUS_OK);
  EXPECT_EQ(1, RegistrationCount());

  EXPECT_CALL(*provider_, UnregisterRegion(geofencing_registration_id));
  service_->UnregisterRegion(geofencing_registration_id);
  EXPECT_EQ(0, RegistrationCount());
}

TEST_F(GeofencingServiceTest, UnregisterRegion_DuringSuccesfullRegistration) {
  SetProviderForTests();
  scoped_refptr<MessageLoopRunner> runner(new MessageLoopRunner());

  // The callback that is passed to the provider.
  GeofencingProvider::StatusCallback callback;

  EXPECT_CALL(
      *provider_,
      RegisterRegion(
          testing::_, WebCircularGeofencingRegionEq(test_region_), testing::_))
      .WillOnce(SaveStatusCallback(&callback));

  int64_t geofencing_registration_id =
      service_->RegisterRegion(test_region_, &delegate_);

  // Service should have synchronously called the provider.
  CHECK(!callback.is_null());

  // Call unregister before registration is finished.
  service_->UnregisterRegion(geofencing_registration_id);

  // Finish up registration by calling the callback and waiting for the
  // provider to be called. The delegate should not be called in this case.
  EXPECT_CALL(delegate_, RegistrationFinished(testing::_, testing::_)).Times(0);
  EXPECT_CALL(*provider_, UnregisterRegion(geofencing_registration_id))
      .WillOnce(QuitRunner(runner));
  callback.Run(GEOFENCING_STATUS_OK);
  runner->Run();
  EXPECT_EQ(0, RegistrationCount());
}

}  // namespace content
