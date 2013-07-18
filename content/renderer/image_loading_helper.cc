// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/image_loading_helper.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/child/image_decoder.h"
#include "content/common/image_messages.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/fetchers/multi_resolution_image_resource_fetcher.h"
#include "net/base/data_url.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFrame;
using WebKit::WebVector;
using WebKit::WebURL;
using WebKit::WebURLRequest;

namespace {

//  Proportionally resizes the |image| to fit in a box of size
// |max_image_size|.
SkBitmap ResizeImage(const SkBitmap& image, uint32_t max_image_size) {
  if (max_image_size == 0)
    return image;
  uint32_t max_dimension = std::max(image.width(), image.height());
  if (max_dimension <= max_image_size)
    return image;
  // Proportionally resize the minimal image to fit in a box of size
  // max_image_size.
  return skia::ImageOperations::Resize(
      image,
      skia::ImageOperations::RESIZE_BEST,
      static_cast<uint64_t>(image.width()) * max_image_size / max_dimension,
      static_cast<uint64_t>(image.height()) * max_image_size / max_dimension);
}

// Filters the array of bitmaps, removing all images that do not fit in a box of
// size |max_image_size|. Returns the result if it is not empty. Otherwise,
// find the smallest image in the array and resize it proportionally to fit
// in a box of size |max_image_size|.
std::vector<SkBitmap> FilterAndResizeImageForMaximalSize(
    const std::vector<SkBitmap>& images,
    uint32_t max_image_size) {
  if (!images.size() || max_image_size == 0)
    return images;
  std::vector<SkBitmap> result;
  const SkBitmap* min_image = NULL;
  uint32_t min_image_size = std::numeric_limits<uint32_t>::max();
  // Filter the images by |max_image_size|, and also identify the smallest image
  // in case all the images are bigger than |max_image_size|.
  for (std::vector<SkBitmap>::const_iterator it = images.begin();
       it != images.end();
       ++it) {
    const SkBitmap& image = *it;
    uint32_t current_size = std::max(it->width(), it->height());
    if (current_size < min_image_size) {
      min_image = &image;
      min_image_size = current_size;
    }
    if (static_cast<uint32_t>(image.width()) <= max_image_size &&
        static_cast<uint32_t>(image.height()) <= max_image_size) {
      result.push_back(image);
    }
  }
  DCHECK(min_image);
  if (result.size())
    return result;
  // Proportionally resize the minimal image to fit in a box of size
  // max_image_size.
  result.push_back(ResizeImage(*min_image, max_image_size));
  return result;
}

}  // namespace

namespace content {

ImageLoadingHelper::ImageLoadingHelper(RenderView* render_view)
    : RenderViewObserver(render_view) {
}

ImageLoadingHelper::~ImageLoadingHelper() {
}

void ImageLoadingHelper::OnDownloadImage(int id,
                                         const GURL& image_url,
                                         bool is_favicon,
                                         uint32_t preferred_image_size,
                                         uint32_t max_image_size) {
  std::vector<SkBitmap> result_images;
  if (image_url.SchemeIs(chrome::kDataScheme)) {
    SkBitmap data_image = ImageFromDataUrl(image_url);
    if (!data_image.empty())
      result_images.push_back(ResizeImage(data_image, max_image_size));
  } else {
    if (DownloadImage(
            id, image_url, is_favicon, preferred_image_size, max_image_size)) {
      // Will complete asynchronously via ImageLoadingHelper::DidDownloadImage
      return;
    }
  }

  Send(new ImageHostMsg_DidDownloadImage(routing_id(),
                                         id,
                                         0,
                                         image_url,
                                         preferred_image_size,
                                         result_images));
}

bool ImageLoadingHelper::DownloadImage(int id,
                                       const GURL& image_url,
                                       bool is_favicon,
                                       uint32_t preferred_image_size,
                                       uint32_t max_image_size) {
  // Make sure webview was not shut down.
  if (!render_view()->GetWebView())
    return false;
  // Create an image resource fetcher and assign it with a call back object.
  image_fetchers_.push_back(new MultiResolutionImageResourceFetcher(
      image_url,
      render_view()->GetWebView()->mainFrame(),
      id,
      is_favicon ? WebURLRequest::TargetIsFavicon :
                   WebURLRequest::TargetIsImage,
      base::Bind(&ImageLoadingHelper::DidDownloadImage,
                 base::Unretained(this),
                 preferred_image_size,
                 max_image_size)));
  return true;
}

void ImageLoadingHelper::DidDownloadImage(
    uint32_t preferred_image_size,
    uint32_t max_image_size,
    MultiResolutionImageResourceFetcher* fetcher,
    const std::vector<SkBitmap>& images) {
  // Notify requester of image download status.
  Send(new ImageHostMsg_DidDownloadImage(
      routing_id(),
      fetcher->id(),
      fetcher->http_status_code(),
      fetcher->image_url(),
      preferred_image_size,
      FilterAndResizeImageForMaximalSize(images, max_image_size)));

  // Remove the image fetcher from our pending list. We're in the callback from
  // MultiResolutionImageResourceFetcher, best to delay deletion.
  ImageResourceFetcherList::iterator iter =
      std::find(image_fetchers_.begin(), image_fetchers_.end(), fetcher);
  if (iter != image_fetchers_.end()) {
    image_fetchers_.weak_erase(iter);
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);
  }
}

SkBitmap ImageLoadingHelper::ImageFromDataUrl(const GURL& url) const {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data) && !data.empty()) {
    // Decode the image using WebKit's image decoder.
    ImageDecoder decoder(gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));
    const unsigned char* src_data =
        reinterpret_cast<const unsigned char*>(&data[0]);

    return decoder.Decode(src_data, data.size());
  }
  return SkBitmap();
}

bool ImageLoadingHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ImageLoadingHelper, message)
    IPC_MESSAGE_HANDLER(ImageMsg_DownloadImage, OnDownloadImage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

}  // namespace content
