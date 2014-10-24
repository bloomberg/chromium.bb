// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"

#include "base/path_service.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_paths.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ownership/owner_key_util.h"
#include "components/ownership/owner_key_util_impl.h"

namespace chromeos {

OwnerSettingsServiceChromeOSFactory::OwnerSettingsServiceChromeOSFactory()
    : BrowserContextKeyedServiceFactory(
          "OwnerSettingsService",
          BrowserContextDependencyManager::GetInstance()) {
}

OwnerSettingsServiceChromeOSFactory::~OwnerSettingsServiceChromeOSFactory() {
}

// static
OwnerSettingsServiceChromeOS*
OwnerSettingsServiceChromeOSFactory::GetForProfile(Profile* profile) {
  return static_cast<OwnerSettingsServiceChromeOS*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
OwnerSettingsServiceChromeOSFactory*
OwnerSettingsServiceChromeOSFactory::GetInstance() {
  return Singleton<OwnerSettingsServiceChromeOSFactory>::get();
}

scoped_refptr<ownership::OwnerKeyUtil>
OwnerSettingsServiceChromeOSFactory::GetOwnerKeyUtil() {
  if (owner_key_util_.get())
    return owner_key_util_;
  base::FilePath public_key_path;
  if (!PathService::Get(chromeos::FILE_OWNER_KEY, &public_key_path))
    return NULL;
  owner_key_util_ = new ownership::OwnerKeyUtilImpl(public_key_path);
  return owner_key_util_;
}

void OwnerSettingsServiceChromeOSFactory::SetOwnerKeyUtilForTesting(
    const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util) {
  owner_key_util_ = owner_key_util;
}

// static
KeyedService* OwnerSettingsServiceChromeOSFactory::BuildInstanceFor(
    content::BrowserContext* browser_context) {
  Profile* profile = static_cast<Profile*>(browser_context);
  if (profile->IsGuestSession() || ProfileHelper::IsSigninProfile(profile))
    return NULL;
  return new OwnerSettingsServiceChromeOS(profile,
                                          GetInstance()->GetOwnerKeyUtil());
}

bool OwnerSettingsServiceChromeOSFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

KeyedService* OwnerSettingsServiceChromeOSFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}

}  // namespace chromeos
