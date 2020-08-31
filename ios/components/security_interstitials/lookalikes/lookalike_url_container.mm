// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"

#include "base/memory/ptr_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(LookalikeUrlContainer)

LookalikeUrlContainer::LookalikeUrlContainer(web::WebState* web_state) {}

LookalikeUrlContainer::LookalikeUrlContainer(LookalikeUrlContainer&& other) =
    default;

LookalikeUrlContainer& LookalikeUrlContainer::operator=(
    LookalikeUrlContainer&& other) = default;

LookalikeUrlContainer::~LookalikeUrlContainer() = default;

LookalikeUrlContainer::InterstitialParams::InterstitialParams() = default;

LookalikeUrlContainer::InterstitialParams::~InterstitialParams() = default;

LookalikeUrlContainer::InterstitialParams::InterstitialParams(
    const InterstitialParams& other) = default;

void LookalikeUrlContainer::RecordLookalikeBlockingPageParams(
    const GURL& url,
    const web::Referrer& referrer,
    const std::vector<GURL>& redirect_chain) {
  interstitial_params_->url = url;
  interstitial_params_->referrer = referrer;
  interstitial_params_->redirect_chain = redirect_chain;
}

std::unique_ptr<LookalikeUrlContainer::InterstitialParams>
LookalikeUrlContainer::ReleaseInterstitialParams() {
  return std::move(interstitial_params_);
}
