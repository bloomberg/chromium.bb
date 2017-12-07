// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/process_dice_header_delegate_impl.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "url/gurl.h"

namespace {

void RedirectToNtp(content::WebContents* contents) {
  VLOG(1) << "RedirectToNtp";
  content::OpenURLParams params(GURL(chrome::kChromeUINewTabURL),
                                content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  contents->OpenURL(params);
}

Profile* GetProfileForContents(content::WebContents* web_contents) {
  DCHECK(web_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  Profile* profile = browser->profile();
  DCHECK(profile);
  return profile;
}

}  // namespace

ProcessDiceHeaderDelegateImpl::ProcessDiceHeaderDelegateImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      profile_(GetProfileForContents(web_contents)) {
  DCHECK(profile_);

  if (!signin::IsDicePrepareMigrationEnabled())
    return;

  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(web_contents);
  if (tab_helper) {
    should_start_sync_ = tab_helper->should_start_sync_after_web_signin();
    signin_access_point_ = tab_helper->signin_access_point();
    signin_reason_ = tab_helper->signin_reason();
  }
}

bool ProcessDiceHeaderDelegateImpl::ShouldEnableSync() {
  if (!signin::IsDicePrepareMigrationEnabled()) {
    VLOG(1) << "Do not start sync after web sign-in [DICE prepare migration "
               "not enabled].";
    return false;
  }

  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile_);
  DCHECK(signin_manager);
  if (signin_manager->IsAuthenticated()) {
    VLOG(1) << "Do not start sync after web sign-in [already authenticated].";
    return false;
  }

  if (!should_start_sync_) {
    VLOG(1) << "Do not start sync after web sign-in [no a Chrome sign-in tab].";
    return false;
  }

  return true;
}

void ProcessDiceHeaderDelegateImpl::EnableSync(const std::string& account_id) {
  if (!ShouldEnableSync()) {
    // No special treatment is needed if the user is not enabling sync.
    return;
  }

  content::WebContents* web_contents = this->web_contents();
  Browser* browser = nullptr;
  if (web_contents) {
    browser = chrome::FindBrowserWithWebContents(web_contents);
    // After signing in to Chrome, the user should be redirected to the NTP.
    RedirectToNtp(web_contents);
  }

  // DiceTurnSyncOnHelper is suicidal (it will kill itself once it finishes
  // enabling sync).
  VLOG(1) << "Start sync after web sign-in.";
  new DiceTurnSyncOnHelper(profile_, browser, signin_access_point_,
                           signin_reason_, account_id);
}

void ProcessDiceHeaderDelegateImpl::HandleTokenExchangeFailure(
    const std::string& email,
    const GoogleServiceAuthError& error) {
  DCHECK_NE(GoogleServiceAuthError::NONE, error.state());
  bool should_enable_sync = ShouldEnableSync();
  if (!should_enable_sync &&
      !signin::IsDiceEnabledForProfile(profile_->GetPrefs())) {
    return;
  }

  // Show pop up-dialog.
  content::WebContents* web_contents = this->web_contents();
  Browser* browser = web_contents
                         ? chrome::FindBrowserWithWebContents(web_contents)
                         : chrome::FindBrowserWithProfile(profile_);
  LoginUIServiceFactory::GetForProfile(profile_)->DisplayLoginResult(
      browser, base::UTF8ToUTF16(error.ToString()), base::UTF8ToUTF16(email));

  if (should_enable_sync && web_contents)
    RedirectToNtp(web_contents);
}
