// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/process_dice_header_delegate_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
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
}

void ProcessDiceHeaderDelegateImpl::WillStartRefreshTokenFetch(
    const std::string& gaia_id,
    const std::string& email) {
  if (!web_contents())
    return;

  if (!signin::IsDicePrepareMigrationEnabled())
    return;

  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(web_contents());
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

bool ProcessDiceHeaderDelegateImpl::ShouldUpdateCredentials(
    const std::string& gaia_id,
    const std::string& email,
    const std::string& refresh_token) {
  if (!ShouldEnableSync()) {
    // No special treatment is needed if the user is not enabling sync.
    return true;
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
                           signin_reason_, gaia_id, email, refresh_token);

  // Avoid updating the credentials when the user is turning on sync as in
  // some special cases the refresh token may actually need to be copied to
  // a new profile.
  return false;
}
