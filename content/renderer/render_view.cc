// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "content/common/appcache/appcache_dispatcher.h"
#include "content/common/bindings_policy.h"
#include "content/common/clipboard_messages.h"
#include "content/common/content_constants.h"
#include "content/common/content_switches.h"
#include "content/common/database_messages.h"
#include "content/common/drag_messages.h"
#include "content/common/file_system/file_system_dispatcher.h"
#include "content/common/file_system/webfilesystem_callback_dispatcher.h"
#include "content/common/json_value_serializer.h"
#include "content/common/notification_service.h"
#include "content/common/pepper_messages.h"
#include "content/common/pepper_plugin_registry.h"
#include "content/common/quota_dispatcher.h"
#include "content/common/renderer_preferences.h"
#include "content/common/request_extra_data.h"
#include "content/common/url_constants.h"
#include "content/common/view_messages.h"
#include "content/renderer/content_renderer_client.h"
#include "content/renderer/devtools_agent.h"
#include "content/renderer/device_orientation_dispatcher.h"
#include "content/renderer/mhtml_generator.h"
#include "content/renderer/external_popup_menu.h"
#include "content/renderer/geolocation_dispatcher.h"
#include "content/renderer/gpu/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/load_progress_tracker.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/audio_renderer_impl.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/media/render_media_log.h"
#include "content/renderer/navigation_state.h"
#include "content/renderer/notification_provider.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "content/renderer/plugin_channel_host.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_view_observer.h"
#include "content/renderer/render_view_visitor.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_webapplicationcachehost_impl.h"
#include "content/renderer/renderer_webstoragenamespace_impl.h"
#include "content/renderer/speech_input_dispatcher.h"
#include "content/renderer/v8_value_converter.h"
#include "content/renderer/web_ui_bindings.h"
#include "content/renderer/webplugin_delegate_proxy.h"
#include "content/renderer/websharedworker_proxy.h"
#include "content/renderer/webworker_proxy.h"
#include "media/base/filter_collection.h"
#include "media/base/media_switches.h"
#include "media/base/message_loop_factory_impl.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFindOptions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeList.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageSerializer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPlugin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSearchableFormData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSettings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageQuotaCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWindowFeatures.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/message_box_flags.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "v8/include/v8.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/alt_error_page_resource_fetcher.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webkit_constants.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/glue/weburlloader_impl.h"
#include "webkit/plugins/npapi/default_plugin_shared.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_delegate.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/plugins/npapi/webplugin_impl.h"
#include "webkit/plugins/npapi/webview_plugin.h"
#include "webkit/plugins/ppapi/ppapi_webplugin_impl.h"

#if defined(OS_WIN)
// TODO(port): these files are currently Windows only because they concern:
//   * theming
#include "ui/gfx/native_theme_win.h"
#elif defined(USE_X11)
#include "third_party/WebKit/Source/WebKit/chromium/public/linux/WebRenderTheme.h"
#include "ui/gfx/native_theme.h"
#elif defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif

using WebKit::WebAccessibilityCache;
using WebKit::WebAccessibilityNotification;
using WebKit::WebAccessibilityObject;
using WebKit::WebApplicationCacheHost;
using WebKit::WebApplicationCacheHostClient;
using WebKit::WebCString;
using WebKit::WebColor;
using WebKit::WebColorName;
using WebKit::WebConsoleMessage;
using WebKit::WebContextMenuData;
using WebKit::WebCookieJar;
using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebDocument;
using WebKit::WebDragData;
using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using WebKit::WebEditingAction;
using WebKit::WebElement;
using WebKit::WebExternalPopupMenu;
using WebKit::WebExternalPopupMenuClient;
using WebKit::WebFileChooserCompletion;
using WebKit::WebFileSystem;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFindOptions;
using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebHistoryItem;
using WebKit::WebIconURL;
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
using WebKit::WebSecurityPolicy;
using WebKit::WebSettings;
using WebKit::WebSharedWorker;
using WebKit::WebSize;
using WebKit::WebStorageNamespace;
using WebKit::WebStorageQuotaCallbacks;
using WebKit::WebStorageQuotaError;
using WebKit::WebStorageQuotaType;
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
using WebKit::WebWindowFeatures;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;
using appcache::WebApplicationCacheHostImpl;
using base::Time;
using base::TimeDelta;
using webkit_glue::AltErrorPageResourceFetcher;
using webkit_glue::FormField;
using webkit_glue::PasswordForm;
using webkit_glue::PasswordFormDomManager;
using webkit_glue::ResourceFetcher;
using webkit_glue::WebAccessibility;

//-----------------------------------------------------------------------------

typedef std::map<WebKit::WebView*, RenderView*> ViewMap;
static base::LazyInstance<ViewMap> g_view_map(base::LINKER_INITIALIZED);

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

static const float kScalingIncrement = 0.1f;

static void GetRedirectChain(WebDataSource* ds, std::vector<GURL>* result) {
  WebVector<WebURL> urls;
  ds->redirectChain(urls);
  result->reserve(urls.size());
  for (size_t i = 0; i < urls.size(); ++i)
    result->push_back(urls[i]);
}

static bool WebAccessibilityNotificationToViewHostMsg(
    WebAccessibilityNotification notification,
    ViewHostMsg_AccEvent::Value* type) {
  switch (notification) {
    case WebKit::WebAccessibilityNotificationActiveDescendantChanged:
      *type = ViewHostMsg_AccEvent::ACTIVE_DESCENDANT_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationCheckedStateChanged:
      *type = ViewHostMsg_AccEvent::CHECK_STATE_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationChildrenChanged:
      *type = ViewHostMsg_AccEvent::CHILDREN_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationFocusedUIElementChanged:
      *type = ViewHostMsg_AccEvent::FOCUS_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationLayoutComplete:
      *type = ViewHostMsg_AccEvent::LAYOUT_COMPLETE;
      break;
    case WebKit::WebAccessibilityNotificationLiveRegionChanged:
      *type = ViewHostMsg_AccEvent::LIVE_REGION_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationLoadComplete:
      *type = ViewHostMsg_AccEvent::LOAD_COMPLETE;
      break;
    case WebKit::WebAccessibilityNotificationMenuListValueChanged:
      *type = ViewHostMsg_AccEvent::MENU_LIST_VALUE_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationRowCollapsed:
      *type = ViewHostMsg_AccEvent::ROW_COLLAPSED;
      break;
    case WebKit::WebAccessibilityNotificationRowCountChanged:
      *type = ViewHostMsg_AccEvent::ROW_COUNT_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationRowExpanded:
      *type = ViewHostMsg_AccEvent::ROW_EXPANDED;
      break;
    case WebKit::WebAccessibilityNotificationScrolledToAnchor:
      *type = ViewHostMsg_AccEvent::SCROLLED_TO_ANCHOR;
      break;
    case WebKit::WebAccessibilityNotificationSelectedChildrenChanged:
      *type = ViewHostMsg_AccEvent::SELECTED_CHILDREN_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationSelectedTextChanged:
      *type = ViewHostMsg_AccEvent::SELECTED_TEXT_CHANGED;
      break;
    case WebKit::WebAccessibilityNotificationValueChanged:
      *type = ViewHostMsg_AccEvent::VALUE_CHANGED;
      break;
    default:
      return false;
  }
  return true;
}

// If |data_source| is non-null and has a NavigationState associated with it,
// the AltErrorPageResourceFetcher is reset.
static void StopAltErrorPageFetcher(WebDataSource* data_source) {
  if (data_source) {
    NavigationState* state = NavigationState::FromDataSource(data_source);
    if (state)
      state->set_alt_error_page_fetcher(NULL);
  }
}

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
                       gfx::NativeViewId parent_hwnd,
                       int32 opener_id,
                       const RendererPreferences& renderer_prefs,
                       const WebPreferences& webkit_prefs,
                       SharedRenderViewCounter* counter,
                       int32 routing_id,
                       int64 session_storage_namespace_id,
                       const string16& frame_name)
    : RenderWidget(render_thread, WebKit::WebPopupTypeNone),
      webkit_preferences_(webkit_prefs),
      send_content_state_immediately_(false),
      enabled_bindings_(0),
      send_preferred_size_changes_(false),
      is_loading_(false),
      navigation_gesture_(NavigationGestureUnknown),
      opened_by_user_gesture_(true),
      opener_suppressed_(false),
      page_id_(-1),
      last_page_id_sent_to_browser_(-1),
      history_list_offset_(-1),
      history_list_length_(0),
      target_url_status_(TARGET_NONE),
      cached_is_main_frame_pinned_to_left_(false),
      cached_is_main_frame_pinned_to_right_(false),
      cached_has_main_frame_horizontal_scrollbar_(false),
      cached_has_main_frame_vertical_scrollbar_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(pepper_delegate_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(accessibility_method_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(cookie_jar_(this)),
      geolocation_dispatcher_(NULL),
      speech_input_dispatcher_(NULL),
      device_orientation_dispatcher_(NULL),
      accessibility_ack_pending_(false),
      accessibility_logging_(false),
      p2p_socket_dispatcher_(NULL),
      devtools_agent_(NULL),
      session_storage_namespace_id_(session_storage_namespace_id),
      handling_select_range_(false) {
  routing_id_ = routing_id;
  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  webwidget_ = WebView::create(this);

  if (counter) {
    shared_popup_counter_ = counter;
    shared_popup_counter_->data++;
    decrement_shared_popup_at_destruction_ = true;
  } else {
    shared_popup_counter_ = new SharedRenderViewCounter(0);
    decrement_shared_popup_at_destruction_ = false;
  }

  notification_provider_ = new NotificationProvider(this);

  render_thread_->AddRoute(routing_id_, this);
  // Take a reference on behalf of the RenderThread.  This will be balanced
  // when we receive ViewMsg_ClosePage.
  AddRef();

  // If this is a popup, we must wait for the CreatingNew_ACK message before
  // completing initialization.  Otherwise, we can finish it now.
  if (opener_id == MSG_ROUTING_NONE) {
    did_show_ = true;
    CompleteInit(parent_hwnd);
  }

  g_view_map.Get().insert(std::make_pair(webview(), this));
  webkit_preferences_.Apply(webview());
  webview()->initializeMainFrame(this);
  webview()->settings()->setMinimumTimerInterval(
      is_hidden() ? webkit_glue::kBackgroundTabTimerInterval :
          webkit_glue::kForegroundTabTimerInterval);

  OnSetRendererPrefs(renderer_prefs);

  host_window_ = parent_hwnd;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableAccessibility))
    WebAccessibilityCache::enableAccessibility();
  if (command_line.HasSwitch(switches::kEnableAccessibilityLogging))
    accessibility_logging_ = true;

#if defined(ENABLE_P2P_APIS)
  p2p_socket_dispatcher_ = new content::P2PSocketDispatcher(this);
#endif

  new MHTMLGenerator(this);

  devtools_agent_ = new DevToolsAgent(this);

  if (command_line.HasSwitch(switches::kEnableMediaStream)) {
    media_stream_impl_ = new MediaStreamImpl(
        RenderThread::current()->video_capture_impl_manager());
  }

  content::GetContentClient()->renderer()->RenderViewCreated(this);
}

RenderView::~RenderView() {
  history_page_ids_.clear();

  if (decrement_shared_popup_at_destruction_)
    shared_popup_counter_->data--;

  // If file chooser is still waiting for answer, dispatch empty answer.
  while (!file_chooser_completions_.empty()) {
    if (file_chooser_completions_.front()->completion) {
      file_chooser_completions_.front()->completion->didChooseFile(
          WebVector<WebString>());
    }
    file_chooser_completions_.pop_front();
  }

#if defined(OS_MACOSX)
  // Destroy all fake plugin window handles on the browser side.
  while (!fake_plugin_window_handles_.empty()) {
    // Make sure no NULL plugin window handles were inserted into this list.
    DCHECK(*fake_plugin_window_handles_.begin());
    // DestroyFakePluginWindowHandle modifies fake_plugin_window_handles_.
    DestroyFakePluginWindowHandle(*fake_plugin_window_handles_.begin());
  }
#endif

#ifndef NDEBUG
  // Make sure we are no longer referenced by the ViewMap.
  ViewMap* views = g_view_map.Pointer();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
#endif

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, set_render_view(NULL));
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, OnDestruct());
}

/*static*/
void RenderView::ForEach(RenderViewVisitor* visitor) {
  ViewMap* views = g_view_map.Pointer();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it) {
    if (!visitor->Visit(it->second))
      return;
  }
}

/*static*/
RenderView* RenderView::FromWebView(WebView* webview) {
  ViewMap* views = g_view_map.Pointer();
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
  return new RenderView(
      render_thread,
      parent_hwnd,
      opener_id,
      renderer_prefs,
      webkit_prefs,
      counter,
      routing_id,
      session_storage_namespace_id,
      frame_name);  // adds reference
}

// static
void RenderView::SetNextPageID(int32 next_page_id) {
  // This method should only be called during process startup, and the given
  // page id had better not exceed our current next page id!
  DCHECK_EQ(next_page_id_, 1);
  DCHECK(next_page_id >= next_page_id_);
  next_page_id_ = next_page_id;
}

void RenderView::AddObserver(RenderViewObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderView::RemoveObserver(RenderViewObserver* observer) {
  observer->set_render_view(NULL);
  observers_.RemoveObserver(observer);
}

bool RenderView::RendererAccessibilityNotification::ShouldIncludeChildren() {
  typedef ViewHostMsg_AccessibilityNotification_Params params;
  if (type == WebKit::WebAccessibilityNotificationChildrenChanged ||
      type == WebKit::WebAccessibilityNotificationLoadComplete ||
      type == WebKit::WebAccessibilityNotificationLiveRegionChanged) {
    return true;
  }
  return false;
}

WebKit::WebView* RenderView::webview() const {
  return static_cast<WebKit::WebView*>(webwidget());
}

void RenderView::SetReportLoadProgressEnabled(bool enabled) {
  if (!enabled) {
    load_progress_tracker_.reset(NULL);
    return;
  }
  if (load_progress_tracker_ == NULL)
    load_progress_tracker_.reset(new LoadProgressTracker(this));
}

void RenderView::PluginCrashed(const FilePath& plugin_path) {
  Send(new ViewHostMsg_CrashedPlugin(routing_id_, plugin_path));
}

WebPlugin* RenderView::CreatePluginNoCheck(WebFrame* frame,
                                           const WebPluginParams& params) {
  webkit::WebPluginInfo info;
  bool found;
  std::string mime_type;
  Send(new ViewHostMsg_GetPluginInfo(
      routing_id_, params.url, frame->top()->document().url(),
      params.mimeType.utf8(), &found, &info, &mime_type));
  if (!found || !webkit::IsPluginEnabled(info))
    return NULL;

  bool pepper_plugin_was_registered = false;
  scoped_refptr<webkit::ppapi::PluginModule> pepper_module(
      pepper_delegate_.CreatePepperPluginModule(info,
                                                &pepper_plugin_was_registered));
  if (pepper_plugin_was_registered) {
    if (pepper_module)
      return CreatePepperPlugin(frame, params, info.path, pepper_module.get());
    return NULL;
  }
  return CreateNPAPIPlugin(frame, params, info.path, mime_type);
}

void RenderView::RegisterPluginDelegate(WebPluginDelegateProxy* delegate) {
  plugin_delegates_.insert(delegate);
  // If the renderer is visible, set initial visibility and focus state.
  if (!is_hidden()) {
#if defined(OS_MACOSX)
    delegate->SetContainerVisibility(true);
    if (webview() && webview()->isActive())
      delegate->SetWindowFocus(true);
#endif
  }
  // Plugins start assuming the content has focus (so that they work in
  // environments where RenderView isn't hosting them), so we always have to
  // set the initial state. See webplugin_delegate_impl.h for details.
  delegate->SetContentAreaFocus(has_focus());
}

void RenderView::UnregisterPluginDelegate(WebPluginDelegateProxy* delegate) {
  plugin_delegates_.erase(delegate);
}

bool RenderView::OnMessageReceived(const IPC::Message& message) {
  WebFrame* main_frame = webview() ? webview()->mainFrame() : NULL;
  if (main_frame)
    content::GetContentClient()->SetActiveURL(main_frame->document().url());

  ObserverListBase<RenderViewObserver>::Iterator it(observers_);
  RenderViewObserver* observer;
  while ((observer = it.GetNext()) != NULL)
    if (observer->OnMessageReceived(message))
      return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderView, message)
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
    IPC_MESSAGE_HANDLER(ViewMsg_Delete, OnDelete)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectAll, OnSelectAll)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectRange, OnSelectRange)
    IPC_MESSAGE_HANDLER(ViewMsg_CopyImageAt, OnCopyImageAt)
    IPC_MESSAGE_HANDLER(ViewMsg_ExecuteEditCommand, OnExecuteEditCommand)
    IPC_MESSAGE_HANDLER(ViewMsg_Find, OnFind)
    IPC_MESSAGE_HANDLER(ViewMsg_StopFinding, OnStopFinding)
    IPC_MESSAGE_HANDLER(ViewMsg_FindReplyACK, OnFindReplyAck)
    IPC_MESSAGE_HANDLER(ViewMsg_Zoom, OnZoom)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevel, OnSetZoomLevel)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForLoadingURL,
                        OnSetZoomLevelForLoadingURL)
    IPC_MESSAGE_HANDLER(ViewMsg_ExitFullscreen, OnExitFullscreen)
    IPC_MESSAGE_HANDLER(ViewMsg_SetPageEncoding, OnSetPageEncoding)
    IPC_MESSAGE_HANDLER(ViewMsg_ResetPageEncodingToDefault,
                        OnResetPageEncodingToDefault)
    IPC_MESSAGE_HANDLER(ViewMsg_ScriptEvalRequest, OnScriptEvalRequest)
    IPC_MESSAGE_HANDLER(ViewMsg_CSSInsertRequest, OnCSSInsertRequest)
    IPC_MESSAGE_HANDLER(ViewMsg_ReservePageIDRange, OnReservePageIDRange)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragEnter, OnDragTargetDragEnter)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragOver, OnDragTargetDragOver)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDragLeave, OnDragTargetDragLeave)
    IPC_MESSAGE_HANDLER(DragMsg_TargetDrop, OnDragTargetDrop)
    IPC_MESSAGE_HANDLER(DragMsg_SourceEndedOrMoved, OnDragSourceEndedOrMoved)
    IPC_MESSAGE_HANDLER(DragMsg_SourceSystemDragEnded,
                        OnDragSourceSystemDragEnded)
    IPC_MESSAGE_HANDLER(ViewMsg_AllowBindings, OnAllowBindings)
    IPC_MESSAGE_HANDLER(ViewMsg_SetWebUIProperty, OnSetWebUIProperty)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInitialFocus, OnSetInitialFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_ScrollFocusedEditableNodeIntoView,
                        OnScrollFocusedEditableNodeIntoView)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTargetURL_ACK, OnUpdateTargetURLAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateWebPreferences, OnUpdateWebPreferences)
    IPC_MESSAGE_HANDLER(ViewMsg_SetAltErrorPageURL, OnSetAltErrorPageURL)
    IPC_MESSAGE_HANDLER(ViewMsg_EnumerateDirectoryResponse,
                        OnEnumerateDirectoryResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_RunFileChooserResponse, OnFileChooserResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_ShouldClose, OnShouldClose)
    IPC_MESSAGE_HANDLER(ViewMsg_SwapOut, OnSwapOut)
    IPC_MESSAGE_HANDLER(ViewMsg_ClosePage, OnClosePage)
    IPC_MESSAGE_HANDLER(ViewMsg_ThemeChanged, OnThemeChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_DisassociateFromPopupCount,
                        OnDisassociateFromPopupCount)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveOrResizeStarted, OnMoveOrResizeStarted)
    IPC_MESSAGE_HANDLER(ViewMsg_ClearFocusedNode, OnClearFocusedNode)
    IPC_MESSAGE_HANDLER(ViewMsg_SetBackground, OnSetBackground)
    IPC_MESSAGE_HANDLER(ViewMsg_EnablePreferredSizeChangedMode,
                        OnEnablePreferredSizeChangedMode)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableScrollbarsForSmallWindows,
                        OnDisableScrollbarsForSmallWindows)
    IPC_MESSAGE_HANDLER(ViewMsg_SetRendererPrefs, OnSetRendererPrefs)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaPlayerActionAt, OnMediaPlayerActionAt)
    IPC_MESSAGE_HANDLER(ViewMsg_SetActive, OnSetActive)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_SetWindowVisibility, OnSetWindowVisibility)
    IPC_MESSAGE_HANDLER(ViewMsg_WindowFrameChanged, OnWindowFrameChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_PluginImeCompositionCompleted,
                        OnPluginImeCompositionCompleted)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_SetEditCommandsForNextKeyEvent,
                        OnSetEditCommandsForNextKeyEvent)
    IPC_MESSAGE_HANDLER(ViewMsg_CustomContextMenuAction,
                        OnCustomContextMenuAction)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableAccessibility, OnEnableAccessibility)
    IPC_MESSAGE_HANDLER(ViewMsg_SetAccessibilityFocus, OnSetAccessibilityFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_AccessibilityDoDefaultAction,
                        OnAccessibilityDoDefaultAction)
    IPC_MESSAGE_HANDLER(ViewMsg_AccessibilityNotifications_ACK,
                        OnAccessibilityNotificationsAck)
    IPC_MESSAGE_HANDLER(ViewMsg_AsyncOpenFile_ACK, OnAsyncFileOpened)
    IPC_MESSAGE_HANDLER(ViewMsg_PpapiBrokerChannelCreated,
                        OnPpapiBrokerChannelCreated)
    IPC_MESSAGE_HANDLER(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                        OnGetAllSavableResourceLinksForCurrentPage)
    IPC_MESSAGE_HANDLER(
        ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
        OnGetSerializedHtmlDataForCurrentPageWithLocalLinks)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectPopupMenuItem, OnSelectPopupMenuItem)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_ContextMenuClosed, OnContextMenuClosed)
    // TODO(viettrungluu): Move to a separate message filter.
#if defined(ENABLE_FLAPPER_HACKS)
    IPC_MESSAGE_HANDLER(PepperMsg_ConnectTcpACK, OnConnectTcpACK)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInLiveResize, OnSetInLiveResize)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateRemoteAccessClientFirewallTraversal,
                        OnUpdateRemoteAccessClientFirewallTraversal)
    IPC_MESSAGE_HANDLER(ViewMsg_SetHistoryLengthAndPrune,
                        OnSetHistoryLengthAndPrune)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableViewSourceMode, OnEnableViewSourceMode)

    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(handled = RenderWidget::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderView::OnNavigate(const ViewMsg_Navigate_Params& params) {
  if (!webview())
    return;

  bool is_reload =
      params.navigation_type == ViewMsg_Navigate_Type::RELOAD ||
      params.navigation_type == ViewMsg_Navigate_Type::RELOAD_IGNORING_CACHE;

  // If this is a stale back/forward (due to a recent navigation the browser
  // didn't know about), ignore it.
  if (IsBackForwardToStaleEntry(params, is_reload))
    return;

  // Swap this renderer back in if necessary.
  if (is_swapped_out_)
    SetSwappedOut(false);

  history_list_offset_ = params.current_history_list_offset;
  history_list_length_ = params.current_history_list_length;
  if (history_list_length_ >= 0)
    history_page_ids_.resize(history_list_length_, -1);
  if (params.pending_history_list_offset >= 0 &&
      params.pending_history_list_offset < history_list_length_)
    history_page_ids_[params.pending_history_list_offset] = params.page_id;

  content::GetContentClient()->SetActiveURL(params.url);

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
    if (params.navigation_type == ViewMsg_Navigate_Type::RESTORE) {
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

  if (navigation_state) {
    // New loads need to reset the error page fetcher. Otherwise if there is an
    // outstanding error page fetcher it may complete and clobber the current
    // page load.
    navigation_state->set_alt_error_page_fetcher(NULL);
  }

  // If we are reloading, then WebKit will use the history state of the current
  // page, so we should just ignore any given history state.  Otherwise, if we
  // have history state, then we need to navigate to it, which corresponds to a
  // back/forward navigation event.
  if (is_reload) {
    if (navigation_state)
      navigation_state->set_load_type(NavigationState::RELOAD);
    bool ignore_cache = (params.navigation_type ==
                             ViewMsg_Navigate_Type::RELOAD_IGNORING_CACHE);
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
      if (!WebSecurityPolicy::shouldHideReferrer(
              params.url,
              WebString::fromUTF8(params.referrer.spec()))) {
        request.setHTTPHeaderField(WebString::fromUTF8("Referer"),
                                   WebString::fromUTF8(params.referrer.spec()));
      }
    }

    if (!params.extra_headers.empty()) {
      for (net::HttpUtil::HeadersIterator i(params.extra_headers.begin(),
                                            params.extra_headers.end(), "\n");
           i.GetNext(); ) {
        request.addHTTPHeaderField(WebString::fromUTF8(i.name()),
                                   WebString::fromUTF8(i.values()));
      }
    }

    if (navigation_state)
      navigation_state->set_load_type(NavigationState::NORMAL_LOAD);
    main_frame->loadRequest(request);
  }

  // In case LoadRequest failed before DidCreateDataSource was called.
  pending_navigation_state_.reset();
}

bool RenderView::IsBackForwardToStaleEntry(
    const ViewMsg_Navigate_Params& params,
    bool is_reload) {
  // Make sure this isn't a back/forward to an entry we have already cropped
  // or replaced from our history, before the browser knew about it.  If so,
  // a new navigation has committed in the mean time, and we can ignore this.
  bool is_back_forward = !is_reload && !params.state.empty();

  // Note: if the history_list_length_ is 0 for a back/forward, we must be
  // restoring from a previous session.  We'll update our state in OnNavigate.
  if (!is_back_forward || history_list_length_ <= 0)
    return false;

  DCHECK_EQ(static_cast<int>(history_page_ids_.size()), history_list_length_);

  // Check for whether the forward history has been cropped due to a recent
  // navigation the browser didn't know about.
  if (params.pending_history_list_offset >= history_list_length_)
    return true;

  // Check for whether this entry has been replaced with a new one.
  int expected_page_id =
      history_page_ids_[params.pending_history_list_offset];
  if (expected_page_id > 0 && params.page_id != expected_page_id) {
    if (params.page_id < expected_page_id)
      return true;

    // Otherwise we've removed an earlier entry and should have shifted all
    // entries left.  For now, it's ok to lazily update the list.
    // TODO(creis): Notify all live renderers when we remove entries from
    // the front of the list, so that we don't hit this case.
    history_page_ids_[params.pending_history_list_offset] = params.page_id;
  }

  return false;
}

// Stop loading the current page
void RenderView::OnStop() {
  if (webview()) {
    WebFrame* main_frame = webview()->mainFrame();
    // Stop the alt error page fetcher. If we let it continue it may complete
    // and cause RenderViewHostManager to swap to this RenderView, even though
    // it may no longer be active.
    StopAltErrorPageFetcher(main_frame->provisionalDataSource());
    StopAltErrorPageFetcher(main_frame->dataSource());
    main_frame->stopLoading();
  }
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
}

void RenderView::OnRedo() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Redo"));
}

void RenderView::OnCut() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Cut"));
}

void RenderView::OnCopy() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Copy"),
                                            context_menu_node_);
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
        new ClipboardHostMsg_FindPboardWriteStringAsync(selection));
  }
}
#endif

void RenderView::OnPaste() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Paste"));
}

void RenderView::OnReplace(const string16& text) {
  if (!webview())
    return;

  WebFrame* frame = webview()->focusedFrame();
  if (!frame->hasSelection())
    frame->selectWordAroundCaret();
  frame->replaceSelection(text);
}

void RenderView::OnDelete() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Delete"));
}

void RenderView::OnSelectAll() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8("SelectAll"));
}

void RenderView::OnSelectRange(const gfx::Point& start, const gfx::Point& end) {
  if (!webview())
    return;

  handling_select_range_ = true;
  webview()->focusedFrame()->selectRange(start, end);
  handling_select_range_ = false;
}

void RenderView::OnSetHistoryLengthAndPrune(int history_length,
                                            int32 minimum_page_id) {
  DCHECK(history_length >= 0);
  DCHECK(history_list_offset_ == history_list_length_ - 1);
  DCHECK(minimum_page_id >= -1);

  // Generate the new list.
  std::vector<int32> new_history_page_ids(history_length, -1);
  for (size_t i = 0; i < history_page_ids_.size(); ++i) {
    if (minimum_page_id >= 0 && history_page_ids_[i] < minimum_page_id)
      continue;
    new_history_page_ids.push_back(history_page_ids_[i]);
  }
  new_history_page_ids.swap(history_page_ids_);

  // Update indexes.
  history_list_length_ = history_page_ids_.size();
  history_list_offset_ = history_list_length_ - 1;
}


void RenderView::OnSetInitialFocus(bool reverse) {
  if (!webview())
    return;
  webview()->setInitialFocus(reverse);
}

#if defined(OS_MACOSX)
void RenderView::OnSetInLiveResize(bool in_live_resize) {
  if (!webview())
    return;
  if (in_live_resize)
    webview()->willStartLiveResize();
  else
    webview()->willEndLiveResize();
}
#endif

void RenderView::OnScrollFocusedEditableNodeIntoView() {
  WebKit::WebNode node = GetFocusedNode();
  if (!node.isNull()) {
    if (IsEditableNode(node))
      // TODO(varunjain): Change webkit API to scroll a particular node into
      // view and use that API here instead.
      webview()->scrollFocusedNodeIntoView();
  }
}

///////////////////////////////////////////////////////////////////////////////

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
  params.frame_id = frame->identifier();
  params.socket_address.set_host(response.remoteIPAddress().utf8());
  params.socket_address.set_port(response.remotePort());
  params.was_fetched_via_proxy = response.wasFetchedViaProxy();
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
      !response.isMultipartPayload() && (response.httpStatusCode() != 404);

  params.searchable_form_url = navigation_state->searchable_form_url();
  params.searchable_form_encoding =
      navigation_state->searchable_form_encoding();

  const PasswordForm* password_form_data =
      navigation_state->password_form_data();
  if (password_form_data)
    params.password_form = *password_form_data;

  params.gesture = navigation_gesture_;
  navigation_gesture_ = NavigationGestureUnknown;

  // Make navigation state a part of the FrameNavigate message so that commited
  // entry had it at all times.
  const WebHistoryItem& item = frame->currentHistoryItem();
  if (!item.isNull()) {
    params.content_state = webkit_glue::HistoryItemToString(item);
  } else {
    params.content_state =
        webkit_glue::CreateHistoryStateForURL(GURL(request.url()));
  }

  if (!frame->parent()) {
    // Top-level navigation.

    // Set zoom level, but don't do it for full-page plugin since they don't use
    // the same zoom settings.
    HostZoomLevels::iterator host_zoom =
        host_zoom_levels_.find(GURL(request.url()));
    if (webview()->mainFrame()->document().isPluginDocument()) {
      // Reset the zoom levels for plugins.
      webview()->setZoomLevel(false, 0);
    } else {
      if (host_zoom != host_zoom_levels_.end())
        webview()->setZoomLevel(false, host_zoom->second);
    }

    if (host_zoom != host_zoom_levels_.end()) {
      // This zoom level was merely recorded transiently for this load.  We can
      // erase it now.  If at some point we reload this page, the browser will
      // send us a new, up-to-date zoom level.
      host_zoom_levels_.erase(host_zoom);
    }

    // Reset the zoom limits in case a plugin had changed them previously. This
    // will also call us back which will cause us to send a message to
    // update TabContents.
    webview()->zoomLimitsChanged(
        WebView::zoomFactorToZoomLevel(WebView::minTextSizeMultiplier),
        WebView::zoomFactorToZoomLevel(WebView::maxTextSizeMultiplier));

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
    accessibility_.reset();
    pending_accessibility_notifications_.clear();
  }
}

// Tell the embedding application that the title of the active page has changed
void RenderView::UpdateTitle(WebFrame* frame,
                             const string16& title,
                             WebTextDirection title_direction) {
  // Ignore all but top level navigations.
  if (frame->parent())
    return;

  string16 shortened_title = title.substr(0, content::kMaxTitleChars);
  Send(new ViewHostMsg_UpdateTitle(routing_id_, page_id_, shortened_title,
                                   title_direction));
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

// Sends the last committed session history state to the browser so it will be
// saved before we navigate to a new page. This must be called *before* the
// page ID has been updated so we know what it was.
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

  // Do not show alternate error page when DevTools is attached.
  if (devtools_agent_->IsAttached())
    return;

  std::string alt_html = !html.empty() ? html :
      content::GetContentClient()->renderer()->GetNavigationErrorHtml(
          failed_request, error);
  frame->loadHTMLString(alt_html,
                        GURL(chrome::kUnreachableWebDataURL),
                        error.unreachableURL,
                        replace);
}

bool RenderView::RunJavaScriptMessage(int type,
                                      const string16& message,
                                      const string16& default_value,
                                      const GURL& frame_url,
                                      string16* result) {
  bool success = false;
  string16 result_temp;
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

// WebKit::WebViewClient ------------------------------------------------------

WebView* RenderView::createView(
    WebFrame* creator,
    const WebURLRequest& request,
    const WebWindowFeatures& features,
    const WebString& frame_name) {
  // Check to make sure we aren't overloading on popups.
  if (shared_popup_counter_->data > kMaximumNumberOfUnacknowledgedPopups)
    return NULL;

  ViewHostMsg_CreateWindow_Params params;
  params.opener_id = routing_id_;
  params.user_gesture = creator->isProcessingUserGesture();
  params.window_container_type = WindowFeaturesToContainerType(features);
  params.session_storage_namespace_id = session_storage_namespace_id_;
  params.frame_name = frame_name;
  params.opener_frame_id = creator->identifier();
  params.opener_url = creator->document().url();
  params.opener_security_origin =
      creator->document().securityOrigin().toString().utf8();
  if (!request.isNull())
    params.target_url = request.url();

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
  GURL creator_url(creator->document().securityOrigin().toString().utf8());
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
  // TODO(jcivelli): Remove this deprecated method when its been removed from
  //                 the WebViewClient interface. It's been replaced by
  //                 createExternalPopupMenu.
  NOTREACHED();
  return NULL;
}

WebExternalPopupMenu* RenderView::createExternalPopupMenu(
    const WebPopupMenuInfo& popup_menu_info,
    WebExternalPopupMenuClient* popup_menu_client) {
  DCHECK(!external_popup_menu_.get());
  external_popup_menu_.reset(
      new ExternalPopupMenu(this, popup_menu_info, popup_menu_client));
  return external_popup_menu_.get();
}

RenderWidgetFullscreenPepper* RenderView::CreatePepperFullscreenContainer(
    webkit::ppapi::PluginInstance* plugin) {
  GURL active_url;
  if (webview() && webview()->mainFrame())
    active_url = GURL(webview()->mainFrame()->document().url());
  RenderWidgetFullscreenPepper* widget = RenderWidgetFullscreenPepper::Create(
      routing_id_, render_thread_, plugin, active_url);
  widget->show(WebKit::WebNavigationPolicyIgnore);
  return widget;
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
  logging::LogSeverity log_severity = logging::LOG_VERBOSE;
  switch (message.level) {
    case WebConsoleMessage::LevelTip:
      log_severity = logging::LOG_VERBOSE;
      break;
    case WebConsoleMessage::LevelLog:
      log_severity = logging::LOG_INFO;
      break;
    case WebConsoleMessage::LevelWarning:
      log_severity = logging::LOG_WARNING;
      break;
    case WebConsoleMessage::LevelError:
      log_severity = logging::LOG_ERROR;
      break;
    default:
      NOTREACHED();
  }

  Send(new ViewHostMsg_AddMessageToConsole(routing_id_,
                                           static_cast<int32>(log_severity),
                                           message.text,
                                           static_cast<int32>(source_line),
                                           source_name));
}

void RenderView::printPage(WebFrame* frame) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, PrintPage(frame));
}

WebKit::WebNotificationPresenter* RenderView::notificationPresenter() {
  return notification_provider_;
}

bool RenderView::enumerateChosenDirectory(
    const WebString& path,
    WebFileChooserCompletion* chooser_completion) {
  int id = enumeration_completion_id_++;
  enumeration_completions_[id] = chooser_completion;
  return Send(new ViewHostMsg_EnumerateDirectory(
      routing_id_,
      id,
      webkit_glue::WebStringToFilePath(path)));
}

void RenderView::didStartLoading() {
  if (is_loading_) {
    DLOG(WARNING) << "didStartLoading called while loading";
    return;
  }

  is_loading_ = true;

  Send(new ViewHostMsg_DidStartLoading(routing_id_));

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidStartLoading());
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
  Send(new ViewHostMsg_DidStopLoading(routing_id_));

  if (load_progress_tracker_ != NULL)
    load_progress_tracker_->DidStopLoading();

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidStopLoading());
}

void RenderView::didChangeLoadProgress(WebFrame* frame, double load_progress) {
  if (load_progress_tracker_ != NULL)
    load_progress_tracker_->DidChangeLoadProgress(frame, load_progress);
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
#if defined(OS_POSIX)
  if (!handling_input_event_ && !handling_select_range_)
      return;
  handling_select_range_ = false;

  SyncSelectionIfRequired();
#endif  // defined(OS_POSIX)
}

void RenderView::didExecuteCommand(const WebString& command_name) {
  const std::string& name = UTF16ToUTF8(command_name);
  if (StartsWithASCII(name, "Move", true) ||
      StartsWithASCII(name, "Insert", true) ||
      StartsWithASCII(name, "Delete", true))
    return;
  RenderThread::current()->Send(
      new ViewHostMsg_UserMetricsRecordAction(name));
}

void RenderView::SendPendingAccessibilityNotifications() {
  if (!accessibility_.get())
    return;

  if (pending_accessibility_notifications_.empty())
    return;

  // Send all pending accessibility notifications.
  std::vector<ViewHostMsg_AccessibilityNotification_Params> notifications;
  for (size_t i = 0; i < pending_accessibility_notifications_.size(); i++) {
    RendererAccessibilityNotification& notification =
        pending_accessibility_notifications_[i];
    WebAccessibilityObject obj = accessibility_->getObjectById(notification.id);
    if (!obj.isValid())
      continue;

    ViewHostMsg_AccessibilityNotification_Params param;
    WebAccessibilityNotificationToViewHostMsg(
        pending_accessibility_notifications_[i].type, &param.notification_type);
    param.acc_obj = WebAccessibility(
        obj, accessibility_.get(), notification.ShouldIncludeChildren());
    notifications.push_back(param);

#ifndef NDEBUG
    if (accessibility_logging_) {
      LOG(INFO) << "Accessibility update: \n"
                << param.acc_obj.DebugString(true);
    }
#endif
  }
  pending_accessibility_notifications_.clear();
  Send(new ViewHostMsg_AccessibilityNotifications(routing_id_, notifications));
  accessibility_ack_pending_ = true;
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

bool RenderView::runFileChooser(
    const WebKit::WebFileChooserParams& params,
    WebFileChooserCompletion* chooser_completion) {
  // Do not open the file dialog in a hidden RenderView.
  if (is_hidden())
    return false;
  ViewHostMsg_RunFileChooser_Params ipc_params;
  if (params.directory)
    ipc_params.mode = ViewHostMsg_RunFileChooser_Mode::OpenFolder;
  else if (params.multiSelect)
    ipc_params.mode = ViewHostMsg_RunFileChooser_Mode::OpenMultiple;
  else
    ipc_params.mode = ViewHostMsg_RunFileChooser_Mode::Open;
  ipc_params.title = params.title;
  ipc_params.default_file_name =
      webkit_glue::WebStringToFilePath(params.initialValue);
  ipc_params.accept_types = params.acceptTypes;

  return ScheduleFileChooser(ipc_params, chooser_completion);
}

void RenderView::runModalAlertDialog(
    WebFrame* frame, const WebString& message) {
  RunJavaScriptMessage(ui::MessageBoxFlags::kIsJavascriptAlert,
                       message,
                       string16(),
                       frame->document().url(),
                       NULL);
}

bool RenderView::runModalConfirmDialog(
    WebFrame* frame, const WebString& message) {
  return RunJavaScriptMessage(ui::MessageBoxFlags::kIsJavascriptConfirm,
                              message,
                              string16(),
                              frame->document().url(),
                              NULL);
}

bool RenderView::runModalPromptDialog(
    WebFrame* frame, const WebString& message, const WebString& default_value,
    WebString* actual_value) {
  string16 result;
  bool ok = RunJavaScriptMessage(ui::MessageBoxFlags::kIsJavascriptPrompt,
                                 message,
                                 default_value,
                                 frame->document().url(),
                                 &result);
  if (ok)
    actual_value->assign(result);
  return ok;
}

bool RenderView::runModalBeforeUnloadDialog(
    WebFrame* frame, const WebString& message) {
  // If we are swapping out, we have already run the beforeunload handler.
  // TODO(creis): Fix OnSwapOut to clear the frame without running beforeunload
  // at all, to avoid running it twice.
  if (is_swapped_out_)
    return true;

  bool success = false;
  // This is an ignored return value, but is included so we can accept the same
  // response as RunJavaScriptMessage.
  string16 ignored_result;
  SendAndRunNestedMessageLoop(new ViewHostMsg_RunBeforeUnloadConfirm(
      routing_id_, frame->document().url(), message,
      &success, &ignored_result));
  return success;
}

void RenderView::showContextMenu(
    WebFrame* frame, const WebContextMenuData& data) {
  ContextMenuParams params = ContextMenuParams(data);

  // frame is NULL if invoked by BlockedPlugin.
  if (frame)
    params.frame_id = frame->identifier();

  // Serializing a GURL longer than content::kMaxURLChars will fail, so don't do
  // it.  We replace it with an empty GURL so the appropriate items are disabled
  // in the context menu.
  // TODO(jcivelli): http://crbug.com/45160 This prevents us from saving large
  //                 data encoded images.  We should have a way to save them.
  if (params.src_url.spec().size() > content::kMaxURLChars)
    params.src_url = GURL();
  context_menu_node_ = data.node;
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

void RenderView::enterFullscreen() {
  Send(new ViewHostMsg_ToggleFullscreen(routing_id_, true));
}

void RenderView::exitFullscreen() {
  Send(new ViewHostMsg_ToggleFullscreen(routing_id_, false));
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

  nav_state_sync_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(delay), this,
                              &RenderView::SyncNavigationState);
}

void RenderView::setMouseOverURL(const WebURL& url) {
  mouse_over_url_ = GURL(url);
  UpdateTargetURL(mouse_over_url_, focus_url_);
}

void RenderView::setKeyboardFocusURL(const WebURL& url) {
  focus_url_ = GURL(url);
  UpdateTargetURL(focus_url_, mouse_over_url_);
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

  Send(new DragHostMsg_StartDragging(routing_id_,
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
  Send(new ViewHostMsg_FocusedNodeChanged(routing_id_, IsEditableNode(node)));

  if (WebAccessibilityCache::accessibilityEnabled() && node.isNull()) {
    // TODO(ctguil): Make WebKit send this notification.
    // When focus is cleared notify accessibility that the document is focused.
    postAccessibilityNotification(
        webview()->accessibilityObject(),
        WebKit::WebAccessibilityNotificationFocusedUIElementChanged);
  }

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, FocusedNodeChanged(node));
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

  if (content::GetContentClient()->renderer()->AllowPopup(creator_url_))
    opened_by_user_gesture_ = true;

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
  return content::GetContentClient()->renderer()->CreatePlugin(
      this, frame, params);
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
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, WillCreateMediaPlayer(frame, client));

  scoped_ptr<media::MessageLoopFactory> message_loop_factory(
      new media::MessageLoopFactoryImpl());
  scoped_ptr<media::FilterCollection> collection(
      new media::FilterCollection());

  // Add in any custom filter factories first.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kDisableAudio)) {
    // Add the chrome specific audio renderer.
    collection->AddAudioRenderer(new AudioRendererImpl());
  }

  scoped_refptr<webkit_glue::WebVideoRenderer> video_renderer;
  bool pts_logging = cmd_line->HasSwitch(switches::kEnableVideoLogging);
  scoped_refptr<webkit_glue::VideoRendererImpl> renderer(
      new webkit_glue::VideoRendererImpl(pts_logging));
  collection->AddVideoRenderer(renderer);
  video_renderer = renderer;

  scoped_ptr<webkit_glue::WebMediaPlayerImpl> result(
      new webkit_glue::WebMediaPlayerImpl(client,
                                          collection.release(),
                                          message_loop_factory.release(),
                                          media_stream_impl_.get(),
                                          new RenderMediaLog()));
  if (!result->Initialize(frame,
                          cmd_line->HasSwitch(switches::kSimpleDataSource),
                          video_renderer)) {
    return NULL;
  }
  return result.release();
}

WebApplicationCacheHost* RenderView::createApplicationCacheHost(
    WebFrame* frame, WebApplicationCacheHostClient* client) {
  return new RendererWebApplicationCacheHostImpl(
      FromWebView(frame->view()), client,
      RenderThread::current()->appcache_dispatcher()->backend_proxy());
}

WebCookieJar* RenderView::cookieJar(WebFrame* frame) {
  return &cookie_jar_;
}

void RenderView::frameDetached(WebFrame* frame) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, FrameDetached(frame));
}

void RenderView::willClose(WebFrame* frame) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, FrameWillClose(frame));
}

void RenderView::loadURLExternally(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationPolicy policy) {
  loadURLExternally(frame, request, policy, WebString());
}

void RenderView::loadURLExternally(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationPolicy policy,
    const WebString& suggested_name) {
  GURL referrer(request.httpHeaderField(WebString::fromUTF8("Referer")));
  if (policy == WebKit::WebNavigationPolicyDownload) {
    Send(new ViewHostMsg_DownloadUrl(routing_id_, request.url(), referrer,
                                     suggested_name));
  } else {
    OpenURL(request.url(), referrer, policy);
  }
}

WebNavigationPolicy RenderView::decidePolicyForNavigation(
    WebFrame* frame, const WebURLRequest& request, WebNavigationType type,
    const WebNode&, WebNavigationPolicy default_policy, bool is_redirect) {
  // TODO(creis): Remove this when we fix OnSwapOut to not need a navigation.
  if (is_swapped_out_) {
    DCHECK(request.url() == GURL("about:swappedout"));
    return default_policy;
  }

  // Webkit is asking whether to navigate to a new URL.
  // This is fine normally, except if we're showing UI from one security
  // context and they're trying to navigate to a different context.
  const GURL& url = request.url();

  // A content initiated navigation may have originated from a link-click,
  // script, drag-n-drop operation, etc.
  bool is_content_initiated =
      NavigationState::FromDataSource(frame->provisionalDataSource())->
          is_content_initiated();

  // If the browser is interested, then give it a chance to look at top level
  // navigations.
  if (is_content_initiated &&
      renderer_preferences_.browser_handles_top_level_requests &&
      IsNonLocalTopLevelNavigation(url, frame, type)) {
    GURL referrer(request.httpHeaderField(WebString::fromUTF8("Referer")));
    // Reset these counters as the RenderView could be reused for the next
    // navigation.
    page_id_ = -1;
    last_page_id_sent_to_browser_ = -1;
    OpenURL(url, referrer, default_policy);
    return WebKit::WebNavigationPolicyIgnore;  // Suppress the load here.
  }

  // Detect when we're crossing a permission-based boundary (e.g. into or out of
  // an extension or app origin, leaving a WebUI page, etc). We only care about
  // top-level navigations within the current tab (as opposed to, for example,
  // opening a new window). But we sometimes navigate to about:blank to clear a
  // tab, and we want to still allow that.
  //
  // Note: we do this only for GET requests because our mechanism for switching
  // processes only issues GET requests. In particular, POST requests don't
  // work, because this mechanism does not preserve form POST data. If it
  // becomes necessary to support process switching for POST requests, we will
  // need to send the request's httpBody data up to the browser process, and
  // issue a special POST navigation in WebKit (via
  // FrameLoader::loadFrameRequest). See ResourceDispatcher and WebURLLoaderImpl
  // for examples of how to send the httpBody data.
  // Note2: We normally don't do this for browser-initiated navigations, since
  // it's pointless to tell the browser about navigations it gave us. But
  // we do potentially ask the browser to handle a redirect that was originally
  // initiated by the browser. See http://crbug.com/70943
  //
  // TODO(creis): Move this redirect check to the browser process to avoid
  // ping-ponging.  See http://crbug.com/72380.
  if (!frame->parent() && (is_content_initiated || is_redirect) &&
      default_policy == WebKit::WebNavigationPolicyCurrentTab &&
      request.httpMethod() == "GET" && !url.SchemeIs(chrome::kAboutScheme)) {
    bool send_referrer = false;
    bool should_fork =
        BindingsPolicy::is_web_ui_enabled(enabled_bindings_) ||
        frame->isViewSourceModeEnabled() ||
        url.SchemeIs(chrome::kViewSourceScheme);

    if (!should_fork) {
      // Give the embedder a chance.
      bool is_initial_navigation = page_id_ == -1;
      should_fork = content::GetContentClient()->renderer()->ShouldFork(
          frame, url, is_content_initiated, is_initial_navigation,
          &send_referrer);
    }

    if (should_fork) {
      GURL referrer(request.httpHeaderField(WebString::fromUTF8("Referer")));
      OpenURL(url, send_referrer ? referrer : GURL(), default_policy);
      return WebKit::WebNavigationPolicyIgnore;  // Suppress the load here.
    }
  }

  // Use the frame's original request's URL rather than the document's URL for
  // this check.  For a popup, the document's URL may become the opener window's
  // URL if the opener has called document.write.  See http://crbug.com/93517.
  GURL old_url(frame->dataSource()->request().url());

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
      frame->document().url().isEmpty() &&
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

  // In order to save the password that the user actually typed and not one
  // that may have gotten transformed by the site prior to submit, recover it
  // from the form contents already stored by |willSendSubmitEvent| into the
  // dataSource's NavigationState (as opposed to the provisionalDataSource's,
  // which is what we're storing into now.)
  if (password_form_data) {
    NavigationState* old_navigation_state =
        NavigationState::FromDataSource(frame->dataSource());
    if (old_navigation_state) {
      PasswordForm* old_form_data = old_navigation_state->password_form_data();
      if (old_form_data && old_form_data->action == password_form_data->action)
        password_form_data->password_value = old_form_data->password_value;
    }
  }

  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, WillSubmitForm(frame, form));
}

void RenderView::willPerformClientRedirect(
    WebFrame* frame, const WebURL& from, const WebURL& to, double interval,
    double fire_time) {
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_,
      WillPerformClientRedirect(frame, from, to, interval, fire_time));
}

void RenderView::didCancelClientRedirect(WebFrame* frame) {
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidCancelClientRedirect(frame));
}

void RenderView::didCompleteClientRedirect(
    WebFrame* frame, const WebURL& from) {
  if (!frame->parent())
    completed_client_redirect_src_ = from;
  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidCompleteClientRedirect(frame, from));
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

  ds->setExtraData(state);

  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidCreateDataSource(frame, ds));
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
        NavigationGestureUser : NavigationGestureAuto;

    // Make sure redirect tracking state is clear for the new load.
    completed_client_redirect_src_ = GURL();
  } else if (frame->parent()->isLoading()) {
    // Take note of AUTO_SUBFRAME loads here, so that we can know how to
    // load an error page.  See didFailProvisionalLoad.
    navigation_state->set_transition_type(PageTransition::AUTO_SUBFRAME);
  }

  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidStartProvisionalLoad(frame));

  bool has_opener_set = opener_id_ != MSG_ROUTING_NONE;
  Send(new ViewHostMsg_DidStartProvisionalLoadForFrame(
       routing_id_, frame->identifier(), is_top_most, has_opener_set,
       ds->request().url()));
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
    bool has_opener_set = opener_id_ != MSG_ROUTING_NONE;
    Send(new ViewHostMsg_DidRedirectProvisionalLoad(routing_id_, page_id_,
        has_opener_set, redirects[redirects.size() - 2], redirects.back()));
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

  FOR_EACH_OBSERVER(
      RenderViewObserver, observers_, DidFailProvisionalLoad(frame, error));

  bool show_repost_interstitial =
      (error.reason == net::ERR_CACHE_MISS &&
       EqualsASCII(failed_request.httpMethod(), "POST"));
  Send(new ViewHostMsg_DidFailProvisionalLoadWithError(
      routing_id_, frame->identifier(), !frame->parent(), error.reason,
      error.unreachableURL, show_repost_interstitial));

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

  // Do not show alternate error page when DevTools is attached.
  if (devtools_agent_->IsAttached())
    return;

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
  navigation_state->set_use_error_page(false);
}

void RenderView::didCommitProvisionalLoad(WebFrame* frame,
                                          bool is_new_navigation) {
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->dataSource());

  navigation_state->set_commit_load_time(Time::Now());
  if (is_new_navigation) {
    // When we perform a new navigation, we need to update the last committed
    // session history entry with state for the page we are leaving.
    UpdateSessionHistory(frame);

    // We bump our Page ID to correspond with the new session history entry.
    page_id_ = next_page_id_++;

    // Advance our offset in session history, applying the length limit.  There
    // is now no forward history.
    history_list_offset_++;
    if (history_list_offset_ >= content::kMaxSessionHistoryEntries)
      history_list_offset_ = content::kMaxSessionHistoryEntries - 1;
    history_list_length_ = history_list_offset_ + 1;
    history_page_ids_.resize(history_list_length_, -1);
    history_page_ids_[history_list_offset_] = page_id_;
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

      // If the history list is valid, our list of page IDs should be correct.
      DCHECK(history_list_length_ <= 0 ||
             history_list_offset_ < 0 ||
             history_list_offset_ >= history_list_length_ ||
             history_page_ids_[history_list_offset_] == page_id_);
    }
  }

  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    DidCommitProvisionalLoad(frame, is_new_navigation));

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
  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    DidClearWindowObject(frame));

  GURL frame_url = frame->document().url();
  if (BindingsPolicy::is_web_ui_enabled(enabled_bindings_) &&
      (frame_url.SchemeIs(chrome::kChromeUIScheme) ||
      frame_url.SchemeIs(chrome::kDataScheme))) {
    GetWebUIBindings()->set_message_sender(this);
    GetWebUIBindings()->set_routing_id(routing_id_);
    GetWebUIBindings()->BindToJavascript(frame, "chrome");
  }
}

void RenderView::didCreateDocumentElement(WebFrame* frame) {
  // Notify the browser about non-blank documents loading in the top frame.
  GURL url = frame->document().url();
  if (url.is_valid() && url.spec() != chrome::kAboutBlankURL) {
    if (frame == webview()->mainFrame())
      Send(new ViewHostMsg_DocumentAvailableInMainFrame(routing_id_));
  }

  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    DidCreateDocumentElement(frame));
}

void RenderView::didReceiveTitle(WebFrame* frame, const WebString& title,
                                 WebTextDirection direction) {
  UpdateTitle(frame, title, direction);

  // Also check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());
}

void RenderView::didChangeIcon(WebFrame* frame, WebIconURL::Type type) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    DidChangeIcon(frame, type));
}

void RenderView::didFinishDocumentLoad(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);
  navigation_state->set_finish_document_load_time(Time::Now());

  Send(new ViewHostMsg_DocumentLoadedInFrame(routing_id_, frame->identifier()));

  FOR_EACH_OBSERVER(RenderViewObserver, observers_,
                    DidFinishDocumentLoad(frame));

  // Check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());
}

void RenderView::didHandleOnloadEvents(WebFrame* frame) {
  if (webview()->mainFrame() == frame) {
    Send(new ViewHostMsg_DocumentOnLoadCompletedInMainFrame(routing_id_,
                                                            page_id_));
  }
}

void RenderView::didFailLoad(WebFrame* frame, const WebURLError& error) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidFailLoad(frame, error));
}

void RenderView::didFinishLoad(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);
  navigation_state->set_finish_load_time(Time::Now());

  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidFinishLoad(frame));

  Send(new ViewHostMsg_DidFinishLoad(routing_id_, frame->identifier()));
}

void RenderView::didNavigateWithinPage(
    WebFrame* frame, bool is_new_navigation) {
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

  didCommitProvisionalLoad(frame, is_new_navigation);

  WebDataSource* datasource = frame->view()->mainFrame()->dataSource();
  UpdateTitle(frame, datasource->pageTitle(), datasource->pageTitleDirection());
}

void RenderView::didUpdateCurrentHistoryItem(WebFrame* frame) {
  StartNavStateSyncTimerIfNecessary();
}

void RenderView::assignIdentifierToRequest(
    WebFrame* frame, unsigned identifier, const WebURLRequest& request) {
  // Ignore
}

void RenderView::willSendRequest(WebFrame* frame,
                                 unsigned identifier,
                                 WebURLRequest& request,
                                 const WebURLResponse& redirect_response) {
  WebFrame* top_frame = frame->top();
  if (!top_frame)
    top_frame = frame;
  WebDataSource* provisional_data_source = top_frame->provisionalDataSource();
  WebDataSource* top_data_source = top_frame->dataSource();
  WebDataSource* data_source =
      provisional_data_source ? provisional_data_source : top_data_source;

  GURL request_url(request.url());
  GURL new_url;
  if (content::GetContentClient()->renderer()->WillSendRequest(
          frame, request_url, &new_url)) {
    request.setURL(WebURL(new_url));
  }

  PageTransition::Type transition_type = PageTransition::LINK;
  NavigationState* data_state = NavigationState::FromDataSource(data_source);
  if (data_state) {
    if (data_state->is_cache_policy_override_set())
      request.setCachePolicy(data_state->cache_policy_override());
    transition_type = data_state->transition_type();
  }

  request.setExtraData(new RequestExtraData((frame == top_frame),
      frame->identifier(), transition_type));

  NavigationState* top_data_state =
      NavigationState::FromDataSource(top_data_source);
  // TODO(gavinp): separate out prefetching and prerender field trials
  // if the rel=prerender rel type is sticking around.
  if (top_data_state &&
      (request.targetType() == WebURLRequest::TargetIsPrefetch ||
       request.targetType() == WebURLRequest::TargetIsPrerender))
    top_data_state->set_was_prefetcher(true);

  request.setRequestorID(routing_id_);
  request.setHasUserGesture(frame->isProcessingUserGesture());

  if (!renderer_preferences_.enable_referrers)
    request.clearHTTPHeaderField("Referer");
}

void RenderView::didReceiveResponse(
    WebFrame* frame, unsigned identifier, const WebURLResponse& response) {

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
  // Whether or not the http status code actually corresponds to an error is
  // only checked when the page is done loading, if |use_error_page| is
  // still true.
  navigation_state->set_use_error_page(true);
}

void RenderView::didFinishResourceLoad(
    WebFrame* frame, unsigned identifier) {
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->dataSource());
  if (!navigation_state->use_error_page())
    return;

  // Display error page, if appropriate.
  int http_status_code = navigation_state->http_status_code();
  if (http_status_code == 404) {
    // On 404s, try a remote search page as a fallback.
    const GURL& document_url = frame->document().url();

    const GURL& error_page_url =
        GetAlternateErrorPageURL(document_url, HTTP_404);
    if (error_page_url.is_valid()) {
      WebURLError original_error;
      original_error.domain = "http";
      original_error.reason = 404;
      original_error.unreachableURL = document_url;

      navigation_state->set_alt_error_page_fetcher(
          new AltErrorPageResourceFetcher(
              error_page_url, frame, original_error,
              NewCallback(this, &RenderView::AltErrorPageFinished)));
      return;
    }
  }

  content::GetContentClient()->renderer()->ShowErrorPage(
      this, frame, http_status_code);
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
      response.securityInfo(),
      request.httpMethod().utf8(),
      ResourceType::FromTargetType(request.targetType())));
}

void RenderView::didDisplayInsecureContent(WebFrame* frame) {
  Send(new ViewHostMsg_DidDisplayInsecureContent(routing_id_));
}

void RenderView::didRunInsecureContent(
    WebFrame* frame, const WebSecurityOrigin& origin, const WebURL& target) {
  Send(new ViewHostMsg_DidRunInsecureContent(
      routing_id_,
      origin.toString().utf8(),
      target));
}

void RenderView::didAdoptURLLoader(WebKit::WebURLLoader* loader) {
  webkit_glue::WebURLLoaderImpl* loader_impl =
      static_cast<webkit_glue::WebURLLoaderImpl*>(loader);
  loader_impl->UpdateRoutingId(routing_id_);
}

void RenderView::didExhaustMemoryAvailableForScript(WebFrame* frame) {
  Send(new ViewHostMsg_JSOutOfMemory(routing_id_));
}

void RenderView::didCreateScriptContext(WebFrame* frame) {
  content::GetContentClient()->renderer()->DidCreateScriptContext(frame);
}

void RenderView::didDestroyScriptContext(WebFrame* frame) {
  content::GetContentClient()->renderer()->DidDestroyScriptContext(frame);
}

void RenderView::didCreateIsolatedScriptContext(
    WebFrame* frame, int world_id, v8::Handle<v8::Context> context) {
  content::GetContentClient()->renderer()->DidCreateIsolatedScriptContext(
      frame, world_id, context);
}

void RenderView::didUpdateLayout(WebFrame* frame) {
  // We don't always want to set up a timer, only if we've been put in that
  // mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;

  if (check_preferred_size_timer_.IsRunning())
    return;
  check_preferred_size_timer_.Start(FROM_HERE,
                                    TimeDelta::FromMilliseconds(0), this,
                                    &RenderView::CheckPreferredSize);
}

void RenderView::CheckPreferredSize() {
  // We don't always want to send the change messages over IPC, only if we've
  // been put in that mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;

  gfx::Size size(webview()->mainFrame()->contentsPreferredWidth(),
                 webview()->mainFrame()->documentElementScrollHeight());

  // In the presence of zoom, these sizes are still reported as if unzoomed,
  // so we need to adjust.
  double zoom_factor = WebView::zoomLevelToZoomFactor(webview()->zoomLevel());
  size.set_width(static_cast<int>(size.width() * zoom_factor));
  size.set_height(static_cast<int>(size.height() * zoom_factor));

  if (size == preferred_size_)
    return;

  preferred_size_ = size;
  Send(new ViewHostMsg_DidContentsPreferredSizeChange(routing_id_,
                                                      preferred_size_));
}

void RenderView::didChangeContentsSize(WebFrame* frame, const WebSize& size) {
  if (webview()->mainFrame() != frame)
    return;
  WebView* frameView = frame->view();
  if (!frameView)
    return;

  bool has_horizontal_scrollbar = frame->hasHorizontalScrollbar();
  bool has_vertical_scrollbar = frame->hasVerticalScrollbar();

  if (has_horizontal_scrollbar != cached_has_main_frame_horizontal_scrollbar_ ||
      has_vertical_scrollbar != cached_has_main_frame_vertical_scrollbar_) {
    Send(new ViewHostMsg_DidChangeScrollbarsForMainFrame(
          routing_id_, has_horizontal_scrollbar, has_vertical_scrollbar));

    cached_has_main_frame_horizontal_scrollbar_ = has_horizontal_scrollbar;
    cached_has_main_frame_vertical_scrollbar_ = has_vertical_scrollbar;
  }
}

void RenderView::UpdateScrollState(WebFrame* frame) {
  WebSize offset = frame->scrollOffset();
  WebSize minimum_offset = frame->minimumScrollOffset();
  WebSize maximum_offset = frame->maximumScrollOffset();

  bool is_pinned_to_left = offset.width <= minimum_offset.width;
  bool is_pinned_to_right = offset.width >= maximum_offset.width;

  if (is_pinned_to_left != cached_is_main_frame_pinned_to_left_ ||
      is_pinned_to_right != cached_is_main_frame_pinned_to_right_) {
    Send(new ViewHostMsg_DidChangeScrollOffsetPinningForMainFrame(
          routing_id_, is_pinned_to_left, is_pinned_to_right));

    cached_is_main_frame_pinned_to_left_ = is_pinned_to_left;
    cached_is_main_frame_pinned_to_right_ = is_pinned_to_right;
  }
}

void RenderView::didChangeScrollOffset(WebFrame* frame) {
  StartNavStateSyncTimerIfNecessary();

  if (webview()->mainFrame() == frame)
    UpdateScrollState(frame);
}

void RenderView::numberOfWheelEventHandlersChanged(unsigned num_handlers) {
  Send(new ViewHostMsg_DidChangeNumWheelEvents(routing_id_, num_handlers));
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
    bool create,
    WebFileSystemCallbacks* callbacks) {
  DCHECK(callbacks);

  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (origin.isEmpty()) {
    // Uninitialized document?
    callbacks->didFail(WebKit::WebFileErrorAbort);
    return;
  }

  ChildThread::current()->file_system_dispatcher()->OpenFileSystem(
      GURL(origin.toString()), static_cast<fileapi::FileSystemType>(type),
      size, create, new WebFileSystemCallbackDispatcher(callbacks));
}

void RenderView::queryStorageUsageAndQuota(
    WebFrame* frame,
    WebStorageQuotaType type,
    WebStorageQuotaCallbacks* callbacks) {
  DCHECK(frame);
  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (origin.isEmpty()) {
    // Uninitialized document?
    callbacks->didFail(WebKit::WebStorageQuotaErrorAbort);
    return;
  }
  ChildThread::current()->quota_dispatcher()->QueryStorageUsageAndQuota(
      GURL(origin.toString()),
      static_cast<quota::StorageType>(type),
      QuotaDispatcher::CreateWebStorageQuotaCallbacksWrapper(callbacks));
}

void RenderView::requestStorageQuota(
    WebFrame* frame,
    WebStorageQuotaType type,
    unsigned long long requested_size,
    WebStorageQuotaCallbacks* callbacks) {
  DCHECK(frame);
  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (origin.isEmpty()) {
    // Uninitialized document?
    callbacks->didFail(WebKit::WebStorageQuotaErrorAbort);
    return;
  }
  ChildThread::current()->quota_dispatcher()->RequestStorageQuota(
      routing_id(), GURL(origin.toString()),
      static_cast<quota::StorageType>(type), requested_size,
      QuotaDispatcher::CreateWebStorageQuotaCallbacksWrapper(callbacks));
}

// WebKit::WebPageSerializerClient implementation ------------------------------

void RenderView::didSerializeDataForFrame(
    const WebURL& frame_url,
    const WebCString& data,
    WebPageSerializerClient::PageSerializationStatus status) {
  Send(new ViewHostMsg_SendSerializedHtmlData(
    routing_id(),
    frame_url,
    data.data(),
    static_cast<int32>(status)));
}

// webkit_glue::WebPluginPageDelegate -----------------------------------------

webkit::npapi::WebPluginDelegate* RenderView::CreatePluginDelegate(
    const FilePath& file_path,
    const std::string& mime_type) {
  if (!PluginChannelHost::IsListening())
    return NULL;

  bool in_process_plugin = RenderProcess::current()->UseInProcessPlugins();
  if (in_process_plugin) {
#if defined(OS_WIN)  // In-proc plugins aren't supported on Linux or Mac.
    return webkit::npapi::WebPluginDelegateImpl::Create(
        file_path, mime_type, gfx::NativeViewFromId(host_window_));
#else
    NOTIMPLEMENTED();
    return NULL;
#endif
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

void RenderView::DidMovePlugin(const webkit::npapi::WebPluginGeometry& move) {
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

void RenderView::SyncSelectionIfRequired() {
  WebFrame* frame = webview()->focusedFrame();
  const std::string& text = frame->selectionAsText().utf8();

  ui::Range range(ui::Range::InvalidRange());
  size_t location, length;
  if (webview()->caretOrSelectionRange(&location, &length)) {
    range.set_start(location);
    range.set_end(location + length);
  }

  WebPoint start, end;
  webview()->selectionRange(start, end);

  RenderViewSelection this_selection(text, range, start, end);

  // Sometimes we get repeated didChangeSelection calls from webkit when
  // the selection hasn't actually changed. We don't want to report these
  // because it will cause us to continually claim the X clipboard.
  if (this_selection.Equals(last_selection_))
    return;
  last_selection_ = this_selection;

  Send(new ViewHostMsg_SelectionChanged(routing_id_, text, range,
                                        start, end));
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

  if (devtools_agent_->IsAttached()) {
    // Do not show alternate error page when DevTools is attached.
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

WebUIBindings* RenderView::GetWebUIBindings() {
  if (!web_ui_bindings_.get()) {
    web_ui_bindings_.reset(new WebUIBindings());
  }
  return web_ui_bindings_.get();
}

WebKit::WebPlugin* RenderView::GetWebPluginFromPluginDocument() {
  return webview()->mainFrame()->document().to<WebPluginDocument>().plugin();
}

void RenderView::OnFind(int request_id, const string16& search_text,
                        const WebFindOptions& options) {
  WebFrame* main_frame = webview()->mainFrame();

  // Check if the plugin still exists in the document.
  if (main_frame->document().isPluginDocument() &&
      GetWebPluginFromPluginDocument()) {
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
  if (doc.isPluginDocument() && GetWebPluginFromPluginDocument()) {
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

WebPlugin* RenderView::CreatePepperPlugin(
    WebFrame* frame,
    const WebPluginParams& params,
    const FilePath& path,
    webkit::ppapi::PluginModule* pepper_module) {
  return new webkit::ppapi::WebPluginImpl(
      pepper_module, params, pepper_delegate_.AsWeakPtr());
}

WebPlugin* RenderView::CreateNPAPIPlugin(
    WebFrame* frame,
    const WebPluginParams& params,
    const FilePath& path,
    const std::string& mime_type) {
  return new webkit::npapi::WebPluginImpl(
      frame, params, path, mime_type, AsWeakPtr());
}

void RenderView::OnZoom(PageZoom::Function function) {
  if (!webview())  // Not sure if this can happen, but no harm in being safe.
    return;

  webview()->hidePopups();
#if !defined(TOUCH_UI)
  double old_zoom_level = webview()->zoomLevel();
  double zoom_level;
  if (function == PageZoom::RESET) {
    zoom_level = 0;
  } else if (static_cast<int>(old_zoom_level) == old_zoom_level) {
    // Previous zoom level is a whole number, so just increment/decrement.
    zoom_level = old_zoom_level + function;
  } else {
    // Either the user hit the zoom factor limit and thus the zoom level is now
    // not a whole number, or a plugin changed it to a custom value.  We want
    // to go to the next whole number so that the user can always get back to
    // 100% with the keyboard/menu.
    if ((old_zoom_level > 1 && function > 0) ||
        (old_zoom_level < 1 && function < 0)) {
      zoom_level = static_cast<int>(old_zoom_level + function);
    } else {
      // We're going towards 100%, so first go to the next whole number.
      zoom_level = static_cast<int>(old_zoom_level);
    }
  }
  webview()->setZoomLevel(false, zoom_level);
#else
  double old_page_scale_factor = webview()->pageScaleFactor();
  double page_scale_factor;
  if (function == PageZoom::RESET) {
    page_scale_factor = 1.0;
  } else {
    page_scale_factor = old_page_scale_factor +
        (function > 0 ? kScalingIncrement : -kScalingIncrement);
  }
  webview()->scalePage(page_scale_factor, WebPoint(0, 0));
#endif
  zoomLevelChanged();
}

void RenderView::OnSetZoomLevel(double zoom_level) {
  // Don't set zoom level for full-page plugin since they don't use the same
  // zoom settings.
  if (webview()->mainFrame()->document().isPluginDocument())
    return;

  webview()->hidePopups();
  webview()->setZoomLevel(false, zoom_level);
  zoomLevelChanged();
}

void RenderView::OnSetZoomLevelForLoadingURL(const GURL& url,
                                             double zoom_level) {
  host_zoom_levels_[url] = zoom_level;
}

void RenderView::OnExitFullscreen() {
  webview()->exitFullscreen();
}

void RenderView::OnSetPageEncoding(const std::string& encoding_name) {
  webview()->setPageEncoding(WebString::fromUTF8(encoding_name));
}

void RenderView::OnResetPageEncodingToDefault() {
  WebString no_encoding;
  webview()->setPageEncoding(no_encoding);
}

WebFrame* RenderView::GetChildFrame(const string16& xpath) const {
  if (xpath.empty())
    return webview()->mainFrame();

  // xpath string can represent a frame deep down the tree (across multiple
  // frame DOMs).
  // Example, /html/body/table/tbody/tr/td/iframe\n/frameset/frame[0]
  // should break into 2 xpaths
  // /html/body/table/tbody/tr/td/iframe & /frameset/frame[0]
  std::vector<string16> xpaths;
  base::SplitString(xpath, '\n', &xpaths);

  WebFrame* frame = webview()->mainFrame();
  for (std::vector<string16>::const_iterator i = xpaths.begin();
       frame && i != xpaths.end(); ++i) {
    frame = frame->findChildByExpression(*i);
  }

  return frame;
}

WebNode RenderView::GetFocusedNode() const {
  if (!webview())
    return WebNode();
  WebFrame* focused_frame = webview()->focusedFrame();
  if (focused_frame) {
    WebDocument doc = focused_frame->document();
    if (!doc.isNull())
      return doc.focusedNode();
  }

  return WebNode();
}

bool RenderView::IsEditableNode(const WebNode& node) {
  bool is_editable_node = false;
  if (!node.isNull()) {
    if (node.isContentEditable()) {
      is_editable_node = true;
    } else if (node.isElementNode()) {
      is_editable_node =
          node.toConst<WebElement>().isTextFormControlElement();
    }
  }
  return is_editable_node;
}

void RenderView::EvaluateScript(const string16& frame_xpath,
                                const string16& script,
                                int id,
                                bool notify_result) {
  v8::Handle<v8::Value> result;
  WebFrame* web_frame = GetChildFrame(frame_xpath);
  if (web_frame)
    result = web_frame->executeScriptAndReturnValue(WebScriptSource(script));
  if (notify_result) {
    ListValue list;
    if (!result.IsEmpty() && web_frame) {
      v8::HandleScope handle_scope;
      v8::Local<v8::Context> context = web_frame->mainWorldScriptContext();
      v8::Context::Scope context_scope(context);
      V8ValueConverter converter;
      converter.set_allow_date(true);
      converter.set_allow_regexp(true);
      list.Set(0, converter.FromV8Value(result, context));
    } else {
      list.Set(0, Value::CreateNullValue());
    }
    Send(new ViewHostMsg_ScriptEvalResponse(routing_id_, id, list));
  }
}

void RenderView::OnScriptEvalRequest(const string16& frame_xpath,
                                     const string16& jscript,
                                     int id,
                                     bool notify_result) {
  EvaluateScript(frame_xpath, jscript, id, notify_result);
}

void RenderView::OnCSSInsertRequest(const string16& frame_xpath,
                                    const std::string& css) {
  WebFrame* frame = GetChildFrame(frame_xpath);
  if (!frame)
    return;

  frame->document().insertUserStyleSheet(
      WebString::fromUTF8(css),
      WebDocument::UserStyleAuthorLevel);
}

void RenderView::OnAllowBindings(int enabled_bindings_flags) {
  enabled_bindings_ |= enabled_bindings_flags;
}

void RenderView::OnSetWebUIProperty(const std::string& name,
                                    const std::string& value) {
  DCHECK(BindingsPolicy::is_web_ui_enabled(enabled_bindings_));
  GetWebUIBindings()->SetProperty(name, value);
}

void RenderView::OnReservePageIDRange(int size_of_range) {
  next_page_id_ += size_of_range + 1;
}

void RenderView::OnDragTargetDragEnter(const WebDropData& drop_data,
                                       const gfx::Point& client_point,
                                       const gfx::Point& screen_point,
                                       WebDragOperationsMask ops) {
  WebDragOperation operation = webview()->dragTargetDragEnter(
      drop_data.ToDragData(),
      client_point,
      screen_point,
      ops);

  Send(new DragHostMsg_UpdateDragCursor(routing_id_, operation));
}

void RenderView::OnDragTargetDragOver(const gfx::Point& client_point,
                                      const gfx::Point& screen_point,
                                      WebDragOperationsMask ops) {
  WebDragOperation operation = webview()->dragTargetDragOver(
      client_point,
      screen_point,
      ops);

  Send(new DragHostMsg_UpdateDragCursor(routing_id_, operation));
}

void RenderView::OnDragTargetDragLeave() {
  webview()->dragTargetDragLeave();
}

void RenderView::OnDragTargetDrop(const gfx::Point& client_point,
                                  const gfx::Point& screen_point) {
  webview()->dragTargetDrop(client_point, screen_point);

  Send(new DragHostMsg_TargetDrop_ACK(routing_id_));
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

void RenderView::OnUpdateWebPreferences(const WebPreferences& prefs) {
  webkit_preferences_ = prefs;
  webkit_preferences_.Apply(webview());
}

void RenderView::OnUpdateRemoteAccessClientFirewallTraversal(
    const std::string& policy) {
  pepper_delegate_.PublishPolicy(policy);
}

void RenderView::OnSetAltErrorPageURL(const GURL& url) {
  alternate_error_page_url_ = url;
}

void RenderView::OnCustomContextMenuAction(
    const webkit_glue::CustomContextMenuContext& custom_context,
    unsigned action) {
  if (custom_context.is_pepper_menu)
    pepper_delegate_.OnCustomContextMenuAction(custom_context, action);
  else
    webview()->performCustomContextMenuAction(action);
}

void RenderView::OnEnumerateDirectoryResponse(
    int id,
    const std::vector<FilePath>& paths) {
  if (!enumeration_completions_[id])
    return;

  WebVector<WebString> ws_file_names(paths.size());
  for (size_t i = 0; i < paths.size(); ++i)
    ws_file_names[i] = webkit_glue::FilePathToWebString(paths[i]);

  enumeration_completions_[id]->didChooseFile(ws_file_names);
  enumeration_completions_.erase(id);
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

void RenderView::OnEnablePreferredSizeChangedMode(int flags) {
  DCHECK(flags != kPreferredSizeNothing);
  if (send_preferred_size_changes_)
    return;
  send_preferred_size_changes_ = true;

  // Start off with an initial preferred size notification (in case
  // |didUpdateLayout| was already called).
  if (webview())
    didUpdateLayout(webview()->mainFrame());
}

void RenderView::OnDisableScrollbarsForSmallWindows(
    const gfx::Size& disable_scrollbar_size_limit) {
  disable_scrollbars_size_limit_ = disable_scrollbar_size_limit;
}

void RenderView::OnSetRendererPrefs(const RendererPreferences& renderer_prefs) {
  renderer_preferences_ = renderer_prefs;
  UpdateFontRenderingFromRendererPrefs();
#if defined(TOOLKIT_USES_GTK)
  WebColorName name = WebKit::WebColorWebkitFocusRingColor;
  WebKit::setNamedColors(&name, &renderer_prefs.focus_ring_color, 1);
  WebKit::setCaretBlinkInterval(renderer_prefs.caret_blink_interval);
  gfx::NativeTheme::instance()->SetScrollbarColors(
      renderer_prefs.thumb_inactive_color,
      renderer_prefs.thumb_active_color,
      renderer_prefs.track_color);

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

void RenderView::OnEnableAccessibility() {
  if (WebAccessibilityCache::accessibilityEnabled())
    return;

  WebAccessibilityCache::enableAccessibility();

  if (webview()) {
    // It's possible that the webview has already loaded a webpage without
    // accessibility being enabled. Initialize the browser's cached
    // accessibility tree by sending it a 'load complete' notification.
    postAccessibilityNotification(
        webview()->accessibilityObject(),
        WebKit::WebAccessibilityNotificationLoadComplete);
  }
}

void RenderView::OnSetAccessibilityFocus(int acc_obj_id) {
  if (!accessibility_.get())
    return;

  WebAccessibilityObject obj = accessibility_->getObjectById(acc_obj_id);
  WebAccessibilityObject root = webview()->accessibilityObject();
  if (!obj.isValid() || !root.isValid())
    return;

  // By convention, calling SetFocus on the root of the tree should clear the
  // current focus. Otherwise set the focus to the new node.
  if (accessibility_->addOrGetId(obj) == accessibility_->addOrGetId(root))
    webview()->clearFocusedNode();
  else
    obj.setFocused(true);
}

void RenderView::OnAccessibilityDoDefaultAction(int acc_obj_id) {
  if (!accessibility_.get())
    return;

  WebAccessibilityObject obj = accessibility_->getObjectById(acc_obj_id);
  if (!obj.isValid())
    return;

  obj.performDefaultAction();
}

void RenderView::OnAccessibilityNotificationsAck() {
  DCHECK(accessibility_ack_pending_);
  accessibility_ack_pending_ = false;
  SendPendingAccessibilityNotifications();
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
  Send(new ViewHostMsg_SendCurrentPageAllSavableResourceLinks(routing_id(),
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

  WebPageSerializer::serialize(webview()->mainFrame(), true, this, weburl_links,
                               webstring_paths,
                               webkit_glue::FilePathToWebString(
                                   local_directory_name));
}

void RenderView::OnShouldClose() {
  bool should_close = webview()->dispatchBeforeUnloadEvent();
  Send(new ViewHostMsg_ShouldClose_ACK(routing_id_, should_close));
}

void RenderView::OnSwapOut(const ViewMsg_SwapOut_Params& params) {
  if (is_swapped_out_)
    return;

  // Swap this RenderView out so the tab can navigate to a page rendered by a
  // different process.  This involves running the unload handler and clearing
  // the page.  Once WasSwappedOut is called, we also allow this process to exit
  // if there are no other active RenderViews in it.

  // Send an UpdateState message before we get swapped out.
  SyncNavigationState();

  // Synchronously run the unload handler before sending the ACK.
  webview()->dispatchUnloadEvent();

  // Swap out and stop sending any IPC messages that are not ACKs.
  SetSwappedOut(true);

  // Replace the page with a blank dummy URL.  The unload handler will not be
  // run a second time, thanks to a check in FrameLoader::stopLoading.
  // TODO(creis): Need to add a better way to do this that avoids running the
  // beforeunload handler.  For now, we just run it a second time silently.
  webview()->mainFrame()->loadHTMLString(std::string(),
                                         GURL("about:swappedout"),
                                         GURL("about:swappedout"),
                                         false);

  // Just echo back the params in the ACK.
  Send(new ViewHostMsg_SwapOut_ACK(routing_id_, params));
}

void RenderView::OnClosePage() {
  // TODO(creis): We'd rather use webview()->Close() here, but that currently
  // sets the WebView's delegate_ to NULL, preventing any JavaScript dialogs
  // in the onunload handler from appearing.  For now, we're bypassing that and
  // calling the FrameLoader's CloseURL method directly.  This should be
  // revisited to avoid having two ways to close a page.  Having a single way
  // to close that can run onunload is also useful for fixing
  // http://b/issue?id=753080.
  webview()->dispatchUnloadEvent();

  Send(new ViewHostMsg_ClosePage_ACK(routing_id_));
}

void RenderView::OnThemeChanged() {
#if defined(OS_WIN)
  gfx::NativeThemeWin::instance()->CloseHandles();
  if (webview())
    webview()->themeChanged();
#else  // defined(OS_WIN)
  // TODO(port): we don't support theming on non-Windows platforms yet
  NOTIMPLEMENTED();
#endif
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
                        GURL(chrome::kUnreachableWebDataURL),
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

void RenderView::AltErrorPageFinished(WebFrame* frame,
                                      const WebURLError& original_error,
                                      const std::string& html) {
  // Here, we replace the blank page we loaded previously.
  // If we failed to download the alternate error page, LoadNavigationErrorPage
  // will simply display a default error page.
  LoadNavigationErrorPage(frame, WebURLRequest(), original_error, html, true);
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
      webview()->mainFrame()->setCanHaveScrollbars(
          should_display_scrollbars(new_size.width(), new_size.height()));
    }
    UpdateScrollState(webview()->mainFrame());
  }

  RenderWidget::OnResize(new_size, resizer_rect);
}

void RenderView::DidInitiatePaint() {
  // Notify the pepper plugins that we started painting.
  pepper_delegate_.ViewInitiatedPaint();
}

void RenderView::DidFlushPaint() {
  // Notify any pepper plugins that we painted. This will call into the plugin,
  // and we it may ask to close itself as a result. This will, in turn, modify
  // our set, possibly invalidating the iterator. So we iterate on a copy that
  // won't change out from under us.
  pepper_delegate_.ViewFlushedPaint();

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

#if defined(TOUCH_UI)
  SyncSelectionIfRequired();
#endif
}

void RenderView::OnViewContextSwapBuffersPosted() {
  RenderWidget::OnSwapBuffersPosted();
}

void RenderView::OnViewContextSwapBuffersComplete() {
  RenderWidget::OnSwapBuffersComplete();
}

void RenderView::OnViewContextSwapBuffersAborted() {
  RenderWidget::OnSwapBuffersAborted();
}

webkit::ppapi::PluginInstance* RenderView::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip) {
  return pepper_delegate_.GetBitmapForOptimizedPluginPaint(
      paint_bounds, dib, location, clip);
}

gfx::Point RenderView::GetScrollOffset() {
  WebSize scroll_offset = webview()->mainFrame()->scrollOffset();
  return gfx::Point(scroll_offset.width, scroll_offset.height);
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

void RenderView::OnPluginImeCompositionCompleted(const string16& text,
                                                 int plugin_id) {
  // WebPluginDelegateProxy is responsible for figuring out if this event
  // applies to it or not, so inform all the delegates.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->ImeCompositionCompleted(text, plugin_id);
  }
}
#endif  // OS_MACOSX

void RenderView::postAccessibilityNotification(
    const WebAccessibilityObject& obj,
    WebAccessibilityNotification notification) {
  if (!accessibility_.get() && webview()) {
    // Create and initialize our accessibility cache
    accessibility_.reset(WebAccessibilityCache::create());
    accessibility_->initialize(webview());

    // Load complete should be our first notification sent. Send it manually
    // in cases where we don't get it first to avoid focus problems.
    // TODO(ctguil): Investigate if a different notification is a WebCore bug.
    if (notification != WebKit::WebAccessibilityNotificationLoadComplete) {
      postAccessibilityNotification(accessibility_->getObjectById(1000),
          WebKit::WebAccessibilityNotificationLoadComplete);
    }
  }

  if (!accessibility_->isCached(obj)) {
    // The browser doesn't know about objects that are not in the cache. Send a
    // children change for the first accestor that actually is in the cache.
    WebAccessibilityObject parent = obj;
    while (parent.isValid() && !accessibility_->isCached(parent))
      parent = parent.parentObject();

    DCHECK(parent.isValid() && accessibility_->isCached(parent));
    if (!parent.isValid())
      return;
    postAccessibilityNotification(
        parent, WebKit::WebAccessibilityNotificationChildrenChanged);

    // The parent's children change takes care of the child's children change.
    if (notification == WebKit::WebAccessibilityNotificationChildrenChanged)
      return;
  }

  // Add the accessibility object to our cache and ensure it's valid.
  RendererAccessibilityNotification acc_notification;
  acc_notification.id = accessibility_->addOrGetId(obj);
  acc_notification.type = notification;
  if (acc_notification.id < 0)
    return;

  ViewHostMsg_AccEvent::Value temp;
  if (!WebAccessibilityNotificationToViewHostMsg(notification, &temp))
    return;

  // Discard duplicate accessibility notifications.
  for (uint32 i = 0; i < pending_accessibility_notifications_.size(); i++) {
    if (pending_accessibility_notifications_[i].id == acc_notification.id &&
        pending_accessibility_notifications_[i].type == acc_notification.type) {
      return;
    }
  }
  pending_accessibility_notifications_.push_back(acc_notification);

  if (!accessibility_ack_pending_ && accessibility_method_factory_.empty()) {
    // When no accessibility notifications are in-flight post a task to send
    // the notifications to the browser. We use PostTask so that we can queue
    // up additional notifications.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        accessibility_method_factory_.NewRunnableMethod(
            &RenderView::SendPendingAccessibilityNotifications));
  }
}

void RenderView::OnSetEditCommandsForNextKeyEvent(
    const EditCommands& edit_commands) {
  edit_commands_ = edit_commands;
}

void RenderView::Close() {
  // We need to grab a pointer to the doomed WebView before we destroy it.
  WebView* doomed = webview();
  RenderWidget::Close();
  g_view_map.Get().erase(doomed);
}

void RenderView::DidHandleKeyEvent() {
  edit_commands_.clear();
}

void RenderView::DidHandleMouseEvent(const WebKit::WebMouseEvent& event) {
  FOR_EACH_OBSERVER(RenderViewObserver, observers_, DidHandleMouseEvent(event));
}

void RenderView::OnWasHidden() {
  RenderWidget::OnWasHidden();

  if (webview()) {
    webview()->settings()->setMinimumTimerInterval(
        webkit_glue::kBackgroundTabTimerInterval);
    webview()->setVisibilityState(visibilityState(), false);
  }

#if defined(OS_MACOSX)
  // Inform plugins that their container is no longer visible.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(false);
  }
#endif  // OS_MACOSX
}

void RenderView::OnWasRestored(bool needs_repainting) {
  RenderWidget::OnWasRestored(needs_repainting);

  if (webview()) {
    webview()->settings()->setMinimumTimerInterval(
        webkit_glue::kForegroundTabTimerInterval);
    webview()->setVisibilityState(visibilityState(), false);
  }

#if defined(OS_MACOSX)
  // Inform plugins that their container is now visible.
  std::set<WebPluginDelegateProxy*>::iterator plugin_it;
  for (plugin_it = plugin_delegates_.begin();
       plugin_it != plugin_delegates_.end(); ++plugin_it) {
    (*plugin_it)->SetContainerVisibility(true);
  }
#endif  // OS_MACOSX
}

bool RenderView::SupportsAsynchronousSwapBuffers() {
  WebKit::WebGraphicsContext3D* context = webview()->graphicsContext3D();
  if (!context)
    return false;
  std::string extensions(context->getRequestableExtensionsCHROMIUM().utf8());
  return extensions.find("GL_CHROMIUM_swapbuffers_complete_callback") !=
      std::string::npos;
}

void RenderView::OnSetFocus(bool enable) {
  RenderWidget::OnSetFocus(enable);

  if (webview() && webview()->isActive()) {
    // Notify all NPAPI plugins.
    std::set<WebPluginDelegateProxy*>::iterator plugin_it;
    for (plugin_it = plugin_delegates_.begin();
         plugin_it != plugin_delegates_.end(); ++plugin_it) {
#if defined(OS_MACOSX)
      // RenderWidget's call to setFocus can cause the underlying webview's
      // activation state to change just like a call to setIsActive.
      if (enable)
        (*plugin_it)->SetWindowFocus(true);
#endif
      (*plugin_it)->SetContentAreaFocus(enable);
    }

    // Notify all Pepper plugins.
    pepper_delegate_.OnSetFocus(enable);
  }
}

void RenderView::PpapiPluginFocusChanged() {
  UpdateInputMethod();
}

void RenderView::RequestRemoteAccessClientFirewallTraversal() {
  Send(new ViewHostMsg_RequestRemoteAccessClientFirewallTraversal(routing_id_));
}

void RenderView::OnImeSetComposition(
    const string16& text,
    const std::vector<WebKit::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  // Until PPAPI has an interface for handling IME events, we skip sending
  // OnImeSetComposition. Otherwise the composition is canceled when a
  // non-editable DOM element is focused.
  //
  // TODO(kinaba) This temporal remedy can be removed after PPAPI is extended
  // with an IME handling interface.
  if (!pepper_delegate_.IsPluginFocused()) {
    RenderWidget::OnImeSetComposition(text,
                                      underlines,
                                      selection_start,
                                      selection_end);
  }
}

void RenderView::OnImeConfirmComposition(const string16& text) {
  if (pepper_delegate_.IsPluginFocused()) {
    // TODO(kinaba) Until PPAPI has an interface for handling IME events, we
    // send character events.
    for (size_t i = 0; i < text.size(); ++i) {
      WebKit::WebKeyboardEvent char_event;
      char_event.type = WebKit::WebInputEvent::Char;
      char_event.timeStampSeconds = base::Time::Now().ToDoubleT();
      char_event.modifiers = 0;
      char_event.windowsKeyCode = text[i];
      char_event.nativeKeyCode = text[i];
      char_event.text[0] = text[i];
      char_event.unmodifiedText[0] = text[i];
      if (webwidget_)
        webwidget_->handleInputEvent(char_event);
    }
  } else {
    RenderWidget::OnImeConfirmComposition(text);
  }
}

ui::TextInputType RenderView::GetTextInputType() {
  if (pepper_delegate_.IsPluginFocused()) {
    // TODO(kinaba) Until PPAPI has an interface for handling IME events, we
    // consider all the parts of PPAPI plugins are accepting text inputs.
    return ui::TEXT_INPUT_TYPE_TEXT;
  }
  return RenderWidget::GetTextInputType();
}

bool RenderView::CanComposeInline() {
  if (pepper_delegate_.IsPluginFocused()) {
    // TODO(kinaba) Until PPAPI has an interface for handling IME events, there
    // is no way for the browser to know whether the plugin is capable of
    // drawing composition text.  We assume plugins are incapable and let the
    // browser handle composition display for now.
    return false;
  }
  return true;
}

#if defined(OS_MACOSX)
void RenderView::PluginFocusChanged(bool focused, int plugin_id) {
  IPC::Message* msg = new ViewHostMsg_PluginFocusChanged(routing_id(),
                                                         focused, plugin_id);
  Send(msg);
}

void RenderView::StartPluginIme() {
  IPC::Message* msg = new ViewHostMsg_StartPluginIme(routing_id());
  // This message can be sent during event-handling, and needs to be delivered
  // within that context.
  msg->set_unblock(true);
  Send(msg);
}

gfx::PluginWindowHandle RenderView::AllocateFakePluginWindowHandle(
    bool opaque, bool root) {
  gfx::PluginWindowHandle window = NULL;
  Send(new ViewHostMsg_AllocateFakePluginWindowHandle(
      routing_id(), opaque, root, &window));
  if (window) {
    fake_plugin_window_handles_.insert(window);
  }
  return window;
}

void RenderView::DestroyFakePluginWindowHandle(gfx::PluginWindowHandle window) {
  if (window && fake_plugin_window_handles_.find(window) !=
      fake_plugin_window_handles_.end()) {
    Send(new ViewHostMsg_DestroyFakePluginWindowHandle(routing_id(), window));
    fake_plugin_window_handles_.erase(window);
  }
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
    gfx::PluginWindowHandle window, uint64 surface_id) {
  Send(new ViewHostMsg_AcceleratedSurfaceBuffersSwapped(
      routing_id(), window, surface_id));
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

WebKit::WebGeolocationClient* RenderView::geolocationClient() {
  if (!geolocation_dispatcher_)
    geolocation_dispatcher_ = new GeolocationDispatcher(this);
  return geolocation_dispatcher_;
}

WebKit::WebSpeechInputController* RenderView::speechInputController(
    WebKit::WebSpeechInputListener* listener) {
  if (!speech_input_dispatcher_)
    speech_input_dispatcher_ = new SpeechInputDispatcher(this, listener);
  return speech_input_dispatcher_;
}

WebKit::WebDeviceOrientationClient* RenderView::deviceOrientationClient() {
  if (!device_orientation_dispatcher_)
    device_orientation_dispatcher_ = new DeviceOrientationDispatcher(this);
  return device_orientation_dispatcher_;
}

void RenderView::zoomLimitsChanged(double minimum_level, double maximum_level) {
  // For now, don't remember plugin zoom values.  We don't want to mix them with
  // normal web content (i.e. a fixed layout plugin would usually want them
  // different).
  bool remember = !webview()->mainFrame()->document().isPluginDocument();

  int minimum_percent = static_cast<int>(
      WebView::zoomLevelToZoomFactor(minimum_level) * 100);
  int maximum_percent = static_cast<int>(
      WebView::zoomLevelToZoomFactor(maximum_level) * 100);

  Send(new ViewHostMsg_UpdateZoomLimits(
      routing_id_, minimum_percent, maximum_percent, remember));
}

void RenderView::zoomLevelChanged() {
  bool remember = !webview()->mainFrame()->document().isPluginDocument();
#if defined(TOUCH_UI)
  float zoom_level =
      WebView::zoomFactorToZoomLevel(webview()->pageScaleFactor());
#else
  float zoom_level = webview()->zoomLevel();
#endif
  // Tell the browser which url got zoomed so it can update the menu and the
  // saved values if necessary
  Send(new ViewHostMsg_DidZoomURL(
      routing_id_, zoom_level, remember,
      GURL(webview()->mainFrame()->document().url())));
}

void RenderView::registerProtocolHandler(const WebString& scheme,
                                         const WebString& base_url,
                                         const WebString& url,
                                         const WebString& title) {
  GURL base(base_url);
  GURL absolute_url = base.Resolve(UTF16ToUTF8(url));
  if (base.GetOrigin() != absolute_url.GetOrigin()) {
    return;
  }
  RenderThread::current()->Send(
      new ViewHostMsg_RegisterProtocolHandler(routing_id_,
                                              UTF16ToUTF8(scheme),
                                              absolute_url,
                                              title));
}

void RenderView::registerIntentHandler(const WebString& action,
                                       const WebString& type,
                                       const WebString& href,
                                       const WebString& title) {
  RenderThread::current()->Send(
      new ViewHostMsg_RegisterIntentHandler(routing_id_,
                                            action,
                                            type,
                                            href,
                                            title));
}

WebKit::WebPageVisibilityState RenderView::visibilityState() const {
  WebKit::WebPageVisibilityState current_state = is_hidden() ?
      WebKit::WebPageVisibilityStateHidden :
      WebKit::WebPageVisibilityStateVisible;
  WebKit::WebPageVisibilityState override_state = current_state;
  if (content::GetContentClient()->renderer()->
          ShouldOverridePageVisibilityState(this,
                                            &override_state))
    return override_state;
  return current_state;
}

void RenderView::startActivity(const WebKit::WebString& action,
                               const WebKit::WebString& type,
                               const WebKit::WebString& data,
                               int intent_id) {
  RenderThread::current()->Send(new ViewHostMsg_WebIntentDispatch(
      routing_id_, action, type, data, intent_id));
}

bool RenderView::IsNonLocalTopLevelNavigation(
    const GURL& url, WebKit::WebFrame* frame, WebKit::WebNavigationType type) {
  // Must be a top level frame.
  if (frame->parent() != NULL)
    return false;

  // Navigations initiated within Webkit are not sent out to the external host
  // in the following cases.
  // 1. The url scheme is not http/https
  // 2. The origin of the url and the opener is the same in which case the
  //    opener relationship is maintained.
  // 3. Reloads/form submits/back forward navigations
  if (!url.SchemeIs(chrome::kHttpScheme) && !url.SchemeIs(chrome::kHttpsScheme))
    return false;

  // Not interested in reloads/form submits/resubmits/back forward navigations.
  if (type != WebKit::WebNavigationTypeReload &&
      type != WebKit::WebNavigationTypeFormSubmitted &&
      type != WebKit::WebNavigationTypeFormResubmitted &&
      type != WebKit::WebNavigationTypeBackForward) {
    // The opener relationship between the new window and the parent allows the
    // new window to script the parent and vice versa. This is not allowed if
    // the origins of the two domains are different. This can be treated as a
    // top level navigation and routed back to the host.
    WebKit::WebFrame* opener = frame->opener();
    if (!opener) {
      return true;
    } else {
      if (url.GetOrigin() != GURL(opener->document().url()).GetOrigin())
        return true;
    }
  }
  return false;
}

void RenderView::OnAsyncFileOpened(base::PlatformFileError error_code,
                                   IPC::PlatformFileForTransit file_for_transit,
                                   int message_id) {
  pepper_delegate_.OnAsyncFileOpened(
      error_code,
      IPC::PlatformFileForTransitToPlatformFile(file_for_transit),
      message_id);
}

void RenderView::OnPpapiBrokerChannelCreated(
    int request_id,
    base::ProcessHandle broker_process_handle,
    const IPC::ChannelHandle& handle) {
  pepper_delegate_.OnPpapiBrokerChannelCreated(request_id,
                                               broker_process_handle,
                                               handle);
}

#if defined(OS_MACOSX)
void RenderView::OnSelectPopupMenuItem(int selected_index) {
  if (external_popup_menu_ == NULL) {
    // Crash reports from the field indicate that we can be notified with a
    // NULL external popup menu (we probably get notified twice).
    // If you hit this please file a bug against jcivelli and include the page
    // and steps to repro.
    NOTREACHED();
    return;
  }
  external_popup_menu_->DidSelectItem(selected_index);
  external_popup_menu_.reset();
}
#endif

#if defined(ENABLE_FLAPPER_HACKS)
void RenderView::OnConnectTcpACK(
    int request_id,
    IPC::PlatformFileForTransit socket_for_transit,
    const PP_Flash_NetAddress& local_addr,
    const PP_Flash_NetAddress& remote_addr) {
  pepper_delegate_.OnConnectTcpACK(
      request_id,
      IPC::PlatformFileForTransitToPlatformFile(socket_for_transit),
      local_addr,
      remote_addr);
}
#endif

void RenderView::OnContextMenuClosed(
    const webkit_glue::CustomContextMenuContext& custom_context) {
  if (custom_context.is_pepper_menu)
    pepper_delegate_.OnContextMenuClosed(custom_context);
  else
    context_menu_node_.reset();
}

void RenderView::OnEnableViewSourceMode() {
  if (!webview())
    return;
  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;
  main_frame->enableViewSourceMode(true);
}
