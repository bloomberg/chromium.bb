// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/extensions_helper.h"

#include <cstring>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/common/extensions/extension.h"

using sync_datatype_helper::test;

namespace extensions_helper {

const char extension_name_prefix[] = "fakeextension";

bool HasSameExtensionsAsVerifier(int index) {
  return SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(
      test()->GetProfile(index), test()->verifier());
}

bool AllProfilesHaveSameExtensionsAsVerifier() {
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (!HasSameExtensionsAsVerifier(i)) {
      LOG(ERROR) << "Profile " << i << " doesn't have the same extensions as"
                                       " the verifier profile.";
      return false;
    }
  }
  return true;
}

bool AllProfilesHaveSameExtensions() {
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(
        test()->GetProfile(0), test()->GetProfile(i))) {
      LOG(ERROR) << "Profile " << i << " doesnt have the same extensions as"
                                       " profile 0.";
      return false;
    }
  }
  return true;
}


void InstallExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->InstallExtension(
      profile, CreateFakeExtensionName(index), Extension::TYPE_EXTENSION);
}

void UninstallExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->UninstallExtension(
      profile, CreateFakeExtensionName(index));
}

std::vector<int> GetInstalledExtensions(Profile* profile) {
  std::vector<int> indices;
  std::vector<std::string> names =
      SyncExtensionHelper::GetInstance()->GetInstalledExtensionNames(profile);
  for (std::vector<std::string>::const_iterator it = names.begin();
       it != names.end(); ++it) {
    int index;
    if (ExtensionNameToIndex(*it, &index)) {
      indices.push_back(index);
    }
  }
  return indices;
}

void EnableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->EnableExtension(
      profile, CreateFakeExtensionName(index));
}

void DisableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->DisableExtension(
      profile, CreateFakeExtensionName(index));
}

bool IsExtensionEnabled(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IsExtensionEnabled(
      profile, CreateFakeExtensionName(index));
}

void IncognitoEnableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoEnableExtension(
      profile, CreateFakeExtensionName(index));
}

void IncognitoDisableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoDisableExtension(
      profile, CreateFakeExtensionName(index));
}

bool IsIncognitoEnabled(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IsIncognitoEnabled(
      profile, CreateFakeExtensionName(index));
}

void InstallExtensionsPendingForSync(Profile* profile) {
  SyncExtensionHelper::GetInstance()->InstallExtensionsPendingForSync(
      profile, Extension::TYPE_EXTENSION);
}

std::string CreateFakeExtensionName(int index) {
  return extension_name_prefix + base::IntToString(index);
}

bool ExtensionNameToIndex(const std::string& name, int* index) {
  if (!StartsWithASCII(name, extension_name_prefix, true) ||
      !base::StringToInt(name.substr(strlen(extension_name_prefix)), index)) {
    LOG(WARNING) << "Unable to convert extension name \"" << name
                 << "\" to index";
    return false;
  }
  return true;
}

}  // namespace extensions_helper
