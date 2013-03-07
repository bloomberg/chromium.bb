// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shortcut_manager_factory.h"

#include "apps/shortcut_manager.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace apps {

// static
ShortcutManager* ShortcutManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<ShortcutManager*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
void ShortcutManagerFactory::ResetForProfile(Profile* profile) {
  ShortcutManagerFactory* factory = GetInstance();
  factory->ProfileShutdown(profile);
  factory->ProfileDestroyed(profile);
}

ShortcutManagerFactory* ShortcutManagerFactory::GetInstance() {
  return Singleton<ShortcutManagerFactory>::get();
}

ShortcutManagerFactory::ShortcutManagerFactory()
    : ProfileKeyedServiceFactory("ShortcutManager",
                                 ProfileDependencyManager::GetInstance()) {
}

ShortcutManagerFactory::~ShortcutManagerFactory() {
}

ProfileKeyedService* ShortcutManagerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ShortcutManager(profile);
}

bool ShortcutManagerFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

}  // namespace apps
