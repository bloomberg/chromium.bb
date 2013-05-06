// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"

#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"

// static
chrome::MediaGalleriesPreferences*
MediaGalleriesPreferencesFactory::GetForProfile(Profile* profile) {
  return static_cast<chrome::MediaGalleriesPreferences*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
MediaGalleriesPreferencesFactory*
MediaGalleriesPreferencesFactory::GetInstance() {
  return Singleton<MediaGalleriesPreferencesFactory>::get();
}

MediaGalleriesPreferencesFactory::MediaGalleriesPreferencesFactory()
    : ProfileKeyedServiceFactory("MediaGalleriesPreferences",
                                 ProfileDependencyManager::GetInstance()) {}

MediaGalleriesPreferencesFactory::~MediaGalleriesPreferencesFactory() {}

ProfileKeyedService* MediaGalleriesPreferencesFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new chrome::MediaGalleriesPreferences(static_cast<Profile*>(profile));
}

void MediaGalleriesPreferencesFactory::RegisterUserPrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  chrome::MediaGalleriesPreferences::RegisterUserPrefs(prefs);
}

content::BrowserContext*
MediaGalleriesPreferencesFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
