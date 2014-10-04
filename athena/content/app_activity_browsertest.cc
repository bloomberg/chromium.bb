// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/chrome/athena_app_browsertest.h"

namespace athena {

typedef AthenaAppBrowserTest AppActivityBrowserTest;

// Tests that an application can be loaded.
IN_PROC_BROWSER_TEST_F(AppActivityBrowserTest, StartApplication) {
  // There should be an application we can start.
  ASSERT_TRUE(!GetTestAppID().empty());

  // We should be able to start the application.
  ASSERT_TRUE(CreateTestAppActivity(GetTestAppID()));
}

}  // namespace athena
