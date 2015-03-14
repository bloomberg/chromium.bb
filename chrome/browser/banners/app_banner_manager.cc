// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager.h"

#include "base/metrics/field_trial.h"
#include "chrome/browser/banners/app_banner_data_fetcher.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "net/base/load_flags.h"
#include "ui/gfx/screen.h"

namespace {
bool gDisableSecureCheckForTesting = false;
}  // anonymous namespace

namespace banners {

AppBannerManager::AppBannerManager(int icon_size)
    : ideal_icon_size_(icon_size),
      data_fetcher_(nullptr),
      weak_factory_(this) {
}

AppBannerManager::~AppBannerManager() {
  CancelActiveFetcher();
}

void AppBannerManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->GetParent())
    return;

  // A secure scheme is required to show banners, so exit early if we see the
  // URL is invalid.
  if (!validated_url.SchemeIsSecure() && !gDisableSecureCheckForTesting)
    return;

  // Kick off the data retrieval pipeline.
  data_fetcher_ = CreateAppBannerDataFetcher(weak_factory_.GetWeakPtr(),
                                             ideal_icon_size_);
  data_fetcher_->Start(validated_url);
}


bool AppBannerManager::OnInvalidManifest(AppBannerDataFetcher* fetcher) {
  return false;
}

void AppBannerManager::ReplaceWebContents(content::WebContents* web_contents) {
  Observe(web_contents);
  if (data_fetcher_.get())
    data_fetcher_.get()->ReplaceWebContents(web_contents);
}

AppBannerDataFetcher* AppBannerManager::CreateAppBannerDataFetcher(
    base::WeakPtr<AppBannerDataFetcher::Delegate> weak_delegate,
    const int ideal_icon_size) {
  return new AppBannerDataFetcher(web_contents(), weak_delegate,
                                  ideal_icon_size);
}

void AppBannerManager::CancelActiveFetcher() {
  if (data_fetcher_ != nullptr) {
    data_fetcher_->Cancel();
    data_fetcher_ = nullptr;
  }
}

bool AppBannerManager::IsFetcherActive() {
  return data_fetcher_ != nullptr && data_fetcher_->is_active();
}

void AppBannerManager::DisableSecureSchemeCheckForTesting() {
  gDisableSecureCheckForTesting = true;
}

bool AppBannerManager::IsEnabled() {
  return base::FieldTrialList::FindFullName("AppBanners") == "Enabled";
}

}  // namespace banners
