// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/chrome_midi_permission_context_factory.h"

#include "chrome/browser/media/chrome_midi_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

// static
ChromeMIDIPermissionContext*
ChromeMIDIPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<ChromeMIDIPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeMIDIPermissionContextFactory*
ChromeMIDIPermissionContextFactory::GetInstance() {
  return Singleton<ChromeMIDIPermissionContextFactory>::get();
}

ChromeMIDIPermissionContextFactory::
ChromeMIDIPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeMIDIPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}

ChromeMIDIPermissionContextFactory::
~ChromeMIDIPermissionContextFactory() {
}

BrowserContextKeyedService*
ChromeMIDIPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ChromeMIDIPermissionContext(static_cast<Profile*>(profile));
}

content::BrowserContext*
ChromeMIDIPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
