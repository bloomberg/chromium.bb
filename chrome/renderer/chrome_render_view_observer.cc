// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_view_observer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/web_apps.h"
#include "components/translate/content/renderer/translate_helper.h"
#include "components/web_cache/renderer/web_cache_render_process_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/constants.h"
#include "net/base/data_url.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/skbitmap_operations.h"
#include "v8/include/v8-testing.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/chrome_extension_messages.h"
#endif

using blink::WebAXObject;
using blink::WebCString;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebGestureEvent;
using blink::WebIconURL;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebNodeList;
using blink::WebRect;
using blink::WebSecurityOrigin;
using blink::WebSize;
using blink::WebString;
using blink::WebTouchEvent;
using blink::WebURL;
using blink::WebURLRequest;
using blink::WebView;
using blink::WebVector;
using blink::WebWindowFeatures;
using content::RenderFrame;

// Delay in milliseconds that we'll wait before capturing the page contents.
static const int kDelayForCaptureMs = 500;

// Typically, we capture the page data once the page is loaded.
// Sometimes, the page never finishes to load, preventing the page capture
// To workaround this problem, we always perform a capture after the following
// delay.
static const int kDelayForForcedCaptureMs = 6000;

// maximum number of characters in the document to index, any text beyond this
// point will be clipped
static const size_t kMaxIndexChars = 65535;

// Constants for UMA statistic collection.
static const char kTranslateCaptureText[] = "Translate.CaptureText";

PageInfo::PageInfo(PageInfoReceiver* context)
    : context_(context), capture_timer_(false, false) {
  DCHECK(context_);
}

void PageInfo::CapturePageInfoLater(CaptureType capture_type,
                                    RenderFrame* render_frame,
                                    base::TimeDelta delay) {
  capture_timer_.Start(
      FROM_HERE, delay,
      base::Bind(&PageInfo::CapturePageInfo, base::Unretained(this),
                 render_frame, capture_type));
}

bool PageInfo::IsErrorPage(WebLocalFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  return ds && ds->hasUnreachableURL();
}

void PageInfo::CapturePageInfo(RenderFrame* render_frame,
                               CaptureType capture_type) {
  if (!render_frame)
    return;

  WebLocalFrame* frame = render_frame->GetWebFrame();
  if (!frame)
    return;

  // Don't index/capture pages that are in view source mode.
  if (frame->isViewSourceModeEnabled())
    return;

  if (IsErrorPage(frame))
    return;

  // Don't index/capture pages that are being prerendered.
  if (prerender::PrerenderHelper::IsPrerendering(render_frame)) {
    return;
  }

  // Retrieve the frame's full text (up to kMaxIndexChars), and pass it to the
  // translate helper for language detection and possible translation.
  base::string16 contents;
  base::TimeTicks capture_begin_time = base::TimeTicks::Now();
  CaptureText(frame, &contents);
  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::TimeTicks::Now() - capture_begin_time);
  context_->PageCaptured(&contents, capture_type);
}

void PageInfo::CaptureText(WebLocalFrame* frame, base::string16* content) {
  content->clear();

  // get the contents of the frame
  *content = frame->contentAsText(kMaxIndexChars);

  // When the contents are clipped to the maximum, we don't want to have a
  // partial word indexed at the end that might have been clipped. Therefore,
  // terminate the string at the last space to ensure no words are clipped.
  if (content->size() == kMaxIndexChars) {
    size_t last_space_index = content->find_last_of(base::kWhitespaceUTF16);
    if (last_space_index != base::string16::npos)
      content->resize(last_space_index);
  }
}

ChromeRenderViewObserver::ChromeRenderViewObserver(
    content::RenderView* render_view,
    web_cache::WebCacheRenderProcessObserver* web_cache_render_process_observer)
    : content::RenderViewObserver(render_view),
      web_cache_render_process_observer_(web_cache_render_process_observer),
      translate_helper_(
          new translate::TranslateHelper(render_view,
                                         chrome::ISOLATED_WORLD_ID_TRANSLATE,
                                         0,
                                         extensions::kExtensionScheme)),
      phishing_classifier_(NULL),
      webview_visually_deemphasized_(false),
      page_info_(this) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kDisableClientSidePhishingDetection))
    OnSetClientSidePhishingDetection(true);
}

ChromeRenderViewObserver::~ChromeRenderViewObserver() {
}

bool ChromeRenderViewObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderViewObserver, message)
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_WebUIJavaScript, OnWebUIJavaScript)
#endif
#if defined(ENABLE_EXTENSIONS)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetVisuallyDeemphasized,
                        OnSetVisuallyDeemphasized)
#endif
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_UpdateTopControlsState,
                        OnUpdateTopControlsState)
#endif
    IPC_MESSAGE_HANDLER(ChromeViewMsg_GetWebApplicationInfo,
                        OnGetWebApplicationInfo)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetClientSidePhishingDetection,
                        OnSetClientSidePhishingDetection)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetWindowFeatures, OnSetWindowFeatures)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
void ChromeRenderViewObserver::OnWebUIJavaScript(
    const base::string16& javascript) {
  webui_javascript_.push_back(javascript);
}
#endif

#if defined(OS_ANDROID)
void ChromeRenderViewObserver::OnUpdateTopControlsState(
    content::TopControlsState constraints,
    content::TopControlsState current,
    bool animate) {
  render_view()->UpdateTopControlsState(constraints, current, animate);
}
#endif

void ChromeRenderViewObserver::OnGetWebApplicationInfo() {
  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  DCHECK(main_frame);

  WebApplicationInfo web_app_info;
  web_apps::ParseWebAppFromWebDocument(main_frame, &web_app_info);

  // The warning below is specific to mobile but it doesn't hurt to show it even
  // if the Chromium build is running on a desktop. It will get more exposition.
  if (web_app_info.mobile_capable ==
        WebApplicationInfo::MOBILE_CAPABLE_APPLE) {
    blink::WebConsoleMessage message(
        blink::WebConsoleMessage::LevelWarning,
        "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\"> is "
        "deprecated. Please include <meta name=\"mobile-web-app-capable\" "
        "content=\"yes\"> - "
        "http://developers.google.com/chrome/mobile/docs/installtohomescreen");
    main_frame->addMessageToConsole(message);
  }

  // Prune out any data URLs in the set of icons.  The browser process expects
  // any icon with a data URL to have originated from a favicon.  We don't want
  // to decode arbitrary data URLs in the browser process.  See
  // http://b/issue?id=1162972
  for (std::vector<WebApplicationInfo::IconInfo>::iterator it =
          web_app_info.icons.begin(); it != web_app_info.icons.end();) {
    if (it->url.SchemeIs(url::kDataScheme))
      it = web_app_info.icons.erase(it);
    else
      ++it;
  }

  // Truncate the strings we send to the browser process.
  web_app_info.title =
      web_app_info.title.substr(0, chrome::kMaxMetaTagAttributeLength);
  web_app_info.description =
      web_app_info.description.substr(0, chrome::kMaxMetaTagAttributeLength);

  Send(new ChromeViewHostMsg_DidGetWebApplicationInfo(
      routing_id(), web_app_info));
}

void ChromeRenderViewObserver::OnSetWindowFeatures(
    const WebWindowFeatures& window_features) {
  render_view()->GetWebView()->setWindowFeatures(window_features);
}

void ChromeRenderViewObserver::Navigate(const GURL& url) {
  // Execute cache clear operations that were postponed until a navigation
  // event (including tab reload).
  if (web_cache_render_process_observer_)
    web_cache_render_process_observer_->ExecutePendingClearCache();
}

void ChromeRenderViewObserver::OnSetClientSidePhishingDetection(
    bool enable_phishing_detection) {
#if defined(FULL_SAFE_BROWSING) && !defined(OS_CHROMEOS)
  phishing_classifier_ = enable_phishing_detection ?
      safe_browsing::PhishingClassifierDelegate::Create(render_view(), NULL) :
      NULL;
#endif
}

#if defined(ENABLE_EXTENSIONS)
void ChromeRenderViewObserver::OnSetVisuallyDeemphasized(bool deemphasized) {
  if (webview_visually_deemphasized_ == deemphasized)
    return;

  webview_visually_deemphasized_ = deemphasized;

  if (deemphasized) {
    // 70% opaque grey.
    SkColor greyish = SkColorSetARGB(178, 0, 0, 0);
    render_view()->GetWebView()->setPageOverlayColor(greyish);
  } else {
    render_view()->GetWebView()->setPageOverlayColor(SK_ColorTRANSPARENT);
  }
}
#endif

void ChromeRenderViewObserver::DidStartLoading() {
  if ((render_view()->GetEnabledBindings() & content::BINDINGS_POLICY_WEB_UI) &&
      !webui_javascript_.empty()) {
    for (size_t i = 0; i < webui_javascript_.size(); ++i) {
      render_view()->GetMainRenderFrame()->ExecuteJavaScript(
          webui_javascript_[i]);
    }
    webui_javascript_.clear();
  }
}

void ChromeRenderViewObserver::DidFinishLoad(blink::WebLocalFrame* frame) {
  // Don't do anything for subframes.
  if (frame->parent())
    return;

  GURL osdd_url = frame->document().openSearchDescriptionURL();
  if (!osdd_url.is_empty()) {
    Send(new ChromeViewHostMsg_PageHasOSDD(
        routing_id(), frame->document().url(), osdd_url,
        search_provider::AUTODETECTED_PROVIDER));
  }

  // Don't capture pages including refresh meta tag.
  if (HasRefreshMetaTag(frame))
    return;

  page_info_.CapturePageInfoLater(
      FINAL_CAPTURE, render_view()->GetMainRenderFrame(),
      base::TimeDelta::FromMilliseconds(
          render_view()->GetContentStateImmediately() ? 0
                                                      : kDelayForCaptureMs));
}

void ChromeRenderViewObserver::DidStartProvisionalLoad(
    blink::WebLocalFrame* frame) {
  // Let translate_helper do any preparatory work for loading a URL.
  if (translate_helper_)
    translate_helper_->PrepareForUrl(frame->document().url());
}

void ChromeRenderViewObserver::DidCommitProvisionalLoad(
    WebLocalFrame* frame, bool is_new_navigation) {
  // Don't capture pages being not new, or including refresh meta tag.
  if (!is_new_navigation || HasRefreshMetaTag(frame))
    return;

  base::debug::SetCrashKeyValue(
      crash_keys::kViewCount,
      base::SizeTToString(content::RenderView::GetRenderViewCount()));

  page_info_.CapturePageInfoLater(
      PRELIMINARY_CAPTURE, render_view()->GetMainRenderFrame(),
      base::TimeDelta::FromMilliseconds(kDelayForForcedCaptureMs));
}

bool ChromeRenderViewObserver::HasRefreshMetaTag(WebFrame* frame) {
  if (!frame)
    return false;
  WebElement head = frame->document().head();
  if (head.isNull() || !head.hasChildNodes())
    return false;

  const WebString tag_name(base::ASCIIToUTF16("meta"));
  const WebString attribute_name(base::ASCIIToUTF16("http-equiv"));

  WebNodeList children = head.childNodes();
  for (size_t i = 0; i < children.length(); ++i) {
    WebNode node = children.item(i);
    if (!node.isElementNode())
      continue;
    WebElement element = node.to<WebElement>();
    if (!element.hasHTMLTagName(tag_name))
      continue;
    WebString value = element.getAttribute(attribute_name);
    if (value.isNull() ||
        !base::LowerCaseEqualsASCII(base::StringPiece16(value), "refresh"))
      continue;
    return true;
  }
  return false;
}

void ChromeRenderViewObserver::PageCaptured(base::string16* content,
                                            CaptureType capture_type) {
  if (translate_helper_)
    translate_helper_->PageCaptured(*content);

  TRACE_EVENT0("renderer", "ChromeRenderViewObserver::CapturePageInfo");

#if defined(FULL_SAFE_BROWSING)
  // Will swap out the string.
  if (phishing_classifier_)
    phishing_classifier_->PageCaptured(content,
                                       capture_type == PRELIMINARY_CAPTURE);
#endif
}
