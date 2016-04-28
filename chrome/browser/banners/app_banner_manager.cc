// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager.h"

#include "chrome/browser/banners/app_banner_data_fetcher.h"
#include "chrome/browser/banners/app_banner_debug_log.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/origin_util.h"
#include "net/base/load_flags.h"

namespace {
bool gDisableSecureCheckForTesting = false;
}  // anonymous namespace

namespace banners {

void AppBannerManager::DisableSecureSchemeCheckForTesting() {
  gDisableSecureCheckForTesting = true;
}

void AppBannerManager::SetEngagementWeights(double direct_engagement,
                                            double indirect_engagement) {
  AppBannerSettingsHelper::SetEngagementWeights(direct_engagement,
                                                indirect_engagement);
}

bool AppBannerManager::URLsAreForTheSamePage(const GURL& first,
                                             const GURL& second) {
  return first.GetWithEmptyPath() == second.GetWithEmptyPath() &&
         first.path() == second.path() && first.query() == second.query();
}

AppBannerManager::AppBannerManager()
    : data_fetcher_(nullptr),
      weak_factory_(this) {
  AppBannerSettingsHelper::UpdateFromFieldTrial();
}

AppBannerManager::AppBannerManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      data_fetcher_(nullptr),
      weak_factory_(this) {
  AppBannerSettingsHelper::UpdateFromFieldTrial();
}

AppBannerManager::~AppBannerManager() {
  CancelActiveFetcher();
}

void AppBannerManager::ReplaceWebContents(content::WebContents* web_contents) {
  Observe(web_contents);
  if (data_fetcher_.get())
    data_fetcher_.get()->ReplaceWebContents(web_contents);
}

bool AppBannerManager::IsFetcherActive() {
  return data_fetcher_ != nullptr && data_fetcher_->is_active();
}

void AppBannerManager::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  last_transition_type_ = params.transition;
}

void AppBannerManager::RequestAppBanner(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_debug_mode) {
  if (render_frame_host->GetParent()) {
    OutputDeveloperNotShownMessage(web_contents(), kNotLoadedInMainFrame,
                                   is_debug_mode);
    return;
  }

  if (data_fetcher_.get() && data_fetcher_->is_active() &&
      URLsAreForTheSamePage(data_fetcher_->validated_url(), validated_url) &&
      !is_debug_mode) {
    return;
  }

  // A secure origin is required to show banners, so exit early if we see the
  // URL is invalid.
  if (!content::IsOriginSecure(validated_url) &&
      !gDisableSecureCheckForTesting) {
    OutputDeveloperNotShownMessage(web_contents(), kNotServedFromSecureOrigin,
                                   is_debug_mode);
    return;
  }

  // Kick off the data retrieval pipeline.
  data_fetcher_ =
      CreateAppBannerDataFetcher(weak_factory_.GetWeakPtr(), is_debug_mode);
  data_fetcher_->Start(validated_url, last_transition_type_);
}

void AppBannerManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // The third argument is the is_debug_mode boolean value, which is true only
  // when it is triggered by the developer's action in DevTools.
  RequestAppBanner(render_frame_host, validated_url, false /* is_debug_mode */);
}

bool AppBannerManager::HandleNonWebApp(const std::string& platform,
                                       const GURL& url,
                                       const std::string& id,
                                       bool is_debug_mode) {
  return false;
}

void AppBannerManager::CancelActiveFetcher() {
  if (data_fetcher_ != nullptr) {
    data_fetcher_->Cancel();
    data_fetcher_ = nullptr;
  }
}

}  // namespace banners
