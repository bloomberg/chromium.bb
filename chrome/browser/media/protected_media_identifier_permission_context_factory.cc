// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_permission_context_factory.h"

#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

// static
ProtectedMediaIdentifierPermissionContext*
ProtectedMediaIdentifierPermissionContextFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ProtectedMediaIdentifierPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProtectedMediaIdentifierPermissionContextFactory*
ProtectedMediaIdentifierPermissionContextFactory::GetInstance() {
  return base::Singleton<
      ProtectedMediaIdentifierPermissionContextFactory>::get();
}

ProtectedMediaIdentifierPermissionContextFactory::
ProtectedMediaIdentifierPermissionContextFactory()
    : PermissionContextFactoryBase(
          "ProtectedMediaIdentifierPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}

ProtectedMediaIdentifierPermissionContextFactory::
~ProtectedMediaIdentifierPermissionContextFactory() {
}

KeyedService*
ProtectedMediaIdentifierPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ProtectedMediaIdentifierPermissionContext(
      static_cast<Profile*>(profile));
}

void
ProtectedMediaIdentifierPermissionContextFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kProtectedMediaIdentifierEnabled, true);
#endif
}
