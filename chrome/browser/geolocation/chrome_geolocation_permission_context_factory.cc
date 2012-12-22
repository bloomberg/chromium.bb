// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_geolocation_permission_context_factory.h"

#include "chrome/common/pref_names.h"
#if defined(OS_ANDROID)
#include "chrome/browser/geolocation/chrome_geolocation_permission_context_android.h"
#else
#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"
#endif

ChromeGeolocationPermissionContext*
    ChromeGeolocationPermissionContextFactory::Create(Profile* profile) {
#if defined(OS_ANDROID)
  return new ChromeGeolocationPermissionContextAndroid(profile);
#else
  return new ChromeGeolocationPermissionContext(profile);
#endif
}

void ChromeGeolocationPermissionContextFactory::RegisterUserPrefs(
    PrefServiceSyncable* user_prefs) {
#if defined(OS_ANDROID)
  user_prefs->RegisterBooleanPref(prefs::kGeolocationEnabled,
                                  true,
                                  PrefServiceSyncable::UNSYNCABLE_PREF);
#endif
}
