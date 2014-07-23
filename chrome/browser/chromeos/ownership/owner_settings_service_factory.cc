// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"

#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user.h"

namespace chromeos {

OwnerSettingsServiceFactory::OwnerSettingsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OwnerSettingsService",
          BrowserContextDependencyManager::GetInstance()) {
}

OwnerSettingsServiceFactory::~OwnerSettingsServiceFactory() {
}

// static
OwnerSettingsService* OwnerSettingsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OwnerSettingsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
OwnerSettingsServiceFactory* OwnerSettingsServiceFactory::GetInstance() {
  return Singleton<OwnerSettingsServiceFactory>::get();
}

void OwnerSettingsServiceFactory::SetUsername(const std::string& username) {
  username_ = username;
  if (!UserManager::IsInitialized())
    return;
  const user_manager::User* user = UserManager::Get()->FindUser(username_);
  if (!user || !user->is_profile_created())
    return;
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return;
  OwnerSettingsService* service = GetForProfile(profile);

  // It's safe to call ReloadPrivateKey() here, as profile is fully created
  // at this time.
  if (service)
    service->ReloadPrivateKey();
}

std::string OwnerSettingsServiceFactory::GetUsername() const {
  return username_;
}

// static
KeyedService* OwnerSettingsServiceFactory::BuildInstanceFor(
    content::BrowserContext* browser_context) {
  Profile* profile = static_cast<Profile*>(browser_context);
  if (profile->IsGuestSession() || ProfileHelper::IsSigninProfile(profile))
    return NULL;
  return new OwnerSettingsService(profile);
}

bool OwnerSettingsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService* OwnerSettingsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}

}  // namespace chromeos
