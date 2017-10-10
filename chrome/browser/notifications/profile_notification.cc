// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/profile_notification.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/signin/core/account_id/account_id.h"

// static
std::string ProfileNotification::GetProfileNotificationId(
    const std::string& delegate_id,
    ProfileID profile_id) {
  DCHECK(profile_id);
  return base::StringPrintf("notification-ui-manager#%p#%s",
                            profile_id,  // Each profile has its unique instance
                                         // including incognito profile.
                            delegate_id.c_str());
}

ProfileNotification::ProfileNotification(Profile* profile,
                                         const Notification& notification)
    : profile_id_(NotificationUIManager::GetProfileID(profile)),
      notification_(
          // Uses Notification's copy constructor to assign the message center
          // id, which should be unique for every profile + Notification pair.
          GetProfileNotificationId(
              notification.id(),
              NotificationUIManager::GetProfileID(profile)),
          notification),
      original_id_(notification.id()),
      keep_alive_(new ScopedKeepAlive(KeepAliveOrigin::NOTIFICATION,
                                      KeepAliveRestartOption::DISABLED)) {
  DCHECK(profile);
#if defined(OS_CHROMEOS)
  notification_.set_profile_id(
      multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail());
#endif
}

ProfileNotification::~ProfileNotification() {}
