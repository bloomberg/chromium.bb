// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_util.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/base/network_change_notifier.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_canon.h"

namespace favicon {
namespace {

// This feature is only used for accessing field trial parameters, not for
// switching on/off the code.
const base::Feature kLargeIconServiceFetchingFeature{
    "LargeIconServiceFetching", base::FEATURE_ENABLED_BY_DEFAULT};

const char kGoogleServerV2RequestFormat[] =
    "https://t0.gstatic.com/faviconV2?"
    "client=chrome&drop_404_icon=true&%s"
    "size=%d&min_size=%d&max_size=%d&fallback_opts=TYPE,SIZE,URL&url=%s";
const char kGoogleServerV2RequestFormatParam[] = "request_format";

const char kCheckSeenParam[] = "check_seen=true&";

const int kGoogleServerV2EnforcedMinSizeInPixel = 16;
const char kGoogleServerV2EnforcedMinSizeInPixelParam[] =
    "enforced_min_size_in_pixel";

const double kGoogleServerV2DesiredToMaxSizeFactor = 2.0;
const char kGoogleServerV2DesiredToMaxSizeFactorParam[] =
    "desired_to_max_size_factor";

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

GURL GetRequestUrlForGoogleServerV2(const GURL& page_url,
                                    int min_source_size_in_pixel,
                                    int desired_size_in_pixel,
                                    bool may_page_url_be_private) {
  std::string url_format = base::GetFieldTrialParamValueByFeature(
      kLargeIconServiceFetchingFeature, kGoogleServerV2RequestFormatParam);
  double desired_to_max_size_factor = base::GetFieldTrialParamByFeatureAsDouble(
      kLargeIconServiceFetchingFeature,
      kGoogleServerV2DesiredToMaxSizeFactorParam,
      kGoogleServerV2DesiredToMaxSizeFactor);

  min_source_size_in_pixel = std::max(
      min_source_size_in_pixel, base::GetFieldTrialParamByFeatureAsInt(
                                    kLargeIconServiceFetchingFeature,
                                    kGoogleServerV2EnforcedMinSizeInPixelParam,
                                    kGoogleServerV2EnforcedMinSizeInPixel));
  desired_size_in_pixel =
      std::max(desired_size_in_pixel, min_source_size_in_pixel);
  int max_size_in_pixel =
      static_cast<int>(desired_size_in_pixel * desired_to_max_size_factor);

  return GURL(base::StringPrintf(
      url_format.empty() ? kGoogleServerV2RequestFormat : url_format.c_str(),
      may_page_url_be_private ? kCheckSeenParam : "", desired_size_in_pixel,
      min_source_size_in_pixel, max_size_in_pixel, page_url.spec().c_str()));
}

bool IsDbResultAdequate(const favicon_base::FaviconRawBitmapResult& db_result,
                        int min_source_size) {
  return db_result.is_valid() &&
         db_result.pixel_size.width() == db_result.pixel_size.height() &&
         db_result.pixel_size.width() >= min_source_size;
}

// Wraps the PNG data in |db_result| in a gfx::Image. If |desired_size| is not
// 0, the image gets decoded and resized to |desired_size| (in px). Must run on
// a background thread in production.
gfx::Image ResizeLargeIconOnBackgroundThread(
    const favicon_base::FaviconRawBitmapResult& db_result,
    int desired_size) {
  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
      db_result.bitmap_data->front(), db_result.bitmap_data->size());

  if (desired_size == 0 || db_result.pixel_size.width() == desired_size) {
    return image;
  }

  SkBitmap resized = skia::ImageOperations::Resize(
      image.AsBitmap(), skia::ImageOperations::RESIZE_LANCZOS3, desired_size,
      desired_size);
  return gfx::Image::CreateFrom1xBitmap(resized);
}

// Processes the |db_result| and writes the result into |raw_result| if
// |raw_result| is not nullptr or to |bitmap|, otherwise. If |db_result| is not
// valid or is smaller than |min_source_size|, the resulting fallback style is
// written into |fallback_icon_style|.
void ProcessIconOnBackgroundThread(
    const favicon_base::FaviconRawBitmapResult& db_result,
    int min_source_size,
    int desired_size,
    favicon_base::FaviconRawBitmapResult* raw_result,
    SkBitmap* bitmap,
    GURL* icon_url,
    favicon_base::FallbackIconStyle* fallback_icon_style) {
  if (IsDbResultAdequate(db_result, min_source_size)) {
    gfx::Image image;
    image = ResizeLargeIconOnBackgroundThread(db_result, desired_size);

    if (!image.IsEmpty()) {
      if (raw_result) {
        *raw_result = db_result;
        if (desired_size != 0)
          raw_result->pixel_size = gfx::Size(desired_size, desired_size);
        raw_result->bitmap_data = image.As1xPNGBytes();
      }
      if (bitmap) {
        *bitmap = image.AsBitmap();
      }
      if (icon_url) {
        *icon_url = db_result.icon_url;
      }
      return;
    }
  }

  if (!fallback_icon_style)
    return;

  *fallback_icon_style = favicon_base::FallbackIconStyle();
  int fallback_icon_size = 0;
  if (db_result.is_valid()) {
    favicon_base::SetDominantColorAsBackground(db_result.bitmap_data,
                                               fallback_icon_style);
    // The size must be positive, we cap to 128 to avoid the sparse histogram
    // to explode (having too many different values, server-side). Size 128
    // already indicates that there is a problem in the code, 128 px _should_ be
    // enough in all current UI surfaces.
    fallback_icon_size = db_result.pixel_size.width();
    DCHECK_GT(fallback_icon_size, 0);
    fallback_icon_size = std::min(fallback_icon_size, 128);
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY("Favicons.LargeIconService.FallbackSize",
                              fallback_icon_size);
}

// Processes the bitmap data returned from the FaviconService as part of a
// LargeIconService request.
class LargeIconWorker : public base::RefCountedThreadSafe<LargeIconWorker> {
 public:
  // Exactly one of the callbacks is expected to be non-null.
  LargeIconWorker(int min_source_size_in_pixel,
                  int desired_size_in_pixel,
                  favicon_base::LargeIconCallback raw_bitmap_callback,
                  favicon_base::LargeIconImageCallback image_callback,
                  base::CancelableTaskTracker* tracker);

  // Must run on the owner (UI) thread in production.
  // Intermediate callback for GetLargeIconOrFallbackStyle(). Invokes
  // ProcessIconOnBackgroundThread() so we do not perform complex image
  // operations on the UI thread.
  void OnIconLookupComplete(
      const favicon_base::FaviconRawBitmapResult& db_result);

 private:
  friend class base::RefCountedThreadSafe<LargeIconWorker>;

  ~LargeIconWorker();

  // Must run on the owner (UI) thread in production.
  // Invoked when ProcessIconOnBackgroundThread() is done.
  void OnIconProcessingComplete();

  int min_source_size_in_pixel_;
  int desired_size_in_pixel_;
  favicon_base::LargeIconCallback raw_bitmap_callback_;
  favicon_base::LargeIconImageCallback image_callback_;
  scoped_refptr<base::TaskRunner> background_task_runner_;
  base::CancelableTaskTracker* tracker_;

  favicon_base::FaviconRawBitmapResult raw_bitmap_result_;
  SkBitmap bitmap_result_;
  GURL icon_url_;
  std::unique_ptr<favicon_base::FallbackIconStyle> fallback_icon_style_;

  DISALLOW_COPY_AND_ASSIGN(LargeIconWorker);
};

LargeIconWorker::LargeIconWorker(
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    favicon_base::LargeIconImageCallback image_callback,
    base::CancelableTaskTracker* tracker)
    : min_source_size_in_pixel_(min_source_size_in_pixel),
      desired_size_in_pixel_(desired_size_in_pixel),
      raw_bitmap_callback_(raw_bitmap_callback),
      image_callback_(image_callback),
      background_task_runner_(base::CreateTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      tracker_(tracker),
      fallback_icon_style_(
          base::MakeUnique<favicon_base::FallbackIconStyle>()) {}

LargeIconWorker::~LargeIconWorker() {
}

void LargeIconWorker::OnIconLookupComplete(
    const favicon_base::FaviconRawBitmapResult& db_result) {
  tracker_->PostTaskAndReply(
      background_task_runner_.get(), FROM_HERE,
      base::Bind(&ProcessIconOnBackgroundThread, db_result,
                 min_source_size_in_pixel_, desired_size_in_pixel_,
                 raw_bitmap_callback_ ? &raw_bitmap_result_ : nullptr,
                 image_callback_ ? &bitmap_result_ : nullptr,
                 image_callback_ ? &icon_url_ : nullptr,
                 fallback_icon_style_.get()),
      base::Bind(&LargeIconWorker::OnIconProcessingComplete, this));
}

void LargeIconWorker::OnIconProcessingComplete() {
  // If |raw_bitmap_callback_| is provided, return the raw result.
  if (raw_bitmap_callback_) {
    if (raw_bitmap_result_.is_valid()) {
      raw_bitmap_callback_.Run(
          favicon_base::LargeIconResult(raw_bitmap_result_));
      return;
    }
    raw_bitmap_callback_.Run(
        favicon_base::LargeIconResult(fallback_icon_style_.release()));
    return;
  }

  if (!bitmap_result_.isNull()) {
    image_callback_.Run(favicon_base::LargeIconImageResult(
        gfx::Image::CreateFrom1xBitmap(bitmap_result_), icon_url_));
    return;
  }
  image_callback_.Run(
      favicon_base::LargeIconImageResult(fallback_icon_style_.release()));
}

void ReportDownloadedSize(int size) {
  UMA_HISTOGRAM_COUNTS_1000("Favicons.LargeIconService.DownloadedSize", size);
}

void OnSetOnDemandFaviconComplete(
    const favicon_base::GoogleFaviconServerCallback& callback,
    bool success) {
  callback.Run(
      success
          ? favicon_base::GoogleFaviconServerRequestStatus::SUCCESS
          : favicon_base::GoogleFaviconServerRequestStatus::FAILURE_ON_WRITE);
}

void OnFetchIconFromGoogleServerComplete(
    FaviconService* favicon_service,
    const GURL& page_url,
    const favicon_base::GoogleFaviconServerCallback& callback,
    const std::string& server_request_url,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  if (image.IsEmpty()) {
    DLOG(WARNING) << "large icon server fetch empty " << server_request_url;
    favicon_service->UnableToDownloadFavicon(GURL(server_request_url));
    callback.Run(metadata.http_response_code ==
                         net::URLFetcher::RESPONSE_CODE_INVALID
                     ? favicon_base::GoogleFaviconServerRequestStatus::
                           FAILURE_CONNECTION_ERROR
                     : favicon_base::GoogleFaviconServerRequestStatus::
                           FAILURE_HTTP_ERROR);
    ReportDownloadedSize(0);
    return;
  }

  ReportDownloadedSize(image.Width());

  // If given, use the original favicon URL from Content-Location http header.
  // Otherwise, use the request URL as fallback.
  std::string original_icon_url = metadata.content_location_header;
  if (original_icon_url.empty()) {
    original_icon_url = server_request_url;
  }

  // Write fetched icons to FaviconService's cache, but only if no icon was
  // available (clients are encouraged to do this in advance, but meanwhile
  // something else could've been written). By marking the icons initially
  // expired (out-of-date), they will be refetched when we visit the original
  // page any time in the future.
  favicon_service->SetOnDemandFavicons(
      page_url, GURL(original_icon_url), favicon_base::IconType::TOUCH_ICON,
      image, base::Bind(&OnSetOnDemandFaviconComplete, callback));
}

}  // namespace

LargeIconService::LargeIconService(
    FaviconService* favicon_service,
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher)
    : favicon_service_(favicon_service),
      image_fetcher_(std::move(image_fetcher)) {
  large_icon_types_.push_back(favicon_base::IconType::WEB_MANIFEST_ICON);
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
    const favicon_base::LargeIconCallback& raw_bitmap_callback,
    base::CancelableTaskTracker* tracker) {
  return GetLargeIconOrFallbackStyleImpl(
      page_url, min_source_size_in_pixel, desired_size_in_pixel,
      raw_bitmap_callback, favicon_base::LargeIconImageCallback(), tracker);
}

base::CancelableTaskTracker::TaskId
LargeIconService::GetLargeIconImageOrFallbackStyle(
    const GURL& page_url,
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    const favicon_base::LargeIconImageCallback& image_callback,
    base::CancelableTaskTracker* tracker) {
  return GetLargeIconOrFallbackStyleImpl(
      page_url, min_source_size_in_pixel, desired_size_in_pixel,
      favicon_base::LargeIconCallback(), image_callback, tracker);
}

void LargeIconService::
    GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
        const GURL& page_url,
        int min_source_size_in_pixel,
        int desired_size_in_pixel,
        bool may_page_url_be_private,
        const net::NetworkTrafficAnnotationTag& traffic_annotation,
        const favicon_base::GoogleFaviconServerCallback& callback) {
  DCHECK_LE(0, min_source_size_in_pixel);

  if (net::NetworkChangeNotifier::IsOffline()) {
    // By exiting early when offline, we avoid caching the failure and thus
    // allow icon fetches later when coming back online.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, favicon_base::GoogleFaviconServerRequestStatus::
                                 FAILURE_CONNECTION_ERROR));
    return;
  }

  const GURL trimmed_page_url = TrimPageUrlForGoogleServer(page_url);
  const GURL server_request_url = GetRequestUrlForGoogleServerV2(
      trimmed_page_url, min_source_size_in_pixel, desired_size_in_pixel,
      may_page_url_be_private);

  if (!server_request_url.is_valid() || !trimmed_page_url.is_valid() ||
      !image_fetcher_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            callback,
            favicon_base::GoogleFaviconServerRequestStatus::FAILURE_INVALID));
    return;
  }

  // Do not download if there is a previous cache miss recorded for
  // |server_request_url|.
  if (favicon_service_->WasUnableToDownloadFavicon(server_request_url)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, favicon_base::GoogleFaviconServerRequestStatus::
                                 FAILURE_HTTP_ERROR_CACHED));
    return;
  }

  image_fetcher_->SetDataUseServiceName(
      data_use_measurement::DataUseUserData::LARGE_ICON_SERVICE);
  image_fetcher_->StartOrQueueNetworkRequest(
      server_request_url.spec(), server_request_url,
      base::Bind(&OnFetchIconFromGoogleServerComplete, favicon_service_,
                 page_url, callback),
      traffic_annotation);
}

void LargeIconService::TouchIconFromGoogleServer(const GURL& icon_url) {
  favicon_service_->TouchOnDemandFavicon(icon_url);
}

base::CancelableTaskTracker::TaskId
LargeIconService::GetLargeIconOrFallbackStyleImpl(
    const GURL& page_url,
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    const favicon_base::LargeIconCallback& raw_bitmap_callback,
    const favicon_base::LargeIconImageCallback& image_callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_LE(1, min_source_size_in_pixel);
  DCHECK_LE(0, desired_size_in_pixel);

  scoped_refptr<LargeIconWorker> worker =
      new LargeIconWorker(min_source_size_in_pixel, desired_size_in_pixel,
                          raw_bitmap_callback, image_callback, tracker);

  int max_size_in_pixel =
      std::max(desired_size_in_pixel, min_source_size_in_pixel);
  // TODO(beaudoin): For now this is just a wrapper around
  //   GetLargestRawFaviconForPageURL. Add the logic required to select the best
  //   possible large icon. Also add logic to fetch-on-demand when the URL of
  //   a large icon is known but its bitmap is not available.
  return favicon_service_->GetLargestRawFaviconForPageURL(
      page_url, large_icon_types_, max_size_in_pixel,
      base::Bind(&LargeIconWorker::OnIconLookupComplete, worker), tracker);
}

}  // namespace favicon
