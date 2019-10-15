// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tips/local_heuristics.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "chrome/browser/lookalikes/lookalike_url_interstitial_page.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "components/security_state/core/features.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"

namespace {

const base::FeatureParam<bool> kEnableLookalikeEditDistance{
    &security_state::features::kSafetyTipUI, "editdistance", false};
const base::FeatureParam<bool> kEnableLookalikeEditDistanceSiteEngagement{
    &security_state::features::kSafetyTipUI, "editdistance_siteengagement",
    false};

}  // namespace

namespace safety_tips {

bool ShouldTriggerSafetyTipFromLookalike(
    const GURL& url,
    const lookalikes::DomainInfo& navigated_domain,
    const std::vector<lookalikes::DomainInfo>& engaged_sites,
    GURL* safe_url) {
  std::string matched_domain;
  LookalikeUrlInterstitialPage::MatchType match_type;

  if (!lookalikes::LookalikeUrlNavigationThrottle::GetMatchingDomain(
          navigated_domain, engaged_sites, &matched_domain, &match_type)) {
    return false;
  }

  // If we're already displaying an interstitial, don't warn again.
  if (lookalikes::LookalikeUrlNavigationThrottle::ShouldDisplayInterstitial(
          match_type, navigated_domain)) {
    return false;
  }

  *safe_url = GURL(std::string(url::kHttpScheme) +
                   url::kStandardSchemeSeparator + matched_domain);
  // Edit distance has higher false positives, so it gets its own feature param
  if (match_type == LookalikeUrlInterstitialPage::MatchType::kEditDistance) {
    return kEnableLookalikeEditDistance.Get();
  }
  if (match_type ==
      LookalikeUrlInterstitialPage::MatchType::kEditDistanceSiteEngagement) {
    return kEnableLookalikeEditDistanceSiteEngagement.Get();
  }

  return true;
}

bool ShouldTriggerSafetyTipFromKeywordInURL(
    const GURL& url,
    const char* const sensitive_keywords[],
    size_t num_keywords) {
  // "eTLD + 1 - 1": "www.google.com" -> "google"
  std::string eTLD_plusminus;
  base::TrimString(url_formatter::top_domains::HostnameWithoutRegistry(
                       lookalikes::GetETLDPlusOne(url.host())),
                   ".", &eTLD_plusminus);
  DCHECK(eTLD_plusminus.find('.') == std::string::npos);

  const std::vector<std::string> eTLD_plusminus_parts = base::SplitString(
      eTLD_plusminus, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // We only care about finding a keyword here if there's more than one part to
  // the tokenized eTLD+1-1.
  if (eTLD_plusminus_parts.size() <= 1) {
    return false;
  }

  for (const auto& eTLD_plusminus_part : eTLD_plusminus_parts) {
    // We use a custom comparator for (char *) here, to avoid the costly
    // construction of two std::strings every time two values are compared,
    // and because (char *) orders by address, not lexicographically.
    if (std::binary_search(sensitive_keywords,
                           sensitive_keywords + num_keywords,
                           eTLD_plusminus_part.c_str(),
                           [](const char* str_one, const char* str_two) {
                             return strcmp(str_one, str_two) < 0;
                           })) {
      return true;
    }
  }

  return false;
}

}  // namespace safety_tips
