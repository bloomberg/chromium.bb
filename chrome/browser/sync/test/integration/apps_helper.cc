// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/apps_helper.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
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
  // TODO(akalin): We may want to filter out non-apps for some tests.
  return SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(
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

void InstallApp(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->InstallExtension(
      profile, CreateFakeAppName(index), Extension::TYPE_HOSTED_APP);
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
      profile, Extension::TYPE_HOSTED_APP);
}

}  // namespace apps_helper
