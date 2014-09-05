// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"

#include "base/path_service.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_paths.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ownership/owner_key_util.h"
#include "components/ownership/owner_key_util_impl.h"

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

scoped_refptr<ownership::OwnerKeyUtil>
OwnerSettingsServiceFactory::GetOwnerKeyUtil() {
  if (owner_key_util_.get())
    return owner_key_util_;
  base::FilePath public_key_path;
  if (!PathService::Get(chromeos::FILE_OWNER_KEY, &public_key_path))
    return NULL;
  owner_key_util_ = new ownership::OwnerKeyUtilImpl(public_key_path);
  return owner_key_util_;
}

void OwnerSettingsServiceFactory::SetOwnerKeyUtilForTesting(
    const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util) {
  owner_key_util_ = owner_key_util;
}

// static
KeyedService* OwnerSettingsServiceFactory::BuildInstanceFor(
    content::BrowserContext* browser_context) {
  Profile* profile = static_cast<Profile*>(browser_context);
  if (profile->IsGuestSession() || ProfileHelper::IsSigninProfile(profile))
    return NULL;
  return new OwnerSettingsService(profile, GetInstance()->GetOwnerKeyUtil());
}

bool OwnerSettingsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService* OwnerSettingsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}

}  // namespace chromeos
