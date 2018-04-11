// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/common/ad_delay_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace subresource_filter {

namespace {

void LogSecureInfo(AdDelayThrottle::SecureInfo info) {
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.AdDelay.SecureInfo", info);
}

}  // namespace

constexpr base::TimeDelta AdDelayThrottle::kDefaultDelay;

AdDelayThrottle::Factory::Factory()
    : insecure_delay_(base::TimeDelta::FromMilliseconds(
          base::GetFieldTrialParamByFeatureAsInt(
              kDelayUnsafeAds,
              kInsecureDelayParam,
              kDefaultDelay.InMilliseconds()))),
      delay_enabled_(base::FeatureList::IsEnabled(kAdTagging) &&
                     base::FeatureList::IsEnabled(kDelayUnsafeAds)) {}

AdDelayThrottle::Factory::~Factory() = default;

std::unique_ptr<AdDelayThrottle> AdDelayThrottle::Factory::MaybeCreate(
    std::unique_ptr<AdDelayThrottle::MetadataProvider> provider) const {
  DCHECK(provider);
  return base::WrapUnique(new AdDelayThrottle(std::move(provider),
                                              insecure_delay_, delay_enabled_));
}

// TODO(csharrison): Log metrics for actual delay time.
AdDelayThrottle::~AdDelayThrottle() {
  if (provider_->IsAdRequest()) {
    LogSecureInfo(was_ever_insecure_ ? SecureInfo::kInsecureAd
                                     : SecureInfo::kSecureAd);
  } else {
    LogSecureInfo(was_ever_insecure_ ? SecureInfo::kInsecureNonAd
                                     : SecureInfo::kSecureNonAd);
  }
}

void AdDelayThrottle::DetachFromCurrentSequence() {
  // The throttle is moving to another thread. Ensure this is done before any
  // weak pointers are created.
  DCHECK(!weak_factory_.HasWeakPtrs());
}

void AdDelayThrottle::WillStartRequest(network::ResourceRequest* request,
                                       bool* defer) {
  *defer = MaybeDefer(request->url);
}

void AdDelayThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head,
    bool* defer) {
  // Note: some MetadataProviders may not be able to distinguish requests that
  // are only tagged as ads after a redirect.
  *defer = MaybeDefer(redirect_info.new_url);
}

bool AdDelayThrottle::MaybeDefer(const GURL& url) {
  if (has_deferred_)
    return false;

  // Note: this should probably be using content::IsOriginSecure which accounts
  // for things like whitelisted origins, localhost, etc. This isn't used here
  // because that function is quite expensive for insecure schemes, involving
  // many allocations and string scans.
  was_ever_insecure_ |= url.SchemeIs(url::kHttpScheme);
  if (!was_ever_insecure_ || !provider_->IsAdRequest())
    return false;

  // Bail out right before we'd actually defer if the feature isn't enabled.
  if (!delay_enabled_)
    return false;

  // TODO(csharrison): Consider logging to the console here that Chrome
  // delayed this request.
  has_deferred_ = true;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AdDelayThrottle::Resume, weak_factory_.GetWeakPtr()),
      insecure_delay_);
  return true;
}

void AdDelayThrottle::Resume() {
  delegate_->Resume();
}

AdDelayThrottle::AdDelayThrottle(std::unique_ptr<MetadataProvider> provider,
                                 base::TimeDelta insecure_delay,
                                 bool delay_enabled)
    : content::URLLoaderThrottle(),
      provider_(std::move(provider)),
      insecure_delay_(insecure_delay),
      delay_enabled_(delay_enabled),
      weak_factory_(this) {}

}  // namespace subresource_filter
