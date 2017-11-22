// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/process_dice_header_observer_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "url/gurl.h"

namespace {

void RedirectToNtp(content::WebContents* contents) {
  VLOG(1) << "RedirectToNtp";
  // Redirect to NTP page.
  content::OpenURLParams params(GURL(chrome::kChromeUINewTabURL),
                                content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  contents->OpenURL(params);
}

}  // namespace

ProcessDiceHeaderObserverImpl::ProcessDiceHeaderObserverImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), should_start_sync_(false) {}

void ProcessDiceHeaderObserverImpl::WillStartRefreshTokenFetch(
    const std::string& gaia_id,
    const std::string& email) {
  if (!web_contents())
    return;

  if (!signin::IsDicePrepareMigrationEnabled())
    return;

  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(web_contents());
  should_start_sync_ =
      tab_helper && tab_helper->should_start_sync_after_web_signin();
}

void ProcessDiceHeaderObserverImpl::DidFinishRefreshTokenFetch(
    const std::string& gaia_id,
    const std::string& email) {
  content::WebContents* web_contents = this->web_contents();
  if (!web_contents || !should_start_sync_) {
    VLOG(1) << "Do not start sync after web sign-in.";
    return;
  }
  if (!signin::IsDicePrepareMigrationEnabled())
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  Profile* profile = browser->profile();
  DCHECK(profile);
  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile);
  DCHECK(signin_manager);
  if (signin_manager->IsAuthenticated()) {
    VLOG(1) << "Do not start sync after web sign-in [already authenticated].";
    return;
  }

  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);

  // After signing in to Chrome, the user should be redirected to the NTP.
  RedirectToNtp(web_contents);

  // Turn on sync for an existing account.
  VLOG(1) << "Start sync after web sign-in.";
  std::string account_id = AccountTrackerServiceFactory::GetForProfile(profile)
                               ->PickAccountIdForAccount(gaia_id, email);

  // DiceTurnSyncOnHelper is suicidal (it will kill itself once it finishes
  // enabling sync).
  new DiceTurnSyncOnHelper(profile, browser, tab_helper->signin_access_point(),
                           tab_helper->signin_reason(), account_id);
}
