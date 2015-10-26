// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_impl.h"

#include <map>
#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/asan_invalid_access.h"
#include "base/debug/dump_without_crashing.h"
#include "base/i18n/char_iterator.h"
#include "base/metrics/histogram.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/base/switches.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/permissions/permission_dispatcher.h"
#include "content/child/plugin_messages.h"
#include "content/child/quota_dispatcher.h"
#include "content/child/request_extra_data.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_network_provider.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/web_service_worker_provider_impl.h"
#include "content/child/v8_value_converter_impl.h"
#include "content/child/web_url_loader_impl.h"
#include "content/child/web_url_request_util.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/child/websocket_bridge.h"
#include "content/child/weburlresponse_extradata_impl.h"
#include "content/common/accessibility_messages.h"
#include "content/common/clipboard_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/navigation_params.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/page_state.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/context_menu_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/accessibility/renderer_accessibility.h"
#include "content/renderer/bluetooth/web_bluetooth_impl.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/child_frame_compositing_helper.h"
#include "content/renderer/context_menu_params_builder.h"
#include "content/renderer/devtools/devtools_agent.h"
#include "content/renderer/dom_automation_controller.h"
#include "content/renderer/external_popup_menu.h"
#include "content/renderer/geolocation_dispatcher.h"
#include "content/renderer/gpu/gpu_benchmarking_extension.h"
#include "content/renderer/history_controller.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/image_downloader/image_downloader_impl.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/manifest/manifest_manager.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "content/renderer/media/crypto/render_cdm_factory.h"
#include "content/renderer/media/media_permission_dispatcher_impl.h"
#include "content/renderer/media/media_permission_dispatcher_proxy.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_renderer_factory_impl.h"
#include "content/renderer/media/midi_dispatcher.h"
#include "content/renderer/media/render_media_log.h"
#include "content/renderer/media/user_media_client_impl.h"
#include "content/renderer/media/webmediaplayer_ms.h"
#include "content/renderer/memory_benchmarking_extension.h"
#include "content/renderer/mojo/service_registry_js_wrapper.h"
#include "content/renderer/navigation_state_impl.h"
#include "content/renderer/notification_permission_dispatcher.h"
#include "content/renderer/npapi/plugin_channel_host.h"
#include "content/renderer/pepper/plugin_instance_throttler_impl.h"
#include "content/renderer/presentation/presentation_dispatcher.h"
#include "content/renderer/push_messaging/push_messaging_dispatcher.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_webapplicationcachehost_impl.h"
#include "content/renderer/renderer_webcolorchooser_impl.h"
#include "content/renderer/savable_resources.h"
#include "content/renderer/screen_orientation/screen_orientation_dispatcher.h"
#include "content/renderer/shared_worker_repository.h"
#include "content/renderer/skia_benchmarking_extension.h"
#include "content/renderer/stats_collection_controller.h"
#include "content/renderer/web_ui_extension.h"
#include "content/renderer/websharedworker_proxy.h"
#include "gin/modules/module_registry.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/media_log.h"
#include "media/blink/webencryptedmediaclient_impl.h"
#include "media/blink/webmediaplayer_impl.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "mojo/common/url_type_converters.h"
#include "net/base/data_url.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_util.h"
#include "third_party/WebKit/public/platform/WebStorageQuotaCallbacks.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/webusb/WebUSBClient.h"
#include "third_party/WebKit/public/web/WebColorSuggestion.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebGlyphCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebMediaStreamRegistry.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebPageSerializer.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSearchableFormData.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebSurroundingText.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/mojo/src/mojo/edk/js/core.h"
#include "third_party/mojo/src/mojo/edk/js/support.h"
#include "url/url_util.h"

#if defined(ENABLE_PLUGINS)
#include "content/renderer/npapi/webplugin_impl.h"
#include "content/renderer/pepper/pepper_browser_connection.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_webplugin_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "content/renderer/media/rtc_peer_connection_handler.h"
#endif

#if defined(OS_ANDROID)
#include <cpu-features.h>

#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/java/gin_java_bridge_dispatcher.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/android/stream_texture_factory_impl.h"
#include "content/renderer/media/android/webmediaplayer_android.h"
#include "content/renderer/media/android/webmediasession_android.h"
#else
#include "cc/blink/context_provider_web_context.h"
#include "content/renderer/usb/web_usb_client_impl.h"
#include "device/devices_app/public/cpp/constants.h"
#endif

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/pepper_cdm_wrapper_impl.h"
#elif defined(ENABLE_BROWSER_CDMS)
#include "content/renderer/media/crypto/renderer_cdm_manager.h"
#endif

#if defined(ENABLE_MOJO_MEDIA)
#include "media/mojo/services/mojo_cdm_factory.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"
#endif

#if defined(ENABLE_MOJO_MEDIA) && !defined(ENABLE_MEDIA_PIPELINE_ON_ANDROID)
#include "media/mojo/services/mojo_renderer_factory.h"
#else
#include "media/renderers/default_renderer_factory.h"
#endif

#if defined(ENABLE_WEBVR)
#include "content/renderer/vr/vr_dispatcher.h"
#endif

using blink::WebContentDecryptionModule;
using blink::WebContextMenuData;
using blink::WebCString;
using blink::WebData;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebDOMEvent;
using blink::WebDOMMessageEvent;
using blink::WebElement;
using blink::WebExternalPopupMenu;
using blink::WebExternalPopupMenuClient;
using blink::WebFrame;
using blink::WebHistoryItem;
using blink::WebHTTPBody;
using blink::WebLocalFrame;
using blink::WebMediaPlayer;
using blink::WebMediaPlayerClient;
using blink::WebMediaPlayerEncryptedMediaClient;
using blink::WebMediaSession;
using blink::WebNavigationPolicy;
using blink::WebNavigationType;
using blink::WebNode;
using blink::WebPageSerializer;
using blink::WebPageSerializerClient;
using blink::WebPluginParams;
using blink::WebPopupMenuInfo;
using blink::WebRange;
using blink::WebReferrerPolicy;
using blink::WebScriptSource;
using blink::WebSearchableFormData;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebSerializedScriptValue;
using blink::WebServiceWorkerProvider;
using blink::WebSettings;
using blink::WebStorageQuotaCallbacks;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::WebUserGestureIndicator;
using blink::WebVector;
using blink::WebView;
using base::Time;
using base::TimeDelta;

namespace content {

namespace {

const char kDefaultAcceptHeader[] =
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/"
    "*;q=0.8";
const char kAcceptHeader[] = "Accept";

const size_t kExtraCharsBeforeAndAfterSelection = 100;

typedef std::map<int, RenderFrameImpl*> RoutingIDFrameMap;
static base::LazyInstance<RoutingIDFrameMap> g_routing_id_frame_map =
    LAZY_INSTANCE_INITIALIZER;

typedef std::map<blink::WebFrame*, RenderFrameImpl*> FrameMap;
base::LazyInstance<FrameMap> g_frame_map = LAZY_INSTANCE_INITIALIZER;

int64 ExtractPostId(HistoryEntry* entry) {
  if (!entry)
    return -1;

  const WebHistoryItem& item = entry->root();
  if (item.isNull() || item.httpBody().isNull())
    return -1;

  return item.httpBody().identifier();
}

WebURLResponseExtraDataImpl* GetExtraDataFromResponse(
    const WebURLResponse& response) {
  return static_cast<WebURLResponseExtraDataImpl*>(response.extraData());
}

void GetRedirectChain(WebDataSource* ds, std::vector<GURL>* result) {
  // Replace any occurrences of swappedout:// with about:blank.
  const WebURL& blank_url = GURL(url::kAboutBlankURL);
  WebVector<WebURL> urls;
  ds->redirectChain(urls);
  result->reserve(urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    if (urls[i] != GURL(kSwappedOutURL))
      result->push_back(urls[i]);
    else
      result->push_back(blank_url);
  }
}

// Returns the original request url. If there is no redirect, the original
// url is the same as ds->request()->url(). If the WebDataSource belongs to a
// frame was loaded by loadData, the original url will be ds->unreachableURL()
GURL GetOriginalRequestURL(WebDataSource* ds) {
  // WebDataSource has unreachable URL means that the frame is loaded through
  // blink::WebFrame::loadData(), and the base URL will be in the redirect
  // chain. However, we never visited the baseURL. So in this case, we should
  // use the unreachable URL as the original URL.
  if (ds->hasUnreachableURL())
    return ds->unreachableURL();

  std::vector<GURL> redirects;
  GetRedirectChain(ds, &redirects);
  if (!redirects.empty())
    return redirects.at(0);

  return ds->originalRequest().url();
}

bool IsBrowserInitiated(NavigationParams* pending) {
  // A navigation resulting from loading a javascript URL should not be treated
  // as a browser initiated event.  Instead, we want it to look as if the page
  // initiated any load resulting from JS execution.
  return pending &&
         !pending->common_params.url.SchemeIs(url::kJavaScriptScheme);
}

NOINLINE void CrashIntentionally() {
  // NOTE(shess): Crash directly rather than using NOTREACHED() so
  // that the signature is easier to triage in crash reports.
  //
  // Linker's ICF feature may merge this function with other functions with the
  // same definition and it may confuse the crash report processing system.
  static int static_variable_to_make_this_function_unique = 0;
  base::debug::Alias(&static_variable_to_make_this_function_unique);

  volatile int* zero = nullptr;
  *zero = 0;
}

NOINLINE void BadCastCrashIntentionally() {
  class A {
    virtual void f() {}
  };

  class B {
    virtual void f() {}
  };

  A a;
  (void)(B*)&a;
}

#if defined(ADDRESS_SANITIZER) || defined(SYZYASAN)
NOINLINE void MaybeTriggerAsanError(const GURL& url) {
  // NOTE(rogerm): We intentionally perform an invalid heap access here in
  //     order to trigger an Address Sanitizer (ASAN) error report.
  const char kCrashDomain[] = "crash";
  const char kHeapOverflow[] = "/heap-overflow";
  const char kHeapUnderflow[] = "/heap-underflow";
  const char kUseAfterFree[] = "/use-after-free";
#if defined(SYZYASAN)
  const char kCorruptHeapBlock[] = "/corrupt-heap-block";
  const char kCorruptHeap[] = "/corrupt-heap";
#endif

  if (!url.DomainIs(kCrashDomain))
    return;

  if (!url.has_path())
    return;

  std::string crash_type(url.path());
  if (crash_type == kHeapOverflow) {
    base::debug::AsanHeapOverflow();
  } else if (crash_type == kHeapUnderflow) {
    base::debug::AsanHeapUnderflow();
  } else if (crash_type == kUseAfterFree) {
    base::debug::AsanHeapUseAfterFree();
#if defined(SYZYASAN)
  } else if (crash_type == kCorruptHeapBlock) {
    base::debug::AsanCorruptHeapBlock();
  } else if (crash_type == kCorruptHeap) {
    base::debug::AsanCorruptHeap();
#endif
  }
}
#endif  // ADDRESS_SANITIZER || SYZYASAN

void MaybeHandleDebugURL(const GURL& url) {
  if (!url.SchemeIs(kChromeUIScheme))
    return;
  if (url == GURL(kChromeUIBadCastCrashURL)) {
    BadCastCrashIntentionally();
  } else if (url == GURL(kChromeUICrashURL)) {
    CrashIntentionally();
  } else if (url == GURL(kChromeUIDumpURL)) {
    // This URL will only correctly create a crash dump file if content is
    // hosted in a process that has correctly called
    // base::debug::SetDumpWithoutCrashingFunction.  Refer to the documentation
    // of base::debug::DumpWithoutCrashing for more details.
    base::debug::DumpWithoutCrashing();
  } else if (url == GURL(kChromeUIKillURL)) {
    base::Process::Current().Terminate(1, false);
  } else if (url == GURL(kChromeUIHangURL)) {
    for (;;) {
      base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
    }
  } else if (url == GURL(kChromeUIShorthangURL)) {
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(20));
  }

#if defined(ADDRESS_SANITIZER) || defined(SYZYASAN)
  MaybeTriggerAsanError(url);
#endif  // ADDRESS_SANITIZER || SYZYASAN
}

// Returns false unless this is a top-level navigation.
bool IsTopLevelNavigation(WebFrame* frame) {
  return frame->parent() == NULL;
}

WebURLRequest CreateURLRequestForNavigation(
    const CommonNavigationParams& common_params,
    scoped_ptr<StreamOverrideParameters> stream_override,
    bool is_view_source_mode_enabled) {
  WebURLRequest request(common_params.url);
  if (is_view_source_mode_enabled)
    request.setCachePolicy(WebURLRequest::ReturnCacheDataElseLoad);

  if (common_params.referrer.url.is_valid()) {
    WebString web_referrer = WebSecurityPolicy::generateReferrerHeader(
        common_params.referrer.policy,
        common_params.url,
        WebString::fromUTF8(common_params.referrer.url.spec()));
    if (!web_referrer.isEmpty())
      request.setHTTPReferrer(web_referrer, common_params.referrer.policy);
  }

  RequestExtraData* extra_data = new RequestExtraData();
  extra_data->set_stream_override(stream_override.Pass());
  request.setExtraData(extra_data);

  // Set the ui timestamp for this navigation. Currently the timestamp here is
  // only non empty when the navigation was triggered by an Android intent. The
  // timestamp is converted to a double version supported by blink. It will be
  // passed back to the browser in the DidCommitProvisionalLoad and the
  // DocumentLoadComplete IPCs.
  base::TimeDelta ui_timestamp = common_params.ui_timestamp - base::TimeTicks();
  request.setUiStartTime(ui_timestamp.InSecondsF());
  request.setInputPerfMetricReportPolicy(
      static_cast<WebURLRequest::InputToLoadPerfMetricReportPolicy>(
          common_params.report_type));
  return request;
}

void UpdateFrameNavigationTiming(WebFrame* frame,
                                 base::TimeTicks browser_navigation_start,
                                 base::TimeTicks renderer_navigation_start) {
  // The browser provides the navigation_start time to bootstrap the
  // Navigation Timing information for the browser-initiated navigations. In
  // case of cross-process navigations, this carries over the time of
  // finishing the onbeforeunload handler of the previous page.
  DCHECK(!browser_navigation_start.is_null());
  if (frame->provisionalDataSource()) {
    // |browser_navigation_start| is likely before this process existed, so we
    // can't use InterProcessTimeTicksConverter. We need at least to ensure
    // that the browser-side navigation start we set is not later than the one
    // on the renderer side.
    base::TimeTicks navigation_start = std::min(
        browser_navigation_start, renderer_navigation_start);
    double navigation_start_seconds =
        (navigation_start - base::TimeTicks()).InSecondsF();
    frame->provisionalDataSource()->setNavigationStartTime(
        navigation_start_seconds);
    // TODO(clamy): We need to provide additional timing values for the
    // Navigation Timing API to work with browser-side navigations.
  }
}

// PlzNavigate
CommonNavigationParams MakeCommonNavigationParams(
    blink::WebURLRequest* request,
    bool should_replace_current_entry) {
  const RequestExtraData kEmptyData;
  const RequestExtraData* extra_data =
      static_cast<RequestExtraData*>(request->extraData());
  if (!extra_data)
    extra_data = &kEmptyData;
  Referrer referrer(
      GURL(request->httpHeaderField(WebString::fromUTF8("Referer")).latin1()),
      request->referrerPolicy());

  // Set the ui timestamp for this navigation. Currently the timestamp here is
  // only non empty when the navigation was triggered by an Android intent, or
  // by the user clicking on a link. The timestamp is converted from a double
  // version supported by blink. It will be passed back to the renderer in the
  // CommitNavigation IPC, and then back to the browser again in the
  // DidCommitProvisionalLoad and the DocumentLoadComplete IPCs.
  base::TimeTicks ui_timestamp =
      base::TimeTicks() + base::TimeDelta::FromSecondsD(request->uiStartTime());
  FrameMsg_UILoadMetricsReportType::Value report_type =
      static_cast<FrameMsg_UILoadMetricsReportType::Value>(
          request->inputPerfMetricReportPolicy());
  return CommonNavigationParams(
      request->url(), referrer, extra_data->transition_type(),
      FrameMsg_Navigate_Type::NORMAL, true, should_replace_current_entry,
      ui_timestamp, report_type, GURL(), GURL(), LOFI_UNSPECIFIED);
}

#if !defined(OS_ANDROID) || defined(ENABLE_MEDIA_PIPELINE_ON_ANDROID)
media::Context3D GetSharedMainThreadContext3D() {
  cc::ContextProvider* provider =
      RenderThreadImpl::current()->SharedMainThreadContextProvider().get();
  if (!provider)
    return media::Context3D();
  return media::Context3D(provider->ContextGL(), provider->GrContext());
}
#endif

bool IsReload(FrameMsg_Navigate_Type::Value navigation_type) {
  return navigation_type == FrameMsg_Navigate_Type::RELOAD ||
         navigation_type == FrameMsg_Navigate_Type::RELOAD_IGNORING_CACHE ||
         navigation_type == FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
}

// Returns the routing ID of the RenderFrameImpl or RenderFrameProxy
// associated with |web_frame|.
int GetRoutingIdForFrameOrProxy(WebFrame* web_frame) {
  if (!web_frame)
    return MSG_ROUTING_NONE;
  if (web_frame->isWebRemoteFrame())
    return RenderFrameProxy::FromWebFrame(web_frame)->routing_id();
  return RenderFrameImpl::FromWebFrame(web_frame)->GetRoutingID();
}

RenderFrameImpl::CreateRenderFrameImplFunction g_create_render_frame_impl =
    nullptr;

void OnGotContentHandlerID(uint32_t content_handler_id) {}

}  // namespace

// static
RenderFrameImpl* RenderFrameImpl::Create(RenderViewImpl* render_view,
                                         int32 routing_id) {
  DCHECK(routing_id != MSG_ROUTING_NONE);
  CreateParams params(render_view, routing_id);

  if (g_create_render_frame_impl)
    return g_create_render_frame_impl(params);
  else
    return new RenderFrameImpl(params);
}

// static
RenderFrame* RenderFrame::FromRoutingID(int routing_id) {
  return RenderFrameImpl::FromRoutingID(routing_id);
}

// static
RenderFrameImpl* RenderFrameImpl::FromRoutingID(int routing_id) {
  RoutingIDFrameMap::iterator iter =
      g_routing_id_frame_map.Get().find(routing_id);
  if (iter != g_routing_id_frame_map.Get().end())
    return iter->second;
  return NULL;
}

// static
RenderFrameImpl* RenderFrameImpl::CreateMainFrame(
    RenderViewImpl* render_view,
    int32_t routing_id,
    int32_t widget_routing_id,
    bool hidden,
    const blink::WebScreenInfo& screen_info,
    CompositorDependencies* compositor_deps) {
  // A main frame RenderFrame must have a RenderWidget.
  DCHECK_NE(MSG_ROUTING_NONE, widget_routing_id);

  RenderFrameImpl* render_frame =
      RenderFrameImpl::Create(render_view, routing_id);
  WebLocalFrame* web_frame =
      WebLocalFrame::create(blink::WebTreeScopeType::Document, render_frame);
  render_frame->SetWebFrame(web_frame);
  render_view->webview()->setMainFrame(web_frame);
  render_frame->render_widget_ = RenderWidget::CreateForFrame(
      widget_routing_id, hidden, screen_info, compositor_deps, web_frame);
  // TODO(kenrb): Observing shouldn't be necessary when we sort out
  // WasShown and WasHidden, separating page-level visibility from
  // frame-level visibility.
  // TODO(avi): This DCHECK is to track cleanup for https://crbug.com/545684
  DCHECK_EQ(render_view, render_frame->render_widget_)
      << "Main frame is no longer reusing the RenderView as its widget! "
      << "Does the RenderFrame need to register itself with the RenderWidget?";
  return render_frame;
}

// static
void RenderFrameImpl::CreateFrame(
    int routing_id,
    int proxy_routing_id,
    int opener_routing_id,
    int parent_routing_id,
    int previous_sibling_routing_id,
    const FrameReplicationState& replicated_state,
    CompositorDependencies* compositor_deps,
    const FrameMsg_NewFrame_WidgetParams& widget_params) {
  blink::WebLocalFrame* web_frame;
  RenderFrameImpl* render_frame;
  if (proxy_routing_id == MSG_ROUTING_NONE) {
    RenderFrameProxy* parent_proxy =
        RenderFrameProxy::FromRoutingID(parent_routing_id);
    // If the browser is sending a valid parent routing id, it should already
    // be created and registered.
    CHECK(parent_proxy);
    blink::WebRemoteFrame* parent_web_frame = parent_proxy->web_frame();

    blink::WebFrame* previous_sibling_web_frame = nullptr;
    RenderFrameProxy* previous_sibling_proxy =
        RenderFrameProxy::FromRoutingID(previous_sibling_routing_id);
    if (previous_sibling_proxy)
      previous_sibling_web_frame = previous_sibling_proxy->web_frame();

    // Create the RenderFrame and WebLocalFrame, linking the two.
    render_frame =
        RenderFrameImpl::Create(parent_proxy->render_view(), routing_id);
    web_frame = parent_web_frame->createLocalChild(
        replicated_state.scope, WebString::fromUTF8(replicated_state.name),
        replicated_state.sandbox_flags, render_frame,
        previous_sibling_web_frame);
  } else {
    RenderFrameProxy* proxy =
        RenderFrameProxy::FromRoutingID(proxy_routing_id);
    CHECK(proxy);
    render_frame = RenderFrameImpl::Create(proxy->render_view(), routing_id);
    web_frame =
        blink::WebLocalFrame::create(replicated_state.scope, render_frame);
    render_frame->proxy_routing_id_ = proxy_routing_id;
    web_frame->initializeToReplaceRemoteFrame(
        proxy->web_frame(), WebString::fromUTF8(replicated_state.name),
        replicated_state.sandbox_flags);
  }
  render_frame->SetWebFrame(web_frame);
  CHECK_IMPLIES(parent_routing_id == MSG_ROUTING_NONE, !web_frame->parent());

  WebFrame* opener = ResolveOpener(opener_routing_id, nullptr);
  web_frame->setOpener(opener);

  if (widget_params.routing_id != MSG_ROUTING_NONE) {
    CHECK_IMPLIES(web_frame->parent(),
                  SiteIsolationPolicy::AreCrossProcessFramesPossible());
    render_frame->render_widget_ = RenderWidget::CreateForFrame(
        widget_params.routing_id, widget_params.hidden,
        render_frame->render_view_->screen_info(), compositor_deps, web_frame);
    // TODO(avi): The main frame re-uses the RenderViewImpl as its widget, so
    // avoid double-registering the frame as an observer.
    // https://crbug.com/545684
    if (web_frame->parent()) {
      // TODO(kenrb): Observing shouldn't be necessary when we sort out
      // WasShown and WasHidden, separating page-level visibility from
      // frame-level visibility.
      render_frame->render_widget_->RegisterRenderFrame(render_frame);
    }
  }

  render_frame->Initialize();
}

// static
RenderFrame* RenderFrame::FromWebFrame(blink::WebFrame* web_frame) {
  return RenderFrameImpl::FromWebFrame(web_frame);
}

// static
RenderFrameImpl* RenderFrameImpl::FromWebFrame(blink::WebFrame* web_frame) {
  FrameMap::iterator iter = g_frame_map.Get().find(web_frame);
  if (iter != g_frame_map.Get().end())
    return iter->second;
  return NULL;
}

// static
void RenderFrameImpl::InstallCreateHook(
    CreateRenderFrameImplFunction create_render_frame_impl) {
  CHECK(!g_create_render_frame_impl);
  g_create_render_frame_impl = create_render_frame_impl;
}

// static
blink::WebFrame* RenderFrameImpl::ResolveOpener(int opener_frame_routing_id,
                                                int* opener_view_routing_id) {
  if (opener_view_routing_id)
    *opener_view_routing_id = MSG_ROUTING_NONE;

  if (opener_frame_routing_id == MSG_ROUTING_NONE)
    return nullptr;

  // Opener routing ID could refer to either a RenderFrameProxy or a
  // RenderFrame, so need to check both.
  RenderFrameProxy* opener_proxy =
      RenderFrameProxy::FromRoutingID(opener_frame_routing_id);
  if (opener_proxy) {
    if (opener_view_routing_id)
      *opener_view_routing_id = opener_proxy->render_view()->GetRoutingID();

    // TODO(nasko,alexmos): This check won't be needed once swappedout:// is
    // gone.
    if (opener_proxy->IsMainFrameDetachedFromTree()) {
      DCHECK(!SiteIsolationPolicy::IsSwappedOutStateForbidden());
      return opener_proxy->render_view()->webview()->mainFrame();
    } else {
      return opener_proxy->web_frame();
    }
  }

  RenderFrameImpl* opener_frame =
      RenderFrameImpl::FromRoutingID(opener_frame_routing_id);
  if (opener_frame) {
    if (opener_view_routing_id)
      *opener_view_routing_id = opener_frame->render_view()->GetRoutingID();
    return opener_frame->GetWebFrame();
  }

  return nullptr;
}

// RenderFrameImpl ----------------------------------------------------------
RenderFrameImpl::RenderFrameImpl(const CreateParams& params)
    : frame_(NULL),
      is_main_frame_(true),
      is_local_root_(false),
      render_view_(params.render_view->AsWeakPtr()),
      routing_id_(params.routing_id),
      is_swapped_out_(false),
      render_frame_proxy_(NULL),
      is_detaching_(false),
      proxy_routing_id_(MSG_ROUTING_NONE),
#if defined(ENABLE_PLUGINS)
      plugin_power_saver_helper_(nullptr),
#endif
      cookie_jar_(this),
      selection_text_offset_(0),
      selection_range_(gfx::Range::InvalidRange()),
      handling_select_range_(false),
      notification_permission_dispatcher_(NULL),
      web_user_media_client_(NULL),
      media_permission_dispatcher_(NULL),
      midi_dispatcher_(NULL),
#if defined(OS_ANDROID)
      media_player_manager_(NULL),
#endif
#if defined(ENABLE_BROWSER_CDMS)
      cdm_manager_(NULL),
#endif
#if defined(VIDEO_HOLE)
      contains_media_player_(false),
#endif
      has_played_media_(false),
      devtools_agent_(nullptr),
      geolocation_dispatcher_(NULL),
      push_messaging_dispatcher_(NULL),
      presentation_dispatcher_(NULL),
      screen_orientation_dispatcher_(NULL),
      manifest_manager_(NULL),
      accessibility_mode_(AccessibilityModeOff),
      renderer_accessibility_(NULL),
      is_using_lofi_(false),
      weak_factory_(this) {
  std::pair<RoutingIDFrameMap::iterator, bool> result =
      g_routing_id_frame_map.Get().insert(std::make_pair(routing_id_, this));
  CHECK(result.second) << "Inserting a duplicate item.";

  RenderThread::Get()->AddRoute(routing_id_, this);

  render_view_->RegisterRenderFrame(this);

  // Everything below subclasses RenderFrameObserver and is automatically
  // deleted when the RenderFrame gets deleted.
#if defined(OS_ANDROID)
  new GinJavaBridgeDispatcher(this);
#endif

#if defined(ENABLE_PLUGINS)
  // Manages its own lifetime.
  plugin_power_saver_helper_ = new PluginPowerSaverHelper(this);
#endif

  manifest_manager_ = new ManifestManager(this);

  GetServiceRegistry()->ConnectToRemoteService(mojo::GetProxy(&mojo_shell_));
}

RenderFrameImpl::~RenderFrameImpl() {
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, RenderFrameGone());
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, OnDestruct());

  base::trace_event::TraceLog::GetInstance()->RemoveProcessLabel(routing_id_);

#if defined(VIDEO_HOLE)
  if (contains_media_player_)
    render_view_->UnregisterVideoHoleFrame(this);
#endif

  if (is_main_frame_) {
    // When using swapped out frames, RenderFrameProxy is owned by
    // RenderFrameImpl in the case it is the main frame. Ensure it is deleted
    // along with this object.
    if (render_frame_proxy_ &&
        !SiteIsolationPolicy::IsSwappedOutStateForbidden()) {
      // The following method calls back into this object and clears
      // |render_frame_proxy_|.
      render_frame_proxy_->frameDetached(
          blink::WebRemoteFrameClient::DetachType::Remove);
    }

    // Ensure the RenderView doesn't point to this object, once it is destroyed.
    // TODO(nasko): Add a check that the |main_render_frame_| of |render_view_|
    // is |this|, once the object is no longer leaked.
    // See https://crbug.com/464764.
    render_view_->main_render_frame_ = nullptr;
  }

  render_view_->UnregisterRenderFrame(this);
  g_routing_id_frame_map.Get().erase(routing_id_);
  RenderThread::Get()->RemoveRoute(routing_id_);
}

void RenderFrameImpl::SetWebFrame(blink::WebLocalFrame* web_frame) {
  DCHECK(!frame_);

  std::pair<FrameMap::iterator, bool> result = g_frame_map.Get().insert(
      std::make_pair(web_frame, this));
  CHECK(result.second) << "Inserting a duplicate item.";

  frame_ = web_frame;
}

void RenderFrameImpl::Initialize() {
  is_main_frame_ = !frame_->parent();
  is_local_root_ = is_main_frame_ || frame_->parent()->isWebRemoteFrame();

  RenderFrameImpl* parent_frame = RenderFrameImpl::FromWebFrame(
      frame_->parent());
  if (parent_frame)
    is_using_lofi_ = parent_frame->IsUsingLoFi();

  bool is_tracing = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("navigation", &is_tracing);
  if (is_tracing) {
    int parent_id = GetRoutingIdForFrameOrProxy(frame_->parent());
    TRACE_EVENT2("navigation", "RenderFrameImpl::Initialize",
                 "id", routing_id_,
                 "parent", parent_id);
  }

#if defined(ENABLE_PLUGINS)
  new PepperBrowserConnection(this);
#endif
  new SharedWorkerRepository(this);

  if (is_local_root_ && !render_frame_proxy_) {
    // DevToolsAgent is a RenderFrameObserver, and will destruct itself
    // when |this| is deleted.
    devtools_agent_ = new DevToolsAgent(this);
  }

  RegisterMojoServices();

  // We delay calling this until we have the WebFrame so that any observer or
  // embedder can call GetWebFrame on any RenderFrame.
  GetContentClient()->renderer()->RenderFrameCreated(this);
}

RenderWidget* RenderFrameImpl::GetRenderWidget() {
  return render_view_.get();
}

#if defined(ENABLE_PLUGINS)
void RenderFrameImpl::PepperPluginCreated(RendererPpapiHost* host) {
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_,
                    DidCreatePepperPlugin(host));
  if (host->GetPluginName() == kFlashPluginName) {
    RenderThread::Get()->RecordAction(
        base::UserMetricsAction("FrameLoadWithFlash"));
  }
}

void RenderFrameImpl::PepperDidChangeCursor(
    PepperPluginInstanceImpl* instance,
    const blink::WebCursorInfo& cursor) {
  // Update the cursor appearance immediately if the requesting plugin is the
  // one which receives the last mouse event. Otherwise, the new cursor won't be
  // picked up until the plugin gets the next input event. That is bad if, e.g.,
  // the plugin would like to set an invisible cursor when there isn't any user
  // input for a while.
  if (instance == render_view_->pepper_last_mouse_event_target())
    GetRenderWidget()->didChangeCursor(cursor);
}

void RenderFrameImpl::PepperDidReceiveMouseEvent(
    PepperPluginInstanceImpl* instance) {
  render_view_->set_pepper_last_mouse_event_target(instance);
}

void RenderFrameImpl::PepperTextInputTypeChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != render_view_->focused_pepper_plugin())
    return;

  GetRenderWidget()->UpdateTextInputState(
      RenderWidget::NO_SHOW_IME, RenderWidget::FROM_NON_IME);

  FocusedNodeChangedForAccessibility(WebNode());
}

void RenderFrameImpl::PepperCaretPositionChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != render_view_->focused_pepper_plugin())
    return;
  GetRenderWidget()->UpdateSelectionBounds();
}

void RenderFrameImpl::PepperCancelComposition(
    PepperPluginInstanceImpl* instance) {
  if (instance != render_view_->focused_pepper_plugin())
    return;
  Send(new InputHostMsg_ImeCancelComposition(render_view_->GetRoutingID()));;
#if defined(OS_MACOSX) || defined(USE_AURA)
  GetRenderWidget()->UpdateCompositionInfo(true);
#endif
}

void RenderFrameImpl::PepperSelectionChanged(
    PepperPluginInstanceImpl* instance) {
  if (instance != render_view_->focused_pepper_plugin())
    return;
  SyncSelectionIfRequired();
}

RenderWidgetFullscreenPepper* RenderFrameImpl::CreatePepperFullscreenContainer(
    PepperPluginInstanceImpl* plugin) {
  GURL active_url;
  if (render_view_->webview() && render_view_->webview()->mainFrame())
    active_url = GURL(render_view_->webview()->mainFrame()->document().url());
  RenderWidgetFullscreenPepper* widget = RenderWidgetFullscreenPepper::Create(
      GetRenderWidget()->routing_id(), GetRenderWidget()->compositor_deps(),
      plugin, active_url, GetRenderWidget()->screenInfo());
  widget->show(blink::WebNavigationPolicyIgnore);
  return widget;
}

bool RenderFrameImpl::IsPepperAcceptingCompositionEvents() const {
  if (!render_view_->focused_pepper_plugin())
    return false;
  return render_view_->focused_pepper_plugin()->
      IsPluginAcceptingCompositionEvents();
}

void RenderFrameImpl::PluginCrashed(const base::FilePath& plugin_path,
                                   base::ProcessId plugin_pid) {
  // TODO(jam): dispatch this IPC in RenderFrameHost and switch to use
  // routing_id_ as a result.
  Send(new FrameHostMsg_PluginCrashed(routing_id_, plugin_path, plugin_pid));
}

void RenderFrameImpl::SimulateImeSetComposition(
    const base::string16& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  render_view_->OnImeSetComposition(
      text, underlines, selection_start, selection_end);
}

void RenderFrameImpl::SimulateImeConfirmComposition(
    const base::string16& text,
    const gfx::Range& replacement_range) {
  render_view_->OnImeConfirmComposition(text, replacement_range, false);
}

void RenderFrameImpl::OnImeSetComposition(
    const base::string16& text,
    const std::vector<blink::WebCompositionUnderline>& underlines,
    int selection_start,
    int selection_end) {
  // When a PPAPI plugin has focus, we bypass WebKit.
  if (!IsPepperAcceptingCompositionEvents()) {
    pepper_composition_text_ = text;
  } else {
    // TODO(kinaba) currently all composition events are sent directly to
    // plugins. Use DOM event mechanism after WebKit is made aware about
    // plugins that support composition.
    // The code below mimics the behavior of WebCore::Editor::setComposition.

    // Empty -> nonempty: composition started.
    if (pepper_composition_text_.empty() && !text.empty()) {
      render_view_->focused_pepper_plugin()->HandleCompositionStart(
          base::string16());
    }
    // Nonempty -> empty: composition canceled.
    if (!pepper_composition_text_.empty() && text.empty()) {
      render_view_->focused_pepper_plugin()->HandleCompositionEnd(
          base::string16());
    }
    pepper_composition_text_ = text;
    // Nonempty: composition is ongoing.
    if (!pepper_composition_text_.empty()) {
      render_view_->focused_pepper_plugin()->HandleCompositionUpdate(
          pepper_composition_text_, underlines, selection_start,
          selection_end);
    }
  }
}

void RenderFrameImpl::OnImeConfirmComposition(
    const base::string16& text,
    const gfx::Range& replacement_range,
    bool keep_selection) {
  // When a PPAPI plugin has focus, we bypass WebKit.
  // Here, text.empty() has a special meaning. It means to commit the last
  // update of composition text (see
  // RenderWidgetHost::ImeConfirmComposition()).
  const base::string16& last_text = text.empty() ? pepper_composition_text_
                                                 : text;

  // last_text is empty only when both text and pepper_composition_text_ is.
  // Ignore it.
  if (last_text.empty())
    return;

  if (!IsPepperAcceptingCompositionEvents()) {
    base::i18n::UTF16CharIterator iterator(&last_text);
    int32 i = 0;
    while (iterator.Advance()) {
      blink::WebKeyboardEvent char_event;
      char_event.type = blink::WebInputEvent::Char;
      char_event.timeStampSeconds = base::Time::Now().ToDoubleT();
      char_event.modifiers = 0;
      char_event.windowsKeyCode = last_text[i];
      char_event.nativeKeyCode = last_text[i];

      const int32 char_start = i;
      for (; i < iterator.array_pos(); ++i) {
        char_event.text[i - char_start] = last_text[i];
        char_event.unmodifiedText[i - char_start] = last_text[i];
      }

      if (GetRenderWidget()->webwidget())
        GetRenderWidget()->webwidget()->handleInputEvent(char_event);
    }
  } else {
    // Mimics the order of events sent by WebKit.
    // See WebCore::Editor::setComposition() for the corresponding code.
    render_view_->focused_pepper_plugin()->HandleCompositionEnd(last_text);
    render_view_->focused_pepper_plugin()->HandleTextInput(last_text);
  }
  pepper_composition_text_.clear();
}
#endif  // defined(ENABLE_PLUGINS)

MediaStreamDispatcher* RenderFrameImpl::GetMediaStreamDispatcher() {
  if (!web_user_media_client_)
    InitializeUserMediaClient();
  return web_user_media_client_ ?
      web_user_media_client_->media_stream_dispatcher() : NULL;
}

bool RenderFrameImpl::Send(IPC::Message* message) {
  if (is_detaching_) {
    delete message;
    return false;
  }
  if (is_swapped_out_) {
    if (!SwappedOutMessages::CanSendWhileSwappedOut(message)) {
      delete message;
      return false;
    }
  }

  return RenderThread::Get()->Send(message);
}

#if defined(OS_MACOSX) || defined(OS_ANDROID)
void RenderFrameImpl::DidHideExternalPopupMenu() {
  // We need to clear external_popup_menu_ as soon as ExternalPopupMenu::close
  // is called. Otherwise, createExternalPopupMenu() for new popup will fail.
  external_popup_menu_.reset();
}
#endif

bool RenderFrameImpl::OnMessageReceived(const IPC::Message& msg) {
  // We may get here while detaching, when the WebFrame has been deleted.  Do
  // not process any messages in this state.
  if (!frame_)
    return false;

  // TODO(kenrb): document() should not be null, but as a transitional step
  // we have RenderFrameProxy 'wrapping' a RenderFrameImpl, passing messages
  // to this method. This happens for a top-level remote frame, where a
  // document-less RenderFrame is replaced by a RenderFrameProxy but kept
  // around and is still able to receive messages.
  if (!frame_->document().isNull())
    GetContentClient()->SetActiveURL(frame_->document().url());

  base::ObserverListBase<RenderFrameObserver>::Iterator it(&observers_);
  RenderFrameObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->OnMessageReceived(msg))
      return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameImpl, msg)
    IPC_MESSAGE_HANDLER(FrameMsg_Navigate, OnNavigate)
    IPC_MESSAGE_HANDLER(FrameMsg_BeforeUnload, OnBeforeUnload)
    IPC_MESSAGE_HANDLER(FrameMsg_SwapOut, OnSwapOut)
    IPC_MESSAGE_HANDLER(FrameMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(FrameMsg_ContextMenuClosed, OnContextMenuClosed)
    IPC_MESSAGE_HANDLER(FrameMsg_CustomContextMenuAction,
                        OnCustomContextMenuAction)
    IPC_MESSAGE_HANDLER(InputMsg_Undo, OnUndo)
    IPC_MESSAGE_HANDLER(InputMsg_Redo, OnRedo)
    IPC_MESSAGE_HANDLER(InputMsg_Cut, OnCut)
    IPC_MESSAGE_HANDLER(InputMsg_Copy, OnCopy)
    IPC_MESSAGE_HANDLER(InputMsg_Paste, OnPaste)
    IPC_MESSAGE_HANDLER(InputMsg_PasteAndMatchStyle, OnPasteAndMatchStyle)
    IPC_MESSAGE_HANDLER(InputMsg_Delete, OnDelete)
    IPC_MESSAGE_HANDLER(InputMsg_SelectAll, OnSelectAll)
    IPC_MESSAGE_HANDLER(InputMsg_SelectRange, OnSelectRange)
    IPC_MESSAGE_HANDLER(InputMsg_AdjustSelectionByCharacterOffset,
                        OnAdjustSelectionByCharacterOffset)
    IPC_MESSAGE_HANDLER(InputMsg_Unselect, OnUnselect)
    IPC_MESSAGE_HANDLER(InputMsg_MoveRangeSelectionExtent,
                        OnMoveRangeSelectionExtent)
    IPC_MESSAGE_HANDLER(InputMsg_Replace, OnReplace)
    IPC_MESSAGE_HANDLER(InputMsg_ReplaceMisspelling, OnReplaceMisspelling)
    IPC_MESSAGE_HANDLER(InputMsg_ExtendSelectionAndDelete,
                        OnExtendSelectionAndDelete)
    IPC_MESSAGE_HANDLER(InputMsg_SetCompositionFromExistingText,
                        OnSetCompositionFromExistingText)
    IPC_MESSAGE_HANDLER(InputMsg_ExecuteNoValueEditCommand,
                        OnExecuteNoValueEditCommand)
    IPC_MESSAGE_HANDLER(FrameMsg_CSSInsertRequest, OnCSSInsertRequest)
    IPC_MESSAGE_HANDLER(FrameMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(FrameMsg_JavaScriptExecuteRequest,
                        OnJavaScriptExecuteRequest)
    IPC_MESSAGE_HANDLER(FrameMsg_JavaScriptExecuteRequestForTests,
                        OnJavaScriptExecuteRequestForTests)
    IPC_MESSAGE_HANDLER(FrameMsg_JavaScriptExecuteRequestInIsolatedWorld,
                        OnJavaScriptExecuteRequestInIsolatedWorld)
    IPC_MESSAGE_HANDLER(FrameMsg_VisualStateRequest,
                        OnVisualStateRequest)
    IPC_MESSAGE_HANDLER(FrameMsg_SetEditableSelectionOffsets,
                        OnSetEditableSelectionOffsets)
    IPC_MESSAGE_HANDLER(FrameMsg_Reload, OnReload)
    IPC_MESSAGE_HANDLER(FrameMsg_TextSurroundingSelectionRequest,
                        OnTextSurroundingSelectionRequest)
    IPC_MESSAGE_HANDLER(FrameMsg_SetAccessibilityMode,
                        OnSetAccessibilityMode)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_SnapshotTree,
                        OnSnapshotAccessibilityTree)
    IPC_MESSAGE_HANDLER(FrameMsg_UpdateOpener, OnUpdateOpener)
    IPC_MESSAGE_HANDLER(FrameMsg_CommitNavigation, OnCommitNavigation)
    IPC_MESSAGE_HANDLER(FrameMsg_DidUpdateSandboxFlags, OnDidUpdateSandboxFlags)
    IPC_MESSAGE_HANDLER(FrameMsg_ClearFocus, OnClearFocus)
    IPC_MESSAGE_HANDLER(FrameMsg_SetTextTrackSettings,
                        OnTextTrackSettingsChanged)
    IPC_MESSAGE_HANDLER(FrameMsg_PostMessageEvent, OnPostMessageEvent)
    IPC_MESSAGE_HANDLER(FrameMsg_FailedNavigation, OnFailedNavigation)
    IPC_MESSAGE_HANDLER(FrameMsg_GetSavableResourceLinks,
                        OnGetSavableResourceLinks)
    IPC_MESSAGE_HANDLER(FrameMsg_GetSerializedHtmlWithLocalLinks,
                        OnGetSerializedHtmlWithLocalLinks)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(FrameMsg_SelectPopupMenuItems, OnSelectPopupMenuItems)
#elif defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(FrameMsg_SelectPopupMenuItem, OnSelectPopupMenuItem)
    IPC_MESSAGE_HANDLER(InputMsg_CopyToFindPboard, OnCopyToFindPboard)
#endif
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderFrameImpl::OnNavigate(
    const CommonNavigationParams& common_params,
    const StartNavigationParams& start_params,
    const RequestNavigationParams& request_params) {
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  // Can be NULL in tests.
  if (render_thread_impl)
    render_thread_impl->GetRendererScheduler()->OnNavigationStarted();
  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  TRACE_EVENT2("navigation", "RenderFrameImpl::OnNavigate", "id", routing_id_,
               "url", common_params.url.possibly_invalid_spec());
  NavigateInternal(common_params, start_params, request_params,
                   scoped_ptr<StreamOverrideParameters>());
}

void RenderFrameImpl::NavigateToSwappedOutURL() {
  // We use loadRequest instead of loadHTMLString because the former commits
  // synchronously.  Otherwise a new navigation can interrupt the navigation
  // to kSwappedOutURL. If that happens to be to the page we had been
  // showing, then WebKit will never send a commit and we'll be left spinning.
  // Set the is_swapped_out_ bit to true, so IPC filtering is in effect and
  // the navigation to swappedout:// is not announced to the browser side.
  is_swapped_out_ = true;
  GURL swappedOutURL(kSwappedOutURL);
  WebURLRequest request(swappedOutURL);
  frame_->loadRequest(request);
}

void RenderFrameImpl::BindServiceRegistry(
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  service_registry_.Bind(services.Pass());
  service_registry_.BindRemoteServiceProvider(exposed_services.Pass());
}

ManifestManager* RenderFrameImpl::manifest_manager() {
  return manifest_manager_;
}

void RenderFrameImpl::SetPendingNavigationParams(
    scoped_ptr<NavigationParams> navigation_params) {
  pending_navigation_params_ = navigation_params.Pass();
}

void RenderFrameImpl::OnBeforeUnload() {
  TRACE_EVENT1("navigation", "RenderFrameImpl::OnBeforeUnload",
               "id", routing_id_);
  // TODO(creis): Right now, this is only called on the main frame.  Make the
  // browser process send dispatchBeforeUnloadEvent to every frame that needs
  // it.
  CHECK(!frame_->parent());

  base::TimeTicks before_unload_start_time = base::TimeTicks::Now();
  bool proceed = frame_->dispatchBeforeUnloadEvent();
  base::TimeTicks before_unload_end_time = base::TimeTicks::Now();
  Send(new FrameHostMsg_BeforeUnload_ACK(routing_id_, proceed,
                                         before_unload_start_time,
                                         before_unload_end_time));
}

void RenderFrameImpl::OnSwapOut(
    int proxy_routing_id,
    bool is_loading,
    const FrameReplicationState& replicated_frame_state) {
  TRACE_EVENT1("navigation", "RenderFrameImpl::OnSwapOut", "id", routing_id_);
  RenderFrameProxy* proxy = NULL;
  bool swapped_out_forbidden =
      SiteIsolationPolicy::IsSwappedOutStateForbidden();

  // This codepath should only be hit for subframes when in --site-per-process.
  CHECK_IMPLIES(!is_main_frame_,
                SiteIsolationPolicy::AreCrossProcessFramesPossible());

  // Only run unload if we're not swapped out yet, but send the ack either way.
  if (!is_swapped_out_) {
    // Swap this RenderFrame out so the frame can navigate to a page rendered by
    // a different process.  This involves running the unload handler and
    // clearing the page.  We also allow this process to exit if there are no
    // other active RenderFrames in it.

    // Send an UpdateState message before we get swapped out.
    render_view_->SendUpdateState();

    // If we need a proxy to replace this, create it now so its routing id is
    // registered for receiving IPC messages.
    if (proxy_routing_id != MSG_ROUTING_NONE) {
      proxy = RenderFrameProxy::CreateProxyToReplaceFrame(
          this, proxy_routing_id, replicated_frame_state.scope);
    }

    // Synchronously run the unload handler before sending the ACK.
    // TODO(creis): Call dispatchUnloadEvent unconditionally here to support
    // unload on subframes as well.
    if (is_main_frame_)
      frame_->dispatchUnloadEvent();

    // Swap out and stop sending any IPC messages that are not ACKs.
    if (is_main_frame_)
      render_view_->SetSwappedOut(true);
    is_swapped_out_ = true;

    // Set the proxy here, since OnStop() below could cause an onload event
    // handler to execute, which could trigger code such as
    // willCheckAndDispatchMessageEvent() that needs the proxy.
    if (proxy)
      set_render_frame_proxy(proxy);

    // Now that we're swapped out and filtering IPC messages, stop loading to
    // ensure that no other in-progress navigation continues.  We do this here
    // to avoid sending a DidStopLoading message to the browser process.
    // TODO(creis): Should we be stopping all frames here and using
    // StopAltErrorPageFetcher with RenderView::OnStop, or just stopping this
    // frame?
    if (!swapped_out_forbidden)
      OnStop();

    // Transfer settings such as initial drawing parameters to the remote frame,
    // if one is created, that will replace this frame.
    if (!is_main_frame_ && proxy)
      proxy->web_frame()->initializeFromFrame(frame_);

    // Replace the page with a blank dummy URL. The unload handler will not be
    // run a second time, thanks to a check in FrameLoader::stopLoading.
    // TODO(creis): Need to add a better way to do this that avoids running the
    // beforeunload handler. For now, we just run it a second time silently.
    if (!swapped_out_forbidden)
      NavigateToSwappedOutURL();

    // Let WebKit know that this view is hidden so it can drop resources and
    // stop compositing.
    // TODO(creis): Support this for subframes as well.
    if (is_main_frame_) {
      render_view_->webview()->setVisibilityState(
          blink::WebPageVisibilityStateHidden, false);
    }
  }

  // It is now safe to show modal dialogs again.
  // TODO(creis): Deal with modal dialogs from subframes.
  if (is_main_frame_)
    render_view_->suppress_dialogs_until_swap_out_ = false;

  Send(new FrameHostMsg_SwapOut_ACK(routing_id_));

  RenderViewImpl* render_view = render_view_.get();
  bool is_main_frame = is_main_frame_;

  // Now that all of the cleanup is complete and the browser side is notified,
  // start using the RenderFrameProxy, if one is created.
  if (proxy && swapped_out_forbidden) {
    frame_->swap(proxy->web_frame());

    if (is_loading)
      proxy->OnDidStartLoading();
  }

  // In --site-per-process, initialize the WebRemoteFrame with the replication
  // state passed by the process that is now rendering the frame.
  // TODO(alexmos): We cannot yet do this for swapped-out main frames, because
  // in that case we leave the LocalFrame as the main frame visible to Blink
  // and don't call swap() above. Because swap() is what creates a RemoteFrame
  // in proxy->web_frame(), the RemoteFrame will not exist for main frames.
  // When we do an unconditional swap for all frames, we can remove
  // !is_main_frame below.
  if (proxy && swapped_out_forbidden)
    proxy->SetReplicatedState(replicated_frame_state);

  // Safe to exit if no one else is using the process.
  // TODO(nasko): Remove the dependency on RenderViewImpl here and ref count
  // the process based on the lifetime of this RenderFrameImpl object.
  if (is_main_frame) {
    render_view->WasSwappedOut();
  }
}

void RenderFrameImpl::OnContextMenuClosed(
    const CustomContextMenuContext& custom_context) {
  if (custom_context.request_id) {
    // External request, should be in our map.
    ContextMenuClient* client =
        pending_context_menus_.Lookup(custom_context.request_id);
    if (client) {
      client->OnMenuClosed(custom_context.request_id);
      pending_context_menus_.Remove(custom_context.request_id);
    }
  } else {
    if (custom_context.link_followed.is_valid())
        frame_->sendPings(context_menu_node_, custom_context.link_followed);
    // Internal request, forward to WebKit.
    context_menu_node_.reset();
  }
}

void RenderFrameImpl::OnCustomContextMenuAction(
    const CustomContextMenuContext& custom_context,
    unsigned action) {
  if (custom_context.request_id) {
    // External context menu request, look in our map.
    ContextMenuClient* client =
        pending_context_menus_.Lookup(custom_context.request_id);
    if (client)
      client->OnMenuAction(custom_context.request_id, action);
  } else {
    // Internal request, forward to WebKit.
    render_view_->webview()->performCustomContextMenuAction(action);
  }
}

void RenderFrameImpl::OnUndo() {
  frame_->executeCommand(WebString::fromUTF8("Undo"), GetFocusedElement());
}

void RenderFrameImpl::OnRedo() {
  frame_->executeCommand(WebString::fromUTF8("Redo"), GetFocusedElement());
}

void RenderFrameImpl::OnCut() {
  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  frame_->executeCommand(WebString::fromUTF8("Cut"), GetFocusedElement());
}

void RenderFrameImpl::OnCopy() {
  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  WebNode current_node = context_menu_node_.isNull() ?
      GetFocusedElement() : context_menu_node_;
  frame_->executeCommand(WebString::fromUTF8("Copy"), current_node);
}

void RenderFrameImpl::OnPaste() {
  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  frame_->executeCommand(WebString::fromUTF8("Paste"), GetFocusedElement());
}

void RenderFrameImpl::OnPasteAndMatchStyle() {
  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  frame_->executeCommand(
      WebString::fromUTF8("PasteAndMatchStyle"), GetFocusedElement());
}

#if defined(OS_MACOSX)
void RenderFrameImpl::OnCopyToFindPboard() {
  // Since the find pasteboard supports only plain text, this can be simpler
  // than the |OnCopy()| case.
  if (frame_->hasSelection()) {
    base::string16 selection = frame_->selectionAsText();
    RenderThread::Get()->Send(
        new ClipboardHostMsg_FindPboardWriteStringAsync(selection));
  }
}
#endif

void RenderFrameImpl::OnDelete() {
  frame_->executeCommand(WebString::fromUTF8("Delete"), GetFocusedElement());
}

void RenderFrameImpl::OnSelectAll() {
  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  frame_->executeCommand(WebString::fromUTF8("SelectAll"), GetFocusedElement());
}

void RenderFrameImpl::OnSelectRange(const gfx::Point& base,
                                    const gfx::Point& extent) {
  // This IPC is dispatched by RenderWidgetHost, so use its routing id.
  Send(new InputHostMsg_SelectRange_ACK(GetRenderWidget()->routing_id()));

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  frame_->selectRange(base, extent);
}

void RenderFrameImpl::OnAdjustSelectionByCharacterOffset(int start_adjust,
                                                         int end_adjust) {
  size_t start, length;
  if (!GetRenderWidget()->webwidget()->caretOrSelectionRange(
      &start, &length)) {
    return;
  }

  // Sanity checks to disallow empty and out of range selections.
  if (start_adjust - end_adjust > static_cast<int>(length)
      || static_cast<int>(start) + start_adjust < 0) {
    return;
  }

  // A negative adjust amount moves the selection towards the beginning of
  // the document, a positive amount moves the selection towards the end of
  // the document.
  start += start_adjust;
  length += end_adjust - start_adjust;

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  frame_->selectRange(WebRange::fromDocumentRange(frame_, start, length));
}

void RenderFrameImpl::OnUnselect() {
  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  frame_->executeCommand(WebString::fromUTF8("Unselect"), GetFocusedElement());
}

void RenderFrameImpl::OnMoveRangeSelectionExtent(const gfx::Point& point) {
  // This IPC is dispatched by RenderWidgetHost, so use its routing id.
  Send(new InputHostMsg_MoveRangeSelectionExtent_ACK(
      GetRenderWidget()->routing_id()));

  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  frame_->moveRangeSelectionExtent(point);
}

void RenderFrameImpl::OnReplace(const base::string16& text) {
  if (!frame_->hasSelection())
    frame_->selectWordAroundCaret();

  frame_->replaceSelection(text);
}

void RenderFrameImpl::OnReplaceMisspelling(const base::string16& text) {
  if (!frame_->hasSelection())
    return;

  frame_->replaceMisspelledRange(text);
}

void RenderFrameImpl::OnCSSInsertRequest(const std::string& css) {
  frame_->document().insertStyleSheet(WebString::fromUTF8(css));
}

void RenderFrameImpl::OnAddMessageToConsole(ConsoleMessageLevel level,
                                            const std::string& message) {
  AddMessageToConsole(level, message);
}

void RenderFrameImpl::OnJavaScriptExecuteRequest(
    const base::string16& jscript,
    int id,
    bool notify_result) {
  TRACE_EVENT_INSTANT0("test_tracing", "OnJavaScriptExecuteRequest",
                       TRACE_EVENT_SCOPE_THREAD);

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> result =
      frame_->executeScriptAndReturnValue(WebScriptSource(jscript));

  HandleJavascriptExecutionResult(jscript, id, notify_result, result);
}

void RenderFrameImpl::OnJavaScriptExecuteRequestForTests(
    const base::string16& jscript,
    int id,
    bool notify_result,
    bool has_user_gesture) {
  TRACE_EVENT_INSTANT0("test_tracing", "OnJavaScriptExecuteRequestForTests",
                       TRACE_EVENT_SCOPE_THREAD);

  // A bunch of tests expect to run code in the context of a user gesture, which
  // can grant additional privileges (e.g. the ability to create popups).
  scoped_ptr<blink::WebScopedUserGesture> gesture(
      has_user_gesture ? new blink::WebScopedUserGesture : nullptr);
  v8::HandleScope handle_scope(blink::mainThreadIsolate());
  v8::Local<v8::Value> result =
      frame_->executeScriptAndReturnValue(WebScriptSource(jscript));

  HandleJavascriptExecutionResult(jscript, id, notify_result, result);
}

void RenderFrameImpl::OnJavaScriptExecuteRequestInIsolatedWorld(
    const base::string16& jscript,
    int id,
    bool notify_result,
    int world_id) {
  TRACE_EVENT_INSTANT0("test_tracing",
                       "OnJavaScriptExecuteRequestInIsolatedWorld",
                       TRACE_EVENT_SCOPE_THREAD);

  if (world_id <= ISOLATED_WORLD_ID_GLOBAL ||
      world_id > ISOLATED_WORLD_ID_MAX) {
    // Return if the world_id is not valid. world_id is passed as a plain int
    // over IPC and needs to be verified here, in the IPC endpoint.
    NOTREACHED();
    return;
  }

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebScriptSource script = WebScriptSource(jscript);
  JavaScriptIsolatedWorldRequest* request = new JavaScriptIsolatedWorldRequest(
      id, notify_result, routing_id_, weak_factory_.GetWeakPtr());
  frame_->requestExecuteScriptInIsolatedWorld(world_id, &script, 1, 0, false,
                                              request);
}

RenderFrameImpl::JavaScriptIsolatedWorldRequest::JavaScriptIsolatedWorldRequest(
    int id,
    bool notify_result,
    int routing_id,
    base::WeakPtr<RenderFrameImpl> render_frame_impl)
    : id_(id),
      notify_result_(notify_result),
      routing_id_(routing_id),
      render_frame_impl_(render_frame_impl) {
}

RenderFrameImpl::JavaScriptIsolatedWorldRequest::
    ~JavaScriptIsolatedWorldRequest() {
}

void RenderFrameImpl::JavaScriptIsolatedWorldRequest::completed(
    const blink::WebVector<v8::Local<v8::Value>>& result) {
  if (!render_frame_impl_.get()) {
    return;
  }

  if (notify_result_) {
    base::ListValue list;
    if (!result.isEmpty()) {
      // It's safe to always use the main world context when converting
      // here. V8ValueConverterImpl shouldn't actually care about the
      // context scope, and it switches to v8::Object's creation context
      // when encountered. (from extensions/renderer/script_injection.cc)
      v8::Local<v8::Context> context =
          render_frame_impl_.get()->frame_->mainWorldScriptContext();
      v8::Context::Scope context_scope(context);
      V8ValueConverterImpl converter;
      converter.SetDateAllowed(true);
      converter.SetRegExpAllowed(true);
      for (const auto& value : result) {
        scoped_ptr<base::Value> result_value(
            converter.FromV8Value(value, context));
        list.Append(result_value ? result_value.Pass()
                                 : base::Value::CreateNullValue());
      }
    } else {
      list.Set(0, base::Value::CreateNullValue());
    }
    render_frame_impl_.get()->Send(
        new FrameHostMsg_JavaScriptExecuteResponse(routing_id_, id_, list));
  }

  delete this;
}

void RenderFrameImpl::HandleJavascriptExecutionResult(
    const base::string16& jscript,
    int id,
    bool notify_result,
    v8::Local<v8::Value> result) {
  if (notify_result) {
    base::ListValue list;
    if (!result.IsEmpty()) {
      v8::Local<v8::Context> context = frame_->mainWorldScriptContext();
      v8::Context::Scope context_scope(context);
      V8ValueConverterImpl converter;
      converter.SetDateAllowed(true);
      converter.SetRegExpAllowed(true);
      scoped_ptr<base::Value> result_value(
          converter.FromV8Value(result, context));
      list.Set(0, result_value ? result_value.Pass()
                               : base::Value::CreateNullValue());
    } else {
      list.Set(0, base::Value::CreateNullValue());
    }
    Send(new FrameHostMsg_JavaScriptExecuteResponse(routing_id_, id, list));
  }
}

void RenderFrameImpl::OnVisualStateRequest(uint64 id) {
  GetRenderWidget()->QueueMessage(
      new FrameHostMsg_VisualStateResponse(routing_id_, id),
      MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE);
}

void RenderFrameImpl::OnSetEditableSelectionOffsets(int start, int end) {
  base::AutoReset<bool> handling_select_range(&handling_select_range_, true);
  if (!GetRenderWidget()->ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(GetRenderWidget());
  frame_->setEditableSelectionOffsets(start, end);
}

void RenderFrameImpl::OnSetCompositionFromExistingText(
    int start, int end,
    const std::vector<blink::WebCompositionUnderline>& underlines) {
  if (!GetRenderWidget()->ShouldHandleImeEvent())
    return;
  ImeEventGuard guard(GetRenderWidget());
  frame_->setCompositionFromExistingText(start, end, underlines);
}

void RenderFrameImpl::OnExecuteNoValueEditCommand(const std::string& name) {
  frame_->executeCommand(WebString::fromUTF8(name), GetFocusedElement());
}

void RenderFrameImpl::OnExtendSelectionAndDelete(int before, int after) {
  if (!GetRenderWidget()->ShouldHandleImeEvent())
    return;

  DCHECK(!WebUserGestureIndicator::isProcessingUserGesture());

  ImeEventGuard guard(GetRenderWidget());
  blink::WebScopedUserGesture gesture_indicator;
  frame_->extendSelectionAndDelete(before, after);
}

void RenderFrameImpl::OnSetAccessibilityMode(AccessibilityMode new_mode) {
  if (accessibility_mode_ == new_mode)
    return;
  accessibility_mode_ = new_mode;
  if (renderer_accessibility_) {
    // Note: this isn't called automatically by the destructor because
    // there'd be no point in calling it in frame teardown, only if there's
    // an accessibility mode change but the frame is persisting.
    renderer_accessibility_->DisableAccessibility();

    delete renderer_accessibility_;
    renderer_accessibility_ = NULL;
  }
  if (accessibility_mode_ == AccessibilityModeOff)
    return;

  if (accessibility_mode_ & AccessibilityModeFlagFullTree)
    renderer_accessibility_ = new RendererAccessibility(this);
}

void RenderFrameImpl::OnSnapshotAccessibilityTree(int callback_id) {
  AXContentTreeUpdate response;
  RendererAccessibility::SnapshotAccessibilityTree(this, &response);
  Send(new AccessibilityHostMsg_SnapshotResponse(
      routing_id_, callback_id, response));
}

void RenderFrameImpl::OnUpdateOpener(int opener_routing_id) {
  WebFrame* opener = ResolveOpener(opener_routing_id, nullptr);
  frame_->setOpener(opener);
}

void RenderFrameImpl::OnDidUpdateSandboxFlags(blink::WebSandboxFlags flags) {
  frame_->setFrameOwnerSandboxFlags(flags);
}

void RenderFrameImpl::OnClearFocus() {
  frame_->clearFocus();
}

void RenderFrameImpl::OnTextTrackSettingsChanged(
    const FrameMsg_TextTrackSettings_Params& params) {
  DCHECK(!frame_->parent());
  if (!render_view_->webview())
    return;

  if (params.text_tracks_enabled) {
      render_view_->webview()->settings()->setTextTrackKindUserPreference(
          WebSettings::TextTrackKindUserPreference::Captions);
  } else {
      render_view_->webview()->settings()->setTextTrackKindUserPreference(
          WebSettings::TextTrackKindUserPreference::Default);
  }
  render_view_->webview()->settings()->setTextTrackBackgroundColor(
      WebString::fromUTF8(params.text_track_background_color));
  render_view_->webview()->settings()->setTextTrackFontFamily(
      WebString::fromUTF8(params.text_track_font_family));
  render_view_->webview()->settings()->setTextTrackFontStyle(
      WebString::fromUTF8(params.text_track_font_style));
  render_view_->webview()->settings()->setTextTrackFontVariant(
      WebString::fromUTF8(params.text_track_font_variant));
  render_view_->webview()->settings()->setTextTrackTextColor(
      WebString::fromUTF8(params.text_track_text_color));
  render_view_->webview()->settings()->setTextTrackTextShadow(
      WebString::fromUTF8(params.text_track_text_shadow));
  render_view_->webview()->settings()->setTextTrackTextSize(
      WebString::fromUTF8(params.text_track_text_size));
}

void RenderFrameImpl::OnPostMessageEvent(
    const FrameMsg_PostMessage_Params& params) {
  // Find the source frame if it exists.
  WebFrame* source_frame = NULL;
  if (params.source_routing_id != MSG_ROUTING_NONE) {
    RenderFrameProxy* source_proxy =
        RenderFrameProxy::FromRoutingID(params.source_routing_id);
    if (source_proxy) {
      // Currently, navigating a top-level frame cross-process does not swap
      // the WebLocalFrame for a WebRemoteFrame in the frame tree, and the
      // WebRemoteFrame will not have an associated blink::Frame. If this is
      // the case for |source_proxy|, use the corresponding (swapped-out)
      // WebLocalFrame instead, so that event.source for this message can be
      // set and used properly.
      if (source_proxy->IsMainFrameDetachedFromTree())
        source_frame = source_proxy->render_view()->webview()->mainFrame();
      else
        source_frame = source_proxy->web_frame();
    }
  }

  // If the message contained MessagePorts, create the corresponding endpoints.
  blink::WebMessagePortChannelArray channels =
      WebMessagePortChannelImpl::CreatePorts(
          params.message_ports, params.new_routing_ids,
          base::ThreadTaskRunnerHandle::Get().get());

  WebSerializedScriptValue serialized_script_value;
  if (params.is_data_raw_string) {
    v8::HandleScope handle_scope(blink::mainThreadIsolate());
    v8::Local<v8::Context> context = frame_->mainWorldScriptContext();
    v8::Context::Scope context_scope(context);
    V8ValueConverterImpl converter;
    converter.SetDateAllowed(true);
    converter.SetRegExpAllowed(true);
    scoped_ptr<base::Value> value(new base::StringValue(params.data));
    v8::Local<v8::Value> result_value = converter.ToV8Value(value.get(),
                                                             context);
    serialized_script_value = WebSerializedScriptValue::serialize(result_value);
  } else {
    serialized_script_value = WebSerializedScriptValue::fromString(params.data);
  }

  // We must pass in the target_origin to do the security check on this side,
  // since it may have changed since the original postMessage call was made.
  WebSecurityOrigin target_origin;
  if (!params.target_origin.empty()) {
    target_origin =
        WebSecurityOrigin::createFromString(WebString(params.target_origin));
  }

  WebDOMMessageEvent msg_event(serialized_script_value,
                               params.source_origin,
                               source_frame,
                               frame_->document(),
                               channels);
  frame_->dispatchMessageEventWithOriginCheck(target_origin, msg_event);
}

#if defined(OS_ANDROID)
void RenderFrameImpl::OnSelectPopupMenuItems(
    bool canceled,
    const std::vector<int>& selected_indices) {
  // It is possible to receive more than one of these calls if the user presses
  // a select faster than it takes for the show-select-popup IPC message to make
  // it to the browser UI thread. Ignore the extra-messages.
  // TODO(jcivelli): http:/b/5793321 Implement a better fix, as detailed in bug.
  if (!external_popup_menu_)
    return;

  external_popup_menu_->DidSelectItems(canceled, selected_indices);
  external_popup_menu_.reset();
}
#endif

#if defined(OS_MACOSX)
void RenderFrameImpl::OnSelectPopupMenuItem(int selected_index) {
  if (external_popup_menu_ == NULL)
    return;
  external_popup_menu_->DidSelectItem(selected_index);
  external_popup_menu_.reset();
}
#endif

void RenderFrameImpl::OnReload(bool ignore_cache) {
  frame_->reload(ignore_cache);
}

void RenderFrameImpl::OnTextSurroundingSelectionRequest(size_t max_length) {
  blink::WebSurroundingText surroundingText;
  surroundingText.initialize(frame_->selectionRange(), max_length);

  if (surroundingText.isNull()) {
    // |surroundingText| might not be correctly initialized, for example if
    // |frame_->selectionRange().isNull()|, in other words, if there was no
    // selection.
    Send(new FrameHostMsg_TextSurroundingSelectionResponse(
        routing_id_, base::string16(), 0, 0));
    return;
  }

  Send(new FrameHostMsg_TextSurroundingSelectionResponse(
      routing_id_,
      surroundingText.textContent(),
      surroundingText.startOffsetInTextContent(),
      surroundingText.endOffsetInTextContent()));
}

bool RenderFrameImpl::RunJavaScriptMessage(JavaScriptMessageType type,
                                           const base::string16& message,
                                           const base::string16& default_value,
                                           const GURL& frame_url,
                                           base::string16* result) {
  // Don't allow further dialogs if we are waiting to swap out, since the
  // PageGroupLoadDeferrer in our stack prevents it.
  if (render_view()->suppress_dialogs_until_swap_out_)
    return false;

  bool success = false;
  base::string16 result_temp;
  if (!result)
    result = &result_temp;

  render_view()->SendAndRunNestedMessageLoop(
      new FrameHostMsg_RunJavaScriptMessage(
        routing_id_, message, default_value, frame_url, type, &success,
        result));
  return success;
}

void RenderFrameImpl::LoadNavigationErrorPage(
    const WebURLRequest& failed_request,
    const WebURLError& error,
    bool replace) {
  std::string error_html;
  GetContentClient()->renderer()->GetNavigationErrorStrings(
      render_view(), frame_, failed_request, error, &error_html, NULL);

  frame_->loadHTMLString(error_html,
                         GURL(kUnreachableWebDataURL),
                         error.unreachableURL,
                         replace);
}

void RenderFrameImpl::DidMeaningfulLayout(
    blink::WebMeaningfulLayout layout_type) {
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_,
                    DidMeaningfulLayout(layout_type));
}

void RenderFrameImpl::DidCommitCompositorFrame() {
  if (BrowserPluginManager::Get())
    BrowserPluginManager::Get()->DidCommitCompositorFrame(GetRoutingID());
  FOR_EACH_OBSERVER(
      RenderFrameObserver, observers_, DidCommitCompositorFrame());
}

RenderView* RenderFrameImpl::GetRenderView() {
  return render_view_.get();
}

int RenderFrameImpl::GetRoutingID() {
  return routing_id_;
}

blink::WebLocalFrame* RenderFrameImpl::GetWebFrame() {
  DCHECK(frame_);
  return frame_;
}

WebElement RenderFrameImpl::GetFocusedElement() const {
  WebDocument doc = frame_->document();
  if (!doc.isNull())
    return doc.focusedElement();

  return WebElement();
}

WebPreferences& RenderFrameImpl::GetWebkitPreferences() {
  return render_view_->GetWebkitPreferences();
}

int RenderFrameImpl::ShowContextMenu(ContextMenuClient* client,
                                     const ContextMenuParams& params) {
  DCHECK(client);  // A null client means "internal" when we issue callbacks.
  ContextMenuParams our_params(params);
  our_params.custom_context.request_id = pending_context_menus_.Add(client);
  Send(new FrameHostMsg_ContextMenu(routing_id_, our_params));
  return our_params.custom_context.request_id;
}

void RenderFrameImpl::CancelContextMenu(int request_id) {
  DCHECK(pending_context_menus_.Lookup(request_id));
  pending_context_menus_.Remove(request_id);
}

blink::WebNode RenderFrameImpl::GetContextMenuNode() const {
  return context_menu_node_;
}

blink::WebPlugin* RenderFrameImpl::CreatePlugin(
    blink::WebFrame* frame,
    const WebPluginInfo& info,
    const blink::WebPluginParams& params,
    scoped_ptr<content::PluginInstanceThrottler> throttler) {
  DCHECK_EQ(frame_, frame);
#if defined(ENABLE_PLUGINS)
  if (info.type == WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN) {
    return BrowserPluginManager::Get()->CreateBrowserPlugin(
        this, GetContentClient()
                  ->renderer()
                  ->CreateBrowserPluginDelegate(this, params.mimeType.utf8(),
                                                GURL(params.url))
                  ->GetWeakPtr());
  }

  bool pepper_plugin_was_registered = false;
  scoped_refptr<PluginModule> pepper_module(PluginModule::Create(
      this, info, &pepper_plugin_was_registered));
  if (pepper_plugin_was_registered) {
    if (pepper_module.get()) {
      return new PepperWebPluginImpl(
          pepper_module.get(), params, this,
          make_scoped_ptr(
              static_cast<PluginInstanceThrottlerImpl*>(throttler.release())));
    }
  }
#if defined(OS_CHROMEOS)
  LOG(WARNING) << "Pepper module/plugin creation failed.";
#else
  if (info.type == WebPluginInfo::PLUGIN_TYPE_NPAPI) {
    // TODO(jam): change to take RenderFrame.
    return new WebPluginImpl(frame, params, info.path, render_view_, this);
  }
#endif
#endif
  return NULL;
}

void RenderFrameImpl::LoadURLExternally(const blink::WebURLRequest& request,
                                        blink::WebNavigationPolicy policy) {
  loadURLExternally(request, policy, WebString(), false);
}

void RenderFrameImpl::ExecuteJavaScript(const base::string16& javascript) {
  OnJavaScriptExecuteRequest(javascript, 0, false);
}

ServiceRegistry* RenderFrameImpl::GetServiceRegistry() {
  return &service_registry_;
}

#if defined(ENABLE_PLUGINS)
void RenderFrameImpl::RegisterPeripheralPlugin(
    const url::Origin& content_origin,
    const base::Closure& unthrottle_callback) {
  return plugin_power_saver_helper_->RegisterPeripheralPlugin(
      content_origin, unthrottle_callback);
}

bool RenderFrameImpl::ShouldThrottleContent(
    const url::Origin& main_frame_origin,
    const url::Origin& content_origin,
    const std::string& plugin_module_name,
    int width,
    int height,
    bool* cross_origin_main_content) const {
  return plugin_power_saver_helper_->ShouldThrottleContent(
      main_frame_origin, content_origin, plugin_module_name, width, height,
      cross_origin_main_content);
}

void RenderFrameImpl::WhitelistContentOrigin(
    const url::Origin& content_origin) {
  return plugin_power_saver_helper_->WhitelistContentOrigin(content_origin);
}
#endif  // defined(ENABLE_PLUGINS)

bool RenderFrameImpl::IsFTPDirectoryListing() {
  WebURLResponseExtraDataImpl* extra_data =
      GetExtraDataFromResponse(frame_->dataSource()->response());
  return extra_data ? extra_data->is_ftp_directory_listing() : false;
}

void RenderFrameImpl::AttachGuest(int element_instance_id) {
  BrowserPluginManager::Get()->Attach(element_instance_id);
}

void RenderFrameImpl::DetachGuest(int element_instance_id) {
  BrowserPluginManager::Get()->Detach(element_instance_id);
}

void RenderFrameImpl::SetSelectedText(const base::string16& selection_text,
                                      size_t offset,
                                      const gfx::Range& range) {
  // Use the routing id of Render Widget Host.
  Send(new ViewHostMsg_SelectionChanged(GetRenderWidget()->routing_id(),
                                        selection_text,
                                        offset,
                                        range));
}

void RenderFrameImpl::EnsureMojoBuiltinsAreAvailable(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context) {
  gin::ModuleRegistry* registry = gin::ModuleRegistry::From(context);
  if (registry->available_modules().count(mojo::js::Core::kModuleName))
    return;

  v8::HandleScope handle_scope(isolate);
  registry->AddBuiltinModule(
      isolate, mojo::js::Core::kModuleName, mojo::js::Core::GetModule(isolate));
  registry->AddBuiltinModule(isolate,
                             mojo::js::Support::kModuleName,
                             mojo::js::Support::GetModule(isolate));
  registry->AddBuiltinModule(
      isolate,
      ServiceRegistryJsWrapper::kModuleName,
      ServiceRegistryJsWrapper::Create(isolate, &service_registry_).ToV8());
}

void RenderFrameImpl::AddMessageToConsole(ConsoleMessageLevel level,
                                          const std::string& message) {
  if (devtools_agent_)
    devtools_agent_->AddMessageToConsole(level, message);
}

bool RenderFrameImpl::IsUsingLoFi() const {
  return is_using_lofi_;
}

// blink::WebFrameClient implementation ----------------------------------------

blink::WebPlugin* RenderFrameImpl::createPlugin(
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params) {
  DCHECK_EQ(frame_, frame);
  blink::WebPlugin* plugin = NULL;
  if (GetContentClient()->renderer()->OverrideCreatePlugin(
          this, frame, params, &plugin)) {
    return plugin;
  }

  if (base::UTF16ToUTF8(base::StringPiece16(params.mimeType)) ==
      kBrowserPluginMimeType) {
    return BrowserPluginManager::Get()->CreateBrowserPlugin(
        this, GetContentClient()
                  ->renderer()
                  ->CreateBrowserPluginDelegate(this, kBrowserPluginMimeType,
                                                GURL(params.url))
                  ->GetWeakPtr());
  }

#if defined(ENABLE_PLUGINS)
  WebPluginInfo info;
  std::string mime_type;
  bool found = false;
  WebString top_origin = frame->top()->securityOrigin().toString();
  Send(new FrameHostMsg_GetPluginInfo(routing_id_, params.url, GURL(top_origin),
                                      params.mimeType.utf8(), &found, &info,
                                      &mime_type));
  if (!found)
    return NULL;

  WebPluginParams params_to_use = params;
  params_to_use.mimeType = WebString::fromUTF8(mime_type);
  return CreatePlugin(frame, info, params_to_use, nullptr /* throttler */);
#else
  return NULL;
#endif  // defined(ENABLE_PLUGINS)
}

blink::WebMediaPlayer* RenderFrameImpl::createMediaPlayer(
    blink::WebLocalFrame* frame,
    const blink::WebURL& url,
    WebMediaPlayerClient* client,
    WebMediaPlayerEncryptedMediaClient* encrypted_client,
    WebContentDecryptionModule* initial_cdm) {
#if defined(VIDEO_HOLE)
  if (!contains_media_player_) {
    render_view_->RegisterVideoHoleFrame(this);
    contains_media_player_ = true;
  }
#endif  // defined(VIDEO_HOLE)

  blink::WebMediaStream web_stream(
      blink::WebMediaStreamRegistry::lookupMediaStreamDescriptor(url));
  if (!web_stream.isNull())
    return CreateWebMediaPlayerForMediaStream(client);

  RenderThreadImpl* render_thread = RenderThreadImpl::current();

#if defined(OS_ANDROID) && !defined(ENABLE_MEDIA_PIPELINE_ON_ANDROID)
  scoped_refptr<media::AudioRendererSink> audio_renderer_sink;
  media::WebMediaPlayerParams::Context3DCB context_3d_cb;
#else
  scoped_refptr<media::AudioRendererSink> audio_renderer_sink =
      render_thread->GetAudioRendererMixerManager()->CreateInput(routing_id_);
  media::WebMediaPlayerParams::Context3DCB context_3d_cb =
      base::Bind(&GetSharedMainThreadContext3D);
#endif  // defined(OS_ANDROID) && !defined(ENABLE_MEDIA_PIPELINE_ON_ANDROID)

  scoped_refptr<media::MediaLog> media_log(new RenderMediaLog());
  media::WebMediaPlayerParams params(
      base::Bind(&ContentRendererClient::DeferMediaLoad,
                 base::Unretained(GetContentClient()->renderer()),
                 static_cast<RenderFrame*>(this), has_played_media_),
      audio_renderer_sink, media_log, render_thread->GetMediaThreadTaskRunner(),
      render_thread->GetWorkerTaskRunner(),
      render_thread->compositor_task_runner(), context_3d_cb,
      GetMediaPermission(), initial_cdm);

// TODO(xhwang, watk): Find a better way to specify these ifdef conditions.
#if defined(OS_ANDROID) && !defined(ENABLE_MEDIA_PIPELINE_ON_ANDROID)
  return CreateAndroidWebMediaPlayer(client, encrypted_client, params);
#else
#if defined(ENABLE_MOJO_MEDIA) && !defined(ENABLE_MEDIA_PIPELINE_ON_ANDROID)
  scoped_ptr<media::RendererFactory> media_renderer_factory(
      new media::MojoRendererFactory(GetMediaServiceFactory()));
#else
  scoped_ptr<media::RendererFactory> media_renderer_factory =
      GetContentClient()->renderer()->CreateMediaRendererFactory(
          this, render_thread->GetGpuFactories(), media_log);

  if (!media_renderer_factory.get()) {
    media_renderer_factory.reset(new media::DefaultRendererFactory(
        media_log, render_thread->GetGpuFactories(),
        *render_thread->GetAudioHardwareConfig()));
  }
#endif  // defined(ENABLE_MOJO_MEDIA) &&
  // !defined(ENABLE_MEDIA_PIPELINE_ON_ANDROID)

  return new media::WebMediaPlayerImpl(
      frame, client, encrypted_client, weak_factory_.GetWeakPtr(),
      media_renderer_factory.Pass(), GetCdmFactory(), params);
#endif  // defined(OS_ANDROID) && !defined(ENABLE_MEDIA_PIPELINE_ON_ANDROID)
}

blink::WebMediaSession* RenderFrameImpl::createMediaSession() {
#if defined(OS_ANDROID)
  return new WebMediaSessionAndroid();
#else
  return nullptr;
#endif  // defined(OS_ANDROID)
}

blink::WebApplicationCacheHost* RenderFrameImpl::createApplicationCacheHost(
    blink::WebLocalFrame* frame,
    blink::WebApplicationCacheHostClient* client) {
  if (!frame || !frame->view())
    return NULL;
  DCHECK(!frame_ || frame_ == frame);
  return new RendererWebApplicationCacheHostImpl(
      RenderViewImpl::FromWebView(frame->view()), client,
      RenderThreadImpl::current()->appcache_dispatcher()->backend_proxy());
}

blink::WebWorkerContentSettingsClientProxy*
RenderFrameImpl::createWorkerContentSettingsClientProxy(
    blink::WebLocalFrame* frame) {
  if (!frame || !frame->view())
    return NULL;
  DCHECK(!frame_ || frame_ == frame);
  return GetContentClient()->renderer()->CreateWorkerContentSettingsClientProxy(
      this, frame);
}

WebExternalPopupMenu* RenderFrameImpl::createExternalPopupMenu(
    const WebPopupMenuInfo& popup_menu_info,
    WebExternalPopupMenuClient* popup_menu_client) {
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  // An IPC message is sent to the browser to build and display the actual
  // popup. The user could have time to click a different select by the time
  // the popup is shown. In that case external_popup_menu_ is non NULL.
  // By returning NULL in that case, we instruct Blink to cancel that new
  // popup. So from the user perspective, only the first one will show, and
  // will have to close the first one before another one can be shown.
  if (external_popup_menu_)
    return NULL;
  external_popup_menu_.reset(
      new ExternalPopupMenu(this, popup_menu_info, popup_menu_client));
  if (render_view_->screen_metrics_emulator_) {
    render_view_->SetExternalPopupOriginAdjustmentsForEmulation(
        external_popup_menu_.get(),
        render_view_->screen_metrics_emulator_.get());
  }
  return external_popup_menu_.get();
#else
  return NULL;
#endif
}

blink::WebCookieJar* RenderFrameImpl::cookieJar(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  return &cookie_jar_;
}

blink::WebServiceWorkerProvider* RenderFrameImpl::createServiceWorkerProvider(
    blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  // At this point we should have non-null data source.
  DCHECK(frame->dataSource());
  if (!ChildThreadImpl::current())
    return nullptr;  // May be null in some tests.
  ServiceWorkerNetworkProvider* provider =
      ServiceWorkerNetworkProvider::FromDocumentState(
          DocumentState::FromDataSource(frame->dataSource()));
  DCHECK(provider);
  if (!provider->context()) {
    // The context can be null when the frame is sandboxed.
    return nullptr;
  }
  return new WebServiceWorkerProviderImpl(
      ChildThreadImpl::current()->thread_safe_sender(),
      provider->context());
}

void RenderFrameImpl::didAccessInitialDocument(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  // If the request hasn't yet committed, notify the browser process that it is
  // no longer safe to show the pending URL of the main frame, since a URL spoof
  // is now possible. (If the request has committed, the browser already knows.)
  if (!frame->parent()) {
    DocumentState* document_state =
        DocumentState::FromDataSource(frame->dataSource());
    NavigationStateImpl* navigation_state =
        static_cast<NavigationStateImpl*>(document_state->navigation_state());

    if (!navigation_state->request_committed()) {
      Send(new FrameHostMsg_DidAccessInitialDocument(routing_id_));
    }
  }
}

blink::WebFrame* RenderFrameImpl::createChildFrame(
    blink::WebLocalFrame* parent,
    blink::WebTreeScopeType scope,
    const blink::WebString& name,
    blink::WebSandboxFlags sandbox_flags) {
  // Synchronously notify the browser of a child frame creation to get the
  // routing_id for the RenderFrame.
  int child_routing_id = MSG_ROUTING_NONE;
  Send(new FrameHostMsg_CreateChildFrame(
      routing_id_, scope,
      base::UTF16ToUTF8(base::StringPiece16(name)), sandbox_flags,
      &child_routing_id));

  // Allocation of routing id failed, so we can't create a child frame. This can
  // happen if this RenderFrameImpl's IPCs are being filtered when in swapped
  // out state or synchronous IPC message above has failed.
  if (child_routing_id == MSG_ROUTING_NONE) {
    NOTREACHED() << "Failed to allocate routing id for child frame.";
    return nullptr;
  }

  // This method is always called by local frames, never remote frames.

  // Tracing analysis uses this to find main frames when this value is
  // MSG_ROUTING_NONE, and build the frame tree otherwise.
  TRACE_EVENT2("navigation", "RenderFrameImpl::createChildFrame",
               "id", routing_id_,
               "child", child_routing_id);

  // Create the RenderFrame and WebLocalFrame, linking the two.
  RenderFrameImpl* child_render_frame = RenderFrameImpl::Create(
      render_view_.get(), child_routing_id);
  blink::WebLocalFrame* web_frame =
      WebLocalFrame::create(scope, child_render_frame);
  child_render_frame->SetWebFrame(web_frame);

  // Add the frame to the frame tree and initialize it.
  parent->appendChild(web_frame);
  child_render_frame->Initialize();

  return web_frame;
}

void RenderFrameImpl::didChangeOpener(blink::WebFrame* opener) {
  // Only active frames are able to disown their opener.
  if (!opener && is_swapped_out_)
    return;

  // Only a local frame should be able to update another frame's opener.
  DCHECK(!opener || opener->isWebLocalFrame());

  int opener_routing_id = opener ?
      RenderFrameImpl::FromWebFrame(opener->toWebLocalFrame())->GetRoutingID() :
      MSG_ROUTING_NONE;
  Send(new FrameHostMsg_DidChangeOpener(routing_id_, opener_routing_id));
}

void RenderFrameImpl::frameDetached(blink::WebFrame* frame, DetachType type) {
  // NOTE: This function is called on the frame that is being detached and not
  // the parent frame.  This is different from createChildFrame() which is
  // called on the parent frame.
  CHECK(!is_detaching_);
  DCHECK(!frame_ || frame_ == frame);

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, FrameDetached());
  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers(),
                    FrameDetached(frame));

  // We only notify the browser process when the frame is being detached for
  // removal. If the frame is being detached for swap, we don't need to do this
  // since we are not modifiying the frame tree.
  if (type == DetachType::Remove)
    Send(new FrameHostMsg_Detach(routing_id_));

  // The |is_detaching_| flag disables Send(). FrameHostMsg_Detach must be
  // sent before setting |is_detaching_| to true.
  is_detaching_ = true;

  // Clean up the associated RenderWidget for the frame, if there is one.
  if (render_widget_) {
    render_widget_->UnregisterRenderFrame(this);
    render_widget_->CloseForFrame();
  }

  // We need to clean up subframes by removing them from the map and deleting
  // the RenderFrameImpl.  In contrast, the main frame is owned by its
  // containing RenderViewHost (so that they have the same lifetime), so only
  // removal from the map is needed and no deletion.
  FrameMap::iterator it = g_frame_map.Get().find(frame);
  CHECK(it != g_frame_map.Get().end());
  CHECK_EQ(it->second, this);
  g_frame_map.Get().erase(it);

  // Only remove the frame from the renderer's frame tree if the frame is
  // being detached for removal. In the case of a swap, the frame needs to
  // remain in the tree so WebFrame::swap() can replace it with the new frame.
  if (!is_main_frame_ && type == DetachType::Remove)
    frame->parent()->removeChild(frame);

  // |frame| is invalid after here.  Be sure to clear frame_ as well, since this
  // object may not be deleted immediately and other methods may try to access
  // it.
  frame->close();
  frame_ = nullptr;

  delete this;
  // Object is invalid after this point.
}

void RenderFrameImpl::frameFocused() {
  Send(new FrameHostMsg_FrameFocused(routing_id_));
}

void RenderFrameImpl::willClose(blink::WebFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, FrameWillClose());
  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers(),
                    FrameWillClose(frame));
}

void RenderFrameImpl::didChangeName(blink::WebLocalFrame* frame,
                                    const blink::WebString& name) {
  DCHECK(!frame_ || frame_ == frame);

  // TODO(alexmos): According to https://crbug.com/169110, sending window.name
  // updates may have performance implications for benchmarks like SunSpider.
  // For now, send these updates only for --site-per-process, which needs to
  // replicate frame names to frame proxies, and when
  // |report_frame_name_changes| is set (used by <webview>).  If needed, this
  // can be optimized further by only sending the update if there are any
  // remote frames in the frame tree, or delaying and batching up IPCs if
  // updates are happening too frequently.
  if (SiteIsolationPolicy::AreCrossProcessFramesPossible() ||
      render_view_->renderer_preferences_.report_frame_name_changes) {
    Send(new FrameHostMsg_DidChangeName(
        routing_id_, base::UTF16ToUTF8(base::StringPiece16(name))));
  }
}

void RenderFrameImpl::didChangeSandboxFlags(blink::WebFrame* child_frame,
                                            blink::WebSandboxFlags flags) {
  Send(new FrameHostMsg_DidChangeSandboxFlags(
      routing_id_, GetRoutingIdForFrameOrProxy(child_frame), flags));
}

void RenderFrameImpl::didMatchCSS(
    blink::WebLocalFrame* frame,
    const blink::WebVector<blink::WebString>& newly_matching_selectors,
    const blink::WebVector<blink::WebString>& stopped_matching_selectors) {
  DCHECK(!frame_ || frame_ == frame);

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_,
                    DidMatchCSS(newly_matching_selectors,
                                stopped_matching_selectors));
}

bool RenderFrameImpl::shouldReportDetailedMessageForSource(
    const blink::WebString& source) {
  return GetContentClient()->renderer()->ShouldReportDetailedMessageForSource(
      source);
}

void RenderFrameImpl::didAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  logging::LogSeverity log_severity = logging::LOG_VERBOSE;
  switch (message.level) {
    case blink::WebConsoleMessage::LevelDebug:
      log_severity = logging::LOG_VERBOSE;
      break;
    case blink::WebConsoleMessage::LevelLog:
    case blink::WebConsoleMessage::LevelInfo:
      log_severity = logging::LOG_INFO;
      break;
    case blink::WebConsoleMessage::LevelWarning:
      log_severity = logging::LOG_WARNING;
      break;
    case blink::WebConsoleMessage::LevelError:
      log_severity = logging::LOG_ERROR;
      break;
    default:
      log_severity = logging::LOG_VERBOSE;
  }

  if (shouldReportDetailedMessageForSource(source_name)) {
    FOR_EACH_OBSERVER(
        RenderFrameObserver, observers_,
        DetailedConsoleMessageAdded(message.text,
                                    source_name,
                                    stack_trace,
                                    source_line,
                                    static_cast<int32>(log_severity)));
  }

  Send(new FrameHostMsg_AddMessageToConsole(routing_id_,
                                            static_cast<int32>(log_severity),
                                            message.text,
                                            static_cast<int32>(source_line),
                                            source_name));
}

void RenderFrameImpl::loadURLExternally(const blink::WebURLRequest& request,
                                        blink::WebNavigationPolicy policy,
                                        const blink::WebString& suggested_name,
                                        bool should_replace_current_entry) {
  Referrer referrer(RenderViewImpl::GetReferrerFromRequest(frame_, request));
  if (policy == blink::WebNavigationPolicyDownload) {
    render_view_->Send(new ViewHostMsg_DownloadUrl(render_view_->GetRoutingID(),
                                                   GetRoutingID(),
                                                   request.url(), referrer,
                                                   suggested_name));
  } else {
    OpenURL(request.url(), referrer, policy, should_replace_current_entry);
  }
}

blink::WebHistoryItem RenderFrameImpl::historyItemForNewChildFrame(
    blink::WebFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  return render_view_->history_controller()->GetItemForNewChildFrame(this);
}

void RenderFrameImpl::willSendSubmitEvent(blink::WebLocalFrame* frame,
                                          const blink::WebFormElement& form) {
  DCHECK(!frame_ || frame_ == frame);

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, WillSendSubmitEvent(form));
}

void RenderFrameImpl::willSubmitForm(blink::WebLocalFrame* frame,
                                     const blink::WebFormElement& form) {
  DCHECK(!frame_ || frame_ == frame);
  DocumentState* document_state =
      DocumentState::FromDataSource(frame->provisionalDataSource());
  NavigationStateImpl* navigation_state =
      static_cast<NavigationStateImpl*>(document_state->navigation_state());
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);

  if (ui::PageTransitionCoreTypeIs(navigation_state->GetTransitionType(),
                                   ui::PAGE_TRANSITION_LINK)) {
    navigation_state->set_transition_type(ui::PAGE_TRANSITION_FORM_SUBMIT);
  }

  // Save these to be processed when the ensuing navigation is committed.
  WebSearchableFormData web_searchable_form_data(form);
  internal_data->set_searchable_form_url(web_searchable_form_data.url());
  internal_data->set_searchable_form_encoding(
      web_searchable_form_data.encoding().utf8());

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, WillSubmitForm(form));
}

void RenderFrameImpl::didCreateDataSource(blink::WebLocalFrame* frame,
                                          blink::WebDataSource* datasource) {
  DCHECK(!frame_ || frame_ == frame);

  bool content_initiated = !pending_navigation_params_.get();

  // Make sure any previous redirect URLs end up in our new data source.
  if (pending_navigation_params_.get()) {
    for (const auto& i :
         pending_navigation_params_->request_params.redirects) {
      datasource->appendRedirect(i);
    }
  }

  DocumentState* document_state = DocumentState::FromDataSource(datasource);
  if (!document_state) {
    document_state = new DocumentState;
    datasource->setExtraData(document_state);
    if (!content_initiated)
      PopulateDocumentStateFromPending(document_state);
  }

  // Carry over the user agent override flag, if it exists.
  blink::WebView* webview = render_view_->webview();
  if (content_initiated && webview && webview->mainFrame() &&
      webview->mainFrame()->isWebLocalFrame() &&
      webview->mainFrame()->dataSource()) {
    DocumentState* old_document_state =
        DocumentState::FromDataSource(webview->mainFrame()->dataSource());
    if (old_document_state) {
      InternalDocumentStateData* internal_data =
          InternalDocumentStateData::FromDocumentState(document_state);
      InternalDocumentStateData* old_internal_data =
          InternalDocumentStateData::FromDocumentState(old_document_state);
      internal_data->set_is_overriding_user_agent(
          old_internal_data->is_overriding_user_agent());
    }
  }

  // The rest of RenderView assumes that a WebDataSource will always have a
  // non-null NavigationState.
  if (content_initiated) {
    document_state->set_navigation_state(
        NavigationStateImpl::CreateContentInitiated());
  } else {
    document_state->set_navigation_state(CreateNavigationStateFromPending());
    pending_navigation_params_.reset();
  }

  // DocumentState::referred_by_prefetcher_ is true if we are
  // navigating from a page that used prefetching using a link on that
  // page.  We are early enough in the request process here that we
  // can still see the DocumentState of the previous page and set
  // this value appropriately.
  // TODO(gavinp): catch the important case of navigation in a new
  // renderer process.
  if (webview) {
    if (WebFrame* old_frame = webview->mainFrame()) {
      const WebURLRequest& original_request = datasource->originalRequest();
      const GURL referrer(
          original_request.httpHeaderField(WebString::fromUTF8("Referer")));
      if (!referrer.is_empty() && old_frame->isWebLocalFrame() &&
          DocumentState::FromDataSource(old_frame->dataSource())
              ->was_prefetcher()) {
        for (; old_frame; old_frame = old_frame->traverseNext(false)) {
          WebDataSource* old_frame_datasource = old_frame->dataSource();
          if (old_frame_datasource &&
              referrer == GURL(old_frame_datasource->request().url())) {
            document_state->set_was_referred_by_prefetcher(true);
            break;
          }
        }
      }
    }
  }

  if (content_initiated) {
    const WebURLRequest& request = datasource->request();
    switch (request.cachePolicy()) {
      case WebURLRequest::UseProtocolCachePolicy:  // normal load.
        document_state->set_load_type(DocumentState::LINK_LOAD_NORMAL);
        break;
      case WebURLRequest::ReloadIgnoringCacheData:  // reload.
      case WebURLRequest::ReloadBypassingCache:     // end-to-end reload.
        document_state->set_load_type(DocumentState::LINK_LOAD_RELOAD);
        break;
      case WebURLRequest::ReturnCacheDataElseLoad:  // allow stale data.
        document_state->set_load_type(DocumentState::LINK_LOAD_CACHE_STALE_OK);
        break;
      case WebURLRequest::ReturnCacheDataDontLoad:  // Don't re-post.
        document_state->set_load_type(DocumentState::LINK_LOAD_CACHE_ONLY);
        break;
      default:
        NOTREACHED();
    }
  }

  // Create the serviceworker's per-document network observing object if it
  // does not exist (When navigation happens within a page, the provider already
  // exists).
  if (!ServiceWorkerNetworkProvider::FromDocumentState(
          DocumentState::FromDataSource(datasource))) {
    ServiceWorkerProviderType provider_type =
        SERVICE_WORKER_PROVIDER_FOR_WINDOW;
    if ((frame->effectiveSandboxFlags() & blink::WebSandboxFlags::Origin) ==
        blink::WebSandboxFlags::Origin) {
      provider_type = SERVICE_WORKER_PROVIDER_FOR_SANDBOXED_FRAME;
    }
    scoped_ptr<ServiceWorkerNetworkProvider> network_provider(
        new ServiceWorkerNetworkProvider(routing_id_, provider_type));
    ServiceWorkerNetworkProvider::AttachToDocumentState(
        DocumentState::FromDataSource(datasource),
        network_provider.Pass());
  }
}

void RenderFrameImpl::didStartProvisionalLoad(blink::WebLocalFrame* frame,
                                              double triggering_event_time) {
  DCHECK(!frame_ || frame_ == frame);
  WebDataSource* ds = frame->provisionalDataSource();

  // In fast/loader/stop-provisional-loads.html, we abort the load before this
  // callback is invoked.
  if (!ds)
    return;

  TRACE_EVENT2("navigation,benchmark",
               "RenderFrameImpl::didStartProvisionalLoad", "id", routing_id_,
               "url", ds->request().url().string().utf8());
  DocumentState* document_state = DocumentState::FromDataSource(ds);

  // We should only navigate to swappedout:// when is_swapped_out_ is true.
  CHECK_IMPLIES(ds->request().url() == GURL(kSwappedOutURL), is_swapped_out_)
      << "Heard swappedout:// when not swapped out.";

  // Update the request time if WebKit has better knowledge of it.
  if (document_state->request_time().is_null() &&
          triggering_event_time != 0.0) {
    document_state->set_request_time(Time::FromDoubleT(triggering_event_time));
  }

  // Start time is only set after request time.
  document_state->set_start_load_time(Time::Now());

  bool is_top_most = !frame->parent();
  if (is_top_most) {
    render_view_->set_navigation_gesture(
        WebUserGestureIndicator::isProcessingUserGesture() ?
            NavigationGestureUser : NavigationGestureAuto);
  } else if (ds->replacesCurrentHistoryItem()) {
    // Subframe navigations that don't add session history items must be
    // marked with AUTO_SUBFRAME. See also didFailProvisionalLoad for how we
    // handle loading of error pages.
    static_cast<NavigationStateImpl*>(document_state->navigation_state())
        ->set_transition_type(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  }

  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers(),
                    DidStartProvisionalLoad(frame));
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, DidStartProvisionalLoad());

  Send(new FrameHostMsg_DidStartProvisionalLoadForFrame(
       routing_id_, ds->request().url()));
}

void RenderFrameImpl::didReceiveServerRedirectForProvisionalLoad(
    blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  render_view_->history_controller()->RemoveChildrenForRedirect(this);
}

void RenderFrameImpl::didFailProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebURLError& error,
    blink::WebHistoryCommitType commit_type) {
  TRACE_EVENT1("navigation,benchmark",
               "RenderFrameImpl::didFailProvisionalLoad", "id", routing_id_);
  DCHECK(!frame_ || frame_ == frame);
  WebDataSource* ds = frame->provisionalDataSource();
  DCHECK(ds);

  const WebURLRequest& failed_request = ds->request();

  // Notify the browser that we failed a provisional load with an error.
  //
  // Note: It is important this notification occur before DidStopLoading so the
  //       SSL manager can react to the provisional load failure before being
  //       notified the load stopped.
  //
  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers(),
                    DidFailProvisionalLoad(frame, error));
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_,
                    DidFailProvisionalLoad(error));

  SendFailedProvisionalLoad(failed_request, error, frame);

  if (!ShouldDisplayErrorPageForFailedLoad(error.reason, error.unreachableURL))
    return;

  // Make sure we never show errors in view source mode.
  frame->enableViewSourceMode(false);

  DocumentState* document_state = DocumentState::FromDataSource(ds);
  NavigationStateImpl* navigation_state =
      static_cast<NavigationStateImpl*>(document_state->navigation_state());

  // If this is a failed back/forward/reload navigation, then we need to do a
  // 'replace' load.  This is necessary to avoid messing up session history.
  // Otherwise, we do a normal load, which simulates a 'go' navigation as far
  // as session history is concerned.
  bool replace = commit_type != blink::WebStandardCommit;

  // If we failed on a browser initiated request, then make sure that our error
  // page load is regarded as the same browser initiated request.
  if (!navigation_state->IsContentInitiated()) {
    pending_navigation_params_.reset(new NavigationParams(
        navigation_state->common_params(), navigation_state->start_params(),
        navigation_state->request_params()));
    pending_navigation_params_->request_params.request_time =
        document_state->request_time();
  }

  // Load an error page.
  LoadNavigationErrorPage(failed_request, error, replace);
}

void RenderFrameImpl::didCommitProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type) {
  TRACE_EVENT2("navigation", "RenderFrameImpl::didCommitProvisionalLoad",
               "id", routing_id_,
               "url", GetLoadingUrl().possibly_invalid_spec());
  DCHECK(!frame_ || frame_ == frame);
  DocumentState* document_state =
      DocumentState::FromDataSource(frame->dataSource());
  NavigationStateImpl* navigation_state =
      static_cast<NavigationStateImpl*>(document_state->navigation_state());
  WebURLResponseExtraDataImpl* extra_data = GetExtraDataFromResponse(
      frame->dataSource()->response());
  is_using_lofi_ = extra_data && extra_data->is_using_lofi();

  if (proxy_routing_id_ != MSG_ROUTING_NONE) {
    RenderFrameProxy* proxy =
        RenderFrameProxy::FromRoutingID(proxy_routing_id_);
    CHECK(proxy);
    proxy->web_frame()->swap(frame_);
    proxy_routing_id_ = MSG_ROUTING_NONE;

    // If this is the main frame going from a remote frame to a local frame,
    // it needs to set RenderViewImpl's pointer for the main frame to itself
    // and ensure RenderWidget is no longer in swapped out mode.
    if (is_main_frame_) {
      CHECK(!render_view_->main_render_frame_);
      render_view_->main_render_frame_ = this;
      if (render_view_->is_swapped_out())
        render_view_->SetSwappedOut(false);
    }
  }

  // For new page navigations, the browser process needs to be notified of the
  // first paint of that page, so it can cancel the timer that waits for it.
  if (is_main_frame_ && !navigation_state->WasWithinSamePage()) {
    render_view_->QueueMessage(
        new ViewHostMsg_DidFirstPaintAfterLoad(render_view_->routing_id_),
        MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE);
  }

  // When we perform a new navigation, we need to update the last committed
  // session history entry with state for the page we are leaving. Do this
  // before updating the HistoryController state.
  render_view_->SendUpdateState();

  render_view_->history_controller()->UpdateForCommit(
      this, item, commit_type, navigation_state->WasWithinSamePage());

  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);

  if (document_state->commit_load_time().is_null())
    document_state->set_commit_load_time(Time::Now());

  if (internal_data->must_reset_scroll_and_scale_state()) {
    render_view_->webview()->resetScrollAndScaleState();
    internal_data->set_must_reset_scroll_and_scale_state(false);
  }

  bool is_new_navigation = commit_type == blink::WebStandardCommit;
  if (is_new_navigation) {
    // We bump our Page ID to correspond with the new session history entry.
    render_view_->page_id_ = render_view_->next_page_id_++;

    // Don't update history list values for kSwappedOutURL, since
    // we don't want to forget the entry that was there, and since we will
    // never come back to kSwappedOutURL.  Note that we have to call
    // SendUpdateState and update page_id_ even in this case, so that
    // the current entry gets a state update and so that we don't send a
    // state update to the wrong entry when we swap back in.
    DCHECK_IMPLIES(
        navigation_state->common_params().should_replace_current_entry,
        render_view_->history_list_length_ > 0);
    if (GetLoadingUrl() != GURL(kSwappedOutURL) &&
        !navigation_state->common_params().should_replace_current_entry) {
      // Advance our offset in session history, applying the length limit.
      // There is now no forward history.
      render_view_->history_list_offset_++;
      if (render_view_->history_list_offset_ >= kMaxSessionHistoryEntries)
        render_view_->history_list_offset_ = kMaxSessionHistoryEntries - 1;
      render_view_->history_list_length_ =
          render_view_->history_list_offset_ + 1;
    }
  } else {
    const RequestNavigationParams& request_params =
        navigation_state->request_params();
    if (request_params.nav_entry_id != 0 &&
        !request_params.intended_as_new_entry) {
      // This is a successful session history navigation!
      render_view_->page_id_ = request_params.page_id;

      render_view_->history_list_offset_ =
          request_params.pending_history_list_offset;
    }
  }

  bool sent = Send(
      new FrameHostMsg_DidAssignPageId(routing_id_, render_view_->page_id_));
  CHECK(sent);  // http://crbug.com/407376

  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers_,
                    DidCommitProvisionalLoad(frame, is_new_navigation));
  FOR_EACH_OBSERVER(
      RenderFrameObserver, observers_,
      DidCommitProvisionalLoad(is_new_navigation,
                               navigation_state->WasWithinSamePage()));

  if (!frame->parent()) {  // Only for top frames.
    RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
    if (render_thread_impl) {  // Can be NULL in tests.
      render_thread_impl->histogram_customizer()->
          RenderViewNavigatedToHost(GURL(GetLoadingUrl()).host(),
                                    RenderView::GetRenderViewCount());
      // The scheduler isn't interested in history inert commits unless they
      // are reloads.
      if (commit_type != blink::WebHistoryInertCommit ||
          PageTransitionCoreTypeIs(
              navigation_state->GetTransitionType(),
              ui::PAGE_TRANSITION_RELOAD)) {
        render_thread_impl->GetRendererScheduler()->OnNavigationStarted();
      }
    }
  }

  // Remember that we've already processed this request, so we don't update
  // the session history again.  We do this regardless of whether this is
  // a session history navigation, because if we attempted a session history
  // navigation without valid HistoryItem state, WebCore will think it is a
  // new navigation.
  navigation_state->set_request_committed(true);

  SendDidCommitProvisionalLoad(frame, commit_type, item);

  // Check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());
}

void RenderFrameImpl::didCreateNewDocument(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, DidCreateNewDocument());
  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers(),
                    DidCreateNewDocument(frame));
}

void RenderFrameImpl::didClearWindowObject(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);

  int enabled_bindings = render_view_->GetEnabledBindings();

  if (enabled_bindings & BINDINGS_POLICY_WEB_UI)
    WebUIExtension::Install(frame);

  if (enabled_bindings & BINDINGS_POLICY_DOM_AUTOMATION)
    DomAutomationController::Install(this, frame);

  if (enabled_bindings & BINDINGS_POLICY_STATS_COLLECTION)
    StatsCollectionController::Install(frame);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(cc::switches::kEnableGpuBenchmarking))
    GpuBenchmarking::Install(frame);

  if (command_line.HasSwitch(switches::kEnableMemoryBenchmarking))
    MemoryBenchmarkingExtension::Install(frame);

  if (command_line.HasSwitch(switches::kEnableSkiaBenchmarking))
    SkiaBenchmarking::Install(frame);

  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers(),
                    DidClearWindowObject(frame));
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, DidClearWindowObject());
}

void RenderFrameImpl::didCreateDocumentElement(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);

  // Notify the browser about non-blank documents loading in the top frame.
  GURL url = frame->document().url();
  if (url.is_valid() && url.spec() != url::kAboutBlankURL) {
    // TODO(nasko): Check if webview()->mainFrame() is the same as the
    // frame->tree()->top().
    blink::WebFrame* main_frame = render_view_->webview()->mainFrame();
    if (frame == main_frame) {
      // For now, don't remember plugin zoom values.  We don't want to mix them
      // with normal web content (i.e. a fixed layout plugin would usually want
      // them different).
      render_view_->Send(new ViewHostMsg_DocumentAvailableInMainFrame(
          render_view_->GetRoutingID(),
          main_frame->document().isPluginDocument()));
    }
  }

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_,
                    DidCreateDocumentElement());
  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers(),
                    DidCreateDocumentElement(frame));
}

void RenderFrameImpl::didReceiveTitle(blink::WebLocalFrame* frame,
                                      const blink::WebString& title,
                                      blink::WebTextDirection direction) {
  DCHECK(!frame_ || frame_ == frame);
  // Ignore all but top level navigations.
  if (!frame->parent()) {
    base::string16 title16 = title;
    base::trace_event::TraceLog::GetInstance()->UpdateProcessLabel(
        routing_id_, base::UTF16ToUTF8(title16));

    base::string16 shortened_title = title16.substr(0, kMaxTitleChars);
    Send(new FrameHostMsg_UpdateTitle(routing_id_,
                                      shortened_title, direction));
  }

  // Also check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());
}

void RenderFrameImpl::didChangeIcon(blink::WebLocalFrame* frame,
                                    blink::WebIconURL::Type icon_type) {
  DCHECK(!frame_ || frame_ == frame);
  // TODO(nasko): Investigate wheather implementation should move here.
  render_view_->didChangeIcon(frame, icon_type);
}

void RenderFrameImpl::didFinishDocumentLoad(blink::WebLocalFrame* frame,
                                            bool document_is_empty) {
  TRACE_EVENT1("navigation,benchmark", "RenderFrameImpl::didFinishDocumentLoad",
               "id", routing_id_);
  DCHECK(!frame_ || frame_ == frame);
  WebDataSource* ds = frame->dataSource();
  DocumentState* document_state = DocumentState::FromDataSource(ds);
  document_state->set_finish_document_load_time(Time::Now());

  Send(new FrameHostMsg_DidFinishDocumentLoad(routing_id_));

  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers(),
                    DidFinishDocumentLoad(frame));
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, DidFinishDocumentLoad());

  // Check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());

  // If this is an empty document with an http status code indicating an error,
  // we may want to display our own error page, so the user doesn't end up
  // with an unexplained blank page.
  if (!document_is_empty)
    return;

  // Do not show error page when DevTools is attached.
  RenderFrameImpl* localRoot = this;
  while (localRoot->frame_ && localRoot->frame_->parent() &&
         localRoot->frame_->parent()->isWebLocalFrame()) {
    localRoot = RenderFrameImpl::FromWebFrame(localRoot->frame_->parent());
    DCHECK(localRoot);
  }
  if (localRoot->devtools_agent_ && localRoot->devtools_agent_->IsAttached())
    return;

  // Display error page instead of a blank page, if appropriate.
  std::string error_domain = "http";
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDataSource(frame->dataSource());
  int http_status_code = internal_data->http_status_code();
  if (GetContentClient()->renderer()->HasErrorPage(http_status_code,
                                                   &error_domain)) {
    WebURLError error;
    error.unreachableURL = frame->document().url();
    error.domain = WebString::fromUTF8(error_domain);
    error.reason = http_status_code;
    LoadNavigationErrorPage(frame->dataSource()->request(), error, true);
  }
}

void RenderFrameImpl::didHandleOnloadEvents(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  if (!frame->parent()) {
    FrameMsg_UILoadMetricsReportType::Value report_type =
        static_cast<FrameMsg_UILoadMetricsReportType::Value>(
            frame->dataSource()->request().inputPerfMetricReportPolicy());
    base::TimeTicks ui_timestamp = base::TimeTicks() +
        base::TimeDelta::FromSecondsD(
            frame->dataSource()->request().uiStartTime());

    Send(new FrameHostMsg_DocumentOnLoadCompleted(
        routing_id_, report_type, ui_timestamp));
  }
}

void RenderFrameImpl::didFailLoad(blink::WebLocalFrame* frame,
                                  const blink::WebURLError& error,
                                  blink::WebHistoryCommitType commit_type) {
  TRACE_EVENT1("navigation", "RenderFrameImpl::didFailLoad",
               "id", routing_id_);
  DCHECK(!frame_ || frame_ == frame);
  // TODO(nasko): Move implementation here. No state needed.
  WebDataSource* ds = frame->dataSource();
  DCHECK(ds);

  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers(),
                    DidFailLoad(frame, error));

  const WebURLRequest& failed_request = ds->request();
  base::string16 error_description;
  GetContentClient()->renderer()->GetNavigationErrorStrings(
      render_view_.get(),
      frame,
      failed_request,
      error,
      NULL,
      &error_description);
  Send(new FrameHostMsg_DidFailLoadWithError(routing_id_,
                                             failed_request.url(),
                                             error.reason,
                                             error_description,
                                             error.wasIgnoredByHandler));
}

void RenderFrameImpl::didFinishLoad(blink::WebLocalFrame* frame) {
  TRACE_EVENT1("navigation,benchmark", "RenderFrameImpl::didFinishLoad", "id",
               routing_id_);
  DCHECK(!frame_ || frame_ == frame);
  WebDataSource* ds = frame->dataSource();
  DocumentState* document_state = DocumentState::FromDataSource(ds);
  if (document_state->finish_load_time().is_null()) {
    if (!frame->parent()) {
      TRACE_EVENT_INSTANT0("WebCore,benchmark", "LoadFinished",
                           TRACE_EVENT_SCOPE_PROCESS);
    }
    document_state->set_finish_load_time(Time::Now());
  }

  FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers(),
                    DidFinishLoad(frame));
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, DidFinishLoad());

  // Don't send this message while the frame is swapped out.
  if (is_swapped_out())
    return;

  Send(new FrameHostMsg_DidFinishLoad(routing_id_,
                                      ds->request().url()));
}

void RenderFrameImpl::didNavigateWithinPage(blink::WebLocalFrame* frame,
    const blink::WebHistoryItem& item,
    blink::WebHistoryCommitType commit_type) {
  TRACE_EVENT1("navigation", "RenderFrameImpl::didNavigateWithinPage",
               "id", routing_id_);
  DCHECK(!frame_ || frame_ == frame);
  // If this was a reference fragment navigation that we initiated, then we
  // could end up having a non-null pending navigation params.  We just need to
  // update the ExtraData on the datasource so that others who read the
  // ExtraData will get the new NavigationState.  Similarly, if we did not
  // initiate this navigation, then we need to take care to reset any pre-
  // existing navigation state to a content-initiated navigation state.
  // didCreateDataSource conveniently takes care of this for us.
  didCreateDataSource(frame, frame->dataSource());

  DocumentState* document_state =
      DocumentState::FromDataSource(frame->dataSource());
  static_cast<NavigationStateImpl*>(document_state->navigation_state())
      ->set_was_within_same_page(true);

  didCommitProvisionalLoad(frame, item, commit_type);
}

void RenderFrameImpl::didUpdateCurrentHistoryItem(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  // TODO(nasko): Move implementation here. Needed methods:
  // * StartNavStateSyncTimerIfNecessary
  render_view_->didUpdateCurrentHistoryItem(frame);
}

void RenderFrameImpl::didChangeThemeColor() {
  if (frame_->parent())
    return;

  Send(new FrameHostMsg_DidChangeThemeColor(
      routing_id_, frame_->document().themeColor()));
}

void RenderFrameImpl::dispatchLoad() {
  Send(new FrameHostMsg_DispatchLoad(routing_id_));
}

void RenderFrameImpl::requestNotificationPermission(
    const blink::WebSecurityOrigin& origin,
    blink::WebNotificationPermissionCallback* callback) {
  if (!notification_permission_dispatcher_) {
    notification_permission_dispatcher_ =
        new NotificationPermissionDispatcher(this);
  }

  notification_permission_dispatcher_->RequestPermission(origin, callback);
}

void RenderFrameImpl::didChangeSelection(bool is_empty_selection) {
  if (!GetRenderWidget()->handling_input_event() && !handling_select_range_)
    return;

  if (is_empty_selection)
    selection_text_.clear();

  // UpdateTextInputState should be called before SyncSelectionIfRequired.
  // UpdateTextInputState may send TextInputStateChanged to notify the focus
  // was changed, and SyncSelectionIfRequired may send SelectionChanged
  // to notify the selection was changed.  Focus change should be notified
  // before selection change.
  GetRenderWidget()->UpdateTextInputState(
      RenderWidget::NO_SHOW_IME, RenderWidget::FROM_NON_IME);
  SyncSelectionIfRequired();
}

blink::WebColorChooser* RenderFrameImpl::createColorChooser(
    blink::WebColorChooserClient* client,
    const blink::WebColor& initial_color,
    const blink::WebVector<blink::WebColorSuggestion>& suggestions) {
  RendererWebColorChooserImpl* color_chooser =
      new RendererWebColorChooserImpl(this, client);
  std::vector<ColorSuggestion> color_suggestions;
  for (size_t i = 0; i < suggestions.size(); i++) {
    color_suggestions.push_back(ColorSuggestion(suggestions[i]));
  }
  color_chooser->Open(static_cast<SkColor>(initial_color), color_suggestions);
  return color_chooser;
}

void RenderFrameImpl::runModalAlertDialog(const blink::WebString& message) {
  RunJavaScriptMessage(JAVASCRIPT_MESSAGE_TYPE_ALERT,
                       message,
                       base::string16(),
                       frame_->document().url(),
                       NULL);
}

bool RenderFrameImpl::runModalConfirmDialog(const blink::WebString& message) {
  return RunJavaScriptMessage(JAVASCRIPT_MESSAGE_TYPE_CONFIRM,
                              message,
                              base::string16(),
                              frame_->document().url(),
                              NULL);
}

bool RenderFrameImpl::runModalPromptDialog(
    const blink::WebString& message,
    const blink::WebString& default_value,
    blink::WebString* actual_value) {
  base::string16 result;
  bool ok = RunJavaScriptMessage(JAVASCRIPT_MESSAGE_TYPE_PROMPT,
                                 message,
                                 default_value,
                                 frame_->document().url(),
                                 &result);
  if (ok)
    actual_value->assign(result);
  return ok;
}

bool RenderFrameImpl::runModalBeforeUnloadDialog(
    bool is_reload,
    const blink::WebString& message) {
  // If we are swapping out, we have already run the beforeunload handler.
  // TODO(creis): Fix OnSwapOut to clear the frame without running beforeunload
  // at all, to avoid running it twice.
  if (is_swapped_out_)
    return true;

  // Don't allow further dialogs if we are waiting to swap out, since the
  // PageGroupLoadDeferrer in our stack prevents it.
  if (render_view()->suppress_dialogs_until_swap_out_)
    return false;

  bool success = false;
  // This is an ignored return value, but is included so we can accept the same
  // response as RunJavaScriptMessage.
  base::string16 ignored_result;
  render_view()->SendAndRunNestedMessageLoop(
      new FrameHostMsg_RunBeforeUnloadConfirm(
          routing_id_, frame_->document().url(), message, is_reload,
          &success, &ignored_result));
  return success;
}

void RenderFrameImpl::showContextMenu(const blink::WebContextMenuData& data) {
  ContextMenuParams params = ContextMenuParamsBuilder::Build(data);
  params.source_type = GetRenderWidget()->context_menu_source_type();
  GetRenderWidget()->OnShowHostContextMenu(&params);
  if (GetRenderWidget()->has_host_context_menu_location()) {
    params.x = GetRenderWidget()->host_context_menu_location().x();
    params.y = GetRenderWidget()->host_context_menu_location().y();
  }

  // Serializing a GURL longer than kMaxURLChars will fail, so don't do
  // it.  We replace it with an empty GURL so the appropriate items are disabled
  // in the context menu.
  // TODO(jcivelli): http://crbug.com/45160 This prevents us from saving large
  //                 data encoded images.  We should have a way to save them.
  if (params.src_url.spec().size() > GetMaxURLChars())
    params.src_url = GURL();
  context_menu_node_ = data.node;

#if defined(OS_ANDROID)
  gfx::Rect start_rect;
  gfx::Rect end_rect;
  GetRenderWidget()->GetSelectionBounds(&start_rect, &end_rect);
  params.selection_start = gfx::Point(start_rect.x(), start_rect.bottom());
  params.selection_end = gfx::Point(end_rect.right(), end_rect.bottom());
#endif

  Send(new FrameHostMsg_ContextMenu(routing_id_, params));
}

void RenderFrameImpl::clearContextMenu() {
  context_menu_node_.reset();
}

void RenderFrameImpl::willSendRequest(
    blink::WebLocalFrame* frame,
    unsigned identifier,
    blink::WebURLRequest& request,
    const blink::WebURLResponse& redirect_response) {
  DCHECK(!frame_ || frame_ == frame);
  // The request my be empty during tests.
  if (request.url().isEmpty())
    return;

  // Set the first party for cookies url if it has not been set yet (new
  // requests). For redirects, it is updated by WebURLLoaderImpl.
  if (request.firstPartyForCookies().isEmpty()) {
    if (request.frameType() == blink::WebURLRequest::FrameTypeTopLevel) {
      request.setFirstPartyForCookies(request.url());
    } else {
      // TODO(nasko): When the top-level frame is remote, there is no document.
      // This is broken and should be fixed to propagate the first party.
      WebFrame* top = frame->top();
      if (top->isWebLocalFrame()) {
        request.setFirstPartyForCookies(
            frame->top()->document().firstPartyForCookies());
      }
    }
  }

  WebDataSource* provisional_data_source = frame->provisionalDataSource();
  WebDataSource* data_source =
      provisional_data_source ? provisional_data_source : frame->dataSource();

  DocumentState* document_state = DocumentState::FromDataSource(data_source);
  DCHECK(document_state);
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);
  NavigationStateImpl* navigation_state =
      static_cast<NavigationStateImpl*>(document_state->navigation_state());
  ui::PageTransition transition_type = navigation_state->GetTransitionType();
  if (provisional_data_source && provisional_data_source->isClientRedirect()) {
    transition_type = ui::PageTransitionFromInt(
        transition_type | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  }

  GURL request_url(request.url());
  GURL new_url;
  if (GetContentClient()->renderer()->WillSendRequest(
          frame,
          transition_type,
          request_url,
          request.firstPartyForCookies(),
          &new_url)) {
    request.setURL(WebURL(new_url));
  }

  if (internal_data->is_cache_policy_override_set())
    request.setCachePolicy(internal_data->cache_policy_override());

  // The request's extra data may indicate that we should set a custom user
  // agent. This needs to be done here, after WebKit is through with setting the
  // user agent on its own. Similarly, it may indicate that we should set an
  // X-Requested-With header. This must be done here to avoid breaking CORS
  // checks.
  // PlzNavigate: there may also be a stream url associated with the request.
  WebString custom_user_agent;
  WebString requested_with;
  scoped_ptr<StreamOverrideParameters> stream_override;
  if (request.extraData()) {
    RequestExtraData* old_extra_data =
        static_cast<RequestExtraData*>(request.extraData());

    custom_user_agent = old_extra_data->custom_user_agent();
    if (!custom_user_agent.isNull()) {
      if (custom_user_agent.isEmpty())
        request.clearHTTPHeaderField("User-Agent");
      else
        request.setHTTPHeaderField("User-Agent", custom_user_agent);
    }

    requested_with = old_extra_data->requested_with();
    if (!requested_with.isNull()) {
      if (requested_with.isEmpty())
        request.clearHTTPHeaderField("X-Requested-With");
      else
        request.setHTTPHeaderField("X-Requested-With", requested_with);
    }
    stream_override = old_extra_data->TakeStreamOverrideOwnership();
  }

  // Add the default accept header for frame request if it has not been set
  // already.
  if ((request.frameType() == blink::WebURLRequest::FrameTypeTopLevel ||
       request.frameType() == blink::WebURLRequest::FrameTypeNested) &&
      request.httpHeaderField(WebString::fromUTF8(kAcceptHeader)).isEmpty()) {
    request.setHTTPHeaderField(WebString::fromUTF8(kAcceptHeader),
                               WebString::fromUTF8(kDefaultAcceptHeader));
  }

  // Add an empty HTTP origin header for non GET methods if none is currently
  // present.
  request.addHTTPOriginIfNeeded(WebString());

  // Attach |should_replace_current_entry| state to requests so that, should
  // this navigation later require a request transfer, all state is preserved
  // when it is re-created in the new process.
  bool should_replace_current_entry = false;
  if (navigation_state->IsContentInitiated()) {
    should_replace_current_entry = data_source->replacesCurrentHistoryItem();
  } else {
    // If the navigation is browser-initiated, the NavigationState contains the
    // correct value instead of the WebDataSource.
    //
    // TODO(davidben): Avoid this awkward duplication of state. See comment on
    // NavigationState::should_replace_current_entry().
    should_replace_current_entry =
        navigation_state->common_params().should_replace_current_entry;
  }

  int provider_id = kInvalidServiceWorkerProviderId;
  if (request.frameType() == blink::WebURLRequest::FrameTypeTopLevel ||
      request.frameType() == blink::WebURLRequest::FrameTypeNested) {
    // |provisionalDataSource| may be null in some content::ResourceFetcher
    // use cases, we don't hook those requests.
    if (frame->provisionalDataSource()) {
      ServiceWorkerNetworkProvider* provider =
          ServiceWorkerNetworkProvider::FromDocumentState(
              DocumentState::FromDataSource(frame->provisionalDataSource()));
      provider_id = provider->provider_id();
    }
  } else if (frame->dataSource()) {
    ServiceWorkerNetworkProvider* provider =
        ServiceWorkerNetworkProvider::FromDocumentState(
            DocumentState::FromDataSource(frame->dataSource()));
    provider_id = provider->provider_id();
    // Explicitly set the SkipServiceWorker flag here if the renderer process
    // hasn't received SetControllerServiceWorker message.
    if (!provider->IsControlledByServiceWorker())
      request.setSkipServiceWorker(true);
  }

  WebFrame* parent = frame->parent();
  int parent_routing_id = parent ? GetRoutingIdForFrameOrProxy(parent) : -1;

  RequestExtraData* extra_data = new RequestExtraData();
  extra_data->set_visibility_state(render_view_->visibilityState());
  extra_data->set_custom_user_agent(custom_user_agent);
  extra_data->set_requested_with(requested_with);
  extra_data->set_render_frame_id(routing_id_);
  extra_data->set_is_main_frame(!parent);
  extra_data->set_frame_origin(
      GURL(frame->document().securityOrigin().toString()));
  extra_data->set_parent_is_main_frame(parent && !parent->parent());
  extra_data->set_parent_render_frame_id(parent_routing_id);
  extra_data->set_allow_download(
      navigation_state->common_params().allow_download);
  extra_data->set_transition_type(transition_type);
  extra_data->set_should_replace_current_entry(should_replace_current_entry);
  extra_data->set_transferred_request_child_id(
      navigation_state->start_params().transferred_request_child_id);
  extra_data->set_transferred_request_request_id(
      navigation_state->start_params().transferred_request_request_id);
  extra_data->set_service_worker_provider_id(provider_id);
  extra_data->set_stream_override(stream_override.Pass());
  // TODO(megjablon): Remove once reloading a page without Lo-Fi uses
  // ReloadDisableLoFi rather than ReloadBypassingCache.
  if (request.cachePolicy() == WebURLRequest::ReloadBypassingCache)
    extra_data->set_lofi_state(LOFI_OFF);
  else if (request.loFiState() != WebURLRequest::LoFiUnspecified)
    extra_data->set_lofi_state(static_cast<LoFiState>(request.loFiState()));
  else if (is_main_frame_ && !navigation_state->request_committed())
    extra_data->set_lofi_state(navigation_state->common_params().lofi_state);
  else
    extra_data->set_lofi_state(is_using_lofi_ ? LOFI_ON : LOFI_OFF);
  request.setExtraData(extra_data);

  // TODO(creis): Update prefetching to work with out-of-process iframes.
  WebFrame* top_frame = frame->top();
  if (top_frame && top_frame->isWebLocalFrame()) {
    DocumentState* top_document_state =
        DocumentState::FromDataSource(top_frame->dataSource());
    if (top_document_state) {
      // TODO(gavinp): separate out prefetching and prerender field trials
      // if the rel=prerender rel type is sticking around.
      if (request.requestContext() == WebURLRequest::RequestContextPrefetch)
        top_document_state->set_was_prefetcher(true);
    }
  }

  // This is an instance where we embed a copy of the routing id
  // into the data portion of the message. This can cause problems if we
  // don't register this id on the browser side, since the download manager
  // expects to find a RenderViewHost based off the id.
  request.setRequestorID(render_view_->GetRoutingID());
  request.setHasUserGesture(WebUserGestureIndicator::isProcessingUserGesture());

  if (!navigation_state->start_params().extra_headers.empty()) {
    for (net::HttpUtil::HeadersIterator i(
             navigation_state->start_params().extra_headers.begin(),
             navigation_state->start_params().extra_headers.end(), "\n");
         i.GetNext();) {
      if (base::LowerCaseEqualsASCII(i.name(), "referer")) {
        WebString referrer = WebSecurityPolicy::generateReferrerHeader(
            blink::WebReferrerPolicyDefault,
            request.url(),
            WebString::fromUTF8(i.values()));
        request.setHTTPReferrer(referrer, blink::WebReferrerPolicyDefault);
      } else {
        request.setHTTPHeaderField(WebString::fromUTF8(i.name()),
                                   WebString::fromUTF8(i.values()));
      }
    }
  }

  if (!render_view_->renderer_preferences_.enable_referrers)
    request.setHTTPReferrer(WebString(), blink::WebReferrerPolicyDefault);
}

void RenderFrameImpl::didReceiveResponse(
    blink::WebLocalFrame* frame,
    unsigned identifier,
    const blink::WebURLResponse& response) {
  DCHECK(!frame_ || frame_ == frame);
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

  DocumentState* document_state =
      DocumentState::FromDataSource(frame->provisionalDataSource());
  int http_status_code = response.httpStatusCode();

  // Record page load flags.
  WebURLResponseExtraDataImpl* extra_data = GetExtraDataFromResponse(response);
  if (extra_data) {
    document_state->set_was_fetched_via_spdy(
        extra_data->was_fetched_via_spdy());
    document_state->set_was_npn_negotiated(
        extra_data->was_npn_negotiated());
    document_state->set_npn_negotiated_protocol(
        extra_data->npn_negotiated_protocol());
    document_state->set_was_alternate_protocol_available(
        extra_data->was_alternate_protocol_available());
    document_state->set_connection_info(
        extra_data->connection_info());
    document_state->set_was_fetched_via_proxy(
        extra_data->was_fetched_via_proxy());
    document_state->set_proxy_server(
        extra_data->proxy_server());
  }
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);
  internal_data->set_http_status_code(http_status_code);
}

void RenderFrameImpl::didLoadResourceFromMemoryCache(
    blink::WebLocalFrame* frame,
    const blink::WebURLRequest& request,
    const blink::WebURLResponse& response) {
  DCHECK(!frame_ || frame_ == frame);
  // The recipients of this message have no use for data: URLs: they don't
  // affect the page's insecure content list and are not in the disk cache. To
  // prevent large (1M+) data: URLs from crashing in the IPC system, we simply
  // filter them out here.
  GURL url(request.url());
  if (url.SchemeIs(url::kDataScheme))
    return;

  // Let the browser know we loaded a resource from the memory cache.  This
  // message is needed to display the correct SSL indicators.
  Send(new FrameHostMsg_DidLoadResourceFromMemoryCache(
      routing_id_, url, response.securityInfo(), request.httpMethod().utf8(),
      response.mimeType().utf8(), WebURLRequestToResourceType(request)));
}

void RenderFrameImpl::didDisplayInsecureContent() {
  Send(new FrameHostMsg_DidDisplayInsecureContent(routing_id_));
}

void RenderFrameImpl::didRunInsecureContent(
    const blink::WebSecurityOrigin& origin,
    const blink::WebURL& target) {
  Send(new FrameHostMsg_DidRunInsecureContent(
      routing_id_, origin.toString().utf8(), target));
  GetContentClient()->renderer()->RecordRapporURL(
      "ContentSettings.MixedScript.RanMixedScript",
      GURL(origin.toString().utf8()));
}

void RenderFrameImpl::didChangePerformanceTiming() {
  FOR_EACH_OBSERVER(RenderFrameObserver,
                    observers_,
                    DidChangePerformanceTiming());
}

void RenderFrameImpl::didAbortLoading(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
#if defined(ENABLE_PLUGINS)
  if (frame != render_view_->webview()->mainFrame())
    return;
  PluginChannelHost::Broadcast(
      new PluginHostMsg_DidAbortLoading(render_view_->GetRoutingID()));
#endif
}

void RenderFrameImpl::didCreateScriptContext(blink::WebLocalFrame* frame,
                                             v8::Local<v8::Context> context,
                                             int extension_group,
                                             int world_id) {
  DCHECK(!frame_ || frame_ == frame);

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_,
                    DidCreateScriptContext(context, extension_group, world_id));
}

void RenderFrameImpl::willReleaseScriptContext(blink::WebLocalFrame* frame,
                                               v8::Local<v8::Context> context,
                                               int world_id) {
  DCHECK(!frame_ || frame_ == frame);

  FOR_EACH_OBSERVER(RenderFrameObserver,
                    observers_,
                    WillReleaseScriptContext(context, world_id));
}

void RenderFrameImpl::didChangeScrollOffset(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  // TODO(nasko): Move implementation here. Needed methods:
  // * StartNavStateSyncTimerIfNecessary
  render_view_->didChangeScrollOffset(frame);

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, DidChangeScrollOffset());
}

void RenderFrameImpl::willInsertBody(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  if (!frame->parent()) {
    render_view_->Send(new ViewHostMsg_WillInsertBody(
        render_view_->GetRoutingID()));
  }
}

void RenderFrameImpl::reportFindInPageMatchCount(int request_id,
                                                 int count,
                                                 bool final_update) {
  int active_match_ordinal = -1;  // -1 = don't update active match ordinal
  if (!count)
    active_match_ordinal = 0;

  render_view_->Send(new ViewHostMsg_Find_Reply(
      render_view_->GetRoutingID(), request_id, count,
      gfx::Rect(), active_match_ordinal, final_update));
}

void RenderFrameImpl::reportFindInPageSelection(
    int request_id,
    int active_match_ordinal,
    const blink::WebRect& selection_rect) {
  render_view_->Send(new ViewHostMsg_Find_Reply(
      render_view_->GetRoutingID(), request_id, -1, selection_rect,
      active_match_ordinal, false));
}

void RenderFrameImpl::requestStorageQuota(
    blink::WebLocalFrame* frame,
    blink::WebStorageQuotaType type,
    unsigned long long requested_size,
    blink::WebStorageQuotaCallbacks callbacks) {
  DCHECK(!frame_ || frame_ == frame);
  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (origin.isUnique()) {
    // Unique origins cannot store persistent state.
    callbacks.didFail(blink::WebStorageQuotaErrorAbort);
    return;
  }
  ChildThreadImpl::current()->quota_dispatcher()->RequestStorageQuota(
      render_view_->GetRoutingID(),
      GURL(origin.toString()),
      static_cast<storage::StorageType>(type),
      requested_size,
      QuotaDispatcher::CreateWebStorageQuotaCallbacksWrapper(callbacks));
}

void RenderFrameImpl::willOpenWebSocket(blink::WebSocketHandle* handle) {
  WebSocketBridge* impl = static_cast<WebSocketBridge*>(handle);
  impl->set_render_frame_id(routing_id_);
}

blink::WebGeolocationClient* RenderFrameImpl::geolocationClient() {
  if (!geolocation_dispatcher_)
    geolocation_dispatcher_ = new GeolocationDispatcher(this);
  return geolocation_dispatcher_;
}

blink::WebPresentationClient* RenderFrameImpl::presentationClient() {
  if (!presentation_dispatcher_)
    presentation_dispatcher_ = new PresentationDispatcher(this);
  return presentation_dispatcher_;
}

blink::WebPushClient* RenderFrameImpl::pushClient() {
  if (!push_messaging_dispatcher_)
    push_messaging_dispatcher_ = new PushMessagingDispatcher(this);
  return push_messaging_dispatcher_;
}

void RenderFrameImpl::willStartUsingPeerConnectionHandler(
    blink::WebLocalFrame* frame,
    blink::WebRTCPeerConnectionHandler* handler) {
  DCHECK(!frame_ || frame_ == frame);
#if defined(ENABLE_WEBRTC)
  static_cast<RTCPeerConnectionHandler*>(handler)->associateWithFrame(frame);
#endif
}

blink::WebUserMediaClient* RenderFrameImpl::userMediaClient() {
  if (!web_user_media_client_)
    InitializeUserMediaClient();
  return web_user_media_client_;
}

blink::WebEncryptedMediaClient* RenderFrameImpl::encryptedMediaClient() {
  if (!web_encrypted_media_client_) {
    web_encrypted_media_client_.reset(new media::WebEncryptedMediaClientImpl(
        // base::Unretained(this) is safe because WebEncryptedMediaClientImpl
        // is destructed before |this|, and does not give away ownership of the
        // callback.
        base::Bind(&RenderFrameImpl::AreSecureCodecsSupported,
                   base::Unretained(this)),
        GetCdmFactory(), GetMediaPermission()));
  }
  return web_encrypted_media_client_.get();
}

blink::WebMIDIClient* RenderFrameImpl::webMIDIClient() {
  if (!midi_dispatcher_)
    midi_dispatcher_ = new MidiDispatcher(this);
  return midi_dispatcher_;
}

bool RenderFrameImpl::willCheckAndDispatchMessageEvent(
    blink::WebLocalFrame* source_frame,
    blink::WebFrame* target_frame,
    blink::WebSecurityOrigin target_origin,
    blink::WebDOMMessageEvent event) {
  DCHECK(!frame_ || frame_ == target_frame);

  // Currently, a postMessage that targets a cross-process frame can be plumbed
  // either through this function or RenderFrameProxy::postMessageEvent. This
  // function is used when the target cross-process frame is a top-level frame
  // which has been swapped out.  In that case, the corresponding WebLocalFrame
  // currently remains in the frame tree even in site-per-process mode (see
  // OnSwapOut). RenderFrameProxy::postMessageEvent is used in
  // --site-per-process mode for all other cases.
  //
  // TODO(alexmos, nasko): When swapped-out:// disappears, this should be
  // cleaned up so that RenderFrameProxy::postMessageEvent is the only path for
  // cross-process postMessages.
  if (!is_swapped_out_)
    return false;

  // It is possible to get here on a swapped-out frame without a
  // |render_frame_proxy_|. This happens when:
  // - This process only has one active RenderView and is about to go away
  //   (e.g., due to cross-process navigation).
  // - The top frame has a subframe with an unload handler.
  // - The subframe sends a postMessage to the top-level frame in its unload
  //   handler.
  // See https://crbug.com/475651 for details.  We return false here, since we
  // don't want to deliver the message to the new process in this case.
  if (!render_frame_proxy_)
    return false;

  render_frame_proxy_->postMessageEvent(
      source_frame, render_frame_proxy_->web_frame(), target_origin, event);
  return true;
}

blink::WebString RenderFrameImpl::userAgentOverride(
    blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  if (!render_view_->webview() || !render_view_->webview()->mainFrame() ||
      render_view_->renderer_preferences_.user_agent_override.empty()) {
    return blink::WebString();
  }

  // TODO(nasko): When the top-level frame is remote, there is no WebDataSource
  // associated with it, so the checks below are not valid. Temporarily
  // return early and fix properly as part of https://crbug.com/426555.
  if (render_view_->webview()->mainFrame()->isWebRemoteFrame())
    return blink::WebString();

  // If we're in the middle of committing a load, the data source we need
  // will still be provisional.
  WebFrame* main_frame = render_view_->webview()->mainFrame();
  WebDataSource* data_source = NULL;
  if (main_frame->provisionalDataSource())
    data_source = main_frame->provisionalDataSource();
  else
    data_source = main_frame->dataSource();

  InternalDocumentStateData* internal_data = data_source ?
      InternalDocumentStateData::FromDataSource(data_source) : NULL;
  if (internal_data && internal_data->is_overriding_user_agent())
    return WebString::fromUTF8(
        render_view_->renderer_preferences_.user_agent_override);
  return blink::WebString();
}

blink::WebString RenderFrameImpl::doNotTrackValue(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);
  if (render_view_->renderer_preferences_.enable_do_not_track)
    return WebString::fromUTF8("1");
  return WebString();
}

bool RenderFrameImpl::allowWebGL(blink::WebLocalFrame* frame,
                                 bool default_value) {
  DCHECK(!frame_ || frame_ == frame);
  if (!default_value)
    return false;

  bool blocked = true;
  Send(new FrameHostMsg_Are3DAPIsBlocked(
      routing_id_,
      GURL(frame->top()->securityOrigin().toString()),
      THREE_D_API_TYPE_WEBGL,
      &blocked));
  return !blocked;
}

void RenderFrameImpl::didLoseWebGLContext(blink::WebLocalFrame* frame,
                                          int arb_robustness_status_code) {
  DCHECK(!frame_ || frame_ == frame);
  Send(new FrameHostMsg_DidLose3DContext(
      GURL(frame->top()->securityOrigin().toString()),
      THREE_D_API_TYPE_WEBGL,
      arb_robustness_status_code));
}

blink::WebScreenOrientationClient*
    RenderFrameImpl::webScreenOrientationClient() {
  if (!screen_orientation_dispatcher_)
    screen_orientation_dispatcher_ = new ScreenOrientationDispatcher(this);
  return screen_orientation_dispatcher_;
}

bool RenderFrameImpl::isControlledByServiceWorker(WebDataSource& data_source) {
  ServiceWorkerNetworkProvider* provider =
      ServiceWorkerNetworkProvider::FromDocumentState(
          DocumentState::FromDataSource(&data_source));
  return provider->IsControlledByServiceWorker();
}

int64_t RenderFrameImpl::serviceWorkerID(WebDataSource& data_source) {
  ServiceWorkerNetworkProvider* provider =
      ServiceWorkerNetworkProvider::FromDocumentState(
          DocumentState::FromDataSource(&data_source));
  if (provider->context() && provider->context()->controller())
    return provider->context()->controller()->version_id();
  return kInvalidServiceWorkerVersionId;
}

void RenderFrameImpl::postAccessibilityEvent(const blink::WebAXObject& obj,
                                             blink::WebAXEvent event) {
  HandleWebAccessibilityEvent(obj, event);
}

void RenderFrameImpl::handleAccessibilityFindInPageResult(
    int identifier,
    int match_index,
    const blink::WebAXObject& start_object,
    int start_offset,
    const blink::WebAXObject& end_object,
    int end_offset) {
  if (renderer_accessibility_) {
    renderer_accessibility_->HandleAccessibilityFindInPageResult(
        identifier, match_index, start_object, start_offset,
        end_object, end_offset);
  }
}

void RenderFrameImpl::didChangeManifest(blink::WebLocalFrame* frame) {
  DCHECK(!frame_ || frame_ == frame);

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, DidChangeManifest());
}

bool RenderFrameImpl::enterFullscreen() {
  Send(new FrameHostMsg_ToggleFullscreen(routing_id_, true));
  return true;
}

bool RenderFrameImpl::exitFullscreen() {
  Send(new FrameHostMsg_ToggleFullscreen(routing_id_, false));
  return true;
}

blink::WebPermissionClient* RenderFrameImpl::permissionClient() {
  if (!permission_client_)
    permission_client_.reset(new PermissionDispatcher(GetServiceRegistry()));

  return permission_client_.get();
}

blink::WebAppBannerClient* RenderFrameImpl::appBannerClient() {
  if (!app_banner_client_) {
    app_banner_client_ =
        GetContentClient()->renderer()->CreateAppBannerClient(this);
  }

  return app_banner_client_.get();
}

void RenderFrameImpl::registerProtocolHandler(const WebString& scheme,
                                              const WebURL& url,
                                              const WebString& title) {
  bool user_gesture = WebUserGestureIndicator::isProcessingUserGesture();
  Send(new FrameHostMsg_RegisterProtocolHandler(
      routing_id_,
      base::UTF16ToUTF8(base::StringPiece16(scheme)),
      url,
      title,
      user_gesture));
}

void RenderFrameImpl::unregisterProtocolHandler(const WebString& scheme,
                                                const WebURL& url) {
  bool user_gesture = WebUserGestureIndicator::isProcessingUserGesture();
  Send(new FrameHostMsg_UnregisterProtocolHandler(
      routing_id_,
      base::UTF16ToUTF8(base::StringPiece16(scheme)),
      url,
      user_gesture));
}

blink::WebBluetooth* RenderFrameImpl::bluetooth() {
  if (!bluetooth_) {
    bluetooth_.reset(new WebBluetoothImpl(
        ChildThreadImpl::current()->thread_safe_sender(), routing_id_));
  }

  return bluetooth_.get();
}

blink::WebUSBClient* RenderFrameImpl::usbClient() {
#if !defined(OS_ANDROID)
  if (!usb_client_) {
    usb_client_.reset(new WebUSBClientImpl(GetServiceRegistry()));
  }
#endif
  return usb_client_.get();
}

#if defined(ENABLE_WEBVR)
blink::WebVRClient* RenderFrameImpl::webVRClient() {
  if (!vr_dispatcher_)
    vr_dispatcher_.reset(new VRDispatcher(GetServiceRegistry()));

  return vr_dispatcher_.get();
}
#endif

void RenderFrameImpl::DidPlay(WebMediaPlayer* player) {
  has_played_media_ = true;
  Send(new FrameHostMsg_MediaPlayingNotification(
      routing_id_, reinterpret_cast<int64>(player), player->hasVideo(),
      player->hasAudio(), player->isRemote()));
}

void RenderFrameImpl::DidPause(WebMediaPlayer* player) {
  Send(new FrameHostMsg_MediaPausedNotification(
      routing_id_, reinterpret_cast<int64>(player)));
}

void RenderFrameImpl::PlayerGone(WebMediaPlayer* player) {
  DidPause(player);
}

void RenderFrameImpl::didSerializeDataForFrame(
    const WebURL& frame_url,
    const WebCString& data,
    WebPageSerializerClient::PageSerializationStatus status) {
  bool end_of_data = status == WebPageSerializerClient::CurrentFrameIsFinished;
  Send(new FrameHostMsg_SerializedHtmlWithLocalLinksResponse(
      routing_id_, frame_url, data.data(), end_of_data));
}

void RenderFrameImpl::AddObserver(RenderFrameObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderFrameImpl::RemoveObserver(RenderFrameObserver* observer) {
  observer->RenderFrameGone();
  observers_.RemoveObserver(observer);
}

void RenderFrameImpl::OnStop() {
  DCHECK(frame_);
  frame_->stopLoading();
  if (!frame_->parent())
    FOR_EACH_OBSERVER(RenderViewObserver, render_view_->observers_, OnStop());

  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, OnStop());
}

void RenderFrameImpl::WasHidden() {
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, WasHidden());
}

void RenderFrameImpl::WasShown() {
  // TODO(kenrb): Need to figure out how to do this better. Should
  // VisibilityState remain a page-level concept or move to frames?
  // The semantics of 'Show' might have to change here.
  // TODO(avi): This DCHECK is to track cleanup for https://crbug.com/545684
  DCHECK_IMPLIES(IsMainFrame(), render_widget_.get() == render_view_.get())
      << "The main render frame is no longer reusing the RenderView as its "
      << "RenderWidget!";
  if (render_widget_ && render_view_.get() != render_widget_.get()) {
    static_cast<blink::WebFrameWidget*>(render_widget_->webwidget())->
        setVisibilityState(blink::WebPageVisibilityStateVisible, false);
  }
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, WasShown());
}

void RenderFrameImpl::WidgetWillClose() {
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, WidgetWillClose());
}

bool RenderFrameImpl::IsMainFrame() {
  return is_main_frame_;
}

bool RenderFrameImpl::IsHidden() {
  return GetRenderWidget()->is_hidden();
}

// Tell the embedding application that the URL of the active page has changed.
void RenderFrameImpl::SendDidCommitProvisionalLoad(
    blink::WebFrame* frame,
    blink::WebHistoryCommitType commit_type,
    const blink::WebHistoryItem& item) {
  DCHECK(!frame_ || frame_ == frame);
  WebDataSource* ds = frame->dataSource();
  DCHECK(ds);

  const WebURLRequest& request = ds->request();
  const WebURLResponse& response = ds->response();

  DocumentState* document_state = DocumentState::FromDataSource(ds);
  NavigationStateImpl* navigation_state =
      static_cast<NavigationStateImpl*>(document_state->navigation_state());
  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.http_status_code = response.httpStatusCode();
  params.url_is_unreachable = ds->hasUnreachableURL();
  params.is_post = false;
  params.intended_as_new_entry =
      navigation_state->request_params().intended_as_new_entry;
  params.did_create_new_entry = commit_type == blink::WebStandardCommit;
  params.post_id = -1;
  params.page_id = render_view_->page_id_;
  params.nav_entry_id = navigation_state->request_params().nav_entry_id;
  // We need to track the RenderViewHost routing_id because of downstream
  // dependencies (crbug.com/392171 DownloadRequestHandle, SaveFileManager,
  // ResourceDispatcherHostImpl, MediaStreamUIProxy,
  // SpeechRecognitionDispatcherHost and possibly others). They look up the view
  // based on the ID stored in the resource requests. Once those dependencies
  // are unwound or moved to RenderFrameHost (crbug.com/304341) we can move the
  // client to be based on the routing_id of the RenderFrameHost.
  params.render_view_routing_id = render_view_->routing_id();
  params.socket_address.set_host(response.remoteIPAddress().utf8());
  params.socket_address.set_port(response.remotePort());
  WebURLResponseExtraDataImpl* extra_data = GetExtraDataFromResponse(response);
  if (extra_data)
    params.was_fetched_via_proxy = extra_data->was_fetched_via_proxy();
  params.was_within_same_page = navigation_state->WasWithinSamePage();
  params.security_info = response.securityInfo();

  // Set the URL to be displayed in the browser UI to the user.
  params.url = GetLoadingUrl();
  DCHECK(!is_swapped_out_ || params.url == GURL(kSwappedOutURL));

  // Set the origin of the frame.  This will be replicated to the corresponding
  // RenderFrameProxies in other processes.
  // TODO(alexmos): Origins for URLs with non-standard schemes are excluded due
  // to https://crbug.com/439608 and will be replicated as unique origins.
  if (!is_swapped_out_) {
    std::string scheme = frame->document().securityOrigin().protocol().utf8();
    if (url::IsStandard(scheme.c_str(),
                        url::Component(0, static_cast<int>(scheme.length())))) {
      params.origin = frame->document().securityOrigin();
    }
  }

  if (frame->document().baseURL() != params.url)
    params.base_url = frame->document().baseURL();

  GetRedirectChain(ds, &params.redirects);
  params.should_update_history = !ds->hasUnreachableURL() &&
      !response.isMultipartPayload() && (response.httpStatusCode() != 404);

  params.searchable_form_url = internal_data->searchable_form_url();
  params.searchable_form_encoding = internal_data->searchable_form_encoding();

  params.gesture = render_view_->navigation_gesture_;
  render_view_->navigation_gesture_ = NavigationGestureUnknown;

  // Make navigation state a part of the DidCommitProvisionalLoad message so
  // that committed entry has it at all times.
  HistoryEntry* entry = render_view_->history_controller()->GetCurrentEntry();
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    if (entry)
      params.page_state = HistoryEntryToPageState(entry);
    else
      params.page_state = PageState::CreateFromURL(request.url());
  } else {
    // In --site-per-process, just send a single HistoryItem for this frame,
    // rather than the whole tree.  It will be stored in the corresponding
    // FrameNavigationEntry.
    params.page_state = SingleHistoryItemToPageState(item);
  }
  params.item_sequence_number = item.itemSequenceNumber();
  params.document_sequence_number = item.documentSequenceNumber();

  if (!frame->parent()) {
    // Top-level navigation.

    // Reset the zoom limits in case a plugin had changed them previously. This
    // will also call us back which will cause us to send a message to
    // update WebContentsImpl.
    render_view_->webview()->zoomLimitsChanged(
        ZoomFactorToZoomLevel(kMinimumZoomFactor),
        ZoomFactorToZoomLevel(kMaximumZoomFactor));

    // Set zoom level, but don't do it for full-page plugin since they don't use
    // the same zoom settings.
    HostZoomLevels::iterator host_zoom =
        render_view_->host_zoom_levels_.find(GURL(request.url()));
    if (render_view_->webview()->mainFrame()->isWebLocalFrame() &&
        render_view_->webview()->mainFrame()->document().isPluginDocument()) {
      // Reset the zoom levels for plugins.
      render_view_->webview()->setZoomLevel(0);
    } else {
      // If the zoom level is not found, then do nothing. In-page navigation
      // relies on not changing the zoom level in this case.
      if (host_zoom != render_view_->host_zoom_levels_.end())
        render_view_->webview()->setZoomLevel(host_zoom->second);
    }

    if (host_zoom != render_view_->host_zoom_levels_.end()) {
      // This zoom level was merely recorded transiently for this load.  We can
      // erase it now.  If at some point we reload this page, the browser will
      // send us a new, up-to-date zoom level.
      render_view_->host_zoom_levels_.erase(host_zoom);
    }

    // Update contents MIME type for main frame.
    params.contents_mime_type = ds->response().mimeType().utf8();

    params.transition = navigation_state->GetTransitionType();
    if (!ui::PageTransitionIsMainFrame(params.transition)) {
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
      params.transition = ui::PAGE_TRANSITION_LINK;
    }

    // If the page contained a client redirect (meta refresh, document.loc...),
    // set the referrer and transition appropriately.
    if (ds->isClientRedirect()) {
      params.referrer =
          Referrer(params.redirects[0], ds->request().referrerPolicy());
      params.transition = ui::PageTransitionFromInt(
          params.transition | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
    } else {
      params.referrer = RenderViewImpl::GetReferrerFromRequest(
          frame, ds->request());
    }

    base::string16 method = request.httpMethod();
    if (base::EqualsASCII(method, "POST")) {
      params.is_post = true;
      params.post_id = ExtractPostId(entry);
    }

    // Send the user agent override back.
    params.is_overriding_user_agent = internal_data->is_overriding_user_agent();

    // Track the URL of the original request.  We use the first entry of the
    // redirect chain if it exists because the chain may have started in another
    // process.
    params.original_request_url = GetOriginalRequestURL(ds);

    params.history_list_was_cleared =
        navigation_state->request_params().should_clear_history_list;

    params.report_type = static_cast<FrameMsg_UILoadMetricsReportType::Value>(
        frame->dataSource()->request().inputPerfMetricReportPolicy());
    params.ui_timestamp = base::TimeTicks() + base::TimeDelta::FromSecondsD(
        frame->dataSource()->request().uiStartTime());

    // Save some histogram data so we can compute the average memory used per
    // page load of the glyphs.
    UMA_HISTOGRAM_COUNTS_10000("Memory.GlyphPagesPerLoad",
                               blink::WebGlyphCache::pageCount());

    // This message needs to be sent before any of allowScripts(),
    // allowImages(), allowPlugins() is called for the new page, so that when
    // these functions send a ViewHostMsg_ContentBlocked message, it arrives
    // after the FrameHostMsg_DidCommitProvisionalLoad message.
    Send(new FrameHostMsg_DidCommitProvisionalLoad(routing_id_, params));
  } else {
    // Subframe navigation: the type depends on whether this navigation
    // generated a new session history entry. When they do generate a session
    // history entry, it means the user initiated the navigation and we should
    // mark it as such.
    if (commit_type == blink::WebStandardCommit)
      params.transition = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
    else
      params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;

    DCHECK(!navigation_state->request_params().should_clear_history_list);
    params.history_list_was_cleared = false;
    params.report_type = FrameMsg_UILoadMetricsReportType::NO_REPORT;

    // Don't send this message while the subframe is swapped out.
    if (!is_swapped_out())
      Send(new FrameHostMsg_DidCommitProvisionalLoad(routing_id_, params));
  }

  // If we end up reusing this WebRequest (for example, due to a #ref click),
  // we don't want the transition type to persist.  Just clear it.
  navigation_state->set_transition_type(ui::PAGE_TRANSITION_LINK);
}

void RenderFrameImpl::didStartLoading(bool to_different_document) {
  TRACE_EVENT1("navigation", "RenderFrameImpl::didStartLoading",
               "id", routing_id_);
  render_view_->FrameDidStartLoading(frame_);
  Send(new FrameHostMsg_DidStartLoading(routing_id_, to_different_document));
}

void RenderFrameImpl::didStopLoading() {
  TRACE_EVENT1("navigation", "RenderFrameImpl::didStopLoading",
               "id", routing_id_);
  render_view_->FrameDidStopLoading(frame_);
  Send(new FrameHostMsg_DidStopLoading(routing_id_));
}

void RenderFrameImpl::didChangeLoadProgress(double load_progress) {
  Send(new FrameHostMsg_DidChangeLoadProgress(routing_id_, load_progress));
}

void RenderFrameImpl::HandleWebAccessibilityEvent(
    const blink::WebAXObject& obj, blink::WebAXEvent event) {
  if (renderer_accessibility_)
    renderer_accessibility_->HandleWebAccessibilityEvent(obj, event);
}

void RenderFrameImpl::FocusedNodeChanged(const WebNode& node) {
  FOR_EACH_OBSERVER(RenderFrameObserver, observers_, FocusedNodeChanged(node));
}

void RenderFrameImpl::FocusedNodeChangedForAccessibility(const WebNode& node) {
  if (renderer_accessibility())
    renderer_accessibility()->AccessibilityFocusedNodeChanged(node);
}

// PlzNavigate
void RenderFrameImpl::OnCommitNavigation(
    const ResourceResponseHead& response,
    const GURL& stream_url,
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  // This will override the url requested by the WebURLLoader, as well as
  // provide it with the response to the request.
  scoped_ptr<StreamOverrideParameters> stream_override(
      new StreamOverrideParameters());
  stream_override->stream_url = stream_url;
  stream_override->response = response;

  NavigateInternal(common_params, StartNavigationParams(), request_params,
                   stream_override.Pass());
}

void RenderFrameImpl::OnFailedNavigation(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    bool has_stale_copy_in_cache,
    int error_code) {
  bool is_reload = IsReload(common_params.navigation_type);
  bool is_history_navigation = request_params.page_state.IsValid();
  WebURLRequest::CachePolicy cache_policy =
      WebURLRequest::UseProtocolCachePolicy;
  RenderFrameImpl::PrepareRenderViewForNavigation(
      common_params.url, request_params, &is_reload, &cache_policy);

  GetContentClient()->SetActiveURL(common_params.url);

  pending_navigation_params_.reset(new NavigationParams(
      common_params, StartNavigationParams(), request_params));

  // Inform the browser of the start of the provisional load. This is needed so
  // that the load is properly tracked by the WebNavigation API.
  Send(new FrameHostMsg_DidStartProvisionalLoadForFrame(
      routing_id_, common_params.url));

  // Send the provisional load failure.
  blink::WebURLError error =
      CreateWebURLError(common_params.url, has_stale_copy_in_cache, error_code);
  WebURLRequest failed_request = CreateURLRequestForNavigation(
      common_params, scoped_ptr<StreamOverrideParameters>(),
      frame_->isViewSourceModeEnabled());
  SendFailedProvisionalLoad(failed_request, error, frame_);

  if (!ShouldDisplayErrorPageForFailedLoad(error_code, common_params.url)) {
    // TODO(avi): Remove this; we shouldn't ever be dropping navigations.
    // http://crbug.com/501960
    Send(new FrameHostMsg_DidDropNavigation(routing_id_));
    return;
  }

  // Make sure errors are not shown in view source mode.
  frame_->enableViewSourceMode(false);

  // Replace the current history entry in reloads, history navigations and loads
  // of the same url. This corresponds to Blink's notion of a standard
  // commit.
  // TODO(clamy): see if initial commits in subframes should be handled
  // separately.
  bool replace = is_reload || is_history_navigation ||
                 common_params.url == GetLoadingUrl();
  LoadNavigationErrorPage(failed_request, error, replace);
}

WebNavigationPolicy RenderFrameImpl::decidePolicyForNavigation(
    const NavigationPolicyInfo& info) {
  // A content initiated navigation may have originated from a link-click,
  // script, drag-n-drop operation, etc.
  // info.extraData is only non-null if this is a redirect. Use the extraData
  // initiation information for redirects, and check pending_navigation_params_
  // otherwise.
  bool is_content_initiated =
      info.extraData
          ? static_cast<DocumentState*>(info.extraData)
                ->navigation_state()
                ->IsContentInitiated()
          : !IsBrowserInitiated(pending_navigation_params_.get());
  bool is_redirect =
      info.extraData ||
      (pending_navigation_params_ &&
       !pending_navigation_params_->request_params.redirects.empty());

#ifdef OS_ANDROID
  // The handlenavigation API is deprecated and will be removed once
  // crbug.com/325351 is resolved.
  if (info.urlRequest.url() != GURL(kSwappedOutURL) &&
      GetContentClient()->renderer()->HandleNavigation(
          this, is_content_initiated, render_view_->opener_id_, frame_,
          info.urlRequest, info.navigationType, info.defaultPolicy,
          is_redirect)) {
    return blink::WebNavigationPolicyIgnore;
  }
#endif

  Referrer referrer(
      RenderViewImpl::GetReferrerFromRequest(frame_, info.urlRequest));

  // TODO(nick): Is consulting |is_main_frame| here correct?
  if (SiteIsolationPolicy::IsSwappedOutStateForbidden() && !is_main_frame_) {
    // There's no reason to ignore navigations on subframes, since the swap out
    // logic no longer applies.
  } else {
    if (is_swapped_out_) {
      if (info.urlRequest.url() != GURL(kSwappedOutURL)) {
        // Targeted links may try to navigate a swapped out frame.  Allow the
        // browser process to navigate the tab instead.  Note that it is also
        // possible for non-targeted navigations (from this view) to arrive
        // here just after we are swapped out.  It's ok to send them to the
        // browser, as long as they're for the top level frame.
        // TODO(creis): Ensure this supports targeted form submissions when
        // fixing http://crbug.com/101395.
        if (frame_->parent() == NULL) {
          OpenURL(info.urlRequest.url(), referrer, info.defaultPolicy,
                  info.replacesCurrentHistoryItem);
          return blink::WebNavigationPolicyIgnore;  // Suppress the load here.
        }

        // We should otherwise ignore in-process iframe navigations, if they
        // arrive just after we are swapped out.
        return blink::WebNavigationPolicyIgnore;
      }

      // Allow kSwappedOutURL to complete.
      return info.defaultPolicy;
    }
  }

  // Webkit is asking whether to navigate to a new URL.
  // This is fine normally, except if we're showing UI from one security
  // context and they're trying to navigate to a different context.
  const GURL& url = info.urlRequest.url();

  // If the browser is interested, then give it a chance to look at the request.
  if (is_content_initiated && IsTopLevelNavigation(frame_) &&
      render_view_->renderer_preferences_
          .browser_handles_all_top_level_requests) {
    OpenURL(url, referrer, info.defaultPolicy, info.replacesCurrentHistoryItem);
    return blink::WebNavigationPolicyIgnore;  // Suppress the load here.
  }

  // Use the frame's original request's URL rather than the document's URL for
  // subsequent checks.  For a popup, the document's URL may become the opener
  // window's URL if the opener has called document.write().
  // See http://crbug.com/93517.
  GURL old_url(frame_->dataSource()->request().url());

  // Detect when we're crossing a permission-based boundary (e.g. into or out of
  // an extension or app origin, leaving a WebUI page, etc). We only care about
  // top-level navigations (not iframes). But we sometimes navigate to
  // about:blank to clear a tab, and we want to still allow that.
  //
  // Note: this is known to break POST submissions when crossing process
  // boundaries until http://crbug.com/101395 is fixed.  This is better for
  // security than loading a WebUI, extension or app page in the wrong process.
  // POST requests don't work because this mechanism does not preserve form
  // POST data. We will need to send the request's httpBody data up to the
  // browser process, and issue a special POST navigation in WebKit (via
  // FrameLoader::loadFrameRequest). See ResourceDispatcher and WebURLLoaderImpl
  // for examples of how to send the httpBody data.
  if (!frame_->parent() && is_content_initiated &&
      !url.SchemeIs(url::kAboutScheme)) {
    bool send_referrer = false;

    // All navigations to or from WebUI URLs or within WebUI-enabled
    // RenderProcesses must be handled by the browser process so that the
    // correct bindings and data sources can be registered.
    // Similarly, navigations to view-source URLs or within ViewSource mode
    // must be handled by the browser process (except for reloads - those are
    // safe to leave within the renderer).
    // Lastly, access to file:// URLs from non-file:// URL pages must be
    // handled by the browser so that ordinary renderer processes don't get
    // blessed with file permissions.
    int cumulative_bindings = RenderProcess::current()->GetEnabledBindings();
    bool is_initial_navigation = render_view_->history_list_length_ == 0;
    bool should_fork = HasWebUIScheme(url) || HasWebUIScheme(old_url) ||
                       (cumulative_bindings & BINDINGS_POLICY_WEB_UI) ||
                       url.SchemeIs(kViewSourceScheme) ||
                       (frame_->isViewSourceModeEnabled() &&
                        info.navigationType != blink::WebNavigationTypeReload);

    if (!should_fork && url.SchemeIs(url::kFileScheme)) {
      // Fork non-file to file opens.  Check the opener URL if this is the
      // initial navigation in a newly opened window.
      GURL source_url(old_url);
      if (is_initial_navigation && source_url.is_empty() && frame_->opener())
        source_url = frame_->opener()->top()->document().url();
      DCHECK(!source_url.is_empty());
      should_fork = !source_url.SchemeIs(url::kFileScheme);
    }

    if (!should_fork) {
      // Give the embedder a chance.
      should_fork = GetContentClient()->renderer()->ShouldFork(
          frame_, url, info.urlRequest.httpMethod().utf8(),
          is_initial_navigation, is_redirect, &send_referrer);
    }

    if (should_fork) {
      OpenURL(url, send_referrer ? referrer : Referrer(), info.defaultPolicy,
              info.replacesCurrentHistoryItem);
      return blink::WebNavigationPolicyIgnore;  // Suppress the load here.
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
      old_url == GURL(url::kAboutBlankURL) &&
      // Must be the first real navigation of the tab.
      render_view_->historyBackListCount() < 1 &&
      render_view_->historyForwardListCount() < 1 &&
      // The parent page must have set the child's window.opener to null before
      // redirecting to the desired URL.
      frame_->opener() == NULL &&
      // Must be a top-level frame.
      frame_->parent() == NULL &&
      // Must not have issued the request from this page.
      is_content_initiated &&
      // Must be targeted at the current tab.
      info.defaultPolicy == blink::WebNavigationPolicyCurrentTab &&
      // Must be a JavaScript navigation, which appears as "other".
      info.navigationType == blink::WebNavigationTypeOther;

  if (is_fork) {
    // Open the URL via the browser, not via WebKit.
    OpenURL(url, Referrer(), info.defaultPolicy,
            info.replacesCurrentHistoryItem);
    return blink::WebNavigationPolicyIgnore;
  }

  // PlzNavigate: if the navigation is not synchronous, send it to the browser.
  // This includes navigations with no request being sent to the network stack.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation) &&
      info.urlRequest.checkForBrowserSideNavigation() &&
      ShouldMakeNetworkRequestForURL(url)) {
    BeginNavigation(&info.urlRequest);
    return blink::WebNavigationPolicyIgnore;
  }

  return info.defaultPolicy;
}

void RenderFrameImpl::OnGetSavableResourceLinks() {
  std::vector<GURL> resources_list;
  std::vector<GURL> referrer_urls_list;
  std::vector<blink::WebReferrerPolicy> referrer_policies_list;
  SavableResourcesResult result(&resources_list, &referrer_urls_list,
                                &referrer_policies_list);

  if (!GetSavableResourceLinksForFrame(
          frame_, &result, const_cast<const char**>(GetSavableSchemes()))) {
    Send(new FrameHostMsg_SavableResourceLinksError(routing_id_));
    return;
  }

  std::vector<Referrer> referrers_list;
  CHECK_EQ(referrer_urls_list.size(), referrer_policies_list.size());
  for (unsigned i = 0; i < referrer_urls_list.size(); ++i) {
    referrers_list.push_back(
        Referrer(referrer_urls_list[i], referrer_policies_list[i]));
  }

  Send(new FrameHostMsg_SavableResourceLinksResponse(
      routing_id_, frame_->document().url(), resources_list, referrers_list));
}

void RenderFrameImpl::OnGetSerializedHtmlWithLocalLinks(
    std::vector<GURL> original_urls,
    std::vector<base::FilePath> equivalent_local_paths,
    base::FilePath local_directory_path) {
  // Only DCHECK, since the message comes from the trusted browser process.
  DCHECK(original_urls.size() == equivalent_local_paths.size());

  // Convert std::vector of GURLs to WebVector<WebURL>
  WebVector<WebURL> weburl_links(original_urls);

  // Convert std::vector of base::FilePath to WebVector<WebString>
  WebVector<WebString> webstring_paths(equivalent_local_paths.size());
  for (size_t i = 0; i < equivalent_local_paths.size(); i++)
    webstring_paths[i] = equivalent_local_paths[i].AsUTF16Unsafe();

  // Serialize the frame (without recursing into subframes).
  WebPageSerializer::serialize(GetWebFrame(),
                               this,   // WebPageSerializerClient.
                               weburl_links, webstring_paths,
                               local_directory_path.AsUTF16Unsafe());
}

void RenderFrameImpl::OpenURL(const GURL& url,
                              const Referrer& referrer,
                              WebNavigationPolicy policy,
                              bool should_replace_current_entry) {
  FrameHostMsg_OpenURL_Params params;
  params.url = url;
  params.referrer = referrer;
  params.disposition = RenderViewImpl::NavigationPolicyToDisposition(policy);

  if (IsBrowserInitiated(pending_navigation_params_.get())) {
    // This is necessary to preserve the should_replace_current_entry value on
    // cross-process redirects, in the event it was set by a previous process.
    //
    // TODO(davidben): Avoid this awkward duplication of state. See comment on
    // NavigationState::should_replace_current_entry().
    params.should_replace_current_entry =
        pending_navigation_params_->common_params.should_replace_current_entry;
  } else {
    params.should_replace_current_entry =
        should_replace_current_entry && render_view_->history_list_length_;
  }
  params.user_gesture = WebUserGestureIndicator::isProcessingUserGesture();
  if (GetContentClient()->renderer()->AllowPopup())
    params.user_gesture = true;

  if (policy == blink::WebNavigationPolicyNewBackgroundTab ||
      policy == blink::WebNavigationPolicyNewForegroundTab ||
      policy == blink::WebNavigationPolicyNewWindow ||
      policy == blink::WebNavigationPolicyNewPopup) {
    WebUserGestureIndicator::consumeUserGesture();
  }

  Send(new FrameHostMsg_OpenURL(routing_id_, params));
}

void RenderFrameImpl::NavigateInternal(
    const CommonNavigationParams& common_params,
    const StartNavigationParams& start_params,
    const RequestNavigationParams& request_params,
    scoped_ptr<StreamOverrideParameters> stream_params) {
  bool browser_side_navigation =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserSideNavigation);
  bool is_reload = IsReload(common_params.navigation_type);
  bool is_history_navigation = request_params.page_state.IsValid();
  WebURLRequest::CachePolicy cache_policy =
      WebURLRequest::UseProtocolCachePolicy;
  RenderFrameImpl::PrepareRenderViewForNavigation(
      common_params.url, request_params, &is_reload, &cache_policy);

  GetContentClient()->SetActiveURL(common_params.url);

  // If this frame isn't in the same process as the main frame, it may naively
  // assume that this is the first navigation in the iframe, but this may not
  // actually be the case. Inform the frame's state machine if this frame has
  // already committed other loads.
  if (request_params.has_committed_real_load && frame_->parent())
    frame_->setCommittedFirstRealLoad();

  if (is_reload && !render_view_->history_controller()->GetCurrentEntry()) {
    // We cannot reload if we do not have any history state.  This happens, for
    // example, when recovering from a crash.
    is_reload = false;
    cache_policy = WebURLRequest::ReloadIgnoringCacheData;
  }

  pending_navigation_params_.reset(
      new NavigationParams(common_params, start_params, request_params));

  // Create parameters for a standard navigation.
  blink::WebFrameLoadType load_type = blink::WebFrameLoadType::Standard;
  bool should_load_request = false;
  WebHistoryItem item_for_history_navigation;
  WebURLRequest request = CreateURLRequestForNavigation(
      common_params, stream_params.Pass(), frame_->isViewSourceModeEnabled());
#if defined(OS_ANDROID)
  request.setHasUserGesture(start_params.has_user_gesture);
#endif

  // PlzNavigate: Make sure that Blink's loader will not try to use browser side
  // navigation for this request (since it already went to the browser).
  if (browser_side_navigation)
    request.setCheckForBrowserSideNavigation(false);

  // If we are reloading, then use the history state of the current frame.
  // Otherwise, if we have history state, then we need to navigate to it, which
  // corresponds to a back/forward navigation event. Update the parameters
  // depending on the navigation type.
  if (is_reload) {
    bool ignore_cache = (common_params.navigation_type ==
                         FrameMsg_Navigate_Type::RELOAD_IGNORING_CACHE);
    load_type = ignore_cache ? blink::WebFrameLoadType::ReloadFromOrigin
                             : blink::WebFrameLoadType::Reload;

    if (!browser_side_navigation) {
      const GURL override_url =
          (common_params.navigation_type ==
           FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL)
              ? common_params.url
              : GURL();
      request = frame_->requestForReload(load_type, override_url);
    }
    should_load_request = true;
  } else if (is_history_navigation) {
    // We must know the page ID of the page we are navigating back to.
    DCHECK_NE(request_params.page_id, -1);
    // We must know the nav entry ID of the page we are navigating back to,
    // which should be the case because history navigations are routed via the
    // browser.
    DCHECK_NE(0, request_params.nav_entry_id);
    scoped_ptr<HistoryEntry> entry =
        PageStateToHistoryEntry(request_params.page_state);
    if (entry) {
      // Ensure we didn't save the swapped out URL in UpdateState, since the
      // browser should never be telling us to navigate to swappedout://.
      CHECK(entry->root().urlString() != WebString::fromUTF8(kSwappedOutURL));

      if (!browser_side_navigation) {
        scoped_ptr<NavigationParams> navigation_params(
            new NavigationParams(*pending_navigation_params_.get()));
        if (!SiteIsolationPolicy::UseSubframeNavigationEntries()) {
          // By default, tell the HistoryController to go the deserialized
          // HistoryEntry.  This only works if all frames are in the same
          // process.
          DCHECK(!frame_->parent());
          render_view_->history_controller()->GoToEntry(
              frame_, entry.Pass(), navigation_params.Pass(), cache_policy);
        } else {
          // In --site-per-process, the browser process sends a single
          // WebHistoryItem destined for this frame.
          // TODO(creis): Change PageState to FrameState.  In the meantime, we
          // store the relevant frame's WebHistoryItem in the root of the
          // PageState.
          SetPendingNavigationParams(navigation_params.Pass());
          blink::WebHistoryItem history_item = entry->root();
          blink::WebHistoryLoadType load_type =
              request_params.is_same_document_history_load
                  ? blink::WebHistorySameDocumentLoad
                  : blink::WebHistoryDifferentDocumentLoad;

          // Let the history controller know the provisional entry, since it is
          // used at commit time.  Otherwise skip GoToEntry and navigate the
          // frame directly.
          // TODO(creis): Consider cloning the current entry to handle subframe
          // cases.  Changes to SendUpdateState might affect this.
          render_view_->history_controller()->set_provisional_entry(
              entry.Pass());
          WebURLRequest request =
              frame_->requestFromHistoryItem(history_item, cache_policy);
          frame_->load(request, blink::WebFrameLoadType::BackForward,
                       history_item, load_type);
        }
      } else {
        // TODO(clamy): this should be set to the HistoryItem sent by the
        // browser once the HistoryController has moved to the browser.
        // TODO(clamy): distinguish between different document and same document
        // loads.
        // TODO(clamy): update this for subframes history loads.
        item_for_history_navigation =
            entry->GetHistoryNodeForFrame(this)->item();
        load_type = blink::WebFrameLoadType::BackForward;
        should_load_request = true;
      }
    }
  } else {
    // Navigate to the given URL.
    if (!start_params.extra_headers.empty() && !browser_side_navigation) {
      for (net::HttpUtil::HeadersIterator i(start_params.extra_headers.begin(),
                                            start_params.extra_headers.end(),
                                            "\n");
           i.GetNext();) {
        request.addHTTPHeaderField(WebString::fromUTF8(i.name()),
                                   WebString::fromUTF8(i.values()));
      }
    }

    if (start_params.is_post && !browser_side_navigation) {
      request.setHTTPMethod(WebString::fromUTF8("POST"));

      // Set post data.
      WebHTTPBody http_body;
      http_body.initialize();
      const char* data = nullptr;
      if (start_params.browser_initiated_post_data.size()) {
        data = reinterpret_cast<const char*>(
            &start_params.browser_initiated_post_data.front());
      }
      http_body.appendData(
          WebData(data, start_params.browser_initiated_post_data.size()));
      request.setHTTPBody(http_body);
    }

    // A session history navigation should have been accompanied by state.
    CHECK_EQ(request_params.page_id, -1);

    should_load_request = true;
  }

  if (should_load_request) {
    // Record this before starting the load. We need a lower bound of this
    // time to sanitize the navigationStart override set below.
    base::TimeTicks renderer_navigation_start = base::TimeTicks::Now();

    // Perform a navigation to a data url if needed.
    if (!common_params.base_url_for_data_url.is_empty() ||
        (browser_side_navigation &&
         common_params.url.SchemeIs(url::kDataScheme))) {
      LoadDataURL(common_params, frame_);
    } else {
      // Load the request.
      frame_->toWebLocalFrame()->load(request, load_type,
                                      item_for_history_navigation);
    }

    if (load_type == blink::WebFrameLoadType::Standard) {
      UpdateFrameNavigationTiming(frame_,
                                  request_params.browser_navigation_start,
                                  renderer_navigation_start);
    }
  }

  // In case LoadRequest failed before didCreateDataSource was called.
  pending_navigation_params_.reset();
}

void RenderFrameImpl::UpdateEncoding(WebFrame* frame,
                                     const std::string& encoding_name) {
  // Only update main frame's encoding_name.
  if (!frame->parent())
    Send(new FrameHostMsg_UpdateEncoding(routing_id_, encoding_name));
}

void RenderFrameImpl::SyncSelectionIfRequired() {
  base::string16 text;
  size_t offset;
  gfx::Range range;
#if defined(ENABLE_PLUGINS)
  if (render_view_->focused_pepper_plugin_) {
    render_view_->focused_pepper_plugin_->GetSurroundingText(&text, &range);
    offset = 0;  // Pepper API does not support offset reporting.
    // TODO(kinaba): cut as needed.
  } else
#endif
  {
    size_t location, length;
    if (!GetRenderWidget()->webwidget()->caretOrSelectionRange(
            &location, &length)) {
      return;
    }

    range = gfx::Range(location, location + length);

    if (GetRenderWidget()->webwidget()->textInputType() !=
        blink::WebTextInputTypeNone) {
      // If current focused element is editable, we will send 100 more chars
      // before and after selection. It is for input method surrounding text
      // feature.
      if (location > kExtraCharsBeforeAndAfterSelection)
        offset = location - kExtraCharsBeforeAndAfterSelection;
      else
        offset = 0;
      length = location + length - offset + kExtraCharsBeforeAndAfterSelection;
      WebRange webrange = WebRange::fromDocumentRange(frame_, offset, length);
      if (!webrange.isNull())
        text = webrange.toPlainText();
    } else {
      offset = location;
      text = frame_->selectionAsText();
      // http://crbug.com/101435
      // In some case, frame->selectionAsText() returned text's length is not
      // equal to the length returned from webwidget()->caretOrSelectionRange().
      // So we have to set the range according to text.length().
      range.set_end(range.start() + text.length());
    }
  }

  // Sometimes we get repeated didChangeSelection calls from webkit when
  // the selection hasn't actually changed. We don't want to report these
  // because it will cause us to continually claim the X clipboard.
  if (selection_text_offset_ != offset ||
      selection_range_ != range ||
      selection_text_ != text) {
    selection_text_ = text;
    selection_text_offset_ = offset;
    selection_range_ = range;
    SetSelectedText(text, offset, range);
  }
  GetRenderWidget()->UpdateSelectionBounds();
}

void RenderFrameImpl::InitializeUserMediaClient() {
  if (!RenderThreadImpl::current())  // Will be NULL during unit tests.
    return;

#if defined(ENABLE_WEBRTC)
  DCHECK(!web_user_media_client_);
  web_user_media_client_ = new UserMediaClientImpl(
      this,
      RenderThreadImpl::current()->GetPeerConnectionDependencyFactory(),
      make_scoped_ptr(new MediaStreamDispatcher(this)).Pass());
#endif
}

WebMediaPlayer* RenderFrameImpl::CreateWebMediaPlayerForMediaStream(
    WebMediaPlayerClient* client) {
#if defined(ENABLE_WEBRTC)
#if defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
  const bool found_neon =
      (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0;
  UMA_HISTOGRAM_BOOLEAN("Platform.WebRtcNEONFound", found_neon);
#endif  // defined(OS_ANDROID) && defined(ARCH_CPU_ARMEL)
  RenderThreadImpl* const render_thread = RenderThreadImpl::current();

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner =
      render_thread->compositor_task_runner();
  if (!compositor_task_runner.get())
    compositor_task_runner = base::MessageLoop::current()->task_runner();

  return new WebMediaPlayerMS(
      frame_, client, weak_factory_.GetWeakPtr(), new RenderMediaLog(),
      CreateRendererFactory(), compositor_task_runner,
      render_thread->GetMediaThreadTaskRunner(),
      render_thread->GetWorkerTaskRunner(), render_thread->GetGpuFactories());
#else
  return NULL;
#endif  // defined(ENABLE_WEBRTC)
}

scoped_ptr<MediaStreamRendererFactory>
RenderFrameImpl::CreateRendererFactory() {
  scoped_ptr<MediaStreamRendererFactory> factory =
      GetContentClient()->renderer()->CreateMediaStreamRendererFactory();
  if (factory.get())
    return factory.Pass();
#if defined(ENABLE_WEBRTC)
  return scoped_ptr<MediaStreamRendererFactory>(
      new MediaStreamRendererFactoryImpl());
#else
  return scoped_ptr<MediaStreamRendererFactory>(
      static_cast<MediaStreamRendererFactory*>(NULL));
#endif
}

void RenderFrameImpl::PrepareRenderViewForNavigation(
    const GURL& url,
    const RequestNavigationParams& request_params,
    bool* is_reload,
    WebURLRequest::CachePolicy* cache_policy) {
  DCHECK(render_view_->webview());

  MaybeHandleDebugURL(url);

  FOR_EACH_OBSERVER(
      RenderViewObserver, render_view_->observers_, Navigate(url));

  render_view_->history_list_offset_ =
      request_params.current_history_list_offset;
  render_view_->history_list_length_ =
      request_params.current_history_list_length;
  if (request_params.should_clear_history_list) {
    CHECK_EQ(-1, render_view_->history_list_offset_);
    CHECK_EQ(0, render_view_->history_list_length_);
  }

  if (!is_swapped_out_ || frame_->parent())
    return;

  // This is a swapped out main frame, so swap the renderer back in.
  // We marked the view as hidden when swapping the view out, so be sure to
  // reset the visibility state before navigating to the new URL.
  render_view_->webview()->setVisibilityState(
      render_view_->visibilityState(), false);

  // If this is an attempt to reload while we are swapped out, we should not
  // reload swappedout://, but the previous page, which is stored in
  // params.state.  Setting is_reload to false will treat this like a back
  // navigation to accomplish that.
  *is_reload = false;
  *cache_policy = WebURLRequest::ReloadIgnoringCacheData;

  // We refresh timezone when a view is swapped in since timezone
  // can get out of sync when the system timezone is updated while
  // the view is swapped out.
  RenderThreadImpl::NotifyTimezoneChange();

  render_view_->SetSwappedOut(false);
  is_swapped_out_ = false;
  return;
}

void RenderFrameImpl::BeginNavigation(blink::WebURLRequest* request) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBrowserSideNavigation));
  DCHECK(request);
  // TODO(clamy): Execute the beforeunload event.

  // Note: At this stage, the goal is to apply all the modifications the
  // renderer wants to make to the request, and then send it to the browser, so
  // that the actual network request can be started. Ideally, all such
  // modifications should take place in willSendRequest, and in the
  // implementation of willSendRequest for the various InspectorAgents
  // (devtools).
  //
  // TODO(clamy): Apply devtools override.
  // TODO(clamy): Make sure that navigation requests are not modified somewhere
  // else in blink.
  willSendRequest(frame_, 0, *request, blink::WebURLResponse());

  // TODO(clamy): Same-document navigations should not be sent back to the
  // browser.
  // TODO(clamy): Data urls should not be sent back to the browser either.
  bool should_replace_current_entry = false;
  WebDataSource* provisional_data_source = frame_->provisionalDataSource();
  WebDataSource* current_data_source = frame_->dataSource();
  WebDataSource* data_source =
      provisional_data_source ? provisional_data_source : current_data_source;

  // The current entry can only be replaced if there already is an entry in the
  // history list.
  if (data_source && render_view_->history_list_length_ > 0) {
    should_replace_current_entry = data_source->replacesCurrentHistoryItem();
  }

  // These values are assumed on the browser side for navigations. These checks
  // ensure the renderer has the correct values.
  DCHECK_EQ(FETCH_REQUEST_MODE_SAME_ORIGIN,
            GetFetchRequestModeForWebURLRequest(*request));
  DCHECK_EQ(FETCH_CREDENTIALS_MODE_INCLUDE,
            GetFetchCredentialsModeForWebURLRequest(*request));
  DCHECK(GetFetchRedirectModeForWebURLRequest(*request) ==
         FetchRedirectMode::MANUAL_MODE);
  DCHECK_IMPLIES(!frame_->parent(),
                 GetRequestContextFrameTypeForWebURLRequest(*request) ==
                 REQUEST_CONTEXT_FRAME_TYPE_TOP_LEVEL);
  DCHECK_IMPLIES(frame_->parent(),
                 GetRequestContextFrameTypeForWebURLRequest(*request) ==
                 REQUEST_CONTEXT_FRAME_TYPE_NESTED);

  Send(new FrameHostMsg_BeginNavigation(
      routing_id_,
      MakeCommonNavigationParams(request, should_replace_current_entry),
      BeginNavigationParams(
          request->httpMethod().latin1(), GetWebURLRequestHeaders(*request),
          GetLoadFlagsForWebURLRequest(*request), request->hasUserGesture(),
          request->skipServiceWorker(),
          GetRequestContextTypeForWebURLRequest(*request)),
      GetRequestBodyForWebURLRequest(*request)));
}

void RenderFrameImpl::LoadDataURL(const CommonNavigationParams& params,
                                  WebFrame* frame) {
  // A loadData request with a specified base URL.
  std::string mime_type, charset, data;
  if (net::DataURL::Parse(params.url, &mime_type, &charset, &data)) {
    const GURL base_url = params.base_url_for_data_url.is_empty() ?
        params.url : params.base_url_for_data_url;
    frame->loadData(
        WebData(data.c_str(), data.length()),
        WebString::fromUTF8(mime_type),
        WebString::fromUTF8(charset),
        base_url,
        params.history_url_for_data_url,
        false);
  } else {
    CHECK(false) << "Invalid URL passed: "
                 << params.url.possibly_invalid_spec();
  }
}

void RenderFrameImpl::SendFailedProvisionalLoad(
    const blink::WebURLRequest& request,
    const blink::WebURLError& error,
    blink::WebLocalFrame* frame) {
  bool show_repost_interstitial =
      (error.reason == net::ERR_CACHE_MISS &&
       base::EqualsASCII(base::StringPiece16(request.httpMethod()), "POST"));

  FrameHostMsg_DidFailProvisionalLoadWithError_Params params;
  params.error_code = error.reason;
  GetContentClient()->renderer()->GetNavigationErrorStrings(
      render_view_.get(), frame, request, error, NULL,
      &params.error_description);
  params.url = error.unreachableURL;
  params.showing_repost_interstitial = show_repost_interstitial;
  params.was_ignored_by_handler = error.wasIgnoredByHandler;
  Send(new FrameHostMsg_DidFailProvisionalLoadWithError(routing_id_, params));
}

bool RenderFrameImpl::ShouldDisplayErrorPageForFailedLoad(
    int error_code,
    const GURL& unreachable_url) {
  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, Blink doesn't expect it and it will cause a crash.
  if (error_code == net::ERR_ABORTED)
    return false;

  // Don't display "client blocked" error page if browser has asked us not to.
  if (error_code == net::ERR_BLOCKED_BY_CLIENT &&
      render_view_->renderer_preferences_.disable_client_blocked_error_page) {
    return false;
  }

  // Allow the embedder to suppress an error page.
  if (GetContentClient()->renderer()->ShouldSuppressErrorPage(
          this, unreachable_url)) {
    return false;
  }

  if (RenderThreadImpl::current() &&
      RenderThreadImpl::current()->layout_test_mode()) {
    return false;
  }

  return true;
}

GURL RenderFrameImpl::GetLoadingUrl() const {
  WebDataSource* ds = frame_->dataSource();
  if (ds->hasUnreachableURL())
    return ds->unreachableURL();

  const WebURLRequest& request = ds->request();
  return request.url();
}

void RenderFrameImpl::PopulateDocumentStateFromPending(
    DocumentState* document_state) {
  document_state->set_request_time(
      pending_navigation_params_->request_params.request_time);

  InternalDocumentStateData* internal_data =
      InternalDocumentStateData::FromDocumentState(document_state);

  if (!pending_navigation_params_->common_params.url.SchemeIs(
          url::kJavaScriptScheme) &&
      pending_navigation_params_->common_params.navigation_type ==
          FrameMsg_Navigate_Type::RESTORE) {
    // We're doing a load of a page that was restored from the last session. By
    // default this prefers the cache over loading (LOAD_PREFERRING_CACHE) which
    // can result in stale data for pages that are set to expire. We explicitly
    // override that by setting the policy here so that as necessary we load
    // from the network.
    //
    // TODO(davidben): Remove this in favor of passing a cache policy to the
    // loadHistoryItem call in OnNavigate. That requires not overloading
    // UseProtocolCachePolicy to mean both "normal load" and "determine cache
    // policy based on load type, etc".
    internal_data->set_cache_policy_override(
        WebURLRequest::UseProtocolCachePolicy);
  }

  if (IsReload(pending_navigation_params_->common_params.navigation_type))
    document_state->set_load_type(DocumentState::RELOAD);
  else if (pending_navigation_params_->request_params.page_state.IsValid())
    document_state->set_load_type(DocumentState::HISTORY_LOAD);
  else
    document_state->set_load_type(DocumentState::NORMAL_LOAD);

  internal_data->set_is_overriding_user_agent(
      pending_navigation_params_->request_params.is_overriding_user_agent);
  internal_data->set_must_reset_scroll_and_scale_state(
      pending_navigation_params_->common_params.navigation_type ==
      FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL);
  document_state->set_can_load_local_resources(
      pending_navigation_params_->request_params.can_load_local_resources);
}

NavigationState* RenderFrameImpl::CreateNavigationStateFromPending() {
  if (IsBrowserInitiated(pending_navigation_params_.get())) {
    return NavigationStateImpl::CreateBrowserInitiated(
        pending_navigation_params_->common_params,
        pending_navigation_params_->start_params,
        pending_navigation_params_->request_params);
  }
  return NavigationStateImpl::CreateContentInitiated();
}

#if defined(OS_ANDROID)
WebMediaPlayer* RenderFrameImpl::CreateAndroidWebMediaPlayer(
    WebMediaPlayerClient* client,
    WebMediaPlayerEncryptedMediaClient* encrypted_client,
    const media::WebMediaPlayerParams& params) {
  scoped_refptr<StreamTextureFactory> stream_texture_factory;
  if (SynchronousCompositorFactory* factory =
          SynchronousCompositorFactory::GetInstance()) {
    stream_texture_factory = factory->CreateStreamTextureFactory(routing_id_);
  } else {
    GpuChannelHost* gpu_channel_host =
        RenderThreadImpl::current()->EstablishGpuChannelSync(
            CAUSE_FOR_GPU_LAUNCH_VIDEODECODEACCELERATOR_INITIALIZE);

    if (!gpu_channel_host) {
      LOG(ERROR) << "Failed to establish GPU channel for media player";
      return NULL;
    }

    scoped_refptr<cc_blink::ContextProviderWebContext> context_provider =
        RenderThreadImpl::current()->SharedMainThreadContextProvider();

    if (!context_provider.get()) {
      LOG(ERROR) << "Failed to get context3d for media player";
      return NULL;
    }

    stream_texture_factory = StreamTextureFactoryImpl::Create(
        context_provider, gpu_channel_host, routing_id_);
  }

  return new WebMediaPlayerAndroid(
      frame_, client, encrypted_client, weak_factory_.GetWeakPtr(),
      GetMediaPlayerManager(), GetCdmFactory(), stream_texture_factory, params);
}

RendererMediaPlayerManager* RenderFrameImpl::GetMediaPlayerManager() {
  if (!media_player_manager_)
    media_player_manager_ = new RendererMediaPlayerManager(this);
  return media_player_manager_;
}
#endif  // defined(OS_ANDROID)

scoped_ptr<media::MediaPermission> RenderFrameImpl::CreateMediaPermissionProxy(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner) {
  MediaPermissionDispatcherImpl* media_permission =
      static_cast<MediaPermissionDispatcherImpl*>(GetMediaPermission());
  return media_permission->CreateProxy(caller_task_runner).Pass();
}

media::MediaPermission* RenderFrameImpl::GetMediaPermission() {
  if (!media_permission_dispatcher_)
    media_permission_dispatcher_ = new MediaPermissionDispatcherImpl(this);
  return media_permission_dispatcher_;
}

#if defined(ENABLE_MOJO_MEDIA)
media::interfaces::ServiceFactory* RenderFrameImpl::GetMediaServiceFactory() {
  if (!media_service_factory_) {
    mojo::ServiceProviderPtr service_provider =
        ConnectToApplication(GURL("mojo:media"));
    mojo::ConnectToService(service_provider.get(), &media_service_factory_);
    media_service_factory_.set_connection_error_handler(
        base::Bind(&RenderFrameImpl::OnMediaServiceFactoryConnectionError,
                   base::Unretained(this)));
  }

  return media_service_factory_.get();
}

void RenderFrameImpl::OnMediaServiceFactoryConnectionError() {
  // TODO(xhwang): Resetting |media_service_factory_| could cause access
  // violation on the old |media_service_factory_| by outstanding
  // media::CdmFactory or media::RendererFactory. Find a better way to handle
  // this.
  // media_service_factory_.reset();
}
#endif

bool RenderFrameImpl::AreSecureCodecsSupported() {
#if defined(OS_ANDROID)
  // Hardware-secure codecs are only supported if secure surfaces are enabled.
  return render_view_->renderer_preferences_
      .use_video_overlay_for_embedded_encrypted_video;
#else
  return false;
#endif  // defined(OS_ANDROID)
}

media::CdmFactory* RenderFrameImpl::GetCdmFactory() {
#if defined(ENABLE_BROWSER_CDMS)
  if (!cdm_manager_)
    cdm_manager_ = new RendererCdmManager(this);
#endif  // defined(ENABLE_BROWSER_CDMS)

  if (!cdm_factory_) {
    DCHECK(frame_);

#if defined(ENABLE_MOJO_MEDIA)
    cdm_factory_.reset(new media::MojoCdmFactory(GetMediaServiceFactory()));
#else
    cdm_factory_.reset(new RenderCdmFactory(
#if defined(ENABLE_PEPPER_CDMS)
        base::Bind(&PepperCdmWrapperImpl::Create, frame_)
#elif defined(ENABLE_BROWSER_CDMS)
        cdm_manager_
#endif
        ));
#endif  //  defined(ENABLE_MOJO_MEDIA)
  }

  return cdm_factory_.get();
}

void RenderFrameImpl::RegisterMojoServices() {
  // Only main frame have ImageDownloader service.
  if (!frame_->parent()) {
    GetServiceRegistry()->AddService<image_downloader::ImageDownloader>(
        base::Bind(&ImageDownloaderImpl::CreateMojoService,
                   base::Unretained(this)));
  }
}

mojo::ServiceProviderPtr RenderFrameImpl::ConnectToApplication(
    const GURL& url) {
  DCHECK(mojo_shell_);
  mojo::ServiceProviderPtr service_provider;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From(url);
  mojo_shell_->ConnectToApplication(request.Pass(), GetProxy(&service_provider),
                                    nullptr, nullptr,
                                    base::Bind(&OnGotContentHandlerID));
  return service_provider.Pass();
}

}  // namespace content
