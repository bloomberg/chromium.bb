// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/time.h"
#include "content/browser/geolocation/geolocation_provider.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/browser/geolocation/mock_location_arbitrator.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;

namespace content {

class LocationProviderForTestArbitrator : public GeolocationProvider {
 public:
  LocationProviderForTestArbitrator() : mock_arbitrator_(NULL) {}
  virtual ~LocationProviderForTestArbitrator() {}

  // Only valid for use on the geolocation thread.
  MockGeolocationArbitrator* mock_arbitrator() const {
    return mock_arbitrator_;
  }

 protected:
  // GeolocationProvider implementation:
  virtual GeolocationArbitrator* CreateArbitrator() OVERRIDE;

 private:
  MockGeolocationArbitrator* mock_arbitrator_;
};

GeolocationArbitrator* LocationProviderForTestArbitrator::CreateArbitrator() {
  DCHECK(mock_arbitrator_ == NULL);
  mock_arbitrator_ = new MockGeolocationArbitrator;
  return mock_arbitrator_;
}

class NullGeolocationObserver : public GeolocationObserver {
 public:
  // GeolocationObserver
  virtual void OnLocationUpdate(const Geoposition& position) OVERRIDE {}
};

class MockGeolocationObserver : public GeolocationObserver {
 public:
  // GeolocationObserver
  MOCK_METHOD1(OnLocationUpdate, void(const Geoposition& position));
};

class AsyncMockGeolocationObserver : public MockGeolocationObserver {
 public:
  // GeolocationObserver
  virtual void OnLocationUpdate(const Geoposition& position) OVERRIDE {
    MockGeolocationObserver::OnLocationUpdate(position);
    base::MessageLoop::current()->Quit();
  }
};

class MockGeolocationCallbackWrapper {
 public:
  MOCK_METHOD1(Callback, void(const Geoposition& position));
};

class GeopositionEqMatcher
    : public MatcherInterface<const Geoposition&> {
 public:
  explicit GeopositionEqMatcher(const Geoposition& expected)
      : expected_(expected) {}

  virtual bool MatchAndExplain(const Geoposition& actual,
                               MatchResultListener* listener) const OVERRIDE {
    return actual.latitude == expected_.latitude &&
           actual.longitude == expected_.longitude &&
           actual.altitude == expected_.altitude &&
           actual.accuracy == expected_.accuracy &&
           actual.altitude_accuracy == expected_.altitude_accuracy &&
           actual.heading == expected_.heading &&
           actual.speed == expected_.speed &&
           actual.timestamp == expected_.timestamp &&
           actual.error_code == expected_.error_code &&
           actual.error_message == expected_.error_message;
  }

  virtual void DescribeTo(::std::ostream* os) const OVERRIDE {
    *os << "which matches the expected position";
  }

  virtual void DescribeNegationTo(::std::ostream* os) const OVERRIDE{
    *os << "which does not match the expected position";
  }

 private:
  Geoposition expected_;

  DISALLOW_COPY_AND_ASSIGN(GeopositionEqMatcher);
};

Matcher<const Geoposition&> GeopositionEq(const Geoposition& expected) {
  return MakeMatcher(new GeopositionEqMatcher(expected));
}

class GeolocationProviderTest : public testing::Test {
 protected:
  GeolocationProviderTest()
      : message_loop_(),
        io_thread_(BrowserThread::IO, &message_loop_),
        provider_(new LocationProviderForTestArbitrator) {
  }

  virtual ~GeolocationProviderTest() {}

  LocationProviderForTestArbitrator* provider() { return provider_.get(); }

  // Called on test thread.
  bool ProvidersStarted();
  void SendMockLocation(const Geoposition& position);

 private:
  // Called on provider thread.
  void GetProvidersStarted(bool* started);

  base::MessageLoop message_loop_;
  TestBrowserThread io_thread_;
  scoped_ptr<LocationProviderForTestArbitrator> provider_;
};


bool GeolocationProviderTest::ProvidersStarted() {
  DCHECK(provider_->IsRunning());
  DCHECK(base::MessageLoop::current() == &message_loop_);
  bool started;
  provider_->message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GeolocationProviderTest::GetProvidersStarted,
                 base::Unretained(this),
                 &started),
      base::MessageLoop::QuitClosure());
  message_loop_.Run();
  return started;
}

void GeolocationProviderTest::GetProvidersStarted(bool* started) {
  DCHECK(base::MessageLoop::current() == provider_->message_loop());
  *started = provider_->mock_arbitrator()->providers_started();
}

void GeolocationProviderTest::SendMockLocation(const Geoposition& position) {
  DCHECK(provider_->IsRunning());
  DCHECK(base::MessageLoop::current() == &message_loop_);
  provider_->message_loop()
      ->PostTask(FROM_HERE,
                 base::Bind(&GeolocationProvider::OnLocationUpdate,
                            base::Unretained(provider_.get()),
                            position));
}

// Regression test for http://crbug.com/59377
TEST_F(GeolocationProviderTest, OnPermissionGrantedWithoutObservers) {
  EXPECT_FALSE(provider()->HasPermissionBeenGranted());
  provider()->OnPermissionGranted();
  EXPECT_TRUE(provider()->HasPermissionBeenGranted());
}

TEST_F(GeolocationProviderTest, StartStop) {
  EXPECT_FALSE(provider()->IsRunning());
  NullGeolocationObserver null_observer;
  GeolocationObserverOptions options;
  provider()->AddObserver(&null_observer, options);
  EXPECT_TRUE(provider()->IsRunning());
  EXPECT_TRUE(ProvidersStarted());

  provider()->RemoveObserver(&null_observer);
  EXPECT_FALSE(ProvidersStarted());
  EXPECT_TRUE(provider()->IsRunning());
}

TEST_F(GeolocationProviderTest, StalePositionNotSent) {
  Geoposition first_position;
  first_position.latitude = 12;
  first_position.longitude = 34;
  first_position.accuracy = 56;
  first_position.timestamp = base::Time::Now();

  AsyncMockGeolocationObserver first_observer;
  GeolocationObserverOptions options;
  EXPECT_CALL(first_observer, OnLocationUpdate(GeopositionEq(first_position)));
  provider()->AddObserver(&first_observer, options);
  SendMockLocation(first_position);
  base::MessageLoop::current()->Run();

  provider()->RemoveObserver(&first_observer);

  Geoposition second_position;
  second_position.latitude = 13;
  second_position.longitude = 34;
  second_position.accuracy = 56;
  second_position.timestamp = base::Time::Now();

  AsyncMockGeolocationObserver second_observer;

  // After adding a second observer, check that no unexpected position update
  // is sent.
  EXPECT_CALL(second_observer, OnLocationUpdate(testing::_)).Times(0);
  provider()->AddObserver(&second_observer, options);
  base::MessageLoop::current()->RunUntilIdle();

  // The second observer should receive the new position now.
  EXPECT_CALL(second_observer,
              OnLocationUpdate(GeopositionEq(second_position)));
  SendMockLocation(second_position);
  base::MessageLoop::current()->Run();

  provider()->RemoveObserver(&second_observer);
  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest, OverrideLocationForTesting) {
  Geoposition position;
  position.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  provider()->OverrideLocationForTesting(position);
  // Adding an observer when the location is overridden should synchronously
  // update the observer with our overridden position.
  MockGeolocationObserver mock_observer;
  EXPECT_CALL(mock_observer, OnLocationUpdate(GeopositionEq(position)));
  provider()->AddObserver(&mock_observer, GeolocationObserverOptions());
  provider()->RemoveObserver(&mock_observer);
  // Wait for the providers to be stopped now that all clients are gone.
  EXPECT_FALSE(ProvidersStarted());
}

TEST_F(GeolocationProviderTest, Callback) {
  MockGeolocationCallbackWrapper callback_wrapper;
  provider()->RequestCallback(
      base::Bind(&MockGeolocationCallbackWrapper::Callback,
                 base::Unretained(&callback_wrapper)));
  Geoposition position;
  position.latitude = 12;
  position.longitude = 34;
  position.accuracy = 56;
  position.timestamp = base::Time::Now();
  EXPECT_CALL(callback_wrapper, Callback(GeopositionEq(position)));
  provider()->OverrideLocationForTesting(position);
  // Wait for the providers to be stopped now that all clients are gone.
  EXPECT_FALSE(ProvidersStarted());
}

}  // namespace content
