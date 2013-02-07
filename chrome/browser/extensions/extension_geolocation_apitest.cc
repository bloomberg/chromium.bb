// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"

class GeolocationApiTest : public ExtensionApiTest {
 public:
  GeolocationApiTest() {
  }

  // InProcessBrowserTest
  virtual void SetUpOnMainThread() OVERRIDE {
    ui_test_utils::OverrideGeolocation(0, 0);
  }
};

// http://crbug.com/68287
IN_PROC_BROWSER_TEST_F(GeolocationApiTest,
                       DISABLED_ExtensionGeolocationAccessFail) {
  // Test that geolocation cannot be accessed from extension without permission.
  ASSERT_TRUE(RunExtensionTest("geolocation/no_permission")) << message_;
}

// Timing out. http://crbug.com/128412
IN_PROC_BROWSER_TEST_F(GeolocationApiTest,
                       DISABLED_ExtensionGeolocationAccessPass) {
  // Test that geolocation can be accessed from extension with permission.
  ASSERT_TRUE(RunExtensionTest("geolocation/has_permission")) << message_;
}
