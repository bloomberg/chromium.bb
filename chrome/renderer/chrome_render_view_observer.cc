// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_view_observer.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/icon_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/about_handler.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/automation/dom_automation_controller.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/external_host_bindings.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/translate_helper.h"
#include "content/common/bindings_policy.h"
#include "content/common/view_messages.h"
#include "content/renderer/content_renderer_client.h"
#include "googleurl/src/gurl.h"
#include "net/base/data_url.h"
#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageSerializer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/skbitmap_operations.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/image_decoder.h"
#include "webkit/glue/image_resource_fetcher.h"
#include "webkit/glue/webkit_glue.h"
#include "v8/include/v8-testing.h"

using WebKit::WebCString;
using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebIconURL;
using WebKit::WebPageSerializer;
using WebKit::WebPageSerializerClient;
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

ChromeRenderViewObserver::ChromeRenderViewObserver(
    RenderView* render_view,
    ContentSettingsObserver* content_settings,
    ExtensionDispatcher* extension_dispatcher,
    TranslateHelper* translate_helper,
    safe_browsing::PhishingClassifierDelegate* phishing_classifier)
    : RenderViewObserver(render_view),
      content_settings_(content_settings),
      extension_dispatcher_(extension_dispatcher),
      translate_helper_(translate_helper),
      phishing_classifier_(phishing_classifier),
      last_indexed_page_id_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(page_info_method_factory_(this)) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDomAutomationController)) {
    int old_bindings = render_view->enabled_bindings();
    render_view->set_enabled_bindings(
        old_bindings |= BindingsPolicy::DOM_AUTOMATION);
  }
  render_view->webview()->setPermissionClient(this);
}

ChromeRenderViewObserver::~ChromeRenderViewObserver() {
}

bool ChromeRenderViewObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderViewObserver, message)
    IPC_MESSAGE_HANDLER(ViewMsg_CaptureSnapshot, OnCaptureSnapshot)
    IPC_MESSAGE_HANDLER(ViewMsg_HandleMessageFromExternalHost,
                        OnHandleMessageFromExternalHost)
    IPC_MESSAGE_HANDLER(ViewMsg_JavaScriptStressTestControl,
                        OnJavaScriptStressTestControl)
    IPC_MESSAGE_HANDLER(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                        OnGetAllSavableResourceLinksForCurrentPage)
    IPC_MESSAGE_HANDLER(
        ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
        OnGetSerializedHtmlDataForCurrentPageWithLocalLinks)
    IPC_MESSAGE_HANDLER(IconMsg_DownloadFavicon, OnDownloadFavicon)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableViewSourceMode, OnEnableViewSourceMode)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Filter only.
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderViewObserver, message)
    IPC_MESSAGE_HANDLER(ViewMsg_Navigate, OnNavigate)
    IPC_MESSAGE_HANDLER(ViewMsg_SetIsPrerendering, OnSetIsPrerendering);
  IPC_END_MESSAGE_MAP()

  return handled;
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
  Send(new ViewHostMsg_Snapshot(routing_id(), snapshot));
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

void ChromeRenderViewObserver::OnGetAllSavableResourceLinksForCurrentPage(
    const GURL& page_url) {
  // Prepare list to storage all savable resource links.
  std::vector<GURL> resources_list;
  std::vector<GURL> referrers_list;
  std::vector<GURL> frames_list;
  webkit_glue::SavableResourcesResult result(&resources_list,
                                             &referrers_list,
                                             &frames_list);

  if (!webkit_glue::GetAllSavableResourceLinksForCurrentPage(
          render_view()->webview(),
          page_url,
          &result,
          chrome::kSavableSchemes)) {
    // If something is wrong when collecting all savable resource links,
    // send empty list to embedder(browser) to tell it failed.
    referrers_list.clear();
    resources_list.clear();
    frames_list.clear();
  }

  // Send result of all savable resource links to embedder.
  Send(new ViewHostMsg_SendCurrentPageAllSavableResourceLinks(routing_id(),
                                                              resources_list,
                                                              referrers_list,
                                                              frames_list));
}

void
ChromeRenderViewObserver::OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
    const std::vector<GURL>& links,
    const std::vector<FilePath>& local_paths,
    const FilePath& local_directory_name) {

  // Convert std::vector of GURLs to WebVector<WebURL>
  WebVector<WebURL> weburl_links(links);

  // Convert std::vector of std::strings to WebVector<WebString>
  WebVector<WebString> webstring_paths(local_paths.size());
  for (size_t i = 0; i < local_paths.size(); i++)
    webstring_paths[i] = webkit_glue::FilePathToWebString(local_paths[i]);

  WebPageSerializer::serialize(render_view()->webview()->mainFrame(),
                               true, this, weburl_links, webstring_paths,
                               webkit_glue::FilePathToWebString(
                                   local_directory_name));
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

void ChromeRenderViewObserver::OnEnableViewSourceMode() {
  if (!render_view()->webview())
    return;
  WebFrame* main_frame = render_view()->webview()->mainFrame();
  if (!main_frame)
    return;

  main_frame->enableViewSourceMode(true);
}

void ChromeRenderViewObserver::didSerializeDataForFrame(
    const WebURL& frame_url,
    const WebCString& data,
    WebPageSerializerClient::PageSerializationStatus status) {
  Send(new ViewHostMsg_SendSerializedHtmlData(
    routing_id(),
    frame_url,
    data.data(),
    static_cast<int32>(status)));
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

bool ChromeRenderViewObserver::allowImages(WebFrame* frame,
                                          bool enabled_per_settings) {
  return content_settings_->AllowImages(frame, enabled_per_settings);
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
  Send(new ViewHostMsg_CanTriggerClipboardRead(
      routing_id(), frame->url(), &allowed));
  return allowed;
}

bool ChromeRenderViewObserver::allowWriteToClipboard(WebFrame* frame,
                                                    bool default_value) {
  bool allowed = false;
  Send(new ViewHostMsg_CanTriggerClipboardWrite(
      routing_id(), frame->url(), &allowed));
  return allowed;
}

void ChromeRenderViewObserver::didNotAllowPlugins(WebFrame* frame) {
  content_settings_->DidNotAllowPlugins(frame);
}

void ChromeRenderViewObserver::didNotAllowScript(WebFrame* frame) {
  content_settings_->DidNotAllowScript(frame);
}

void ChromeRenderViewObserver::OnNavigate(
    const ViewMsg_Navigate_Params& params) {
  AboutHandler::MaybeHandle(params.url);
}

void ChromeRenderViewObserver::OnSetIsPrerendering(bool is_prerendering) {
  if (is_prerendering) {
    DCHECK(!prerender::PrerenderHelper::Get(render_view()));
    // The PrerenderHelper will destroy itself either after recording histograms
    // or on destruction of the RenderView.
    new prerender::PrerenderHelper(render_view());
  }
}

void ChromeRenderViewObserver::DidStopLoading() {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      page_info_method_factory_.NewRunnableMethod(
          &ChromeRenderViewObserver::CapturePageInfo, render_view()->page_id(),
          false),
      render_view()->content_state_immediately() ? 0 : kDelayForCaptureMs);

  GURL osd_url =
      render_view()->webview()->mainFrame()->openSearchDescriptionURL();
  if (!osd_url.is_empty()) {
    Send(new ViewHostMsg_PageHasOSDD(
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

  if (load_id == last_indexed_page_id_)
    return;  // we already indexed this page

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

  if (!preliminary_capture)
    last_indexed_page_id_ = load_id;

  // Get the URL for this page.
  GURL url(main_frame->url());
  if (url.is_empty())
    return;

  // Retrieve the frame's full text.
  string16 contents;
  CaptureText(main_frame, &contents);
  if (contents.size()) {
    if (translate_helper_)
      translate_helper_->PageCaptured(contents);
    // Send the text to the browser for indexing (the browser might decide not
    // to index, if the URL is HTTPS for instance) and language discovery.
    Send(new ViewHostMsg_PageContents(routing_id(), url, load_id, contents));
  }

  // Generate the thumbnail here if the in-browser thumbnailing isn't
  // enabled. TODO(satorux): Remove this and related code once
  // crbug.com/65936 is complete.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableInBrowserThumbnailing)) {
    CaptureThumbnail();
  }

  // Will swap out the string.
  if (phishing_classifier_)
    phishing_classifier_->PageCaptured(&contents, preliminary_capture);
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
  GURL url(main_frame->url());
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
  Send(new ViewHostMsg_Thumbnail(routing_id(), url, score, thumbnail));
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

  skia::BitmapPlatformDevice& device =
      static_cast<skia::BitmapPlatformDevice&>(canvas.getTopPlatformDevice());

  const SkBitmap& src_bmp = device.accessBitmap(false);

  SkRect dest_rect = { 0, 0, SkIntToScalar(w), SkIntToScalar(h) };
  float dest_aspect = dest_rect.width() / dest_rect.height();

  // Get the src rect so that we can preserve the aspect ratio while filling
  // the destination.
  SkIRect src_rect;
  if (src_bmp.width() < dest_rect.width() ||
      src_bmp.height() < dest_rect.height()) {
    // Source image is smaller: we clip the part of source image within the
    // dest rect, and then stretch it to fill the dest rect. We don't respect
    // the aspect ratio in this case.
    src_rect.set(0, 0, static_cast<S16CPU>(dest_rect.width()),
                 static_cast<S16CPU>(dest_rect.height()));
    score->good_clipping = false;
  } else {
    float src_aspect = static_cast<float>(src_bmp.width()) / src_bmp.height();
    if (src_aspect > dest_aspect) {
      // Wider than tall, clip horizontally: we center the smaller thumbnail in
      // the wider screen.
      S16CPU new_width = static_cast<S16CPU>(src_bmp.height() * dest_aspect);
      S16CPU x_offset = (src_bmp.width() - new_width) / 2;
      src_rect.set(x_offset, 0, new_width + x_offset, src_bmp.height());
      score->good_clipping = false;
    } else {
      src_rect.set(0, 0, src_bmp.width(),
                   static_cast<S16CPU>(src_bmp.width() / dest_aspect));
      score->good_clipping = true;
    }
  }

  score->at_top = (view->mainFrame()->scrollOffset().height == 0);

  SkBitmap subset;
  device.accessBitmap(false).extractSubset(&subset, src_rect);

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

  skia::BitmapPlatformDevice& device =
      static_cast<skia::BitmapPlatformDevice&>(canvas.getTopPlatformDevice());

  const SkBitmap& bitmap = device.accessBitmap(false);
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
    webkit_glue::ImageDecoder decoder(gfx::Size(kFaviconSize, kFaviconSize));
    const unsigned char* src_data =
        reinterpret_cast<const unsigned char*>(&data[0]);

    return decoder.Decode(src_data, data.size());
  }
  return SkBitmap();
}
