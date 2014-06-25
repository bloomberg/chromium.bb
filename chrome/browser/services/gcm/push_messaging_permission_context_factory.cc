// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/push_messaging_permission_context_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/push_messaging_permission_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace gcm {

// static
PushMessagingPermissionContext*
PushMessagingPermissionContextFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PushMessagingPermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PushMessagingPermissionContextFactory*
PushMessagingPermissionContextFactory::GetInstance() {
  return Singleton<PushMessagingPermissionContextFactory>::get();
}

PushMessagingPermissionContextFactory::PushMessagingPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "GCMPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}

PushMessagingPermissionContextFactory
::~PushMessagingPermissionContextFactory() {
}

KeyedService* PushMessagingPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new PushMessagingPermissionContext(static_cast<Profile*>(profile));
}

content::BrowserContext*
PushMessagingPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace gcm
