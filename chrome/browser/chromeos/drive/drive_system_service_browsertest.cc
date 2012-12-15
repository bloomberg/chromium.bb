// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_system_service.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace drive {

class DriveSystemServiceBrowserTest : public InProcessBrowserTest {
};

// Verify DriveSystemService is created during login.
IN_PROC_BROWSER_TEST_F(DriveSystemServiceBrowserTest, CreatedDuringLogin) {
  EXPECT_TRUE(DriveSystemServiceFactory::FindForProfile(browser()->profile()));
}

}  // namespace drive
