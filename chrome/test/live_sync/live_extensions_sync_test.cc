// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_extensions_sync_test.h"

#include <map>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/common/extensions/extension.h"

LiveExtensionsSyncTest::LiveExtensionsSyncTest(TestType test_type)
    : LiveExtensionsSyncTestBase(test_type) {}

LiveExtensionsSyncTest::~LiveExtensionsSyncTest() {}

bool LiveExtensionsSyncTest::HasSameExtensionsAsVerifier(int profile) {
  return HasSameExtensionsHelper(GetProfile(profile),
                                 verifier());
}

bool LiveExtensionsSyncTest::AllProfilesHaveSameExtensionsAsVerifier() {
  for (int i = 0; i < num_clients(); ++i) {
    if (!HasSameExtensionsAsVerifier(i))
      return false;
  }
  return true;
}

namespace {

enum ExtensionState { DISABLED, PENDING, ENABLED };

typedef std::map<std::string, ExtensionState> ExtensionStateMap;

ExtensionStateMap GetExtensionStates(ExtensionService* extensions_service) {
  ExtensionStateMap extension_states;

  const ExtensionList* extensions = extensions_service->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    extension_states[(*it)->id()] = ENABLED;
  }

  const ExtensionList* disabled_extensions =
      extensions_service->disabled_extensions();
  for (ExtensionList::const_iterator it = disabled_extensions->begin();
       it != disabled_extensions->end(); ++it) {
    extension_states[(*it)->id()] = DISABLED;
  }

  const PendingExtensionManager* pending_extension_manager =
      extensions_service->pending_extension_manager();
  PendingExtensionManager::const_iterator it;
  for (it = pending_extension_manager->begin();
       it != pending_extension_manager->end(); ++it) {
    extension_states[it->first] = PENDING;
  }

  return extension_states;
}

}  // namespace

bool LiveExtensionsSyncTest::HasSameExtensionsHelper(
    Profile* profile1, Profile* profile2) {
  ExtensionStateMap extension_states1(
      GetExtensionStates(profile1->GetExtensionService()));
  ExtensionStateMap extension_states2(
      GetExtensionStates(profile2->GetExtensionService()));
  return extension_states1 == extension_states2;
}
