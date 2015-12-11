// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/geolocation/geolocation_permission_context_android.h"
#else
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#endif


// static
GeolocationPermissionContext*
GeolocationPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<GeolocationPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GeolocationPermissionContextFactory*
GeolocationPermissionContextFactory::GetInstance() {
  return base::Singleton<GeolocationPermissionContextFactory>::get();
}

#if !BUILDFLAG(ANDROID_JAVA_UI)
GeolocationPermissionContextFactory::GeolocationPermissionContextFactory()
    : PermissionContextFactoryBase(
          "GeolocationPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}
#else
GeolocationPermissionContextFactory::GeolocationPermissionContextFactory()
    : PermissionContextFactoryBase(
          "GeolocationPermissionContextAndroid",
          BrowserContextDependencyManager::GetInstance()) {
}
#endif


GeolocationPermissionContextFactory::~GeolocationPermissionContextFactory() {
}

KeyedService*
GeolocationPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
#if !BUILDFLAG(ANDROID_JAVA_UI)
  return new GeolocationPermissionContext(static_cast<Profile*>(profile));
#else
  return new GeolocationPermissionContextAndroid(
      static_cast<Profile*>(profile));
#endif
}
