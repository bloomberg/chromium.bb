// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_CHROME_ATHENA_APP_BROWSERTEST_H_
#define ATHENA_TEST_CHROME_ATHENA_APP_BROWSERTEST_H_

#include "chrome/browser/apps/app_browsertest_util.h"

namespace athena {
class Activity;

// Base class for athena tests.
//
// Note: To avoid asynchronous resource manager events, the memory pressure
// callback gets turned off at the beginning to a low memory pressure.
class AthenaAppBrowserTest : public extensions::PlatformAppBrowserTest {
 public:
  AthenaAppBrowserTest();
  virtual ~AthenaAppBrowserTest();

 protected:
  // Creates an app activity and returns after app is fully loaded.
  Activity* CreateTestAppActivity(const std::string app_id);

  // Gets an application ID for an installed test application.
  const std::string& GetTestAppID();

  // BrowserTestBase:
  virtual void SetUpOnMainThread() OVERRIDE;

 private:
  // Our created app id - after it got created and installed.
  std::string app_id_;

  DISALLOW_COPY_AND_ASSIGN(AthenaAppBrowserTest);
};

}  // namespace athena

#endif //  ATHENA_TEST_CHROME_ATHENA_APP_BROWSERTEST_H_

