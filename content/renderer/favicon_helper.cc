// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/favicon_helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "content/common/icon_messages.h"
#include "content/public/renderer/render_view.h"
#include "net/base/data_url.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "webkit/glue/image_decoder.h"
#include "webkit/glue/multi_resolution_image_resource_fetcher.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFrame;
using WebKit::WebIconURL;
using WebKit::WebVector;
using WebKit::WebURL;
using WebKit::WebURLRequest;
using webkit_glue::MultiResolutionImageResourceFetcher;

namespace content {

namespace {

bool TouchEnabled() {
// Based on the definition of chrome::kEnableTouchIcon.
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

}  // namespace


static FaviconURL::IconType ToFaviconType(WebIconURL::Type type) {
  switch (type) {
    case WebIconURL::TypeFavicon:
      return FaviconURL::FAVICON;
    case WebIconURL::TypeTouch:
      return FaviconURL::TOUCH_ICON;
    case WebIconURL::TypeTouchPrecomposed:
      return FaviconURL::TOUCH_PRECOMPOSED_ICON;
    case WebIconURL::TypeInvalid:
      return FaviconURL::INVALID_ICON;
  }
  return FaviconURL::INVALID_ICON;
}

FaviconHelper::FaviconHelper(RenderView* render_view)
    : RenderViewObserver(render_view) {
}

void FaviconHelper::DidChangeIcon(WebKit::WebFrame* frame,
                                  WebKit::WebIconURL::Type icon_type) {
  if (frame->parent())
    return;

  if (!TouchEnabled() && icon_type != WebIconURL::TypeFavicon)
    return;

  WebVector<WebIconURL> icon_urls = frame->iconURLs(icon_type);
  std::vector<FaviconURL> urls;
  for (size_t i = 0; i < icon_urls.size(); i++) {
    urls.push_back(FaviconURL(icon_urls[i].iconURL(),
                              ToFaviconType(icon_urls[i].iconType())));
  }
  SendUpdateFaviconURL(routing_id(), render_view()->GetPageId(), urls);
}

FaviconHelper::~FaviconHelper() {
}

void FaviconHelper::OnDownloadFavicon(int id,
                                      const GURL& image_url,
                                      bool is_favicon,
                                      int image_size) {
  std::vector<SkBitmap> result_images;
  if (image_url.SchemeIs("data")) {
    SkBitmap data_image = ImageFromDataUrl(image_url);
    if (!data_image.empty())
      result_images.push_back(data_image);
  } else {
    if (DownloadFavicon(id, image_url, is_favicon, image_size)) {
      // Will complete asynchronously via FaviconHelper::DidDownloadFavicon
      return;
    }
  }

  Send(new IconHostMsg_DidDownloadFavicon(routing_id(),
                                          id,
                                          image_url,
                                          image_size,
                                          result_images));
}

bool FaviconHelper::DownloadFavicon(int id,
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
      base::Bind(&FaviconHelper::DidDownloadFavicon,
                 base::Unretained(this), image_size)));
  return true;
}

void FaviconHelper::DidDownloadFavicon(
    int requested_size,
    MultiResolutionImageResourceFetcher* fetcher,
    const std::vector<SkBitmap>& images) {
  // Notify requester of image download status.
  Send(new IconHostMsg_DidDownloadFavicon(routing_id(),
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

SkBitmap FaviconHelper::ImageFromDataUrl(const GURL& url) const {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data) && !data.empty()) {
    // Decode the favicon using WebKit's image decoder.
    webkit_glue::ImageDecoder decoder(
        gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize));
    const unsigned char* src_data =
        reinterpret_cast<const unsigned char*>(&data[0]);

    return decoder.Decode(src_data, data.size());
  }
  return SkBitmap();
}

void FaviconHelper::SendUpdateFaviconURL(int32 routing_id,
                                         int32 page_id,
                                         const std::vector<FaviconURL>& urls) {
  if (!urls.empty())
    Send(new IconHostMsg_UpdateFaviconURL(routing_id, page_id, urls));
}

bool FaviconHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FaviconHelper, message)
    IPC_MESSAGE_HANDLER(IconMsg_DownloadFavicon, OnDownloadFavicon)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void FaviconHelper::DidStopLoading() {
  int icon_types = WebIconURL::TypeFavicon;
  if (TouchEnabled())
    icon_types |= WebIconURL::TypeTouchPrecomposed | WebIconURL::TypeTouch;

  WebVector<WebIconURL> icon_urls =
      render_view()->GetWebView()->mainFrame()->iconURLs(icon_types);
  std::vector<FaviconURL> urls;
  for (size_t i = 0; i < icon_urls.size(); i++) {
    WebURL url = icon_urls[i].iconURL();
    if (!url.isEmpty())
      urls.push_back(FaviconURL(url, ToFaviconType(icon_urls[i].iconType())));
  }
  SendUpdateFaviconURL(routing_id(), render_view()->GetPageId(), urls);
}

}  // namespace content
