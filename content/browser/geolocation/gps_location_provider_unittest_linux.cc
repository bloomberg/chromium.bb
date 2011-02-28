// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_thread.h"
#include "content/browser/geolocation/gps_location_provider_linux.h"
#include "content/browser/geolocation/libgps_wrapper_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

struct gps_data_t {
};

namespace {
class MockLibGps : public LibGps {
 public:
  MockLibGps();
  ~MockLibGps();

  virtual bool StartStreaming() {
    ++start_streaming_calls_;
    return start_streaming_ret_;
  }
  virtual bool DataWaiting() {
    EXPECT_GT(start_streaming_calls_, 0);
    ++data_waiting_calls_;
    // Toggle the return value, so the poll loop will exit once per test step.
    return (data_waiting_calls_ & 1) != 0;
  }
  virtual bool GetPositionIfFixed(Geoposition* position) {
    CHECK(position);
    EXPECT_GT(start_streaming_calls_, 0);
    EXPECT_GT(data_waiting_calls_, 0);
    ++get_position_calls_;
    *position = get_position_;
    return get_position_ret_;

  }
  int start_streaming_calls_;
  bool start_streaming_ret_;
  int data_waiting_calls_;
  int get_position_calls_;
  Geoposition get_position_;
  bool get_position_ret_;
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
    return new MockLibGps;
  }
  static LibGps* NoLibGpsFactory() {
    return NULL;
  }

 protected:
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  LocaionProviderListenerLoopQuitter location_listener_;
  scoped_ptr<GpsLocationProviderLinux> provider_;
};

gps_data_t* gps_open_stub(const char*, const char*) {
  // Need to return a non-NULL value here to indicate success, however we don't
  // need (or want) a valid pointer as it should never be dereferenced.
  return static_cast<gps_data_t*>(NULL) + 1;
}
int gps_close_stub(gps_data_t*) {
  return 0;
}
int gps_poll_stub(gps_data_t*) {
  return 0;
}
// v2.34 only
int gps_query_stub(gps_data_t*, const char*, ...) {
  return 0;
}
// v2.90+
int gps_stream_stub(gps_data_t*, unsigned int, void*) {
  return 0;
}
bool gps_waiting_stub(gps_data_t*) {
  return 0;
}

void CheckValidPosition(const Geoposition& expected,
                        const Geoposition& actual) {
  EXPECT_TRUE(actual.IsValidFix());
  EXPECT_DOUBLE_EQ(expected.latitude, actual.latitude);
  EXPECT_DOUBLE_EQ(expected.longitude, actual.longitude);
  EXPECT_DOUBLE_EQ(expected.accuracy, actual.accuracy);
}

MockLibGps* MockLibGps::g_instance_ = NULL;

MockLibGps::MockLibGps()
    : LibGps(new LibGpsLibraryWrapper(NULL,
                                     gps_open_stub,
                                     gps_close_stub,
                                     gps_poll_stub,
                                     gps_query_stub,
                                     gps_stream_stub,
                                     gps_waiting_stub)),
      start_streaming_calls_(0),
      start_streaming_ret_(true),
      data_waiting_calls_(0),
      get_position_calls_(0),
      get_position_ret_(true) {
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

TEST_F(GeolocationGpsProviderLinuxTests, GetPosition) {
  ASSERT_TRUE(provider_.get());
  const bool ok = provider_->StartProvider(true);
  EXPECT_TRUE(ok);
  ASSERT_TRUE(MockLibGps::g_instance_);
  EXPECT_EQ(0, MockLibGps::g_instance_->start_streaming_calls_);
  EXPECT_EQ(0, MockLibGps::g_instance_->data_waiting_calls_);
  EXPECT_EQ(0, MockLibGps::g_instance_->get_position_calls_);
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
  EXPECT_GT(MockLibGps::g_instance_->start_streaming_calls_, 0);
  EXPECT_GT(MockLibGps::g_instance_->data_waiting_calls_, 0);
  EXPECT_EQ(1, MockLibGps::g_instance_->get_position_calls_);
  provider_->GetPosition(&position);
  CheckValidPosition(MockLibGps::g_instance_->get_position_, position);

  // Movement. This will block for up to half a second.
  MockLibGps::g_instance_->get_position_.latitude += 0.01;
  MessageLoop::current()->Run();
  provider_->GetPosition(&position);
  EXPECT_EQ(2, MockLibGps::g_instance_->get_position_calls_);
  CheckValidPosition(MockLibGps::g_instance_->get_position_, position);
}

// TODO(joth): Add a test for LibGps::Start() returning false (i.e. gpsd not
// running).  Need to work around the 10s reconnect delay (either by injecting
// a shorter retry interval, or adapt MessageLoop / Time::Now to be more test
// friendly).

}  // namespace
