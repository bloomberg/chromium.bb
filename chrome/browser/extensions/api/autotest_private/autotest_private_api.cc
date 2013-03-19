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
  const ash::user::LoginStatus status =
      ash::Shell::GetInstance()->system_tray_delegate() ?
      ash::Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus() :
      ash::user::LOGGED_IN_NONE;

  switch (status) {
    case ash::user::LOGGED_IN_LOCKED:
      return std::string("locked");
    case ash::user::LOGGED_IN_USER:
      return std::string("user");
    case ash::user::LOGGED_IN_OWNER:
      return std::string("owner");
    case ash::user::LOGGED_IN_GUEST:
      return std::string("guest");
    case ash::user::LOGGED_IN_RETAIL_MODE:
      return std::string("retail");
    case ash::user::LOGGED_IN_PUBLIC:
      return std::string("public");
    case ash::user::LOGGED_IN_LOCALLY_MANAGED:
      return std::string("local");
    case ash::user::LOGGED_IN_KIOSK_APP:
      return std::string("kiosk");
    case ash::user::LOGGED_IN_NONE:
      return std::string("none");
    // Intentionally leaves out default so that compiler catches missing
    // branches when new login status is added.
  }

  NOTREACHED();
#endif

  return std::string("none");
}

}  // namespace

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

#if defined(OS_CHROME)
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
  SetResult(base::Value::CreateStringValue(GetUserLoginStatus()));
  return true;
}

AutotestPrivateAPI::AutotestPrivateAPI() : test_mode_(false) {
}

AutotestPrivateAPI::~AutotestPrivateAPI() {
}

}  // namespace extensions
