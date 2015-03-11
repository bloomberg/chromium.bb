// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include <limits>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "components/printing/common/print_messages.h"
#include "components/printing/renderer/print_web_view_helper.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "net/base/net_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerPromptReply.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

using blink::WebDataSource;
using blink::WebElement;
using blink::WebNode;
using content::SSLStatus;

namespace {

// If the source image is null or occupies less area than
// |thumbnail_min_area_pixels|, we return the image unmodified.  Otherwise, we
// scale down the image so that the width and height do not exceed
// |thumbnail_max_size_pixels|, preserving the original aspect ratio.
SkBitmap Downscale(const blink::WebImage& image,
                   int thumbnail_min_area_pixels,
                   const gfx::Size& thumbnail_max_size_pixels) {
  if (image.isNull())
    return SkBitmap();

  gfx::Size image_size = image.size();

  if (image_size.GetArea() < thumbnail_min_area_pixels)
    return image.getSkBitmap();

  if (image_size.width() <= thumbnail_max_size_pixels.width() &&
      image_size.height() <= thumbnail_max_size_pixels.height())
    return image.getSkBitmap();

  gfx::SizeF scaled_size = image_size;

  if (scaled_size.width() > thumbnail_max_size_pixels.width()) {
    scaled_size.Scale(thumbnail_max_size_pixels.width() / scaled_size.width());
  }

  if (scaled_size.height() > thumbnail_max_size_pixels.height()) {
    scaled_size.Scale(
        thumbnail_max_size_pixels.height() / scaled_size.height());
  }

  return skia::ImageOperations::Resize(image.getSkBitmap(),
                                       skia::ImageOperations::RESIZE_GOOD,
                                       static_cast<int>(scaled_size.width()),
                                       static_cast<int>(scaled_size.height()));
}

}  // namespace

ChromeRenderFrameObserver::ChromeRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
}

ChromeRenderFrameObserver::~ChromeRenderFrameObserver() {
}

bool ChromeRenderFrameObserver::OnMessageReceived(const IPC::Message& message) {
  // Filter only.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderFrameObserver, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_SetIsPrerendering, OnSetIsPrerendering)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return false;

  IPC_BEGIN_MESSAGE_MAP(ChromeRenderFrameObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_RequestThumbnailForContextNode,
                        OnRequestThumbnailForContextNode)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintNodeUnderContextMenu,
                        OnPrintNodeUnderContextMenu)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_AppBannerPromptRequest,
                        OnAppBannerPromptRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeRenderFrameObserver::OnSetIsPrerendering(bool is_prerendering) {
  if (is_prerendering) {
    // If the PrerenderHelper for this frame already exists, don't create it. It
    // can already be created for subframes during handling of
    // RenderFrameCreated, if the parent frame was prerendering at time of
    // subframe creation.
    if (prerender::PrerenderHelper::Get(render_frame()))
      return;

    // The PrerenderHelper will destroy itself either after recording histograms
    // or on destruction of the RenderView.
    new prerender::PrerenderHelper(render_frame());
  }
}

void ChromeRenderFrameObserver::OnRequestThumbnailForContextNode(
    int thumbnail_min_area_pixels,
    const gfx::Size& thumbnail_max_size_pixels) {
  WebNode context_node = render_frame()->GetContextMenuNode();
  SkBitmap thumbnail;
  gfx::Size original_size;
  if (!context_node.isNull() && context_node.isElementNode()) {
    blink::WebImage image = context_node.to<WebElement>().imageContents();
    original_size = image.size();
    thumbnail = Downscale(image,
                          thumbnail_min_area_pixels,
                          thumbnail_max_size_pixels);
  }

  SkBitmap bitmap;
  if (thumbnail.colorType() == kN32_SkColorType)
    bitmap = thumbnail;
  else
    thumbnail.copyTo(&bitmap, kN32_SkColorType);

  std::string thumbnail_data;
  SkAutoLockPixels lock(bitmap);
  if (bitmap.getPixels()) {
    const int kDefaultQuality = 90;
    std::vector<unsigned char> data;
    if (gfx::JPEGCodec::Encode(
            reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
            gfx::JPEGCodec::FORMAT_SkBitmap, bitmap.width(), bitmap.height(),
            static_cast<int>(bitmap.rowBytes()), kDefaultQuality, &data))
      thumbnail_data = std::string(data.begin(), data.end());
  }

  Send(new ChromeViewHostMsg_RequestThumbnailForContextNode_ACK(
      routing_id(), thumbnail_data, original_size));
}

void ChromeRenderFrameObserver::OnPrintNodeUnderContextMenu() {
  printing::PrintWebViewHelper* helper =
      printing::PrintWebViewHelper::Get(render_frame()->GetRenderView());
  if (helper)
    helper->PrintNode(render_frame()->GetContextMenuNode());
}

void ChromeRenderFrameObserver::DidFinishDocumentLoad() {
  // If the navigation is to a localhost URL (and the flag is set to
  // allow localhost SSL misconfigurations), print a warning to the
  // console telling the developer to check their SSL configuration
  // before going to production.
  bool allow_localhost = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowInsecureLocalhost);
  WebDataSource* ds = render_frame()->GetWebFrame()->dataSource();

  if (allow_localhost) {
    SSLStatus ssl_status = render_frame()->GetRenderView()->GetSSLStatusOfFrame(
        render_frame()->GetWebFrame());
    bool is_cert_error = net::IsCertStatusError(ssl_status.cert_status) &&
                         !net::IsCertStatusMinorError(ssl_status.cert_status);
    bool is_localhost = net::IsLocalhost(GURL(ds->request().url()).host());

    if (is_cert_error && is_localhost) {
      render_frame()->GetWebFrame()->addMessageToConsole(
          blink::WebConsoleMessage(
              blink::WebConsoleMessage::LevelWarning,
              base::ASCIIToUTF16(
                  "This site does not have a valid SSL "
                  "certificate! Without SSL, your site's and "
                  "visitors' data is vulnerable to theft and "
                  "tampering. Get a valid SSL certificate before"
                  " releasing your website to the public.")));
    }
  }
}

void ChromeRenderFrameObserver::OnAppBannerPromptRequest(
    int request_id, const std::string& platform) {
  blink::WebAppBannerPromptReply reply = blink::WebAppBannerPromptReply::None;
  render_frame()->GetWebFrame()->willShowInstallBannerPrompt(
      base::UTF8ToUTF16(platform), &reply);

  Send(new ChromeViewHostMsg_AppBannerPromptReply(
      routing_id(), request_id, reply));
}
