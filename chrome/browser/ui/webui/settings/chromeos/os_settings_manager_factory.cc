// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager_factory.h"

#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace settings {

// static
OsSettingsManager* OsSettingsManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<OsSettingsManager*>(
      OsSettingsManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
OsSettingsManagerFactory* OsSettingsManagerFactory::GetInstance() {
  return base::Singleton<OsSettingsManagerFactory>::get();
}

OsSettingsManagerFactory::OsSettingsManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "OsSettingsManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(local_search_service::LocalSearchServiceFactory::GetInstance());
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(SupervisedUserServiceFactory::GetInstance());
  DependsOn(KerberosCredentialsManagerFactory::GetInstance());
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(android_sms::AndroidSmsServiceFactory::GetInstance());
  DependsOn(CupsPrintersManagerFactory::GetInstance());
}

OsSettingsManagerFactory::~OsSettingsManagerFactory() = default;

KeyedService* OsSettingsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new OsSettingsManager(
      profile,
      local_search_service::LocalSearchServiceFactory::GetForProfile(profile),
      multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(profile),
      ProfileSyncServiceFactory::GetForProfile(profile),
      SupervisedUserServiceFactory::GetForProfile(profile),
      KerberosCredentialsManagerFactory::Get(profile),
      ArcAppListPrefsFactory::GetForBrowserContext(profile),
      IdentityManagerFactory::GetForProfile(profile),
      android_sms::AndroidSmsServiceFactory::GetForBrowserContext(profile),
      CupsPrintersManagerFactory::GetForBrowserContext(profile));
}

bool OsSettingsManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

content::BrowserContext* OsSettingsManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace settings
}  // namespace chromeos
