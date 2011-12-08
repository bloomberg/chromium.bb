// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_view_observer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/icon_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/about_handler.h"
#include "chrome/renderer/automation/dom_automation_controller.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/external_host_bindings.h"
#include "chrome/renderer/frame_sniffer.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/translate_helper.h"
#include "chrome/renderer/webview_color_overlay.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/content_renderer_client.h"
#include "net/base/data_url.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "webkit/glue/image_decoder.h"
#include "webkit/glue/image_resource_fetcher.h"
#include "webkit/glue/webkit_glue.h"
#include "v8/include/v8-testing.h"

using WebKit::WebAccessibilityObject;
using WebKit::WebCString;
using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebIconURL;
using WebKit::WebRect;
using WebKit::WebSecurityOrigin;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebTouchEvent;
using WebKit::WebURL;
using WebKit::WebURLRequest;
using WebKit::WebView;
using WebKit::WebVector;
using webkit_glue::ImageResourceFetcher;

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

// Size of the thumbnails that we'll generate
static const int kThumbnailWidth = 212;
static const int kThumbnailHeight = 132;

// Constants for UMA statistic collection.
static const char kWWWDotGoogleDotCom[] = "www.google.com";
static const char kMailDotGoogleDotCom[] = "mail.google.com";
static const char kPlusDotGoogleDotCom[] = "plus.google.com";
static const char kDocsDotGoogleDotCom[] = "docs.google.com";
static const char kSitesDotGoogleDotCom[] = "sites.google.com";
static const char kPicasawebDotGoogleDotCom[] = "picasaweb.google.com";
static const char kCodeDotGoogleDotCom[] = "code.google.com";
static const char kGroupsDotGoogleDotCom[] = "groups.google.com";
static const char kMapsDotGoogleDotCom[] = "maps.google.com";
static const char kWWWDotYoutubeDotCom[] = "www.youtube.com";
static const char kDotGoogleUserContentDotCom[] = ".googleusercontent.com";
static const char kGoogleReaderPathPrefix[] = "/reader/";
static const char kGoogleSupportPathPrefix[] = "/support/";
static const char kGoogleIntlPathPrefix[] = "/intl/";
static const char kDotJS[] = ".js";
static const char kDotCSS[] = ".css";
static const char kDotSWF[] = ".swf";
static const char kDotHTML[] = ".html";
enum {
  INSECURE_CONTENT_DISPLAY = 0,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_WWW_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HTML,
  INSECURE_CONTENT_RUN,
  INSECURE_CONTENT_RUN_HOST_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_WWW_GOOGLE,
  INSECURE_CONTENT_RUN_TARGET_YOUTUBE,
  INSECURE_CONTENT_RUN_JS,
  INSECURE_CONTENT_RUN_CSS,
  INSECURE_CONTENT_RUN_SWF,
  INSECURE_CONTENT_DISPLAY_HOST_YOUTUBE,
  INSECURE_CONTENT_RUN_HOST_YOUTUBE,
  INSECURE_CONTENT_RUN_HOST_GOOGLEUSERCONTENT,
  INSECURE_CONTENT_DISPLAY_HOST_MAIL_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_MAIL_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_PLUS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_PLUS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_DOCS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_DOCS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_SITES_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_SITES_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_PICASAWEB_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_PICASAWEB_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_READER,
  INSECURE_CONTENT_RUN_HOST_GOOGLE_READER,
  INSECURE_CONTENT_DISPLAY_HOST_CODE_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_CODE_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_GROUPS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_GROUPS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_MAPS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_MAPS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_SUPPORT,
  INSECURE_CONTENT_RUN_HOST_GOOGLE_SUPPORT,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_INTL,
  INSECURE_CONTENT_RUN_HOST_GOOGLE_INTL,
  INSECURE_CONTENT_NUM_EVENTS
};

// Constants for mixed-content blocking.
static const char kGoogleDotCom[] = "google.com";
static const char kFacebookDotCom[] = "facebook.com";
static const char kTwitterDotCom[] = "twitter.com";

static bool PaintViewIntoCanvas(WebView* view,
                                skia::PlatformCanvas& canvas) {
  view->layout();
  const WebSize& size = view->size();

  if (!canvas.initialize(size.width, size.height, true))
    return false;

  view->paint(webkit_glue::ToWebCanvas(&canvas),
              WebRect(0, 0, size.width, size.height));
  // TODO: Add a way to snapshot the whole page, not just the currently
  // visible part.

  return true;
}

// Calculates how "boring" a thumbnail is. The boring score is the
// 0,1 ranged percentage of pixels that are the most common
// luma. Higher boring scores indicate that a higher percentage of a
// bitmap are all the same brightness.
static double CalculateBoringScore(SkBitmap* bitmap) {
  int histogram[256] = {0};
  color_utils::BuildLumaHistogram(bitmap, histogram);

  int color_count = *std::max_element(histogram, histogram + 256);
  int pixel_count = bitmap->width() * bitmap->height();
  return static_cast<double>(color_count) / pixel_count;
}

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

static bool isHostInDomain(const std::string& host, const std::string& domain) {
  return (EndsWith(host, domain, false) &&
          (host.length() == domain.length() ||
           (host.length() > domain.length() &&
            host[host.length() - domain.length() - 1] == '.')));
}

namespace {
GURL StripRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}
}  // namespace

ChromeRenderViewObserver::ChromeRenderViewObserver(
    content::RenderView* render_view,
    ContentSettingsObserver* content_settings,
    ChromeRenderProcessObserver* chrome_render_process_observer,
    ExtensionDispatcher* extension_dispatcher,
    TranslateHelper* translate_helper)
    : content::RenderViewObserver(render_view),
      chrome_render_process_observer_(chrome_render_process_observer),
      extension_dispatcher_(extension_dispatcher),
      content_settings_(content_settings),
      translate_helper_(translate_helper),
      phishing_classifier_(NULL),
      last_indexed_page_id_(-1),
      allow_displaying_insecure_content_(false),
      allow_running_insecure_content_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDomAutomationController)) {
    int old_bindings = render_view->GetEnabledBindings();
    render_view->SetEnabledBindings(
        old_bindings |= content::BINDINGS_POLICY_DOM_AUTOMATION);
  }
  render_view->GetWebView()->setPermissionClient(this);
  if (!command_line.HasSwitch(switches::kDisableClientSidePhishingDetection))
    OnSetClientSidePhishingDetection(true);
}

ChromeRenderViewObserver::~ChromeRenderViewObserver() {
}

bool ChromeRenderViewObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderViewObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_WebUIJavaScript, OnWebUIJavaScript)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_CaptureSnapshot, OnCaptureSnapshot)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_HandleMessageFromExternalHost,
                        OnHandleMessageFromExternalHost)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_JavaScriptStressTestControl,
                        OnJavaScriptStressTestControl)
    IPC_MESSAGE_HANDLER(IconMsg_DownloadFavicon, OnDownloadFavicon)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetAllowDisplayingInsecureContent,
                        OnSetAllowDisplayingInsecureContent)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetAllowRunningInsecureContent,
                        OnSetAllowRunningInsecureContent)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetClientSidePhishingDetection,
                        OnSetClientSidePhishingDetection)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetVisuallyDeemphasized,
                        OnSetVisuallyDeemphasized)
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_StartFrameSniffer, OnStartFrameSniffer)
#endif
    IPC_MESSAGE_HANDLER(ChromeViewMsg_GetFPS, OnGetFPS)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_AddStrictSecurityHost,
                        OnAddStrictSecurityHost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Filter only.
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderViewObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetIsPrerendering, OnSetIsPrerendering);
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeRenderViewObserver::OnWebUIJavaScript(
    const string16& frame_xpath,
    const string16& jscript,
    int id,
    bool notify_result) {
  webui_javascript_.reset(new WebUIJavaScript());
  webui_javascript_->frame_xpath = frame_xpath;
  webui_javascript_->jscript = jscript;
  webui_javascript_->id = id;
  webui_javascript_->notify_result = notify_result;
}

void ChromeRenderViewObserver::OnCaptureSnapshot() {
  SkBitmap snapshot;
  bool error = false;

  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  if (!main_frame)
    error = true;

  if (!error && !CaptureSnapshot(render_view()->GetWebView(), &snapshot))
    error = true;

  DCHECK(error == snapshot.empty()) <<
      "Snapshot should be empty on error, non-empty otherwise.";

  // Send the snapshot to the browser process.
  Send(new ChromeViewHostMsg_Snapshot(routing_id(), snapshot));
}

void ChromeRenderViewObserver::OnHandleMessageFromExternalHost(
    const std::string& message,
    const std::string& origin,
    const std::string& target) {
  if (message.empty())
    return;
  GetExternalHostBindings()->ForwardMessageFromExternalHost(message, origin,
                                                            target);
}

void ChromeRenderViewObserver::OnJavaScriptStressTestControl(int cmd,
                                                             int param) {
  if (cmd == kJavaScriptStressTestSetStressRunType) {
    v8::Testing::SetStressRunType(static_cast<v8::Testing::StressType>(param));
  } else if (cmd == kJavaScriptStressTestPrepareStressRun) {
    v8::Testing::PrepareStressRun(param);
  }
}

void ChromeRenderViewObserver::OnDownloadFavicon(int id,
                                                 const GURL& image_url,
                                                 int image_size) {
  bool data_image_failed = false;
  if (image_url.SchemeIs("data")) {
    SkBitmap data_image = ImageFromDataUrl(image_url);
    data_image_failed = data_image.empty();
    if (!data_image_failed) {
      Send(new IconHostMsg_DidDownloadFavicon(
          routing_id(), id, image_url, false, data_image));
    }
  }

  if (data_image_failed ||
      !DownloadFavicon(id, image_url, image_size)) {
    Send(new IconHostMsg_DidDownloadFavicon(
        routing_id(), id, image_url, true, SkBitmap()));
  }
}

void ChromeRenderViewObserver::OnSetAllowDisplayingInsecureContent(bool allow) {
  allow_displaying_insecure_content_ = allow;
  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  if (main_frame)
    main_frame->reload();
}

void ChromeRenderViewObserver::OnSetAllowRunningInsecureContent(bool allow) {
  allow_running_insecure_content_ = allow;
  OnSetAllowDisplayingInsecureContent(allow);
}

void ChromeRenderViewObserver::OnAddStrictSecurityHost(
    const std::string& host) {
  strict_security_hosts_.insert(host);
}

void ChromeRenderViewObserver::Navigate(const GURL& url) {
  // Execute cache clear operations that were postponed until a navigation
  // event (including tab reload).
  if (chrome_render_process_observer_)
    chrome_render_process_observer_->ExecutePendingClearCache();
  AboutHandler::MaybeHandle(url);
}

void ChromeRenderViewObserver::OnSetClientSidePhishingDetection(
    bool enable_phishing_detection) {
#if defined(ENABLE_SAFE_BROWSING) && !defined(OS_CHROMEOS)
  phishing_classifier_ = enable_phishing_detection ?
      safe_browsing::PhishingClassifierDelegate::Create(
          render_view(), NULL) :
      NULL;
#endif
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

void ChromeRenderViewObserver::OnStartFrameSniffer(const string16& frame_name) {
  new FrameSniffer(render_view(), frame_name);
}

void ChromeRenderViewObserver::OnGetFPS() {
  float fps = (render_view()->GetFilteredTimePerFrame() > 0.0f)?
      1.0f / render_view()->GetFilteredTimePerFrame() : 0.0f;
  Send(new ChromeViewHostMsg_FPS(routing_id(), fps));
}

bool ChromeRenderViewObserver::allowDatabase(
    WebFrame* frame,
    const WebString& name,
    const WebString& display_name,
    unsigned long estimated_size) {
  return content_settings_->AllowDatabase(
      frame, name, display_name, estimated_size);
}

bool ChromeRenderViewObserver::allowFileSystem(WebFrame* frame) {
  return content_settings_->AllowFileSystem(frame);
}

bool ChromeRenderViewObserver::allowImage(WebFrame* frame,
                                          bool enabled_per_settings,
                                          const WebURL& image_url) {
  return content_settings_->AllowImage(frame, enabled_per_settings, image_url);
}

bool ChromeRenderViewObserver::allowIndexedDB(WebFrame* frame,
                                              const WebString& name,
                                              const WebSecurityOrigin& origin) {
  return content_settings_->AllowIndexedDB(frame, name, origin);
}

bool ChromeRenderViewObserver::allowPlugins(WebFrame* frame,
                                           bool enabled_per_settings) {
  return content_settings_->AllowPlugins(frame, enabled_per_settings);
}

bool ChromeRenderViewObserver::allowScript(WebFrame* frame,
                                          bool enabled_per_settings) {
  return content_settings_->AllowScript(frame, enabled_per_settings);
}

bool ChromeRenderViewObserver::allowScriptFromSource(
    WebFrame* frame,
    bool enabled_per_settings,
    const WebURL& script_url) {
  return content_settings_->AllowScriptFromSource(frame,
                                                  enabled_per_settings,
                                                  script_url);
}

bool ChromeRenderViewObserver::allowScriptExtension(
    WebFrame* frame, const WebString& extension_name, int extension_group) {
  return extension_dispatcher_->AllowScriptExtension(
      frame, extension_name.utf8(), extension_group);
}

bool ChromeRenderViewObserver::allowStorage(WebFrame* frame, bool local) {
  return content_settings_->AllowStorage(frame, local);
}

bool ChromeRenderViewObserver::allowReadFromClipboard(WebFrame* frame,
                                                     bool default_value) {
  bool allowed = false;
  // TODO(dcheng): Should we consider a toURL() method on WebSecurityOrigin?
  Send(new ChromeViewHostMsg_CanTriggerClipboardRead(
      routing_id(), GURL(frame->document().securityOrigin().toString().utf8()),
      &allowed));
  return allowed;
}

bool ChromeRenderViewObserver::allowWriteToClipboard(WebFrame* frame,
                                                    bool default_value) {
  bool allowed = false;
  Send(new ChromeViewHostMsg_CanTriggerClipboardWrite(
      routing_id(), GURL(frame->document().securityOrigin().toString().utf8()),
      &allowed));
  return allowed;
}

static void SendInsecureContentSignal(int signal) {
  UMA_HISTOGRAM_ENUMERATION("SSL.InsecureContent", signal,
                            INSECURE_CONTENT_NUM_EVENTS);
}

bool ChromeRenderViewObserver::allowDisplayingInsecureContent(
    WebKit::WebFrame* frame,
    bool allowed_per_settings,
    const WebKit::WebSecurityOrigin& origin,
    const WebKit::WebURL& resource_url) {
  SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY);

  std::string origin_host(origin.host().utf8());
  GURL frame_gurl(frame->document().url());
  if (isHostInDomain(origin_host, kGoogleDotCom)) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleSupportPathPrefix, false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_SUPPORT);
    } else if (StartsWithASCII(frame_gurl.path(),
                               kGoogleIntlPathPrefix,
                               false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_INTL);
    }
  }

  if (origin_host == kWWWDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_WWW_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleReaderPathPrefix, false))
      SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_READER);
  } else if (origin_host == kMailDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_MAIL_GOOGLE);
  } else if (origin_host == kPlusDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_PLUS_GOOGLE);
  } else if (origin_host == kDocsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_DOCS_GOOGLE);
  } else if (origin_host == kSitesDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_SITES_GOOGLE);
  } else if (origin_host == kPicasawebDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_PICASAWEB_GOOGLE);
  } else if (origin_host == kCodeDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_CODE_GOOGLE);
  } else if (origin_host == kGroupsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GROUPS_GOOGLE);
  } else if (origin_host == kMapsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_MAPS_GOOGLE);
  } else if (origin_host == kWWWDotYoutubeDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_YOUTUBE);
  }

  GURL resource_gurl(resource_url);
  if (EndsWith(resource_gurl.path(), kDotHTML, false))
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HTML);

  if (allowed_per_settings || allow_displaying_insecure_content_)
    return true;

  Send(new ChromeViewHostMsg_DidBlockDisplayingInsecureContent(routing_id()));
  return false;
}

bool ChromeRenderViewObserver::allowRunningInsecureContent(
    WebKit::WebFrame* frame,
    bool allowed_per_settings,
    const WebKit::WebSecurityOrigin& origin,
    const WebKit::WebURL& resource_url) {
  // Single value to control permissive mixed content behaviour.  We flip
  // this at the present between beta / stable releases.
  const bool block_insecure_content_on_all_domains = false;

  std::string origin_host(origin.host().utf8());
  GURL frame_gurl(frame->document().url());
  DCHECK_EQ(frame_gurl.host(), origin_host);

  bool is_google = isHostInDomain(origin_host, kGoogleDotCom);
  if (is_google) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleSupportPathPrefix, false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE_SUPPORT);
    } else if (StartsWithASCII(frame_gurl.path(),
                               kGoogleIntlPathPrefix,
                               false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE_INTL);
    }
  }

  if (origin_host == kWWWDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_WWW_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleReaderPathPrefix, false))
      SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE_READER);
  } else if (origin_host == kMailDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_MAIL_GOOGLE);
  } else if (origin_host == kPlusDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_PLUS_GOOGLE);
  } else if (origin_host == kDocsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_DOCS_GOOGLE);
  } else if (origin_host == kSitesDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_SITES_GOOGLE);
  } else if (origin_host == kPicasawebDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_PICASAWEB_GOOGLE);
  } else if (origin_host == kCodeDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_CODE_GOOGLE);
  } else if (origin_host == kGroupsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GROUPS_GOOGLE);
  } else if (origin_host == kMapsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_MAPS_GOOGLE);
  } else if (origin_host == kWWWDotYoutubeDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_YOUTUBE);
  } else if (EndsWith(origin_host, kDotGoogleUserContentDotCom, false)) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLEUSERCONTENT);
  }

  GURL resource_gurl(resource_url);
  if (resource_gurl.host() == kWWWDotYoutubeDotCom)
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_TARGET_YOUTUBE);

  if (EndsWith(resource_gurl.path(), kDotJS, false))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_JS);
  else if (EndsWith(resource_gurl.path(), kDotCSS, false))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_CSS);
  else if (EndsWith(resource_gurl.path(), kDotSWF, false))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_SWF);

  if (!allow_running_insecure_content_ &&
      !allowed_per_settings &&
      (block_insecure_content_on_all_domains ||
       CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kNoRunningInsecureContent) ||
       is_google ||
       isHostInDomain(origin_host, kFacebookDotCom) ||
       isHostInDomain(origin_host, kTwitterDotCom) ||
       IsStrictSecurityHost(origin_host))) {
    Send(new ChromeViewHostMsg_DidBlockRunningInsecureContent(routing_id()));
    return false;
  }

  return true;
}

void ChromeRenderViewObserver::didNotAllowPlugins(WebFrame* frame) {
  content_settings_->DidNotAllowPlugins(frame);
}

void ChromeRenderViewObserver::didNotAllowScript(WebFrame* frame) {
  content_settings_->DidNotAllowScript(frame);
}

void ChromeRenderViewObserver::OnSetIsPrerendering(bool is_prerendering) {
  if (is_prerendering) {
    DCHECK(!prerender::PrerenderHelper::Get(render_view()));
    // The PrerenderHelper will destroy itself either after recording histograms
    // or on destruction of the RenderView.
    new prerender::PrerenderHelper(render_view());
  }
}

void ChromeRenderViewObserver::DidStartLoading() {
  if ((render_view()->GetEnabledBindings() & content::BINDINGS_POLICY_WEB_UI) &&
      webui_javascript_.get()) {
    render_view()->EvaluateScript(webui_javascript_->frame_xpath,
                                  webui_javascript_->jscript,
                                  webui_javascript_->id,
                                  webui_javascript_->notify_result);
  }
}

void ChromeRenderViewObserver::DidStopLoading() {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ChromeRenderViewObserver::CapturePageInfo,
                 weak_factory_.GetWeakPtr(), render_view()->GetPageId(), false),
      render_view()->GetContentStateImmediately() ? 0 : kDelayForCaptureMs);

  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  GURL osd_url = main_frame->document().openSearchDescriptionURL();
  if (!osd_url.is_empty()) {
    Send(new ChromeViewHostMsg_PageHasOSDD(
        routing_id(), render_view()->GetPageId(), osd_url,
        search_provider::AUTODETECTED_PROVIDER));
  }

  int icon_types = WebIconURL::TypeFavicon;
  if (chrome::kEnableTouchIcon)
    icon_types |= WebIconURL::TypeTouchPrecomposed | WebIconURL::TypeTouch;

  WebVector<WebIconURL> icon_urls =
      render_view()->GetWebView()->mainFrame()->iconURLs(icon_types);
  std::vector<FaviconURL> urls;
  for (size_t i = 0; i < icon_urls.size(); i++) {
    WebURL url = icon_urls[i].iconURL();
    if (!url.isEmpty())
      urls.push_back(FaviconURL(url, ToFaviconType(icon_urls[i].iconType())));
  }
  if (!urls.empty()) {
    Send(new IconHostMsg_UpdateFaviconURL(
        routing_id(), render_view()->GetPageId(), urls));
  }
}

void ChromeRenderViewObserver::DidChangeIcon(WebFrame* frame,
                                             WebIconURL::Type icon_type) {
  if (frame->parent())
    return;

  if (!chrome::kEnableTouchIcon &&
      icon_type != WebIconURL::TypeFavicon)
    return;

  WebVector<WebIconURL> icon_urls = frame->iconURLs(icon_type);
  std::vector<FaviconURL> urls;
  for (size_t i = 0; i < icon_urls.size(); i++) {
    urls.push_back(FaviconURL(icon_urls[i].iconURL(),
                              ToFaviconType(icon_urls[i].iconType())));
  }
  Send(new IconHostMsg_UpdateFaviconURL(
      routing_id(), render_view()->GetPageId(), urls));
}

void ChromeRenderViewObserver::DidCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  if (!is_new_navigation)
    return;

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ChromeRenderViewObserver::CapturePageInfo,
                 weak_factory_.GetWeakPtr(), render_view()->GetPageId(), true),
      kDelayForForcedCaptureMs);
}

void ChromeRenderViewObserver::DidClearWindowObject(WebFrame* frame) {
  if (render_view()->GetEnabledBindings() &
          content::BINDINGS_POLICY_DOM_AUTOMATION) {
    BindDOMAutomationController(frame);
  }

  if (render_view()->GetEnabledBindings() &
          content::BINDINGS_POLICY_EXTERNAL_HOST) {
    GetExternalHostBindings()->BindToJavascript(frame, "externalHost");
  }
}

void ChromeRenderViewObserver::DidHandleTouchEvent(const WebTouchEvent& event) {
  // TODO(mazda): Consider using WebKit::WebInputEvent::GestureTap event when
  //              it's implemented. Only sends the message on touch end event
  //              for now.
  if (event.type != WebKit::WebInputEvent::TouchEnd)
    return;
  // Ignore the case of multiple touches
  if (event.touchesLength != 1)
    return;
  if (render_view()->GetWebView()->textInputType() ==
      WebKit::WebTextInputTypeNone) {
    return;
  }
  WebKit::WebNode node = render_view()->GetFocusedNode();
  if (node.isNull())
    return;
  WebKit::WebAccessibilityObject accessibility =
      render_view()->GetWebView()->accessibilityObject();
  if (accessibility.isNull())
    return;
  const WebKit::WebTouchPoint point = event.touches[0];
  accessibility = accessibility.hitTest(point.position);
  if (accessibility.isNull())
    return;
  if (accessibility.node() == node)
    render_view()->Send(new ChromeViewHostMsg_FocusedEditableNodeTouched(
    render_view()->GetRoutingId()));
}

void ChromeRenderViewObserver::CapturePageInfo(int load_id,
                                               bool preliminary_capture) {
  if (load_id != render_view()->GetPageId())
    return;  // This capture call is no longer relevant due to navigation.

  // Skip indexing if this is not a new load.  Note that the case where
  // load_id == last_indexed_page_id_ is more complicated, since we need to
  // reindex if the toplevel URL has changed (such as from a redirect), even
  // though this may not cause the page id to be incremented.
  if (load_id < last_indexed_page_id_)
    return;

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
  if (prerender::PrerenderHelper::IsPrerendering(render_view()))
    return;

  bool same_page_id = last_indexed_page_id_ == load_id;
  if (!preliminary_capture)
    last_indexed_page_id_ = load_id;

  // Get the URL for this page.
  GURL url(main_frame->document().url());
  if (url.is_empty()) {
    if (!preliminary_capture)
      last_indexed_url_ = GURL();
    return;
  }

  // If the page id is unchanged, check whether the URL (ignoring fragments)
  // has changed.  If so, we need to reindex.  Otherwise, assume this is a
  // reload, in-page navigation, or some other load type where we don't want to
  // reindex.  Note: subframe navigations after onload increment the page id,
  // so these will trigger a reindex.
  GURL stripped_url(StripRef(url));
  if (same_page_id && stripped_url == last_indexed_url_)
    return;

  if (!preliminary_capture)
    last_indexed_url_ = stripped_url;

  TRACE_EVENT0("renderer", "ChromeRenderViewObserver::CapturePageInfo");

  // Retrieve the frame's full text.
  string16 contents;
  CaptureText(main_frame, &contents);
  if (translate_helper_)
    translate_helper_->PageCaptured(contents);
  if (contents.size()) {
    // Send the text to the browser for indexing (the browser might decide not
    // to index, if the URL is HTTPS for instance) and language discovery.
    Send(new ChromeViewHostMsg_PageContents(routing_id(), url, load_id,
                                            contents));
  }

  // Generate the thumbnail here if the in-browser thumbnailing isn't
  // enabled. TODO(satorux): Remove this and related code once
  // crbug.com/65936 is complete.
  if (!switches::IsInBrowserThumbnailingEnabled()) {
    CaptureThumbnail();
  }

#if defined(ENABLE_SAFE_BROWSING)
  // Will swap out the string.
  if (phishing_classifier_)
    phishing_classifier_->PageCaptured(&contents, preliminary_capture);
#endif
}

void ChromeRenderViewObserver::CaptureText(WebFrame* frame,
                                           string16* contents) {
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
    size_t last_space_index = contents->find_last_of(kWhitespaceUTF16);
    if (last_space_index == std::wstring::npos)
      return;  // don't index if we got a huge block of text with no spaces
    contents->resize(last_space_index);
  }
}

void ChromeRenderViewObserver::CaptureThumbnail() {
  WebFrame* main_frame = render_view()->GetWebView()->mainFrame();
  if (!main_frame)
    return;

  // get the URL for this page
  GURL url(main_frame->document().url());
  if (url.is_empty())
    return;

  if (render_view()->GetSize().IsEmpty())
    return;  // Don't create an empty thumbnail!

  TRACE_EVENT0("renderer", "ChromeRenderViewObserver::CaptureThumbnail");

  ThumbnailScore score;
  SkBitmap thumbnail;
  if (!CaptureFrameThumbnail(render_view()->GetWebView(), kThumbnailWidth,
                             kThumbnailHeight, &thumbnail, &score))
    return;

  // send the thumbnail message to the browser process
  Send(new ChromeViewHostMsg_Thumbnail(routing_id(), url, score, thumbnail));
}

bool ChromeRenderViewObserver::CaptureFrameThumbnail(WebView* view,
                                                     int w,
                                                     int h,
                                                     SkBitmap* thumbnail,
                                                     ThumbnailScore* score) {
  base::TimeTicks beginning_time = base::TimeTicks::Now();

  skia::PlatformCanvas canvas;

  {
    TRACE_EVENT0("renderer",
        "ChromeRenderViewObserver::CaptureFrameThumbnail::PaintViewIntoCanvas");
    // Paint |view| into |canvas|.
    if (!PaintViewIntoCanvas(view, canvas))
      return false;
  }

  SkDevice* device = skia::GetTopDevice(canvas);

  const SkBitmap& src_bmp = device->accessBitmap(false);
  // Cut off the vertical scrollbars (if any).
  int src_bmp_width = view->mainFrame()->contentsSize().width;

  SkRect dest_rect = { 0, 0, SkIntToScalar(w), SkIntToScalar(h) };
  float dest_aspect = dest_rect.width() / dest_rect.height();

  // Get the src rect so that we can preserve the aspect ratio while filling
  // the destination.
  SkIRect src_rect;
  if (src_bmp_width < dest_rect.width() ||
      src_bmp.height() < dest_rect.height()) {
    // Source image is smaller: we clip the part of source image within the
    // dest rect, and then stretch it to fill the dest rect. We don't respect
    // the aspect ratio in this case.
    src_rect.set(0, 0, static_cast<S16CPU>(dest_rect.width()),
                 static_cast<S16CPU>(dest_rect.height()));
    score->good_clipping = false;
  } else {
    float src_aspect = static_cast<float>(src_bmp_width) / src_bmp.height();
    if (src_aspect > dest_aspect) {
      // Wider than tall, clip horizontally: we center the smaller thumbnail in
      // the wider screen.
      S16CPU new_width = static_cast<S16CPU>(src_bmp.height() * dest_aspect);
      S16CPU x_offset = (src_bmp_width - new_width) / 2;
      src_rect.set(x_offset, 0, new_width + x_offset, src_bmp.height());
      score->good_clipping = false;
    } else {
      src_rect.set(0, 0, src_bmp_width,
                   static_cast<S16CPU>(src_bmp_width / dest_aspect));
      score->good_clipping = true;
    }
  }

  score->at_top = (view->mainFrame()->scrollOffset().height == 0);

  SkBitmap subset;
  device->accessBitmap(false).extractSubset(&subset, src_rect);

  TRACE_EVENT_BEGIN0("renderer",
        "ChromeRenderViewObserver::CaptureFrameThumbnail::DownsampleByTwo");
  // First do a fast downsample by powers of two to get close to the final size.
  SkBitmap downsampled_subset =
      SkBitmapOperations::DownsampleByTwoUntilSize(subset, w, h);
  TRACE_EVENT_END0("renderer",
        "ChromeRenderViewObserver::CaptureFrameThumbnail::DownsampleByTwo");

  {
    TRACE_EVENT0("renderer",
        "ChromeRenderViewObserver::CaptureFrameThumbnail::DownsampleLanczos3");
    // Do a high-quality resize from the downscaled size to the final size.
    *thumbnail = skia::ImageOperations::Resize(
        downsampled_subset, skia::ImageOperations::RESIZE_LANCZOS3, w, h);
  }

  score->boring_score = CalculateBoringScore(thumbnail);

  HISTOGRAM_TIMES("Renderer4.Thumbnail",
                  base::TimeTicks::Now() - beginning_time);

  return true;
}

bool ChromeRenderViewObserver::CaptureSnapshot(WebView* view,
                                               SkBitmap* snapshot) {
  base::TimeTicks beginning_time = base::TimeTicks::Now();

  skia::PlatformCanvas canvas;
  if (!PaintViewIntoCanvas(view, canvas))
    return false;

  SkDevice* device = skia::GetTopDevice(canvas);

  const SkBitmap& bitmap = device->accessBitmap(false);
  if (!bitmap.copyTo(snapshot, SkBitmap::kARGB_8888_Config))
    return false;

  HISTOGRAM_TIMES("Renderer4.Snapshot",
                  base::TimeTicks::Now() - beginning_time);
  return true;
}

void ChromeRenderViewObserver::BindDOMAutomationController(WebFrame* frame) {
  if (!dom_automation_controller_.get()) {
    dom_automation_controller_.reset(new DomAutomationController());
  }
  dom_automation_controller_->set_message_sender(this);
  dom_automation_controller_->set_routing_id(routing_id());
  dom_automation_controller_->BindToJavascript(frame,
                                               "domAutomationController");
}

ExternalHostBindings* ChromeRenderViewObserver::GetExternalHostBindings() {
  if (!external_host_bindings_.get()) {
    external_host_bindings_.reset(new ExternalHostBindings(
        render_view(), routing_id()));
  }
  return external_host_bindings_.get();
}

bool ChromeRenderViewObserver::DownloadFavicon(int id,
                                               const GURL& image_url,
                                               int image_size) {
  // Make sure webview was not shut down.
  if (!render_view()->GetWebView())
    return false;
  // Create an image resource fetcher and assign it with a call back object.
  image_fetchers_.push_back(linked_ptr<ImageResourceFetcher>(
      new ImageResourceFetcher(
          image_url, render_view()->GetWebView()->mainFrame(), id, image_size,
          WebURLRequest::TargetIsFavicon,
          base::Bind(&ChromeRenderViewObserver::DidDownloadFavicon,
                     base::Unretained(this)))));
  return true;
}

void ChromeRenderViewObserver::DidDownloadFavicon(
    ImageResourceFetcher* fetcher, const SkBitmap& image) {
  // Notify requester of image download status.
  Send(new IconHostMsg_DidDownloadFavicon(routing_id(),
                                          fetcher->id(),
                                          fetcher->image_url(),
                                          image.isNull(),
                                          image));

  // Remove the image fetcher from our pending list. We're in the callback from
  // ImageResourceFetcher, best to delay deletion.
  ImageResourceFetcherList::iterator iter;
  for (iter = image_fetchers_.begin(); iter != image_fetchers_.end(); ++iter) {
    if (iter->get() == fetcher) {
      iter->release();
      image_fetchers_.erase(iter);
      break;
    }
  }
  MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);
}

SkBitmap ChromeRenderViewObserver::ImageFromDataUrl(const GURL& url) const {
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

bool ChromeRenderViewObserver::IsStrictSecurityHost(const std::string& host) {
  return (strict_security_hosts_.find(host) != strict_security_hosts_.end());
}
