// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/predictor_tab_helper.h"

#include "base/command_line.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(chrome_browser_net::PredictorTabHelper);

namespace chrome_browser_net {

PredictorTabHelper::PredictorTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

PredictorTabHelper::~PredictorTabHelper() {
}

void PredictorTabHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  chrome_browser_net::Predictor* predictor = profile->GetNetworkPredictor();
  if (!predictor)
    return;
  if (url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))
    predictor->PreconnectUrlAndSubresources(url, GURL());
}

}  // namespace chrome_browser_net
