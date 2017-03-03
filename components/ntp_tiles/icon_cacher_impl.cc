// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/icon_cacher_impl.h"

#include <utility>

#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_util.h"
#include "components/image_fetcher/image_fetcher.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace ntp_tiles {

namespace {

favicon_base::IconType IconType(const PopularSites::Site& site) {
  return site.large_icon_url.is_valid() ? favicon_base::TOUCH_ICON
                                        : favicon_base::FAVICON;
}

const GURL& IconURL(const PopularSites::Site& site) {
  return site.large_icon_url.is_valid() ? site.large_icon_url
                                        : site.favicon_url;
}

}  // namespace

IconCacherImpl::IconCacherImpl(
    favicon::FaviconService* favicon_service,
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher)
    : favicon_service_(favicon_service),
      image_fetcher_(std::move(image_fetcher)) {
  image_fetcher_->SetDataUseServiceName(
      data_use_measurement::DataUseUserData::NTP_TILES);
  // For images with multiple frames, prefer one of size 128x128px.
  image_fetcher_->SetDesiredImageFrameSize(gfx::Size(128, 128));
}

IconCacherImpl::~IconCacherImpl() = default;

void IconCacherImpl::StartFetch(
    PopularSites::Site site,
    const base::Closure& icon_available,
    const base::Closure& preliminary_icon_available) {
  favicon::GetFaviconImageForPageURL(
      favicon_service_, site.url, IconType(site),
      base::Bind(&IconCacherImpl::OnGetFaviconImageForPageURLFinished,
                 base::Unretained(this), std::move(site), icon_available,
                 preliminary_icon_available),
      &tracker_);
}

void IconCacherImpl::OnGetFaviconImageForPageURLFinished(
    PopularSites::Site site,
    const base::Closure& icon_available,
    const base::Closure& preliminary_icon_available,
    const favicon_base::FaviconImageResult& result) {
  if (!result.image.IsEmpty()) {
    return;
  }
  if (ProvideDefaultIcon(site) && !preliminary_icon_available.is_null()) {
    preliminary_icon_available.Run();
  }

  image_fetcher_->StartOrQueueNetworkRequest(
      std::string(), IconURL(site),
      base::Bind(&IconCacherImpl::OnFaviconDownloaded, base::Unretained(this),
                 site, icon_available));
}

void IconCacherImpl::OnFaviconDownloaded(PopularSites::Site site,
                                         const base::Closure& icon_available,
                                         const std::string& id,
                                         const gfx::Image& fetched_image) {
  if (fetched_image.IsEmpty()) {
    return;
  }

  SaveIconForSite(site, fetched_image);
  if (icon_available) {
    icon_available.Run();
  }
}

void IconCacherImpl::SaveIconForSite(const PopularSites::Site& site,
                                     gfx::Image image) {
  favicon_base::SetFaviconColorSpace(&image);
  favicon_service_->SetFavicons(site.url, IconURL(site), IconType(site), image);
}

bool IconCacherImpl::ProvideDefaultIcon(const PopularSites::Site& site) {
  if (site.default_icon_resource < 0) {
    return false;
  }
  SaveIconForSite(site, ResourceBundle::GetSharedInstance().GetNativeImageNamed(
                            site.default_icon_resource));
  return true;
}

}  // namespace ntp_tiles
