// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_extensions_sync_test.h"

#include <cstring>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"

const char extension_name_prefix[] = "fakeextension";

LiveExtensionsSyncTest::LiveExtensionsSyncTest(TestType test_type)
    : LiveSyncTest(test_type) {}

LiveExtensionsSyncTest::~LiveExtensionsSyncTest() {}

bool LiveExtensionsSyncTest::SetupClients() {
  if (!LiveSyncTest::SetupClients())
    return false;

  extension_helper_.Setup(this);
  return true;
}

bool LiveExtensionsSyncTest::HasSameExtensionsAsVerifier(int index) {
  return extension_helper_.ExtensionStatesMatch(GetProfile(index), verifier());
}

bool LiveExtensionsSyncTest::AllProfilesHaveSameExtensionsAsVerifier() {
  for (int i = 0; i < num_clients(); ++i) {
    if (!HasSameExtensionsAsVerifier(i)) {
      LOG(ERROR) << "Profile " << i << " doesn't have the same extensions as"
                                       " the verifier profile.";
      return false;
    }
  }
  return true;
}

bool LiveExtensionsSyncTest::AllProfilesHaveSameExtensions() {
  for (int i = 1; i < num_clients(); ++i) {
    if (!extension_helper_.ExtensionStatesMatch(GetProfile(0),
                                                GetProfile(i))) {
      LOG(ERROR) << "Profile " << i << " doesnt have the same extensions as"
                                       " profile 0.";
      return false;
    }
  }
  return true;
}


void LiveExtensionsSyncTest::InstallExtension(Profile* profile, int index) {
  return extension_helper_.InstallExtension(profile,
                                            CreateFakeExtensionName(index),
                                            Extension::TYPE_EXTENSION);
}

void LiveExtensionsSyncTest::UninstallExtension(Profile* profile, int index) {
  return extension_helper_.UninstallExtension(profile,
                                              CreateFakeExtensionName(index));
}

std::vector<int> LiveExtensionsSyncTest::GetInstalledExtensions(
    Profile* profile) {
  std::vector<int> indices;
  std::vector<std::string> names =
      extension_helper_.GetInstalledExtensionNames(profile);
  for (std::vector<std::string>::const_iterator it = names.begin();
       it != names.end(); ++it) {
    int index;
    if (ExtensionNameToIndex(*it, &index)) {
      indices.push_back(index);
    }
  }
  return indices;
}

void LiveExtensionsSyncTest::EnableExtension(Profile* profile, int index) {
  return extension_helper_.EnableExtension(profile,
                                           CreateFakeExtensionName(index));
}

void LiveExtensionsSyncTest::DisableExtension(Profile* profile, int index) {
  return extension_helper_.DisableExtension(profile,
                                            CreateFakeExtensionName(index));
}

bool LiveExtensionsSyncTest::IsExtensionEnabled(Profile* profile, int index) {
  return extension_helper_.IsExtensionEnabled(profile,
                                              CreateFakeExtensionName(index));
}

void LiveExtensionsSyncTest::IncognitoEnableExtension(Profile* profile,
                                                      int index) {
  return extension_helper_.IncognitoEnableExtension(
      profile, CreateFakeExtensionName(index));
}

void LiveExtensionsSyncTest::IncognitoDisableExtension(Profile* profile,
                                                       int index) {
  return extension_helper_.IncognitoDisableExtension(
      profile, CreateFakeExtensionName(index));
}

bool LiveExtensionsSyncTest::IsIncognitoEnabled(Profile* profile, int index) {
  return extension_helper_.IsIncognitoEnabled(profile,
                                              CreateFakeExtensionName(index));
}

void LiveExtensionsSyncTest::InstallExtensionsPendingForSync(
    Profile* profile) {
  extension_helper_.InstallExtensionsPendingForSync(
      profile, Extension::TYPE_EXTENSION);
}

std::string LiveExtensionsSyncTest::CreateFakeExtensionName(int index) {
  return extension_name_prefix + base::IntToString(index);
}

bool LiveExtensionsSyncTest::ExtensionNameToIndex(const std::string& name,
                                                  int* index) {
  if (!StartsWithASCII(name, extension_name_prefix, true) ||
      !base::StringToInt(name.substr(strlen(extension_name_prefix)), index)) {
    LOG(WARNING) << "Unable to convert extension name \"" << name
                 << "\" to index";
    return false;
  }
  return true;
}
