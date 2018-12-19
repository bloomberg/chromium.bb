// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace image_fetcher {

class ImageCache;
class ImageFetcher;
struct RequestMetadata;

// Encapsulates a request to simplify argument lists.
struct CachedImageFetcherRequest;

// CachedImageFetcher takes care of fetching images from the network and caching
// them. Has a read-only mode which doesn't perform write operations on the
// cache.
class CachedImageFetcher : public ImageFetcher {
 public:
  CachedImageFetcher(std::unique_ptr<ImageFetcher> image_fetcher,
                     scoped_refptr<ImageCache> image_cache,
                     bool read_only);
  ~CachedImageFetcher() override;

  // ImageFetcher:
  void SetDataUseServiceName(DataUseServiceName data_use_service_name) override;
  void SetDesiredImageFrameSize(const gfx::Size& size) override;
  void SetImageDownloadLimit(
      base::Optional<int64_t> max_download_bytes) override;
  void FetchImageAndData(
      const std::string& id,
      const GURL& image_url,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  ImageDecoder* GetImageDecoder() override;

 private:
  // Cache
  void OnImageFetchedFromCache(
      CachedImageFetcherRequest request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback,
      std::string image_data);
  void OnImageDecodedFromCache(
      CachedImageFetcherRequest request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback,
      const std::string& image_data,
      const gfx::Image& image);

  // Network
  void EnqueueFetchImageFromNetwork(
      CachedImageFetcherRequest request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback);
  void FetchImageFromNetwork(
      CachedImageFetcherRequest request,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ImageDataFetcherCallback image_data_callback,
      ImageFetcherCallback image_callback);
  void OnImageFetchedFromNetwork(CachedImageFetcherRequest request,
                                 ImageFetcherCallback image_callback,
                                 const std::string& id,
                                 const gfx::Image& image,
                                 const RequestMetadata& request_metadata);
  void StoreEncodedData(const GURL& url, std::string image_data);

  // Whether the ImageChache is allowed to be modified in any way from requests
  // made by this CachedImageFetcher. This includes updating last used times,
  // writing new data to the cache, or cleaning up unreadable data. Note that
  // the ImageCache may still decide to perform eviction/reconciliation even
  // when only read only CachedImageFetchers are using it.
  std::unique_ptr<ImageFetcher> image_fetcher_;

  scoped_refptr<ImageCache> image_cache_;

  // When true, operations won't affect the longeivity of valid cache items.
  bool read_only_;

  // Capture parameters when ImageFetcher Set* methods are called.
  gfx::Size desired_frame_size_;
  DataUseServiceName data_use_service_name_;
  base::Optional<int64_t> image_download_limit_bytes_;

  base::WeakPtrFactory<CachedImageFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcher);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHED_IMAGE_FETCHER_H_
