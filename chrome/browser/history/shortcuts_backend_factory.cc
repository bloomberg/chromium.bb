// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/shortcuts_backend_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/history/shortcuts_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/pref_names.h"

using history::ShortcutsBackend;

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForProfile(profile, true).get());
}

// static
scoped_refptr<ShortcutsBackend> ShortcutsBackendFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<ShortcutsBackend*>(
      GetInstance()->GetServiceForProfile(profile, false).get());
}

// static
ShortcutsBackendFactory* ShortcutsBackendFactory::GetInstance() {
  return Singleton<ShortcutsBackendFactory>::get();
}

// static
scoped_refptr<RefcountedProfileKeyedService>
ShortcutsBackendFactory::BuildProfileForTesting(
    content::BrowserContext* profile) {
  scoped_refptr<history::ShortcutsBackend> backend(
      new ShortcutsBackend(static_cast<Profile*>(profile), false));
  if (backend->Init())
    return backend;
  return NULL;
}

// static
scoped_refptr<RefcountedProfileKeyedService>
ShortcutsBackendFactory::BuildProfileNoDatabaseForTesting(
    content::BrowserContext* profile) {
  scoped_refptr<history::ShortcutsBackend> backend(
      new ShortcutsBackend(static_cast<Profile*>(profile), true));
  if (backend->Init())
    return backend;
  return NULL;
}

ShortcutsBackendFactory::ShortcutsBackendFactory()
    : RefcountedProfileKeyedServiceFactory(
        "ShortcutsBackend",
        ProfileDependencyManager::GetInstance()) {
}

ShortcutsBackendFactory::~ShortcutsBackendFactory() {}

scoped_refptr<RefcountedProfileKeyedService>
ShortcutsBackendFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  scoped_refptr<history::ShortcutsBackend> backend(
      new ShortcutsBackend(static_cast<Profile*>(profile), false));
  if (backend->Init())
    return backend;
  return NULL;
}

bool ShortcutsBackendFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
