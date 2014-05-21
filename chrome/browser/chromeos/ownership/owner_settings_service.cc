// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/owner_settings_service.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"

using content::BrowserThread;

namespace chromeos {

OwnerSettingsService::OwnerSettingsService(Profile* profile)
    : profile_(profile), weak_factory_(this) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CREATED,
                 content::Source<Profile>(profile_));
}

OwnerSettingsService::~OwnerSettingsService() {
}

void OwnerSettingsService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_PROFILE_CREATED) {
    NOTREACHED();
    return;
  }

  Profile* profile = content::Source<Profile>(source).ptr();
  if (profile != profile_) {
    NOTREACHED();
    return;
  }

  ReloadOwnerKey();
}

void OwnerSettingsService::ReloadOwnerKey() {
  if (!UserManager::IsInitialized())
    return;
  const User* user = UserManager::Get()->GetUserByProfile(profile_);
  if (!user || !user->is_profile_created())
    return;
  std::string user_id = user->email();
  if (user_id != OwnerSettingsServiceFactory::GetInstance()->GetUsername())
    return;
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&crypto::GetPublicSlotForChromeOSUser, user->username_hash()),
      base::Bind(&DeviceSettingsService::InitOwner,
                 base::Unretained(DeviceSettingsService::Get()),
                 user_id));
}

}  // namespace chromeos
