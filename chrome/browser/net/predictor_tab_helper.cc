// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/predictor_tab_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(chrome_browser_net::PredictorTabHelper);

namespace chrome_browser_net {

namespace {

// Triggers the preconnector on the new navigation api. This captures more
// navigations.
const base::Feature kPreconnectMore{"PreconnectMore",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

PredictorTabHelper::PredictorTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      predicted_from_pending_entry_(false) {
}

PredictorTabHelper::~PredictorTabHelper() {
}

void PredictorTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(kPreconnectMore))
    return;
  // Subframe navigations are handled in WitnessURLRequest.
  if (!navigation_handle->IsInMainFrame())
    return;
  if (predicted_from_pending_entry_) {
    predicted_from_pending_entry_ = false;
    return;
  }
  PreconnectUrl(navigation_handle->GetURL());
}

void PredictorTabHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  // The standard way to preconnect based on navigation.
  PreconnectUrl(url);
  predicted_from_pending_entry_ = true;
}

void PredictorTabHelper::PreconnectUrl(const GURL& url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  chrome_browser_net::Predictor* predictor = profile->GetNetworkPredictor();
  if (predictor && url.SchemeIsHTTPOrHTTPS())
    predictor->PreconnectUrlAndSubresources(url, GURL());
}

}  // namespace chrome_browser_net
