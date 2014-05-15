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

namespace {

class Service : public KeyedService {
 public:
  explicit Service(Profile* profile) {
    context_ = new ProtectedMediaIdentifierPermissionContext(profile);
  }

  ProtectedMediaIdentifierPermissionContext* context() {
    return context_.get();
  }

  virtual void Shutdown() OVERRIDE {
    context()->ShutdownOnUIThread();
  }

 private:
  scoped_refptr<ProtectedMediaIdentifierPermissionContext> context_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace

// static
ProtectedMediaIdentifierPermissionContext*
ProtectedMediaIdentifierPermissionContextFactory::GetForProfile(
    Profile* profile) {
  return static_cast<Service*>(
      GetInstance()->GetServiceForBrowserContext(profile, true))->context();
}

// static
ProtectedMediaIdentifierPermissionContextFactory*
ProtectedMediaIdentifierPermissionContextFactory::GetInstance() {
  return Singleton<
      ProtectedMediaIdentifierPermissionContextFactory>::get();
}

ProtectedMediaIdentifierPermissionContextFactory::
ProtectedMediaIdentifierPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "ProtectedMediaIdentifierPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}

ProtectedMediaIdentifierPermissionContextFactory::
~ProtectedMediaIdentifierPermissionContextFactory() {
}

KeyedService*
ProtectedMediaIdentifierPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new Service(static_cast<Profile*>(profile));
}

void
ProtectedMediaIdentifierPermissionContextFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kProtectedMediaIdentifierEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

content::BrowserContext*
ProtectedMediaIdentifierPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
