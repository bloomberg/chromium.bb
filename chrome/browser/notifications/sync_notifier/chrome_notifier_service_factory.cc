// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace notifier {

// static
ChromeNotifierService* ChromeNotifierServiceFactory::GetForProfile(
    Profile* profile, Profile::ServiceAccessType sat) {
  return static_cast<ChromeNotifierService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
ChromeNotifierServiceFactory* ChromeNotifierServiceFactory::GetInstance() {
  return Singleton<ChromeNotifierServiceFactory>::get();
}

ChromeNotifierServiceFactory::ChromeNotifierServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChromeNotifierService", ProfileDependencyManager::GetInstance()) {}

ChromeNotifierServiceFactory::~ChromeNotifierServiceFactory() {
}

ProfileKeyedService*
ChromeNotifierServiceFactory::BuildServiceInstanceFor(Profile* profile) const {
  NotificationUIManager* notification_manager =
      g_browser_process->notification_ui_manager();
  ChromeNotifierService* chrome_notifier_service =
      new ChromeNotifierService(profile, notification_manager);
  return chrome_notifier_service;
}

}  // namespace notifier
