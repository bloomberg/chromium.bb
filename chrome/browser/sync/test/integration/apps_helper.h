// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

class Profile;

namespace apps_helper {

// Returns true iff the profile with index |index| has the same apps as the
// verifier.
bool HasSameAppsAsVerifier(int index) WARN_UNUSED_RESULT;

// Returns true iff all existing profiles have the same apps as the verifier.
bool AllProfilesHaveSameAppsAsVerifier() WARN_UNUSED_RESULT;

// Installs the app for the given index to |profile|.
void InstallApp(Profile* profile, int index);

// Uninstalls the app for the given index from |profile|. Assumes that it was
// previously installed.
void UninstallApp(Profile* profile, int index);

// Installs all pending synced apps for |profile|.
void InstallAppsPendingForSync(Profile* profile);

// Enables the app for the given index on |profile|.
void EnableApp(Profile* profile, int index);

// Disables the appfor the given index on |profile|.
void DisableApp(Profile* profile, int index);

// Enables the app for the given index in incognito mode on |profile|.
void IncognitoEnableApp(Profile* profile, int index);

// Disables the app for the given index in incognito mode on |profile|.
void IncognitoDisableApp(Profile* profile, int index);

}  // namespace apps_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_HELPER_H_
