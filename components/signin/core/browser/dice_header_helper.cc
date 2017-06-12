// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/dice_header_helper.h"

#include "components/signin/core/common/profile_management_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

namespace signin {

bool DiceHeaderHelper::IsUrlEligibleForRequestHeader(const GURL& url) {
  if (switches::GetAccountConsistencyMethod() !=
      switches::AccountConsistencyMethod::kDice) {
    return false;
  }
  return gaia::IsGaiaSignonRealm(url.GetOrigin());
}

std::string DiceHeaderHelper::BuildRequestHeader(bool is_header_request,
                                                 const GURL& url,
                                                 const std::string& account_id,
                                                 int profile_mode_mask) {
  return "client_id=" + GaiaUrls::GetInstance()->oauth2_chrome_client_id();
}

}  // namespace signin
