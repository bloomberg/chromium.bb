// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/geolocation/location_arbitrator.h"
#include "chrome/browser/geolocation/mock_location_provider.h"
#include "chrome/browser/geolocation/arbitrator_dependency_factories_for_test.h"

class GeolocationApiTest : public ExtensionApiTest {
 public:
  GeolocationApiTest()
      : dependency_factory_(
          new GeolocationArbitratorDependencyFactoryWithLocationProvider(
              &NewAutoSuccessMockLocationProvider)) {
  }

  // InProcessBrowserTest
  virtual void SetUpInProcessBrowserTestFixture() {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    GeolocationArbitrator::SetDependencyFactoryForTest(
        dependency_factory_.get());
  }

  // InProcessBrowserTest
  virtual void TearDownInProcessBrowserTestFixture() {
    GeolocationArbitrator::SetDependencyFactoryForTest(NULL);
  }

 private:
  scoped_refptr<GeolocationArbitratorDependencyFactory> dependency_factory_;
};

IN_PROC_BROWSER_TEST_F(GeolocationApiTest,
                       FLAKY_ExtensionGeolocationAccessFail) {
  // Test that geolocation cannot be accessed from extension without permission.
  ASSERT_TRUE(RunExtensionTest("geolocation/no_permission")) << message_;
}

IN_PROC_BROWSER_TEST_F(GeolocationApiTest, ExtensionGeolocationAccessPass) {
  // Test that geolocation can be accessed from extension with permission.
  ASSERT_TRUE(RunExtensionTest("geolocation/has_permission")) << message_;
}
