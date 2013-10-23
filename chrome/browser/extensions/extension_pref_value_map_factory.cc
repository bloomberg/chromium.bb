// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_pref_value_map_factory.h"

#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

ExtensionPrefValueMapFactory::ExtensionPrefValueMapFactory()
    : BrowserContextKeyedServiceFactory(
        "ExtensionPrefValueMap",
        BrowserContextDependencyManager::GetInstance()) {
}

ExtensionPrefValueMapFactory::~ExtensionPrefValueMapFactory() {
}

// static
ExtensionPrefValueMap* ExtensionPrefValueMapFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionPrefValueMap*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionPrefValueMapFactory* ExtensionPrefValueMapFactory::GetInstance() {
  return Singleton<ExtensionPrefValueMapFactory>::get();
}

BrowserContextKeyedService*
ExtensionPrefValueMapFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ExtensionPrefValueMap();
}
