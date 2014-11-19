// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_enrollment_handler_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_enrollment_handler.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace policy {

// static
ConsumerEnrollmentHandler*
ConsumerEnrollmentHandlerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ConsumerEnrollmentHandler*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ConsumerEnrollmentHandlerFactory*
ConsumerEnrollmentHandlerFactory::GetInstance() {
  return Singleton<ConsumerEnrollmentHandlerFactory>::get();
}

ConsumerEnrollmentHandlerFactory::ConsumerEnrollmentHandlerFactory()
    : BrowserContextKeyedServiceFactory(
          "ConsumerEnrollmentHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

ConsumerEnrollmentHandlerFactory::~ConsumerEnrollmentHandlerFactory() {
}

bool ConsumerEnrollmentHandlerFactory::ShouldCreateHandler(
    Profile* profile,
    ConsumerManagementService* service) const {
  if (!service)
    return false;

  // On a fresh device, the first time the owner signs in, IsOwnerProfile()
  // will return false. But it is okay since there's no enrollment in progress
  // so we don't need to create a handler.
  if (!chromeos::ProfileHelper::IsOwnerProfile(profile))
    return false;

  return service->GetStatus() == ConsumerManagementService::STATUS_ENROLLING ||
      service->HasPendingEnrollmentNotification();
}

KeyedService* ConsumerEnrollmentHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  ConsumerManagementService* service =
      connector->GetConsumerManagementService();

  if (ShouldCreateHandler(profile, service)) {
    return new ConsumerEnrollmentHandler(
        profile,
        service,
        connector->GetDeviceManagementServiceForConsumer());
  } else {
    return nullptr;
  }
}

bool ConsumerEnrollmentHandlerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace policy
