// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_UI_COORDINATORS_BROWSER_COORDINATOR_TEST_H_
#define IOS_SHARED_CHROME_BROWSER_UI_COORDINATORS_BROWSER_COORDINATOR_TEST_H_

#include <memory>

#include "testing/platform_test.h"

class Browser;
class TestChromeBrowserState;

class BrowserCoordinatorTest : public PlatformTest {
 protected:
  BrowserCoordinatorTest();
  ~BrowserCoordinatorTest() override;

  Browser* GetBrowser() { return browser_.get(); }

 private:
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

#endif  // IOS_SHARED_CHROME_BROWSER_UI_COORDINATORS_BROWSER_COORDINATOR_TEST_H_
