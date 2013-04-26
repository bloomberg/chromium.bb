// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_pref_value_map_factory.h"

#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

ExtensionPrefValueMapFactory::ExtensionPrefValueMapFactory()
    : ProfileKeyedServiceFactory(
        "ExtensionPrefValueMap",
        ProfileDependencyManager::GetInstance()) {
}

ExtensionPrefValueMapFactory::~ExtensionPrefValueMapFactory() {
}

// static
ExtensionPrefValueMap* ExtensionPrefValueMapFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ExtensionPrefValueMap*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ExtensionPrefValueMapFactory* ExtensionPrefValueMapFactory::GetInstance() {
  return Singleton<ExtensionPrefValueMapFactory>::get();
}

ProfileKeyedService* ExtensionPrefValueMapFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ExtensionPrefValueMap();
}
