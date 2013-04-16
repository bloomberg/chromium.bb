// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autotest_private/autotest_private_api.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/autotest_private/autotest_private_api_factory.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/extensions/api/autotest_private.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace extensions {

bool AutotestPrivateLogoutFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateLogoutFunction";
  if (!AutotestPrivateAPIFactory::GetForProfile(profile())->test_mode())
    chrome::AttemptUserExit();
  return true;
}

bool AutotestPrivateRestartFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateRestartFunction";
  if (!AutotestPrivateAPIFactory::GetForProfile(profile())->test_mode())
    chrome::AttemptRestart();
  return true;
}

bool AutotestPrivateShutdownFunction::RunImpl() {
  scoped_ptr<api::autotest_private::Shutdown::Params> params(
      api::autotest_private::Shutdown::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateShutdownFunction " << params->force;

#if defined(OS_CHROMEOS)
  if (params->force) {
    if (!AutotestPrivateAPIFactory::GetForProfile(profile())->test_mode())
      chrome::ExitCleanly();
    return true;
  }
#endif
  if (!AutotestPrivateAPIFactory::GetForProfile(profile())->test_mode())
    chrome::AttemptExit();
  return true;
}

bool AutotestPrivateLoginStatusFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateLoginStatusFunction";

  DictionaryValue* result(new DictionaryValue);
#if defined(OS_CHROMEOS)
  const chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  const bool is_screen_locked =
      !!chromeos::ScreenLocker::default_screen_locker();

  if (user_manager) {
    result->SetBoolean("isLoggedIn", user_manager->IsUserLoggedIn());
    result->SetBoolean("isOwner", user_manager->IsCurrentUserOwner());
    result->SetBoolean("isScreenLocked", is_screen_locked);
    if (user_manager->IsUserLoggedIn()) {
      result->SetBoolean("isRegularUser",
                         user_manager->IsLoggedInAsRegularUser());
      result->SetBoolean("isGuest", user_manager->IsLoggedInAsGuest());
      result->SetBoolean("isKiosk", user_manager->IsLoggedInAsKioskApp());

      const chromeos::User* user = user_manager->GetLoggedInUser();
      result->SetString("email", user->email());
      result->SetString("displayEmail", user->display_email());

      std::string user_image;
      switch (user->image_index()) {
        case chromeos::User::kExternalImageIndex:
          user_image = "file";
          break;

        case chromeos::User::kProfileImageIndex:
          user_image = "profile";
          break;

        default:
          user_image = base::IntToString(user->image_index());
          break;
      }
      result->SetString("userImage", user_image);
    }
  }
#endif

  SetResult(result);
  return true;
}

AutotestPrivateAPI::AutotestPrivateAPI() : test_mode_(false) {
}

AutotestPrivateAPI::~AutotestPrivateAPI() {
}

}  // namespace extensions
