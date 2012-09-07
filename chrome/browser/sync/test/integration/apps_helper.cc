// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/apps_helper.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_app_helper.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/common/extensions/extension.h"

using sync_datatype_helper::test;

namespace {

std::string CreateFakeAppName(int index) {
  return "fakeapp" + base::IntToString(index);
}

}  // namespace

namespace apps_helper {

bool HasSameAppsAsVerifier(int index) {
  return SyncAppHelper::GetInstance()->AppStatesMatch(
      test()->GetProfile(index), test()->verifier());
}

bool AllProfilesHaveSameAppsAsVerifier() {
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (!HasSameAppsAsVerifier(i)) {
      LOG(ERROR) << "Profile " << i << " doesn't have the same apps as the"
                                       " verifier profile.";
      return false;
    }
  }
  return true;
}

std::string InstallApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->InstallExtension(
      profile,
      CreateFakeAppName(index),
      extensions::Extension::TYPE_HOSTED_APP);
}

std::string InstallAppForAllProfiles(int index) {
  for (int i = 0; i < test()->num_clients(); ++i)
    InstallApp(test()->GetProfile(i), index);
  return InstallApp(test()->verifier(), index);
}

void UninstallApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->UninstallExtension(
      profile, CreateFakeAppName(index));
}

void EnableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->EnableExtension(
      profile, CreateFakeAppName(index));
}

void DisableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->DisableExtension(
      profile, CreateFakeAppName(index));
}

void IncognitoEnableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoEnableExtension(
      profile, CreateFakeAppName(index));
}

void IncognitoDisableApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoDisableExtension(
      profile, CreateFakeAppName(index));
}

void InstallAppsPendingForSync(Profile* profile) {
  SyncExtensionHelper::GetInstance()->InstallExtensionsPendingForSync(
      profile, extensions::Extension::TYPE_HOSTED_APP);
}

syncer::StringOrdinal GetPageOrdinalForApp(Profile* profile,
                                           int app_index) {
  return SyncAppHelper::GetInstance()->GetPageOrdinalForApp(
      profile, CreateFakeAppName(app_index));
}

void SetPageOrdinalForApp(Profile* profile,
                          int app_index,
                          const syncer::StringOrdinal& page_ordinal) {
  SyncAppHelper::GetInstance()->SetPageOrdinalForApp(
      profile, CreateFakeAppName(app_index), page_ordinal);
}

syncer::StringOrdinal GetAppLaunchOrdinalForApp(Profile* profile,
                                                int app_index) {
  return SyncAppHelper::GetInstance()->GetAppLaunchOrdinalForApp(
      profile, CreateFakeAppName(app_index));
}

void SetAppLaunchOrdinalForApp(
    Profile* profile,
    int app_index,
    const syncer::StringOrdinal& app_launch_ordinal) {
  SyncAppHelper::GetInstance()->SetAppLaunchOrdinalForApp(
      profile, CreateFakeAppName(app_index), app_launch_ordinal);
}

void CopyNTPOrdinals(Profile* source, Profile* destination, int index) {
  SetPageOrdinalForApp(destination, index, GetPageOrdinalForApp(source, index));
  SetAppLaunchOrdinalForApp(
      destination, index, GetAppLaunchOrdinalForApp(source, index));
}

void FixNTPOrdinalCollisions(Profile* profile) {
  SyncAppHelper::GetInstance()->FixNTPOrdinalCollisions(profile);
}

}  // namespace apps_helper
