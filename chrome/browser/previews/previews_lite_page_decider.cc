// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_decider.h"

#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

PreviewsLitePageDecider::PreviewsLitePageDecider(
    content::BrowserContext* browser_context)
    : clock_(base::DefaultTickClock::GetInstance()),
      page_id_(base::RandUint64()),
      drp_settings_(nullptr) {
  if (browser_context && !browser_context->IsOffTheRecord()) {
    DataReductionProxyChromeSettings* drp_settings =
        DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
            browser_context);
    DCHECK(drp_settings);

    drp_settings_ = drp_settings;
    drp_settings_->AddProxyRequestHeadersObserver(this);
  }
}

PreviewsLitePageDecider::~PreviewsLitePageDecider() = default;

// static
std::unique_ptr<content::NavigationThrottle>
PreviewsLitePageDecider::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  DCHECK(handle);
  DCHECK(handle->GetWebContents());
  DCHECK(handle->GetWebContents()->GetBrowserContext());

  content::BrowserContext* browser_context =
      handle->GetWebContents()->GetBrowserContext();

  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (!previews_service)
    return nullptr;
  DCHECK(!browser_context->IsOffTheRecord());

  PreviewsLitePageDecider* decider =
      previews_service->previews_lite_page_decider();
  DCHECK(decider);

  // TODO(crbug/842233): Replace this logic with PreviewsState.
  bool drp_enabled = decider->drp_settings_->IsDataReductionProxyEnabled();
  bool preview_enabled = previews::params::ArePreviewsAllowed() &&
                         previews::params::IsLitePageServerPreviewsEnabled();

  if (drp_enabled && preview_enabled) {
    return std::make_unique<PreviewsLitePageNavigationThrottle>(handle,
                                                                decider);
  }
  return nullptr;
}

void PreviewsLitePageDecider::OnProxyRequestHeadersChanged(
    const net::HttpRequestHeaders& headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This is done so that successive page ids cannot be used to track users
  // across sessions. These sessions are contained in the chrome-proxy header.
  page_id_ = base::RandUint64();
}

void PreviewsLitePageDecider::Shutdown() {
  if (drp_settings_)
    drp_settings_->RemoveProxyRequestHeadersObserver(this);
}

void PreviewsLitePageDecider::SetClockForTesting(const base::TickClock* clock) {
  clock_ = clock;
}

void PreviewsLitePageDecider::ClearSingleBypassForTesting() {
  single_bypass_.clear();
}

void PreviewsLitePageDecider::SetServerUnavailableFor(
    base::TimeDelta retry_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks retry_at = clock_->NowTicks() + retry_after;
  if (!retry_at_.has_value() || retry_at > retry_at_)
    retry_at_ = retry_at;
}

bool PreviewsLitePageDecider::IsServerUnavailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!retry_at_.has_value())
    return false;
  bool server_loadshedding = retry_at_ > clock_->NowTicks();
  if (!server_loadshedding)
    retry_at_.reset();
  return server_loadshedding;
}

void PreviewsLitePageDecider::AddSingleBypass(std::string url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Garbage collect any old entries while looking for the one for |url|.
  auto entry = single_bypass_.end();
  for (auto iter = single_bypass_.begin(); iter != single_bypass_.end();
       /* no increment */) {
    if (iter->second < clock_->NowTicks()) {
      iter = single_bypass_.erase(iter);
      continue;
    }
    if (iter->first == url)
      entry = iter;
    ++iter;
  }

  // Update the entry for |url|.
  const base::TimeTicks ttl =
      clock_->NowTicks() + base::TimeDelta::FromMinutes(5);
  if (entry == single_bypass_.end()) {
    single_bypass_.emplace(url, ttl);
    return;
  }
  entry->second = ttl;
}

bool PreviewsLitePageDecider::CheckSingleBypass(std::string url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto entry = single_bypass_.find(url);
  if (entry == single_bypass_.end())
    return false;
  return entry->second >= clock_->NowTicks();
}

uint64_t PreviewsLitePageDecider::GeneratePageID() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++page_id_;
}
