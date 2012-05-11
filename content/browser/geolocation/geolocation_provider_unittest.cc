// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "content/browser/geolocation/arbitrator_dependency_factory.h"
#include "content/browser/geolocation/geolocation_provider.h"
#include "content/browser/geolocation/location_arbitrator.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/browser/geolocation/mock_location_provider.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/browser_thread.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::Geoposition;
using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;

namespace {
class NonSingletonGeolocationProvider : public GeolocationProvider {
 public:
  NonSingletonGeolocationProvider() {}

  virtual ~NonSingletonGeolocationProvider() {}
};

class StartStopMockLocationProvider : public MockLocationProvider {
 public:
  StartStopMockLocationProvider(base::WaitableEvent* event) :
      MockLocationProvider(&instance_),
      event_(event) {
  }

  virtual ~StartStopMockLocationProvider() {
    event_->Signal();
  }

 private:
  base::WaitableEvent* event_;
};

// The AccessTokenStore will be accessed from the geolocation helper thread. The
// existing FakeAccessTokenStore class cannot be used here because it is based
// on gmock and gmock is not thread-safe on Windows.
// See: http://code.google.com/p/googlemock/issues/detail?id=156
class TestingAccessTokenStore : public content::AccessTokenStore {
 public:
  TestingAccessTokenStore(base::WaitableEvent* event) : event_(event) {}

  virtual void LoadAccessTokens(const LoadAccessTokensCallbackType& callback)
        OVERRIDE {
    callback.Run(AccessTokenSet(), NULL);
    event_->Signal();
  }

  virtual void SaveAccessToken(const GURL& server_url,
                               const string16& access_token) OVERRIDE {}

 private:
  base::WaitableEvent* event_;
};

class TestingDependencyFactory
    : public DefaultGeolocationArbitratorDependencyFactory {
 public:
  TestingDependencyFactory(base::WaitableEvent* event) : event_(event) {}

  virtual content::AccessTokenStore* NewAccessTokenStore() OVERRIDE {
    return new TestingAccessTokenStore(event_);
  }

  virtual LocationProviderBase* NewNetworkLocationProvider(
      content::AccessTokenStore* access_token_store,
      net::URLRequestContextGetter* context,
      const GURL& url,
      const string16& access_token) OVERRIDE {
    return new StartStopMockLocationProvider(event_);
  }

  virtual LocationProviderBase* NewSystemLocationProvider() OVERRIDE  {
    return NULL;
  }

 private:
  base::WaitableEvent* event_;
};

class NullGeolocationObserver : public GeolocationObserver {
 public:
  // GeolocationObserver
  virtual void OnLocationUpdate(const content::Geoposition& position) {}
};

class MockGeolocationObserver : public GeolocationObserver {
 public:
  // GeolocationObserver
  MOCK_METHOD1(OnLocationUpdate, void(const content::Geoposition& position));
};

class MockGeolocationCallbackWrapper {
 public:
  MOCK_METHOD1(Callback, void(const content::Geoposition& position));
};

class GeopositionEqMatcher
    : public MatcherInterface<const content::Geoposition&> {
 public:
  explicit GeopositionEqMatcher(const content::Geoposition& expected)
      : expected_(expected) {}

  virtual bool MatchAndExplain(const content::Geoposition& actual,
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
  content::Geoposition expected_;

  DISALLOW_COPY_AND_ASSIGN(GeopositionEqMatcher);
};

Matcher<const content::Geoposition&> GeopositionEq(
    const content::Geoposition& expected) {
  return MakeMatcher(new GeopositionEqMatcher(expected));
}

class GeolocationProviderTest : public testing::Test {
 protected:
  GeolocationProviderTest()
      : message_loop_(),
        io_thread_(content::BrowserThread::IO, &message_loop_),
        event_(false, false),
        dependency_factory_(new TestingDependencyFactory(&event_)),
        provider_(new NonSingletonGeolocationProvider) {
    GeolocationArbitrator::SetDependencyFactoryForTest(
        dependency_factory_.get());
  }

  ~GeolocationProviderTest() {
    GeolocationArbitrator::SetDependencyFactoryForTest(NULL);
  }

  void WaitAndReset() {
    event_.Wait();
    event_.Reset();
  }

  MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;

  base::WaitableEvent event_;
  scoped_refptr<TestingDependencyFactory> dependency_factory_;
  scoped_ptr<NonSingletonGeolocationProvider> provider_;
};

// Regression test for http://crbug.com/59377
TEST_F(GeolocationProviderTest, OnPermissionGrantedWithoutObservers) {
  EXPECT_FALSE(provider_->HasPermissionBeenGranted());
  provider_->OnPermissionGranted();
  EXPECT_TRUE(provider_->HasPermissionBeenGranted());
}

TEST_F(GeolocationProviderTest, StartStop) {
  EXPECT_FALSE(provider_->IsRunning());
  NullGeolocationObserver null_observer;
  GeolocationObserverOptions options;
  provider_->AddObserver(&null_observer, options);
  EXPECT_TRUE(provider_->IsRunning());
  // Wait for token load request from the arbitrator to come through.
  WaitAndReset();

  EXPECT_EQ(MockLocationProvider::instance_->state_,
            MockLocationProvider::LOW_ACCURACY);
  provider_->RemoveObserver(&null_observer);
  // Wait for the providers to be stopped now that all clients are gone.
  WaitAndReset();
  EXPECT_TRUE(provider_->IsRunning());
}

TEST_F(GeolocationProviderTest, OverrideLocationForTesting) {
  content::Geoposition position;
  position.error_code = content::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
  provider_->OverrideLocationForTesting(position);
  // Adding an observer when the location is overridden should synchronously
  // update the observer with our overridden position.
  MockGeolocationObserver mock_observer;
  EXPECT_CALL(mock_observer, OnLocationUpdate(GeopositionEq(position)));
  provider_->AddObserver(&mock_observer, GeolocationObserverOptions());
  // Wait for token load request from the arbitrator to come through.
  WaitAndReset();

  provider_->RemoveObserver(&mock_observer);
  // Wait for the providers to be stopped now that all clients are gone.
  WaitAndReset();
}

TEST_F(GeolocationProviderTest, Callback) {
  MockGeolocationCallbackWrapper callback_wrapper;
  provider_->RequestCallback(
      base::Bind(&MockGeolocationCallbackWrapper::Callback,
                 base::Unretained(&callback_wrapper)));
  // Wait for token load request from the arbitrator to come through.
  WaitAndReset();

  content::Geoposition position;
  position.latitude = 12;
  position.longitude = 34;
  position.accuracy = 56;
  position.timestamp = base::Time::Now();
  EXPECT_CALL(callback_wrapper, Callback(GeopositionEq(position)));
  provider_->OverrideLocationForTesting(position);
  // Wait for the providers to be stopped now that all clients are gone.
  WaitAndReset();
}

}  // namespace
