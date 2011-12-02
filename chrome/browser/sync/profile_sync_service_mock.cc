// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"

ProfileSyncServiceMock::ProfileSyncServiceMock()
    : ProfileSyncService(NULL, NULL, new SigninManager(), "") {
}

ProfileSyncServiceMock::ProfileSyncServiceMock(
    Profile* profile) : ProfileSyncService(NULL, profile, NULL, "") {
}

ProfileSyncServiceMock::~ProfileSyncServiceMock() {
}

// static
Profile* ProfileSyncServiceMock::MakeSignedInTestingProfile() {
  TestingProfile* profile = new TestingProfile();
  TestingPrefStore* user_prefs = new TestingPrefStore();
  PrefService* prefs = PrefServiceMockBuilder()
      .WithUserPrefs(user_prefs)
      .Create();
  profile->SetPrefService(prefs);
  SigninManager::RegisterUserPrefs(prefs);
  user_prefs->SetString(prefs::kGoogleServicesUsername, "foo");
  return profile;
}
