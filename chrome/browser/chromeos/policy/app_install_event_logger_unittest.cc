// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_logger.h"

#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using AppInstallEventLoggerTest = testing::Test;

namespace policy {

// Store lists of apps for which push-install has been requested and is still
// pending. Clear all data related to app-install event log collection. Verify
// that the lists are cleared.
TEST_F(AppInstallEventLoggerTest, Clear) {
  content::TestBrowserThreadBundle browser_thread_bundle;
  TestingProfile profile;
  base::ListValue list;
  list.AppendString("test");
  profile.GetPrefs()->Set(arc::prefs::kArcPushInstallAppsRequested, list);
  profile.GetPrefs()->Set(arc::prefs::kArcPushInstallAppsPending, list);
  AppInstallEventLogger::Clear(&profile);
  EXPECT_TRUE(profile.GetPrefs()
                  ->FindPreference(arc::prefs::kArcPushInstallAppsRequested)
                  ->IsDefaultValue());
  EXPECT_TRUE(profile.GetPrefs()
                  ->FindPreference(arc::prefs::kArcPushInstallAppsPending)
                  ->IsDefaultValue());
}

}  // namespace policy
