// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "content/common/notification_service.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#endif

// static
BrowserThemeProvider* ThemeServiceFactory::GetForProfile(Profile* top_profile) {
  // We may be asked for the Theme of an incognito profile. Make sure we're
  // operating on the real profile.
  Profile* profile = top_profile->GetOriginalProfile();

  ThemeServiceFactory* service = GetInstance();

  std::map<Profile*, BrowserThemeProvider*>::const_iterator it =
      service->mapping_.find(profile);
  if (it != service->mapping_.end())
    return it->second;

  BrowserThemeProvider* provider = NULL;
#if defined(TOOLKIT_USES_GTK)
  provider = new GtkThemeProvider;
#else
  provider = new BrowserThemeProvider;
#endif
  provider->Init(profile);

  service->Associate(profile, provider);
  return provider;
}

const Extension* ThemeServiceFactory::GetThemeForProfile(Profile* profile) {
  std::string id = GetForProfile(profile)->GetThemeID();
  if (id == BrowserThemeProvider::kDefaultThemeID)
    return NULL;

  return profile->GetExtensionService()->GetExtensionById(id, false);
}

void ThemeServiceFactory::ForceAssociationBetween(Profile* top_profile,
                                           BrowserThemeProvider* provider) {
  ThemeServiceFactory* service = GetInstance();
  Profile* profile = top_profile->GetOriginalProfile();

  service->Associate(profile, provider);
}

ThemeServiceFactory* ThemeServiceFactory::GetInstance() {
  return Singleton<ThemeServiceFactory>::get();
}

ThemeServiceFactory::ThemeServiceFactory() {}

ThemeServiceFactory::~ThemeServiceFactory() {
  DCHECK(mapping_.empty());
}

void ThemeServiceFactory::Associate(Profile* profile,
                             BrowserThemeProvider* provider) {
  DCHECK(mapping_.find(profile) == mapping_.end());
  mapping_.insert(std::make_pair(profile, provider));

  registrar_.Add(this,
                 NotificationType::PROFILE_DESTROYED,
                 Source<Profile>(profile));
  registrar_.Add(this,
                 NotificationType::THEME_INSTALLED,
                 Source<Profile>(profile));
}

void ThemeServiceFactory::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  std::map<Profile*, BrowserThemeProvider*>::iterator it =
      mapping_.find(Source<Profile>(source).ptr());
  DCHECK(it != mapping_.end());

  if (NotificationType::PROFILE_DESTROYED == type) {
    delete it->second;
    mapping_.erase(it);

    // Remove ourselves from listening to all notifications because the source
    // profile has been deleted. We have to do this because several unit tests
    // are set up so a Profile is on the same place on the stack multiple
    // times, so while they are different instances, they have the same
    // addresses.
    registrar_.Remove(this,
                      NotificationType::PROFILE_DESTROYED,
                      source);
    registrar_.Remove(this,
                      NotificationType::THEME_INSTALLED,
                      source);
  } else if (NotificationType::THEME_INSTALLED == type) {
    const Extension* extension = Details<const Extension>(details).ptr();
    it->second->SetTheme(extension);
  } else {
    NOTREACHED();
  }
}
