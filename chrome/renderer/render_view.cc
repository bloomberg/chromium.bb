// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_view.h"

#include <algorithm>
#include <string>
#include <vector>

#include "app/message_box_flags.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/field_trial.h"
#include "base/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/appcache/appcache_dispatcher.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_zoom.h"
#include "chrome/common/pepper_plugin_registry.h"
#include "chrome/common/plugin_group.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/renderer_preferences.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/window_container_type.h"
#include "chrome/renderer/about_handler.h"
#include "chrome/renderer/audio_message_filter.h"
#include "chrome/renderer/autofill_helper.h"
#include "chrome/renderer/blocked_plugin.h"
#include "chrome/renderer/device_orientation_dispatcher.h"
#include "chrome/renderer/devtools_agent.h"
#include "chrome/renderer/devtools_client.h"
#include "chrome/renderer/extension_groups.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/extension_renderer_info.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/geolocation_dispatcher.h"
#include "chrome/renderer/localized_error.h"
#include "chrome/renderer/media/audio_renderer_impl.h"
#include "chrome/renderer/media/ipc_video_decoder.h"
#include "chrome/renderer/media/ipc_video_renderer.h"
#include "chrome/renderer/navigation_state.h"
#include "chrome/renderer/notification_provider.h"
#include "chrome/renderer/page_click_tracker.h"
#include "chrome/renderer/password_autocomplete_manager.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "chrome/renderer/print_web_view_helper.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view_visitor.h"
#include "chrome/renderer/render_widget_fullscreen.h"
#include "chrome/renderer/renderer_webapplicationcachehost_impl.h"
#include "chrome/renderer/renderer_webstoragenamespace_impl.h"
#include "chrome/renderer/speech_input_dispatcher.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "chrome/renderer/user_script_slave.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "chrome/renderer/webgles2context_impl.h"
#include "chrome/renderer/webplugin_delegate_pepper.h"
#include "chrome/renderer/webplugin_delegate_proxy.h"
#include "chrome/renderer/websharedworker_proxy.h"
#include "chrome/renderer/webworker_proxy.h"
#include "gfx/color_utils.h"
#include "gfx/favicon_size.h"
#include "gfx/native_widget_types.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "media/base/media_switches.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/image_operations.h"
#include "third_party/cld/encodings/compact_lang_det/win/cld_unicodetext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/WebKit/chromium/public/WebAccessibilityCache.h"
#include "third_party/WebKit/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileChooserParams.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFindOptions.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebImage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNodeList.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPageSerializer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPlugin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSearchableFormData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSettings.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/WebKit/chromium/public/WebWindowFeatures.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/image_decoder.h"
#include "webkit/glue/image_resource_fetcher.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "webkit/glue/plugins/default_plugin_shared.h"
#include "webkit/glue/plugins/pepper_webplugin_impl.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin_delegate.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/plugins/webplugin_impl.h"
#include "webkit/glue/plugins/webview_plugin.h"
#include "webkit/glue/site_isolation_metrics.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webmediaplayer_impl.h"

#if defined(OS_WIN)
// TODO(port): these files are currently Windows only because they concern:
//   * theming
#include "gfx/native_theme_win.h"
#elif defined(USE_X11)
#include "third_party/WebKit/WebKit/chromium/public/linux/WebRenderTheme.h"
#elif defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif

using appcache::WebApplicationCacheHostImpl;
using base::Time;
using base::TimeDelta;
using webkit_glue::AltErrorPageResourceFetcher;
using webkit_glue::FormData;
using webkit_glue::FormField;
using webkit_glue::ImageResourceFetcher;
using webkit_glue::PasswordForm;
using webkit_glue::PasswordFormDomManager;
using webkit_glue::SiteIsolationMetrics;
using webkit_glue::WebAccessibility;
using WebKit::WebAccessibilityCache;
using WebKit::WebAccessibilityObject;
using WebKit::WebApplicationCacheHost;
using WebKit::WebApplicationCacheHostClient;
using WebKit::WebColor;
using WebKit::WebColorName;
using WebKit::WebConsoleMessage;
using WebKit::WebContextMenuData;
using WebKit::WebCookieJar;
using WebKit::WebCString;
using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebDevToolsAgent;
using WebKit::WebDocument;
using WebKit::WebDragData;
using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using WebKit::WebEditingAction;
using WebKit::WebFileChooserCompletion;
using WebKit::WebFileSystem;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFindOptions;
using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebHistoryItem;
using WebKit::WebImage;
using WebKit::WebInputElement;
using WebKit::WebMediaPlayer;
using WebKit::WebMediaPlayerAction;
using WebKit::WebMediaPlayerClient;
using WebKit::WebNavigationPolicy;
using WebKit::WebNavigationType;
using WebKit::WebNode;
using WebKit::WebPageSerializer;
using WebKit::WebPageSerializerClient;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPluginDocument;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRange;
using WebKit::WebRect;
using WebKit::WebScriptSource;
using WebKit::WebSearchableFormData;
using WebKit::WebSecurityOrigin;
using WebKit::WebSettings;
using WebKit::WebSharedWorker;
using WebKit::WebSize;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebTextAffinity;
using WebKit::WebTextDirection;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebVector;
using WebKit::WebView;
using WebKit::WebWidget;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;
using WebKit::WebWindowFeatures;

//-----------------------------------------------------------------------------

typedef std::map<WebKit::WebView*, RenderView*> ViewMap;

// define to write the time necessary for thumbnail/DOM text retrieval,
// respectively, into the system debug log
// #define TIME_TEXT_RETRIEVAL

// maximum number of characters in the document to index, any text beyond this
// point will be clipped
static const size_t kMaxIndexChars = 65535;

// Size of the thumbnails that we'll generate
static const int kThumbnailWidth = 212;
static const int kThumbnailHeight = 132;

// Delay in milliseconds that we'll wait before capturing the page contents
// and thumbnail.
static const int kDelayForCaptureMs = 500;

// Typically, we capture the page data once the page is loaded.
// Sometimes, the page never finishes to load, preventing the page capture
// To workaround this problem, we always perform a capture after the following
// delay.
static const int kDelayForForcedCaptureMs = 6000;

// Time, in seconds, we delay before sending content state changes (such as form
// state and scroll position) to the browser. We delay sending changes to avoid
// spamming the browser.
// To avoid having tab/session restore require sending a message to get the
// current content state during tab closing we use a shorter timeout for the
// foreground renderer. This means there is a small window of time from which
// content state is modified and not sent to session restore, but this is
// better than having to wake up all renderers during shutdown.
static const int kDelaySecondsForContentStateSyncHidden = 5;
static const int kDelaySecondsForContentStateSync = 1;

// The maximum number of popups that can be spawned from one page.
static const int kMaximumNumberOfUnacknowledgedPopups = 25;

static const char* const kUnreachableWebDataURL =
    "chrome://chromewebdata/";

static const char* const kBackForwardNavigationScheme = "history";

static void GetRedirectChain(WebDataSource* ds, std::vector<GURL>* result) {
  WebVector<WebURL> urls;
  ds->redirectChain(urls);
  result->reserve(urls.size());
  for (size_t i = 0; i < urls.size(); ++i)
    result->push_back(urls[i]);
}

static bool UrlMatchesPermissions(
    const GURL& url, const std::vector<URLPattern>& host_permissions) {
  for (size_t i = 0; i < host_permissions.size(); ++i) {
    if (host_permissions[i].MatchesUrl(url))
      return true;
  }

  return false;
}

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

// True if |frame| contains content that is white-listed for content settings.
static bool IsWhitelistedForContentSettings(WebFrame* frame) {
  WebSecurityOrigin origin = frame->securityOrigin();
  if (origin.isEmpty())
    return false;  // Uninitialized document?

  if (EqualsASCII(origin.protocol(), chrome::kChromeUIScheme))
    return true;  // Browser UI elements should still work.

  // If the scheme is ftp: or file:, an empty file name indicates a directory
  // listing, which requires JavaScript to function properly.
  GURL frame_url = frame->url();
  const char* kDirProtocols[] = { "ftp", "file" };
  for (size_t i = 0; i < arraysize(kDirProtocols); ++i) {
    if (EqualsASCII(origin.protocol(), kDirProtocols[i])) {
      return frame_url.SchemeIs(kDirProtocols[i]) &&
             frame_url.ExtractFileName().empty();
    }
  }

  return false;
}

// Returns true if the frame is navigating to an URL either into or out of an
// extension app's extent.
static bool CrossesExtensionExtents(WebFrame* frame, const GURL& new_url) {
  if (!RenderThread::current())
    return false;

  // If the URL is still empty, this is a window.open navigation. Check the
  // opener's URL.
  GURL old_url(frame->url());
  if (old_url.is_empty() && frame->opener())
    old_url = frame->opener()->url();

  return !ExtensionRendererInfo::InSameExtent(old_url, new_url);
}

// Returns the ISO 639_1 language code of the specified |text|, or 'unknown'
// if it failed.
static std::string DetermineTextLanguage(const string16& text) {
  std::string language = chrome::kUnknownLanguageCode;
  int num_languages = 0;
  int text_bytes = 0;
  bool is_reliable = false;
  Language cld_language =
      DetectLanguageOfUnicodeText(NULL, text.c_str(), true, &is_reliable,
                                  &num_languages, NULL, &text_bytes);
  // We don't trust the result if the CLD reports that the detection is not
  // reliable, or if the actual text used to detect the language was less than
  // 100 bytes (short texts can often lead to wrong results).
  if (is_reliable && text_bytes >= 100 && cld_language != NUM_LANGUAGES &&
      cld_language != UNKNOWN_LANGUAGE && cld_language != TG_UNKNOWN_LANGUAGE) {
    // We should not use LanguageCode_ISO_639_1 because it does not cover all
    // the languages CLD can detect. As a result, it'll return the invalid
    // language code for tradtional Chinese among others.
    // |LanguageCodeWithDialect| will go through ISO 639-1, ISO-639-2 and
    // 'other' tables to do the 'right' thing. In addition, it'll return zh-CN
    // for Simplified Chinese.
    language = LanguageCodeWithDialects(cld_language);
  }
  return language;
}

// Holds pending openFileSystem callbacks.
struct RenderView::PendingOpenFileSystem {
  explicit PendingOpenFileSystem(WebFileSystemCallbacks* c) : callbacks(c) {
  }
  ~PendingOpenFileSystem() {
    if (callbacks)
      callbacks->didFail(WebKit::WebFileErrorAbort);
  }
  WebFileSystemCallbacks* callbacks;
};

///////////////////////////////////////////////////////////////////////////////

int32 RenderView::next_page_id_ = 1;

struct RenderView::PendingFileChooser {
  PendingFileChooser(const ViewHostMsg_RunFileChooser_Params& p,
                     WebFileChooserCompletion* c)
      : params(p),
        completion(c) {
  }
  ViewHostMsg_RunFileChooser_Params params;
  WebFileChooserCompletion* completion;  // MAY BE NULL to skip callback.
};

RenderView::RenderView(RenderThreadBase* render_thread,
                       const WebPreferences& webkit_preferences,
                       int64 session_storage_namespace_id)
    : RenderWidget(render_thread, WebKit::WebPopupTypeNone),
      webkit_preferences_(webkit_preferences),
      send_content_state_immediately_(false),
      enabled_bindings_(0),
      send_preferred_size_changes_(false),
      script_can_close_(true),
      is_loading_(false),
      navigation_gesture_(NavigationGestureUnknown),
      opened_by_user_gesture_(true),
      opener_suppressed_(false),
      page_id_(-1),
      last_page_id_sent_to_browser_(-1),
      last_indexed_page_id_(-1),
      history_list_offset_(-1),
      history_list_length_(0),
      has_unload_listener_(false),
#if defined(OS_MACOSX)
      has_document_tag_(false),
#endif
      document_tag_(0),
      cross_origin_access_count_(0),
      same_origin_access_count_(0),
      target_url_status_(TARGET_NONE),
      spelling_panel_visible_(false),
      view_type_(ViewType::INVALID),
      browser_window_id_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(pepper_delegate_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(page_info_method_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(autofill_method_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(translate_helper_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(cookie_jar_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          notification_provider_(new NotificationProvider(this))),
      session_storage_namespace_id_(session_storage_namespace_id),
      decrement_shared_popup_at_destruction_(false) {
  password_autocomplete_manager_.reset(new PasswordAutocompleteManager(this));
  autofill_helper_.reset(new AutoFillHelper(this));
  page_click_tracker_.reset(new PageClickTracker(this));
  // Note that the order of insertion of the listeners is important.
  // The password_autocomplete_manager_ takes the first shot at processing the
  // notification and can stop the propagation.
  page_click_tracker_->AddListener(password_autocomplete_manager_.get());
  page_click_tracker_->AddListener(autofill_helper_.get());
  ClearBlockedContentSettings();
}

RenderView::~RenderView() {
  if (decrement_shared_popup_at_destruction_)
    shared_popup_counter_->data--;

  // Dispose of un-disposed image fetchers.
  for (ImageResourceFetcherSet::iterator i = image_fetchers_.begin();
       i != image_fetchers_.end(); ++i) {
    delete *i;
  }

  // If file chooser is still waiting for answer, dispatch empty answer.
  while (!file_chooser_completions_.empty()) {
    if (file_chooser_completions_.front()->completion) {
      file_chooser_completions_.front()->completion->didChooseFile(
          WebVector<WebString>());
    }
    file_chooser_completions_.pop_front();
  }

#if defined(OS_MACOSX)
  // Tell the spellchecker that the document is closed.
  if (has_document_tag_)
    Send(new ViewHostMsg_DocumentWithTagClosed(routing_id_, document_tag_));
#endif

  render_thread_->RemoveFilter(audio_message_filter_);

#ifndef NDEBUG
  // Make sure we are no longer referenced by the ViewMap.
  ViewMap* views = Singleton<ViewMap>::get();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
#endif
}

/*static*/
void RenderView::ForEach(RenderViewVisitor* visitor) {
  ViewMap* views = Singleton<ViewMap>::get();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it) {
    if (!visitor->Visit(it->second))
      return;
  }
}

/*static*/
RenderView* RenderView::FromWebView(WebView* webview) {
  ViewMap* views = Singleton<ViewMap>::get();
  ViewMap::iterator it = views->find(webview);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::Create(
    RenderThreadBase* render_thread,
    gfx::NativeViewId parent_hwnd,
    int32 opener_id,
    const RendererPreferences& renderer_prefs,
    const WebPreferences& webkit_prefs,
    SharedRenderViewCounter* counter,
    int32 routing_id,
    int64 session_storage_namespace_id,
    const string16& frame_name) {
  DCHECK(routing_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderView> view = new RenderView(render_thread, webkit_prefs,
                                                  session_storage_namespace_id);
  view->Init(parent_hwnd,
             opener_id,
             renderer_prefs,
             counter,
             routing_id,
             frame_name);  // adds reference
  return view;
}

/*static*/
void RenderView::SetNextPageID(int32 next_page_id) {
  // This method should only be called during process startup, and the given
  // page id had better not exceed our current next page id!
  DCHECK_EQ(next_page_id_, 1);
  DCHECK(next_page_id >= next_page_id_);
  next_page_id_ = next_page_id;
}

WebKit::WebView* RenderView::webview() const {
  return static_cast<WebKit::WebView*>(webwidget());
}

void RenderView::UserMetricsRecordAction(const std::string& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(routing_id_, action));
}

void RenderView::PluginCrashed(const FilePath& plugin_path) {
  Send(new ViewHostMsg_CrashedPlugin(routing_id_, plugin_path));
}

WebPlugin* RenderView::CreatePluginNoCheck(WebFrame* frame,
                                           const WebPluginParams& params) {
  WebPluginInfo info;
  bool found;
  ContentSetting setting;
  std::string mime_type;
  Send(new ViewHostMsg_GetPluginInfo(
      params.url, frame->top()->url(), params.mimeType.utf8(), &found,
      &info, &setting, &mime_type));
  if (!found || !info.enabled)
    return NULL;
  scoped_refptr<pepper::PluginModule> pepper_module =
      PepperPluginRegistry::GetInstance()->GetModule(info.path);
  if (pepper_module)
    return CreatePepperPlugin(frame, params, info.path, pepper_module.get());
  else
    return CreateNPAPIPlugin(frame, params, info.path, mime_type);
}

#if defined(OS_MACOSX)
void RenderView::RegisterPluginDelegate(WebPluginDelegateProxy* delegate) {
  plugin_delegates_.insert(delegate);
  // If the renderer is visible, set initial visibility and focus state.
  if (!is_hidden()) {
    delegate->SetContainerVisibility(true);
    if (webview() && webview()->isActive())
      delegate->SetWindowFocus(true);
    if (has_focus())
      delegate->SetContentAreaFocus(true);
  }
}

void RenderView::UnregisterPluginDelegate(WebPluginDelegateProxy* delegate) {
  plugin_delegates_.erase(delegate);
}
#endif

void RenderView::Init(gfx::NativeViewId parent_hwnd,
                      int32 opener_id,
                      const RendererPreferences& renderer_prefs,
                      SharedRenderViewCounter* counter,
                      int32 routing_id,
                      const string16& frame_name) {
  DCHECK(!webview());

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  if (counter) {
    shared_popup_counter_ = counter;
    shared_popup_counter_->data++;
    decrement_shared_popup_at_destruction_ = true;
  } else {
    shared_popup_counter_ = new SharedRenderViewCounter(0);
    decrement_shared_popup_at_destruction_ = false;
  }

  devtools_agent_.reset(new DevToolsAgent(routing_id, this));

  webwidget_ = WebView::create(this, devtools_agent_.get());
  Singleton<ViewMap>::get()->insert(std::make_pair(webview(), this));
  webkit_preferences_.Apply(webview());
  webview()->initializeMainFrame(this);
  if (!frame_name.empty())
    webview()->mainFrame()->setName(frame_name);

  OnSetRendererPrefs(renderer_prefs);

  routing_id_ = routing_id;
  render_thread_->AddRoute(routing_id_, this);
  // Take a reference on behalf of the RenderThread.  This will be balanced
  // when we receive ViewMsg_Close.
  AddRef();

  // If this is a popup, we must wait for the CreatingNew_ACK message before
  // completing initialization.  Otherwise, we can finish it now.
  if (opener_id == MSG_ROUTING_NONE) {
    did_show_ = true;
    CompleteInit(parent_hwnd);
  }

  host_window_ = parent_hwnd;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDomAutomationController))
    enabled_bindings_ |= BindingsPolicy::DOM_AUTOMATION;

  audio_message_filter_ = new AudioMessageFilter(routing_id_);
  render_thread_->AddFilter(audio_message_filter_);
}

void RenderView::OnMessageReceived(const IPC::Message& message) {
  WebFrame* main_frame = webview() ? webview()->mainFrame() : NULL;
  if (main_frame)
    child_process_logging::SetActiveURL(main_frame->url());

  // If this is developer tools renderer intercept tools messages first.
  if (devtools_client_.get() && devtools_client_->OnMessageReceived(message))
    return;
  if (devtools_agent_.get() && devtools_agent_->OnMessageReceived(message))
    return;
  if (notification_provider_->OnMessageReceived(message))
    return;
  if (geolocation_dispatcher_.get() &&
      geolocation_dispatcher_->OnMessageReceived(message)) {
    return;
  }
  if (speech_input_dispatcher_.get() &&
      speech_input_dispatcher_->OnMessageReceived(message)) {
    return;
  }
  if (device_orientation_dispatcher_.get() &&
      device_orientation_dispatcher_->OnMessageReceived(message)) {
    return;
  }

  IPC_BEGIN_MESSAGE_MAP(RenderView, message)
    IPC_MESSAGE_HANDLER(ViewMsg_CaptureThumbnail, OnCaptureThumbnail)
    IPC_MESSAGE_HANDLER(ViewMsg_CaptureSnapshot, OnCaptureSnapshot)
    IPC_MESSAGE_HANDLER(ViewMsg_PrintPages, OnPrintPages)
    IPC_MESSAGE_HANDLER(ViewMsg_PrintingDone, OnPrintingDone)
    IPC_MESSAGE_HANDLER(ViewMsg_Navigate, OnNavigate)
    IPC_MESSAGE_HANDLER(ViewMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(ViewMsg_ReloadFrame, OnReloadFrame)
    IPC_MESSAGE_HANDLER(ViewMsg_Undo, OnUndo)
    IPC_MESSAGE_HANDLER(ViewMsg_Redo, OnRedo)
    IPC_MESSAGE_HANDLER(ViewMsg_Cut, OnCut)
    IPC_MESSAGE_HANDLER(ViewMsg_Copy, OnCopy)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_CopyToFindPboard, OnCopyToFindPboard)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_Paste, OnPaste)
    IPC_MESSAGE_HANDLER(ViewMsg_Replace, OnReplace)
    IPC_MESSAGE_HANDLER(ViewMsg_ToggleSpellPanel, OnToggleSpellPanel)
    IPC_MESSAGE_HANDLER(ViewMsg_AdvanceToNextMisspelling,
                        OnAdvanceToNextMisspelling)
    IPC_MESSAGE_HANDLER(ViewMsg_ToggleSpellCheck, OnToggleSpellCheck)
    IPC_MESSAGE_HANDLER(ViewMsg_Delete, OnDelete)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectAll, OnSelectAll)
    IPC_MESSAGE_HANDLER(ViewMsg_CopyImageAt, OnCopyImageAt)
    IPC_MESSAGE_HANDLER(ViewMsg_ExecuteEditCommand, OnExecuteEditCommand)
    IPC_MESSAGE_HANDLER(ViewMsg_Find, OnFind)
    IPC_MESSAGE_HANDLER(ViewMsg_StopFinding, OnStopFinding)
    IPC_MESSAGE_HANDLER(ViewMsg_FindReplyACK, OnFindReplyAck)
    IPC_MESSAGE_HANDLER(ViewMsg_Zoom, OnZoom)
    IPC_MESSAGE_HANDLER(ViewMsg_SetContentSettingsForLoadingURL,
                        OnSetContentSettingsForLoadingURL)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForLoadingURL,
                        OnSetZoomLevelForLoadingURL)
    IPC_MESSAGE_HANDLER(ViewMsg_SetPageEncoding, OnSetPageEncoding)
    IPC_MESSAGE_HANDLER(ViewMsg_ResetPageEncodingToDefault,
                        OnResetPageEncodingToDefault)
    IPC_MESSAGE_HANDLER(ViewMsg_SetupDevToolsClient, OnSetupDevToolsClient)
    IPC_MESSAGE_HANDLER(ViewMsg_DownloadFavIcon, OnDownloadFavIcon)
    IPC_MESSAGE_HANDLER(ViewMsg_ScriptEvalRequest, OnScriptEvalRequest)
    IPC_MESSAGE_HANDLER(ViewMsg_CSSInsertRequest, OnCSSInsertRequest)
    IPC_MESSAGE_HANDLER(ViewMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(ViewMsg_ReservePageIDRange, OnReservePageIDRange)
    IPC_MESSAGE_HANDLER(ViewMsg_FillPasswordForm, OnFillPasswordForm)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDragEnter, OnDragTargetDragEnter)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDragOver, OnDragTargetDragOver)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDragLeave, OnDragTargetDragLeave)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDrop, OnDragTargetDrop)
    IPC_MESSAGE_HANDLER(ViewMsg_AllowBindings, OnAllowBindings)
    IPC_MESSAGE_HANDLER(ViewMsg_SetDOMUIProperty, OnSetDOMUIProperty)
    IPC_MESSAGE_HANDLER(ViewMsg_DragSourceEndedOrMoved,
                        OnDragSourceEndedOrMoved)
    IPC_MESSAGE_HANDLER(ViewMsg_DragSourceSystemDragEnded,
                        OnDragSourceSystemDragEnded)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInitialFocus, OnSetInitialFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTargetURL_ACK, OnUpdateTargetURLAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateWebPreferences, OnUpdateWebPreferences)
    IPC_MESSAGE_HANDLER(ViewMsg_SetAltErrorPageURL, OnSetAltErrorPageURL)
    IPC_MESSAGE_HANDLER(ViewMsg_InstallMissingPlugin, OnInstallMissingPlugin)
    IPC_MESSAGE_HANDLER(ViewMsg_LoadBlockedPlugins, OnLoadBlockedPlugins)
    IPC_MESSAGE_HANDLER(ViewMsg_RunFileChooserResponse, OnFileChooserResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableViewSourceMode, OnEnableViewSourceMode)
    IPC_MESSAGE_HANDLER(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                        OnGetAllSavableResourceLinksForCurrentPage)
    IPC_MESSAGE_HANDLER(
        ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
        OnGetSerializedHtmlDataForCurrentPageWithLocalLinks)
    IPC_MESSAGE_HANDLER(ViewMsg_GetApplicationInfo, OnGetApplicationInfo)
    IPC_MESSAGE_HANDLER(ViewMsg_ShouldClose, OnShouldClose)
    IPC_MESSAGE_HANDLER(ViewMsg_ClosePage, OnClosePage)
    IPC_MESSAGE_HANDLER(ViewMsg_ThemeChanged, OnThemeChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_HandleMessageFromExternalHost,
                        OnHandleMessageFromExternalHost)
    IPC_MESSAGE_HANDLER(ViewMsg_DisassociateFromPopupCount,
                        OnDisassociateFromPopupCount)
    IPC_MESSAGE_HANDLER(ViewMsg_AutoFillSuggestionsReturned,
                        OnAutoFillSuggestionsReturned)
    IPC_MESSAGE_HANDLER(ViewMsg_AutoFillFormDataFilled,
                        OnAutoFillFormDataFilled)
    IPC_MESSAGE_HANDLER(ViewMsg_AllowScriptToClose,
                        OnAllowScriptToClose)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveOrResizeStarted, OnMoveOrResizeStarted)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtensionResponse, OnExtensionResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtensionMessageInvoke,
                        OnExtensionMessageInvoke)
    IPC_MESSAGE_HANDLER(ViewMsg_ClearFocusedNode, OnClearFocusedNode)
    IPC_MESSAGE_HANDLER(ViewMsg_SetBackground, OnSetBackground)
    IPC_MESSAGE_HANDLER(ViewMsg_EnablePreferredSizeChangedMode,
                        OnEnablePreferredSizeChangedMode)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableScrollbarsForSmallWindows,
                        OnDisableScrollbarsForSmallWindows)
    IPC_MESSAGE_HANDLER(ViewMsg_SetRendererPrefs, OnSetRendererPrefs)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateBrowserWindowId,
                        OnUpdateBrowserWindowId)
    IPC_MESSAGE_HANDLER(ViewMsg_NotifyRenderViewType,
                        OnNotifyRendererViewType)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaPlayerActionAt, OnMediaPlayerActionAt)
    IPC_MESSAGE_HANDLER(ViewMsg_SetActive, OnSetActive)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_SetWindowVisibility, OnSetWindowVisibility)
    IPC_MESSAGE_HANDLER(ViewMsg_WindowFrameChanged, OnWindowFrameChanged)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_SetEditCommandsForNextKeyEvent,
                        OnSetEditCommandsForNextKeyEvent)
    IPC_MESSAGE_HANDLER(ViewMsg_ExecuteCode,
                        OnExecuteCode)
    IPC_MESSAGE_HANDLER(ViewMsg_CustomContextMenuAction,
                        OnCustomContextMenuAction)
    IPC_MESSAGE_HANDLER(ViewMsg_TranslatePage, OnTranslatePage)
    IPC_MESSAGE_HANDLER(ViewMsg_RevertTranslation, OnRevertTranslation)
    IPC_MESSAGE_HANDLER(ViewMsg_GetAccessibilityTree, OnGetAccessibilityTree)
    IPC_MESSAGE_HANDLER(ViewMsg_SetAccessibilityFocus, OnSetAccessibilityFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_AccessibilityDoDefaultAction,
                        OnAccessibilityDoDefaultAction)
    IPC_MESSAGE_HANDLER(ViewMsg_AccessibilityObjectChildrenChange_ACK,
                        OnAccessibilityObjectChildrenChangeAck)
    IPC_MESSAGE_HANDLER(ViewMsg_OpenFileSystemRequest_Complete,
                        OnOpenFileSystemRequestComplete)
    IPC_MESSAGE_HANDLER(ViewMsg_AsyncOpenFile_ACK, OnAsyncFileOpened)

    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(RenderWidget::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()
}

void RenderView::OnCaptureThumbnail() {
  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  // get the URL for this page
  GURL url(main_frame->url());
  if (url.is_empty())
    return;

  if (size_.IsEmpty())
    return;  // Don't create an empty thumbnail!

  ThumbnailScore score;
  SkBitmap thumbnail;
  if (!CaptureThumbnail(webview(), kThumbnailWidth, kThumbnailHeight,
                        &thumbnail, &score))
    return;

  // send the thumbnail message to the browser process
  Send(new ViewHostMsg_Thumbnail(routing_id_, url, score, thumbnail));
}

void RenderView::OnCaptureSnapshot() {
  SkBitmap snapshot;
  bool error = false;

  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    error = true;

  if (!error && !CaptureSnapshot(webview(), &snapshot))
    error = true;

  DCHECK(error == snapshot.empty()) <<
      "Snapshot should be empty on error, non-empty otherwise.";

  // Send the snapshot to the browser process.
  Send(new ViewHostMsg_Snapshot(routing_id_, snapshot));
}

void RenderView::OnPrintPages() {
  DCHECK(webview());
  if (webview()) {
    // If the user has selected text in the currently focused frame we print
    // only that frame (this makes print selection work for multiple frames).
    if (webview()->focusedFrame()->hasSelection())
      Print(webview()->focusedFrame(), false);
    else
      Print(webview()->mainFrame(), false);
  }
}

void RenderView::OnPrintingDone(int document_cookie, bool success) {
  // Ignoring document cookie here since only one print job can be outstanding
  // per renderer and document_cookie is 0 when printing is successful.
  DCHECK(print_helper_.get());
  if (print_helper_.get() != NULL) {
    print_helper_->DidFinishPrinting(success);
  }
}

void RenderView::CapturePageInfo(int load_id, bool preliminary_capture) {
  if (load_id != page_id_)
    return;  // this capture call is no longer relevant due to navigation
  if (load_id == last_indexed_page_id_)
    return;  // we already indexed this page

  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();
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
    WebKit::WebDocument document = main_frame->document();
    // If the page explicitly specifies a language, use it, otherwise we'll
    // determine it based on the text content using the CLD.
    std::string language =
        TranslateHelper::GetPageLanguageFromMetaTag(&document);
    if (language.empty()) {
      base::TimeTicks begin_time = base::TimeTicks::Now();
      language = DetermineTextLanguage(contents);
      UMA_HISTOGRAM_MEDIUM_TIMES("Renderer4.LanguageDetection",
                                 base::TimeTicks::Now() - begin_time);
    }
    // Send the text to the browser for indexing (the browser might decide not
    // to index, if the URL is HTTPS for instance) and language discovery.
    Send(new ViewHostMsg_PageContents(
        routing_id_, url, load_id, contents, language,
        TranslateHelper::IsPageTranslatable(&document)));
  }

  OnCaptureThumbnail();
}

void RenderView::CaptureText(WebFrame* frame, string16* contents) {
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

bool RenderView::CaptureThumbnail(WebView* view,
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

  // Resample the subset that we want to get it the right size.
  *thumbnail = skia::ImageOperations::Resize(
      subset, skia::ImageOperations::RESIZE_LANCZOS3, w, h);

  score->boring_score = CalculateBoringScore(thumbnail);

  HISTOGRAM_TIMES("Renderer4.Thumbnail",
                  base::TimeTicks::Now() - beginning_time);
  return true;
}

bool RenderView::CaptureSnapshot(WebView* view, SkBitmap* snapshot) {
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

void RenderView::OnNavigate(const ViewMsg_Navigate_Params& params) {
  if (!webview())
    return;

  history_list_offset_ = params.current_history_list_offset;
  history_list_length_ = params.current_history_list_length;

  if (devtools_agent_.get())
    devtools_agent_->OnNavigate();

  if (notification_provider_.get())
    notification_provider_->OnNavigate();

  child_process_logging::SetActiveURL(params.url);

  AboutHandler::MaybeHandle(params.url);

  bool is_reload =
      params.navigation_type == ViewMsg_Navigate_Params::RELOAD ||
      params.navigation_type == ViewMsg_Navigate_Params::RELOAD_IGNORING_CACHE;

  WebFrame* main_frame = webview()->mainFrame();
  if (is_reload && main_frame->currentHistoryItem().isNull()) {
    // We cannot reload if we do not have any history state.  This happens, for
    // example, when recovering from a crash.  Our workaround here is a bit of
    // a hack since it means that reload after a crashed tab does not cause an
    // end-to-end cache validation.
    is_reload = false;
  }

  // A navigation resulting from loading a javascript URL should not be treated
  // as a browser initiated event.  Instead, we want it to look as if the page
  // initiated any load resulting from JS execution.
  if (!params.url.SchemeIs(chrome::kJavaScriptScheme)) {
    NavigationState* state = NavigationState::CreateBrowserInitiated(
        params.page_id,
        params.pending_history_list_offset,
        params.transition,
        params.request_time);
    if (params.navigation_type == ViewMsg_Navigate_Params::RESTORE) {
      // We're doing a load of a page that was restored from the last session.
      // By default this prefers the cache over loading (LOAD_PREFERRING_CACHE)
      // which can result in stale data for pages that are set to expire. We
      // explicitly override that by setting the policy here so that as
      // necessary we load from the network.
      state->set_cache_policy_override(WebURLRequest::UseProtocolCachePolicy);
    }
    pending_navigation_state_.reset(state);
  }

  NavigationState* navigation_state = pending_navigation_state_.get();

  // If we are reloading, then WebKit will use the history state of the current
  // page, so we should just ignore any given history state.  Otherwise, if we
  // have history state, then we need to navigate to it, which corresponds to a
  // back/forward navigation event.
  if (is_reload) {
    if (navigation_state)
      navigation_state->set_load_type(NavigationState::RELOAD);
    bool ignore_cache = (params.navigation_type ==
                             ViewMsg_Navigate_Params::RELOAD_IGNORING_CACHE);
    main_frame->reload(ignore_cache);
  } else if (!params.state.empty()) {
    // We must know the page ID of the page we are navigating back to.
    DCHECK_NE(params.page_id, -1);
    if (navigation_state)
      navigation_state->set_load_type(NavigationState::HISTORY_LOAD);
    main_frame->loadHistoryItem(
        webkit_glue::HistoryItemFromString(params.state));
  } else {
    // Navigate to the given URL.
    WebURLRequest request(params.url);

    // A session history navigation should have been accompanied by state.
    DCHECK_EQ(params.page_id, -1);

    if (main_frame->isViewSourceModeEnabled())
      request.setCachePolicy(WebURLRequest::ReturnCacheDataElseLoad);

    if (params.referrer.is_valid()) {
      request.setHTTPHeaderField(WebString::fromUTF8("Referer"),
                                 WebString::fromUTF8(params.referrer.spec()));
    }

    if (navigation_state)
      navigation_state->set_load_type(NavigationState::NORMAL_LOAD);
    main_frame->loadRequest(request);
  }

  // In case LoadRequest failed before DidCreateDataSource was called.
  pending_navigation_state_.reset();
}

// Stop loading the current page
void RenderView::OnStop() {
  if (webview())
    webview()->mainFrame()->stopLoading();
}

// Reload current focused frame.
// E.g. called by right-clicking on the frame and picking "reload this frame".
void RenderView::OnReloadFrame() {
  if (webview() && webview()->focusedFrame()) {
    // We always obey the cache (ignore_cache=false) here.
    // TODO(evanm): perhaps we could allow shift-clicking the menu item to do
    // a cache-ignoring reload of the frame.
    webview()->focusedFrame()->reload(false);
  }
}

void RenderView::OnCopyImageAt(int x, int y) {
  webview()->copyImageAt(WebPoint(x, y));
}

void RenderView::OnExecuteEditCommand(const std::string& name,
    const std::string& value) {
  if (!webview() || !webview()->focusedFrame())
    return;

  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8(name), WebString::fromUTF8(value));
}

void RenderView::OnSetupDevToolsClient() {
  DCHECK(!devtools_client_.get());
  devtools_client_.reset(new DevToolsClient(this));
}

void RenderView::OnUpdateTargetURLAck() {
  // Check if there is a targeturl waiting to be sent.
  if (target_url_status_ == TARGET_PENDING) {
    Send(new ViewHostMsg_UpdateTargetURL(routing_id_, page_id_,
                                         pending_target_url_));
  }

  target_url_status_ = TARGET_NONE;
}

void RenderView::OnUndo() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Undo"));
  UserMetricsRecordAction("Undo");
}

void RenderView::OnRedo() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Redo"));
  UserMetricsRecordAction("Redo");
}

void RenderView::OnCut() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Cut"));
  UserMetricsRecordAction("Cut");
}

void RenderView::OnCopy() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Copy"));
  UserMetricsRecordAction("Copy");
}

#if defined(OS_MACOSX)
void RenderView::OnCopyToFindPboard() {
  if (!webview())
    return;

  // Since the find pasteboard supports only plain text, this can be simpler
  // than the |OnCopy()| case.
  WebFrame* frame = webview()->focusedFrame();
  if (frame->hasSelection()) {
    string16 selection = frame->selectionAsText();
    RenderThread::current()->Send(
        new ViewHostMsg_ClipboardFindPboardWriteStringAsync(selection));
  }

  UserMetricsRecordAction("CopyToFindPboard");
}
#endif

void RenderView::OnPaste() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Paste"));
  UserMetricsRecordAction("Paste");
}

void RenderView::OnReplace(const string16& text) {
  if (!webview())
    return;

  WebFrame* frame = webview()->focusedFrame();
  if (!frame->hasSelection())
    frame->selectWordAroundCaret();
  frame->replaceSelection(text);
}

void RenderView::OnAdvanceToNextMisspelling() {
  if (!webview())
    return;
  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8("AdvanceToNextMisspelling"));
}

void RenderView::OnToggleSpellPanel(bool is_currently_visible) {
  if (!webview())
    return;
  // We need to tell the webView whether the spelling panel is visible or not so
  // that it won't need to make ipc calls later.
  spelling_panel_visible_ = is_currently_visible;
  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8("ToggleSpellPanel"));
}

void RenderView::OnToggleSpellCheck() {
  if (!webview())
    return;

  WebFrame* frame = webview()->focusedFrame();
  frame->enableContinuousSpellChecking(
      !frame->isContinuousSpellCheckingEnabled());
}

void RenderView::OnDelete() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Delete"));
  UserMetricsRecordAction("DeleteSelection");
}

void RenderView::OnSelectAll() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8("SelectAll"));
  UserMetricsRecordAction("SelectAll");
}

void RenderView::OnSetInitialFocus(bool reverse) {
  if (!webview())
    return;
  webview()->setInitialFocus(reverse);
}

///////////////////////////////////////////////////////////////////////////////

void RenderView::SetContentSettings(const ContentSettings& settings) {
  current_content_settings_ = settings;
}

// Tell the embedding application that the URL of the active page has changed
void RenderView::UpdateURL(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  DCHECK(ds);

  const WebURLRequest& request = ds->request();
  const WebURLRequest& original_request = ds->originalRequest();
  const WebURLResponse& response = ds->response();

  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);

  ViewHostMsg_FrameNavigate_Params params;
  params.http_status_code = response.httpStatusCode();
  params.is_post = false;
  params.page_id = page_id_;
  params.is_content_filtered = response.isContentFiltered();
  params.was_within_same_page = navigation_state->was_within_same_page();
  if (!navigation_state->security_info().empty()) {
    // SSL state specified in the request takes precedence over the one in the
    // response.
    // So far this is only intended for error pages that are not expected to be
    // over ssl, so we should not get any clash.
    DCHECK(response.securityInfo().isEmpty());
    params.security_info = navigation_state->security_info();
  } else {
    params.security_info = response.securityInfo();
  }

  // Set the URL to be displayed in the browser UI to the user.
  if (ds->hasUnreachableURL()) {
    params.url = ds->unreachableURL();
  } else {
    params.url = request.url();
  }

  GetRedirectChain(ds, &params.redirects);
  params.should_update_history = !ds->hasUnreachableURL() &&
      !response.isMultipartPayload();

  params.searchable_form_url = navigation_state->searchable_form_url();
  params.searchable_form_encoding =
      navigation_state->searchable_form_encoding();

  const PasswordForm* password_form_data =
      navigation_state->password_form_data();
  if (password_form_data)
    params.password_form = *password_form_data;

  params.gesture = navigation_gesture_;
  navigation_gesture_ = NavigationGestureUnknown;

  if (!frame->parent()) {
    // Top-level navigation.

    // Clear "block" flags for the new page. This needs to happen before any of
    // allowScripts(), allowImages(), allowPlugins() is called for the new page
    // so that these functions can correctly detect that a piece of content
    // flipped from "not blocked" to "blocked".
    ClearBlockedContentSettings();

    // Set content settings. Default them from the parent window if one exists.
    // This makes sure about:blank windows work as expected.
    HostContentSettings::iterator host_content_settings =
        host_content_settings_.find(GURL(request.url()));
    if (host_content_settings != host_content_settings_.end()) {
      SetContentSettings(host_content_settings->second);

      // These content settings were merely recorded transiently for this load.
      // We can erase them now.  If at some point we reload this page, the
      // browser will send us new, up-to-date content settings.
      host_content_settings_.erase(host_content_settings);
    } else if (frame->opener()) {
      // The opener's view is not guaranteed to be non-null (it could be
      // detached from its page but not yet destructed).
      if (WebView* opener_view = frame->opener()->view()) {
        RenderView* opener = FromWebView(opener_view);
        SetContentSettings(opener->current_content_settings_);
      }
    }

    // Set zoom level.
    HostZoomLevels::iterator host_zoom =
        host_zoom_levels_.find(GURL(request.url()));
    if (host_zoom != host_zoom_levels_.end()) {
      webview()->setZoomLevel(false, host_zoom->second);
      // This zoom level was merely recorded transiently for this load.  We can
      // erase it now.  If at some point we reload this page, the browser will
      // send us a new, up-to-date zoom level.
      host_zoom_levels_.erase(host_zoom);
    }

    // Update contents MIME type for main frame.
    params.contents_mime_type = ds->response().mimeType().utf8();

    params.transition = navigation_state->transition_type();
    if (!PageTransition::IsMainFrame(params.transition)) {
      // If the main frame does a load, it should not be reported as a subframe
      // navigation.  This can occur in the following case:
      // 1. You're on a site with frames.
      // 2. You do a subframe navigation.  This is stored with transition type
      //    MANUAL_SUBFRAME.
      // 3. You navigate to some non-frame site, say, google.com.
      // 4. You navigate back to the page from step 2.  Since it was initially
      //    MANUAL_SUBFRAME, it will be that same transition type here.
      // We don't want that, because any navigation that changes the toplevel
      // frame should be tracked as a toplevel navigation (this allows us to
      // update the URL bar, etc).
      params.transition = PageTransition::LINK;
    }

    // If we have a valid consumed client redirect source,
    // the page contained a client redirect (meta refresh, document.loc...),
    // so we set the referrer and transition to match.
    if (completed_client_redirect_src_.is_valid()) {
      DCHECK(completed_client_redirect_src_ == params.redirects[0]);
      params.referrer = completed_client_redirect_src_;
      params.transition = static_cast<PageTransition::Type>(
          params.transition | PageTransition::CLIENT_REDIRECT);
    } else {
      // Bug 654101: the referrer will be empty on https->http transitions. It
      // would be nice if we could get the real referrer from somewhere.
      params.referrer = GURL(
          original_request.httpHeaderField(WebString::fromUTF8("Referer")));
    }

    string16 method = request.httpMethod();
    if (EqualsASCII(method, "POST"))
      params.is_post = true;

    // Save some histogram data so we can compute the average memory used per
    // page load of the glyphs.
    UMA_HISTOGRAM_COUNTS_10000("Memory.GlyphPagesPerLoad",
                               webkit_glue::GetGlyphPageCount());

    // This message needs to be sent before any of allowScripts(),
    // allowImages(), allowPlugins() is called for the new page, so that when
    // these functions send a ViewHostMsg_ContentBlocked message, it arrives
    // after the ViewHostMsg_FrameNavigate message.
    Send(new ViewHostMsg_FrameNavigate(routing_id_, params));
  } else {
    // Subframe navigation: the type depends on whether this navigation
    // generated a new session history entry. When they do generate a session
    // history entry, it means the user initiated the navigation and we should
    // mark it as such. This test checks if this is the first time UpdateURL
    // has been called since WillNavigateToURL was called to initiate the load.
    if (page_id_ > last_page_id_sent_to_browser_)
      params.transition = PageTransition::MANUAL_SUBFRAME;
    else
      params.transition = PageTransition::AUTO_SUBFRAME;

    Send(new ViewHostMsg_FrameNavigate(routing_id_, params));
  }

  last_page_id_sent_to_browser_ =
      std::max(last_page_id_sent_to_browser_, page_id_);

  // If we end up reusing this WebRequest (for example, due to a #ref click),
  // we don't want the transition type to persist.  Just clear it.
  navigation_state->set_transition_type(PageTransition::LINK);

  // Check if the navigation was within the same page, in which case we don't
  // want to clear the accessibility cache.
  if (accessibility_.get() && !navigation_state->was_within_same_page()) {
    accessibility_->clear();
    accessibility_.reset();
  }
  accessibility_changes_.clear();
}

// Tell the embedding application that the title of the active page has changed
void RenderView::UpdateTitle(WebFrame* frame, const string16& title) {
  // Ignore all but top level navigations...
  if (!frame->parent()) {
    Send(new ViewHostMsg_UpdateTitle(
        routing_id_,
        page_id_,
        UTF16ToWideHack(title.length() > chrome::kMaxTitleChars ?
            title.substr(0, chrome::kMaxTitleChars) : title)));
  }
}

void RenderView::UpdateEncoding(WebFrame* frame,
                                const std::string& encoding_name) {
  // Only update main frame's encoding_name.
  if (webview()->mainFrame() == frame &&
      last_encoding_name_ != encoding_name) {
    // Save the encoding name for later comparing.
    last_encoding_name_ = encoding_name;

    Send(new ViewHostMsg_UpdateEncoding(routing_id_, last_encoding_name_));
  }
}

// Sends the previous session history state to the browser so it will be saved
// before we navigate to a new page. This must be called *before* the page ID
// has been updated so we know what it was.
void RenderView::UpdateSessionHistory(WebFrame* frame) {
  // If we have a valid page ID at this point, then it corresponds to the page
  // we are navigating away from.  Otherwise, this is the first navigation, so
  // there is no past session history to record.
  if (page_id_ == -1)
    return;

  const WebHistoryItem& item =
      webview()->mainFrame()->previousHistoryItem();
  if (item.isNull())
    return;

  Send(new ViewHostMsg_UpdateState(
      routing_id_, page_id_, webkit_glue::HistoryItemToString(item)));
}

void RenderView::OpenURL(
    const GURL& url, const GURL& referrer, WebNavigationPolicy policy) {
  Send(new ViewHostMsg_OpenURL(
      routing_id_, url, referrer, NavigationPolicyToDisposition(policy)));
}

// WebViewDelegate ------------------------------------------------------------

void RenderView::LoadNavigationErrorPage(WebFrame* frame,
                                         const WebURLRequest& failed_request,
                                         const WebURLError& error,
                                         const std::string& html,
                                         bool replace) {
  GURL failed_url = error.unreachableURL;
  std::string alt_html;
  ExtensionRendererInfo* extension = NULL;
  if (html.empty()) {
    // Use a local error page.
    int resource_id;
    DictionaryValue error_strings;

    if (failed_url.is_valid())
      extension = ExtensionRendererInfo::GetByURL(failed_url);
    if (extension) {
      LocalizedError::GetAppErrorStrings(error, failed_url, extension,
                                         &error_strings);

      // TODO(erikkay): Should we use a different template for different
      // error messages?
      resource_id = IDR_ERROR_APP_HTML;
    } else {
      if (error.domain == WebString::fromUTF8(net::kErrorDomain) &&
          error.reason == net::ERR_CACHE_MISS &&
          EqualsASCII(failed_request.httpMethod(), "POST")) {
        LocalizedError::GetFormRepostStrings(failed_url, &error_strings);
        resource_id = IDR_ERROR_NO_DETAILS_HTML;
      } else {
        LocalizedError::GetStrings(error, &error_strings);
        resource_id = IDR_NET_ERROR_HTML;
      }
    }

    alt_html = GetAltHTMLForTemplate(error_strings, resource_id);
  } else {
    alt_html = html;
  }

  frame->loadHTMLString(alt_html,
                        GURL(kUnreachableWebDataURL),
                        failed_url,
                        replace);
}

void RenderView::BindDOMAutomationController(WebFrame* frame) {
  dom_automation_controller_.set_message_sender(this);
  dom_automation_controller_.set_routing_id(routing_id_);
  dom_automation_controller_.BindToJavascript(frame,
                                              L"domAutomationController");
}

bool RenderView::RunJavaScriptMessage(int type,
                                      const std::wstring& message,
                                      const std::wstring& default_value,
                                      const GURL& frame_url,
                                      std::wstring* result) {
  bool success = false;
  std::wstring result_temp;
  if (!result)
    result = &result_temp;

  SendAndRunNestedMessageLoop(new ViewHostMsg_RunJavaScriptMessage(
      routing_id_, message, default_value, frame_url, type, &success, result));
  return success;
}

bool RenderView::SendAndRunNestedMessageLoop(IPC::SyncMessage* message) {
  // Before WebKit asks us to show an alert (etc.), it takes care of doing the
  // equivalent of WebView::willEnterModalLoop.  In the case of showModalDialog
  // it is particularly important that we do not call willEnterModalLoop as
  // that would defer resource loads for the dialog itself.
  if (RenderThread::current())  // Will be NULL during unit tests.
    RenderThread::current()->DoNotNotifyWebKitOfModalLoop();

  message->EnableMessagePumping();  // Runs a nested message loop.
  return Send(message);
}

void RenderView::AddGURLSearchProvider(const GURL& osd_url, bool autodetected) {
  if (!osd_url.is_empty())
    Send(new ViewHostMsg_PageHasOSDD(routing_id_, page_id_, osd_url,
                                     autodetected));
}

void RenderView::OnAutoFillSuggestionsReturned(
    int query_id,
    const std::vector<string16>& values,
    const std::vector<string16>& labels,
    const std::vector<string16>& icons,
    const std::vector<int>& unique_ids) {
  autofill_helper_->SuggestionsReceived(
      query_id, values, labels, icons, unique_ids);
}

void RenderView::OnAutoFillFormDataFilled(int query_id,
                                          const webkit_glue::FormData& form) {
  autofill_helper_->FormDataFilled(query_id, form);
}

void RenderView::OnAllowScriptToClose(bool script_can_close) {
  script_can_close_ = script_can_close;
}

uint32 RenderView::GetCPBrowsingContext() {
  uint32 context = 0;
  Send(new ViewHostMsg_GetCPBrowsingContext(&context));
  return context;
}

void RenderView::AddSearchProvider(const std::string& url) {
  AddGURLSearchProvider(GURL(url),
                        false);  // not autodetected
}

ViewHostMsg_GetSearchProviderInstallState_Params
RenderView::GetSearchProviderInstallState(WebFrame* frame,
                                          const std::string& url) {
  GURL inquiry_url = GURL(url);
  if (inquiry_url.is_empty())
    return ViewHostMsg_GetSearchProviderInstallState_Params::Denied();

  ViewHostMsg_GetSearchProviderInstallState_Params install;
  Send(new ViewHostMsg_GetSearchProviderInstallState(routing_id_,
                                                     frame->url(),
                                                     inquiry_url,
                                                     &install));
  return install;
}

void RenderView::OnMissingPluginStatus(
    WebPluginDelegateProxy* delegate,
    int status) {
#if defined(OS_WIN)
  if (!first_default_plugin_) {
    // Show the InfoBar for the first available plugin.
    if (status == default_plugin::MISSING_PLUGIN_AVAILABLE) {
      first_default_plugin_ = delegate->AsWeakPtr();
      Send(new ViewHostMsg_MissingPluginStatus(routing_id_, status));
    }
  } else {
    // Closes the InfoBar if user clicks on the plugin (instead of the InfoBar)
    // to start the download/install.
    if (status == default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD) {
      Send(new ViewHostMsg_MissingPluginStatus(routing_id_, status));
    }
  }
#else
  // TODO(port): Implement the infobar that accompanies the default plugin.
  // Linux: http://crbug.com/10952
  // Mac: http://crbug.com/17392
  NOTIMPLEMENTED();
#endif
}

// WebKit::WebViewClient ------------------------------------------------------

WebView* RenderView::createView(
    WebFrame* creator,
    const WebWindowFeatures& features,
    const WebString& frame_name) {
  // Check to make sure we aren't overloading on popups.
  if (shared_popup_counter_->data > kMaximumNumberOfUnacknowledgedPopups)
    return NULL;

  // This window can't be closed from a window.close() call until we receive a
  // message from the Browser process explicitly allowing it.
  script_can_close_ = false;

  ViewHostMsg_CreateWindow_Params params;
  params.opener_id = routing_id_;
  params.user_gesture = creator->isProcessingUserGesture();
  params.window_container_type = WindowFeaturesToContainerType(features);
  params.session_storage_namespace_id = session_storage_namespace_id_;
  params.frame_name = frame_name;

  int32 routing_id = MSG_ROUTING_NONE;
  int64 cloned_session_storage_namespace_id;
  bool opener_suppressed = creator->willSuppressOpenerInNewFrame();

  render_thread_->Send(
      new ViewHostMsg_CreateWindow(params,
                                   &routing_id,
                                   &cloned_session_storage_namespace_id));
  if (routing_id == MSG_ROUTING_NONE)
    return NULL;

  RenderView* view = RenderView::Create(render_thread_,
                                        0,
                                        routing_id_,
                                        renderer_preferences_,
                                        webkit_preferences_,
                                        shared_popup_counter_,
                                        routing_id,
                                        cloned_session_storage_namespace_id,
                                        frame_name);
  view->opened_by_user_gesture_ = params.user_gesture;

  // Record whether the creator frame is trying to suppress the opener field.
  view->opener_suppressed_ = opener_suppressed;

  // Record the security origin of the creator.
  GURL creator_url(creator->securityOrigin().toString().utf8());
  if (!creator_url.is_valid() || !creator_url.IsStandard())
    creator_url = GURL();
  view->creator_url_ = creator_url;

  // Copy over the alternate error page URL so we can have alt error pages in
  // the new render view (we don't need the browser to send the URL back down).
  view->alternate_error_page_url_ = alternate_error_page_url_;

  return view->webview();
}

WebWidget* RenderView::createPopupMenu(WebKit::WebPopupType popup_type) {
  RenderWidget* widget = RenderWidget::Create(routing_id_,
                                              render_thread_,
                                              popup_type);
  return widget->webwidget();
}

WebWidget* RenderView::createPopupMenu(const WebPopupMenuInfo& info) {
  RenderWidget* widget = RenderWidget::Create(routing_id_,
                                              render_thread_,
                                              WebKit::WebPopupTypeSelect);
  widget->ConfigureAsExternalPopupMenu(info);
  return widget->webwidget();
}

WebWidget* RenderView::createFullscreenWindow(WebKit::WebPopupType popup_type) {
  RenderWidget* widget = RenderWidgetFullscreen::Create(routing_id_,
                                                        render_thread_,
                                                        popup_type);
  return widget->webwidget();
}

WebStorageNamespace* RenderView::createSessionStorageNamespace(unsigned quota) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return WebStorageNamespace::createSessionStorageNamespace(quota);
  CHECK(session_storage_namespace_id_ != kInvalidSessionStorageNamespaceId);
  return new RendererWebStorageNamespaceImpl(DOM_STORAGE_SESSION,
                                             session_storage_namespace_id_);
}

void RenderView::didAddMessageToConsole(
    const WebConsoleMessage& message, const WebString& source_name,
    unsigned source_line) {
  Send(new ViewHostMsg_AddMessageToConsole(routing_id_,
                                           UTF16ToWideHack(message.text),
                                           static_cast<int32>(source_line),
                                           UTF16ToWideHack(source_name)));
}

void RenderView::printPage(WebFrame* frame) {
  DCHECK(frame);
  Print(frame, true);
}

WebKit::WebNotificationPresenter* RenderView::notificationPresenter() {
  return notification_provider_.get();
}

void RenderView::didStartLoading() {
  if (is_loading_) {
    DLOG(WARNING) << "didStartLoading called while loading";
    return;
  }

  is_loading_ = true;
  // Clear the pointer so that we can assign it only when there is an unknown
  // plugin on a page.
  first_default_plugin_.reset();

  Send(new ViewHostMsg_DidStartLoading(routing_id_));
}

void RenderView::didStopLoading() {
  if (!is_loading_) {
    DLOG(WARNING) << "DidStopLoading called while not loading";
    return;
  }

  is_loading_ = false;

  // NOTE: For now we're doing the safest thing, and sending out notification
  // when done loading. This currently isn't an issue as the favicon is only
  // displayed when done loading. Ideally we would send notification when
  // finished parsing the head, but webkit doesn't support that yet.
  // The feed discovery code would also benefit from access to the head.
  GURL favicon_url(webview()->mainFrame()->favIconURL());
  if (!favicon_url.is_empty())
    Send(new ViewHostMsg_UpdateFavIconURL(routing_id_, page_id_, favicon_url));

  AddGURLSearchProvider(webview()->mainFrame()->openSearchDescriptionURL(),
                        true);  // autodetected

  Send(new ViewHostMsg_DidStopLoading(routing_id_));

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      page_info_method_factory_.NewRunnableMethod(
          &RenderView::CapturePageInfo, page_id_, false),
      send_content_state_immediately_ ? 0 : kDelayForCaptureMs);
}

bool RenderView::isSmartInsertDeleteEnabled() {
#if defined(OS_MACOSX)
  return true;
#else
  return false;
#endif
}

bool RenderView::isSelectTrailingWhitespaceEnabled() {
#if defined(OS_WIN)
  return true;
#else
  return false;
#endif
}

void RenderView::didChangeSelection(bool is_empty_selection) {
#if defined(USE_X11) || defined(OS_MACOSX)
  if (!handling_input_event_)
      return;
  // TODO(estade): investigate incremental updates to the selection so that we
  // don't send the entire selection over IPC every time.
  if (!is_empty_selection) {
    // Sometimes we get repeated didChangeSelection calls from webkit when
    // the selection hasn't actually changed. We don't want to report these
    // because it will cause us to continually claim the X clipboard.
    const std::string& this_selection =
        webview()->focusedFrame()->selectionAsText().utf8();
    if (this_selection == last_selection_)
      return;

    Send(new ViewHostMsg_SelectionChanged(routing_id_,
         this_selection));
    last_selection_ = this_selection;
  } else {
    last_selection_.clear();
    Send(new ViewHostMsg_SelectionChanged(routing_id_,
         last_selection_));
  }
#endif
}

void RenderView::didExecuteCommand(const WebString& command_name) {
  const std::string& name = UTF16ToUTF8(command_name);
  if (StartsWithASCII(name, "Move", true) ||
      StartsWithASCII(name, "Insert", true) ||
      StartsWithASCII(name, "Delete", true))
    return;
  UserMetricsRecordAction(name);
}

void RenderView::textFieldDidEndEditing(
    const WebKit::WebInputElement& element) {
#if defined(WEBKIT_BUG_41283_IS_FIXED)
  password_autocomplete_manager_->TextFieldDidEndEditing(element);
#endif
}

void RenderView::textFieldDidChange(const WebKit::WebInputElement& element) {
#if defined(WEBKIT_BUG_41283_IS_FIXED)
  // We post a task for doing the AutoFill as the caret position is not set
  // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976) and
  // it is needed to trigger autofill.
  autofill_method_factory_.RevokeAll();
  MessageLoop::current()->PostTask(
        FROM_HERE,
        autofill_method_factory_.NewRunnableMethod(
            &RenderView::TextFieldDidChangeImpl, element));
#endif
}

void RenderView::TextFieldDidChangeImpl(
    const WebKit::WebInputElement& element) {
  if (password_autocomplete_manager_->TextDidChangeInTextField(element))
    return;
  autofill_helper_->TextDidChangeInTextField(element);
}

void RenderView::textFieldDidReceiveKeyDown(
    const WebKit::WebInputElement& element,
    const WebKit::WebKeyboardEvent& event) {
#if defined(WEBKIT_BUG_41283_IS_FIXED)
  password_autocomplete_manager_->TextFieldHandlingKeyDown(element, event);
#endif
}

bool RenderView::handleCurrentKeyboardEvent() {
  if (edit_commands_.empty())
    return false;

  WebFrame* frame = webview()->focusedFrame();
  if (!frame)
    return false;

  EditCommands::iterator it = edit_commands_.begin();
  EditCommands::iterator end = edit_commands_.end();

  bool did_execute_command = false;
  for (; it != end; ++it) {
    // In gtk and cocoa, it's possible to bind multiple edit commands to one
    // key (but it's the exception). Once one edit command is not executed, it
    // seems safest to not execute the rest.
    if (!frame->executeCommand(WebString::fromUTF8(it->name),
                               WebString::fromUTF8(it->value)))
      break;
    did_execute_command = true;
  }

  return did_execute_command;
}

void RenderView::inputElementClicked(const WebKit::WebInputElement& element,
                                     bool already_focused) {
#if defined(WEBKIT_BUG_41283_IS_FIXED)
  if (password_autocomplete_manager_->InputElementClicked(element,
                                                          already_focused)) {
    return;
  }
  autofill_helper_->InputElementClicked(element, already_focused);
#endif
}

void RenderView::spellCheck(const WebString& text,
                            int& misspelled_offset,
                            int& misspelled_length) {
  EnsureDocumentTag();

  string16 word(text);
  RenderThread* thread = RenderThread::current();
  // Will be NULL during unit tests.
  if (thread) {
    thread->spellchecker()->SpellCheckWord(
        word.c_str(), word.size(), document_tag_,
        &misspelled_offset, &misspelled_length, NULL);
  }
}

WebString RenderView::autoCorrectWord(const WebKit::WebString& word) {
  string16 autocorrect_word;
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kExperimentalSpellcheckerFeatures)) {
    EnsureDocumentTag();
    RenderThread* thread = RenderThread::current();
    // Will be NULL during unit tests.
    if (thread) {
      autocorrect_word =
          thread->spellchecker()->GetAutoCorrectionWord(
              word, document_tag_);
    }
  }
  return autocorrect_word;
}

void RenderView::showSpellingUI(bool show) {
  Send(new ViewHostMsg_ShowSpellingPanel(routing_id_, show));
}

bool RenderView::isShowingSpellingUI() {
  return spelling_panel_visible_;
}

void RenderView::updateSpellingUIWithMisspelledWord(const WebString& word) {
  Send(new ViewHostMsg_UpdateSpellingPanelWithMisspelledWord(routing_id_,
                                                             word));
}

bool RenderView::runFileChooser(
    const WebKit::WebFileChooserParams& params,
    WebFileChooserCompletion* chooser_completion) {
  ViewHostMsg_RunFileChooser_Params ipc_params;
  if (params.directory)
    ipc_params.mode = ViewHostMsg_RunFileChooser_Params::OpenFolder;
  else if (params.multiSelect)
    ipc_params.mode = ViewHostMsg_RunFileChooser_Params::OpenMultiple;
  else
    ipc_params.mode = ViewHostMsg_RunFileChooser_Params::Open;
  ipc_params.title = params.title;
  ipc_params.default_file_name =
      webkit_glue::WebStringToFilePath(params.initialValue);
  ipc_params.accept_types = params.acceptTypes;

  return ScheduleFileChooser(ipc_params, chooser_completion);
}

void RenderView::runModalAlertDialog(
    WebFrame* frame, const WebString& message) {
  RunJavaScriptMessage(MessageBoxFlags::kIsJavascriptAlert,
                       UTF16ToWideHack(message),
                       std::wstring(),
                       frame->url(),
                       NULL);
}

bool RenderView::runModalConfirmDialog(
    WebFrame* frame, const WebString& message) {
  return RunJavaScriptMessage(MessageBoxFlags::kIsJavascriptConfirm,
                              UTF16ToWideHack(message),
                              std::wstring(),
                              frame->url(),
                              NULL);
}

bool RenderView::runModalPromptDialog(
    WebFrame* frame, const WebString& message, const WebString& default_value,
    WebString* actual_value) {
  std::wstring result;
  bool ok = RunJavaScriptMessage(MessageBoxFlags::kIsJavascriptPrompt,
                                 UTF16ToWideHack(message),
                                 UTF16ToWideHack(default_value),
                                 frame->url(),
                                 &result);
  if (ok)
    actual_value->assign(WideToUTF16Hack(result));
  return ok;
}

bool RenderView::runModalBeforeUnloadDialog(
    WebFrame* frame, const WebString& message) {
  bool success = false;
  // This is an ignored return value, but is included so we can accept the same
  // response as RunJavaScriptMessage.
  std::wstring ignored_result;
  SendAndRunNestedMessageLoop(new ViewHostMsg_RunBeforeUnloadConfirm(
      routing_id_, frame->url(), UTF16ToWideHack(message), &success,
      &ignored_result));
  return success;
}

void RenderView::showContextMenu(
    WebFrame* frame, const WebContextMenuData& data) {
  ContextMenuParams params = ContextMenuParams(data);
  if (!params.misspelled_word.empty() && RenderThread::current()) {
    int misspelled_offset, misspelled_length;
    bool spelled_right = RenderThread::current()->spellchecker()->
        SpellCheckWord(
            params.misspelled_word.c_str(), params.misspelled_word.size(),
            document_tag_,
            &misspelled_offset, &misspelled_length,
            &params.dictionary_suggestions);
    if (spelled_right)
      params.misspelled_word.clear();
  }
  // Serializing a GURL longer than chrome::kMaxURLChars will fail, so don't do
  // it.  We replace it with an empty GURL so the appropriate items are disabled
  // in the context menu.
  // TODO(jcivelli): http://crbug.com/45160 This prevents us from saving large
  //                 data encoded images.  We should have a way to save them.
  if (params.src_url.spec().size() > chrome::kMaxURLChars)
    params.src_url = GURL();
  Send(new ViewHostMsg_ContextMenu(routing_id_, params));
}

bool RenderView::supportsFullscreen() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVideoFullscreen);
}

void RenderView::enterFullscreenForNode(const WebKit::WebNode& node) {
  NOTIMPLEMENTED();
}

void RenderView::exitFullscreenForNode(const WebKit::WebNode& node) {
  NOTIMPLEMENTED();
}

void RenderView::setStatusText(const WebString& text) {
}

void RenderView::UpdateTargetURL(const GURL& url, const GURL& fallback_url) {
  GURL latest_url = url.is_empty() ? fallback_url : url;
  if (latest_url == target_url_)
    return;

  // Tell the browser to display a destination link.
  if (target_url_status_ == TARGET_INFLIGHT ||
      target_url_status_ == TARGET_PENDING) {
    // If we have a request in-flight, save the URL to be sent when we
    // receive an ACK to the in-flight request. We can happily overwrite
    // any existing pending sends.
    pending_target_url_ = latest_url;
    target_url_status_ = TARGET_PENDING;
  } else {
    Send(new ViewHostMsg_UpdateTargetURL(routing_id_, page_id_, latest_url));
    target_url_ = latest_url;
    target_url_status_ = TARGET_INFLIGHT;
  }
}

void RenderView::StartNavStateSyncTimerIfNecessary() {
  int delay;
  if (send_content_state_immediately_)
    delay = 0;
  else if (is_hidden())
    delay = kDelaySecondsForContentStateSyncHidden;
  else
    delay = kDelaySecondsForContentStateSync;

  if (nav_state_sync_timer_.IsRunning()) {
    // The timer is already running. If the delay of the timer maches the amount
    // we want to delay by, then return. Otherwise stop the timer so that it
    // gets started with the right delay.
    if (nav_state_sync_timer_.GetCurrentDelay().InSeconds() == delay)
      return;
    nav_state_sync_timer_.Stop();
  }

  nav_state_sync_timer_.Start(
      TimeDelta::FromSeconds(delay), this, &RenderView::SyncNavigationState);
}

void RenderView::setMouseOverURL(const WebURL& url) {
  mouse_over_url_ = GURL(url);
  UpdateTargetURL(mouse_over_url_, focus_url_);
}

void RenderView::setKeyboardFocusURL(const WebURL& url) {
  focus_url_ = GURL(url);
  UpdateTargetURL(focus_url_, mouse_over_url_);
}

void RenderView::setToolTipText(const WebString& text, WebTextDirection hint) {
  Send(new ViewHostMsg_SetTooltipText(routing_id_, UTF16ToWideHack(text),
                                      hint));
}

void RenderView::startDragging(const WebDragData& data,
                               WebDragOperationsMask mask,
                               const WebImage& image,
                               const WebPoint& imageOffset) {
#if WEBKIT_USING_SKIA
  SkBitmap bitmap(image.getSkBitmap());
#elif WEBKIT_USING_CG
  SkBitmap bitmap = gfx::CGImageToSkBitmap(image.getCGImageRef());
#endif

  Send(new ViewHostMsg_StartDragging(routing_id_,
                                     WebDropData(data),
                                     mask,
                                     bitmap,
                                     imageOffset));
}

bool RenderView::acceptsLoadDrops() {
  return renderer_preferences_.can_accept_load_drops;
}

void RenderView::focusNext() {
  Send(new ViewHostMsg_TakeFocus(routing_id_, false));
}

void RenderView::focusPrevious() {
  Send(new ViewHostMsg_TakeFocus(routing_id_, true));
}

void RenderView::focusedNodeChanged(const WebNode& node) {
  Send(new ViewHostMsg_FocusedNodeChanged(routing_id_));
}

void RenderView::navigateBackForwardSoon(int offset) {
  Send(new ViewHostMsg_GoToEntryAtOffset(routing_id_, offset));
}

int RenderView::historyBackListCount() {
  return history_list_offset_ < 0 ? 0 : history_list_offset_;
}

int RenderView::historyForwardListCount() {
  return history_list_length_ - historyBackListCount() - 1;
}

void RenderView::didUpdateInspectorSetting(const WebString& key,
                                           const WebString& value) {
  Send(new ViewHostMsg_UpdateInspectorSetting(routing_id_,
                                              key.utf8(),
                                              value.utf8()));
}

void RenderView::queryAutofillSuggestions(const WebNode& node,
                                          const WebString& name,
                                          const WebString& value) {
  autofill_helper_->QueryAutoFillSuggestions(node, name, value);
}

void RenderView::removeAutofillSuggestions(const WebString& name,
                                           const WebString& value) {
  autofill_helper_->RemoveAutocompleteSuggestion(name, value);
}

void RenderView::didAcceptAutoFillSuggestion(const WebKit::WebNode& node,
                                             const WebKit::WebString& value,
                                             const WebKit::WebString& label,
                                             int unique_id,
                                             unsigned index) {
  autofill_helper_->DidAcceptAutoFillSuggestion(node, value, label, unique_id,
                                                index);
}

void RenderView::didSelectAutoFillSuggestion(const WebKit::WebNode& node,
                                             const WebKit::WebString& value,
                                             const WebKit::WebString& label,
                                             int unique_id) {
  autofill_helper_->DidSelectAutoFillSuggestion(node, value, label, unique_id);
}

void RenderView::didClearAutoFillSelection(const WebKit::WebNode& node) {
  autofill_helper_->DidClearAutoFillSelection(node);
}

void RenderView::didAcceptAutocompleteSuggestion(
    const WebKit::WebInputElement& user_element) {
#if defined(WEBKIT_BUG_41283_IS_FIXED)
  bool result = password_autocomplete_manager_->FillPassword(user_element);
  // Since this user name was selected from a suggestion list, we should always
  // have password for it.
  DCHECK(result);
#endif
}

// WebKit::WebWidgetClient ----------------------------------------------------

void RenderView::didFocus() {
  // TODO(jcivelli): when https://bugs.webkit.org/show_bug.cgi?id=33389 is fixed
  //                 we won't have to test for user gesture anymore and we can
  //                 move that code back to render_widget.cc
  if (webview() && webview()->mainFrame() &&
      webview()->mainFrame()->isProcessingUserGesture()) {
    Send(new ViewHostMsg_Focus(routing_id_));
  }
}

void RenderView::didBlur() {
  // TODO(jcivelli): see TODO above in didFocus().
  if (webview() && webview()->mainFrame() &&
      webview()->mainFrame()->isProcessingUserGesture()) {
    Send(new ViewHostMsg_Blur(routing_id_));
  }
}

// We are supposed to get a single call to Show for a newly created RenderView
// that was created via RenderView::CreateWebView.  So, we wait until this
// point to dispatch the ShowView message.
//
// This method provides us with the information about how to display the newly
// created RenderView (i.e., as a constrained popup or as a new tab).
//
void RenderView::show(WebNavigationPolicy policy) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  if (did_show_)
    return;
  did_show_ = true;

  // Extensions and apps always allowed to create unrequested popups. The second
  // check is necessary to include content scripts.
  if (ExtensionRendererInfo::GetByURL(creator_url_) ||
      bindings_utils::GetInfoForCurrentContext()) {
    opened_by_user_gesture_ = true;
  }

  // Force new windows to a popup if they were not opened with a user gesture.
  if (!opened_by_user_gesture_) {
    // We exempt background tabs for compat with older versions of Chrome.
    // TODO(darin): This seems bogus.  These should have a user gesture, so
    // we probably don't need this check.
    if (policy != WebKit::WebNavigationPolicyNewBackgroundTab)
      policy = WebKit::WebNavigationPolicyNewPopup;
  }

  // NOTE: initial_pos_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if disposition is not NEW_POPUP, or the
  // browser process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowView(opener_id_, routing_id_,
      NavigationPolicyToDisposition(policy), initial_pos_,
      opened_by_user_gesture_));
  SetPendingWindowRect(initial_pos_);
}

void RenderView::closeWidgetSoon() {
  // Cancel pending translations so that the translate_helper_ does not attempt
  // to access the WebView.
  translate_helper_.CancelPendingTranslation();

  if (script_can_close_)
    RenderWidget::closeWidgetSoon();
}

void RenderView::runModal() {
  DCHECK(did_show_) << "should already have shown the view";

  // We must keep WebKit's shared timer running in this case in order to allow
  // showModalDialog to function properly.
  //
  // TODO(darin): WebKit should really be smarter about suppressing events and
  // timers so that we do not need to manage the shared timer in such a heavy
  // handed manner.
  //
  if (RenderThread::current())  // Will be NULL during unit tests.
    RenderThread::current()->DoNotSuspendWebKitSharedTimer();

  SendAndRunNestedMessageLoop(new ViewHostMsg_RunModal(routing_id_));
}

// WebKit::WebFrameClient -----------------------------------------------------

WebPlugin* RenderView::createPlugin(WebFrame* frame,
                                    const WebPluginParams& params) {
  bool found = false;
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  WebPluginInfo info;
  GURL url(params.url);
  std::string actual_mime_type;
  Send(new ViewHostMsg_GetPluginInfo(url,
                                     frame->top()->url(),
                                     params.mimeType.utf8(),
                                     &found,
                                     &info,
                                     &setting,
                                     &actual_mime_type));

  if (!found)
    return NULL;

  scoped_ptr<PluginGroup> group(PluginGroup::FindHardcodedPluginGroup(info));
  group->AddPlugin(info, 0);

  if (!info.enabled) {
    if (cmd->HasSwitch(switches::kDisableOutdatedPlugins) &&
        group->IsVulnerable()) {
      Send(new ViewHostMsg_DisabledOutdatedPlugin(routing_id_,
                                                  group->GetGroupName(),
                                                  GURL(group->GetUpdateURL())));
      return CreatePluginPlaceholder(frame, params, group.get());
    }
    return NULL;
  }

  if (info.path.value() != kDefaultPluginLibraryName) {
    std::string resource;
    if (cmd->HasSwitch(switches::kEnableResourceContentSettings)) {
#if defined(OS_POSIX)
      resource = info.path.value();
#elif defined(OS_WIN)
      resource = base::SysWideToUTF8(info.path.value());
#endif
    }
    if (setting == CONTENT_SETTING_BLOCK) {
      DCHECK(!cmd->HasSwitch(switches::kDisableClickToPlay));
      DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS, resource);
      return CreatePluginPlaceholder(frame, params, NULL);
    }
    scoped_refptr<pepper::PluginModule> pepper_module =
        PepperPluginRegistry::GetInstance()->GetModule(info.path);
    if (pepper_module)
      return CreatePepperPlugin(frame, params, info.path, pepper_module.get());
    if (setting != CONTENT_SETTING_ALLOW) {
      // If the host is not whitelisted for this plugin, block it.
      Send(new ViewHostMsg_NonSandboxedPluginBlocked(routing_id_,
                                                     resource,
                                                     group->GetGroupName()));
      return CreatePluginPlaceholder(frame, params, NULL);
    }
  }
  return CreateNPAPIPlugin(frame, params, info.path, actual_mime_type);
}

WebWorker* RenderView::createWorker(WebFrame* frame, WebWorkerClient* client) {
  WebApplicationCacheHostImpl* appcache_host =
      WebApplicationCacheHostImpl::FromFrame(frame);
  int appcache_host_id = appcache_host ? appcache_host->host_id() : 0;
  return new WebWorkerProxy(client, RenderThread::current(), routing_id_,
                            appcache_host_id);
}

WebSharedWorker* RenderView::createSharedWorker(
    WebFrame* frame, const WebURL& url, const WebString& name,
    unsigned long long document_id) {

  int route_id = MSG_ROUTING_NONE;
  bool exists = false;
  bool url_mismatch = false;
  ViewHostMsg_CreateWorker_Params params;
  params.url = url;
  params.is_shared = true;
  params.name = name;
  params.document_id = document_id;
  params.render_view_route_id = routing_id_;
  params.route_id = MSG_ROUTING_NONE;
  params.parent_appcache_host_id = 0;
  params.script_resource_appcache_id = 0;
  Send(new ViewHostMsg_LookupSharedWorker(
      params, &exists, &route_id, &url_mismatch));
  if (url_mismatch) {
    return NULL;
  } else {
    return new WebSharedWorkerProxy(RenderThread::current(),
                                    document_id,
                                    exists,
                                    route_id,
                                    routing_id_);
  }
}

WebMediaPlayer* RenderView::createMediaPlayer(
    WebFrame* frame, WebMediaPlayerClient* client) {
  scoped_refptr<media::FilterFactoryCollection> factory =
      new media::FilterFactoryCollection();
  // Add in any custom filter factories first.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kDisableAudio)) {
    // Add the chrome specific audio renderer.
    factory->AddFactory(
        AudioRendererImpl::CreateFactory(audio_message_filter()));
  }

  // TODO(hclam): Need to inject Gles2VideoDecodeContext here. Also I need
  // to create a factory for FFmpegVideoDecoder here so that it can use
  // the Gles2VideoDecodeContext.
  if (cmd_line->HasSwitch(switches::kEnableAcceleratedDecoding) &&
      !cmd_line->HasSwitch(switches::kDisableAcceleratedCompositing)) {
    // Add the hardware video decoder factory.
    factory->AddFactory(IpcVideoDecoder::CreateFactory(
        MessageLoop::current(),
        reinterpret_cast<WebGLES2ContextImpl*>(
            webview()->gles2Context())->context()));
  }

  WebApplicationCacheHostImpl* appcache_host =
      WebApplicationCacheHostImpl::FromFrame(frame);

  // TODO(hclam): obtain the following parameters from |client|.
  webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory =
      new webkit_glue::MediaResourceLoaderBridgeFactory(
          GURL(),  // referrer
          "null",  // frame origin
          "null",  // main_frame_origin
          base::GetCurrentProcId(),
          appcache_host ? appcache_host->host_id() : appcache::kNoHostId,
          routing_id());

  // A simple data source that keeps all data in memory.
  media::FilterFactory* simple_data_source_factory =
      webkit_glue::SimpleDataSource::CreateFactory(MessageLoop::current(),
                                                   bridge_factory);
  // A sophisticated data source that does memory caching.
  media::FilterFactory* buffered_data_source_factory =
      webkit_glue::BufferedDataSource::CreateFactory(MessageLoop::current(),
                                                     bridge_factory);
  if (cmd_line->HasSwitch(switches::kSimpleDataSource)) {
    factory->AddFactory(simple_data_source_factory);
    factory->AddFactory(buffered_data_source_factory);
  } else {
    factory->AddFactory(buffered_data_source_factory);
    factory->AddFactory(simple_data_source_factory);
  }

  webkit_glue::WebVideoRendererFactoryFactory* factory_factory = NULL;
  if (cmd_line->HasSwitch(switches::kEnableVideoLayering)) {
    factory_factory = new IPCVideoRenderer::FactoryFactory(routing_id_);
  } else {
    bool pts_logging = cmd_line->HasSwitch(switches::kEnableVideoLogging);
    factory_factory =
        new webkit_glue::VideoRendererImpl::FactoryFactory(pts_logging);
  }

  return new webkit_glue::WebMediaPlayerImpl(client, factory, factory_factory);
}

WebApplicationCacheHost* RenderView::createApplicationCacheHost(
    WebFrame* frame, WebApplicationCacheHostClient* client) {
  return new RendererWebApplicationCacheHostImpl(
      FromWebView(frame->view()), client,
      RenderThread::current()->appcache_dispatcher()->backend_proxy());
}

WebCookieJar* RenderView::cookieJar() {
  return &cookie_jar_;
}

void RenderView::frameDetached(WebFrame* frame) {
  autofill_helper_->FrameDetached(frame);
  page_click_tracker_->StopTrackingFrame(frame, true);
}

void RenderView::willClose(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);

  if (!frame->parent()) {
    const GURL& url = frame->url();
    // Ensure state contains scheme.
    if (url.SchemeIs("http"))
      navigation_state->set_scheme_type(URLPattern::SCHEME_HTTP);
    else if (url.SchemeIs("https"))
      navigation_state->set_scheme_type(URLPattern::SCHEME_HTTPS);

    // Dump will only be provided when scheme is http or https.
    DumpLoadHistograms();
  }

  navigation_state->user_script_idle_scheduler()->Cancel();

  // TODO(jhawkins): Remove once frameDetached is called by WebKit.
  autofill_helper_->FrameWillClose(frame);
}

bool RenderView::allowImages(WebFrame* frame, bool enabled_per_settings) {
  if (enabled_per_settings &&
      AllowContentType(CONTENT_SETTINGS_TYPE_IMAGES))
    return true;

  if (IsWhitelistedForContentSettings(frame))
    return true;

  DidBlockContentType(CONTENT_SETTINGS_TYPE_IMAGES, std::string());
  return false;  // Other protocols fall through here.
}

bool RenderView::allowPlugins(WebFrame* frame, bool enabled_per_settings) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableClickToPlay)) {
    return WebFrameClient::allowPlugins(frame, enabled_per_settings);
  }
  return enabled_per_settings &&
      AllowContentType(CONTENT_SETTINGS_TYPE_PLUGINS);
}


void RenderView::loadURLExternally(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationPolicy policy) {
  GURL referrer(request.httpHeaderField(WebString::fromUTF8("Referer")));
  if (policy == WebKit::WebNavigationPolicyDownload) {
    Send(new ViewHostMsg_DownloadUrl(routing_id_, request.url(), referrer));
  } else {
    OpenURL(request.url(), referrer, policy);
  }
}

WebNavigationPolicy RenderView::decidePolicyForNavigation(
    WebFrame* frame, const WebURLRequest& request, WebNavigationType type,
    const WebNode&, WebNavigationPolicy default_policy, bool is_redirect) {
  // Webkit is asking whether to navigate to a new URL.
  // This is fine normally, except if we're showing UI from one security
  // context and they're trying to navigate to a different context.
  const GURL& url = request.url();

  // If the browser is interested, then give it a chance to look at top level
  // navigations.
  if (renderer_preferences_.browser_handles_top_level_requests &&
      IsNonLocalTopLevelNavigation(url, frame, type)) {
    GURL referrer(request.httpHeaderField(WebString::fromUTF8("Referer")));
    // Reset these counters as the RenderView could be reused for the next
    // navigation.
    page_id_ = -1;
    next_page_id_ = 1;
    last_page_id_sent_to_browser_ = -1;
    OpenURL(url, referrer, default_policy);
    return WebKit::WebNavigationPolicyIgnore;  // Suppress the load here.
  }

  // A content initiated navigation may have originated from a link-click,
  // script, drag-n-drop operation, etc.
  bool is_content_initiated =
      NavigationState::FromDataSource(frame->provisionalDataSource())->
          is_content_initiated();
  GURL old_url(frame->url());

  // We only care about navigations that are within the current tab (as opposed
  // to, for example, opening a new window).
  // But we sometimes navigate to about:blank to clear a tab, and we want to
  // still allow that.
  if (default_policy == WebKit::WebNavigationPolicyCurrentTab &&
      is_content_initiated && frame->parent() == NULL &&
      type != WebKit::WebNavigationTypeFormSubmitted &&
      !url.SchemeIs(chrome::kAboutScheme)) {
    // When we received such unsolicited navigations, we sometimes want to
    // punt them up to the browser to handle.
    if (BindingsPolicy::is_dom_ui_enabled(enabled_bindings_) ||
        frame->isViewSourceModeEnabled() ||
        url.SchemeIs(chrome::kViewSourceScheme)) {
      // We don't send referrer from these special pages.
      OpenURL(url, GURL(), default_policy);
      return WebKit::WebNavigationPolicyIgnore;  // Suppress the load here.
    }

    // We forward non-local navigations from extensions to the browser if they
    // are top-level events, even if the browser hasn't expressed interest.
    // TODO(erikkay) crbug.com/54118 - combine this clause and the next into
    // some shared logic.
    if (BindingsPolicy::is_extension_enabled(enabled_bindings_) &&
        old_url.SchemeIs(chrome::kExtensionScheme) &&
        IsNonLocalTopLevelNavigation(url, frame, type)) {
        // We don't send referrer from extensions.
        OpenURL(url, GURL(), default_policy);
        return WebKit::WebNavigationPolicyIgnore;  // Suppress the load here.
    }

    // If the navigation would cross an app extent boundary, we also need
    // to defer to the browser to ensure process isolation.
    // TODO(erikkay) This is happening inside of a check to is_content_initiated
    // which means that things like the back button won't trigger it.  Is that
    // OK?
    if (CrossesExtensionExtents(frame, url)) {
      // Include the referrer in this case since we're going from a hosted web
      // page. (the packaged case is handled previously by the extension
      // navigation test)
      GURL referrer(request.httpHeaderField(WebString::fromUTF8("Referer")));
      OpenURL(url, referrer, default_policy);
      return WebKit::WebNavigationPolicyIgnore;  // Suppress the load here.
    }
  }

  // Detect when a page is "forking" a new tab that can be safely rendered in
  // its own process.  This is done by sites like Gmail that try to open links
  // in new windows without script connections back to the original page.  We
  // treat such cases as browser navigations (in which we will create a new
  // renderer for a cross-site navigation), rather than WebKit navigations.
  //
  // We use the following heuristic to decide whether to fork a new page in its
  // own process:
  // The parent page must open a new tab to about:blank, set the new tab's
  // window.opener to null, and then redirect the tab to a cross-site URL using
  // JavaScript.
  //
  // TODO(creis): Deprecate this logic once we can rely on rel=noreferrer
  // (see below).
  bool is_fork =
      // Must start from a tab showing about:blank, which is later redirected.
      old_url == GURL(chrome::kAboutBlankURL) &&
      // Must be the first real navigation of the tab.
      historyBackListCount() < 1 &&
      historyForwardListCount() < 1 &&
      // The parent page must have set the child's window.opener to null before
      // redirecting to the desired URL.
      frame->opener() == NULL &&
      // Must be a top-level frame.
      frame->parent() == NULL &&
      // Must not have issued the request from this page.
      is_content_initiated &&
      // Must be targeted at the current tab.
      default_policy == WebKit::WebNavigationPolicyCurrentTab &&
      // Must be a JavaScript navigation, which appears as "other".
      type == WebKit::WebNavigationTypeOther;

  // Recognize if this navigation is from a link with rel=noreferrer and
  // target=_blank attributes, in which case the opener will be suppressed. If
  // so, it is safe to load cross-site pages in a separate process, so we
  // should let the browser handle it.
  bool is_noreferrer_and_blank_target =
      // Frame should be top level and not yet navigated.
      frame->parent() == NULL &&
      frame->url().isEmpty() &&
      historyBackListCount() < 1 &&
      historyForwardListCount() < 1 &&
      // Links with rel=noreferrer will have no Referer field, and their
      // resulting frame will have its window.opener suppressed.
      // TODO(creis): should add a request.httpReferrer() method to help avoid
      // typos on the unusual spelling of Referer.
      request.httpHeaderField(WebString::fromUTF8("Referer")).isNull() &&
      opener_suppressed_ &&
      frame->opener() == NULL &&
      // Links with target=_blank will have no name.
      frame->name().isNull() &&
      // Another frame (with a non-empty creator) should have initiated the
      // request, targeted at this frame.
      !creator_url_.is_empty() &&
      is_content_initiated &&
      default_policy == WebKit::WebNavigationPolicyCurrentTab &&
      type == WebKit::WebNavigationTypeOther;

  if (is_fork || is_noreferrer_and_blank_target) {
    // Open the URL via the browser, not via WebKit.
    OpenURL(url, GURL(), default_policy);
    return WebKit::WebNavigationPolicyIgnore;
  }

  return default_policy;
}

bool RenderView::canHandleRequest(
    WebFrame* frame, const WebURLRequest& request) {
  // We allow WebKit to think that everything can be handled even though
  // browser-side we limit what we load.
  return true;
}

WebURLError RenderView::cannotHandleRequestError(
    WebFrame* frame, const WebURLRequest& request) {
  NOTREACHED();  // Since we said we can handle all requests.
  return WebURLError();
}

WebURLError RenderView::cancelledError(
    WebFrame* frame, const WebURLRequest& request) {
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = request.url();
  return error;
}

void RenderView::unableToImplementPolicyWithError(
    WebFrame*, const WebURLError&) {
  NOTREACHED();  // Since we said we can handle all requests.
}

void RenderView::willSendSubmitEvent(WebKit::WebFrame* frame,
    const WebKit::WebFormElement& form) {
  // Some login forms have onSubmit handlers that put a hash of the password
  // into a hidden field and then clear the password. (Issue 28910.)
  // This method gets called before any of those handlers run, so save away
  // a copy of the password in case it gets lost.
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->dataSource());
  navigation_state->set_password_form_data(
      PasswordFormDomManager::CreatePasswordForm(form));
}

void RenderView::willSubmitForm(WebFrame* frame, const WebFormElement& form) {
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->provisionalDataSource());

  if (navigation_state->transition_type() == PageTransition::LINK)
    navigation_state->set_transition_type(PageTransition::FORM_SUBMIT);

  // Save these to be processed when the ensuing navigation is committed.
  WebSearchableFormData web_searchable_form_data(form);
  navigation_state->set_searchable_form_url(web_searchable_form_data.url());
  navigation_state->set_searchable_form_encoding(
      web_searchable_form_data.encoding().utf8());
  PasswordForm* password_form_data =
      PasswordFormDomManager::CreatePasswordForm(form);
  navigation_state->set_password_form_data(password_form_data);

  // If the password has been cleared, recover it from the form contents already
  // stored by willSendSubmitEvent into the dataSource's NavigationState (as
  // opposed to the provisionalDataSource's, which is what we're storing into
  // now.)
  if (password_form_data && password_form_data->password_value.empty()) {
    NavigationState* old_navigation_state =
        NavigationState::FromDataSource(frame->dataSource());
    if (old_navigation_state) {
      PasswordForm* old_form_data = old_navigation_state->password_form_data();
      if (old_form_data && old_form_data->action == password_form_data->action)
        password_form_data->password_value = old_form_data->password_value;
    }
  }

  FormData form_data;
  if (FormManager::WebFormElementToFormData(
          form, FormManager::REQUIRE_AUTOCOMPLETE, true, &form_data))
    Send(new ViewHostMsg_FormSubmitted(routing_id_, form_data));
}

void RenderView::willPerformClientRedirect(
    WebFrame* frame, const WebURL& from, const WebURL& to, double interval,
    double fire_time) {
  // Ignore
}

void RenderView::didCancelClientRedirect(WebFrame* frame) {
  // Ignore
}

void RenderView::didCompleteClientRedirect(
    WebFrame* frame, const WebURL& from) {
  if (!frame->parent())
    completed_client_redirect_src_ = from;
}

void RenderView::didCreateDataSource(WebFrame* frame, WebDataSource* ds) {
  // The rest of RenderView assumes that a WebDataSource will always have a
  // non-null NavigationState.
  bool content_initiated = !pending_navigation_state_.get();
  NavigationState* state = content_initiated ?
      NavigationState::CreateContentInitiated() :
      pending_navigation_state_.release();

  // NavigationState::referred_by_prefetcher_ is true if we are
  // navigating from a page that used prefetching using a link on that
  // page.  We are early enough in the request process here that we
  // can still see the NavigationState of the previous page and set
  // this value appropriately.
  // TODO(gavinp): catch the important case of navigation in a new
  // renderer process.
  if (webview()) {
    if (WebFrame* old_frame = webview()->mainFrame()) {
      const WebURLRequest& original_request = ds->originalRequest();
      const GURL referrer(
          original_request.httpHeaderField(WebString::fromUTF8("Referer")));
      if (!referrer.is_empty() &&
          NavigationState::FromDataSource(
              old_frame->dataSource())->was_prefetcher()) {
        for (;old_frame;old_frame = old_frame->traverseNext(false)) {
          WebDataSource* old_frame_ds = old_frame->dataSource();
          if (old_frame_ds && referrer == GURL(old_frame_ds->request().url())) {
            state->set_was_referred_by_prefetcher(true);
            break;
          }
        }
      }
    }
  }

  if (content_initiated) {
    const WebURLRequest& request = ds->request();
    switch (request.cachePolicy()) {
      case WebURLRequest::UseProtocolCachePolicy:  // normal load.
        state->set_load_type(NavigationState::LINK_LOAD_NORMAL);
        break;
      case WebURLRequest::ReloadIgnoringCacheData:  // reload.
        state->set_load_type(NavigationState::LINK_LOAD_RELOAD);
        break;
      case WebURLRequest::ReturnCacheDataElseLoad:  // allow stale data.
        state->set_load_type(NavigationState::LINK_LOAD_CACHE_STALE_OK);
        break;
      case WebURLRequest::ReturnCacheDataDontLoad:  // Don't re-post.
        state->set_load_type(NavigationState::LINK_LOAD_CACHE_ONLY);
        break;
    }
  }

  state->set_user_script_idle_scheduler(
      new UserScriptIdleScheduler(this, frame));
  ds->setExtraData(state);
}

void RenderView::didStartProvisionalLoad(WebFrame* frame) {
  WebDataSource* ds = frame->provisionalDataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);

  // Update the request time if WebKit has better knowledge of it.
  if (navigation_state->request_time().is_null()) {
    double event_time = ds->triggeringEventTime();
    if (event_time != 0.0)
      navigation_state->set_request_time(Time::FromDoubleT(event_time));
  }

  // Start time is only set after request time.
  navigation_state->set_start_load_time(Time::Now());

  bool is_top_most = !frame->parent();
  if (is_top_most) {
    navigation_gesture_ = frame->isProcessingUserGesture() ?
        NavigationGestureUnknown : NavigationGestureAuto;

    // Make sure redirect tracking state is clear for the new load.
    completed_client_redirect_src_ = GURL();
  } else if (frame->parent()->isLoading()) {
    // Take note of AUTO_SUBFRAME loads here, so that we can know how to
    // load an error page.  See didFailProvisionalLoad.
    navigation_state->set_transition_type(PageTransition::AUTO_SUBFRAME);
  }

  Send(new ViewHostMsg_DidStartProvisionalLoadForFrame(
       routing_id_, is_top_most, ds->request().url()));
}

void RenderView::didReceiveServerRedirectForProvisionalLoad(WebFrame* frame) {
  if (frame->parent())
    return;
  // Received a redirect on the main frame.
  WebDataSource* data_source = frame->provisionalDataSource();
  if (!data_source) {
    // Should only be invoked when we have a data source.
    NOTREACHED();
    return;
  }
  std::vector<GURL> redirects;
  GetRedirectChain(data_source, &redirects);
  if (redirects.size() >= 2) {
    Send(new ViewHostMsg_DidRedirectProvisionalLoad(routing_id_, page_id_,
        redirects[redirects.size() - 2], redirects.back()));
  }
}

void RenderView::didFailProvisionalLoad(WebFrame* frame,
                                        const WebURLError& error) {
  // Notify the browser that we failed a provisional load with an error.
  //
  // Note: It is important this notification occur before DidStopLoading so the
  //       SSL manager can react to the provisional load failure before being
  //       notified the load stopped.
  //
  WebDataSource* ds = frame->provisionalDataSource();
  DCHECK(ds);

  const WebURLRequest& failed_request = ds->request();

  bool show_repost_interstitial =
      (error.reason == net::ERR_CACHE_MISS &&
       EqualsASCII(failed_request.httpMethod(), "POST"));
  Send(new ViewHostMsg_DidFailProvisionalLoadWithError(
      routing_id_, !frame->parent(), error.reason, error.unreachableURL,
      show_repost_interstitial));

  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, WebCore doesn't expect it and it will cause a crash.
  if (error.reason == net::ERR_ABORTED)
    return;

  // Make sure we never show errors in view source mode.
  frame->enableViewSourceMode(false);

  NavigationState* navigation_state = NavigationState::FromDataSource(ds);

  // If this is a failed back/forward/reload navigation, then we need to do a
  // 'replace' load.  This is necessary to avoid messing up session history.
  // Otherwise, we do a normal load, which simulates a 'go' navigation as far
  // as session history is concerned.
  //
  // AUTO_SUBFRAME loads should always be treated as loads that do not advance
  // the page id.
  //
  bool replace =
      navigation_state->pending_page_id() != -1 ||
      navigation_state->transition_type() == PageTransition::AUTO_SUBFRAME;

  // If we failed on a browser initiated request, then make sure that our error
  // page load is regarded as the same browser initiated request.
  if (!navigation_state->is_content_initiated()) {
    pending_navigation_state_.reset(NavigationState::CreateBrowserInitiated(
        navigation_state->pending_page_id(),
        navigation_state->pending_history_list_offset(),
        navigation_state->transition_type(),
        navigation_state->request_time()));
  }

  // Provide the user with a more helpful error page?
  if (MaybeLoadAlternateErrorPage(frame, error, replace))
    return;

  // Fallback to a local error page.
  LoadNavigationErrorPage(frame, failed_request, error, std::string(), replace);
}

void RenderView::didReceiveDocumentData(
    WebFrame* frame, const char* data, size_t data_len,
    bool& prevent_default) {
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->dataSource());
  if (!navigation_state->postpone_loading_data())
    return;

  // We're going to call commitDocumentData ourselves...
  prevent_default = true;

  // Continue buffering the response data for the original error page.  If it
  // grows too large, then we'll just let it through.  For any error other than
  // a 404, "too large" means any data at all.
  navigation_state->append_postponed_data(data, data_len);
  if (navigation_state->postponed_data().size() >= 512 ||
      navigation_state->http_status_code() != 404) {
    navigation_state->set_postpone_loading_data(false);
    frame->commitDocumentData(navigation_state->postponed_data().data(),
                              navigation_state->postponed_data().size());
    navigation_state->clear_postponed_data();
  }
}

void RenderView::didCommitProvisionalLoad(WebFrame* frame,
                                          bool is_new_navigation) {
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->dataSource());

  navigation_state->set_commit_load_time(Time::Now());
  if (is_new_navigation) {
    // When we perform a new navigation, we need to update the previous session
    // history entry with state for the page we are leaving.
    UpdateSessionHistory(frame);

    // We bump our Page ID to correspond with the new session history entry.
    page_id_ = next_page_id_++;

    // Any pending translation is now obsolete.
    translate_helper_.CancelPendingTranslation();

    // Advance our offset in session history, applying the length limit.  There
    // is now no forward history.
    history_list_offset_++;
    if (history_list_offset_ >= chrome::kMaxSessionHistoryEntries)
      history_list_offset_ = chrome::kMaxSessionHistoryEntries - 1;
    history_list_length_ = history_list_offset_ + 1;

    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        page_info_method_factory_.NewRunnableMethod(
            &RenderView::CapturePageInfo, page_id_, true),
        kDelayForForcedCaptureMs);
  } else {
    // Inspect the navigation_state on this frame to see if the navigation
    // corresponds to a session history navigation...  Note: |frame| may or
    // may not be the toplevel frame, but for the case of capturing session
    // history, the first committed frame suffices.  We keep track of whether
    // we've seen this commit before so that only capture session history once
    // per navigation.
    //
    // Note that we need to check if the page ID changed. In the case of a
    // reload, the page ID doesn't change, and UpdateSessionHistory gets the
    // previous URL and the current page ID, which would be wrong.
    if (navigation_state->pending_page_id() != -1 &&
        navigation_state->pending_page_id() != page_id_ &&
        !navigation_state->request_committed()) {
      // This is a successful session history navigation!
      UpdateSessionHistory(frame);
      page_id_ = navigation_state->pending_page_id();

      history_list_offset_ = navigation_state->pending_history_list_offset();
    }
  }

  // Remember that we've already processed this request, so we don't update
  // the session history again.  We do this regardless of whether this is
  // a session history navigation, because if we attempted a session history
  // navigation without valid HistoryItem state, WebCore will think it is a
  // new navigation.
  navigation_state->set_request_committed(true);

  UpdateURL(frame);

  // If this committed load was initiated by a client redirect, we're
  // at the last stop now, so clear it.
  completed_client_redirect_src_ = GURL();

  // Check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());
}

void RenderView::didClearWindowObject(WebFrame* frame) {
  if (BindingsPolicy::is_dom_automation_enabled(enabled_bindings_))
    BindDOMAutomationController(frame);
  if (BindingsPolicy::is_dom_ui_enabled(enabled_bindings_)) {
    dom_ui_bindings_.set_message_sender(this);
    dom_ui_bindings_.set_routing_id(routing_id_);
    dom_ui_bindings_.BindToJavascript(frame, L"chrome");
  }
  if (BindingsPolicy::is_external_host_enabled(enabled_bindings_)) {
    external_host_bindings_.set_message_sender(this);
    external_host_bindings_.set_routing_id(routing_id_);
    external_host_bindings_.BindToJavascript(frame, L"externalHost");
  }
}

void RenderView::didCreateDocumentElement(WebFrame* frame) {
  if (RenderThread::current()) {  // Will be NULL during unit tests.
    RenderThread::current()->user_script_slave()->InjectScripts(
        frame, UserScript::DOCUMENT_START);
  }

  // Notify the browser about non-blank documents loading in the top frame.
  GURL url = frame->url();
  if (url.is_valid() && url.spec() != chrome::kAboutBlankURL) {
    if (frame == webview()->mainFrame())
      Send(new ViewHostMsg_DocumentAvailableInMainFrame(routing_id_));
  }
}

void RenderView::didReceiveTitle(WebFrame* frame, const WebString& title) {
  UpdateTitle(frame, title);

  // Also check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());
}

void RenderView::didChangeIcons(WebFrame* frame) {
  if (!frame->parent()) {
    Send(new ViewHostMsg_UpdateFavIconURL(
        routing_id_,
        page_id_,
        frame->favIconURL()));
  }
}

void RenderView::didFinishDocumentLoad(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);
  navigation_state->set_finish_document_load_time(Time::Now());

  Send(new ViewHostMsg_DocumentLoadedInFrame(routing_id_));

  page_click_tracker_->StartTrackingFrame(frame);
  // The document has now been fully loaded.  Scan for forms to be sent up to
  // the browser.
  autofill_helper_->FrameContentsAvailable(frame);
  password_autocomplete_manager_->SendPasswordForms(frame, false);

  // Check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());

  if (RenderThread::current()) {  // Will be NULL during unit tests.
    RenderThread::current()->user_script_slave()->InjectScripts(
        frame, UserScript::DOCUMENT_END);
  }

  navigation_state->user_script_idle_scheduler()->DidFinishDocumentLoad();
}

void RenderView::OnUserScriptIdleTriggered(WebFrame* frame) {
  if (RenderThread::current()) {  // Will be NULL during unit tests.
    RenderThread::current()->user_script_slave()->InjectScripts(
        frame, UserScript::DOCUMENT_IDLE);
  }

  WebFrame* main_frame = webview()->mainFrame();
  if (frame == main_frame) {
    while (!pending_code_execution_queue_.empty()) {
      linked_ptr<ViewMsg_ExecuteCode_Params>& params =
          pending_code_execution_queue_.front();
      ExecuteCodeImpl(main_frame, *params);
      pending_code_execution_queue_.pop();
    }
  }
}

void RenderView::didHandleOnloadEvents(WebFrame* frame) {
  if (webview()->mainFrame() == frame) {
    Send(new ViewHostMsg_DocumentOnLoadCompletedInMainFrame(routing_id_,
                                                            page_id_));
  }
}

void RenderView::didFailLoad(WebFrame* frame, const WebURLError& error) {
  // Ignore
}

void RenderView::didFinishLoad(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);
  navigation_state->set_finish_load_time(Time::Now());
  navigation_state->user_script_idle_scheduler()->DidFinishLoad();

  // Let the password manager know which password forms are actually visible.
  password_autocomplete_manager_->SendPasswordForms(frame, true);
}

void RenderView::didNavigateWithinPage(
    WebFrame* frame, bool is_new_navigation) {
  // Determine if the UserScriptIdleScheduler already ran scripts on this page,
  // since a new one gets created by didCreateDataSource.
  NavigationState* state = NavigationState::FromDataSource(frame->dataSource());
  bool idle_scheduler_ran = state->user_script_idle_scheduler()->has_run();

  // If this was a reference fragment navigation that we initiated, then we
  // could end up having a non-null pending navigation state.  We just need to
  // update the ExtraData on the datasource so that others who read the
  // ExtraData will get the new NavigationState.  Similarly, if we did not
  // initiate this navigation, then we need to take care to reset any pre-
  // existing navigation state to a content-initiated navigation state.
  // DidCreateDataSource conveniently takes care of this for us.
  didCreateDataSource(frame, frame->dataSource());

  NavigationState* new_state =
      NavigationState::FromDataSource(frame->dataSource());
  new_state->set_was_within_same_page(true);

  if (idle_scheduler_ran) {
    // Update the new UserScriptIdleScheduler so we don't re-run scripts.
    new_state->user_script_idle_scheduler()->set_has_run(true);
  }

  didCommitProvisionalLoad(frame, is_new_navigation);

  UpdateTitle(frame, frame->view()->mainFrame()->dataSource()->pageTitle());
}

void RenderView::didUpdateCurrentHistoryItem(WebFrame* frame) {
  StartNavStateSyncTimerIfNecessary();
}

void RenderView::assignIdentifierToRequest(
    WebFrame* frame, unsigned identifier, const WebURLRequest& request) {
  // Ignore
}

void RenderView::willSendRequest(
    WebFrame* frame, unsigned identifier, WebURLRequest& request,
    const WebURLResponse& redirect_response) {
  WebFrame* top_frame = frame->top();
  if (!top_frame)
    top_frame = frame;
  WebDataSource* provisional_data_source = top_frame->provisionalDataSource();
  WebDataSource* top_data_source = top_frame->dataSource();
  WebDataSource* data_source =
      provisional_data_source ? provisional_data_source : top_data_source;
  if (data_source) {
    NavigationState* state = NavigationState::FromDataSource(data_source);
    if (state && state->is_cache_policy_override_set())
      request.setCachePolicy(state->cache_policy_override());
  }

  if (top_data_source) {
    NavigationState* state = NavigationState::FromDataSource(top_data_source);
    if (state && request.targetType() == WebURLRequest::TargetIsPrefetch)
      state->set_was_prefetcher(true);
  }

  request.setRequestorID(routing_id_);
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoReferrers))
    request.clearHTTPHeaderField("Referer");

  // Temporary metrics, see site_isolation_metrics.h
  SiteIsolationMetrics::AddRequest(identifier, request.targetType());
}

void RenderView::didReceiveResponse(
    WebFrame* frame, unsigned identifier, const WebURLResponse& response) {

  // Temporary metrics, see site_isolation_metrics.h
  SiteIsolationMetrics::LogMimeTypeForCrossOriginRequest(frame,
                                                         identifier,
                                                         response);

  // Only do this for responses that correspond to a provisional data source
  // of the top-most frame.  If we have a provisional data source, then we
  // can't have any sub-resources yet, so we know that this response must
  // correspond to a frame load.
  if (!frame->provisionalDataSource() || frame->parent())
    return;

  // If we are in view source mode, then just let the user see the source of
  // the server's error page.
  if (frame->isViewSourceModeEnabled())
    return;

  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->provisionalDataSource());
  CHECK(navigation_state);
  int http_status_code = response.httpStatusCode();

  // Record page load flags.
  navigation_state->set_was_fetched_via_spdy(response.wasFetchedViaSPDY());
  navigation_state->set_was_npn_negotiated(response.wasNpnNegotiated());
  navigation_state->set_was_alternate_protocol_available(
      response.wasAlternateProtocolAvailable());
  navigation_state->set_was_fetched_via_proxy(response.wasFetchedViaProxy());
  navigation_state->set_http_status_code(http_status_code);

  // Consider loading an alternate error page for 404 responses.
  if (http_status_code == 404) {
    // Can we even load an alternate error page for this URL?
    if (!GetAlternateErrorPageURL(response.url(), HTTP_404).is_valid())
      return;
  } else if (!LocalizedError::HasStrings(LocalizedError::kHttpErrorDomain,
                                         http_status_code)) {
    // If no corresponding error strings for a particular status code, just
    // render any received data, regardless of whether or not the status code
    // indicates an error.
    return;
  }

  navigation_state->set_postpone_loading_data(true);
  navigation_state->clear_postponed_data();
}

void RenderView::didFinishResourceLoad(
    WebFrame* frame, unsigned identifier) {
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->dataSource());
  if (!navigation_state->postpone_loading_data())
    return;

  // The server returned an error and the content was < 512 bytes (which we
  // suppressed).  Go ahead and fetch the alternate page content.
  int http_status_code = navigation_state->http_status_code();
  if (http_status_code == 404) {
    // On 404s, try a remote search page as a fallback.
    const GURL& frame_url = frame->url();

    const GURL& error_page_url = GetAlternateErrorPageURL(frame_url, HTTP_404);
    DCHECK(error_page_url.is_valid());

    WebURLError original_error;
    original_error.unreachableURL = frame_url;

    navigation_state->set_alt_error_page_fetcher(
        new AltErrorPageResourceFetcher(
            error_page_url, frame, original_error,
            NewCallback(this, &RenderView::AltErrorPageFinished)));
  } else {
    // On other errors, use an internal error page.
    WebURLError error;
    error.unreachableURL = frame->url();
    error.domain = WebString::fromUTF8(LocalizedError::kHttpErrorDomain);
    error.reason = http_status_code;

    LoadNavigationErrorPage(frame, frame->dataSource()->request(), error,
                            std::string(), true);
  }
}

void RenderView::didFailResourceLoad(
    WebFrame* frame, unsigned identifier, const WebURLError& error) {
  // Ignore
}

void RenderView::didLoadResourceFromMemoryCache(
    WebFrame* frame, const WebURLRequest& request,
    const WebURLResponse& response) {
  // Let the browser know we loaded a resource from the memory cache.  This
  // message is needed to display the correct SSL indicators.
  Send(new ViewHostMsg_DidLoadResourceFromMemoryCache(
      routing_id_,
      request.url(),
      frame->securityOrigin().toString().utf8(),
      frame->top()->securityOrigin().toString().utf8(),
      response.securityInfo()));
}

void RenderView::didDisplayInsecureContent(WebFrame* frame) {
  Send(new ViewHostMsg_DidDisplayInsecureContent(routing_id_));
}

void RenderView::didRunInsecureContent(
    WebFrame* frame, const WebSecurityOrigin& origin) {
  Send(new ViewHostMsg_DidRunInsecureContent(
      routing_id_,
      origin.toString().utf8()));
}

bool RenderView::allowScript(WebFrame* frame, bool enabled_per_settings) {
  if (enabled_per_settings &&
      AllowContentType(CONTENT_SETTINGS_TYPE_JAVASCRIPT))
    return true;

  if (IsWhitelistedForContentSettings(frame))
    return true;

  return false;  // Other protocols fall through here.
}

bool RenderView::allowDatabase(
    WebFrame* frame, const WebString& name, const WebString& display_name,
    unsigned long estimated_size) {
  WebSecurityOrigin origin = frame->securityOrigin();
  if (origin.isEmpty())
    return false;  // Uninitialized document?

  bool result;
  if (!Send(new ViewHostMsg_AllowDatabase(routing_id_,
      origin.toString().utf8(), name, display_name, estimated_size, &result)))
    return false;
  Send(new ViewHostMsg_WebDatabaseAccessed(routing_id_,
                                           GURL(origin.toString().utf8()),
                                           name,
                                           display_name,
                                           estimated_size,
                                           !result));
  return result;
}
void RenderView::didNotAllowScript(WebKit::WebFrame* frame) {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string());
}

void RenderView::didNotAllowPlugins(WebKit::WebFrame* frame) {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS, std::string());
}

void RenderView::didExhaustMemoryAvailableForScript(WebFrame* frame) {
  Send(new ViewHostMsg_JSOutOfMemory(routing_id_));
}

void RenderView::didCreateScriptContext(WebFrame* frame) {
  EventBindings::HandleContextCreated(frame, false);
}

void RenderView::didDestroyScriptContext(WebFrame* frame) {
  EventBindings::HandleContextDestroyed(frame);
}

void RenderView::didCreateIsolatedScriptContext(WebFrame* frame) {
  EventBindings::HandleContextCreated(frame, true);
}

void RenderView::logCrossFramePropertyAccess(WebFrame* frame,
                                             WebFrame* target,
                                             bool cross_origin,
                                             const WebString& property_name,
                                             unsigned long long event_id) {
  // TODO(johnnyg): track the individual properties and repeat event_ids.
  if (cross_origin)
    cross_origin_access_count_++;
  else
    same_origin_access_count_++;
}

void RenderView::didChangeContentsSize(WebFrame* frame, const WebSize& size) {
  CheckPreferredSize();
}

void RenderView::CheckPreferredSize() {
  // We don't always want to send the change messages over IPC, only if we've
  // be put in that mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;

  // WebCore likes to tell us things have changed even when they haven't, so
  // cache the width and height and only send the IPC message when we're sure
  // they're different.
  gfx::Size size(webview()->mainFrame()->contentsPreferredWidth(),
                 webview()->mainFrame()->documentElementScrollHeight());
  if (size == preferred_size_)
    return;

  preferred_size_ = size;
  Send(new ViewHostMsg_DidContentsPreferredSizeChange(routing_id_,
                                                      preferred_size_));
}

void RenderView::didChangeScrollOffset(WebFrame* frame) {
  StartNavStateSyncTimerIfNecessary();
}

void RenderView::reportFindInPageMatchCount(int request_id, int count,
                                            bool final_update) {
  int active_match_ordinal = -1;  // -1 = don't update active match ordinal
  if (!count)
    active_match_ordinal = 0;

  IPC::Message* msg = new ViewHostMsg_Find_Reply(
      routing_id_,
      request_id,
      count,
      gfx::Rect(),
      active_match_ordinal,
      final_update);

  // If we have a message that has been queued up, then we should just replace
  // it. The ACK from the browser will make sure it gets sent when the browser
  // wants it.
  if (queued_find_reply_message_.get()) {
    queued_find_reply_message_.reset(msg);
  } else {
    // Send the search result over to the browser process.
    Send(msg);
  }
}

void RenderView::reportFindInPageSelection(int request_id,
                                           int active_match_ordinal,
                                           const WebRect& selection_rect) {
  // Send the search result over to the browser process.
  Send(new ViewHostMsg_Find_Reply(routing_id_,
                                  request_id,
                                  -1,
                                  selection_rect,
                                  active_match_ordinal,
                                  false));
}

void RenderView::openFileSystem(
    WebFrame* frame,
    WebFileSystem::Type type,
    long long size,
    WebFileSystemCallbacks* callbacks) {
  scoped_ptr<PendingOpenFileSystem> request(
      new PendingOpenFileSystem(callbacks));

  WebSecurityOrigin origin = frame->securityOrigin();
  if (origin.isEmpty())
    return;  // Uninitialized document?

  ViewHostMsg_OpenFileSystemRequest_Params params;
  params.routing_id = routing_id_;
  params.request_id = pending_file_system_requests_.Add(request.release());
  params.origin_url = GURL(origin.toString());
  params.type = type;
  params.requested_size = size;

  Send(new ViewHostMsg_OpenFileSystemRequest(params));
}

// webkit_glue::WebPluginPageDelegate -----------------------------------------

webkit_glue::WebPluginDelegate* RenderView::CreatePluginDelegate(
    const FilePath& file_path,
    const std::string& mime_type) {
  if (!PluginChannelHost::IsListening())
    return NULL;

  bool use_pepper_host = false;
  bool in_process_plugin = RenderProcess::current()->UseInProcessPlugins();
  // Check for trusted Pepper plugins.
  const char kPepperPrefix[] = "pepper-";
  if (StartsWithASCII(mime_type, kPepperPrefix, true)) {
    if (CommandLine::ForCurrentProcess()->
            HasSwitch(switches::kInternalPepper)) {
      in_process_plugin = true;
      use_pepper_host = true;
    } else {
      // In process Pepper plugins must be explicitly enabled.
      return NULL;
    }
  } else {
    FilePath internal_pdf_path;
    PathService::Get(chrome::FILE_PDF_PLUGIN, &internal_pdf_path);
    if (file_path == internal_pdf_path) {
      in_process_plugin = true;
      use_pepper_host = true;
    }
  }

  // Check for Native Client modules.
  if (mime_type == "application/x-nacl-srpc" &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kInternalNaCl)) {
    // NaCl is only permitted when we're in an extension/application with the
    // appropriate permission, or when explicitly enabled on the command line.

    GURL main_frame_url(webview()->mainFrame()->url());
    ExtensionRendererInfo* extension =
        ExtensionRendererInfo::GetByURL(main_frame_url);
    bool in_ext = extension != NULL;
    bool explicit_enable =
        CommandLine::ForCurrentProcess()->HasSwitch(switches::kInternalNaCl);

    if (in_ext) {
      // TODO(cbiffle): NaCl is back to experimental for M7.
      if (ExtensionProcessBindings::HasPermission(extension->id(),
              Extension::kExperimentalPermission)) {
        in_process_plugin = true;
        use_pepper_host = true;
      } else {
        // Disable NaCl for apps lacking the permission, even with the flag.
        return NULL;
      }
    } else if (explicit_enable) {
      in_process_plugin = true;
      use_pepper_host = true;
    }
  }

  if (in_process_plugin) {
    if (use_pepper_host) {
      WebPluginDelegatePepper* pepper_plugin =
           WebPluginDelegatePepper::Create(file_path, mime_type, AsWeakPtr());
      if (!pepper_plugin)
        return NULL;

      current_oldstyle_pepper_plugins_.insert(pepper_plugin);
      return pepper_plugin;
    } else {
#if defined(OS_WIN)  // In-proc plugins aren't supported on Linux or Mac.
      return WebPluginDelegateImpl::Create(
          file_path, mime_type, gfx::NativeViewFromId(host_window_));
#else
      NOTIMPLEMENTED();
      return NULL;
#endif
    }
  }

  return new WebPluginDelegateProxy(mime_type, AsWeakPtr());
}

void RenderView::CreatedPluginWindow(gfx::PluginWindowHandle window) {
#if defined(USE_X11)
  RenderThread::current()->Send(new ViewHostMsg_CreatePluginContainer(
      routing_id(), window));
#endif
}

void RenderView::WillDestroyPluginWindow(gfx::PluginWindowHandle window) {
#if defined(USE_X11)
  RenderThread::current()->Send(new ViewHostMsg_DestroyPluginContainer(
      routing_id(), window));
#endif
  CleanupWindowInPluginMoves(window);
}

void RenderView::DidMovePlugin(const webkit_glue::WebPluginGeometry& move) {
  SchedulePluginMove(move);
}

void RenderView::DidStartLoadingForPlugin() {
  // TODO(darin): Make is_loading_ be a counter!
  didStartLoading();
}

void RenderView::DidStopLoadingForPlugin() {
  // TODO(darin): Make is_loading_ be a counter!
  didStopLoading();
}

void RenderView::ShowModalHTMLDialogForPlugin(
    const GURL& url,
    const gfx::Size& size,
    const std::string& json_arguments,
    std::string* json_retval) {
  SendAndRunNestedMessageLoop(new ViewHostMsg_ShowModalHTMLDialog(
      routing_id_, url, size.width(), size.height(), json_arguments,
      json_retval));
}

WebCookieJar* RenderView::GetCookieJar() {
  return &cookie_jar_;
}

void RenderView::SyncNavigationState() {
  if (!webview())
    return;

  const WebHistoryItem& item = webview()->mainFrame()->currentHistoryItem();
  if (item.isNull())
    return;

  Send(new ViewHostMsg_UpdateState(
      routing_id_, page_id_, webkit_glue::HistoryItemToString(item)));
}

bool RenderView::DownloadImage(int id, const GURL& image_url, int image_size) {
  // Make sure webview was not shut down.
  if (!webview())
    return false;
  // Create an image resource fetcher and assign it with a call back object.
  image_fetchers_.insert(new ImageResourceFetcher(
      image_url, webview()->mainFrame(), id, image_size,
      NewCallback(this, &RenderView::DidDownloadImage)));
  return true;
}

void RenderView::DidDownloadImage(ImageResourceFetcher* fetcher,
                                  const SkBitmap& image) {
  // Notify requester of image download status.
  Send(new ViewHostMsg_DidDownloadFavIcon(routing_id_,
                                          fetcher->id(),
                                          fetcher->image_url(),
                                          image.isNull(),
                                          image));
  // Dispose of the image fetcher.
  DCHECK(image_fetchers_.find(fetcher) != image_fetchers_.end());
  image_fetchers_.erase(fetcher);
  // We're in the callback from the ImageResourceFetcher, best to delay
  // deletion.
  MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);
}

void RenderView::OnDownloadFavIcon(int id,
                                   const GURL& image_url,
                                   int image_size) {
  bool data_image_failed = false;
  if (image_url.SchemeIs("data")) {
    SkBitmap data_image = ImageFromDataUrl(image_url);
    data_image_failed = data_image.empty();
    if (!data_image_failed) {
      Send(new ViewHostMsg_DidDownloadFavIcon(routing_id_, id, image_url, false,
                                              data_image));
    }
  }

  if (data_image_failed ||
      !DownloadImage(id, image_url, image_size)) {
    Send(new ViewHostMsg_DidDownloadFavIcon(routing_id_, id, image_url, true,
                                            SkBitmap()));
  }
}

SkBitmap RenderView::ImageFromDataUrl(const GURL& url) const {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data) && !data.empty()) {
    // Decode the favicon using WebKit's image decoder.
    webkit_glue::ImageDecoder decoder(gfx::Size(kFavIconSize, kFavIconSize));
    const unsigned char* src_data =
        reinterpret_cast<const unsigned char*>(&data[0]);

    return decoder.Decode(src_data, data.size());
  }
  return SkBitmap();
}

void RenderView::OnGetApplicationInfo(int page_id) {
  webkit_glue::WebApplicationInfo app_info;
  if (page_id == page_id_)
    webkit_glue::GetApplicationInfo(webview(), &app_info);

  // Prune out any data URLs in the set of icons.  The browser process expects
  // any icon with a data URL to have originated from a favicon.  We don't want
  // to decode arbitrary data URLs in the browser process.  See
  // http://b/issue?id=1162972
  for (size_t i = 0; i < app_info.icons.size(); ++i) {
    if (app_info.icons[i].url.SchemeIs(chrome::kDataScheme)) {
      app_info.icons.erase(app_info.icons.begin() + i);
      --i;
    }
  }

  Send(new ViewHostMsg_DidGetApplicationInfo(routing_id_, page_id, app_info));
}

GURL RenderView::GetAlternateErrorPageURL(const GURL& failed_url,
                                          ErrorPageType error_type) {
  if (failed_url.SchemeIsSecure()) {
    // If the URL that failed was secure, then the embedding web page was not
    // expecting a network attacker to be able to manipulate its contents.  As
    // we fetch alternate error pages over HTTP, we would be allowing a network
    // attacker to manipulate the contents of the response if we tried to use
    // the link doctor here.
    return GURL();
  }

  // Grab the base URL from the browser process.
  if (!alternate_error_page_url_.is_valid())
    return GURL();

  // Strip query params from the failed URL.
  GURL::Replacements remove_params;
  remove_params.ClearUsername();
  remove_params.ClearPassword();
  remove_params.ClearQuery();
  remove_params.ClearRef();
  const GURL url_to_send = failed_url.ReplaceComponents(remove_params);
  std::string spec_to_send = url_to_send.spec();
  // Notify link doctor of the url truncation by sending of "?" at the end.
  if (failed_url.has_query())
      spec_to_send.append("?");

  // Construct the query params to send to link doctor.
  std::string params(alternate_error_page_url_.query());
  params.append("&url=");
  params.append(EscapeQueryParamValue(spec_to_send, true));
  params.append("&sourceid=chrome");
  params.append("&error=");
  switch (error_type) {
    case DNS_ERROR:
      params.append("dnserror");
      break;

    case HTTP_404:
      params.append("http404");
      break;

    case CONNECTION_ERROR:
      params.append("connectionfailure");
      break;

    default:
      NOTREACHED() << "unknown ErrorPageType";
  }

  // OK, build the final url to return.
  GURL::Replacements link_doctor_params;
  link_doctor_params.SetQueryStr(params);
  GURL url = alternate_error_page_url_.ReplaceComponents(link_doctor_params);
  return url;
}

WebKit::WebPlugin* RenderView::GetWebPluginFromPluginDocument() {
  return webview()->mainFrame()->document().to<WebPluginDocument>().plugin();
}

void RenderView::OnFind(int request_id, const string16& search_text,
                        const WebFindOptions& options) {
  WebFrame* main_frame = webview()->mainFrame();

  if (main_frame->document().isPluginDocument()) {
    if (options.findNext) {
      // Just navigate back/forward.
      GetWebPluginFromPluginDocument()->selectFindResult(options.forward);
    } else {
      if (GetWebPluginFromPluginDocument()->startFind(
          search_text, options.matchCase, request_id)) {
      } else {
        // Send "no results".
        Send(new ViewHostMsg_Find_Reply(routing_id_,
                                        request_id,
                                        0,
                                        gfx::Rect(),
                                        0,
                                        true));
      }
    }
    return;
  }

  WebFrame* frame_after_main = main_frame->traverseNext(true);
  WebFrame* focused_frame = webview()->focusedFrame();
  WebFrame* search_frame = focused_frame;  // start searching focused frame.

  bool multi_frame = (frame_after_main != main_frame);

  // If we have multiple frames, we don't want to wrap the search within the
  // frame, so we check here if we only have main_frame in the chain.
  bool wrap_within_frame = !multi_frame;

  WebRect selection_rect;
  bool result = false;

  // If something is selected when we start searching it means we cannot just
  // increment the current match ordinal; we need to re-generate it.
  WebRange current_selection = focused_frame->selectionRange();

  do {
    result = search_frame->find(
        request_id, search_text, options, wrap_within_frame, &selection_rect);

    if (!result) {
      // don't leave text selected as you move to the next frame.
      search_frame->executeCommand(WebString::fromUTF8("Unselect"));

      // Find the next frame, but skip the invisible ones.
      do {
        // What is the next frame to search? (we might be going backwards). Note
        // that we specify wrap=true so that search_frame never becomes NULL.
        search_frame = options.forward ?
            search_frame->traverseNext(true) :
            search_frame->traversePrevious(true);
      } while (!search_frame->hasVisibleContent() &&
               search_frame != focused_frame);

      // Make sure selection doesn't affect the search operation in new frame.
      search_frame->executeCommand(WebString::fromUTF8("Unselect"));

      // If we have multiple frames and we have wrapped back around to the
      // focused frame, we need to search it once more allowing wrap within
      // the frame, otherwise it will report 'no match' if the focused frame has
      // reported matches, but no frames after the focused_frame contain a
      // match for the search word(s).
      if (multi_frame && search_frame == focused_frame) {
        result = search_frame->find(
            request_id, search_text, options, true,  // Force wrapping.
            &selection_rect);
      }
    }

    webview()->setFocusedFrame(search_frame);
  } while (!result && search_frame != focused_frame);

  if (options.findNext && current_selection.isNull()) {
    // Force the main_frame to report the actual count.
    main_frame->increaseMatchCount(0, request_id);
  } else {
    // If nothing is found, set result to "0 of 0", otherwise, set it to
    // "-1 of 1" to indicate that we found at least one item, but we don't know
    // yet what is active.
    int ordinal = result ? -1 : 0;  // -1 here means, we might know more later.
    int match_count = result ? 1 : 0;  // 1 here means possibly more coming.

    // If we find no matches then this will be our last status update.
    // Otherwise the scoping effort will send more results.
    bool final_status_update = !result;

    // Send the search result over to the browser process.
    Send(new ViewHostMsg_Find_Reply(routing_id_,
                                    request_id,
                                    match_count,
                                    selection_rect,
                                    ordinal,
                                    final_status_update));

    // Scoping effort begins, starting with the mainframe.
    search_frame = main_frame;

    main_frame->resetMatchCount();

    do {
      // Cancel all old scoping requests before starting a new one.
      search_frame->cancelPendingScopingEffort();

      // We don't start another scoping effort unless at least one match has
      // been found.
      if (result) {
        // Start new scoping request. If the scoping function determines that it
        // needs to scope, it will defer until later.
        search_frame->scopeStringMatches(request_id,
                                         search_text,
                                         options,
                                         true);  // reset the tickmarks
      }

      // Iterate to the next frame. The frame will not necessarily scope, for
      // example if it is not visible.
      search_frame = search_frame->traverseNext(true);
    } while (search_frame != main_frame);
  }
}

void RenderView::OnStopFinding(const ViewMsg_StopFinding_Params& params) {
  WebView* view = webview();
  if (!view)
    return;

  WebDocument doc = view->mainFrame()->document();
  if (doc.isPluginDocument()) {
    GetWebPluginFromPluginDocument()->stopFind();
    return;
  }

  bool clear_selection =
      params.action == ViewMsg_StopFinding_Params::kClearSelection;
  if (clear_selection)
    view->focusedFrame()->executeCommand(WebString::fromUTF8("Unselect"));

  WebFrame* frame = view->mainFrame();
  while (frame) {
    frame->stopFinding(clear_selection);
    frame = frame->traverseNext(false);
  }

  if (params.action == ViewMsg_StopFinding_Params::kActivateSelection) {
    WebFrame* focused_frame = view->focusedFrame();
    if (focused_frame) {
      WebDocument doc = focused_frame->document();
      if (!doc.isNull()) {
        WebNode node = doc.focusedNode();
        if (!node.isNull())
          node.simulateClick();
      }
    }
  }
}

void RenderView::OnFindReplyAck() {
  // Check if there is any queued up request waiting to be sent.
  if (queued_find_reply_message_.get()) {
    // Send the search result over to the browser process.
    Send(queued_find_reply_message_.release());
  }
}

bool RenderView::AllowContentType(ContentSettingsType settings_type) {
  // CONTENT_SETTING_ASK is only valid for cookies.
  return current_content_settings_.settings[settings_type] !=
    CONTENT_SETTING_BLOCK;
}

void RenderView::DidBlockContentType(ContentSettingsType settings_type,
                                     const std::string& resource_identifier) {
  if (!content_blocked_[settings_type]) {
    content_blocked_[settings_type] = true;
    Send(new ViewHostMsg_ContentBlocked(routing_id_, settings_type,
                                        resource_identifier));
  }
}

void RenderView::ClearBlockedContentSettings() {
  for (size_t i = 0; i < arraysize(content_blocked_); ++i)
    content_blocked_[i] = false;
}

WebPlugin* RenderView::CreatePepperPlugin(WebFrame* frame,
                                          const WebPluginParams& params,
                                          const FilePath& path,
                                          pepper::PluginModule* pepper_module) {
  WebPlugin* plugin = new pepper::WebPluginImpl(pepper_module, params,
                                                pepper_delegate_.AsWeakPtr());
  if (plugin && !frame->parent() && frame->document().isPluginDocument()) {
    // If this is a full-page plugin hosting the internal PDF plugin, we want
    // to notify the browser so that it can treat things like zooming
    // differently.
    // TODO(sanjeevr): Use a Pepper interface to determine this rather than
    // hardcode this for the PDF plugin path.
    FilePath pdf_path;
    PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path);
    if (path == pdf_path)
      Send(new ViewHostMsg_SetDisplayingPDFContent(routing_id_));
  }
  return plugin;
}

WebPlugin* RenderView::CreateNPAPIPlugin(WebFrame* frame,
                                         const WebPluginParams& params,
                                         const FilePath& path,
                                         const std::string& mime_type) {
  std::string actual_mime_type(mime_type);
  if (actual_mime_type.empty())
    actual_mime_type = params.mimeType.utf8();

  return new webkit_glue::WebPluginImpl(frame, params, path,
                                        actual_mime_type, AsWeakPtr());
}

WebPlugin* RenderView::CreatePluginPlaceholder(WebFrame* frame,
                                               const WebPluginParams& params,
                                               PluginGroup* group) {
  // |blocked_plugin| will delete itself when the WebViewPlugin is destroyed.
  BlockedPlugin* blocked_plugin = new BlockedPlugin(this, frame, params, group);
  WebViewPlugin* plugin = blocked_plugin->plugin();
  WebView* web_view = plugin->web_view();
  webkit_preferences_.Apply(web_view);
  return plugin;
}

void RenderView::OnZoom(PageZoom::Function function) {
  if (!webview())  // Not sure if this can happen, but no harm in being safe.
    return;

  webview()->hidePopups();

  int zoom_level = webview()->zoomLevel();
  int new_zoom_level = webview()->setZoomLevel(false,
      (function == PageZoom::RESET) ? 0 : (zoom_level + function));

  // Tell the browser which url got zoomed so it can update the saved values.
  Send(new ViewHostMsg_DidZoomURL(
      GURL(webview()->mainFrame()->url()), new_zoom_level));
}

void RenderView::OnSetContentSettingsForLoadingURL(
    const GURL& url,
    const ContentSettings& content_settings) {
  host_content_settings_[url] = content_settings;
}

void RenderView::OnSetZoomLevelForLoadingURL(const GURL& url,
                                             int zoom_level) {
  host_zoom_levels_[url] = zoom_level;
}

void RenderView::OnSetPageEncoding(const std::string& encoding_name) {
  webview()->setPageEncoding(WebString::fromUTF8(encoding_name));
}

void RenderView::OnResetPageEncodingToDefault() {
  WebString no_encoding;
  webview()->setPageEncoding(no_encoding);
}

bool RenderView::GetAllChildFrames(
    WebFrame* parent_frame,
    std::vector<WebFrame*>* frames_vector) const {
  if (!parent_frame)
    return false;
  for (WebFrame* child_frame = parent_frame->firstChild(); child_frame;
       child_frame = child_frame->nextSibling()) {
    frames_vector->push_back(child_frame);
    GetAllChildFrames(child_frame, frames_vector);
  }
  return true;
}

WebFrame* RenderView::GetChildFrame(const std::wstring& xpath) const {
  if (xpath.empty())
    return webview()->mainFrame();

  // xpath string can represent a frame deep down the tree (across multiple
  // frame DOMs).
  // Example, /html/body/table/tbody/tr/td/iframe\n/frameset/frame[0]
  // should break into 2 xpaths
  // /html/body/table/tbody/tr/td/iframe & /frameset/frame[0]

  WebFrame* frame = webview()->mainFrame();

  std::wstring xpath_remaining = xpath;
  while (!xpath_remaining.empty()) {
    std::wstring::size_type delim_pos = xpath_remaining.find_first_of(L'\n');
    std::wstring xpath_child;
    if (delim_pos != std::wstring::npos) {
      xpath_child = xpath_remaining.substr(0, delim_pos);
      xpath_remaining.erase(0, delim_pos + 1);
    } else {
      xpath_remaining.swap(xpath_child);
    }
    frame = frame->findChildByExpression(WideToUTF16Hack(xpath_child));
  }

  return frame;
}

void RenderView::SetSuggestResult(const std::string& suggest) {
  if (!suggest.empty())
    Send(new ViewHostMsg_SetSuggestResult(routing_id_, page_id_, suggest));
}

void RenderView::EvaluateScript(const std::wstring& frame_xpath,
                                const std::wstring& script) {
  WebFrame* web_frame = GetChildFrame(frame_xpath);
  if (!web_frame)
    return;

  web_frame->executeScript(WebScriptSource(WideToUTF16Hack(script)));
}

void RenderView::InsertCSS(const std::wstring& frame_xpath,
                           const std::string& css,
                           const std::string& id) {
  WebFrame* web_frame = GetChildFrame(frame_xpath);
  if (!web_frame)
    return;

  web_frame->insertStyleText(WebString::fromUTF8(css), WebString::fromUTF8(id));
}

void RenderView::OnPepperPluginDestroy(
    WebPluginDelegatePepper* pepper_plugin) {
  std::set<WebPluginDelegatePepper*>::iterator found_pepper =
      current_oldstyle_pepper_plugins_.find(pepper_plugin);
  if (found_pepper == current_oldstyle_pepper_plugins_.end()) {
    NOTREACHED();
    return;
  }
  current_oldstyle_pepper_plugins_.erase(found_pepper);

  // The plugin could have been destroyed while it was waiting for a file
  // choose callback, so check all pending completion callbacks and NULL them.
  for (std::deque< linked_ptr<PendingFileChooser> >::iterator i =
           file_chooser_completions_.begin();
       i != file_chooser_completions_.end(); /* nothing */) {
    if ((*i)->completion == pepper_plugin) {
      // We NULL the first one instead of deleting it because the plugin might
      // be the one waiting for a file choose callback. If the callback later
      // comes, we don't want to send the result to the next callback in line.
      if (i == file_chooser_completions_.begin())
        (*i)->completion = NULL;
      else
        i = file_chooser_completions_.erase(i);
    } else {
      ++i;
    }
  }
}

void RenderView::OnScriptEvalRequest(const std::wstring& frame_xpath,
                                     const std::wstring& jscript) {
  EvaluateScript(frame_xpath, jscript);
}

void RenderView::OnCSSInsertRequest(const std::wstring& frame_xpath,
                                    const std::string& css,
                                    const std::string& id) {
  InsertCSS(frame_xpath, css, id);

  // Notify RenderViewHost that css has been inserted into the frame.
  Send(new ViewHostMsg_OnCSSInserted(routing_id_));
}

void RenderView::OnAddMessageToConsole(
    const string16& frame_xpath,
    const string16& message,
    const WebConsoleMessage::Level& level) {
  WebFrame* web_frame = GetChildFrame(UTF16ToWideHack(frame_xpath));
  if (web_frame)
    web_frame->addMessageToConsole(WebConsoleMessage(level, message));
}

void RenderView::OnAllowBindings(int enabled_bindings_flags) {
  enabled_bindings_ |= enabled_bindings_flags;
}

void RenderView::OnSetDOMUIProperty(const std::string& name,
                                    const std::string& value) {
  DCHECK(BindingsPolicy::is_dom_ui_enabled(enabled_bindings_));
  dom_ui_bindings_.SetProperty(name, value);
}

void RenderView::OnReservePageIDRange(int size_of_range) {
  next_page_id_ += size_of_range + 1;
}

void RenderView::OnDragSourceEndedOrMoved(const gfx::Point& client_point,
                                          const gfx::Point& screen_point,
                                          bool ended,
                                          WebDragOperation op) {
  if (ended) {
    webview()->dragSourceEndedAt(client_point, screen_point, op);
  } else {
    webview()->dragSourceMovedTo(client_point, screen_point, op);
  }
}

void RenderView::OnDragSourceSystemDragEnded() {
  webview()->dragSourceSystemDragEnded();
}

void RenderView::OnFillPasswordForm(
    const webkit_glue::PasswordFormFillData& form_data) {
#if defined(WEBKIT_BUG_41283_IS_FIXED)
  password_autocomplete_manager_->ReceivedPasswordFormFillData(webview(),
                                                              form_data);
#else
  webkit_glue::FillPasswordForm(this->webview(), form_data);
#endif
}

void RenderView::OnDragTargetDragEnter(const WebDropData& drop_data,
                                       const gfx::Point& client_point,
                                       const gfx::Point& screen_point,
                                       WebDragOperationsMask ops) {
  WebDragOperation operation = webview()->dragTargetDragEnter(
      drop_data.ToDragData(),
      drop_data.identity,
      client_point,
      screen_point,
      ops);

  Send(new ViewHostMsg_UpdateDragCursor(routing_id_, operation));
}

void RenderView::OnDragTargetDragOver(const gfx::Point& client_point,
                                      const gfx::Point& screen_point,
                                      WebDragOperationsMask ops) {
  WebDragOperation operation = webview()->dragTargetDragOver(
      client_point,
      screen_point,
      ops);

  Send(new ViewHostMsg_UpdateDragCursor(routing_id_, operation));
}

void RenderView::OnDragTargetDragLeave() {
  webview()->dragTargetDragLeave();
}

void RenderView::OnDragTargetDrop(const gfx::Point& client_point,
                                  const gfx::Point& screen_point) {
  webview()->dragTargetDrop(client_point, screen_point);
}

void RenderView::OnUpdateWebPreferences(const WebPreferences& prefs) {
  webkit_preferences_ = prefs;
  webkit_preferences_.Apply(webview());
}

void RenderView::OnSetAltErrorPageURL(const GURL& url) {
  alternate_error_page_url_ = url;
}

void RenderView::OnCustomContextMenuAction(unsigned action) {
  webview()->performCustomContextMenuAction(action);
}

void RenderView::OnTranslatePage(int page_id,
                                 const std::string& translate_script,
                                 const std::string& source_lang,
                                 const std::string& target_lang) {
  translate_helper_.TranslatePage(page_id, source_lang, target_lang,
                                  translate_script);
}

void RenderView::OnRevertTranslation(int page_id) {
  translate_helper_.RevertTranslation(page_id);
}

void RenderView::OnInstallMissingPlugin() {
  // This could happen when the first default plugin is deleted.
  if (first_default_plugin_)
    first_default_plugin_->InstallMissingPlugin();
}

void RenderView::OnLoadBlockedPlugins() {
  NotificationService::current()->Notify(NotificationType::SHOULD_LOAD_PLUGINS,
                                         Source<RenderView>(this),
                                         NotificationService::NoDetails());
}

void RenderView::OnFileChooserResponse(const std::vector<FilePath>& paths) {
  // This could happen if we navigated to a different page before the user
  // closed the chooser.
  if (file_chooser_completions_.empty())
    return;

  WebVector<WebString> ws_file_names(paths.size());
  for (size_t i = 0; i < paths.size(); ++i)
    ws_file_names[i] = webkit_glue::FilePathToWebString(paths[i]);

  if (file_chooser_completions_.front()->completion)
    file_chooser_completions_.front()->completion->didChooseFile(ws_file_names);
  file_chooser_completions_.pop_front();

  // If there are more pending file chooser requests, schedule one now.
  if (!file_chooser_completions_.empty()) {
    Send(new ViewHostMsg_RunFileChooser(routing_id_,
        file_chooser_completions_.front()->params));
  }
}

void RenderView::OnEnableViewSourceMode() {
  if (!webview())
    return;
  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  main_frame->enableViewSourceMode(true);
}

void RenderView::OnEnablePreferredSizeChangedMode(int flags) {
  DCHECK(flags != kPreferredSizeNothing);
  if (send_preferred_size_changes_)
    return;
  send_preferred_size_changes_ = true;

  // WebKit doesn't send a notification of the effective height of the page
  // changes, so poll for it.
  // TODO: Add a notification for this to WebKit, remove polling. After that's
  // done, rename kPreferredSizeHeightThisIsSlow to kPreferredSizeHeight.
  // http://crbug.com/44850
  if (flags & kPreferredSizeHeightThisIsSlow) {
    preferred_size_change_timer_.Start(TimeDelta::FromMilliseconds(10), this,
                                       &RenderView::CheckPreferredSize);
  }
}

void RenderView::OnDisableScrollbarsForSmallWindows(
    const gfx::Size& disable_scrollbar_size_limit) {
  disable_scrollbars_size_limit_ = disable_scrollbar_size_limit;
}

void RenderView::OnSetRendererPrefs(const RendererPreferences& renderer_prefs) {
  renderer_preferences_ = renderer_prefs;
  UpdateFontRenderingFromRendererPrefs();
#if defined(TOOLKIT_GTK)
  WebColorName name = WebKit::WebColorWebkitFocusRingColor;
  WebKit::setNamedColors(&name, &renderer_prefs.focus_ring_color, 1);
  WebKit::setCaretBlinkInterval(renderer_prefs.caret_blink_interval);

  if (webview()) {
    webview()->setScrollbarColors(
        renderer_prefs.thumb_inactive_color,
        renderer_prefs.thumb_active_color,
        renderer_prefs.track_color);
    webview()->setSelectionColors(
        renderer_prefs.active_selection_bg_color,
        renderer_prefs.active_selection_fg_color,
        renderer_prefs.inactive_selection_bg_color,
        renderer_prefs.inactive_selection_fg_color);
    webview()->themeChanged();
  }
#endif
}

void RenderView::OnMediaPlayerActionAt(const gfx::Point& location,
                                       const WebMediaPlayerAction& action) {
  if (webview())
    webview()->performMediaPlayerAction(action, location);
}

void RenderView::OnNotifyRendererViewType(ViewType::Type type) {
  view_type_ = type;
}

void RenderView::OnUpdateBrowserWindowId(int window_id) {
  browser_window_id_ = window_id;
}

void RenderView::OnGetAccessibilityTree() {
  if (accessibility_.get())
    accessibility_->clear();
    accessibility_.reset(WebAccessibilityCache::create());
    accessibility_->initialize(webview());
  accessibility_changes_.clear();

  WebAccessibilityObject src_tree = webview()->accessibilityObject();
  WebAccessibility dst_tree(src_tree, accessibility_.get());
  Send(new ViewHostMsg_AccessibilityTree(routing_id_, dst_tree));
}

void RenderView::OnSetAccessibilityFocus(int acc_obj_id) {
  if (!accessibility_.get())
    return;
  if (accessibility_->isValidId(acc_obj_id)) {
    // TODO(dmazzoni) fix the cache so that id=1000 is not a magic number.
    // By convention, calling SetFocus on the root of the tree (id = 1000)
    // should clear the current focus. Otherwise set the focus to the new
    // node.
    if (acc_obj_id == 1000)
      webview()->clearFocusedNode();
    else
      accessibility_->getObjectById(acc_obj_id).setFocused(true);
  }
}

void RenderView::OnAccessibilityDoDefaultAction(int acc_obj_id) {
  if (!accessibility_.get())
    return;
  if (accessibility_->isValidId(acc_obj_id)) {
    accessibility_->getObjectById(acc_obj_id).performDefaultAction();
  }
}

void RenderView::OnAccessibilityObjectChildrenChangeAck() {
  if (!accessibility_.get())
    return;

  if (!accessibility_changes_.empty()) {
    Send(new ViewHostMsg_AccessibilityObjectChildrenChange(
        routing_id_,
        accessibility_changes_));
  }

  accessibility_changes_.clear();
}

void RenderView::OnGetAllSavableResourceLinksForCurrentPage(
    const GURL& page_url) {
  // Prepare list to storage all savable resource links.
  std::vector<GURL> resources_list;
  std::vector<GURL> referrers_list;
  std::vector<GURL> frames_list;
  webkit_glue::SavableResourcesResult result(&resources_list,
                                             &referrers_list,
                                             &frames_list);

  if (!webkit_glue::GetAllSavableResourceLinksForCurrentPage(
          webview(),
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
  Send(new ViewHostMsg_SendCurrentPageAllSavableResourceLinks(routing_id_,
                                                              resources_list,
                                                              referrers_list,
                                                              frames_list));
}

void RenderView::OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
    const std::vector<GURL>& links,
    const std::vector<FilePath>& local_paths,
    const FilePath& local_directory_name) {

  // Convert std::vector of GURLs to WebVector<WebURL>
  WebVector<WebURL> weburl_links(links);

  // Convert std::vector of std::strings to WebVector<WebString>
  WebVector<WebString> webstring_paths(local_paths.size());
  for (size_t i = 0; i < local_paths.size(); i++)
    webstring_paths[i] = webkit_glue::FilePathToWebString(local_paths[i]);

  WebPageSerializer::serialize(webview()->mainFrame(),
                               true, this, weburl_links, webstring_paths,
                               webkit_glue::FilePathToWebString(
                                   local_directory_name));
}

void RenderView::didSerializeDataForFrame(const WebURL& frame_url,
    const WebCString& data,
    WebPageSerializerClient::PageSerializationStatus status) {
  Send(new ViewHostMsg_SendSerializedHtmlData(
    routing_id_,
    frame_url,
    data.data(),
    static_cast<int32>(status)));
}

void RenderView::OnShouldClose() {
  bool should_close = webview()->dispatchBeforeUnloadEvent();
  Send(new ViewHostMsg_ShouldClose_ACK(routing_id_, should_close));
}

void RenderView::OnClosePage(const ViewMsg_ClosePage_Params& params) {
  // TODO(creis): We'd rather use webview()->Close() here, but that currently
  // sets the WebView's delegate_ to NULL, preventing any JavaScript dialogs
  // in the onunload handler from appearing.  For now, we're bypassing that and
  // calling the FrameLoader's CloseURL method directly.  This should be
  // revisited to avoid having two ways to close a page.  Having a single way
  // to close that can run onunload is also useful for fixing
  // http://b/issue?id=753080.
  WebFrame* main_frame = webview()->mainFrame();
  if (main_frame) {
    const GURL& url = main_frame->url();
    // TODO(davemoore) this code should be removed once willClose() gets
    // called when a page is destroyed. DumpLoadHistograms() is safe to call
    // multiple times for the same frame, but it will simplify things.

    // Ensure state contains scheme.
    NavigationState* navigation_state =
        NavigationState::FromDataSource(main_frame->dataSource());
    if (url.SchemeIs("http"))
      navigation_state->set_scheme_type(URLPattern::SCHEME_HTTP);
    else if (url.SchemeIs("https"))
      navigation_state->set_scheme_type(URLPattern::SCHEME_HTTPS);

    // Dump will only be provided when scheme is http or https.
    DumpLoadHistograms();
  }
  webview()->dispatchUnloadEvent();

  // Reset stats
  cross_origin_access_count_ = 0;
  same_origin_access_count_ = 0;

  // Just echo back the params in the ACK.
  Send(new ViewHostMsg_ClosePage_ACK(routing_id_, params));
}

void RenderView::OnThemeChanged() {
#if defined(OS_WIN)
  gfx::NativeTheme::instance()->CloseHandles();
  if (webview())
    webview()->themeChanged();
#else  // defined(OS_WIN)
  // TODO(port): we don't support theming on non-Windows platforms yet
  NOTIMPLEMENTED();
#endif
}

void RenderView::OnHandleMessageFromExternalHost(const std::string& message,
                                                 const std::string& origin,
                                                 const std::string& target) {
  if (message.empty())
    return;
  external_host_bindings_.ForwardMessageFromExternalHost(message, origin,
                                                         target);
}

void RenderView::OnDisassociateFromPopupCount() {
  if (decrement_shared_popup_at_destruction_)
    shared_popup_counter_->data--;
  shared_popup_counter_ = new SharedRenderViewCounter(0);
  decrement_shared_popup_at_destruction_ = false;
}

bool RenderView::MaybeLoadAlternateErrorPage(WebFrame* frame,
                                             const WebURLError& error,
                                             bool replace) {
  // We only show alternate error pages in the main frame.  They are
  // intended to assist the user when navigating, so there is not much
  // value in showing them for failed subframes.  Ideally, we would be
  // able to use the TYPED transition type for this, but that flag is
  // not preserved across page reloads.
  if (frame->parent())
    return false;

  // Use the alternate error page service if this is a DNS failure or
  // connection failure.
  int ec = error.reason;
  if (ec != net::ERR_NAME_NOT_RESOLVED &&
      ec != net::ERR_CONNECTION_FAILED &&
      ec != net::ERR_CONNECTION_REFUSED &&
      ec != net::ERR_ADDRESS_UNREACHABLE &&
      ec != net::ERR_CONNECTION_TIMED_OUT)
    return false;

  const GURL& error_page_url = GetAlternateErrorPageURL(error.unreachableURL,
      ec == net::ERR_NAME_NOT_RESOLVED ? DNS_ERROR : CONNECTION_ERROR);
  if (!error_page_url.is_valid())
    return false;

  // Load an empty page first so there is an immediate response to the error,
  // and then kick off a request for the alternate error page.
  frame->loadHTMLString(std::string(),
                        GURL(kUnreachableWebDataURL),
                        error.unreachableURL,
                        replace);

  // Now, create a fetcher for the error page and associate it with the data
  // source we just created via the LoadHTMLString call.  That way if another
  // navigation occurs, the fetcher will get destroyed.
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->provisionalDataSource());
  navigation_state->set_alt_error_page_fetcher(
      new AltErrorPageResourceFetcher(
          error_page_url, frame, error,
          NewCallback(this, &RenderView::AltErrorPageFinished)));
  return true;
}

std::string RenderView::GetAltHTMLForTemplate(
    const DictionaryValue& error_strings, int template_resource_id) const {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          template_resource_id));

  if (template_html.empty()) {
    NOTREACHED() << "unable to load template. ID: " << template_resource_id;
    return "";
  }

  // "t" is the id of the templates root node.
  return jstemplate_builder::GetTemplatesHtml(
      template_html, &error_strings, "t");
}

void RenderView::AltErrorPageFinished(WebFrame* frame,
                                      const WebURLError& original_error,
                                      const std::string& html) {
  // Here, we replace the blank page we loaded previously.

  // If we failed to download the alternate error page, fall back to the
  // original error page if present.  Otherwise, LoadNavigationErrorPage
  // will simply display a default error page.
  const std::string* html_to_load = &html;
  if (html.empty()) {
    NavigationState* navigation_state =
        NavigationState::FromDataSource(frame->dataSource());
    html_to_load = &navigation_state->postponed_data();
  }
  LoadNavigationErrorPage(frame, WebURLRequest(), original_error, *html_to_load,
                          true);
}

void RenderView::OnMoveOrResizeStarted() {
  if (webview())
    webview()->hidePopups();
}

void RenderView::OnResize(const gfx::Size& new_size,
                          const gfx::Rect& resizer_rect) {
  if (webview()) {
    webview()->hidePopups();

    if (send_preferred_size_changes_) {
      // If resizing to a size larger than |disable_scrollbars_size_limit_| in
      // either width or height, allow scroll bars.
      bool allow_scrollbars = (
          disable_scrollbars_size_limit_.width() <= new_size.width() ||
          disable_scrollbars_size_limit_.height() <= new_size.height());
      webview()->mainFrame()->setCanHaveScrollbars(allow_scrollbars);
    }
  }

  RenderWidget::OnResize(new_size, resizer_rect);
}

void RenderView::DidInitiatePaint() {
  // Notify the pepper plugins that we started painting.
  pepper_delegate_.ViewInitiatedPaint();

  // Notify any "old-style" pepper plugins that we started painting. This is
  // used for internal bookkeeping only, so we know that the set can not change
  // under us.
  for (std::set<WebPluginDelegatePepper*>::iterator i =
           current_oldstyle_pepper_plugins_.begin();
       i != current_oldstyle_pepper_plugins_.end(); ++i)
    (*i)->RenderViewInitiatedPaint();
}

void RenderView::DidFlushPaint() {
  // Notify any pepper plugins that we painted. This will call into the plugin,
  // and we it may ask to close itself as a result. This will, in turn, modify
  // our set, possibly invalidating the iterator. So we iterate on a copy that
  // won't change out from under us.
  pepper_delegate_.ViewFlushedPaint();

  // Notify any old-style pepper plugins that we painted. This will call into
  // the plugin, and we it may ask to close itself as a result. This will, in
  // turn, modify our set, possibly invalidating the iterator. So we iterate on
  // a copy that won't change out from under us.
  // This should be deleted when we don't support old Pepper anymore.
  std::set<WebPluginDelegatePepper*> plugins = current_oldstyle_pepper_plugins_;
  for (std::set<WebPluginDelegatePepper*>::iterator i = plugins.begin();
       i != plugins.end(); ++i) {
    // The copy above makes sure our iterator is never invalid if some plugins
    // are destroyed. But some plugin may decide to close all of its views in
    // response to a paint in one of them, so we need to make sure each one is
    // still "current" before using it.
    if (current_oldstyle_pepper_plugins_.find(*i) !=
        current_oldstyle_pepper_plugins_.end())
      (*i)->RenderViewFlushedPaint();
  }

  WebFrame* main_frame = webview()->mainFrame();

  // If we have a provisional frame we are between the start and commit stages
  // of loading and we don't want to save stats.
  if (!main_frame->provisionalDataSource()) {
    WebDataSource* ds = main_frame->dataSource();
    NavigationState* navigation_state = NavigationState::FromDataSource(ds);
    DCHECK(navigation_state);

    // TODO(jar): The following code should all be inside a method, probably in
    // NavigatorState.
    Time now = Time::Now();
    if (navigation_state->first_paint_time().is_null()) {
      navigation_state->set_first_paint_time(now);
    }
    if (navigation_state->first_paint_after_load_time().is_null() &&
        !navigation_state->finish_load_time().is_null()) {
      navigation_state->set_first_paint_after_load_time(now);
    }
  }
}

void RenderView::OnClearFocusedNode() {
  if (webview())
    webview()->clearFocusedNode();
}

void RenderView::OnSetBackground(const SkBitmap& background) {
  if (webview())
    webview()->setIsTransparent(!background.empty());

  SetBackground(background);
}

void RenderView::OnSetActive(bool active) {
  if (webview())
    webview()->setIsActive(active);

#if defined(OS_MACOSX)
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetWindowFocus(active);
  }
#endif
}

#if defined(OS_MACOSX)
void RenderView::OnSetWindowVisibility(bool visible) {
  // Inform plugins that their container has changed visibility.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(visible);
  }
}

void RenderView::OnWindowFrameChanged(const gfx::Rect& window_frame,
                                      const gfx::Rect& view_frame) {
  // Inform plugins that their window's frame has changed.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->WindowFrameChanged(window_frame, view_frame);
  }
}
#endif  // OS_MACOSX

void RenderView::SendExtensionRequest(
    const ViewHostMsg_DomMessage_Params& params) {
  Send(new ViewHostMsg_ExtensionRequest(routing_id_, params));
}

void RenderView::OnExtensionResponse(int request_id,
                                     bool success,
                                     const std::string& response,
                                     const std::string& error) {
  ExtensionProcessBindings::HandleResponse(
      request_id, success, response, error);
}

void RenderView::OnExtensionMessageInvoke(const std::string& function_name,
                                          const ListValue& args,
                                          bool requires_incognito_access,
                                          const GURL& event_url) {
  RendererExtensionBindings::Invoke(
      function_name, args, this, requires_incognito_access, event_url);
}

// Dump all load time histograms.
//
// The time points we keep are
//    request: time document was requested by user
//    start: time load of document started
//    commit: time load of document started
//    finish_document: main document loaded, before onload()
//    finish_all_loads: after onload() and all resources are loaded
//    first_paint: first paint performed
//    first_paint_after_load: first paint performed after load is finished
//    begin: request if it was user requested, start otherwise
//
// It's possible for the request time not to be set, if a client
// redirect had been done (the user never requested the page)
// Also, it's possible to load a page without ever laying it out
// so first_paint and first_paint_after_load can be 0.
void RenderView::DumpLoadHistograms() const {
  // Configuration for PLT related histograms.
  static const TimeDelta kPLTMin(TimeDelta::FromMilliseconds(10));
  static const TimeDelta kPLTMax(TimeDelta::FromMinutes(10));
  static const size_t kPLTCount(100);
  #define PLT_HISTOGRAM(name, sample) \
      UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, kPLTMin, kPLTMax, kPLTCount);

  WebFrame* main_frame = webview()->mainFrame();
  NavigationState* navigation_state =
      NavigationState::FromDataSource(main_frame->dataSource());

  URLPattern::SchemeMasks scheme_type = navigation_state->scheme_type();
  if (scheme_type == 0)
     return;
  DCHECK(scheme_type == URLPattern::SCHEME_HTTP ||
         scheme_type == URLPattern::SCHEME_HTTPS);

  // If we've already dumped, do nothing.
  if (navigation_state->load_histograms_recorded())
    return;

  // Collect measurement times.
  Time start = navigation_state->start_load_time();
  if (start.is_null())
    return;  // Probably very premature abandonment of page.
  Time commit = navigation_state->commit_load_time();
  if (commit.is_null())
    return;  // Probably very premature abandonment of page.

  // We properly handle null values for the next 3 variables.
  Time request = navigation_state->request_time();
  Time first_paint = navigation_state->first_paint_time();
  Time first_paint_after_load = navigation_state->first_paint_after_load_time();
  Time finish_doc = navigation_state->finish_document_load_time();
  Time finish_all_loads = navigation_state->finish_load_time();

  // Handle case where user hits "stop" or "back" before loading completely.
  bool abandoned_page = finish_doc.is_null();
  if (abandoned_page) {
    finish_doc = Time::Now();
    navigation_state->set_finish_document_load_time(finish_doc);
  }

  // TODO(jar): We should really discriminate the definition of "abandon" more
  // finely.  We should have:
  // abandon_before_document_loaded
  // abandon_before_onload_fired

  if (finish_all_loads.is_null()) {
    finish_all_loads = Time::Now();
    navigation_state->set_finish_load_time(finish_all_loads);
  } else {
    DCHECK(!abandoned_page);  // How can the doc have finished but not the page?
    if (!abandoned_page)
      return;  // Don't try to record a stat which is broken.
  }

  // Note: Client side redirects will have no request time.
  Time begin = request.is_null() ? start : request;
  TimeDelta begin_to_finish_doc = finish_doc - begin;
  TimeDelta begin_to_finish_all_loads = finish_all_loads - begin;
  TimeDelta start_to_finish_all_loads = finish_all_loads - start;
  TimeDelta start_to_commit = commit - start;

  NavigationState::LoadType load_type = navigation_state->load_type();

  // The above code sanitized all values of times, in preparation for creating
  // actual histograms.  The remainder of this code could be run at destructor
  // time for the navigation_state, since all data is intact.

  // Aggregate PLT data across all link types.
  UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned", abandoned_page ? 1 : 0, 2);
  UMA_HISTOGRAM_ENUMERATION("PLT.LoadType", load_type,
      NavigationState::kLoadTypeMax);
  PLT_HISTOGRAM("PLT.StartToCommit", start_to_commit);
  PLT_HISTOGRAM("PLT.CommitToFinishDoc", finish_doc - commit);
  PLT_HISTOGRAM("PLT.FinishDocToFinish", finish_all_loads - finish_doc);
  PLT_HISTOGRAM("PLT.BeginToCommit", commit - begin);
  PLT_HISTOGRAM("PLT.StartToFinish", start_to_finish_all_loads);
  if (!request.is_null()) {
    PLT_HISTOGRAM("PLT.RequestToStart", start - request);
    PLT_HISTOGRAM("PLT.RequestToFinish", finish_all_loads - request);
  }
  PLT_HISTOGRAM("PLT.CommitToFinish", finish_all_loads - commit);
  if (!first_paint.is_null()) {
    DCHECK(begin <= first_paint);
    PLT_HISTOGRAM("PLT.BeginToFirstPaint", first_paint - begin);
    DCHECK(commit <= first_paint);
    PLT_HISTOGRAM("PLT.CommitToFirstPaint", first_paint - commit);
  }
  if (!first_paint_after_load.is_null()) {
    DCHECK(begin <= first_paint_after_load);
    PLT_HISTOGRAM("PLT.BeginToFirstPaintAfterLoad",
      first_paint_after_load - begin);
    DCHECK(commit <= first_paint_after_load);
    PLT_HISTOGRAM("PLT.CommitToFirstPaintAfterLoad",
        first_paint_after_load - commit);
    DCHECK(finish_all_loads <= first_paint_after_load);
    PLT_HISTOGRAM("PLT.FinishToFirstPaintAfterLoad",
        first_paint_after_load - finish_all_loads);
  }
  PLT_HISTOGRAM("PLT.BeginToFinishDoc", begin_to_finish_doc);
  PLT_HISTOGRAM("PLT.BeginToFinish", begin_to_finish_all_loads);

  // Load type related histograms.
  switch (load_type) {
    case NavigationState::UNDEFINED_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_UndefLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_UndefLoad", begin_to_finish_all_loads);
      break;
    case NavigationState::RELOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_Reload", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_Reload", begin_to_finish_all_loads);
      break;
    case NavigationState::HISTORY_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_HistoryLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_HistoryLoad", begin_to_finish_all_loads);
      break;
    case NavigationState::NORMAL_LOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_NormalLoad", begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_NormalLoad", begin_to_finish_all_loads);
      break;
    case NavigationState::LINK_LOAD_NORMAL:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadNormal",
          begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadNormal",
                    begin_to_finish_all_loads);
      break;
    case NavigationState::LINK_LOAD_RELOAD:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadReload",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadReload",
                    begin_to_finish_all_loads);
      break;
    case NavigationState::LINK_LOAD_CACHE_STALE_OK:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadStaleOk",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadStaleOk",
                    begin_to_finish_all_loads);
      break;
    case NavigationState::LINK_LOAD_CACHE_ONLY:
      PLT_HISTOGRAM("PLT.BeginToFinishDoc_LinkLoadCacheOnly",
           begin_to_finish_doc);
      PLT_HISTOGRAM("PLT.BeginToFinish_LinkLoadCacheOnly",
                    begin_to_finish_all_loads);
      break;
    default:
      break;
  }

  // Histograms to determine if DNS prefetching has an impact on PLT.
  static bool use_dns_histogram(FieldTrialList::Find("DnsImpact") &&
      !FieldTrialList::Find("DnsImpact")->group_name().empty());
  if (use_dns_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("PLT.Abandoned", "DnsImpact"),
        abandoned_page ? 1 : 0, 2);
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("PLT.LoadType", "DnsImpact"),
        load_type, NavigationState::kLoadTypeMax);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "DnsImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "DnsImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "DnsImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "DnsImpact"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine if content prefetching has an impact on PLT.
  static const bool prefetching_fieldtrial =
      FieldTrialList::Find("Prefetch") &&
      !FieldTrialList::Find("Prefetch")->group_name().empty();
  if (prefetching_fieldtrial) {
    if (navigation_state->was_prefetcher()) {
      PLT_HISTOGRAM(
          FieldTrial::MakeName("PLT.BeginToFinishDoc_ContentPrefetcher",
                               "Prefetch"),
          begin_to_finish_doc);
      PLT_HISTOGRAM(
          FieldTrial::MakeName("PLT.BeginToFinish_ContentPrefetcher",
                               "Prefetch"),
          begin_to_finish_all_loads);
    }
    if (navigation_state->was_referred_by_prefetcher()) {
      PLT_HISTOGRAM(
          FieldTrial::MakeName("PLT.BeginToFinishDoc_ContentPrefetcherReferrer",
                               "Prefetch"),
          begin_to_finish_doc);
      PLT_HISTOGRAM(
          FieldTrial::MakeName("PLT.BeginToFinish_ContentPrefetcherReferrer",
                               "Prefetch"),
          begin_to_finish_all_loads);
    }
    UMA_HISTOGRAM_ENUMERATION(FieldTrial::MakeName("PLT.Abandoned", "Prefetch"),
                              abandoned_page ? 1 : 0, 2);
    PLT_HISTOGRAM(FieldTrial::MakeName("PLT.BeginToFinishDoc", "Prefetch"),
                  begin_to_finish_doc);
    PLT_HISTOGRAM(FieldTrial::MakeName("PLT.BeginToFinish", "Prefetch"),
                  begin_to_finish_all_loads);
  }

  // Histograms to determine if backup connection jobs have an impact on PLT.
  static const bool connect_backup_jobs_fieldtrial(
      FieldTrialList::Find("ConnnectBackupJobs") &&
      !FieldTrialList::Find("ConnnectBackupJobs")->group_name().empty());
  if (connect_backup_jobs_fieldtrial) {
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("PLT.Abandoned", "ConnnectBackupJobs"),
        abandoned_page ? 1 : 0, 2);
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("PLT.LoadType", "ConnnectBackupJobs"),
        load_type, NavigationState::kLoadTypeMax);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "ConnnectBackupJobs"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "ConnnectBackupJobs"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "ConnnectBackupJobs"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "ConnnectBackupJobs"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine if the number of connections has an
  // impact on PLT.
  // TODO(jar): Consider removing the per-link-type versions.  We
  //   really only need LINK_LOAD_NORMAL and NORMAL_LOAD.
  static bool use_connection_impact_histogram(
      FieldTrialList::Find("ConnCountImpact") &&
      !FieldTrialList::Find("ConnCountImpact")->group_name().empty());
  if (use_connection_impact_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("PLT.Abandoned", "ConnCountImpact"),
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "ConnCountImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "ConnCountImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "ConnCountImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "ConnCountImpact"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine effect of idle socket timeout.
  static bool use_idle_socket_timeout_histogram(
      FieldTrialList::Find("IdleSktToImpact") &&
      !FieldTrialList::Find("IdleSktToImpact")->group_name().empty());
  if (use_idle_socket_timeout_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("PLT.Abandoned", "IdleSktToImpact"),
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "IdleSktToImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "IdleSktToImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "IdleSktToImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "IdleSktToImpact"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine effect of number of connections per proxy.
  static bool use_proxy_connection_impact_histogram(
      FieldTrialList::Find("ProxyConnectionImpact") &&
      !FieldTrialList::Find("ProxyConnectionImpact")->group_name().empty());
  if (use_proxy_connection_impact_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("PLT.Abandoned", "ProxyConnectionImpact"),
        abandoned_page ? 1 : 0, 2);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "ProxyConnectionImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "ProxyConnectionImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "ProxyConnectionImpact"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "ProxyConnectionImpact"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine if SDCH has an impact.
  // TODO(jar): Consider removing per-link load types and the enumeration.
  static bool use_sdch_histogram(FieldTrialList::Find("GlobalSdch") &&
      !FieldTrialList::Find("GlobalSdch")->group_name().empty());
  if (use_sdch_histogram) {
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("PLT.LoadType", "GlobalSdch"),
        load_type, NavigationState::kLoadTypeMax);
    switch (load_type) {
      case NavigationState::NORMAL_LOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_NormalLoad", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_NORMAL:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadNormal", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_RELOAD:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadReload", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_STALE_OK:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadStaleOk", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      case NavigationState::LINK_LOAD_CACHE_ONLY:
        PLT_HISTOGRAM(FieldTrial::MakeName(
            "PLT.BeginToFinish_LinkLoadCacheOnly", "GlobalSdch"),
            begin_to_finish_all_loads);
        break;
      default:
        break;
    }
  }

  // Histograms to determine if cache size has an impact on PLT.
  static bool use_cache_histogram1(FieldTrialList::Find("CacheSize") &&
      !FieldTrialList::Find("CacheSize")->group_name().empty());
  if (use_cache_histogram1 && NavigationState::LINK_LOAD_NORMAL <= load_type &&
      NavigationState::LINK_LOAD_CACHE_ONLY >= load_type) {
    // TODO(mbelshe): Do we really want BeginToFinishDoc here?  It seems like
    //                StartToFinish or BeginToFinish would be better.
    PLT_HISTOGRAM(FieldTrial::MakeName(
        "PLT.BeginToFinishDoc_LinkLoad", "CacheSize"), begin_to_finish_doc);
  }

  // For the SPDY field trials, we need to verify that the page loaded was
  // the type we requested:
  //   if we asked for a SPDY request, we got a SPDY request
  //   if we asked for a HTTP request, we got a HTTP request
  // Due to spdy version mismatches, it is possible that we ask for SPDY
  // but didn't get SPDY.
  static bool use_spdy_histogram(FieldTrialList::Find("SpdyImpact") &&
      !FieldTrialList::Find("SpdyImpact")->group_name().empty());
  if (use_spdy_histogram) {
    // We take extra effort to only compute these once.
    static bool in_spdy_trial =
        FieldTrialList::Find("SpdyImpact")->group_name() == "npn_with_spdy";
    static bool in_http_trial =
        FieldTrialList::Find("SpdyImpact")->group_name() == "npn_with_http";

    bool spdy_trial_success = navigation_state->was_fetched_via_spdy() ?
        in_spdy_trial : in_http_trial;
    if (spdy_trial_success) {
      // Histograms to determine if SPDY has an impact for https traffic.
      // TODO(mbelshe): After we've seen the difference between BeginToFinish
      //                and StartToFinish, consider removing one or the other.
      if (scheme_type == URLPattern::SCHEME_HTTPS &&
          navigation_state->was_npn_negotiated()) {
        UMA_HISTOGRAM_ENUMERATION(
            FieldTrial::MakeName("PLT.Abandoned", "SpdyImpact"),
            abandoned_page ? 1 : 0, 2);
        switch (load_type) {
          case NavigationState::LINK_LOAD_NORMAL:
            PLT_HISTOGRAM(FieldTrial::MakeName(
                "PLT.BeginToFinish_LinkLoadNormal_SpdyTrial", "SpdyImpact"),
                begin_to_finish_all_loads);
            PLT_HISTOGRAM(FieldTrial::MakeName(
                "PLT.StartToFinish_LinkLoadNormal_SpdyTrial", "SpdyImpact"),
                start_to_finish_all_loads);
            PLT_HISTOGRAM(FieldTrial::MakeName(
                "PLT.StartToCommit_LinkLoadNormal_SpdyTrial", "SpdyImpact"),
                start_to_commit);
            break;
          case NavigationState::NORMAL_LOAD:
            PLT_HISTOGRAM(FieldTrial::MakeName(
                "PLT.BeginToFinish_NormalLoad_SpdyTrial", "SpdyImpact"),
                begin_to_finish_all_loads);
            PLT_HISTOGRAM(FieldTrial::MakeName(
                "PLT.StartToFinish_NormalLoad_SpdyTrial", "SpdyImpact"),
                start_to_finish_all_loads);
            PLT_HISTOGRAM(FieldTrial::MakeName(
                "PLT.StartToCommit_NormalLoad_SpdyTrial", "SpdyImpact"),
                start_to_commit);
            break;
          default:
            break;
        }
      }

      // Histograms to compare the impact of alternate protocol over http
      // traffic: when spdy is used vs. when http is used.
      if (scheme_type == URLPattern::SCHEME_HTTP &&
          navigation_state->was_alternate_protocol_available()) {
        if (!navigation_state->was_npn_negotiated()) {
          // This means that even there is alternate protocols for npn_http or
          // npn_spdy, they are not taken (due to the fieldtrial).
          switch (load_type) {
            case NavigationState::LINK_LOAD_NORMAL:
              PLT_HISTOGRAM(
                  "PLT.StartToFinish_LinkLoadNormal_AlternateProtocol_http",
                  start_to_finish_all_loads);
              PLT_HISTOGRAM(
                  "PLT.StartToCommit_LinkLoadNormal_AlternateProtocol_http",
                  start_to_commit);
              break;
            case NavigationState::NORMAL_LOAD:
              PLT_HISTOGRAM(
                  "PLT.StartToFinish_NormalLoad_AlternateProtocol_http",
                  start_to_finish_all_loads);
              PLT_HISTOGRAM(
                  "PLT.StartToCommit_NormalLoad_AlternateProtocol_http",
                  start_to_commit);
              break;
            default:
              break;
          }
        } else if (navigation_state->was_fetched_via_spdy()) {
          switch (load_type) {
            case NavigationState::LINK_LOAD_NORMAL:
              PLT_HISTOGRAM(
                  "PLT.StartToFinish_LinkLoadNormal_AlternateProtocol_spdy",
                  start_to_finish_all_loads);
              PLT_HISTOGRAM(
                  "PLT.StartToCommit_LinkLoadNormal_AlternateProtocol_spdy",
                  start_to_commit);
              break;
            case NavigationState::NORMAL_LOAD:
              PLT_HISTOGRAM(
                  "PLT.StartToFinish_NormalLoad_AlternateProtocol_spdy",
                  start_to_finish_all_loads);
              PLT_HISTOGRAM(
                  "PLT.StartToCommit_NormalLoad_AlternateProtocol_spdy",
                  start_to_commit);
              break;
            default:
              break;
          }
        }
      }
    }
  }

  // Record page load time and abandonment rates for proxy cases.
  if (navigation_state->was_fetched_via_proxy()) {
    if (scheme_type == URLPattern::SCHEME_HTTPS) {
      PLT_HISTOGRAM("PLT.StartToFinish.Proxy.https", start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.Proxy.https",
                                abandoned_page ? 1 : 0, 2);
    } else {
      DCHECK(scheme_type == URLPattern::SCHEME_HTTP);
      PLT_HISTOGRAM("PLT.StartToFinish.Proxy.http", start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.Proxy.http",
                                abandoned_page ? 1 : 0, 2);
    }
  } else {
    if (scheme_type == URLPattern::SCHEME_HTTPS) {
      PLT_HISTOGRAM("PLT.StartToFinish.NoProxy.https",
                    start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.NoProxy.https",
                                abandoned_page ? 1 : 0, 2);
    } else {
      DCHECK(scheme_type == URLPattern::SCHEME_HTTP);
      PLT_HISTOGRAM("PLT.StartToFinish.NoProxy.http",
                    start_to_finish_all_loads);
      UMA_HISTOGRAM_ENUMERATION("PLT.Abandoned.NoProxy.http",
                                abandoned_page ? 1 : 0, 2);
    }
  }

  // Site isolation metrics.
  UMA_HISTOGRAM_COUNTS("SiteIsolation.PageLoadsWithCrossSiteFrameAccess",
                       cross_origin_access_count_);
  UMA_HISTOGRAM_COUNTS("SiteIsolation.PageLoadsWithSameSiteFrameAccess",
                       same_origin_access_count_);

  // TODO(jar): This is the ONLY call in this method that needed more than
  // navigation_state.  This should be relocated, or changed, so that this
  // body can be made a method on NavigationStates, and run via its destructor.
  // Log some PLT data to the error log.
  LogNavigationState(navigation_state, main_frame->dataSource());

  navigation_state->set_load_histograms_recorded(true);

  // Since there are currently no guarantees that renderer histograms will be
  // sent to the browser, we initiate a PostTask here to be sure that we send
  // the histograms we generated.  Without this call, pages that don't have an
  // on-close-handler might generate data that is lost when the renderer is
  // shutdown abruptly (perchance because the user closed the tab).
  // TODO(jar) BUG=33233: This needs to be moved to a PostDelayedTask, and it
  // should post when the onload is complete, so that it doesn't interfere with
  // the next load.
  if (RenderThread::current()) {
    RenderThread::current()->SendHistograms(
        chrome::kHistogramSynchronizerReservedSequenceNumber);
  }
}

void RenderView::LogNavigationState(const NavigationState* state,
                                    const WebDataSource* ds) const {
  // Because this function gets called on every page load,
  // take extra care to optimize it away if logging is turned off.
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;

  DCHECK(state);
  DCHECK(ds);
  GURL url(ds->request().url());
  Time start = state->start_load_time();
  Time finish = state->finish_load_time();
  // TODO(mbelshe): should we log more stats?
  LOG(INFO) << "PLT: "
            << (finish - start).InMilliseconds()
            << "ms "
            << url.spec();
}

void RenderView::focusAccessibilityObject(
    const WebAccessibilityObject& acc_obj) {
#if defined(OS_WIN)
  // TODO(dglazkov): Current logic implies that focus change can only be made
  // after at least one call to RenderView::OnGetAccessibilityInfo, which is
  // where accessibility is initialized. We should determine whether that's
  // right.
  if (!accessibility_.get())
    return;

  // Retrieve the accessibility object id of the AccessibilityObject.
  int acc_obj_id = accessibility_->addOrGetId(acc_obj);

  // If id is valid, alert the browser side that an accessibility focus change
  // occurred.
  if (acc_obj_id >= 0)
    Send(new ViewHostMsg_AccessibilityFocusChange(routing_id_, acc_obj_id));

#else  // defined(OS_WIN)
  // TODO(port): accessibility not yet implemented
  NOTIMPLEMENTED();
#endif
}

void RenderView::didChangeAccessibilityObjectState(
    const WebAccessibilityObject& acc_obj) {
  if (!accessibility_.get())
    return;

  Send(new ViewHostMsg_AccessibilityObjectStateChange(
      routing_id_, WebAccessibility(acc_obj, accessibility_.get())));
}

void RenderView::didChangeAccessibilityObjectChildren(
    const WebAccessibilityObject& acc_obj) {
  if (!accessibility_.get())
    return;

  if (accessibility_changes_.empty()) {
    Send(new ViewHostMsg_AccessibilityObjectChildrenChange(
        routing_id_, std::vector<WebAccessibility>()));
  }

  accessibility_changes_.push_back(
      WebAccessibility(acc_obj, accessibility_.get()));
}

void RenderView::Print(WebFrame* frame, bool script_initiated) {
  DCHECK(frame);
  if (print_helper_.get() == NULL) {
    print_helper_.reset(new PrintWebViewHelper(this));
  }
  print_helper_->Print(frame, script_initiated);
}

void RenderView::OnSetEditCommandsForNextKeyEvent(
    const EditCommands& edit_commands) {
  edit_commands_ = edit_commands;
}

void RenderView::OnExecuteCode(const ViewMsg_ExecuteCode_Params& params) {
  WebFrame* main_frame = webview() ? webview()->mainFrame() : NULL;
  if (!main_frame) {
    Send(new ViewMsg_ExecuteCodeFinished(routing_id_, params.request_id,
                                         false));
    return;
  }

  WebDataSource* ds = main_frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  if (!navigation_state->user_script_idle_scheduler()->has_run()) {
    pending_code_execution_queue_.push(
        linked_ptr<ViewMsg_ExecuteCode_Params>(
            new ViewMsg_ExecuteCode_Params(params)));
    return;
  }

  ExecuteCodeImpl(main_frame, params);
}

void RenderView::ExecuteCodeImpl(WebFrame* frame,
                                 const ViewMsg_ExecuteCode_Params& params) {
  // Don't execute scripts in gallery pages.
  GURL frame_url = GURL(frame->url());
  if (frame_url.host() == GURL(Extension::ChromeStoreURL()).host()
      && !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowScriptingGallery)) {
    Send(new ViewMsg_ExecuteCodeFinished(routing_id_, params.request_id, true));
    return;
  }

  std::vector<WebFrame*> frame_vector;
  frame_vector.push_back(frame);
  if (params.all_frames)
    GetAllChildFrames(frame, &frame_vector);

  for (std::vector<WebFrame*>::iterator frame_it = frame_vector.begin();
       frame_it != frame_vector.end(); ++frame_it) {
    WebFrame* frame = *frame_it;
    if (params.is_javascript) {
      if (!UrlMatchesPermissions(frame->url(), params.host_permissions))
        continue;

      std::vector<WebScriptSource> sources;
      sources.push_back(
          WebScriptSource(WebString::fromUTF8(params.code)));
      UserScriptSlave::InsertInitExtensionCode(&sources, params.extension_id);
      frame->executeScriptInIsolatedWorld(
          UserScriptSlave::GetIsolatedWorldId(params.extension_id),
          &sources.front(), sources.size(), EXTENSION_GROUP_CONTENT_SCRIPTS);
    } else {
      frame->insertStyleText(WebString::fromUTF8(params.code), WebString());
    }
  }

  Send(new ViewMsg_ExecuteCodeFinished(routing_id_, params.request_id, true));
}

void RenderView::Close() {
  // We need to grab a pointer to the doomed WebView before we destroy it.
  WebView* doomed = webview();
  RenderWidget::Close();
  Singleton<ViewMap>::get()->erase(doomed);
}

void RenderView::DidHandleKeyEvent() {
  edit_commands_.clear();
}

void RenderView::DidHandleMouseEvent(const WebKit::WebMouseEvent& event) {
  page_click_tracker_->DidHandleMouseEvent(event);
}

#if defined(OS_MACOSX)
void RenderView::OnWasHidden() {
  RenderWidget::OnWasHidden();

  // Inform plugins that their container is no longer visible.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(false);
  }
}

void RenderView::OnWasRestored(bool needs_repainting) {
  RenderWidget::OnWasRestored(needs_repainting);

  // Inform plugins that their container is now visible.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(true);
  }
}

void RenderView::OnSetFocus(bool enable) {
  RenderWidget::OnSetFocus(enable);

  if (webview() && webview()->isActive()) {
    std::set<WebPluginDelegateProxy*>::iterator plugin_it;
    for (plugin_it = plugin_delegates_.begin();
         plugin_it != plugin_delegates_.end(); ++plugin_it) {
      // RenderWidget's call to setFocus can cause the underlying webview's
      // activation state to change just like a call to setIsActive.
      if (enable)
        (*plugin_it)->SetWindowFocus(true);
      (*plugin_it)->SetContentAreaFocus(enable);
    }
  }
}
#endif  // OS_MACOSX

void RenderView::EnsureDocumentTag() {
  // TODO(darin): There's actually no reason for this to be here.  We should
  // have the browser side manage the document tag.
#if defined(OS_MACOSX)
  if (!has_document_tag_) {
    // Make the call to get the tag.
    Send(new ViewHostMsg_GetDocumentTag(routing_id_, &document_tag_));
    has_document_tag_ = true;
  }
#endif
}

#if defined(OS_MACOSX)
gfx::PluginWindowHandle RenderView::AllocateFakePluginWindowHandle(
    bool opaque, bool root) {
  gfx::PluginWindowHandle window = NULL;
  Send(new ViewHostMsg_AllocateFakePluginWindowHandle(
      routing_id(), opaque, root, &window));
  return window;
}

void RenderView::DestroyFakePluginWindowHandle(gfx::PluginWindowHandle window) {
  if (window)
    Send(new ViewHostMsg_DestroyFakePluginWindowHandle(routing_id(), window));
}

void RenderView::AcceleratedSurfaceSetIOSurface(gfx::PluginWindowHandle window,
                                                int32 width,
                                                int32 height,
                                                uint64 io_surface_identifier) {
  Send(new ViewHostMsg_AcceleratedSurfaceSetIOSurface(
      routing_id(), window, width, height, io_surface_identifier));
}

void RenderView::AcceleratedSurfaceSetTransportDIB(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  Send(new ViewHostMsg_AcceleratedSurfaceSetTransportDIB(
      routing_id(), window, width, height, transport_dib));
}

TransportDIB::Handle RenderView::AcceleratedSurfaceAllocTransportDIB(
    size_t size) {
  TransportDIB::Handle dib_handle;
  // Assume this is a synchronous RPC.
  if (Send(new ViewHostMsg_AllocTransportDIB(size, true, &dib_handle)))
    return dib_handle;
  // Return an invalid handle if Send() fails.
  return TransportDIB::DefaultHandleValue();
}

void RenderView::AcceleratedSurfaceFreeTransportDIB(TransportDIB::Id dib_id) {
  Send(new ViewHostMsg_FreeTransportDIB(dib_id));
}

void RenderView::AcceleratedSurfaceBuffersSwapped(
    gfx::PluginWindowHandle window) {
  Send(new ViewHostMsg_AcceleratedSurfaceBuffersSwapped(routing_id(), window));
}
#endif

bool RenderView::ScheduleFileChooser(
    const ViewHostMsg_RunFileChooser_Params& params,
    WebFileChooserCompletion* completion) {
  static const size_t kMaximumPendingFileChooseRequests = 4;
  if (file_chooser_completions_.size() > kMaximumPendingFileChooseRequests) {
    // This sanity check prevents too many file choose requests from getting
    // queued which could DoS the user. Getting these is most likely a
    // programming error (there are many ways to DoS the user so it's not
    // considered a "real" security check), either in JS requesting many file
    // choosers to pop up, or in a plugin.
    //
    // TODO(brettw) we might possibly want to require a user gesture to open
    // a file picker, which will address this issue in a better way.
    return false;
  }

  file_chooser_completions_.push_back(linked_ptr<PendingFileChooser>(
      new PendingFileChooser(params, completion)));
  if (file_chooser_completions_.size() == 1) {
    // Actually show the browse dialog when this is the first request.
    Send(new ViewHostMsg_RunFileChooser(routing_id_, params));
  }
  return true;
}

void RenderView::OnPageTranslated() {
  WebFrame* frame = webview()->mainFrame();
  if (!frame)
    return;

  // The page is translated, so try to extract the form data again.
  autofill_helper_->FrameContentsAvailable(frame);
}

WebKit::WebGeolocationService* RenderView::geolocationService() {
  if (!geolocation_dispatcher_.get())
    geolocation_dispatcher_.reset(new GeolocationDispatcher(this));
  return geolocation_dispatcher_.get();
}

WebKit::WebSpeechInputController* RenderView::speechInputController(
    WebKit::WebSpeechInputListener* listener) {
  if (!speech_input_dispatcher_.get())
    speech_input_dispatcher_.reset(new SpeechInputDispatcher(this, listener));
  return speech_input_dispatcher_.get();
}

WebKit::WebDeviceOrientationClient* RenderView::deviceOrientationClient() {
  if (!device_orientation_dispatcher_.get())
    device_orientation_dispatcher_.reset(new DeviceOrientationDispatcher(this));
  return device_orientation_dispatcher_.get();
}

bool RenderView::IsNonLocalTopLevelNavigation(
    const GURL& url, WebKit::WebFrame* frame, WebKit::WebNavigationType type) {
  // Must be a top level frame.
  if (frame->parent() != NULL)
    return false;

  // Navigations initiated within Webkit are not sent out to the external host
  // in the following cases.
  // 1. The url scheme is not http/https
  // 2. There is no opener and this is not the first url being opened by this
  //    RenderView.
  // 3. The origin of the url and the opener is the same in which case the
  //    opener relationship is maintained.
  // 4. Reloads/form submits/back forward navigations
  if (!url.SchemeIs("http") && !url.SchemeIs("https"))
    return false;

  // Not interested in reloads.
  if (type != WebKit::WebNavigationTypeReload &&
      type != WebKit::WebNavigationTypeFormSubmitted &&
      type != WebKit::WebNavigationTypeBackForward) {
    // The opener relationship between the new window and the parent allows the
    // new window to script the parent and vice versa. This is not allowed if
    // the origins of the two domains are different. This can be treated as a
    // top level navigation and routed back to the host.
    WebKit::WebFrame* opener = frame->opener();
    if (!opener) {
      // Force link click navigations to always be routed to the host as they
      // may update session state on the server.
      if (type == WebKit::WebNavigationTypeLinkClicked)
        return true;
      // If this is the first page being loaded by this RenderView instance then
      // it should stay here.
      if (page_id_ == -1) {
        return false;
      } else {
        return true;
      }
    }

    if (url.GetOrigin() != GURL(opener->url()).GetOrigin())
      return true;
  }
  return false;
}

void RenderView::OnOpenFileSystemRequestComplete(
    int request_id, bool accepted, const string16& name,
    const string16& root_path) {
  PendingOpenFileSystem* request = pending_file_system_requests_.Lookup(
      request_id);
  DCHECK(request);
  if (accepted)
    request->callbacks->didOpenFileSystem(name, root_path);
  else
    request->callbacks->didFail(WebKit::WebFileErrorSecurity);
  request->callbacks = NULL;
  pending_file_system_requests_.Remove(request_id);
}

void RenderView::OnAsyncFileOpened(base::PlatformFileError error_code,
                                   IPC::PlatformFileForTransit file_for_transit,
                                   int message_id) {
  pepper_delegate_.OnAsyncFileOpened(
      error_code,
      IPC::PlatformFileForTransitToPlatformFile(file_for_transit),
      message_id);
}
