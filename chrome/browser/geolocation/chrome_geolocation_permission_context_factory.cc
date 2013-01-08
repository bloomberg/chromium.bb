// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_geolocation_permission_context_factory.h"

#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/pref_names.h"
#if defined(OS_ANDROID)
#include "chrome/browser/geolocation/chrome_geolocation_permission_context_android.h"
#else
#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"
#endif

namespace {

class Service : public ProfileKeyedService {
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

 private:
  scoped_refptr<ChromeGeolocationPermissionContext> context_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace

// static
ChromeGeolocationPermissionContext*
ChromeGeolocationPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<Service*>(
      GetInstance()->GetServiceForProfile(profile, true))->context();
}

// static
ChromeGeolocationPermissionContextFactory*
ChromeGeolocationPermissionContextFactory::GetInstance() {
  return Singleton<ChromeGeolocationPermissionContextFactory>::get();
}

ChromeGeolocationPermissionContextFactory::
ChromeGeolocationPermissionContextFactory()
    : ProfileKeyedServiceFactory(
          "ChromeGeolocationPermissionContext",
          ProfileDependencyManager::GetInstance()) {
}

ChromeGeolocationPermissionContextFactory::
~ChromeGeolocationPermissionContextFactory() {
}

ProfileKeyedService*
ChromeGeolocationPermissionContextFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new Service(profile);
}

void ChromeGeolocationPermissionContextFactory::RegisterUserPrefs(
    PrefServiceSyncable* user_prefs) {
#if defined(OS_ANDROID)
  user_prefs->RegisterBooleanPref(prefs::kGeolocationEnabled,
                                  true,
                                  PrefServiceSyncable::UNSYNCABLE_PREF);
#endif
}

bool ChromeGeolocationPermissionContextFactory::
ServiceRedirectedInIncognito() const {
  return true;
}
