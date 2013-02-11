// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_system_service.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace drive {

class DriveSystemServiceBrowserTest : public InProcessBrowserTest {
};

// Verify DriveSystemService is created during login.
IN_PROC_BROWSER_TEST_F(DriveSystemServiceBrowserTest, CreatedDuringLogin) {
  EXPECT_TRUE(DriveSystemServiceFactory::FindForProfile(browser()->profile()));
}


IN_PROC_BROWSER_TEST_F(DriveSystemServiceBrowserTest,
                       DisableDrivePolicyTest) {
  // First make sure the pref is set to its default value which should permit
  // drive.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableDrive, false);

  drive::DriveSystemService* drive_service =
      drive::DriveSystemServiceFactory::GetForProfile(browser()->profile());

  EXPECT_TRUE(drive_service);

  // ...next try to disable drive.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableDrive, true);

  drive_service =
      drive::DriveSystemServiceFactory::GetForProfile(browser()->profile());

  EXPECT_FALSE(drive_service);
}

}  // namespace drive
