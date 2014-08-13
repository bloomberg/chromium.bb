// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_view_observer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/translate/translate_helper.h"
#include "chrome/renderer/webview_color_overlay.h"
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
#include "ui/gfx/size.h"
#include "ui/gfx/size_f.h"
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

// Delay in milliseconds that we'll wait before capturing the page contents
// and thumbnail.
static const int kDelayForCaptureMs = 500;

// Typically, we capture the page data once the page is loaded.
// Sometimes, the page never finishes to load, preventing the page capture
// To workaround this problem, we always perform a capture after the following
// delay.
static const int kDelayForForcedCaptureMs = 6000;

// define to write the time necessary for thumbnail/DOM text retrieval,
// respectively, into the system debug log
// #define TIME_TEXT_RETRIEVAL

// maximum number of characters in the document to index, any text beyond this
// point will be clipped
static const size_t kMaxIndexChars = 65535;

// Constants for UMA statistic collection.
static const char kTranslateCaptureText[] = "Translate.CaptureText";

namespace {

#if defined(OS_ANDROID)
// Parses the DOM for a <meta> tag with a particular name.
// |meta_tag_content| is set to the contents of the 'content' attribute.
// |found_tag| is set to true if the tag was successfully found.
// Returns true if the document was parsed without errors.
bool RetrieveMetaTagContent(const WebFrame* main_frame,
                            const GURL& expected_url,
                            const std::string& meta_tag_name,
                            bool* found_tag,
                            std::string* meta_tag_content) {
  WebDocument document =
      main_frame ? main_frame->document() : WebDocument();
  WebElement head = document.isNull() ? WebElement() : document.head();
  GURL document_url = document.isNull() ? GURL() : GURL(document.url());

  // Search the DOM for the <meta> tag with the given name.
  *found_tag = false;
  *meta_tag_content = "";
  if (!head.isNull()) {
    WebNodeList children = head.childNodes();
    for (unsigned i = 0; i < children.length(); ++i) {
      WebNode child = children.item(i);
      if (!child.isElementNode())
        continue;
      WebElement elem = child.to<WebElement>();
      if (elem.hasHTMLTagName("meta")) {
        if (elem.hasAttribute("name") && elem.hasAttribute("content")) {
          std::string name = elem.getAttribute("name").utf8();
          if (name == meta_tag_name) {
            *meta_tag_content = elem.getAttribute("content").utf8();
            *found_tag = true;
            break;
          }
        }
      }
    }
  }

  // Make sure we're checking the right page and that the length of the content
  // string is reasonable.
  bool success = document_url == expected_url;
  if (meta_tag_content->size() > chrome::kMaxMetaTagAttributeLength) {
    *meta_tag_content = "";
    success = false;
  }

  return success;
}
#endif

}  // namespace

ChromeRenderViewObserver::ChromeRenderViewObserver(
    content::RenderView* render_view,
    ChromeRenderProcessObserver* chrome_render_process_observer)
    : content::RenderViewObserver(render_view),
      chrome_render_process_observer_(chrome_render_process_observer),
      translate_helper_(new TranslateHelper(render_view)),
      phishing_classifier_(NULL),
      capture_timer_(false, false) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
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
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetName, OnSetName)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetVisuallyDeemphasized,
                        OnSetVisuallyDeemphasized)
#endif
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_UpdateTopControlsState,
                        OnUpdateTopControlsState)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_RetrieveWebappInformation,
                        OnRetrieveWebappInformation)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_RetrieveMetaTagContent,
                        OnRetrieveMetaTagContent)
#endif
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

void ChromeRenderViewObserver::OnRetrieveWebappInformation(
    const GURL& expected_url) {
  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  bool found_tag;
  std::string content_str;

  // Search for the "mobile-web-app-capable" tag.
  bool mobile_parse_success = RetrieveMetaTagContent(
      main_frame,
      expected_url,
      "mobile-web-app-capable",
      &found_tag,
      &content_str);
  bool is_mobile_webapp_capable = mobile_parse_success && found_tag &&
      LowerCaseEqualsASCII(content_str, "yes");

  // Search for the "apple-mobile-web-app-capable" tag.
  bool apple_parse_success = RetrieveMetaTagContent(
      main_frame,
      expected_url,
      "apple-mobile-web-app-capable",
      &found_tag,
      &content_str);
  bool is_apple_mobile_webapp_capable = apple_parse_success && found_tag &&
      LowerCaseEqualsASCII(content_str, "yes");

  bool is_only_apple_mobile_webapp_capable =
      is_apple_mobile_webapp_capable && !is_mobile_webapp_capable;
  if (main_frame && is_only_apple_mobile_webapp_capable) {
    blink::WebConsoleMessage message(
        blink::WebConsoleMessage::LevelWarning,
        "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\"> is "
        "deprecated. Please include <meta name=\"mobile-web-app-capable\" "
        "content=\"yes\"> - "
        "http://developers.google.com/chrome/mobile/docs/installtohomescreen");
    main_frame->addMessageToConsole(message);
  }

  Send(new ChromeViewHostMsg_DidRetrieveWebappInformation(
      routing_id(),
      mobile_parse_success && apple_parse_success,
      is_mobile_webapp_capable,
      is_apple_mobile_webapp_capable,
      expected_url));
}

void ChromeRenderViewObserver::OnRetrieveMetaTagContent(
    const GURL& expected_url,
    const std::string tag_name) {
  bool found_tag;
  std::string content_str;
  bool parsed_successfully = RetrieveMetaTagContent(
      render_view()->GetWebView()->mainFrame(),
      expected_url,
      tag_name,
      &found_tag,
      &content_str);

  Send(new ChromeViewHostMsg_DidRetrieveMetaTagContent(
      routing_id(),
      parsed_successfully && found_tag,
      tag_name,
      content_str,
      expected_url));
}
#endif

void ChromeRenderViewObserver::OnSetWindowFeatures(
    const WebWindowFeatures& window_features) {
  render_view()->GetWebView()->setWindowFeatures(window_features);
}

void ChromeRenderViewObserver::Navigate(const GURL& url) {
  // Execute cache clear operations that were postponed until a navigation
  // event (including tab reload).
  if (chrome_render_process_observer_)
    chrome_render_process_observer_->ExecutePendingClearCache();
  // Let translate_helper do any preparatory work for loading a URL.
  if (translate_helper_)
    translate_helper_->PrepareForUrl(url);
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
void ChromeRenderViewObserver::OnSetName(const std::string& name) {
  blink::WebView* web_view = render_view()->GetWebView();
  if (web_view)
    web_view->mainFrame()->setName(WebString::fromUTF8(name));
}

void ChromeRenderViewObserver::OnSetVisuallyDeemphasized(bool deemphasized) {
  bool already_deemphasized = !!dimmed_color_overlay_.get();
  if (already_deemphasized == deemphasized)
    return;

  if (deemphasized) {
    // 70% opaque grey.
    SkColor greyish = SkColorSetARGB(178, 0, 0, 0);
    dimmed_color_overlay_.reset(
        new WebViewColorOverlay(render_view(), greyish));
  } else {
    dimmed_color_overlay_.reset();
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

void ChromeRenderViewObserver::DidStopLoading() {
  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  GURL osdd_url = main_frame->document().openSearchDescriptionURL();
  if (!osdd_url.is_empty()) {
    Send(new ChromeViewHostMsg_PageHasOSDD(
        routing_id(), main_frame->document().url(), osdd_url,
        search_provider::AUTODETECTED_PROVIDER));
  }

  // Don't capture pages including refresh meta tag.
  if (HasRefreshMetaTag(main_frame))
    return;

  CapturePageInfoLater(
      false,  // preliminary_capture
      base::TimeDelta::FromMilliseconds(
          render_view()->GetContentStateImmediately() ?
              0 : kDelayForCaptureMs));
}

void ChromeRenderViewObserver::DidCommitProvisionalLoad(
    WebLocalFrame* frame, bool is_new_navigation) {
  // Don't capture pages being not new, or including refresh meta tag.
  if (!is_new_navigation || HasRefreshMetaTag(frame))
    return;

  CapturePageInfoLater(
      true,  // preliminary_capture
      base::TimeDelta::FromMilliseconds(kDelayForForcedCaptureMs));
}

void ChromeRenderViewObserver::CapturePageInfoLater(bool preliminary_capture,
                                                    base::TimeDelta delay) {
  capture_timer_.Start(
      FROM_HERE,
      delay,
      base::Bind(&ChromeRenderViewObserver::CapturePageInfo,
                 base::Unretained(this),
                 preliminary_capture));
}

void ChromeRenderViewObserver::CapturePageInfo(bool preliminary_capture) {
  if (!render_view()->GetWebView())
    return;

  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  if (!main_frame)
    return;

  // Don't index/capture pages that are in view source mode.
  if (main_frame->isViewSourceModeEnabled())
    return;

  // Don't index/capture pages that failed to load.  This only checks the top
  // level frame so the thumbnail may contain a frame that failed to load.
  WebDataSource* ds = main_frame->dataSource();
  if (ds && ds->hasUnreachableURL())
    return;

  // Don't index/capture pages that are being prerendered.
  if (prerender::PrerenderHelper::IsPrerendering(
          render_view()->GetMainRenderFrame())) {
    return;
  }

  // Retrieve the frame's full text (up to kMaxIndexChars), and pass it to the
  // translate helper for language detection and possible translation.
  base::string16 contents;
  base::TimeTicks capture_begin_time = base::TimeTicks::Now();
  CaptureText(main_frame, &contents);
  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::TimeTicks::Now() - capture_begin_time);
  if (translate_helper_)
    translate_helper_->PageCaptured(contents);

  TRACE_EVENT0("renderer", "ChromeRenderViewObserver::CapturePageInfo");

#if defined(FULL_SAFE_BROWSING)
  // Will swap out the string.
  if (phishing_classifier_)
    phishing_classifier_->PageCaptured(&contents, preliminary_capture);
#endif
}

void ChromeRenderViewObserver::CaptureText(WebFrame* frame,
                                           base::string16* contents) {
  contents->clear();
  if (!frame)
    return;

#ifdef TIME_TEXT_RETRIEVAL
  double begin = time_util::GetHighResolutionTimeNow();
#endif

  // get the contents of the frame
  *contents = frame->contentAsText(kMaxIndexChars);

#ifdef TIME_TEXT_RETRIEVAL
  double end = time_util::GetHighResolutionTimeNow();
  char buf[128];
  sprintf_s(buf, "%d chars retrieved for indexing in %gms\n",
            contents.size(), (end - begin)*1000);
  OutputDebugStringA(buf);
#endif

  // When the contents are clipped to the maximum, we don't want to have a
  // partial word indexed at the end that might have been clipped. Therefore,
  // terminate the string at the last space to ensure no words are clipped.
  if (contents->size() == kMaxIndexChars) {
    size_t last_space_index = contents->find_last_of(base::kWhitespaceUTF16);
    if (last_space_index != base::string16::npos)
      contents->resize(last_space_index);
  }
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
    if (value.isNull() || !LowerCaseEqualsASCII(value, "refresh"))
      continue;
    return true;
  }
  return false;
}
