// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_activation_list_utils.h"

namespace subresource_filter {

ActivationList GetListForThreatTypeAndMetadata(
    safe_browsing::SBThreatType threat_type,
    safe_browsing::ThreatPatternType threat_type_metadata) {
  bool is_phishing_interstitial =
      (threat_type == safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  bool is_soc_engineering_ads_interstitial =
      threat_type_metadata ==
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS;
  if (is_phishing_interstitial) {
    if (is_soc_engineering_ads_interstitial) {
      return ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL;
    }
    return ActivationList::PHISHING_INTERSTITIAL;
  }
  return ActivationList::NONE;
}

}  // namespace subresource_filter
