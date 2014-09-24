// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/extension_welcome_notification_factory.h"

#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/extension_welcome_notification.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"

// static
ExtensionWelcomeNotification*
ExtensionWelcomeNotificationFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return static_cast<ExtensionWelcomeNotification*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionWelcomeNotificationFactory*
ExtensionWelcomeNotificationFactory::GetInstance() {
  return Singleton<ExtensionWelcomeNotificationFactory>::get();
}

ExtensionWelcomeNotificationFactory::ExtensionWelcomeNotificationFactory()
    : BrowserContextKeyedServiceFactory(
        "ExtensionWelcomeNotification",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DesktopNotificationServiceFactory::GetInstance());
}

ExtensionWelcomeNotificationFactory::~ExtensionWelcomeNotificationFactory() {}

KeyedService* ExtensionWelcomeNotificationFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return ExtensionWelcomeNotification::Create(static_cast<Profile*>(context));
}

content::BrowserContext*
ExtensionWelcomeNotificationFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
