// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_notifier_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_notifier.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user_manager.h"

namespace policy {

// static
ConsumerManagementNotifier*
ConsumerManagementNotifierFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ConsumerManagementNotifier*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ConsumerManagementNotifierFactory*
ConsumerManagementNotifierFactory::GetInstance() {
  return Singleton<ConsumerManagementNotifierFactory>::get();
}

ConsumerManagementNotifierFactory::ConsumerManagementNotifierFactory()
    : BrowserContextKeyedServiceFactory(
          "ConsumerManagementNotifier",
          BrowserContextDependencyManager::GetInstance()) {
}

ConsumerManagementNotifierFactory::~ConsumerManagementNotifierFactory() {
}

KeyedService* ConsumerManagementNotifierFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // Some tests don't have a user manager and may crash at IsOwnerProfile().
  if (!user_manager::UserManager::IsInitialized())
    return nullptr;

  // On a fresh device, the first time the owner signs in, IsOwnerProfile()
  // will return false. But it is okay since there's no notification to show.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!chromeos::ProfileHelper::IsOwnerProfile(profile))
    return nullptr;

  ConsumerManagementService* service =
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
      GetConsumerManagementService();
  if (!service)
    return nullptr;

  return new ConsumerManagementNotifier(profile, service);
}

bool ConsumerManagementNotifierFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace policy
