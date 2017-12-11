// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_tab_helper.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "google_apis/gaia/gaia_urls.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(DiceTabHelper);

DiceTabHelper::DiceTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

DiceTabHelper::~DiceTabHelper() = default;

void DiceTabHelper::InitializeSigninFlow(
    signin_metrics::AccessPoint access_point,
    signin_metrics::Reason reason) {
  signin_access_point_ = access_point;
  signin_reason_ = reason;
  should_start_sync_after_web_signin_ = true;
  did_finish_loading_signin_page_ = false;

  if (signin_reason_ == signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT) {
    signin_metrics::LogSigninAccessPointStarted(access_point);
    signin_metrics::RecordSigninUserActionForAccessPoint(access_point);
    base::RecordAction(base::UserMetricsAction("Signin_SigninPage_Loading"));
  }
}

void DiceTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (signin::GetAccountConsistencyMethod() !=
      signin::AccountConsistencyMethod::kDicePrepareMigration) {
    // Chrome relies on a Gaia signal to enable Chrome sync for all DICE methods
    // different than prepare migration.
    return;
  }

  if (!should_start_sync_after_web_signin_)
    return;

  if (!navigation_handle->IsInMainFrame()) {
    VLOG(1) << "Ignore subframe navigation to " << navigation_handle->GetURL();
    return;
  }
  if (navigation_handle->GetURL().GetOrigin() !=
      GaiaUrls::GetInstance()->gaia_url()) {
    VLOG(1) << "Avoid starting sync after a user navigation to "
            << navigation_handle->GetURL()
            << " which is outside of Gaia domain ("
            << GaiaUrls::GetInstance()->gaia_url() << ")";
    should_start_sync_after_web_signin_ = false;
    return;
  }
}

void DiceTabHelper::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                  const GURL& validated_url) {
  if (!should_start_sync_after_web_signin_ || did_finish_loading_signin_page_)
    return;

  if (validated_url.GetOrigin() == GaiaUrls::GetInstance()->gaia_url()) {
    VLOG(1) << "Finished loading sign-in page: " << validated_url.spec();
    did_finish_loading_signin_page_ = true;
    base::RecordAction(base::UserMetricsAction("Signin_SigninPage_Shown"));
  }
}
