// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cached_image_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "components/image_fetcher/core/cache/cached_image_fetcher_metrics_reporter.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace image_fetcher {

struct CachedImageFetcherRequest {
  // The url to be fetched.
  const GURL url;

  // An identifier passed back to the caller.
  const std::string id;

  // The desired frame size if there are multiple frames to choose from.
  const gfx::Size desired_frame_size;

  // The service to track data usage for.
  const DataUseServiceName data_use_service_name;

  // Limit the number of bytes to download for the image.
  const base::Optional<int64_t> image_download_limit_bytes;

  // Analytic events below.

  // True if there was a cache hit during the fetch sequence.
  bool cache_hit_before_network_request;

  // The start time of the fetch sequence.
  const base::Time start_time;
};

namespace {

void DataCallbackIfPresent(ImageDataFetcherCallback data_callback,
                           const std::string& image_data,
                           const image_fetcher::RequestMetadata& metadata) {
  if (data_callback.is_null()) {
    return;
  }
  std::move(data_callback).Run(image_data, metadata);
}

void ImageCallbackIfPresent(ImageFetcherCallback image_callback,
                            const std::string& id,
                            const gfx::Image& image,
                            const image_fetcher::RequestMetadata& metadata) {
  if (image_callback.is_null()) {
    return;
  }
  std::move(image_callback).Run(id, image, metadata);
}

std::string EncodeSkBitmapToPNG(const SkBitmap& bitmap) {
  std::vector<unsigned char> encoded_data;
  bool result = gfx::PNGCodec::Encode(
      static_cast<const unsigned char*>(bitmap.getPixels()),
      gfx::PNGCodec::FORMAT_RGBA, gfx::Size(bitmap.width(), bitmap.height()),
      static_cast<int>(bitmap.rowBytes()), /* discard_transparency */ false,
      std::vector<gfx::PNGCodec::Comment>(), &encoded_data);
  return result ? std::string(encoded_data.begin(), encoded_data.end()) : "";
}

}  // namespace

CachedImageFetcher::CachedImageFetcher(
    std::unique_ptr<ImageFetcher> image_fetcher,
    scoped_refptr<ImageCache> image_cache,
    bool read_only)
    : image_fetcher_(std::move(image_fetcher)),
      image_cache_(image_cache),
      read_only_(read_only),
      weak_ptr_factory_(this) {
  DCHECK(image_fetcher_);
  DCHECK(image_cache_);
}

CachedImageFetcher::~CachedImageFetcher() = default;

void CachedImageFetcher::SetDataUseServiceName(
    DataUseServiceName data_use_service_name) {
  data_use_service_name_ = data_use_service_name;
}

void CachedImageFetcher::SetDesiredImageFrameSize(const gfx::Size& size) {
  desired_frame_size_ = size;
}

void CachedImageFetcher::SetImageDownloadLimit(
    base::Optional<int64_t> max_download_bytes) {
  image_download_limit_bytes_ = max_download_bytes;
}

ImageDecoder* CachedImageFetcher::GetImageDecoder() {
  return image_fetcher_->GetImageDecoder();
}

void CachedImageFetcher::FetchImageAndData(
    const std::string& id,
    const GURL& image_url,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // TODO(wylieb): Inject a clock for better testability.
  CachedImageFetcherRequest request = {
      image_url,
      id,
      desired_frame_size_,
      data_use_service_name_,
      image_download_limit_bytes_,
      /* cache_hit_before_network_request */ false,
      /* start_time */ base::Time::Now()};

  // First, try to load the image from the cache, then try the network.
  image_cache_->LoadImage(
      read_only_, image_url.spec(),
      base::BindOnce(&CachedImageFetcher::OnImageFetchedFromCache,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     traffic_annotation, std::move(image_data_callback),
                     std::move(image_callback)));

  CachedImageFetcherMetricsReporter::ReportEvent(
      CachedImageFetcherEvent::kImageRequest);
}

void CachedImageFetcher::OnImageFetchedFromCache(
    CachedImageFetcherRequest request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback,
    std::string image_data) {
  if (image_data.empty()) {
    // Fetching from the DB failed, start a network fetch.
    EnqueueFetchImageFromNetwork(std::move(request), traffic_annotation,
                                 std::move(image_data_callback),
                                 std::move(image_callback));

    CachedImageFetcherMetricsReporter::ReportEvent(
        CachedImageFetcherEvent::kCacheMiss);
  } else {
    DataCallbackIfPresent(std::move(image_data_callback), image_data,
                          RequestMetadata());
    GetImageDecoder()->DecodeImage(
        image_data,
        /* The frame size had already been chosen during fetch. */ gfx::Size(),
        base::BindRepeating(
            &CachedImageFetcher::OnImageDecodedFromCache,
            weak_ptr_factory_.GetWeakPtr(), std::move(request),
            traffic_annotation, base::Passed(std::move(image_data_callback)),
            base::Passed(std::move(image_callback)), image_data));
    CachedImageFetcherMetricsReporter::ReportEvent(
        CachedImageFetcherEvent::kCacheHit);
  }
}

void CachedImageFetcher::OnImageDecodedFromCache(
    CachedImageFetcherRequest request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback,
    const std::string& image_data,
    const gfx::Image& image) {
  if (image.IsEmpty()) {
    // Upon failure, fetch from the network.
    request.cache_hit_before_network_request = true;
    EnqueueFetchImageFromNetwork(std::move(request), traffic_annotation,
                                 std::move(image_data_callback),
                                 std::move(image_callback));

    CachedImageFetcherMetricsReporter::ReportEvent(
        CachedImageFetcherEvent::kCacheDecodingError);
  } else {
    ImageCallbackIfPresent(std::move(image_callback), request.id, image,
                           RequestMetadata());
    CachedImageFetcherMetricsReporter::ReportImageLoadFromCacheTime(
        request.start_time);
  }
}

void CachedImageFetcher::EnqueueFetchImageFromNetwork(
    CachedImageFetcherRequest request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CachedImageFetcher::FetchImageFromNetwork,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     traffic_annotation, std::move(image_data_callback),
                     std::move(image_callback)));
}

void CachedImageFetcher::FetchImageFromNetwork(
    CachedImageFetcherRequest request,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ImageDataFetcherCallback image_data_callback,
    ImageFetcherCallback image_callback) {
  std::string id = request.id;
  const GURL& url = request.url;
  // Fetch image data and the image itself. The image data will be stored in
  // the image cache, and the image will be returned to the caller.
  image_fetcher_->SetDesiredImageFrameSize(request.desired_frame_size);
  image_fetcher_->SetDataUseServiceName(request.data_use_service_name);
  image_fetcher_->SetImageDownloadLimit(request.image_download_limit_bytes);
  image_fetcher_->FetchImageAndData(
      id, url, std::move(image_data_callback),
      base::BindOnce(&CachedImageFetcher::OnImageFetchedFromNetwork,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     std::move(image_callback)),
      traffic_annotation);
}

void CachedImageFetcher::OnImageFetchedFromNetwork(
    CachedImageFetcherRequest request,
    ImageFetcherCallback image_callback,
    const std::string& id,
    const gfx::Image& image,
    const RequestMetadata& request_metadata) {
  // The image has been deocded by the fetcher already, return straight to the
  // caller.
  ImageCallbackIfPresent(std::move(image_callback), request.id, image,
                         request_metadata);

  // Copy the image data out and store it on disk.
  const SkBitmap* bitmap = image.IsEmpty() ? nullptr : image.ToSkBitmap();
  // If the bitmap is null or otherwise not ready, skip encoding.
  if (bitmap == nullptr || bitmap->isNull() || !bitmap->readyToDraw()) {
    StoreEncodedData(request.url, "");
    CachedImageFetcherMetricsReporter::ReportEvent(
        CachedImageFetcherEvent::kTotalFailure);
  } else {
    // Post a task to another thread to encode the image data downloaded.
    base::PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&EncodeSkBitmapToPNG, *bitmap),
        base::BindOnce(&CachedImageFetcher::StoreEncodedData,
                       weak_ptr_factory_.GetWeakPtr(), request.url));
  }

  // Report to different histograms depending upon if there was a cache hit.
  if (request.cache_hit_before_network_request) {
    CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkAfterCacheHit(
        request.start_time);
  } else {
    CachedImageFetcherMetricsReporter::ReportImageLoadFromNetworkTime(
        request.start_time);
  }
}

void CachedImageFetcher::StoreEncodedData(const GURL& url,
                                          std::string image_data) {
  // If the image is empty, delete the image.
  if (image_data.empty()) {
    CachedImageFetcherMetricsReporter::ReportEvent(
        CachedImageFetcherEvent::kTranscodingError);
    image_cache_->DeleteImage(url.spec());
    return;
  }

  if (!read_only_) {
    image_cache_->SaveImage(url.spec(), std::move(image_data));
  }
}

}  // namespace image_fetcher
