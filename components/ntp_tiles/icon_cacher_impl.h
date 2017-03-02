// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ICON_CACHER_IMPL_H_
#define COMPONENTS_NTP_TILES_ICON_CACHER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/popular_sites.h"

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace favicon_base {
struct FaviconImageResult;
}  // namespace favicon_base

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace ntp_tiles {

class IconCacherImpl : public IconCacher {
 public:
  IconCacherImpl(favicon::FaviconService* favicon_service,
                 std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher);
  ~IconCacherImpl() override;

  void StartFetch(PopularSites::Site site,
                  const base::Closure& icon_available,
                  const base::Closure& preliminary_icon_available) override;

 private:
  void OnGetFaviconImageForPageURLFinished(
      PopularSites::Site site,
      const base::Closure& icon_available,
      const base::Closure& preliminary_icon_available,
      const favicon_base::FaviconImageResult& result);

  void OnFaviconDownloaded(PopularSites::Site site,
                           const base::Closure& icon_available,
                           const std::string& id,
                           const gfx::Image& fetched_image);

  bool ProvideDefaultIcon(const PopularSites::Site& site);
  void SaveIconForSite(const PopularSites::Site& site, const gfx::Image image);

  base::CancelableTaskTracker tracker_;
  favicon::FaviconService* const favicon_service_;
  std::unique_ptr<image_fetcher::ImageFetcher> const image_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(IconCacherImpl);
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_ICON_CACHER_IMPL_H_
