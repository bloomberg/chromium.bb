// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/change_password_handler.h"

#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/referrer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace settings {

ChangePasswordHandler::ChangePasswordHandler(Profile* profile)
    : profile_(profile), service_(nullptr) {
  if (g_browser_process && g_browser_process->safe_browsing_service()) {
    service_ = g_browser_process->safe_browsing_service()
                   ->GetPasswordProtectionService(profile_);
  }
}

ChangePasswordHandler::~ChangePasswordHandler() {}

void ChangePasswordHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onChangePasswordPageShown",
      base::Bind(&ChangePasswordHandler::HandleChangePasswordPageShown,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "changePassword", base::Bind(&ChangePasswordHandler::HandleChangePassword,
                                   base::Unretained(this)));
}

void ChangePasswordHandler::HandleChangePasswordPageShown(
    const base::ListValue* args) {
  if (service_) {
    service_->OnWarningShown(
        web_ui()->GetWebContents(),
        safe_browsing::PasswordProtectionService::CHROME_SETTINGS);
  }
}

void ChangePasswordHandler::HandleChangePassword(const base::ListValue* args) {
  if (service_) {
    service_->OnWarningDone(
        web_ui()->GetWebContents(),
        safe_browsing::PasswordProtectionService::CHROME_SETTINGS,
        safe_browsing::PasswordProtectionService::CHANGE_PASSWORD);
  }
}

}  // namespace settings
