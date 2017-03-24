// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_util.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_canon.h"

namespace favicon {
namespace {

const char kGoogleServerV2RequestFormat[] =
    "https://t0.gstatic.com/"
    "faviconV2?user=chrome&drop_404_icon=true&size=%d&min_size=%d&max_size=%d&"
    "fallback_opts=TYPE&url=%s";
const int kGoogleServerV2MaxSizeInPixel = 256;
const int kGoogleServerV2DesiredSizeInPixel = 192;

GURL TrimPageUrlForGoogleServer(const GURL& page_url) {
  if (!page_url.SchemeIsHTTPOrHTTPS() || page_url.HostIsIPAddress())
    return GURL();

  url::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  return page_url.ReplaceComponents(replacements);
}

GURL GetIconUrlForGoogleServerV2(const GURL& page_url,
                                 int min_source_size_in_pixel) {
  return GURL(base::StringPrintf(
      kGoogleServerV2RequestFormat, kGoogleServerV2DesiredSizeInPixel,
      min_source_size_in_pixel, kGoogleServerV2MaxSizeInPixel,
      page_url.spec().c_str()));
}

// Processes the bitmap data returned from the FaviconService as part of a
// LargeIconService request.
class LargeIconWorker : public base::RefCountedThreadSafe<LargeIconWorker> {
 public:
  LargeIconWorker(int min_source_size_in_pixel,
                  int desired_size_in_pixel,
                  favicon_base::LargeIconCallback callback,
                  scoped_refptr<base::TaskRunner> background_task_runner,
                  base::CancelableTaskTracker* tracker);

  // Must run on the owner (UI) thread in production.
  // Intermediate callback for GetLargeIconOrFallbackStyle(). Invokes
  // ProcessIconOnBackgroundThread() so we do not perform complex image
  // operations on the UI thread.
  void OnIconLookupComplete(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

 private:
  friend class base::RefCountedThreadSafe<LargeIconWorker>;

  ~LargeIconWorker();

  // Must run on a background thread in production.
  // Tries to resize |bitmap_result_| and pass the output to |callback_|. If
  // that does not work, computes the icon fallback style and uses it to
  // invoke |callback_|. This must be run on a background thread because image
  // resizing and dominant color extraction can be expensive.
  void ProcessIconOnBackgroundThread();

  // Must run on a background thread in production.
  // If |bitmap_result_| is square and large enough (>= |min_source_in_pixel_|),
  // resizes it to |desired_size_in_pixel_| (but if |desired_size_in_pixel_| is
  // 0 then don't resize). If successful, stores the resulting bitmap data
  // into |resized_bitmap_result| and returns true.
  bool ResizeLargeIconOnBackgroundThreadIfValid(
      favicon_base::FaviconRawBitmapResult* resized_bitmap_result);

  // Must run on the owner (UI) thread in production.
  // Invoked when ProcessIconOnBackgroundThread() is done.
  void OnIconProcessingComplete();

  int min_source_size_in_pixel_;
  int desired_size_in_pixel_;
  favicon_base::LargeIconCallback callback_;
  scoped_refptr<base::TaskRunner> background_task_runner_;
  base::CancelableTaskTracker* tracker_;
  favicon_base::FaviconRawBitmapResult bitmap_result_;
  std::unique_ptr<favicon_base::LargeIconResult> result_;

  DISALLOW_COPY_AND_ASSIGN(LargeIconWorker);
};

LargeIconWorker::LargeIconWorker(
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback callback,
    scoped_refptr<base::TaskRunner> background_task_runner,
    base::CancelableTaskTracker* tracker)
    : min_source_size_in_pixel_(min_source_size_in_pixel),
      desired_size_in_pixel_(desired_size_in_pixel),
      callback_(callback),
      background_task_runner_(background_task_runner),
      tracker_(tracker) {
}

LargeIconWorker::~LargeIconWorker() {
}

void LargeIconWorker::OnIconLookupComplete(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  bitmap_result_ = bitmap_result;
  tracker_->PostTaskAndReply(
      background_task_runner_.get(), FROM_HERE,
      base::Bind(&LargeIconWorker::ProcessIconOnBackgroundThread, this),
      base::Bind(&LargeIconWorker::OnIconProcessingComplete, this));
}

void LargeIconWorker::ProcessIconOnBackgroundThread() {
  favicon_base::FaviconRawBitmapResult resized_bitmap_result;
  if (ResizeLargeIconOnBackgroundThreadIfValid(&resized_bitmap_result)) {
    result_.reset(
        new favicon_base::LargeIconResult(resized_bitmap_result));
  } else {
    // Failed to resize |bitmap_result_|, so compute fallback icon style.
    std::unique_ptr<favicon_base::FallbackIconStyle> fallback_icon_style(
        new favicon_base::FallbackIconStyle());
    if (bitmap_result_.is_valid()) {
      favicon_base::SetDominantColorAsBackground(
          bitmap_result_.bitmap_data, fallback_icon_style.get());
    }
    result_.reset(
        new favicon_base::LargeIconResult(fallback_icon_style.release()));
  }
}

bool LargeIconWorker::ResizeLargeIconOnBackgroundThreadIfValid(
    favicon_base::FaviconRawBitmapResult* resized_bitmap_result) {
  // Require bitmap to be valid and square.
  if (!bitmap_result_.is_valid() ||
      bitmap_result_.pixel_size.width() != bitmap_result_.pixel_size.height())
    return false;

  // Require bitmap to be large enough. It's square, so just check width.
  if (bitmap_result_.pixel_size.width() < min_source_size_in_pixel_)
    return false;

  *resized_bitmap_result = bitmap_result_;

  // Special case: Can use |bitmap_result_| as is.
  if (desired_size_in_pixel_ == 0 ||
      bitmap_result_.pixel_size.width() == desired_size_in_pixel_)
    return true;

  // Resize bitmap: decode PNG, resize, and re-encode PNG.
  SkBitmap decoded_bitmap;
  if (!gfx::PNGCodec::Decode(bitmap_result_.bitmap_data->front(),
          bitmap_result_.bitmap_data->size(), &decoded_bitmap))
    return false;

  SkBitmap resized_bitmap = skia::ImageOperations::Resize(
      decoded_bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
      desired_size_in_pixel_, desired_size_in_pixel_);

  std::vector<unsigned char> bitmap_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(resized_bitmap, false, &bitmap_data))
    return false;

  resized_bitmap_result->pixel_size =
      gfx::Size(desired_size_in_pixel_, desired_size_in_pixel_);
  resized_bitmap_result->bitmap_data =
      base::RefCountedBytes::TakeVector(&bitmap_data);
  return true;
}

void LargeIconWorker::OnIconProcessingComplete() {
  callback_.Run(*result_);
}

void OnFetchIconFromGoogleServerComplete(
    FaviconService* favicon_service,
    const GURL& page_url,
    const base::Callback<void(bool success)>& callback,
    const std::string& icon_url,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  if (image.IsEmpty()) {
    DLOG(WARNING) << "large icon server fetch empty " << icon_url;
    favicon_service->UnableToDownloadFavicon(GURL(icon_url));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }

  // TODO(crbug.com/699542): Extract the original icon url from the response
  // headers if available and use it instead of |icon_url|. Possibly the type
  // too, because using TOUCH_ICON is a hacky way that allows us to not
  // interfere with sync.

  // Write fetched icons to FaviconService's cache, but only if no icon was
  // available (clients are encouraged to do this in advance, but meanwhile
  // something else could've been written). By marking the icons initially
  // expired (out-of-date), they will be refetched when we visit the original
  // page any time in the future.
  favicon_service->SetLastResortFavicons(page_url, GURL(icon_url),
                                         favicon_base::IconType::TOUCH_ICON,
                                         image, callback);
}

}  // namespace

LargeIconService::LargeIconService(
    FaviconService* favicon_service,
    const scoped_refptr<base::TaskRunner>& background_task_runner,
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher)
    : favicon_service_(favicon_service),
      background_task_runner_(background_task_runner),
      image_fetcher_(std::move(image_fetcher)) {
  large_icon_types_.push_back(favicon_base::IconType::FAVICON);
  large_icon_types_.push_back(favicon_base::IconType::TOUCH_ICON);
  large_icon_types_.push_back(favicon_base::IconType::TOUCH_PRECOMPOSED_ICON);
}

LargeIconService::~LargeIconService() {
}

base::CancelableTaskTracker::TaskId
    LargeIconService::GetLargeIconOrFallbackStyle(
        const GURL& page_url,
        int min_source_size_in_pixel,
        int desired_size_in_pixel,
        const favicon_base::LargeIconCallback& callback,
        base::CancelableTaskTracker* tracker) {
  DCHECK_LE(1, min_source_size_in_pixel);
  DCHECK_LE(0, desired_size_in_pixel);

  scoped_refptr<LargeIconWorker> worker =
      new LargeIconWorker(min_source_size_in_pixel, desired_size_in_pixel,
                          callback, background_task_runner_, tracker);

  // TODO(beaudoin): For now this is just a wrapper around
  //   GetLargestRawFaviconForPageURL. Add the logic required to select the best
  //   possible large icon. Also add logic to fetch-on-demand when the URL of
  //   a large icon is known but its bitmap is not available.
  return favicon_service_->GetLargestRawFaviconForPageURL(
      page_url, large_icon_types_, min_source_size_in_pixel,
      base::Bind(&LargeIconWorker::OnIconLookupComplete, worker),
      tracker);
}

void LargeIconService::
    GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
        const GURL& page_url,
        int min_source_size_in_pixel,
        const base::Callback<void(bool success)>& callback) {
  DCHECK_LE(0, min_source_size_in_pixel);

  const GURL trimmed_page_url = TrimPageUrlForGoogleServer(page_url);
  const GURL icon_url =
      GetIconUrlForGoogleServerV2(trimmed_page_url, min_source_size_in_pixel);

  // Do not download if the URL is invalid after trimming, or there is a
  // previous cache miss recorded for |icon_url|.
  if (!trimmed_page_url.is_valid() || !image_fetcher_ ||
      favicon_service_->WasUnableToDownloadFavicon(icon_url)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }

  image_fetcher_->SetDataUseServiceName(
      data_use_measurement::DataUseUserData::LARGE_ICON_SERVICE);
  image_fetcher_->StartOrQueueNetworkRequest(
      icon_url.spec(), icon_url,
      base::Bind(&OnFetchIconFromGoogleServerComplete, favicon_service_,
                 page_url, callback));
}

}  // namespace favicon
