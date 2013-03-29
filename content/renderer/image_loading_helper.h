// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_IMAGE_LOADING_HELPER_H_
#define CONTENT_RENDERER_IMAGE_LOADING_HELPER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "content/public/renderer/render_view_observer.h"
#include "googleurl/src/gurl.h"

class SkBitmap;

namespace webkit_glue {
class MultiResolutionImageResourceFetcher;
}

namespace content {

// This class deals with image downloading.
// One instance of ImageLoadingHelper is owned by RenderView.
class ImageLoadingHelper : public RenderViewObserver {
 public:
  explicit ImageLoadingHelper(RenderView* render_view);

 private:
  virtual ~ImageLoadingHelper();

  // Message handler.
  void OnDownloadImage(int id,
                       const GURL& image_url,
                       bool is_favicon,
                       int image_size);

  // Requests to download an image. When done, the ImageLoadingHelper
  // is notified by way of DidDownloadImage. Returns true if the
  // request was successfully started, false otherwise. id is used to
  // uniquely identify the request and passed back to the
  // DidDownloadImage method. If the image is a favicon, cookies will not be
  // sent nor accepted during download. If the image has multiple frames, the
  // frame whose size is image_size is returned. If the image doesn't
  // have a frame at the specified size, the first is returned.
  bool DownloadImage(int id,
                     const GURL& image_url,
                     bool is_favicon,
                     int image_size);

  // This callback is triggered when DownloadImage completes, either
  // succesfully or with a failure. See DownloadImage for more
  // details.
  void DidDownloadImage(
      int requested_size,
      webkit_glue::MultiResolutionImageResourceFetcher* fetcher,
      const std::vector<SkBitmap>& images);

  // Decodes a data: URL image or returns an empty image in case of failure.
  SkBitmap ImageFromDataUrl(const GURL&) const;

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  typedef ScopedVector<webkit_glue::MultiResolutionImageResourceFetcher>
      ImageResourceFetcherList;

  // ImageResourceFetchers schedule via DownloadImage.
  ImageResourceFetcherList image_fetchers_;

  DISALLOW_COPY_AND_ASSIGN(ImageLoadingHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_IMAGE_LOADING_HELPER_H_

