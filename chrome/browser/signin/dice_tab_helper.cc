// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_tab_helper.h"

#include "base/logging.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "google_apis/gaia/gaia_urls.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(DiceTabHelper);

DiceTabHelper::DiceTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      signin_access_point_(signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN),
      signin_reason_(signin_metrics::Reason::REASON_UNKNOWN_REASON),
      should_start_sync_after_web_signin_(true) {
}

DiceTabHelper::~DiceTabHelper() {}

void DiceTabHelper::InitializeSigninFlow(
    signin_metrics::AccessPoint access_point,
    signin_metrics::Reason reason) {
  signin_access_point_ = access_point;
  signin_reason_ = reason;
  should_start_sync_after_web_signin_ = true;
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

  // TODO(msarda): Figure out if this condition can be restricted even more
  // (e.g. avoid starting sync after a browser initiated navigation).
  // if (!navigation_handle->IsRendererInitiated()) {
  //  // Avoid starting sync if the navigations comes from the browser.
  //  should_start_sync_after_web_signin_ = false;
  //}
}
