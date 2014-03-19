// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/autocomplete/shortcuts_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForBrowserContext(profile, false).get());
}

// static
ShortcutsBackendFactory* ShortcutsBackendFactory::GetInstance() {
  return Singleton<ShortcutsBackendFactory>::get();
}

// static
scoped_refptr<RefcountedBrowserContextKeyedService>
ShortcutsBackendFactory::BuildProfileForTesting(
    content::BrowserContext* profile) {
  scoped_refptr<ShortcutsBackend> backend(
      new ShortcutsBackend(static_cast<Profile*>(profile), false));
  if (backend->Init())
    return backend;
  return NULL;
}

// static
scoped_refptr<RefcountedBrowserContextKeyedService>
ShortcutsBackendFactory::BuildProfileNoDatabaseForTesting(
    content::BrowserContext* profile) {
  scoped_refptr<ShortcutsBackend> backend(
      new ShortcutsBackend(static_cast<Profile*>(profile), true));
  if (backend->Init())
    return backend;
  return NULL;
}

ShortcutsBackendFactory::ShortcutsBackendFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
        "ShortcutsBackend",
        BrowserContextDependencyManager::GetInstance()) {
}

ShortcutsBackendFactory::~ShortcutsBackendFactory() {}

scoped_refptr<RefcountedBrowserContextKeyedService>
ShortcutsBackendFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  scoped_refptr<ShortcutsBackend> backend(
      new ShortcutsBackend(static_cast<Profile*>(profile), false));
  if (backend->Init())
    return backend;
  return NULL;
}

bool ShortcutsBackendFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
