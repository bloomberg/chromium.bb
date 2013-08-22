// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/predictor_tab_helper.h"

#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/frame_navigate_params.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(chrome_browser_net::PredictorTabHelper);

using content::BrowserThread;

namespace {

bool IsUserLinkNavigationRequest(content::PageTransition page_transition) {
  bool is_link = (content::PageTransitionStripQualifier(page_transition) ==
                  content::PAGE_TRANSITION_LINK);
  bool is_forward_back = (page_transition &
                          content::PAGE_TRANSITION_FORWARD_BACK) != 0;
  // Tracking navigation session including client-side redirect
  // is currently not supported.
  bool is_client_redirect = (page_transition &
                             content::PAGE_TRANSITION_CLIENT_REDIRECT) != 0;
  return is_link && !is_forward_back && !is_client_redirect;
}

}  // namespace

namespace chrome_browser_net {

PredictorTabHelper::PredictorTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

PredictorTabHelper::~PredictorTabHelper() {
}

void PredictorTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (!IsUserLinkNavigationRequest(params.transition) ||
      !(params.url.SchemeIsHTTPOrHTTPS()))
    return;

  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());
  Predictor* predictor = profile->GetNetworkPredictor();
  if (predictor) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&Predictor::RecordLinkNavigation,
                   base::Unretained(predictor),
                   params.url));
  }
}

}  // namespace chrome_browser_net
