// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager.h"

#include "chrome/browser/banners/app_banner_data_fetcher.h"
#include "chrome/browser/banners/app_banner_debug_log.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/origin_util.h"

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

AppBannerManager::AppBannerManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      SiteEngagementObserver(nullptr),
      data_fetcher_(nullptr),
      banner_request_queued_(false),
      load_finished_(false),
      weak_factory_(this) {
  AppBannerSettingsHelper::UpdateFromFieldTrial();
}

AppBannerManager::~AppBannerManager() {
  CancelActiveFetcher();
}

void AppBannerManager::ReplaceWebContents(content::WebContents* web_contents) {
  content::WebContentsObserver::Observe(web_contents);
  if (data_fetcher_.get())
    data_fetcher_.get()->ReplaceWebContents(web_contents);
}

bool AppBannerManager::IsFetcherActive() {
  return data_fetcher_ && data_fetcher_->is_active();
}

void AppBannerManager::RequestAppBanner(const GURL& validated_url,
                                        bool is_debug_mode) {
  content::WebContents* contents = web_contents();
  if (contents->GetMainFrame()->GetParent()) {
    OutputDeveloperNotShownMessage(contents, kNotLoadedInMainFrame,
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
    OutputDeveloperNotShownMessage(contents, kNotServedFromSecureOrigin,
                                   is_debug_mode);
    return;
  }

  // Kick off the data retrieval pipeline.
  data_fetcher_ =
      CreateAppBannerDataFetcher(weak_factory_.GetWeakPtr(), is_debug_mode);
  data_fetcher_->Start(validated_url, last_transition_type_);
}

void AppBannerManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  load_finished_ = false;
  if (AppBannerSettingsHelper::ShouldUseSiteEngagementScore() &&
      GetSiteEngagementService() == nullptr) {
    // Ensure that we are observing the site engagement service on navigation
    // start. This may be the first navigation, or we may have stopped
    // observing if the banner flow was triggered on the previous page.
    SiteEngagementObserver::Observe(SiteEngagementService::Get(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())));
  }
}

void AppBannerManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted())
    last_transition_type_ = navigation_handle->GetPageTransition();
}

void AppBannerManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // Don't start the banner flow unless the main frame has finished loading.
  if (render_frame_host->GetParent())
    return;

  load_finished_ = true;
  if (!AppBannerSettingsHelper::ShouldUseSiteEngagementScore() ||
      banner_request_queued_) {
    banner_request_queued_ = false;

    // The third argument is the is_debug_mode boolean value, which is true only
    // when it is triggered by the developer's action in DevTools.
    RequestAppBanner(validated_url, false /* is_debug_mode */);
  }
}

void AppBannerManager::MediaStartedPlaying(const MediaPlayerId& id) {
  active_media_players_.push_back(id);
}

void AppBannerManager::MediaStoppedPlaying(const MediaPlayerId& id) {
  active_media_players_.erase(std::remove(active_media_players_.begin(),
                                          active_media_players_.end(), id),
                              active_media_players_.end());
}

bool AppBannerManager::HandleNonWebApp(const std::string& platform,
                                       const GURL& url,
                                       const std::string& id,
                                       bool is_debug_mode) {
  return false;
}

void AppBannerManager::OnEngagementIncreased(content::WebContents* contents,
                                             const GURL& url,
                                             double score) {
  // Only trigger a banner using site engagement if:
  //  1. engagement increased for the web contents which we are attached to; and
  //  2. there are no currently active media players; and
  //  3. we have accumulated sufficient engagement.
  if (web_contents() == contents && active_media_players_.empty() &&
      AppBannerSettingsHelper::HasSufficientEngagement(score)) {
    // Stop observing so we don't double-trigger the banner.
    SiteEngagementObserver::Observe(nullptr);

    if (!load_finished_) {
      // Wait until the main frame finishes loading before requesting a banner.
      banner_request_queued_ = true;
    } else {
      // Requesting a banner performs some simple tests, creates a data fetcher,
      // and starts some asynchronous checks to test installability. It should
      // be safe to start this in response to user input.
      RequestAppBanner(url, false /* is_debug_mode */);
    }
  }
}

void AppBannerManager::CancelActiveFetcher() {
  if (data_fetcher_) {
    data_fetcher_->Cancel();
    data_fetcher_ = nullptr;
  }
}

}  // namespace banners
