// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_permission_context_factory.h"

#include "chrome/browser/media/midi_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
MidiPermissionContext*
MidiPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<MidiPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MidiPermissionContextFactory*
MidiPermissionContextFactory::GetInstance() {
  return Singleton<MidiPermissionContextFactory>::get();
}

MidiPermissionContextFactory::MidiPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "MidiPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}

MidiPermissionContextFactory::~MidiPermissionContextFactory() {
}

KeyedService* MidiPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new MidiPermissionContext(static_cast<Profile*>(profile));
}

content::BrowserContext*
MidiPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
