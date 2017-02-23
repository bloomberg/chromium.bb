// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/image_downloader/image_downloader_base.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/child/image_decoder.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/fetchers/multi_resolution_image_resource_fetcher.h"
#include "net/base/data_url.h"
#include "third_party/WebKit/public/platform/WebCachePolicy.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_constants.h"

using blink::WebCachePolicy;
using blink::WebFrame;
using blink::WebURLRequest;

namespace {

// Decodes a data: URL image or returns an empty image in case of failure.
SkBitmap ImageFromDataUrl(const GURL& url) {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data) && !data.empty()) {
    // Decode the image using Blink's image decoder.
    content::ImageDecoder decoder(
        gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));
    const unsigned char* src_data =
        reinterpret_cast<const unsigned char*>(data.data());

    return decoder.Decode(src_data, data.size());
  }
  return SkBitmap();
}

}  // namespace

namespace content {

ImageDownloaderBase::ImageDownloaderBase(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
  RenderThread::Get()->AddObserver(this);
}

ImageDownloaderBase::~ImageDownloaderBase() {
  RenderThread* thread = RenderThread::Get();
  // The destructor may run after message loop shutdown, so we need to check
  // whether RenderThread is null.
  if (thread)
    thread->RemoveObserver(this);
}

// Ensure all loaders cleared before calling blink::shutdown.
void ImageDownloaderBase::OnRenderProcessShutdown() {
  image_fetchers_.clear();
}

void ImageDownloaderBase::DownloadImage(const GURL& image_url,
                                        bool is_favicon,
                                        bool bypass_cache,
                                        const DownloadCallback& callback) {
  std::vector<SkBitmap> result_images;

  if (image_url.SchemeIs(url::kDataScheme)) {
    SkBitmap data_image = ImageFromDataUrl(image_url);
    // Drop null or empty SkBitmap.
    if (!data_image.drawsNothing())
      result_images.push_back(data_image);
  } else {
    if (FetchImage(image_url, is_favicon, bypass_cache, callback)) {
      // Will complete asynchronously via ImageDownloaderBase::DidFetchImage
      return;
    }
  }

  callback.Run(0, result_images);
}

bool ImageDownloaderBase::FetchImage(const GURL& image_url,
                                     bool is_favicon,
                                     bool bypass_cache,
                                     const DownloadCallback& callback) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  DCHECK(frame);

  // Create an image resource fetcher and assign it with a call back object.
  image_fetchers_.push_back(
      base::MakeUnique<MultiResolutionImageResourceFetcher>(
          image_url, frame, 0,
          is_favicon ? WebURLRequest::RequestContextFavicon
                     : WebURLRequest::RequestContextImage,
          bypass_cache ? WebCachePolicy::BypassingCache
                       : WebCachePolicy::UseProtocolCachePolicy,
          base::Bind(&ImageDownloaderBase::DidFetchImage,
                     base::Unretained(this), callback)));
  return true;
}

void ImageDownloaderBase::DidFetchImage(
    const DownloadCallback& callback,
    MultiResolutionImageResourceFetcher* fetcher,
    const std::vector<SkBitmap>& images) {
  int32_t http_status_code = fetcher->http_status_code();

  // Remove the image fetcher from our pending list. We're in the callback from
  // MultiResolutionImageResourceFetcher, best to delay deletion.
  for (auto iter = image_fetchers_.begin(); iter != image_fetchers_.end();
       ++iter) {
    if (iter->get() == fetcher) {
      iter->release();
      image_fetchers_.erase(iter);
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, fetcher);
      break;
    }
  }

  // |this| may be destructed after callback is run.
  callback.Run(http_status_code, images);
}

void ImageDownloaderBase::OnDestruct() {
  for (const auto& fetchers : image_fetchers_) {
    // Will run callbacks with an empty image vector.
    fetchers->OnRenderFrameDestruct();
  }
}

}  // namespace content
