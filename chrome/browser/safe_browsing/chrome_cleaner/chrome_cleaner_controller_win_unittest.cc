// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"

#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

class ChromeCleanerControllerTest : public ::testing::Test {
 private:
  // We need this because we need a UI thread during tests. The thread bundle
  // must be the first member of the class so that it will be destroyed last.
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(ChromeCleanerControllerTest, GetInstance) {
  // Ensure that GetInstance() returns the same controller object between
  // invocations as long someone is holding on to a reference to the controller.
  //
  // Also ensure that the controller object is destroyed if it is in the IDLE
  // state and there are no more references to it.

  {
    scoped_refptr<ChromeCleanerController> controller =
        ChromeCleanerController::GetInstance();
    EXPECT_TRUE(controller);
    EXPECT_EQ(controller, ChromeCleanerController::GetRawInstanceForTesting());
    EXPECT_EQ(controller->state(), ChromeCleanerController::State::kIdle);

    // The same controller instance is returned as long as there are references
    // to it.
    EXPECT_EQ(controller, ChromeCleanerController::GetInstance());
  }
  // All references have now gone out of scope and the controller object should
  // be destroyed.
  EXPECT_FALSE(ChromeCleanerController::GetRawInstanceForTesting());

  // GetInstance() always returns a valid object.
  scoped_refptr<ChromeCleanerController> controller =
      ChromeCleanerController::GetInstance();
  EXPECT_TRUE(controller);
  EXPECT_EQ(controller, ChromeCleanerController::GetRawInstanceForTesting());
}

}  // namespace
}  // namespace safe_browsing
