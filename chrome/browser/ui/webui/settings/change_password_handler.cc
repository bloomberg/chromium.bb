// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/change_password_handler.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"


namespace settings {

using safe_browsing::ChromePasswordProtectionService;

ChangePasswordHandler::ChangePasswordHandler(Profile* profile)
    : profile_(profile),
      service_(nullptr),
      password_protection_observer_(this) {}

ChangePasswordHandler::~ChangePasswordHandler() {}

void ChangePasswordHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeChangePasswordHandler",
      base::Bind(&ChangePasswordHandler::HandleInitialize,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "onChangePasswordPageShown",
      base::Bind(&ChangePasswordHandler::HandleChangePasswordPageShown,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "changePassword", base::Bind(&ChangePasswordHandler::HandleChangePassword,
                                   base::Unretained(this)));
}

void ChangePasswordHandler::OnJavascriptAllowed() {
  if (service_)
    password_protection_observer_.Add(service_);
}

void ChangePasswordHandler::OnJavascriptDisallowed() {
  password_protection_observer_.RemoveAll();
}

void ChangePasswordHandler::OnGaiaPasswordChanged() {
  FireWebUIListener("change-password-on-dismiss");
}

void ChangePasswordHandler::OnMarkingSiteAsLegitimate(const GURL& url) {
  if (!ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile_)) {
    FireWebUIListener("change-password-on-dismiss");
  }
}

void ChangePasswordHandler::OnGaiaPasswordReuseWarningShown() {
  FireWebUIListener("change-password-on-show");
}

void ChangePasswordHandler::InvokeActionForTesting(
    ChromePasswordProtectionService::WarningAction action) {
  if (!ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile_))
    return;

  DCHECK_EQ(ChromePasswordProtectionService::CHANGE_PASSWORD, action);
  base::ListValue value;
  HandleChangePassword(&value);
}

ChromePasswordProtectionService::WarningUIType
ChangePasswordHandler::GetObserverType() {
  return ChromePasswordProtectionService::CHROME_SETTINGS;
}

void ChangePasswordHandler::HandleInitialize(const base::ListValue* args) {
  service_ = safe_browsing::ChromePasswordProtectionService::
      GetPasswordProtectionService(profile_);
  if (service_)
    AllowJavascript();
}

void ChangePasswordHandler::HandleChangePasswordPageShown(
    const base::ListValue* args) {
  AllowJavascript();
  if (service_) {
    service_->RecordWarningAction(
        safe_browsing::PasswordProtectionService::CHROME_SETTINGS,
        safe_browsing::PasswordProtectionService::SHOWN);
  }
}

void ChangePasswordHandler::HandleChangePassword(const base::ListValue* args) {
  if (service_) {
    service_->OnUserAction(
        web_ui()->GetWebContents(),
        safe_browsing::PasswordProtectionService::CHROME_SETTINGS,
        safe_browsing::PasswordProtectionService::CHANGE_PASSWORD);
  }
}

}  // namespace settings
