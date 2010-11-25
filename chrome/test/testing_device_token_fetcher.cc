// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/testing_device_token_fetcher.h"

#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/browser/profile.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#else
#include "chrome/browser/sync/signin_manager.h"
#endif

namespace policy {

const char* kTestManagedDomainUsername = "___@example.com";

void TestingDeviceTokenFetcher::SimulateLogin(const std::string& username) {
  username_ = username;
#if defined(OS_CHROMEOS)
  chromeos::UserManager::User user;
  user.set_email(username_);
  NotificationService::current()->Notify(
      NotificationType::LOGIN_USER_CHANGED,
      Source<chromeos::UserManager>(NULL),
      Details<const chromeos::UserManager::User>(&user));
#else
  GoogleServiceSigninSuccessDetails details(username_, "");
  NotificationService::current()->Notify(
      NotificationType::GOOGLE_SIGNIN_SUCCESSFUL,
      Source<Profile>(profile_),
      Details<const GoogleServiceSigninSuccessDetails>(&details));
#endif
}

std::string TestingDeviceTokenFetcher::GetCurrentUser() {
  return username_;
}

} // namespace policy
