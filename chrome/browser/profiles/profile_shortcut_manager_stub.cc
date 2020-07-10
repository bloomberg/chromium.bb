// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_shortcut_manager.h"

void ProfileShortcutManager::DisableForUnitTests() {}

// static
bool ProfileShortcutManager::IsFeatureEnabled() {
  return false;
}

// static
std::unique_ptr<ProfileShortcutManager> ProfileShortcutManager::Create(
    ProfileManager* manager) {
  return nullptr;
}
