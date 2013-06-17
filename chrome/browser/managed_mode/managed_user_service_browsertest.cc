// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

typedef InProcessBrowserTest ManagedUserServiceTest;

IN_PROC_BROWSER_TEST_F(ManagedUserServiceTest, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kForceSafeSearch));
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kForceSafeSearch));

  ManagedUserService* managed_user_service =
       ManagedUserServiceFactory::GetForProfile(profile);
  managed_user_service->InitForTesting();
  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(prefs->GetBoolean(prefs::kForceSafeSearch));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kForceSafeSearch));
}
