// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_geolocation_permission_context_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"
#if defined(OS_ANDROID)
#include "chrome/browser/geolocation/chrome_geolocation_permission_context_android.h"
#else
#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"
#endif

namespace {

class Service : public BrowserContextKeyedService {
 public:
  explicit Service(Profile* profile) {
#if defined(OS_ANDROID)
    context_ = new ChromeGeolocationPermissionContextAndroid(profile);
#else
    context_ = new ChromeGeolocationPermissionContext(profile);
#endif
  }

  ChromeGeolocationPermissionContext* context() {
    return context_.get();
  }

  virtual void Shutdown() OVERRIDE {
    context()->ShutdownOnUIThread();
  }

 private:
  scoped_refptr<ChromeGeolocationPermissionContext> context_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace

// static
ChromeGeolocationPermissionContext*
ChromeGeolocationPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<Service*>(
      GetInstance()->GetServiceForBrowserContext(profile, true))->context();
}

// static
ChromeGeolocationPermissionContextFactory*
ChromeGeolocationPermissionContextFactory::GetInstance() {
  return Singleton<ChromeGeolocationPermissionContextFactory>::get();
}

ChromeGeolocationPermissionContextFactory::
ChromeGeolocationPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeGeolocationPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}

ChromeGeolocationPermissionContextFactory::
~ChromeGeolocationPermissionContextFactory() {
}

BrowserContextKeyedService*
ChromeGeolocationPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new Service(static_cast<Profile*>(profile));
}

void ChromeGeolocationPermissionContextFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kGeolocationEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
}

content::BrowserContext*
ChromeGeolocationPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
