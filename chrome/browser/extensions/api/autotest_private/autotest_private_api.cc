// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autotest_private/autotest_private_api.h"

#include "chrome/browser/extensions/api/autotest_private/autotest_private_api_factory.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/extensions/api/autotest_private.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#endif

namespace extensions {
namespace {

std::string GetUserLoginStatus() {
#if defined(OS_CHROMEOS)
  typedef struct {
    ash::user::LoginStatus login_status;
    std::string status_string;
  } StatusString;
  const StatusString kStatusStrings[] = {
      { ash::user::LOGGED_IN_LOCKED, "locked" },
      { ash::user::LOGGED_IN_USER, "user" },
      { ash::user::LOGGED_IN_OWNER, "owner" },
      { ash::user::LOGGED_IN_GUEST, "guest" },
      { ash::user::LOGGED_IN_KIOSK, "kiosk" },
      { ash::user::LOGGED_IN_PUBLIC, "public" },
      { ash::user::LOGGED_IN_NONE, "none" },
  };
  const ash::user::LoginStatus status =
      ash::Shell::GetInstance()->system_tray_delegate() ?
      ash::Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus() :
      ash::user::LOGGED_IN_NONE;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kStatusStrings); ++i) {
    if (kStatusStrings[i].login_status == status)
      return kStatusStrings[i].status_string;
  }
#endif

  return "none";
}

}  // namespace

bool AutotestPrivateLogoutFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateLogoutFunction";
  if (!AutotestPrivateAPIFactory::GetForProfile(profile())->test_mode())
    browser::AttemptUserExit();
  return true;
}

bool AutotestPrivateRestartFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateRestartFunction";
  if (!AutotestPrivateAPIFactory::GetForProfile(profile())->test_mode())
    browser::AttemptRestart();
  return true;
}

bool AutotestPrivateShutdownFunction::RunImpl() {
  scoped_ptr<api::autotest_private::Shutdown::Params> params(
      api::autotest_private::Shutdown::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateShutdownFunction " << params->force;

#if defined(OS_CHROME)
  if (params->force) {
    if (!AutotestPrivateAPIFactory::GetForProfile(profile())->test_mode())
      browser::ExitCleanly();
    return true;
  }
#endif
  if (!AutotestPrivateAPIFactory::GetForProfile(profile())->test_mode())
    browser::AttemptExit();
  return true;
}

bool AutotestPrivateLoginStatusFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateLoginStatusFunction";
  SetResult(base::Value::CreateStringValue(GetUserLoginStatus()));
  return true;
}

AutotestPrivateAPI::AutotestPrivateAPI() : test_mode_(false) {
}

AutotestPrivateAPI::~AutotestPrivateAPI() {
}

}  // namespace extensions
