// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#if defined(OS_ANDROID)
#include "chrome/browser/geolocation/geolocation_permission_context_android.h"
#else
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#endif

namespace {

class Service : public KeyedService {
 public:
  explicit Service(Profile* profile) {
#if defined(OS_ANDROID)
    context_ = new GeolocationPermissionContextAndroid(profile);
#else
    context_ = new GeolocationPermissionContext(profile);
#endif
  }

  GeolocationPermissionContext* context() {
    return context_.get();
  }

  virtual void Shutdown() OVERRIDE {
    context()->ShutdownOnUIThread();
  }

 private:
  scoped_refptr<GeolocationPermissionContext> context_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace

// static
GeolocationPermissionContext*
GeolocationPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<Service*>(
      GetInstance()->GetServiceForBrowserContext(profile, true))->context();
}

// static
GeolocationPermissionContextFactory*
GeolocationPermissionContextFactory::GetInstance() {
  return Singleton<GeolocationPermissionContextFactory>::get();
}

GeolocationPermissionContextFactory::GeolocationPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "GeolocationPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}

GeolocationPermissionContextFactory::~GeolocationPermissionContextFactory() {
}

KeyedService*
GeolocationPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new Service(static_cast<Profile*>(profile));
}

void GeolocationPermissionContextFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kGeolocationEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
}

content::BrowserContext*
GeolocationPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
