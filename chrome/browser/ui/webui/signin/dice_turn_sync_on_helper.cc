// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"

namespace {

AccountInfo GetAccountInfo(Profile* profile, const std::string& account_id) {
  return AccountTrackerServiceFactory::GetForProfile(profile)->GetAccountInfo(
      account_id);
}

}  // namespace

DiceTurnSyncOnHelper::DiceTurnSyncOnHelper(
    Profile* profile,
    Browser* browser,
    signin_metrics::AccessPoint signin_access_point,
    signin_metrics::Reason signin_reason,
    const std::string& account_id)
    : profile_(profile),
      browser_(browser),
      signin_access_point_(signin_access_point),
      signin_reason_(signin_reason),
      account_info_(GetAccountInfo(profile, account_id)) {
  DCHECK(profile_);
  DCHECK(browser_);
  DCHECK(!account_info_.account_id.empty());
  DCHECK(!account_info_.email.empty());
  DCHECK(!account_info_.gaia.empty());

  // Should not start synching if the profile is already authenticated
  DCHECK(!SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated());

  // Force sign-in uses the modal sign-in flow.
  DCHECK(!signin_util::IsForceSigninEnabled());

  // One initial sign-in goes throught the DiceTurnSyncOnHelper.
  DCHECK_EQ(signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
            signin_reason_);

  if (!HandleCanOfferSigninError() && !HandleCrossAccountError()) {
    CreateSyncStarter(OneClickSigninSyncStarter::CURRENT_PROFILE);
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }
}

DiceTurnSyncOnHelper::~DiceTurnSyncOnHelper() {}

bool DiceTurnSyncOnHelper::HandleCanOfferSigninError() {
  std::string error_msg;
  bool can_offer =
      CanOfferSignin(profile_, CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS,
                     account_info_.gaia, account_info_.email, &error_msg);
  if (can_offer)
    return false;

  // Display the error message
  LoginUIServiceFactory::GetForProfile(profile_)->DisplayLoginResult(
      browser_, base::UTF8ToUTF16(error_msg),
      base::UTF8ToUTF16(account_info_.email));
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  return true;
}

bool DiceTurnSyncOnHelper::HandleCrossAccountError() {
  std::string last_email =
      profile_->GetPrefs()->GetString(prefs::kGoogleServicesLastUsername);

  // TODO(skym): Warn for high risk upgrade scenario, crbug.com/572754.
  if (!IsCrossAccountError(profile_, account_info_.email, account_info_.gaia))
    return false;

  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();

  SigninEmailConfirmationDialog::AskForConfirmation(
      web_contents, profile_, last_email, account_info_.email,
      base::Bind(&DiceTurnSyncOnHelper::ConfirmEmailAction,
                 base::Unretained(this), web_contents));
  return true;
}

void DiceTurnSyncOnHelper::ConfirmEmailAction(
    content::WebContents* web_contents,
    SigninEmailConfirmationDialog::Action action) {
  switch (action) {
    case SigninEmailConfirmationDialog::CREATE_NEW_USER:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_DontImport"));
      CreateSyncStarter(OneClickSigninSyncStarter::NEW_PROFILE);
      break;
    case SigninEmailConfirmationDialog::START_SYNC:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_ImportData"));
      CreateSyncStarter(OneClickSigninSyncStarter::CURRENT_PROFILE);
      break;
    case SigninEmailConfirmationDialog::CLOSE:
      base::RecordAction(
          base::UserMetricsAction("Signin_ImportDataPrompt_Cancel"));
      break;
    default:
      NOTREACHED() << "Invalid action";
  }
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void DiceTurnSyncOnHelper::CreateSyncStarter(
    OneClickSigninSyncStarter::ProfileMode profile_mode) {
  // OneClickSigninSyncStarter will delete itself once the job is done.
  new OneClickSigninSyncStarter(
      profile_, browser_, account_info_.account_id, signin_access_point_,
      signin_reason_, profile_mode, OneClickSigninSyncStarter::Callback());
}
