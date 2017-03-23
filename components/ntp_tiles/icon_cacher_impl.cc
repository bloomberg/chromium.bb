// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/icon_cacher_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/favicon_util.h"
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
      image_fetcher_(std::move(image_fetcher)),
      weak_ptr_factory_(this) {
  image_fetcher_->SetDataUseServiceName(
      data_use_measurement::DataUseUserData::NTP_TILES);
  // For images with multiple frames, prefer one of size 128x128px.
  image_fetcher_->SetDesiredImageFrameSize(
      gfx::Size(kDesiredFrameSize, kDesiredFrameSize));
}

IconCacherImpl::~IconCacherImpl() = default;

void IconCacherImpl::StartFetch(
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
      base::Bind(&IconCacherImpl::OnFaviconDownloaded, base::Unretained(this),
                 site, base::Passed(std::move(preliminary_callback)),
                 icon_available));
}

void IconCacherImpl::OnFaviconDownloaded(
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

}  // namespace ntp_tiles
