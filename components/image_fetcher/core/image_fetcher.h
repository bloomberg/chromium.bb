// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace image_fetcher {

class ImageDecoder;

// Encapsulates image fetching customization options.
// (required)
// traffic_annotation
//   Documents what the network traffic is for, gives you free metrics.
// max_download_size
//   Limits the size of the downloaded image.
// frame_size
//   If multiple sizes of the image are available on the server, choose the one
//   that's closest to the given size (only useful for .icos). Does NOT resize
//   the downloaded image to the given dimensions.
class ImageFetcherParams {
 public:
  // Sets the UMA client name to report feature-specific metrics. Make sure
  // |uma_client_name| is also present in histograms.xml.
  ImageFetcherParams(
      net::NetworkTrafficAnnotationTag network_traffic_annotation_tag,
      std::string uma_client_name);
  ImageFetcherParams(const ImageFetcherParams& params);
  ImageFetcherParams(ImageFetcherParams&& params);

  ~ImageFetcherParams();

  const net::NetworkTrafficAnnotationTag traffic_annotation() const {
    return network_traffic_annotation_tag_;
  }

  void set_max_download_size(base::Optional<int64_t> max_download_bytes) {
    max_download_bytes_ = max_download_bytes;
  }

  base::Optional<int64_t> max_download_size() const {
    return max_download_bytes_;
  }

  void set_frame_size(gfx::Size desired_frame_size) {
    desired_frame_size_ = desired_frame_size;
  }

  gfx::Size frame_size() const { return desired_frame_size_; }

  const std::string& uma_client_name() const { return uma_client_name_; }

 private:
  const net::NetworkTrafficAnnotationTag network_traffic_annotation_tag_;

  base::Optional<int64_t> max_download_bytes_;
  gfx::Size desired_frame_size_;
  std::string uma_client_name_;
};

// A class used to fetch server images. It can be called from any thread and the
// callback will be called on the thread which initiated the fetch.
class ImageFetcher {
 public:
  ImageFetcher() {}
  virtual ~ImageFetcher() {}

  // Fetch an image and optionally decode it. |image_data_callback| is called
  // when the image fetch completes, but |image_data_callback| may be empty.
  // |image_callback| is called when the image is finished decoding.
  // |image_callback| may be empty if image decoding is not required. If a
  // callback is provided, it will be called exactly once. On failure, an empty
  // string/gfx::Image is returned.
  virtual void FetchImageAndData(const GURL& image_url,
                                 ImageDataFetcherCallback image_data_callback,
                                 ImageFetcherCallback image_callback,
                                 ImageFetcherParams params) = 0;

  // Fetch an image and decode it. An empty gfx::Image will be returned to the
  // callback in case the image could not be fetched. This is the same as
  // calling FetchImageAndData without an |image_data_callback|.
  void FetchImage(const GURL& image_url,
                  ImageFetcherCallback callback,
                  ImageFetcherParams params) {
    FetchImageAndData(image_url, ImageDataFetcherCallback(),
                      std::move(callback), params);
  }

  // Just fetch the image data, do not decode. This is the same as
  // calling FetchImageAndData without an |image_callback|.
  void FetchImageData(const GURL& image_url,
                      ImageDataFetcherCallback callback,
                      ImageFetcherParams params) {
    FetchImageAndData(image_url, std::move(callback), ImageFetcherCallback(),
                      params);
  }

  virtual ImageDecoder* GetImageDecoder() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageFetcher);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_H_
