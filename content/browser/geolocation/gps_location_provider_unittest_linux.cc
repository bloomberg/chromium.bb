// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_thread_impl.h"
#include "content/browser/geolocation/gps_location_provider_linux.h"
#include "content/browser/geolocation/libgps_wrapper_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"

using content::BrowserThread;
using content::BrowserThreadImpl;

namespace {
class MockLibGps : public LibGps {
 public:
  MockLibGps();
  ~MockLibGps();

  virtual bool GetPositionIfFixed(Geoposition* position) {
    CHECK(position);
    ++get_position_calls_;
    *position = get_position_;
    return get_position_ret_;
  }

  static int gps_open_stub(const char*, const char*, struct gps_data_t*) {
    CHECK(g_instance_);
    g_instance_->gps_open_calls_++;
    return g_instance_->gps_open_ret_;
  }

  static int gps_close_stub(struct gps_data_t*) {
    return 0;
  }

  static int gps_read_stub(struct gps_data_t*) {
    CHECK(g_instance_);
    g_instance_->gps_read_calls_++;
    return g_instance_->gps_read_ret_;
  }

  int get_position_calls_;
  bool get_position_ret_;
  int gps_open_calls_;
  int gps_open_ret_;
  int gps_read_calls_;
  int gps_read_ret_;
  Geoposition get_position_;
  static MockLibGps* g_instance_;
};

class LocaionProviderListenerLoopQuitter
    : public LocationProviderBase::ListenerInterface {
  // LocationProviderBase::ListenerInterface
  virtual void LocationUpdateAvailable(LocationProviderBase* provider) {
    MessageLoop::current()->Quit();
  }
};

class GeolocationGpsProviderLinuxTests : public testing::Test {
 public:
  GeolocationGpsProviderLinuxTests();
  ~GeolocationGpsProviderLinuxTests();

  static LibGps* NewMockLibGps() {
    return new MockLibGps();
  }
  static LibGps* NoLibGpsFactory() {
    return NULL;
  }

 protected:
  MessageLoop message_loop_;
  BrowserThreadImpl ui_thread_;
  LocaionProviderListenerLoopQuitter location_listener_;
  scoped_ptr<GpsLocationProviderLinux> provider_;
};

void CheckValidPosition(const Geoposition& expected,
                        const Geoposition& actual) {
  EXPECT_TRUE(actual.IsValidFix());
  EXPECT_DOUBLE_EQ(expected.latitude, actual.latitude);
  EXPECT_DOUBLE_EQ(expected.longitude, actual.longitude);
  EXPECT_DOUBLE_EQ(expected.accuracy, actual.accuracy);
}

MockLibGps* MockLibGps::g_instance_ = NULL;

MockLibGps::MockLibGps()
    : LibGps(NULL, gps_open_stub, gps_close_stub, gps_read_stub),
      get_position_calls_(0),
      get_position_ret_(true),
      gps_open_calls_(0),
      gps_open_ret_(0),
      gps_read_calls_(0),
      gps_read_ret_(0) {
  get_position_.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  EXPECT_FALSE(g_instance_);
  g_instance_ = this;
}

MockLibGps::~MockLibGps() {
  EXPECT_EQ(this, g_instance_);
  g_instance_ = NULL;
}

GeolocationGpsProviderLinuxTests::GeolocationGpsProviderLinuxTests()
    : ui_thread_(BrowserThread::IO, &message_loop_),
      provider_(new GpsLocationProviderLinux(NewMockLibGps)) {
  provider_->RegisterListener(&location_listener_);
}

GeolocationGpsProviderLinuxTests::~GeolocationGpsProviderLinuxTests() {
  provider_->UnregisterListener(&location_listener_);
}

TEST_F(GeolocationGpsProviderLinuxTests, NoLibGpsInstalled) {
  provider_.reset(new GpsLocationProviderLinux(NoLibGpsFactory));
  ASSERT_TRUE(provider_.get());
  const bool ok = provider_->StartProvider(true);
  EXPECT_FALSE(ok);
  Geoposition position;
  provider_->GetPosition(&position);
  EXPECT_TRUE(position.IsInitialized());
  EXPECT_FALSE(position.IsValidFix());
  EXPECT_EQ(Geoposition::ERROR_CODE_POSITION_UNAVAILABLE, position.error_code);
}

#if defined(OS_CHROMEOS)

TEST_F(GeolocationGpsProviderLinuxTests, GetPosition) {
  ASSERT_TRUE(provider_.get());
  const bool ok = provider_->StartProvider(true);
  EXPECT_TRUE(ok);
  ASSERT_TRUE(MockLibGps::g_instance_);
  EXPECT_EQ(0, MockLibGps::g_instance_->get_position_calls_);
  EXPECT_EQ(0, MockLibGps::g_instance_->gps_open_calls_);
  EXPECT_EQ(0, MockLibGps::g_instance_->gps_read_calls_);
  Geoposition position;
  provider_->GetPosition(&position);
  EXPECT_TRUE(position.IsInitialized());
  EXPECT_FALSE(position.IsValidFix());
  EXPECT_EQ(Geoposition::ERROR_CODE_POSITION_UNAVAILABLE, position.error_code);
  MockLibGps::g_instance_->get_position_.error_code =
      Geoposition::ERROR_CODE_NONE;
  MockLibGps::g_instance_->get_position_.latitude = 4.5;
  MockLibGps::g_instance_->get_position_.longitude = -34.1;
  MockLibGps::g_instance_->get_position_.accuracy = 345;
  MockLibGps::g_instance_->get_position_.timestamp =
      base::Time::FromDoubleT(200);
  EXPECT_TRUE(MockLibGps::g_instance_->get_position_.IsValidFix());
  MessageLoop::current()->Run();
  EXPECT_EQ(1, MockLibGps::g_instance_->get_position_calls_);
  EXPECT_EQ(1, MockLibGps::g_instance_->gps_open_calls_);
  EXPECT_EQ(1, MockLibGps::g_instance_->gps_read_calls_);
  provider_->GetPosition(&position);
  CheckValidPosition(MockLibGps::g_instance_->get_position_, position);

  // Movement. This will block for up to half a second.
  MockLibGps::g_instance_->get_position_.latitude += 0.01;
  MessageLoop::current()->Run();
  provider_->GetPosition(&position);
  EXPECT_EQ(2, MockLibGps::g_instance_->get_position_calls_);
  EXPECT_EQ(1, MockLibGps::g_instance_->gps_open_calls_);
  EXPECT_EQ(2, MockLibGps::g_instance_->gps_read_calls_);
  CheckValidPosition(MockLibGps::g_instance_->get_position_, position);
}

void EnableGpsOpenCallback() {
  CHECK(MockLibGps::g_instance_);
  MockLibGps::g_instance_->gps_open_ret_ = 0;
}

TEST_F(GeolocationGpsProviderLinuxTests, LibGpsReconnect) {
  // Setup gpsd reconnect interval to be 1000ms to speed up test.
  provider_->SetGpsdReconnectIntervalMillis(1000);
  provider_->SetPollPeriodMovingMillis(200);
  const bool ok = provider_->StartProvider(true);
  EXPECT_TRUE(ok);
  ASSERT_TRUE(MockLibGps::g_instance_);
  // Let gps_open() fails, and so will LibGps::Start().
  // Reconnect will happen in 1000ms.
  MockLibGps::g_instance_->gps_open_ret_ = 1;
  Geoposition position;
  MockLibGps::g_instance_->get_position_.error_code =
      Geoposition::ERROR_CODE_NONE;
  MockLibGps::g_instance_->get_position_.latitude = 4.5;
  MockLibGps::g_instance_->get_position_.longitude = -34.1;
  MockLibGps::g_instance_->get_position_.accuracy = 345;
  MockLibGps::g_instance_->get_position_.timestamp =
      base::Time::FromDoubleT(200);
  EXPECT_TRUE(MockLibGps::g_instance_->get_position_.IsValidFix());
  // This task makes gps_open() and LibGps::Start() to succeed after
  // 1500ms.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&EnableGpsOpenCallback), 1500);
  MessageLoop::current()->Run();
  provider_->GetPosition(&position);
  EXPECT_TRUE(position.IsInitialized());
  EXPECT_TRUE(position.IsValidFix());
  // 3 gps_open() calls are expected (2 failures and 1 success)
  EXPECT_EQ(1, MockLibGps::g_instance_->get_position_calls_);
  EXPECT_EQ(3, MockLibGps::g_instance_->gps_open_calls_);
  EXPECT_EQ(1, MockLibGps::g_instance_->gps_read_calls_);
}

#endif  // #if defined(OS_CHROMEOS)

}  // namespace
