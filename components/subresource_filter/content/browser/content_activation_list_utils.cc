// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_activation_list_utils.h"

#include <string>

namespace subresource_filter {

ActivationList GetListForThreatTypeAndMetadata(
    safe_browsing::SBThreatType threat_type,
    const safe_browsing::ThreatMetadata& threat_type_metadata) {
  bool is_phishing_interstitial =
      (threat_type == safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  bool is_soc_engineering_ads_interstitial =
      threat_type_metadata.threat_pattern_type ==
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS;
  bool subresource_filter =
      (threat_type == safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
  if (is_phishing_interstitial) {
    if (is_soc_engineering_ads_interstitial) {
      return ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL;
    }
    return ActivationList::PHISHING_INTERSTITIAL;
  } else if (subresource_filter) {
    // TODO(crbug.com/761385): For now, hardcode when we have both ABUSIVE and
    // BETTER_ADS. In the future these will be separated.
    bool has_abusive = false;
    bool has_better_ads = false;
    for (const auto& match : threat_type_metadata.subresource_filter_match) {
      if (match.first == safe_browsing::SubresourceFilterType::ABUSIVE)
        has_abusive = true;
      else if (match.first == safe_browsing::SubresourceFilterType::BETTER_ADS)
        has_better_ads = true;
    }

    if (has_abusive && has_better_ads)
      return ActivationList::ALL_ADS;
    if (has_abusive)
      return ActivationList::ABUSIVE_ADS;
    if (has_better_ads)
      return ActivationList::BETTER_ADS;
    return ActivationList::SUBRESOURCE_FILTER;
  }

  return ActivationList::NONE;
}

}  // namespace subresource_filter
