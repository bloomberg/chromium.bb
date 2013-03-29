// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/image_loading_helper.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/common/image_messages.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_view.h"
#include "net/base/data_url.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "webkit/glue/image_decoder.h"
#include "webkit/glue/multi_resolution_image_resource_fetcher.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFrame;
using WebKit::WebVector;
using WebKit::WebURL;
using WebKit::WebURLRequest;
using webkit_glue::MultiResolutionImageResourceFetcher;

namespace content {

ImageLoadingHelper::ImageLoadingHelper(RenderView* render_view)
    : RenderViewObserver(render_view) {
}

ImageLoadingHelper::~ImageLoadingHelper() {
}

void ImageLoadingHelper::OnDownloadImage(int id,
                                         const GURL& image_url,
                                         bool is_favicon,
                                         int image_size) {
  std::vector<SkBitmap> result_images;
  if (image_url.SchemeIs(chrome::kDataScheme)) {
    SkBitmap data_image = ImageFromDataUrl(image_url);
    if (!data_image.empty())
      result_images.push_back(data_image);
  } else {
    if (DownloadImage(id, image_url, is_favicon, image_size)) {
      // Will complete asynchronously via ImageLoadingHelper::DidDownloadImage
      return;
    }
  }

  Send(new ImageHostMsg_DidDownloadImage(routing_id(),
                                         id,
                                         image_url,
                                         image_size,
                                         result_images));
}

bool ImageLoadingHelper::DownloadImage(int id,
                                       const GURL& image_url,
                                       bool is_favicon,
                                       int image_size) {
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
                 base::Unretained(this), image_size)));
  return true;
}

void ImageLoadingHelper::DidDownloadImage(
    int requested_size,
    MultiResolutionImageResourceFetcher* fetcher,
    const std::vector<SkBitmap>& images) {
  // Notify requester of image download status.
  Send(new ImageHostMsg_DidDownloadImage(routing_id(),
                                         fetcher->id(),
                                         fetcher->image_url(),
                                         requested_size,
                                         images));

  // Remove the image fetcher from our pending list. We're in the callback from
  // MultiResolutionImageResourceFetcher, best to delay deletion.
  ImageResourceFetcherList::iterator iter =
      std::find(image_fetchers_.begin(), image_fetchers_.end(), fetcher);
  if (iter != image_fetchers_.end()) {
    image_fetchers_.weak_erase(iter);
    MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);
  }
}

SkBitmap ImageLoadingHelper::ImageFromDataUrl(const GURL& url) const {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data) && !data.empty()) {
    // Decode the image using WebKit's image decoder.
    webkit_glue::ImageDecoder decoder(
        gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));
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

