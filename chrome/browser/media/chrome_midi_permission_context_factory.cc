// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/chrome_midi_permission_context_factory.h"

#include "chrome/browser/media/chrome_midi_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
ChromeMidiPermissionContext*
ChromeMidiPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromeMidiPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeMidiPermissionContextFactory*
ChromeMidiPermissionContextFactory::GetInstance() {
  return Singleton<ChromeMidiPermissionContextFactory>::get();
}

ChromeMidiPermissionContextFactory::
ChromeMidiPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeMidiPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}

ChromeMidiPermissionContextFactory::
~ChromeMidiPermissionContextFactory() {
}

KeyedService* ChromeMidiPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ChromeMidiPermissionContext(static_cast<Profile*>(profile));
}

content::BrowserContext*
ChromeMidiPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
