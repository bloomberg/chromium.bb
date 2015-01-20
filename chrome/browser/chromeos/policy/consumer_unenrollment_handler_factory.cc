// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_unenrollment_handler_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_unenrollment_handler.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace policy {

// static
ConsumerUnenrollmentHandler*
ConsumerUnenrollmentHandlerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ConsumerUnenrollmentHandler*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ConsumerUnenrollmentHandlerFactory*
ConsumerUnenrollmentHandlerFactory::GetInstance() {
  return Singleton<ConsumerUnenrollmentHandlerFactory>::get();
}

ConsumerUnenrollmentHandlerFactory::ConsumerUnenrollmentHandlerFactory()
    : BrowserContextKeyedServiceFactory(
          "ConsumerUnenrollmentHandler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(chromeos::OwnerSettingsServiceChromeOSFactory::GetInstance());
}

ConsumerUnenrollmentHandlerFactory::~ConsumerUnenrollmentHandlerFactory() {
}

KeyedService* ConsumerUnenrollmentHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return new ConsumerUnenrollmentHandler(
      chromeos::DeviceSettingsService::Get(),
      connector->GetConsumerManagementService(),
      connector->GetDeviceCloudPolicyManager(),
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          context));
}

}  // namespace policy
