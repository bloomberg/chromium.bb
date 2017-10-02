// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_tab_helper.h"

#include "base/logging.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "google_apis/gaia/gaia_urls.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(DiceTabHelper);

DiceTabHelper::DiceTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      should_start_sync_after_web_signin_(true) {
  DCHECK_EQ(GaiaUrls::GetInstance()->add_account_url(),
            content::WebContentsObserver::web_contents()->GetVisibleURL());
}

DiceTabHelper::~DiceTabHelper() {}

void DiceTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
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
