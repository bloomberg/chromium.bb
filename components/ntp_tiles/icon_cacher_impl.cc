// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/icon_cacher_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_util.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace ntp_tiles {

namespace {

constexpr int kDesiredFrameSize = 128;

// TODO(jkrcal): Make the size in dip and the scale factor be passed as
// arguments from the UI so that we desire for the right size on a given device.
// See crbug.com/696563.
constexpr int kTileIconMinSizePx = 48;
constexpr int kTileIconDesiredSizePx = 96;

favicon_base::IconType IconType(const PopularSites::Site& site) {
  return site.large_icon_url.is_valid() ? favicon_base::TOUCH_ICON
                                        : favicon_base::FAVICON;
}

const GURL& IconURL(const PopularSites::Site& site) {
  return site.large_icon_url.is_valid() ? site.large_icon_url
                                        : site.favicon_url;
}

bool HasResultDefaultBackgroundColor(
    const favicon_base::LargeIconResult& result) {
  if (!result.fallback_icon_style) {
    return false;
  }
  return result.fallback_icon_style->is_default_background_color;
}

}  // namespace

IconCacherImpl::IconCacherImpl(
    favicon::FaviconService* favicon_service,
    favicon::LargeIconService* large_icon_service,
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher)
    : favicon_service_(favicon_service),
      large_icon_service_(large_icon_service),
      image_fetcher_(std::move(image_fetcher)),
      weak_ptr_factory_(this) {
  image_fetcher_->SetDataUseServiceName(
      data_use_measurement::DataUseUserData::NTP_TILES);
  // For images with multiple frames, prefer one of size 128x128px.
  image_fetcher_->SetDesiredImageFrameSize(
      gfx::Size(kDesiredFrameSize, kDesiredFrameSize));
}

IconCacherImpl::~IconCacherImpl() = default;

void IconCacherImpl::StartFetchPopularSites(
    PopularSites::Site site,
    const base::Closure& icon_available,
    const base::Closure& preliminary_icon_available) {
  // Copy values from |site| before it is moved.
  GURL site_url = site.url;
  favicon_base::IconType icon_type = IconType(site);
  favicon::GetFaviconImageForPageURL(
      favicon_service_, site_url, icon_type,
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

  std::unique_ptr<CancelableImageCallback> preliminary_callback =
      MaybeProvideDefaultIcon(site, preliminary_icon_available);

  image_fetcher_->StartOrQueueNetworkRequest(
      std::string(), IconURL(site),
      base::Bind(&IconCacherImpl::OnPopularSitesFaviconDownloaded,
                 base::Unretained(this), site,
                 base::Passed(std::move(preliminary_callback)),
                 icon_available));
}

void IconCacherImpl::OnPopularSitesFaviconDownloaded(
    PopularSites::Site site,
    std::unique_ptr<CancelableImageCallback> preliminary_callback,
    const base::Closure& icon_available,
    const std::string& id,
    const gfx::Image& fetched_image,
    const image_fetcher::RequestMetadata& metadata) {
  if (fetched_image.IsEmpty()) {
    return;
  }

  // Avoid invoking callback about preliminary icon to be triggered. The best
  // possible icon has already been downloaded.
  if (preliminary_callback) {
    preliminary_callback->Cancel();
  }
  SaveAndNotifyIconForSite(site, icon_available, fetched_image);
}

void IconCacherImpl::SaveAndNotifyIconForSite(
    const PopularSites::Site& site,
    const base::Closure& icon_available,
    const gfx::Image& image) {
  // Although |SetFaviconColorSpace| affects OSX only, copies of gfx::Images are
  // just copies of the reference to the image and therefore cheap.
  gfx::Image img(image);
  favicon_base::SetFaviconColorSpace(&img);

  favicon_service_->SetFavicons(site.url, IconURL(site), IconType(site),
                                std::move(img));

  if (icon_available) {
    icon_available.Run();
  }
}

std::unique_ptr<IconCacherImpl::CancelableImageCallback>
IconCacherImpl::MaybeProvideDefaultIcon(const PopularSites::Site& site,
                                        const base::Closure& icon_available) {
  if (site.default_icon_resource < 0) {
    return std::unique_ptr<CancelableImageCallback>();
  }
  std::unique_ptr<CancelableImageCallback> preliminary_callback(
      new CancelableImageCallback(
          base::Bind(&IconCacherImpl::SaveAndNotifyIconForSite,
                     weak_ptr_factory_.GetWeakPtr(), site, icon_available)));
  image_fetcher_->GetImageDecoder()->DecodeImage(
      ResourceBundle::GetSharedInstance()
          .GetRawDataResource(site.default_icon_resource)
          .as_string(),
      gfx::Size(kDesiredFrameSize, kDesiredFrameSize),
      preliminary_callback->callback());
  return preliminary_callback;
}

void IconCacherImpl::StartFetchMostLikely(const GURL& page_url,
                                          const base::Closure& icon_available) {
  // Desired size 0 means that we do not want the service to resize the image
  // (as we will not use it anyway).
  large_icon_service_->GetLargeIconOrFallbackStyle(
      page_url, kTileIconMinSizePx, /*desired_size_in_pixel=*/0,
      base::Bind(&IconCacherImpl::OnGetLargeIconOrFallbackStyleFinished,
                 weak_ptr_factory_.GetWeakPtr(), page_url, icon_available),
      &tracker_);
}

void IconCacherImpl::OnGetLargeIconOrFallbackStyleFinished(
    const GURL& page_url,
    const base::Closure& icon_available,
    const favicon_base::LargeIconResult& result) {
  if (!HasResultDefaultBackgroundColor(result)) {
    // We should only fetch for default "gray" tiles so that we never overrite
    // any favicon of any size.
    return;
  }

  large_icon_service_
      ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          page_url, kTileIconMinSizePx, kTileIconDesiredSizePx,
          base::Bind(&IconCacherImpl::OnMostLikelyFaviconDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), icon_available));
}

void IconCacherImpl::OnMostLikelyFaviconDownloaded(
    const base::Closure& icon_available,
    bool success) {
  if (!success) {
    return;
  }
  if (icon_available) {
    icon_available.Run();
  }
}

}  // namespace ntp_tiles
