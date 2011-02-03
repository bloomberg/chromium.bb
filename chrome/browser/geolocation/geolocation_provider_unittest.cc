// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_provider.h"

#include "base/singleton.h"
#include "chrome/browser/geolocation/arbitrator_dependency_factories_for_test.h"
#include "chrome/browser/geolocation/fake_access_token_store.h"
#include "chrome/browser/geolocation/location_arbitrator.h"
#include "chrome/browser/geolocation/mock_location_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class GeolocationProviderTest : public testing::Test {
 protected:
  GeolocationProviderTest()
      : provider_(new GeolocationProvider),
        dependency_factory_(
            new GeolocationArbitratorDependencyFactoryWithLocationProvider(
                &NewAutoSuccessMockNetworkLocationProvider))
  {
  }

  ~GeolocationProviderTest() {
    DefaultSingletonTraits<GeolocationProvider>::Delete(provider_);
  }

  // testing::Test
  virtual void SetUp() {
    GeolocationArbitrator::SetDependencyFactoryForTest(
        dependency_factory_.get());
  }

  // testing::Test
  virtual void TearDown() {
    provider_->Stop();
    GeolocationArbitrator::SetDependencyFactoryForTest(NULL);
  }

  // Message loop for main thread, as provider depends on it existing.
  MessageLoop message_loop_;

  // Object under tests. Owned, but not a scoped_ptr due to specialized
  // destruction protocol.
  GeolocationProvider* provider_;

  scoped_refptr<GeolocationArbitratorDependencyFactory> dependency_factory_;
};

// Regression test for http://crbug.com/59377
TEST_F(GeolocationProviderTest, OnPermissionGrantedWithoutObservers) {
  EXPECT_FALSE(provider_->HasPermissionBeenGranted());
  provider_->OnPermissionGranted(GURL("http://example.com"));
  EXPECT_TRUE(provider_->HasPermissionBeenGranted());
}

class NullGeolocationObserver : public GeolocationObserver {
 public:
  // GeolocationObserver
  virtual void OnLocationUpdate(const Geoposition& position) {}
};

class StartStopMockLocationProvider : public MockLocationProvider {
 public:
  explicit StartStopMockLocationProvider(MessageLoop* test_loop)
      : MockLocationProvider(&instance_),
        test_loop_(test_loop) {
  }

  virtual bool StartProvider(bool high_accuracy) {
    bool result = MockLocationProvider::StartProvider(high_accuracy);
    test_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    return result;
  }

  virtual void StopProvider() {
    MockLocationProvider::StopProvider();
    test_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

 private:
  MessageLoop* test_loop_;
};

class MockDependencyFactory : public GeolocationArbitratorDependencyFactory {
 public:
  MockDependencyFactory(MessageLoop* test_loop_,
                        AccessTokenStore* access_token_store)
      : test_loop_(test_loop_),
        access_token_store_(access_token_store) {
  }

  virtual URLRequestContextGetter* GetContextGetter() {
    return NULL;
  }

  virtual GeolocationArbitrator::GetTimeNow GetTimeFunction() {
    return base::Time::Now;
  }

  virtual AccessTokenStore* NewAccessTokenStore() {
    return access_token_store_.get();
  }

  virtual LocationProviderBase* NewNetworkLocationProvider(
      AccessTokenStore* access_token_store,
      URLRequestContextGetter* context,
      const GURL& url,
      const string16& access_token) {
    return new StartStopMockLocationProvider(test_loop_);
  }

  virtual LocationProviderBase* NewSystemLocationProvider() {
    return NULL;
  }

 private:
  MessageLoop* test_loop_;
  scoped_refptr<AccessTokenStore> access_token_store_;
};

TEST_F(GeolocationProviderTest, StartStop) {
  scoped_refptr<FakeAccessTokenStore> fake_access_token_store =
      new FakeAccessTokenStore;
  scoped_refptr<GeolocationArbitratorDependencyFactory> dependency_factory =
      new MockDependencyFactory(&message_loop_, fake_access_token_store.get());

  GeolocationArbitrator::SetDependencyFactoryForTest(dependency_factory.get());

  EXPECT_FALSE(provider_->IsRunning());
  NullGeolocationObserver null_observer;
  GeolocationObserverOptions options;
  provider_->AddObserver(&null_observer, options);
  // The GeolocationArbitrator won't start the providers until it has
  // finished loading access tokens.
  fake_access_token_store->NotifyDelegateTokensLoaded();
  EXPECT_TRUE(provider_->IsRunning());
  message_loop_.Run();
  EXPECT_EQ(MockLocationProvider::instance_->state_,
            MockLocationProvider::LOW_ACCURACY);
  provider_->RemoveObserver(&null_observer);
  message_loop_.Run();
  EXPECT_EQ(MockLocationProvider::instance_->state_,
            MockLocationProvider::STOPPED);
  EXPECT_TRUE(provider_->IsRunning());

  GeolocationArbitrator::SetDependencyFactoryForTest(NULL);
}

}  // namespace
