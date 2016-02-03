// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include <stddef.h>

#include <limits>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "components/translate/content/renderer/translate_helper.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/constants.h"
#include "net/base/net_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerPromptReply.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"

#if defined(ENABLE_PRINTING)
#include "components/printing/common/print_messages.h"
#include "components/printing/renderer/print_web_view_helper.h"
#endif

using blink::WebDataSource;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebString;
using content::SSLStatus;
using content::RenderFrame;

// Delay in milliseconds that we'll wait before capturing the page contents.
static const int kDelayForCaptureMs = 500;

// Typically, we capture the page data once the page is loaded.
// Sometimes, the page never finishes to load, preventing the page capture
// To workaround this problem, we always perform a capture after the following
// delay.
static const int kDelayForForcedCaptureMs = 6000;

// Maximum number of characters in the document to index.
// Any text beyond this point will be clipped.
static const size_t kMaxIndexChars = 65535;

// Constants for UMA statistic collection.
static const char kTranslateCaptureText[] = "Translate.CaptureText";

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

  gfx::SizeF scaled_size = gfx::SizeF(image_size);

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
    : content::RenderFrameObserver(render_frame),
      capture_timer_(false, false),
      translate_helper_(nullptr),
      phishing_classifier_(nullptr) {
  // Don't do anything for subframes.
  if (!render_frame->IsMainFrame())
    return;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kDisableClientSidePhishingDetection))
    OnSetClientSidePhishingDetection(true);
  translate_helper_ = new translate::TranslateHelper(
      render_frame, chrome::ISOLATED_WORLD_ID_TRANSLATE, 0,
      extensions::kExtensionScheme);
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
    IPC_MESSAGE_HANDLER(ChromeViewMsg_RequestReloadImageForContextNode,
                        OnRequestReloadImageForContextNode)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_RequestThumbnailForContextNode,
                        OnRequestThumbnailForContextNode)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetClientSidePhishingDetection,
                        OnSetClientSidePhishingDetection)
#if defined(ENABLE_PRINTING)
    IPC_MESSAGE_HANDLER(PrintMsg_PrintNodeUnderContextMenu,
                        OnPrintNodeUnderContextMenu)
#endif
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

void ChromeRenderFrameObserver::OnRequestReloadImageForContextNode() {
  WebNode context_node = render_frame()->GetContextMenuNode();
  if (!context_node.isNull() && context_node.isElementNode() &&
      render_frame()->GetWebFrame()) {
    render_frame()->GetWebFrame()->reloadImage(context_node);
  }
}

void ChromeRenderFrameObserver::OnRequestThumbnailForContextNode(
    int thumbnail_min_area_pixels,
    const gfx::Size& thumbnail_max_size_pixels,
    int callback_id) {
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
      routing_id(), thumbnail_data, original_size, callback_id));
}

void ChromeRenderFrameObserver::OnPrintNodeUnderContextMenu() {
#if defined(ENABLE_PRINTING)
  printing::PrintWebViewHelper* helper =
      printing::PrintWebViewHelper::Get(render_frame()->GetRenderView());
  if (helper)
    helper->PrintNode(render_frame()->GetContextMenuNode());
#endif
}

void ChromeRenderFrameObserver::OnSetClientSidePhishingDetection(
    bool enable_phishing_detection) {
#if defined(SAFE_BROWSING_CSD)
  phishing_classifier_ =
      enable_phishing_detection
          ? safe_browsing::PhishingClassifierDelegate::Create(render_frame(),
                                                              nullptr)
          : nullptr;
#endif
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
    int request_id,
    const std::string& platform) {
  // App banner prompt requests are handled in the general chrome render frame
  // observer, not the AppBannerClient, as the AppBannerClient is created lazily
  // by blink and may not exist when the request is sent.
  blink::WebAppBannerPromptReply reply = blink::WebAppBannerPromptReply::None;
  blink::WebString web_platform(base::UTF8ToUTF16(platform));
  blink::WebVector<blink::WebString> web_platforms(&web_platform, 1);

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  frame->willShowInstallBannerPrompt(request_id, web_platforms, &reply);

  // Extract the referrer header for this site according to its referrer policy.
  // Pass in an empty URL as the destination so that it is always treated
  // as a cross-origin request.
  std::string referrer = blink::WebSecurityPolicy::generateReferrerHeader(
      frame->document().referrerPolicy(), GURL(),
      frame->document().outgoingReferrer()).utf8();

  Send(new ChromeViewHostMsg_AppBannerPromptReply(
      routing_id(), request_id, reply, referrer));
}

void ChromeRenderFrameObserver::DidFinishLoad() {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->parent())
    return;

  GURL osdd_url = frame->document().openSearchDescriptionURL();
  if (!osdd_url.is_empty()) {
    Send(new ChromeViewHostMsg_PageHasOSDD(
        routing_id(), frame->document().url(), osdd_url,
        search_provider::AUTODETECTED_PROVIDER));
  }

  // Don't capture pages that have pending redirect or location change.
  if (frame->isNavigationScheduled())
    return;

  CapturePageTextLater(
      FINAL_CAPTURE,
      base::TimeDelta::FromMilliseconds(
          render_frame()->GetRenderView()->GetContentStateImmediately()
              ? 0
              : kDelayForCaptureMs));
}

void ChromeRenderFrameObserver::DidStartProvisionalLoad() {
  // Let translate_helper do any preparatory work for loading a URL.
  if (!translate_helper_)
    return;

  translate_helper_->PrepareForUrl(
      render_frame()->GetWebFrame()->document().url());
}

void ChromeRenderFrameObserver::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  WebLocalFrame* frame = render_frame()->GetWebFrame();

  // Don't do anything for subframes.
  if (frame->parent())
    return;

  // Don't capture pages being not new, with pending redirect, or location
  // change.
  if (!is_new_navigation || frame->isNavigationScheduled())
    return;

  base::debug::SetCrashKeyValue(
      crash_keys::kViewCount,
      base::SizeTToString(content::RenderView::GetRenderViewCount()));

  CapturePageTextLater(PRELIMINARY_CAPTURE, base::TimeDelta::FromMilliseconds(
                                                kDelayForForcedCaptureMs));
}

void ChromeRenderFrameObserver::CapturePageText(TextCaptureType capture_type) {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame)
    return;

  // Don't index/capture pages that are in view source mode.
  if (frame->isViewSourceModeEnabled())
    return;

  // Don't capture text of the error pages.
  WebDataSource* ds = frame->dataSource();
  if (ds && ds->hasUnreachableURL())
    return;

  // Don't index/capture pages that are being prerendered.
  if (prerender::PrerenderHelper::IsPrerendering(render_frame()))
    return;

  base::TimeTicks capture_begin_time = base::TimeTicks::Now();

  // Retrieve the frame's full text (up to kMaxIndexChars), and pass it to the
  // translate helper for language detection and possible translation.
  base::string16 contents = frame->contentAsText(kMaxIndexChars);

  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::TimeTicks::Now() - capture_begin_time);

  if (translate_helper_)
    translate_helper_->PageCaptured(contents);

  TRACE_EVENT0("renderer", "ChromeRenderFrameObserver::CapturePageText");

#if defined(SAFE_BROWSING_CSD)
  // Will swap out the string.
  if (phishing_classifier_)
    phishing_classifier_->PageCaptured(&contents,
                                       capture_type == PRELIMINARY_CAPTURE);
#endif
}

void ChromeRenderFrameObserver::CapturePageTextLater(
    TextCaptureType capture_type,
    base::TimeDelta delay) {
  capture_timer_.Start(FROM_HERE, delay,
                       base::Bind(&ChromeRenderFrameObserver::CapturePageText,
                                  base::Unretained(this), capture_type));
}
