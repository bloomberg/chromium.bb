// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/geolocation/location_arbitrator.h"
#include "chrome/browser/geolocation/mock_location_provider.h"
#include "chrome/common/chrome_switches.h"

class GeolocationApiTest : public ExtensionApiTest {
public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableGeolocation);
    GeolocationArbitrator::SetProviderFactoryForTest(
        &NewAutoSuccessMockLocationProvider);
  }
};

IN_PROC_BROWSER_TEST_F(GeolocationApiTest, ExtensionGeolocationAccessFail) {
  // Test that geolocation cannot be accessed from extension.
  ASSERT_TRUE(RunExtensionTest("geolocation")) << message_;
}
