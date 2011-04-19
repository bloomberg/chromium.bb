// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_extensions_sync_test.h"

#include <map>
#include <string>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"

LiveExtensionsSyncTest::LiveExtensionsSyncTest(TestType test_type)
    : LiveSyncTest(test_type) {}

LiveExtensionsSyncTest::~LiveExtensionsSyncTest() {}

bool LiveExtensionsSyncTest::SetupClients() {
  if (!LiveSyncTest::SetupClients())
    return false;

  extension_helper_.Setup(this);
  return true;
}

namespace {

enum ExtensionState { DISABLED, PENDING, ENABLED };

typedef std::map<std::string, ExtensionState> ExtensionStateMap;

ExtensionStateMap GetExtensionStates(ExtensionService* extensions_service) {
  const std::string& profile_name =
      extensions_service->profile()->GetPath().BaseName().MaybeAsASCII();

  ExtensionStateMap extension_state_map;

  const ExtensionList* extensions = extensions_service->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    extension_state_map[(*it)->id()] = ENABLED;
    VLOG(2) << "Extension " << (*it)->id() << " in profile "
            << profile_name << " is enabled";
  }

  const ExtensionList* disabled_extensions =
      extensions_service->disabled_extensions();
  for (ExtensionList::const_iterator it = disabled_extensions->begin();
       it != disabled_extensions->end(); ++it) {
    extension_state_map[(*it)->id()] = DISABLED;
    VLOG(2) << "Extension " << (*it)->id() << " in profile "
            << profile_name << " is disabled";
  }

  const PendingExtensionManager* pending_extension_manager =
      extensions_service->pending_extension_manager();
  PendingExtensionManager::const_iterator it;
  for (it = pending_extension_manager->begin();
       it != pending_extension_manager->end(); ++it) {
    extension_state_map[it->first] = PENDING;
    VLOG(2) << "Extension " << it->first << " in profile "
            << profile_name << " is pending";
  }

  return extension_state_map;
}

}  // namespace

bool LiveExtensionsSyncTest::AllProfilesHaveSameExtensionsAsVerifier() {
  ExtensionStateMap verifier_extension_state_map(
      GetExtensionStates(verifier()->GetExtensionService()));
  for (int i = 0; i < num_clients(); ++i) {
    ExtensionStateMap extension_state_map(
        GetExtensionStates(GetProfile(i)->GetExtensionService()));
    if (extension_state_map != verifier_extension_state_map) {
      return false;
    }
  }
  return true;
}

void LiveExtensionsSyncTest::InstallExtension(Profile* profile, int index) {
  std::string name = "fakeextension" + base::IntToString(index);
  return extension_helper_.InstallExtension(
      profile, name, Extension::TYPE_EXTENSION);
}

void LiveExtensionsSyncTest::InstallExtensionsPendingForSync(
    Profile* profile) {
  extension_helper_.InstallExtensionsPendingForSync(
      profile, Extension::TYPE_EXTENSION);
}
