// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSIONS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSIONS_HELPER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

class Profile;

namespace extensions_helper {

// Returns true iff the profile with index |index| has the same extensions
// as the verifier.
bool HasSameExtensionsAsVerifier(int index) WARN_UNUSED_RESULT;

// Returns true iff all existing profiles have the same extensions
// as the verifier.
bool AllProfilesHaveSameExtensionsAsVerifier() WARN_UNUSED_RESULT;

// Returns true iff all existing profiles have the same extensions.
bool AllProfilesHaveSameExtensions() WARN_UNUSED_RESULT;

// Installs the extension for the given index to |profile|, and returns the
// extension ID of the new extension.
std::string InstallExtension(Profile* profile, int index);

// Installs the extension for the given index to all profiles (including the
// verifier), and returns the extension ID of the new extension.
std::string InstallExtensionForAllProfiles(int index);

// Uninstalls the extension for the given index from |profile|. Assumes that
// it was previously installed.
void UninstallExtension(Profile* profile, int index);

// Returns a vector containing the indices of all currently installed
// test extensions on |profile|.
std::vector<int> GetInstalledExtensions(Profile* profile);

// Installs all pending synced extensions for |profile|.
void InstallExtensionsPendingForSync(Profile* profile);

// Enables the extension for the given index on |profile|.
void EnableExtension(Profile* profile, int index);

// Disables the extension for the given index on |profile|.
void DisableExtension(Profile* profile, int index);

// Returns true if the extension with index |index| is enabled on |profile|.
bool IsExtensionEnabled(Profile* profile, int index);

// Enables the extension for the given index in incognito mode on |profile|.
void IncognitoEnableExtension(Profile* profile, int index);

// Disables the extension for the given index in incognito mode on |profile|.
void IncognitoDisableExtension(Profile* profile, int index);

// Returns true if the extension with index |index| is enabled in incognito
// mode on |profile|.
bool IsIncognitoEnabled(Profile* profile, int index);

// Returns a unique extension name based in the integer |index|.
std::string CreateFakeExtensionName(int index);

// Converts a fake extension name back into the index used to generate it.
// Returns true if successful, false on failure.
bool ExtensionNameToIndex(const std::string& name, int* index);

// Runs the message loop until AllProfilesHaveSameExtensionsAsVerifier()
// is true.  Returns false on timeout.
bool AwaitAllProfilesHaveSameExtensionsAsVerifier();

}  // namespace extensions_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSIONS_HELPER_H_
