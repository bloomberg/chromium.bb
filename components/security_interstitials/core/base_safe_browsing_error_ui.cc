// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"

namespace security_interstitials {

BaseSafeBrowsingErrorUI::BaseSafeBrowsingErrorUI(
    const GURL& request_url,
    const GURL& main_frame_url,
    BaseSafeBrowsingErrorUI::SBInterstitialReason reason,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
    const std::string& app_locale,
    const base::Time& time_triggered,
    ControllerClient* controller)
    : request_url_(request_url),
      main_frame_url_(main_frame_url),
      interstitial_reason_(reason),
      display_options_(display_options),
      app_locale_(app_locale),
      time_triggered_(time_triggered),
      controller_(controller) {}

BaseSafeBrowsingErrorUI::~BaseSafeBrowsingErrorUI() {}

}  // security_interstitials
