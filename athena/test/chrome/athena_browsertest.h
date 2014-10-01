// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_CHROME_ATHENA_BROWSERTEST_H_
#define ATHENA_TEST_CHROME_ATHENA_BROWSERTEST_H_

#include "athena/resource_manager/public/resource_manager.h"
#include "chrome/test/base/in_process_browser_test.h"

class GURL;

namespace athena {
class Activity;

// Base class for athena tests.
//
// Note: To avoid asynchronous resource manager events, the memory pressure
// callback gets turned off at the beginning to a low memory pressure.
class AthenaBrowserTest : public InProcessBrowserTest {
 public:
  AthenaBrowserTest();
  virtual ~AthenaBrowserTest();

  // Configures everything for an in process browser test, then invokes
  // BrowserMain. BrowserMain ends up invoking RunTestOnMainThreadLoop.
  virtual void SetUp() OVERRIDE;

 protected:
  // Sends a memory pressure event to the resource manager with a new |pressure|
  // level. turning off asynchronous pressure changed events.
  void SendMemoryPressureEvent(ResourceManager::MemoryPressure pressure);

  // Create a new web activity and return after the page is fully loaded.
  Activity* CreateWebActivity(const base::string16& title,
                              const GURL& url);

  // Creates an app activity and returns after app is fully loaded.
  Activity* CreateAppActivity(const base::string16& title,
                              const std::string app_id);

  // Wait until the system is idle.
  void WaitUntilIdle();

  // BrowserTestBase:
  virtual void SetUpOnMainThread() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaBrowserTest);
};

}  // namespace athena

#endif //  ATHENA_TEST_CHROME_ATHENA_BROWSERTEST_H_

