// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_view_observer.h"

#include "base/command_line.h"
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
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/external_host_bindings.h"
#include "chrome/renderer/frame_sniffer.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/translate_helper.h"
#include "content/common/bindings_policy.h"
#include "content/common/view_messages.h"
#include "content/renderer/content_renderer_client.h"
#include "net/base/data_url.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "webkit/glue/image_decoder.h"
#include "webkit/glue/image_resource_fetcher.h"
#include "webkit/glue/webkit_glue.h"
#include "v8/include/v8-testing.h"

using WebKit::WebCString;
using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebIconURL;
using WebKit::WebRect;
using WebKit::WebSecurityOrigin;
using WebKit::WebSize;
using WebKit::WebString;
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
static const char kSSLInsecureContent[] = "SSL.InsecureContent";
static const char kDotGoogleDotCom[] = ".google.com";
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

namespace {
GURL StripRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}
}  // namespace

ChromeRenderViewObserver::ChromeRenderViewObserver(
    RenderView* render_view,
    ContentSettingsObserver* content_settings,
    ExtensionDispatcher* extension_dispatcher,
    TranslateHelper* translate_helper)
    : RenderViewObserver(render_view),
      content_settings_(content_settings),
      extension_dispatcher_(extension_dispatcher),
      translate_helper_(translate_helper),
      phishing_classifier_(NULL),
      last_indexed_page_id_(-1),
      allow_displaying_insecure_content_(false),
      allow_running_insecure_content_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(page_info_method_factory_(this)) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDomAutomationController)) {
    int old_bindings = render_view->enabled_bindings();
    render_view->set_enabled_bindings(
        old_bindings |= BindingsPolicy::DOM_AUTOMATION);
  }
  render_view->webview()->setPermissionClient(this);
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
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_StartFrameSniffer, OnStartFrameSniffer)
#endif
    IPC_MESSAGE_HANDLER(ChromeViewMsg_GetFPS, OnGetFPS)
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

  WebFrame* main_frame = render_view()->webview()->mainFrame();
  if (!main_frame)
    error = true;

  if (!error && !CaptureSnapshot(render_view()->webview(), &snapshot))
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
  WebFrame* main_frame = render_view()->webview()->mainFrame();
  if (main_frame)
    main_frame->reload();
}

void ChromeRenderViewObserver::OnSetAllowRunningInsecureContent(bool allow) {
  allow_running_insecure_content_ = allow;
  OnSetAllowDisplayingInsecureContent(allow);
}

void ChromeRenderViewObserver::Navigate(const GURL& url) {
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

void ChromeRenderViewObserver::OnStartFrameSniffer(const string16& frame_name) {
  new FrameSniffer(render_view(), frame_name);
}

void ChromeRenderViewObserver::OnGetFPS() {
  float fps = (render_view()->filtered_time_per_frame() > 0.0f)?
      1.0f / render_view()->filtered_time_per_frame() : 0.0f;
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
  Send(new ChromeViewHostMsg_CanTriggerClipboardRead(
      routing_id(), frame->document().url(), &allowed));
  return allowed;
}

bool ChromeRenderViewObserver::allowWriteToClipboard(WebFrame* frame,
                                                    bool default_value) {
  bool allowed = false;
  Send(new ChromeViewHostMsg_CanTriggerClipboardWrite(
      routing_id(), frame->document().url(), &allowed));
  return allowed;
}

bool ChromeRenderViewObserver::allowDisplayingInsecureContent(
    WebKit::WebFrame* frame,
    bool allowed_per_settings,
    const WebKit::WebSecurityOrigin& origin,
    const WebKit::WebURL& url) {
  UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                            INSECURE_CONTENT_DISPLAY,
                            INSECURE_CONTENT_NUM_EVENTS);
  std::string host(origin.host().utf8());
  GURL frame_url(frame->document().url());
  if (EndsWith(host, kDotGoogleDotCom, false)) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
    if (StartsWithASCII(frame_url.path(), kGoogleSupportPathPrefix, false)) {
      UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                                INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_SUPPORT,
                                INSECURE_CONTENT_NUM_EVENTS);
    } else if (StartsWithASCII(frame_url.path(),
                               kGoogleIntlPathPrefix,
                               false)) {
      UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                                INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_INTL,
                                INSECURE_CONTENT_NUM_EVENTS);
    }
  }
  if (host == kWWWDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_WWW_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
    if (StartsWithASCII(frame_url.path(), kGoogleReaderPathPrefix, false)) {
      UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                                INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_READER,
                                INSECURE_CONTENT_NUM_EVENTS);
    }
  } else if (host == kMailDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_MAIL_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kPlusDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_PLUS_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kDocsDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_DOCS_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kSitesDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_SITES_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kPicasawebDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_PICASAWEB_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kCodeDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_CODE_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kGroupsDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_GROUPS_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kMapsDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_MAPS_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kWWWDotYoutubeDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HOST_YOUTUBE,
                              INSECURE_CONTENT_NUM_EVENTS);
  }
  GURL gurl(url);
  if (EndsWith(gurl.path(), kDotHTML, false)) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_DISPLAY_HTML,
                              INSECURE_CONTENT_NUM_EVENTS);
  }

  if (allowed_per_settings || allow_displaying_insecure_content_)
    return true;

  Send(new ChromeViewHostMsg_DidBlockDisplayingInsecureContent(routing_id()));
  return false;
}

bool ChromeRenderViewObserver::allowRunningInsecureContent(
    WebKit::WebFrame* frame,
    bool allowed_per_settings,
    const WebKit::WebSecurityOrigin& origin,
    const WebKit::WebURL& url) {
  UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                            INSECURE_CONTENT_RUN,
                            INSECURE_CONTENT_NUM_EVENTS);
  std::string host(origin.host().utf8());
  GURL frame_url(frame->document().url());
  if (EndsWith(host, kDotGoogleDotCom, false)) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
    if (StartsWithASCII(frame_url.path(), kGoogleSupportPathPrefix, false)) {
      UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                                INSECURE_CONTENT_RUN_HOST_GOOGLE_SUPPORT,
                                INSECURE_CONTENT_NUM_EVENTS);
    } else if (StartsWithASCII(frame_url.path(),
                               kGoogleIntlPathPrefix,
                               false)) {
      UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                                INSECURE_CONTENT_RUN_HOST_GOOGLE_INTL,
                                INSECURE_CONTENT_NUM_EVENTS);
    }
  }
  if (host == kWWWDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_WWW_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
    if (StartsWithASCII(frame_url.path(), kGoogleReaderPathPrefix, false)) {
      UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                                INSECURE_CONTENT_RUN_HOST_GOOGLE_READER,
                                INSECURE_CONTENT_NUM_EVENTS);
    }
  } else if (host == kMailDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_MAIL_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kPlusDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_PLUS_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kDocsDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_DOCS_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kSitesDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_SITES_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kPicasawebDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_PICASAWEB_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kCodeDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_CODE_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kGroupsDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_GROUPS_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kMapsDotGoogleDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_MAPS_GOOGLE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (host == kWWWDotYoutubeDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_YOUTUBE,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (EndsWith(host, kDotGoogleUserContentDotCom, false)) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_HOST_GOOGLEUSERCONTENT,
                              INSECURE_CONTENT_NUM_EVENTS);
  }
  GURL gurl(url);
  if (gurl.host() == kWWWDotYoutubeDotCom) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_TARGET_YOUTUBE,
                              INSECURE_CONTENT_NUM_EVENTS);
  }
  if (EndsWith(gurl.path(), kDotJS, false)) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_JS,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (EndsWith(gurl.path(), kDotCSS, false)) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_CSS,
                              INSECURE_CONTENT_NUM_EVENTS);
  } else if (EndsWith(gurl.path(), kDotSWF, false)) {
    UMA_HISTOGRAM_ENUMERATION(kSSLInsecureContent,
                              INSECURE_CONTENT_RUN_SWF,
                              INSECURE_CONTENT_NUM_EVENTS);
  }

  if (allowed_per_settings || allow_running_insecure_content_)
    return true;

  Send(new ChromeViewHostMsg_DidBlockRunningInsecureContent(routing_id()));
  return false;
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
  if (BindingsPolicy::is_web_ui_enabled(render_view()->enabled_bindings()) &&
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
      page_info_method_factory_.NewRunnableMethod(
          &ChromeRenderViewObserver::CapturePageInfo, render_view()->page_id(),
          false),
      render_view()->content_state_immediately() ? 0 : kDelayForCaptureMs);

  WebFrame* main_frame = render_view()->webview()->mainFrame();
  GURL osd_url = main_frame->document().openSearchDescriptionURL();
  if (!osd_url.is_empty()) {
    Send(new ChromeViewHostMsg_PageHasOSDD(
        routing_id(), render_view()->page_id(), osd_url,
        search_provider::AUTODETECTED_PROVIDER));
  }

  int icon_types = WebIconURL::TypeFavicon;
  if (chrome::kEnableTouchIcon)
    icon_types |= WebIconURL::TypeTouchPrecomposed | WebIconURL::TypeTouch;

  WebVector<WebIconURL> icon_urls =
      render_view()->webview()->mainFrame()->iconURLs(icon_types);
  std::vector<FaviconURL> urls;
  for (size_t i = 0; i < icon_urls.size(); i++) {
    WebURL url = icon_urls[i].iconURL();
    if (!url.isEmpty())
      urls.push_back(FaviconURL(url, ToFaviconType(icon_urls[i].iconType())));
  }
  if (!urls.empty()) {
    Send(new IconHostMsg_UpdateFaviconURL(
        routing_id(), render_view()->page_id(), urls));
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
      routing_id(), render_view()->page_id(), urls));
}

void ChromeRenderViewObserver::DidCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  if (!is_new_navigation)
    return;

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      page_info_method_factory_.NewRunnableMethod(
          &ChromeRenderViewObserver::CapturePageInfo, render_view()->page_id(),
          true),
      kDelayForForcedCaptureMs);
}

void ChromeRenderViewObserver::DidClearWindowObject(WebFrame* frame) {
  if (BindingsPolicy::is_dom_automation_enabled(
          render_view()->enabled_bindings())) {
    BindDOMAutomationController(frame);
  }

  if (BindingsPolicy::is_external_host_enabled(
          render_view()->enabled_bindings())) {
    GetExternalHostBindings()->set_message_sender(render_view());
    GetExternalHostBindings()->set_routing_id(routing_id());
    GetExternalHostBindings()->BindToJavascript(frame, "externalHost");
  }
}

void ChromeRenderViewObserver::CapturePageInfo(int load_id,
                                               bool preliminary_capture) {
  if (load_id != render_view()->page_id())
    return;  // This capture call is no longer relevant due to navigation.

  // Skip indexing if this is not a new load.  Note that the case where
  // load_id == last_indexed_page_id_ is more complicated, since we need to
  // reindex if the toplevel URL has changed (such as from a redirect), even
  // though this may not cause the page id to be incremented.
  if (load_id < last_indexed_page_id_)
    return;

  if (!render_view()->webview())
    return;

  WebFrame* main_frame = render_view()->webview()->mainFrame();
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
  WebFrame* main_frame = render_view()->webview()->mainFrame();
  if (!main_frame)
    return;

  // get the URL for this page
  GURL url(main_frame->document().url());
  if (url.is_empty())
    return;

  if (render_view()->size().IsEmpty())
    return;  // Don't create an empty thumbnail!

  ThumbnailScore score;
  SkBitmap thumbnail;
  if (!CaptureFrameThumbnail(render_view()->webview(), kThumbnailWidth,
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

  // Paint |view| into |canvas|.
  if (!PaintViewIntoCanvas(view, canvas))
    return false;

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

  // First do a fast downsample by powers of two to get close to the final size.
  SkBitmap downsampled_subset =
      SkBitmapOperations::DownsampleByTwoUntilSize(subset, w, h);

  // Do a high-quality resize from the downscaled size to the final size.
  *thumbnail = skia::ImageOperations::Resize(
      downsampled_subset, skia::ImageOperations::RESIZE_LANCZOS3, w, h);

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
  if (!external_host_bindings_.get())
    external_host_bindings_.reset(new ExternalHostBindings());
  return external_host_bindings_.get();
}

bool ChromeRenderViewObserver::DownloadFavicon(int id,
                                               const GURL& image_url,
                                               int image_size) {
  // Make sure webview was not shut down.
  if (!render_view()->webview())
    return false;
  // Create an image resource fetcher and assign it with a call back object.
  image_fetchers_.push_back(linked_ptr<ImageResourceFetcher>(
      new ImageResourceFetcher(
          image_url, render_view()->webview()->mainFrame(), id, image_size,
          WebURLRequest::TargetIsFavicon,
          NewCallback(this, &ChromeRenderViewObserver::DidDownloadFavicon))));
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
  RenderView::ImageResourceFetcherList::iterator iter;
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
