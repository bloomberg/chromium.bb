// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/connection_help_tab_helper.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace {
const char kHelpCenterConnectionHelpUrl[] =
    "https://support.google.com/chrome/answer/6098869/";
const char kBundledConnectionHelpUrl[] = "chrome://connection-help";

void MaybeRedirectToBundledHelp(content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(features::kBundledConnectionHelpFeature))
    return;
  GURL::Replacements replacements;
  std::string error_code = web_contents->GetURL().ref();
  replacements.SetRefStr(error_code);
  web_contents->GetController().LoadURL(
      GURL(kBundledConnectionHelpUrl).ReplaceComponents(replacements),
      content::Referrer(), ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}
}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ConnectionHelpTabHelper);

ConnectionHelpTabHelper::~ConnectionHelpTabHelper() {}

void ConnectionHelpTabHelper::DidAttachInterstitialPage() {
  GURL::Replacements replacements;
  replacements.ClearRef();
  if (web_contents()->GetURL().ReplaceComponents(replacements) ==
      GetHelpCenterURL()) {
    UMA_HISTOGRAM_ENUMERATION(
        "SSL.CertificateErrorHelpCenterVisited",
        ConnectionHelpTabHelper::LearnMoreClickResult::kFailedWithInterstitial,
        ConnectionHelpTabHelper::LearnMoreClickResult::kLearnMoreResultCount);
    MaybeRedirectToBundledHelp(web_contents());
  }
}

void ConnectionHelpTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  if (web_contents()->GetURL().ReplaceComponents(replacements) ==
      GetHelpCenterURL()) {
    LearnMoreClickResult histogram_value;
    if (navigation_handle->IsErrorPage()) {
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kCommittedInterstitials) &&
          net::IsCertificateError(navigation_handle->GetNetErrorCode())) {
        // When committed interstitials are enabled, DidAttachInterstitialPage
        // does not get called, so check if this navigation resulted in an SSL
        // error.
        histogram_value = ConnectionHelpTabHelper::LearnMoreClickResult::
            kFailedWithInterstitial;
        MaybeRedirectToBundledHelp(web_contents());
      } else {
        histogram_value =
            ConnectionHelpTabHelper::LearnMoreClickResult::kFailedOther;
      }
    } else {
      histogram_value =
          ConnectionHelpTabHelper::LearnMoreClickResult::kSucceeded;
    }
    UMA_HISTOGRAM_ENUMERATION(
        "SSL.CertificateErrorHelpCenterVisited", histogram_value,
        ConnectionHelpTabHelper::LearnMoreClickResult::kLearnMoreResultCount);
  }
}

void ConnectionHelpTabHelper::SetHelpCenterUrlForTesting(const GURL& url) {
  testing_url_ = url;
}

ConnectionHelpTabHelper::ConnectionHelpTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

GURL ConnectionHelpTabHelper::GetHelpCenterURL() {
  if (testing_url_.is_valid())
    return testing_url_;
  return GURL(kHelpCenterConnectionHelpUrl);
}
