// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_extensions_sync_test.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"

namespace {

std::string CreateFakeExtensionName(int index) {
  return "fakeextension" + base::IntToString(index);
}

}  // namespace

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

void LiveExtensionsSyncTest::InstallExtension(Profile* profile, int index) {
  return extension_helper_.InstallExtension(profile,
                                            CreateFakeExtensionName(index),
                                            Extension::TYPE_EXTENSION);
}

void LiveExtensionsSyncTest::UninstallExtension(Profile* profile, int index) {
  return extension_helper_.UninstallExtension(profile,
                                              CreateFakeExtensionName(index));
}

void LiveExtensionsSyncTest::EnableExtension(Profile* profile, int index) {
  return extension_helper_.EnableExtension(profile,
                                           CreateFakeExtensionName(index));
}

void LiveExtensionsSyncTest::DisableExtension(Profile* profile, int index) {
  return extension_helper_.DisableExtension(profile,
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

void LiveExtensionsSyncTest::InstallExtensionsPendingForSync(
    Profile* profile) {
  extension_helper_.InstallExtensionsPendingForSync(
      profile, Extension::TYPE_EXTENSION);
}
