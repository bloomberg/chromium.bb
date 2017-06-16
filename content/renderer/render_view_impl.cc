// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/paint/skia_paint_canvas.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/appcache/web_application_cache_host_impl.h"
#include "content/child/request_extra_data.h"
#include "content/child/v8_value_converter_impl.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_switches_internal.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/common/drag_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/page_messages.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/common/associated_interface_provider.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_importance_signals.h"
#include "content/public/common/page_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/window_features_converter.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/idle_user_detector.h"
#include "content/renderer/ime_event_guard.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/internal_document_state_data.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/navigation_state_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_widget_fullscreen_pepper.h"
#include "content/renderer/renderer_webapplicationcachehost_impl.h"
#include "content/renderer/resizing_mode_selector.h"
#include "content/renderer/savable_resources.h"
#include "content/renderer/shared_worker/websharedworker_proxy.h"
#include "content/renderer/speech_recognition_dispatcher.h"
#include "content/renderer/web_ui_extension_data.h"
#include "media/audio/audio_output_device.h"
#include "media/base/media_switches.h"
#include "media/media_features.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_util.h"
#include "ppapi/features/features.h"
#include "services/ui/public/cpp/bitmap/child_shared_bitmap_manager.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebConnectionType.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebInputEventResult.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebStorageQuotaCallbacks.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/public_features.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "third_party/WebKit/public/web/WebColorSuggestion.h"
#include "third_party/WebKit/public/web/WebDOMEvent.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserCompletion.h"
#include "third_party/WebKit/public/web/WebDateTimeChooserParams.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFileChooserParams.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFrameContentDumper.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebHitTestResult.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebMediaPlayerAction.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebPageImportanceSignals.h"
#include "third_party/WebKit/public/web/WebPlugin.h"
#include "third_party/WebKit/public/web/WebPluginAction.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebRenderTheme.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSearchableFormData.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/switches.h"
#include "ui/latency/latency_info.h"
#include "url/origin.h"
#include "url/url_constants.h"
#include "v8/include/v8.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>

#include "base/android/build_info.h"
#include "content/renderer/android/disambiguation_popup_helper.h"
#include "ui/gfx/geometry/rect_f.h"

#elif defined(OS_MACOSX)
#include "skia/ext/skia_utils_mac.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_plugin_registry.h"
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#endif

using blink::WebAXObject;
using blink::WebApplicationCacheHost;
using blink::WebApplicationCacheHostClient;
using blink::WebColor;
using blink::WebConsoleMessage;
using blink::WebData;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebDragOperation;
using blink::WebElement;
using blink::WebFileChooserCompletion;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebFrameContentDumper;
using blink::WebGestureEvent;
using blink::WebHistoryItem;
using blink::WebHTTPBody;
using blink::WebHitTestResult;
using blink::WebImage;
using blink::WebInputElement;
using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMediaPlayerAction;
using blink::WebMouseEvent;
using blink::WebNavigationPolicy;
using blink::WebNavigationType;
using blink::WebNode;
using blink::WebPluginAction;
using blink::WebPoint;
using blink::WebRect;
using blink::WebReferrerPolicy;
using blink::WebScriptSource;
using blink::WebSearchableFormData;
using blink::WebSecurityOrigin;
using blink::WebSecurityPolicy;
using blink::WebSettings;
using blink::WebSize;
using blink::WebStorageNamespace;
using blink::WebStorageQuotaCallbacks;
using blink::WebStorageQuotaError;
using blink::WebStorageQuotaType;
using blink::WebString;
using blink::WebTextDirection;
using blink::WebTouchEvent;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebURLResponse;
using blink::WebUserGestureIndicator;
using blink::WebVector;
using blink::WebView;
using blink::WebWidget;
using blink::WebWindowFeatures;
using blink::WebRuntimeFeatures;
using base::Time;
using base::TimeDelta;


namespace content {

//-----------------------------------------------------------------------------

typedef std::map<blink::WebView*, RenderViewImpl*> ViewMap;
static base::LazyInstance<ViewMap>::Leaky g_view_map =
    LAZY_INSTANCE_INITIALIZER;
typedef std::map<int32_t, RenderViewImpl*> RoutingIDViewMap;
static base::LazyInstance<RoutingIDViewMap>::Leaky g_routing_id_view_map =
    LAZY_INSTANCE_INITIALIZER;

// Time, in seconds, we delay before sending content state changes (such as form
// state and scroll position) to the browser. We delay sending changes to avoid
// spamming the browser.
// To avoid having tab/session restore require sending a message to get the
// current content state during tab closing we use a shorter timeout for the
// foreground renderer. This means there is a small window of time from which
// content state is modified and not sent to session restore, but this is
// better than having to wake up all renderers during shutdown.
const int kDelaySecondsForContentStateSyncHidden = 5;
const int kDelaySecondsForContentStateSync = 1;

static RenderViewImpl* (*g_create_render_view_impl)(
    CompositorDependencies* compositor_deps,
    const mojom::CreateViewParams&) = nullptr;

// static
Referrer RenderViewImpl::GetReferrerFromRequest(
    WebFrame* frame,
    const WebURLRequest& request) {
  return Referrer(blink::WebStringToGURL(
                      request.HttpHeaderField(WebString::FromUTF8("Referer"))),
                  request.GetReferrerPolicy());
}

// static
WindowOpenDisposition RenderViewImpl::NavigationPolicyToDisposition(
    WebNavigationPolicy policy) {
  switch (policy) {
    case blink::kWebNavigationPolicyIgnore:
      return WindowOpenDisposition::IGNORE_ACTION;
    case blink::kWebNavigationPolicyDownload:
      return WindowOpenDisposition::SAVE_TO_DISK;
    case blink::kWebNavigationPolicyCurrentTab:
      return WindowOpenDisposition::CURRENT_TAB;
    case blink::kWebNavigationPolicyNewBackgroundTab:
      return WindowOpenDisposition::NEW_BACKGROUND_TAB;
    case blink::kWebNavigationPolicyNewForegroundTab:
      return WindowOpenDisposition::NEW_FOREGROUND_TAB;
    case blink::kWebNavigationPolicyNewWindow:
      return WindowOpenDisposition::NEW_WINDOW;
    case blink::kWebNavigationPolicyNewPopup:
      return WindowOpenDisposition::NEW_POPUP;
  default:
    NOTREACHED() << "Unexpected WebNavigationPolicy";
    return WindowOpenDisposition::IGNORE_ACTION;
  }
}

// Returns true if the device scale is high enough that losing subpixel
// antialiasing won't have a noticeable effect on text quality.
static bool DeviceScaleEnsuresTextQuality(float device_scale_factor) {
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // On Android, we never have subpixel antialiasing. On Chrome OS we prefer to
  // composite all scrollers so that we get animated overlay scrollbars.
  return true;
#else
  // 1.5 is a common touchscreen tablet device scale factor. For such
  // devices main thread antialiasing is a heavy burden.
  return device_scale_factor >= 1.5f;
#endif
}

static bool PreferCompositingToLCDText(CompositorDependencies* compositor_deps,
                                       float device_scale_factor) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisablePreferCompositingToLCDText))
    return false;
  if (command_line.HasSwitch(switches::kEnablePreferCompositingToLCDText))
    return true;
  if (!compositor_deps->IsLcdTextEnabled())
    return true;
  return DeviceScaleEnsuresTextQuality(device_scale_factor);
}

///////////////////////////////////////////////////////////////////////////////

namespace {

typedef void (*SetFontFamilyWrapper)(blink::WebSettings*,
                                     const base::string16&,
                                     UScriptCode);

void SetStandardFontFamilyWrapper(WebSettings* settings,
                                  const base::string16& font,
                                  UScriptCode script) {
  settings->SetStandardFontFamily(WebString::FromUTF16(font), script);
}

void SetFixedFontFamilyWrapper(WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->SetFixedFontFamily(WebString::FromUTF16(font), script);
}

void SetSerifFontFamilyWrapper(WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->SetSerifFontFamily(WebString::FromUTF16(font), script);
}

void SetSansSerifFontFamilyWrapper(WebSettings* settings,
                                   const base::string16& font,
                                   UScriptCode script) {
  settings->SetSansSerifFontFamily(WebString::FromUTF16(font), script);
}

void SetCursiveFontFamilyWrapper(WebSettings* settings,
                                 const base::string16& font,
                                 UScriptCode script) {
  settings->SetCursiveFontFamily(WebString::FromUTF16(font), script);
}

void SetFantasyFontFamilyWrapper(WebSettings* settings,
                                 const base::string16& font,
                                 UScriptCode script) {
  settings->SetFantasyFontFamily(WebString::FromUTF16(font), script);
}

void SetPictographFontFamilyWrapper(WebSettings* settings,
                                    const base::string16& font,
                                    UScriptCode script) {
  settings->SetPictographFontFamily(WebString::FromUTF16(font), script);
}

// If |scriptCode| is a member of a family of "similar" script codes, returns
// the script code in that family that is used by WebKit for font selection
// purposes.  For example, USCRIPT_KATAKANA_OR_HIRAGANA and USCRIPT_JAPANESE are
// considered equivalent for the purposes of font selection.  WebKit uses the
// script code USCRIPT_KATAKANA_OR_HIRAGANA.  So, if |scriptCode| is
// USCRIPT_JAPANESE, the function returns USCRIPT_KATAKANA_OR_HIRAGANA.  WebKit
// uses different scripts than the ones in Chrome pref names because the version
// of ICU included on certain ports does not have some of the newer scripts.  If
// |scriptCode| is not a member of such a family, returns |scriptCode|.
UScriptCode GetScriptForWebSettings(UScriptCode scriptCode) {
  switch (scriptCode) {
    case USCRIPT_HIRAGANA:
    case USCRIPT_KATAKANA:
    case USCRIPT_JAPANESE:
      return USCRIPT_KATAKANA_OR_HIRAGANA;
    case USCRIPT_KOREAN:
      return USCRIPT_HANGUL;
    default:
      return scriptCode;
  }
}

void ApplyFontsFromMap(const ScriptFontFamilyMap& map,
                       SetFontFamilyWrapper setter,
                       WebSettings* settings) {
  for (ScriptFontFamilyMap::const_iterator it = map.begin(); it != map.end();
       ++it) {
    int32_t script = u_getPropertyValueEnum(UCHAR_SCRIPT, (it->first).c_str());
    if (script >= 0 && script < USCRIPT_CODE_LIMIT) {
      UScriptCode code = static_cast<UScriptCode>(script);
      (*setter)(settings, it->second, GetScriptForWebSettings(code));
    }
  }
}

void ApplyBlinkSettings(const base::CommandLine& command_line,
                        WebSettings* settings) {
  if (!command_line.HasSwitch(switches::kBlinkSettings))
    return;

  std::vector<std::string> blink_settings = base::SplitString(
      command_line.GetSwitchValueASCII(switches::kBlinkSettings),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const std::string& setting : blink_settings) {
    size_t pos = setting.find('=');
    settings->SetFromStrings(
        blink::WebString::FromLatin1(setting.substr(0, pos)),
        blink::WebString::FromLatin1(
            pos == std::string::npos ? "" : setting.substr(pos + 1)));
  }
}

WebSettings::V8CacheStrategiesForCacheStorage
GetV8CacheStrategiesForCacheStorage() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string v8_cache_strategies = command_line.GetSwitchValueASCII(
      switches::kV8CacheStrategiesForCacheStorage);
  if (v8_cache_strategies.empty()) {
    v8_cache_strategies =
        base::FieldTrialList::FindFullName("V8CacheStrategiesForCacheStorage");
  }

  if (base::StartsWith(v8_cache_strategies, "none",
                       base::CompareCase::SENSITIVE)) {
    return WebSettings::V8CacheStrategiesForCacheStorage::kNone;
  } else if (base::StartsWith(v8_cache_strategies, "normal",
                              base::CompareCase::SENSITIVE)) {
    return WebSettings::V8CacheStrategiesForCacheStorage::kNormal;
  } else if (base::StartsWith(v8_cache_strategies, "aggressive",
                              base::CompareCase::SENSITIVE)) {
    return WebSettings::V8CacheStrategiesForCacheStorage::kAggressive;
  } else {
    return WebSettings::V8CacheStrategiesForCacheStorage::kDefault;
  }
}

// This class represents promise which is robust to (will not be broken by)
// |DidNotSwapReason::SWAP_FAILS| events.
class AlwaysDrawSwapPromise : public cc::SwapPromise {
 public:
  explicit AlwaysDrawSwapPromise(const ui::LatencyInfo& latency_info)
      : latency_info_(latency_info) {}

  ~AlwaysDrawSwapPromise() override = default;

  void DidActivate() override {}

  void WillSwap(cc::CompositorFrameMetadata* metadata) override {
    DCHECK(!latency_info_.terminated());
    metadata->latency_info.push_back(latency_info_);
  }

  void DidSwap() override {}

  DidNotSwapAction DidNotSwap(DidNotSwapReason reason) override {
    return reason == DidNotSwapReason::SWAP_FAILS
               ? DidNotSwapAction::KEEP_ACTIVE
               : DidNotSwapAction::BREAK_PROMISE;
  }

  void OnCommit() override {}

  int64_t TraceId() const override { return latency_info_.trace_id(); }

 private:
  ui::LatencyInfo latency_info_;
};

content::mojom::WindowContainerType WindowFeaturesToContainerType(
    const blink::WebWindowFeatures& window_features) {
  if (window_features.background) {
    if (window_features.persistent)
      return content::mojom::WindowContainerType::PERSISTENT;
    else
      return content::mojom::WindowContainerType::BACKGROUND;
  } else {
    return content::mojom::WindowContainerType::NORMAL;
  }
}

}  // namespace

RenderViewImpl::RenderViewImpl(CompositorDependencies* compositor_deps,
                               const mojom::CreateViewParams& params)
    : RenderWidget(params.view_id,
                   compositor_deps,
                   blink::kWebPopupTypeNone,
                   params.initial_size.screen_info,
                   params.swapped_out,
                   params.hidden,
                   params.never_visible),
      webkit_preferences_(params.web_preferences),
      send_content_state_immediately_(false),
      enabled_bindings_(0),
      send_preferred_size_changes_(false),
      navigation_gesture_(NavigationGestureUnknown),
      history_list_offset_(-1),
      history_list_length_(0),
      frames_in_progress_(0),
      target_url_status_(TARGET_NONE),
      uses_temporary_zoom_level_(false),
#if defined(OS_ANDROID)
      top_controls_constraints_(BROWSER_CONTROLS_STATE_BOTH),
#endif
      browser_controls_shrink_blink_size_(false),
      top_controls_height_(0.f),
      webview_(nullptr),
      has_scrolled_focused_editable_node_into_rect_(false),
      page_zoom_level_(params.page_zoom_level),
      main_render_frame_(nullptr),
      frame_widget_(nullptr),
      speech_recognition_dispatcher_(NULL),
#if defined(OS_ANDROID)
      was_created_by_renderer_(false),
#endif
      enumeration_completion_id_(0),
      session_storage_namespace_id_(params.session_storage_namespace_id),
      weak_ptr_factory_(this) {
  GetWidget()->set_owner_delegate(this);
}

void RenderViewImpl::Initialize(
    const mojom::CreateViewParams& params,
    const RenderWidget::ShowCallback& show_callback) {
  bool was_created_by_renderer = !show_callback.is_null();
#if defined(OS_ANDROID)
  // TODO(sgurun): crbug.com/325351 Needed only for android webview's deprecated
  // HandleNavigation codepath.
  was_created_by_renderer_ = was_created_by_renderer;
#endif
  display_mode_ = params.initial_size.display_mode;

  webview_ = WebView::Create(this, is_hidden()
                                       ? blink::kWebPageVisibilityStateHidden
                                       : blink::kWebPageVisibilityStateVisible);
  RenderWidget::Init(show_callback, webview_->GetWidget());

  g_view_map.Get().insert(std::make_pair(webview(), this));
  g_routing_id_view_map.Get().insert(std::make_pair(GetRoutingID(), this));

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kStatsCollectionController))
    stats_collection_observer_.reset(new StatsCollectionObserver(this));

  webview()->SetDisplayMode(display_mode_);
  webview()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      PreferCompositingToLCDText(compositor_deps_, device_scale_factor_));
  webview()->GetSettings()->SetThreadedScrollingEnabled(
      !command_line.HasSwitch(switches::kDisableThreadedScrolling));
  webview()->SetShowFPSCounter(
      command_line.HasSwitch(cc::switches::kShowFPSCounter));
  webview()->SetDeviceColorProfile(params.image_decode_color_space);

  ApplyWebPreferencesInternal(webkit_preferences_, webview(), compositor_deps_);

  if (switches::IsTouchDragDropEnabled())
    webview()->GetSettings()->SetTouchDragDropEnabled(true);

  webview()->GetSettings()->SetBrowserSideNavigationEnabled(
      IsBrowserSideNavigationEnabled());

  WebSettings::SelectionStrategyType selection_strategy =
      WebSettings::SelectionStrategyType::kCharacter;
  const std::string selection_strategy_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTouchTextSelectionStrategy);
  if (selection_strategy_str == "direction")
    selection_strategy = WebSettings::SelectionStrategyType::kDirection;
  webview()->GetSettings()->SetSelectionStrategy(selection_strategy);

  std::string passiveListenersDefault =
      command_line.GetSwitchValueASCII(switches::kPassiveListenersDefault);
  if (!passiveListenersDefault.empty()) {
    WebSettings::PassiveEventListenerDefault passiveDefault =
        WebSettings::PassiveEventListenerDefault::kFalse;
    if (passiveListenersDefault == "true")
      passiveDefault = WebSettings::PassiveEventListenerDefault::kTrue;
    else if (passiveListenersDefault == "forcealltrue")
      passiveDefault = WebSettings::PassiveEventListenerDefault::kForceAllTrue;
    webview()->GetSettings()->SetPassiveEventListenerDefault(passiveDefault);
  }

  ApplyBlinkSettings(command_line, webview()->GetSettings());

  WebFrame* opener_frame =
      RenderFrameImpl::ResolveOpener(params.opener_frame_route_id);

  if (params.main_frame_routing_id != MSG_ROUTING_NONE) {
    main_render_frame_ = RenderFrameImpl::CreateMainFrame(
        this, params.main_frame_routing_id, params.main_frame_widget_routing_id,
        params.hidden, screen_info(), compositor_deps_, opener_frame);
  }

  if (params.proxy_routing_id != MSG_ROUTING_NONE) {
    CHECK(params.swapped_out);
    RenderFrameProxy::CreateFrameProxy(params.proxy_routing_id, GetRoutingID(),
                                       opener_frame, MSG_ROUTING_NONE,
                                       params.replicated_frame_state);
  }

  if (main_render_frame_)
    main_render_frame_->Initialize();

  // If this RenderView's creation was initiated by an opener page in this
  // process, (e.g. window.open()), we won't be visible until we ask the opener,
  // via show_callback, to make us visible. Otherwise, we went through a
  // browser-initiated creation, and show() won't be called.
  if (!was_created_by_renderer)
    did_show_ = true;

  // Set the main frame's name.  Only needs to be done for WebLocalFrames,
  // since the remote case was handled as part of SetReplicatedState on the
  // proxy above.
  if (!params.replicated_frame_state.name.empty() &&
      webview()->MainFrame()->IsWebLocalFrame()) {
    webview()->MainFrame()->SetName(
        blink::WebString::FromUTF8(params.replicated_frame_state.name));
  }

  // TODO(davidben): Move this state from Blink into content.
  if (params.window_was_created_with_opener)
    webview()->SetOpenedByDOM();

  UpdateWebViewWithDeviceScaleFactor();
  OnSetRendererPrefs(params.renderer_preferences);

  if (!params.enable_auto_resize) {
    OnResize(params.initial_size);
  } else {
    OnEnableAutoResize(params.min_size, params.max_size);
  }

  idle_user_detector_.reset(new IdleUserDetector(this));

  GetContentClient()->renderer()->RenderViewCreated(this);

  // Ensure that sandbox flags are inherited from an opener in a different
  // process.  In that case, the browser process will set any inherited sandbox
  // flags in |replicated_frame_state|, so apply them here.
  if (!was_created_by_renderer && webview()->MainFrame()->IsWebLocalFrame()) {
    webview()->MainFrame()->ToWebLocalFrame()->ForceSandboxFlags(
        params.replicated_frame_state.sandbox_flags);
  }

  page_zoom_level_ = params.page_zoom_level;
}

RenderViewImpl::~RenderViewImpl() {
  DCHECK(!frame_widget_);

  for (BitmapMap::iterator it = disambiguation_bitmaps_.begin();
       it != disambiguation_bitmaps_.end();
       ++it)
    delete it->second;

#if defined(OS_ANDROID)
  // The date/time picker client is both a std::unique_ptr member of this class
  // and a RenderViewObserver. Reset it to prevent double deletion.
  date_time_picker_client_.reset();
#endif

#ifndef NDEBUG
  // Make sure we are no longer referenced by the ViewMap or RoutingIDViewMap.
  ViewMap* views = g_view_map.Pointer();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
  RoutingIDViewMap* routing_id_views = g_routing_id_view_map.Pointer();
  for (RoutingIDViewMap::iterator it = routing_id_views->begin();
       it != routing_id_views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
#endif

  idle_user_detector_.reset();
  for (auto& observer : observers_)
    observer.RenderViewGone();
  for (auto& observer : observers_)
    observer.OnDestruct();
}

/*static*/
RenderViewImpl* RenderViewImpl::FromWebView(WebView* webview) {
  ViewMap* views = g_view_map.Pointer();
  ViewMap::iterator it = views->find(webview);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::FromWebView(blink::WebView* webview) {
  return RenderViewImpl::FromWebView(webview);
}

/*static*/
RenderViewImpl* RenderViewImpl::FromRoutingID(int32_t routing_id) {
  RoutingIDViewMap* views = g_routing_id_view_map.Pointer();
  RoutingIDViewMap::iterator it = views->find(routing_id);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::FromRoutingID(int routing_id) {
  return RenderViewImpl::FromRoutingID(routing_id);
}

/* static */
size_t RenderView::GetRenderViewCount() {
  return g_view_map.Get().size();
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
void RenderView::ApplyWebPreferences(const WebPreferences& prefs,
                                     WebView* web_view) {
  WebSettings* settings = web_view->GetSettings();
  ApplyFontsFromMap(prefs.standard_font_family_map,
                    SetStandardFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.fixed_font_family_map,
                    SetFixedFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.serif_font_family_map,
                    SetSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.sans_serif_font_family_map,
                    SetSansSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.cursive_font_family_map,
                    SetCursiveFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.fantasy_font_family_map,
                    SetFantasyFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.pictograph_font_family_map,
                    SetPictographFontFamilyWrapper, settings);
  settings->SetDefaultFontSize(prefs.default_font_size);
  settings->SetDefaultFixedFontSize(prefs.default_fixed_font_size);
  settings->SetMinimumFontSize(prefs.minimum_font_size);
  settings->SetMinimumLogicalFontSize(prefs.minimum_logical_font_size);
  settings->SetDefaultTextEncodingName(
      WebString::FromASCII(prefs.default_encoding));
  settings->SetJavaScriptEnabled(prefs.javascript_enabled);
  settings->SetWebSecurityEnabled(prefs.web_security_enabled);
  settings->SetLoadsImagesAutomatically(prefs.loads_images_automatically);
  settings->SetImagesEnabled(prefs.images_enabled);
  settings->SetPluginsEnabled(prefs.plugins_enabled);
  settings->SetEncryptedMediaEnabled(prefs.encrypted_media_enabled);
  settings->SetDOMPasteAllowed(prefs.dom_paste_enabled);
  settings->SetTextAreasAreResizable(prefs.text_areas_are_resizable);
  settings->SetAllowScriptsToCloseWindows(prefs.allow_scripts_to_close_windows);
  settings->SetDownloadableBinaryFontsEnabled(prefs.remote_fonts_enabled);
  settings->SetJavaScriptCanAccessClipboard(
      prefs.javascript_can_access_clipboard);
  WebRuntimeFeatures::EnableXSLT(prefs.xslt_enabled);
  settings->SetXSSAuditorEnabled(prefs.xss_auditor_enabled);
  settings->SetDNSPrefetchingEnabled(prefs.dns_prefetching_enabled);
  settings->SetDataSaverEnabled(prefs.data_saver_enabled);
  settings->SetLocalStorageEnabled(prefs.local_storage_enabled);
  settings->SetSyncXHRInDocumentsEnabled(prefs.sync_xhr_in_documents_enabled);
  WebRuntimeFeatures::EnableDatabase(prefs.databases_enabled);
  settings->SetOfflineWebApplicationCacheEnabled(
      prefs.application_cache_enabled);
  settings->SetHistoryEntryRequiresUserGesture(
      prefs.history_entry_requires_user_gesture);
  settings->SetHyperlinkAuditingEnabled(prefs.hyperlink_auditing_enabled);
  settings->SetCookieEnabled(prefs.cookie_enabled);
  settings->SetNavigateOnDragDrop(prefs.navigate_on_drag_drop);

  // By default, allow_universal_access_from_file_urls is set to false and thus
  // we mitigate attacks from local HTML files by not granting file:// URLs
  // universal access. Only test shell will enable this.
  settings->SetAllowUniversalAccessFromFileURLs(
      prefs.allow_universal_access_from_file_urls);
  settings->SetAllowFileAccessFromFileURLs(
      prefs.allow_file_access_from_file_urls);

  // Enable experimental WebGL support if requested on command line
  // and support is compiled in.
  settings->SetExperimentalWebGLEnabled(prefs.experimental_webgl_enabled);

  // Enable WebGL errors to the JS console if requested.
  settings->SetWebGLErrorsToConsoleEnabled(
      prefs.webgl_errors_to_console_enabled);

  // Uses the mock theme engine for scrollbars.
  settings->SetMockScrollbarsEnabled(prefs.mock_scrollbars_enabled);

  settings->SetHideScrollbars(prefs.hide_scrollbars);

  // Enable gpu-accelerated 2d canvas if requested on the command line.
  WebRuntimeFeatures::EnableAccelerated2dCanvas(
      prefs.accelerated_2d_canvas_enabled);

  settings->SetMinimumAccelerated2dCanvasSize(
      prefs.minimum_accelerated_2d_canvas_size);

  // Disable antialiasing for 2d canvas if requested on the command line.
  settings->SetAntialiased2dCanvasEnabled(
      !prefs.antialiased_2d_canvas_disabled);
  WebRuntimeFeatures::ForceDisable2dCanvasCopyOnWrite(
      prefs.disable_2d_canvas_copy_on_write);

  // Disable antialiasing of clips for 2d canvas if requested on the command
  // line.
  settings->SetAntialiasedClips2dCanvasEnabled(
      prefs.antialiased_clips_2d_canvas_enabled);

  // Set MSAA sample count for 2d canvas if requested on the command line (or
  // default value if not).
  settings->SetAccelerated2dCanvasMSAASampleCount(
      prefs.accelerated_2d_canvas_msaa_sample_count);

  // Tabs to link is not part of the settings. WebCore calls
  // ChromeClient::tabsToLinks which is part of the glue code.
  web_view->SetTabsToLinks(prefs.tabs_to_links);

  settings->SetAllowRunningOfInsecureContent(
      prefs.allow_running_insecure_content);
  settings->SetDisableReadingFromCanvas(prefs.disable_reading_from_canvas);
  settings->SetStrictMixedContentChecking(prefs.strict_mixed_content_checking);

  settings->SetStrictlyBlockBlockableMixedContent(
      prefs.strictly_block_blockable_mixed_content);

  settings->SetStrictMixedContentCheckingForPlugin(
      prefs.block_mixed_plugin_content);

  settings->SetStrictPowerfulFeatureRestrictions(
      prefs.strict_powerful_feature_restrictions);
  settings->SetAllowGeolocationOnInsecureOrigins(
      prefs.allow_geolocation_on_insecure_origins);
  settings->SetPasswordEchoEnabled(prefs.password_echo_enabled);
  settings->SetShouldPrintBackgrounds(prefs.should_print_backgrounds);
  settings->SetShouldClearDocumentBackground(
      prefs.should_clear_document_background);
  settings->SetEnableScrollAnimator(prefs.enable_scroll_animator);

  WebRuntimeFeatures::EnableTouchEventFeatureDetection(
      prefs.touch_event_feature_detection_enabled);
  settings->SetMaxTouchPoints(prefs.pointer_events_max_touch_points);
  settings->SetAvailablePointerTypes(prefs.available_pointer_types);
  settings->SetPrimaryPointerType(
      static_cast<blink::PointerType>(prefs.primary_pointer_type));
  settings->SetAvailableHoverTypes(prefs.available_hover_types);
  settings->SetPrimaryHoverType(
      static_cast<blink::HoverType>(prefs.primary_hover_type));
  settings->SetEnableTouchAdjustment(prefs.touch_adjustment_enabled);

  WebRuntimeFeatures::EnableColorCorrectRendering(
      prefs.color_correct_rendering_enabled);

  settings->SetShouldRespectImageOrientation(
      prefs.should_respect_image_orientation);

  settings->SetEditingBehavior(
      static_cast<WebSettings::EditingBehavior>(prefs.editing_behavior));

  settings->SetSupportsMultipleWindows(prefs.supports_multiple_windows);

  settings->SetInertVisualViewport(prefs.inert_visual_viewport);

  settings->SetMainFrameClipsContent(!prefs.record_whole_document);

  settings->SetSmartInsertDeleteEnabled(prefs.smart_insert_delete_enabled);

  settings->SetSpatialNavigationEnabled(prefs.spatial_navigation_enabled);

  settings->SetSelectionIncludesAltImageText(true);

  settings->SetV8CacheOptions(
      static_cast<WebSettings::V8CacheOptions>(prefs.v8_cache_options));

  settings->SetV8CacheStrategiesForCacheStorage(
      GetV8CacheStrategiesForCacheStorage());

  settings->SetImageAnimationPolicy(
      static_cast<WebSettings::ImageAnimationPolicy>(prefs.animation_policy));

  settings->SetPresentationRequiresUserGesture(
      prefs.user_gesture_required_for_presentation);

  settings->SetTextTrackMarginPercentage(prefs.text_track_margin_percentage);

  // Needs to happen before setIgnoreVIewportTagScaleLimits below.
  web_view->SetDefaultPageScaleLimits(prefs.default_minimum_page_scale_factor,
                                      prefs.default_maximum_page_scale_factor);

#if defined(OS_ANDROID)
  settings->SetAllowCustomScrollbarInMainFrame(false);
  settings->SetTextAutosizingEnabled(prefs.text_autosizing_enabled);
  settings->SetAccessibilityFontScaleFactor(prefs.font_scale_factor);
  settings->SetDeviceScaleAdjustment(prefs.device_scale_adjustment);
  settings->SetFullscreenSupported(prefs.fullscreen_supported);
  web_view->SetIgnoreViewportTagScaleLimits(prefs.force_enable_zoom);
  settings->SetAutoZoomFocusedNodeToLegibleScale(true);
  settings->SetDoubleTapToZoomEnabled(prefs.double_tap_to_zoom_enabled);
  settings->SetMediaPlaybackGestureWhitelistScope(
      blink::WebString::FromUTF8(prefs.media_playback_gesture_whitelist_scope));
  settings->SetDefaultVideoPosterURL(
      WebString::FromASCII(prefs.default_video_poster_url.spec()));
  settings->SetSupportDeprecatedTargetDensityDPI(
      prefs.support_deprecated_target_density_dpi);
  settings->SetUseLegacyBackgroundSizeShorthandBehavior(
      prefs.use_legacy_background_size_shorthand_behavior);
  settings->SetWideViewportQuirkEnabled(prefs.wide_viewport_quirk);
  settings->SetUseWideViewport(prefs.use_wide_viewport);
  settings->SetForceZeroLayoutHeight(prefs.force_zero_layout_height);
  settings->SetViewportMetaLayoutSizeQuirk(
      prefs.viewport_meta_layout_size_quirk);
  settings->SetViewportMetaMergeContentQuirk(
      prefs.viewport_meta_merge_content_quirk);
  settings->SetViewportMetaNonUserScalableQuirk(
      prefs.viewport_meta_non_user_scalable_quirk);
  settings->SetViewportMetaZeroValuesQuirk(
      prefs.viewport_meta_zero_values_quirk);
  settings->SetClobberUserAgentInitialScaleQuirk(
      prefs.clobber_user_agent_initial_scale_quirk);
  settings->SetIgnoreMainFrameOverflowHiddenQuirk(
      prefs.ignore_main_frame_overflow_hidden_quirk);
  settings->SetReportScreenSizeInPhysicalPixelsQuirk(
      prefs.report_screen_size_in_physical_pixels_quirk);
  settings->SetShouldReuseGlobalForUnownedMainFrame(
      prefs.resue_global_for_unowned_main_frame);
  settings->SetProgressBarCompletion(
      static_cast<WebSettings::ProgressBarCompletion>(
          prefs.progress_bar_completion));
  settings->SetPreferHiddenVolumeControls(true);
  settings->SetSpellCheckEnabledByDefault(prefs.spellcheck_enabled_by_default);

  // Force preload=none and disable autoplay on older Android
  // platforms because their media pipelines are not stable enough to handle
  // concurrent elements. See http://crbug.com/612909, http://crbug.com/622826.
  const bool is_jelly_bean =
      base::android::BuildInfo::GetInstance()->sdk_int() <=
      base::android::SDK_VERSION_JELLY_BEAN_MR2;
  settings->SetForcePreloadNoneForMediaElements(is_jelly_bean);

  WebRuntimeFeatures::EnableVideoFullscreenOrientationLock(
      prefs.video_fullscreen_orientation_lock_enabled);
  WebRuntimeFeatures::EnableVideoRotateToFullscreen(
      prefs.video_rotate_to_fullscreen_enabled);
  WebRuntimeFeatures::EnableVideoFullscreenDetection(
      prefs.video_fullscreen_detection_enabled);
  settings->SetEmbeddedMediaExperienceEnabled(
      prefs.embedded_media_experience_enabled);
  settings->SetPagePopupsSuppressed(prefs.page_popups_suppressed);
  settings->SetDoNotUpdateSelectionOnMutatingSelectionRange(
      prefs.do_not_update_selection_on_mutating_selection_range);
#endif  // defined(OS_ANDROID)

  switch (prefs.autoplay_policy) {
    case AutoplayPolicy::kNoUserGestureRequired:
      settings->SetAutoplayPolicy(
          WebSettings::AutoplayPolicy::kNoUserGestureRequired);
      break;
    case AutoplayPolicy::kUserGestureRequired:
      settings->SetAutoplayPolicy(
          WebSettings::AutoplayPolicy::kUserGestureRequired);
      break;
    case AutoplayPolicy::kUserGestureRequiredForCrossOrigin:
      settings->SetAutoplayPolicy(
          WebSettings::AutoplayPolicy::kUserGestureRequiredForCrossOrigin);
      break;
    case AutoplayPolicy::kDocumentUserActivationRequired:
      settings->SetAutoplayPolicy(
          WebSettings::AutoplayPolicy::kDocumentUserActivationRequired);
      break;
  }

  settings->SetViewportEnabled(prefs.viewport_enabled);
  settings->SetViewportMetaEnabled(prefs.viewport_meta_enabled);
  settings->SetShrinksViewportContentToFit(
      prefs.shrinks_viewport_contents_to_fit);
  settings->SetViewportStyle(
      static_cast<blink::WebViewportStyle>(prefs.viewport_style));

  settings->SetLoadWithOverviewMode(prefs.initialize_at_minimum_page_scale);
  settings->SetMainFrameResizesAreOrientationChanges(
      prefs.main_frame_resizes_are_orientation_changes);

  settings->SetUseSolidColorScrollbars(prefs.use_solid_color_scrollbars);

  settings->SetShowContextMenuOnMouseUp(prefs.context_menu_on_mouse_up);
  settings->SetAlwaysShowContextMenuOnTouch(
      prefs.always_show_context_menu_on_touch);

  settings->SetHideDownloadUI(prefs.hide_download_ui);
  WebRuntimeFeatures::EnableBackgroundVideoTrackOptimization(
      prefs.background_video_track_optimization_enabled);
  WebRuntimeFeatures::EnableNewRemotePlaybackPipeline(
      base::FeatureList::IsEnabled(media::kNewRemotePlaybackPipeline));

  settings->SetPresentationReceiver(prefs.presentation_receiver);

  settings->SetMediaControlsEnabled(prefs.media_controls_enabled);

#if defined(OS_MACOSX)
  settings->SetDoubleTapToZoomEnabled(true);
  web_view->SetMaximumLegibleScale(prefs.default_maximum_page_scale_factor);
#endif

#if defined(OS_WIN)
  WebRuntimeFeatures::EnableMiddleClickAutoscroll(true);
#endif
}

/*static*/
RenderViewImpl* RenderViewImpl::Create(
    CompositorDependencies* compositor_deps,
    const mojom::CreateViewParams& params,
    const RenderWidget::ShowCallback& show_callback) {
  DCHECK(params.view_id != MSG_ROUTING_NONE);
  RenderViewImpl* render_view = NULL;
  if (g_create_render_view_impl)
    render_view = g_create_render_view_impl(compositor_deps, params);
  else
    render_view = new RenderViewImpl(compositor_deps, params);

  render_view->Initialize(params, show_callback);
  return render_view;
}

// static
void RenderViewImpl::InstallCreateHook(RenderViewImpl* (
    *create_render_view_impl)(CompositorDependencies* compositor_deps,
                              const mojom::CreateViewParams&)) {
  CHECK(!g_create_render_view_impl);
  g_create_render_view_impl = create_render_view_impl;
}

void RenderViewImpl::AddObserver(RenderViewObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderViewImpl::RemoveObserver(RenderViewObserver* observer) {
  observer->RenderViewGone();
  observers_.RemoveObserver(observer);
}

blink::WebView* RenderViewImpl::webview() const {
  return webview_;
}

#if BUILDFLAG(ENABLE_PLUGINS)

#if defined(OS_MACOSX)
void RenderViewImpl::OnGetRenderedText() {
  if (!webview())
    return;

  if (!webview()->MainFrame()->IsWebLocalFrame())
    return;

  // Get rendered text from WebLocalFrame.
  // TODO: Currently IPC truncates any data that has a
  // size > kMaximumMessageSize. May be split the text into smaller chunks and
  // send back using multiple IPC. See http://crbug.com/393444.
  static const size_t kMaximumMessageSize = 8 * 1024 * 1024;
  // TODO(dglazkov): Using this API is wrong. It's not OOPIF-compatible and
  // sends text in the wrong order. See http://crbug.com/584798.
  // TODO(dglazkov): WebFrameContentDumper should only be used for
  // testing purposes. See http://crbug.com/585164.
  std::string text =
      WebFrameContentDumper::DumpWebViewAsText(webview(), kMaximumMessageSize)
          .Utf8();

  Send(new ViewMsg_GetRenderedTextCompleted(GetRoutingID(), text));
}
#endif  // defined(OS_MACOSX)

#endif  // ENABLE_PLUGINS

void RenderViewImpl::TransferActiveWheelFlingAnimation(
    const blink::WebActiveWheelFlingParameters& params) {
  if (webview())
    webview()->TransferActiveWheelFlingAnimation(params);
}

// RenderWidgetInputHandlerDelegate -----------------------------------------

bool RenderViewImpl::DoesRenderWidgetHaveTouchEventHandlersAt(
    const gfx::Point& point) const {
  if (!webview())
    return false;
  return webview()->HasTouchEventHandlersAt(point);
}

bool RenderViewImpl::RenderWidgetWillHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  // If the mouse is locked, only the current owner of the mouse lock can
  // process mouse events.
  return mouse_lock_dispatcher_->WillHandleMouseEvent(event);
}

// IPC::Listener implementation ----------------------------------------------

bool RenderViewImpl::OnMessageReceived(const IPC::Message& message) {
  WebFrame* main_frame = webview() ? webview()->MainFrame() : NULL;
  if (main_frame && main_frame->IsWebLocalFrame())
    GetContentClient()->SetActiveURL(main_frame->GetDocument().Url());

  // Input IPC messages must not be processed if the RenderView is in
  // swapped out state.
  if (is_swapped_out_ &&
      IPC_MESSAGE_ID_CLASS(message.type()) == InputMsgStart) {
    // TODO(dtapuska): Remove this histogram once we have seen that it actually
    // produces results true. See crbug.com/615090
    UMA_HISTOGRAM_BOOLEAN("Event.RenderView.DiscardInput", true);
    IPC_BEGIN_MESSAGE_MAP(RenderViewImpl, message)
      IPC_MESSAGE_HANDLER(InputMsg_HandleInputEvent, OnDiscardInputEvent)
    IPC_END_MESSAGE_MAP()
    return false;
  }

  for (auto& observer : observers_) {
    if (observer.OnMessageReceived(message))
      return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderViewImpl, message)
    IPC_MESSAGE_HANDLER(InputMsg_ExecuteEditCommand, OnExecuteEditCommand)
    IPC_MESSAGE_HANDLER(InputMsg_MoveCaret, OnMoveCaret)
    IPC_MESSAGE_HANDLER(InputMsg_ScrollFocusedEditableNodeIntoRect,
                        OnScrollFocusedEditableNodeIntoRect)
    IPC_MESSAGE_HANDLER(ViewMsg_SetPageScale, OnSetPageScale)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInitialFocus, OnSetInitialFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTargetURL_ACK, OnUpdateTargetURLAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateWebPreferences, OnUpdateWebPreferences)
    IPC_MESSAGE_HANDLER(ViewMsg_EnumerateDirectoryResponse,
                        OnEnumerateDirectoryResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_ClosePage, OnClosePage)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveOrResizeStarted, OnMoveOrResizeStarted)
    IPC_MESSAGE_HANDLER(ViewMsg_SetBackgroundOpaque, OnSetBackgroundOpaque)
    IPC_MESSAGE_HANDLER(ViewMsg_EnablePreferredSizeChangedMode,
                        OnEnablePreferredSizeChangedMode)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableAutoResize, OnEnableAutoResize)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableAutoResize, OnDisableAutoResize)
    IPC_MESSAGE_HANDLER(ViewMsg_DisableScrollbarsForSmallWindows,
                        OnDisableScrollbarsForSmallWindows)
    IPC_MESSAGE_HANDLER(ViewMsg_SetRendererPrefs, OnSetRendererPrefs)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaPlayerActionAt, OnMediaPlayerActionAt)
    IPC_MESSAGE_HANDLER(ViewMsg_PluginActionAt, OnPluginActionAt)
    IPC_MESSAGE_HANDLER(ViewMsg_SetActive, OnSetActive)
    IPC_MESSAGE_HANDLER(ViewMsg_ReleaseDisambiguationPopupBitmap,
                        OnReleaseDisambiguationPopupBitmap)
    IPC_MESSAGE_HANDLER(ViewMsg_ResolveTapDisambiguation,
                        OnResolveTapDisambiguation)
    IPC_MESSAGE_HANDLER(ViewMsg_ForceRedraw, OnForceRedraw)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectWordAroundCaret, OnSelectWordAroundCaret)

    // Page messages.
    IPC_MESSAGE_HANDLER(PageMsg_UpdateWindowScreenRect,
                        OnUpdateWindowScreenRect)
    IPC_MESSAGE_HANDLER(PageMsg_SetZoomLevel, OnSetZoomLevel)
    IPC_MESSAGE_HANDLER(PageMsg_SetDeviceScaleFactor, OnSetDeviceScaleFactor);
    IPC_MESSAGE_HANDLER(PageMsg_WasHidden, OnPageWasHidden)
    IPC_MESSAGE_HANDLER(PageMsg_WasShown, OnPageWasShown)
    IPC_MESSAGE_HANDLER(PageMsg_SetHistoryOffsetAndLength,
                        OnSetHistoryOffsetAndLength)
    IPC_MESSAGE_HANDLER(PageMsg_AudioStateChanged, OnAudioStateChanged)
    IPC_MESSAGE_HANDLER(PageMsg_UpdateScreenInfo, OnUpdateScreenInfo)

#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateBrowserControlsState,
                        OnUpdateBrowserControlsState)
#elif defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_GetRenderedText,
                        OnGetRenderedText)
    IPC_MESSAGE_HANDLER(ViewMsg_Close, OnClose)
#endif
    // Adding a new message? Add platform independent ones first, then put the
    // platform specific ones at the end.

    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(handled = RenderWidget::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderViewImpl::OnSelectWordAroundCaret() {
  // Set default values for the ACK
  bool did_select = false;
  int start_adjust = 0;
  int end_adjust = 0;

  if (webview()) {
    WebLocalFrame* focused_frame = GetWebView()->FocusedFrame();
    if (focused_frame) {
      input_handler_->set_handling_input_event(true);
      blink::WebRange initial_range = focused_frame->SelectionRange();
      if (!initial_range.IsNull())
        did_select = focused_frame->SelectWordAroundCaret();
      if (did_select) {
        blink::WebRange adjusted_range = focused_frame->SelectionRange();
        start_adjust =
            adjusted_range.StartOffset() - initial_range.StartOffset();
        end_adjust = adjusted_range.EndOffset() - initial_range.EndOffset();
      }
      input_handler_->set_handling_input_event(false);
    }
  }
  Send(new ViewHostMsg_SelectWordAroundCaretAck(GetRoutingID(), did_select,
                                                start_adjust, end_adjust));
}

void RenderViewImpl::OnUpdateTargetURLAck() {
  // Check if there is a targeturl waiting to be sent.
  if (target_url_status_ == TARGET_PENDING)
    Send(new ViewHostMsg_UpdateTargetURL(GetRoutingID(), pending_target_url_));

  target_url_status_ = TARGET_NONE;
}

void RenderViewImpl::OnExecuteEditCommand(const std::string& name,
    const std::string& value) {
  if (!webview() || !webview()->FocusedFrame())
    return;

  webview()->FocusedFrame()->ExecuteCommand(WebString::FromUTF8(name),
                                            WebString::FromUTF8(value));
}

void RenderViewImpl::OnMoveCaret(const gfx::Point& point) {
  if (!webview())
    return;

  Send(new InputHostMsg_MoveCaret_ACK(GetRoutingID()));
  webview()->FocusedFrame()->MoveCaretSelection(
      ConvertWindowPointToViewport(point));
}

void RenderViewImpl::OnScrollFocusedEditableNodeIntoRect(
    const gfx::Rect& rect) {
  blink::WebAutofillClient* autofill_client = nullptr;
  if (auto* focused_frame = GetWebView()->FocusedFrame())
    autofill_client = focused_frame->AutofillClient();

  if (has_scrolled_focused_editable_node_into_rect_ &&
      rect == rect_for_scrolled_focused_editable_node_ && autofill_client) {
    autofill_client->DidCompleteFocusChangeInFrame();
    return;
  }

  if (!webview()->ScrollFocusedEditableElementIntoRect(rect))
    return;

  rect_for_scrolled_focused_editable_node_ = rect;
  has_scrolled_focused_editable_node_into_rect_ = true;
  if (!compositor()->HasPendingPageScaleAnimation() && autofill_client)
    autofill_client->DidCompleteFocusChangeInFrame();
}

void RenderViewImpl::OnSetHistoryOffsetAndLength(int history_offset,
                                                 int history_length) {
  DCHECK_GE(history_offset, -1);
  DCHECK_GE(history_length, 0);

  history_list_offset_ = history_offset;
  history_list_length_ = history_length;
}

void RenderViewImpl::OnSetInitialFocus(bool reverse) {
  if (!webview())
    return;
  webview()->SetInitialFocus(reverse);
}

void RenderViewImpl::OnUpdateWindowScreenRect(gfx::Rect window_screen_rect) {
  RenderWidget::OnUpdateWindowScreenRect(window_screen_rect);
}

void RenderViewImpl::OnAudioStateChanged(bool is_audio_playing) {
  webview()->AudioStateChanged(is_audio_playing);
}

///////////////////////////////////////////////////////////////////////////////

void RenderViewImpl::ShowCreatedPopupWidget(RenderWidget* popup_widget,
                                            WebNavigationPolicy policy,
                                            const gfx::Rect& initial_rect) {
  Send(new ViewHostMsg_ShowWidget(GetRoutingID(), popup_widget->routing_id(),
                                  initial_rect));
}

void RenderViewImpl::ShowCreatedFullscreenWidget(
    RenderWidget* fullscreen_widget,
    WebNavigationPolicy policy,
    const gfx::Rect& initial_rect) {
  Send(new ViewHostMsg_ShowFullscreenWidget(GetRoutingID(),
                                            fullscreen_widget->routing_id()));
}

void RenderViewImpl::SendFrameStateUpdates() {
  // Tell each frame with pending state to send its UpdateState message.
  for (int render_frame_routing_id : frames_with_pending_state_) {
    RenderFrameImpl* frame =
        RenderFrameImpl::FromRoutingID(render_frame_routing_id);
    if (frame)
      frame->SendUpdateState();
  }
  frames_with_pending_state_.clear();
}

void RenderViewImpl::ApplyWebPreferencesInternal(
    const WebPreferences& prefs,
    blink::WebView* web_view,
    CompositorDependencies* compositor_deps) {
  ApplyWebPreferences(prefs, web_view);
}

void RenderViewImpl::OnForceRedraw(const ui::LatencyInfo& latency_info) {
  if (RenderWidgetCompositor* rwc = compositor()) {
    rwc->QueueSwapPromise(
        base::MakeUnique<AlwaysDrawSwapPromise>(latency_info));
    rwc->SetNeedsForcedRedraw();
  }
}

// blink::WebViewClient ------------------------------------------------------

// TODO(csharrison): Migrate this method to WebFrameClient / RenderFrameImpl, as
// it is now serviced by a mojo interface scoped to the opener frame.
WebView* RenderViewImpl::CreateView(WebLocalFrame* creator,
                                    const WebURLRequest& request,
                                    const WebWindowFeatures& features,
                                    const WebString& frame_name,
                                    WebNavigationPolicy policy,
                                    bool suppress_opener) {
  RenderFrameImpl* creator_frame = RenderFrameImpl::FromWebFrame(creator);
  mojom::CreateNewWindowParamsPtr params = mojom::CreateNewWindowParams::New();
  params->user_gesture = WebUserGestureIndicator::IsProcessingUserGesture();
  if (GetContentClient()->renderer()->AllowPopup())
    params->user_gesture = true;
  params->window_container_type = WindowFeaturesToContainerType(features);
  params->session_storage_namespace_id = session_storage_namespace_id_;
  if (frame_name != "_blank")
    params->frame_name = frame_name.Utf8(
        WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);

  params->opener_suppressed = suppress_opener;
  params->disposition = NavigationPolicyToDisposition(policy);
  if (!request.IsNull()) {
    params->target_url = request.Url();
    params->referrer = GetReferrerFromRequest(creator, request);
  }
  params->features = ConvertWebWindowFeaturesToMojoWindowFeatures(features);

  // We preserve this information before sending the message since |params| is
  // moved on send.
  bool is_background_tab =
      params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;
  bool opened_by_user_gesture = params->user_gesture;

  mojom::CreateNewWindowReplyPtr reply;
  mojom::FrameHostAssociatedPtr frame_host_ptr = creator_frame->GetFrameHost();
  bool err = !frame_host_ptr->CreateNewWindow(std::move(params), &reply);
  if (err || reply->route_id == MSG_ROUTING_NONE)
    return nullptr;

  WebUserGestureIndicator::ConsumeUserGesture();

  // For Android WebView, we support a pop-up like behavior for window.open()
  // even if the embedding app doesn't support multiple windows. In this case,
  // window.open() will return "window" and navigate it to whatever URL was
  // passed.
  if (reply->route_id == GetRoutingID())
    return webview();

  // While this view may be a background extension page, it can spawn a visible
  // render view. So we just assume that the new one is not another background
  // page instead of passing on our own value.
  // TODO(vangelis): Can we tell if the new view will be a background page?
  bool never_visible = false;

  ResizeParams initial_size = ResizeParams();
  initial_size.screen_info = screen_info_;

  // The initial hidden state for the RenderViewImpl here has to match what the
  // browser will eventually decide for the given disposition. Since we have to
  // return from this call synchronously, we just have to make our best guess
  // and rely on the browser sending a WasHidden / WasShown message if it
  // disagrees.
  mojom::CreateViewParams view_params;

  view_params.opener_frame_route_id = creator_frame->GetRoutingID();
  DCHECK_EQ(GetRoutingID(), creator_frame->render_view()->GetRoutingID());

  view_params.window_was_created_with_opener = true;
  view_params.renderer_preferences = renderer_preferences_;
  view_params.web_preferences = webkit_preferences_;
  view_params.view_id = reply->route_id;
  view_params.main_frame_routing_id = reply->main_frame_route_id;
  view_params.main_frame_widget_routing_id = reply->main_frame_widget_route_id;
  view_params.session_storage_namespace_id =
      reply->cloned_session_storage_namespace_id;
  view_params.swapped_out = false;
  // WebCore will take care of setting the correct name.
  view_params.replicated_frame_state = FrameReplicationState();
  view_params.hidden = is_background_tab;
  view_params.never_visible = never_visible;
  view_params.initial_size = initial_size;
  view_params.enable_auto_resize = false;
  view_params.min_size = gfx::Size();
  view_params.max_size = gfx::Size();
  view_params.page_zoom_level = page_zoom_level_;

  // Unretained() is safe here because our calling function will also call
  // show().
  RenderWidget::ShowCallback show_callback =
      base::Bind(&RenderFrameImpl::ShowCreatedWindow,
                 base::Unretained(creator_frame), opened_by_user_gesture);

  RenderViewImpl* view =
      RenderViewImpl::Create(compositor_deps_, view_params, show_callback);

  return view->webview();
}

WebWidget* RenderViewImpl::CreatePopupMenu(blink::WebPopupType popup_type) {
  RenderWidget* widget = RenderWidget::CreateForPopup(this, compositor_deps_,
                                                      popup_type, screen_info_);
  if (!widget)
    return NULL;
  if (screen_metrics_emulator_) {
    widget->SetPopupOriginAdjustmentsForEmulation(
        screen_metrics_emulator_.get());
  }
  return widget->GetWebWidget();
}

WebStorageNamespace* RenderViewImpl::CreateSessionStorageNamespace() {
  CHECK(session_storage_namespace_id_ != kInvalidSessionStorageNamespaceId);
  return new WebStorageNamespaceImpl(session_storage_namespace_id_);
}

void RenderViewImpl::PrintPage(WebLocalFrame* frame) {
  UMA_HISTOGRAM_BOOLEAN("PrintPreview.InitiatedByScript",
                        frame->Top() == frame);

  // Logging whether the top frame is remote is sufficient in this case. If
  // the top frame is local, the printing code will function correctly and
  // the frame itself will be printed, so the cases this histogram tracks is
  // where printing of a subframe will fail as of now.
  UMA_HISTOGRAM_BOOLEAN("PrintPreview.OutOfProcessSubframe",
                        frame->Top()->IsWebRemoteFrame());

  RenderFrameImpl::FromWebFrame(frame)->ScriptedPrint(
      input_handler().handling_input_event());
}

bool RenderViewImpl::EnumerateChosenDirectory(
    const WebString& path,
    WebFileChooserCompletion* chooser_completion) {
  int id = enumeration_completion_id_++;
  enumeration_completions_[id] = chooser_completion;
  return Send(new ViewHostMsg_EnumerateDirectory(
      GetRoutingID(), id, blink::WebStringToFilePath(path)));
}

void RenderViewImpl::FrameDidStartLoading(WebFrame* frame) {
  DCHECK_GE(frames_in_progress_, 0);
  if (frames_in_progress_ == 0) {
    for (auto& observer : observers_)
      observer.DidStartLoading();
  }
  frames_in_progress_++;
}

void RenderViewImpl::FrameDidStopLoading(WebFrame* frame) {
  // TODO(japhet): This should be a DCHECK, but the pdf plugin sometimes
  // calls DidStopLoading() without a matching DidStartLoading().
  if (frames_in_progress_ == 0)
    return;
  frames_in_progress_--;
  if (frames_in_progress_ == 0) {
    for (auto& observer : observers_)
      observer.DidStopLoading();
  }
}

void RenderViewImpl::AttachWebFrameWidget(blink::WebFrameWidget* frame_widget) {
  // The previous WebFrameWidget must already be detached by CloseForFrame().
  DCHECK(!frame_widget_);
  frame_widget_ = frame_widget;
}

void RenderViewImpl::SetZoomLevel(double zoom_level) {
  // If we change the zoom level for the view, make sure any subsequent subframe
  // loads reflect the current zoom level.
  page_zoom_level_ = zoom_level;

  webview()->SetZoomLevel(zoom_level);
  for (auto& observer : observers_)
    observer.OnZoomLevelChanged();
}

void RenderViewImpl::SetValidationMessageDirection(
    base::string16* wrapped_main_text,
    blink::WebTextDirection main_text_hint,
    base::string16* wrapped_sub_text,
    blink::WebTextDirection sub_text_hint) {
  if (main_text_hint == blink::kWebTextDirectionLeftToRight) {
    *wrapped_main_text =
        base::i18n::GetDisplayStringInLTRDirectionality(*wrapped_main_text);
  } else if (main_text_hint == blink::kWebTextDirectionRightToLeft &&
             !base::i18n::IsRTL()) {
    base::i18n::WrapStringWithRTLFormatting(wrapped_main_text);
  }

  if (!wrapped_sub_text->empty()) {
    if (sub_text_hint == blink::kWebTextDirectionLeftToRight) {
      *wrapped_sub_text =
          base::i18n::GetDisplayStringInLTRDirectionality(*wrapped_sub_text);
    } else if (sub_text_hint == blink::kWebTextDirectionRightToLeft) {
      base::i18n::WrapStringWithRTLFormatting(wrapped_sub_text);
    }
  }
}

void RenderViewImpl::ShowValidationMessage(
    const blink::WebRect& anchor_in_viewport,
    const blink::WebString& main_text,
    blink::WebTextDirection main_text_hint,
    const blink::WebString& sub_text,
    blink::WebTextDirection sub_text_hint) {
  base::string16 wrapped_main_text = main_text.Utf16();
  base::string16 wrapped_sub_text = sub_text.Utf16();

  SetValidationMessageDirection(
      &wrapped_main_text, main_text_hint, &wrapped_sub_text, sub_text_hint);

  Send(new ViewHostMsg_ShowValidationMessage(
      GetRoutingID(), AdjustValidationMessageAnchor(anchor_in_viewport),
      wrapped_main_text, wrapped_sub_text));
}

void RenderViewImpl::HideValidationMessage() {
  Send(new ViewHostMsg_HideValidationMessage(GetRoutingID()));
}

void RenderViewImpl::MoveValidationMessage(
    const blink::WebRect& anchor_in_viewport) {
  Send(new ViewHostMsg_MoveValidationMessage(
      GetRoutingID(), AdjustValidationMessageAnchor(anchor_in_viewport)));
}

void RenderViewImpl::UpdateTargetURL(const GURL& url,
                                     const GURL& fallback_url) {
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
    // URLs larger than |kMaxURLChars| cannot be sent through IPC -
    // see |ParamTraits<GURL>|.
    if (latest_url.possibly_invalid_spec().size() > url::kMaxURLChars)
      latest_url = GURL();
    Send(new ViewHostMsg_UpdateTargetURL(GetRoutingID(), latest_url));
    target_url_ = latest_url;
    target_url_status_ = TARGET_INFLIGHT;
  }
}

gfx::RectF RenderViewImpl::ClientRectToPhysicalWindowRect(
    const gfx::RectF& rect) const {
  gfx::RectF window_rect = rect;
  window_rect.Scale(device_scale_factor_ * webview()->PageScaleFactor());
  return window_rect;
}

void RenderViewImpl::StartNavStateSyncTimerIfNecessary(RenderFrameImpl* frame) {
  // Keep track of which frames have pending updates.
  frames_with_pending_state_.insert(frame->GetRoutingID());

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

  // Tell each frame with pending state to inform the browser.
  nav_state_sync_timer_.Start(FROM_HERE, TimeDelta::FromSeconds(delay), this,
                              &RenderViewImpl::SendFrameStateUpdates);
}

void RenderViewImpl::SetMouseOverURL(const WebURL& url) {
  mouse_over_url_ = GURL(url);
  UpdateTargetURL(mouse_over_url_, focus_url_);
}

void RenderViewImpl::SetKeyboardFocusURL(const WebURL& url) {
  focus_url_ = GURL(url);
  UpdateTargetURL(focus_url_, mouse_over_url_);
}

bool RenderViewImpl::AcceptsLoadDrops() {
  return renderer_preferences_.can_accept_load_drops;
}

void RenderViewImpl::FocusNext() {
  Send(new ViewHostMsg_TakeFocus(GetRoutingID(), false));
}

void RenderViewImpl::FocusPrevious() {
  Send(new ViewHostMsg_TakeFocus(GetRoutingID(), true));
}

// TODO(esprehn): Blink only ever passes Elements, this should take WebElement.
void RenderViewImpl::FocusedNodeChanged(const WebNode& fromNode,
                                        const WebNode& toNode) {
  has_scrolled_focused_editable_node_into_rect_ = false;

  RenderFrameImpl* previous_frame = nullptr;
  if (!fromNode.IsNull())
    previous_frame =
        RenderFrameImpl::FromWebFrame(fromNode.GetDocument().GetFrame());
  RenderFrameImpl* new_frame = nullptr;
  if (!toNode.IsNull())
    new_frame = RenderFrameImpl::FromWebFrame(toNode.GetDocument().GetFrame());

  if (previous_frame && previous_frame != new_frame)
    previous_frame->FocusedNodeChanged(WebNode());
  if (new_frame)
    new_frame->FocusedNodeChanged(toNode);

  // TODO(dmazzoni): remove once there's a separate a11y tree per frame.
  if (main_render_frame_)
    main_render_frame_->FocusedNodeChangedForAccessibility(toNode);
}

void RenderViewImpl::DidUpdateLayout() {
  for (auto& observer : observers_)
    observer.DidUpdateLayout();

  // We don't always want to set up a timer, only if we've been put in that
  // mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;

  if (check_preferred_size_timer_.IsRunning())
    return;
  check_preferred_size_timer_.Start(FROM_HERE,
                                    TimeDelta::FromMilliseconds(0), this,
                                    &RenderViewImpl::CheckPreferredSize);
}

void RenderViewImpl::NavigateBackForwardSoon(int offset) {
  Send(new ViewHostMsg_GoToEntryAtOffset(GetRoutingID(), offset));
}

int RenderViewImpl::HistoryBackListCount() {
  return history_list_offset_ < 0 ? 0 : history_list_offset_;
}

int RenderViewImpl::HistoryForwardListCount() {
  return history_list_length_ - HistoryBackListCount() - 1;
}

// blink::WebWidgetClient ----------------------------------------------------

void RenderViewImpl::DidFocus() {
  // TODO(jcivelli): when https://bugs.webkit.org/show_bug.cgi?id=33389 is fixed
  //                 we won't have to test for user gesture anymore and we can
  //                 move that code back to render_widget.cc
  if (WebUserGestureIndicator::IsProcessingUserGesture() &&
      !RenderThreadImpl::current()->layout_test_mode()) {
    Send(new ViewHostMsg_Focus(GetRoutingID()));
  }
}

// We are supposed to get a single call to show() for a newly created RenderView
// that was created via RenderViewImpl::createView.  We wait until this point to
// run |show_callback|, which is bound to our opener's ShowCreatedViewWidget()
// method.
//
// This method provides us with the information about how to display the newly
// created RenderView (i.e., as a blocked popup or as a new tab).
void RenderViewImpl::Show(WebNavigationPolicy policy) {
  if (did_show_ && !webkit_preferences_.supports_multiple_windows) {
    // When supports_multiple_windows is disabled, popups are reusing
    // the same view. In some scenarios, this makes WebKit to call show() twice.
    return;
  }

  RenderWidget::Show(policy);
}


bool RenderViewImpl::CanHandleGestureEvent() {
  return true;
}

bool RenderViewImpl::CanUpdateLayout() {
  return true;
}

void RenderViewImpl::DidHandleGestureEvent(const WebGestureEvent& event,
                                           bool event_cancelled) {
  RenderWidget::DidHandleGestureEvent(event, event_cancelled);

  if (!event_cancelled) {
    for (auto& observer : observers_)
      observer.DidHandleGestureEvent(event);
  }
}

blink::WebLayerTreeView* RenderViewImpl::InitializeLayerTreeView() {
  // TODO(!wjmaclean): We should be able to just remove this function, and
  // expect the RenderWidget version of the function to be called instead.
  // However, we have a diamond inheritance pattern going on:
  //       WebWidgetClient
  //         |          |
  //  RenderWidget    WebViewClient
  //           |        |
  //        RenderViewImpl
  //
  // and this seems to prefer calling the empty version in WebWidgetClient
  // or WebViewClient over the non-empty one in RenderWidget.
  return RenderWidget::InitializeLayerTreeView();
}

void RenderViewImpl::CloseWidgetSoon() {
  RenderWidget::CloseWidgetSoon();
}

void RenderViewImpl::ConvertViewportToWindow(blink::WebRect* rect) {
  RenderWidget::ConvertViewportToWindow(rect);
}

void RenderViewImpl::ConvertWindowToViewport(blink::WebFloatRect* rect) {
  RenderWidget::ConvertWindowToViewport(rect);
}

void RenderViewImpl::DidAutoResize(const blink::WebSize& newSize) {
  RenderWidget::DidAutoResize(newSize);
}

void RenderViewImpl::DidOverscroll(
    const blink::WebFloatSize& overscrollDelta,
    const blink::WebFloatSize& accumulatedOverscroll,
    const blink::WebFloatPoint& positionInViewport,
    const blink::WebFloatSize& velocityInViewport) {
  RenderWidget::DidOverscroll(overscrollDelta, accumulatedOverscroll,
                              positionInViewport, velocityInViewport);
}

void RenderViewImpl::HasTouchEventHandlers(bool has_handlers) {
  RenderWidget::HasTouchEventHandlers(has_handlers);
}

blink::WebRect RenderViewImpl::RootWindowRect() {
  return RenderWidget::WindowRect();
}

blink::WebScreenInfo RenderViewImpl::GetScreenInfo() {
  return RenderWidget::GetScreenInfo();
}

void RenderViewImpl::SetToolTipText(const blink::WebString& text,
                                    blink::WebTextDirection hint) {
  RenderWidget::SetToolTipText(text, hint);
}

void RenderViewImpl::SetTouchAction(blink::WebTouchAction touchAction) {
  RenderWidget::SetTouchAction(touchAction);
}

void RenderViewImpl::ShowUnhandledTapUIIfNeeded(
    const blink::WebPoint& tappedPosition,
    const blink::WebNode& tappedNode,
    bool pageChanged) {
  RenderWidget::ShowUnhandledTapUIIfNeeded(tappedPosition, tappedNode,
                                           pageChanged);
}

blink::WebWidgetClient* RenderViewImpl::WidgetClient() {
  return static_cast<RenderWidget*>(this);
}

// blink::WebFrameClient -----------------------------------------------------

void RenderViewImpl::Repaint(const gfx::Size& size) {
  OnRepaint(size);
}

void RenderViewImpl::SetEditCommandForNextKeyEvent(const std::string& name,
                                                   const std::string& value) {
  GetWidget()->SetEditCommandForNextKeyEvent(name, value);
}

void RenderViewImpl::ClearEditCommands() {
  GetWidget()->ClearEditCommands();
}

const std::string& RenderViewImpl::GetAcceptLanguages() const {
  return renderer_preferences_.accept_languages;
}

void RenderViewImpl::ConvertViewportToWindowViaWidget(blink::WebRect* rect) {
  ConvertViewportToWindow(rect);
}

gfx::RectF RenderViewImpl::ElementBoundsInWindow(
    const blink::WebElement& element) {
  blink::WebRect bounding_box_in_window = element.BoundsInViewport();
  ConvertViewportToWindowViaWidget(&bounding_box_in_window);
  return gfx::RectF(bounding_box_in_window);
}

bool RenderViewImpl::HasAddedInputHandler() const {
  return has_added_input_handler_;
}

void RenderViewImpl::CheckPreferredSize() {
  // We don't always want to send the change messages over IPC, only if we've
  // been put in that mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (!send_preferred_size_changes_ || !webview())
    return;
  blink::WebSize tmp_size = webview()->ContentsPreferredMinimumSize();
  blink::WebRect tmp_rect(0, 0, tmp_size.width, tmp_size.height);
  ConvertViewportToWindow(&tmp_rect);
  gfx::Size size(tmp_rect.width, tmp_rect.height);
  if (size == preferred_size_)
    return;

  preferred_size_ = size;
  Send(new ViewHostMsg_DidContentsPreferredSizeChange(GetRoutingID(),
                                                      preferred_size_));
}

blink::WebString RenderViewImpl::AcceptLanguages() {
  return WebString::FromUTF8(renderer_preferences_.accept_languages);
}

// RenderView implementation ---------------------------------------------------

bool RenderViewImpl::Send(IPC::Message* message) {
  return RenderWidget::Send(message);
}

RenderWidget* RenderViewImpl::GetWidget() const {
  return const_cast<RenderWidget*>(static_cast<const RenderWidget*>(this));
}

RenderFrameImpl* RenderViewImpl::GetMainRenderFrame() {
  return main_render_frame_;
}

int RenderViewImpl::GetRoutingID() const {
  return routing_id();
}

gfx::Size RenderViewImpl::GetSize() const {
  return size();
}

float RenderViewImpl::GetDeviceScaleFactor() const {
  return device_scale_factor_;
}

const WebPreferences& RenderViewImpl::GetWebkitPreferences() {
  return webkit_preferences_;
}

void RenderViewImpl::SetWebkitPreferences(const WebPreferences& preferences) {
  OnUpdateWebPreferences(preferences);
}

blink::WebView* RenderViewImpl::GetWebView() {
  return webview();
}

blink::WebFrameWidget* RenderViewImpl::GetWebFrameWidget() {
  return frame_widget_;
}

bool RenderViewImpl::ShouldDisplayScrollbars(int width, int height) const {
  return (!send_preferred_size_changes_ ||
          (disable_scrollbars_size_limit_.width() <= width ||
           disable_scrollbars_size_limit_.height() <= height));
}

bool RenderViewImpl::GetContentStateImmediately() const {
  return send_content_state_immediately_;
}

void RenderViewImpl::OnSetPageScale(float page_scale_factor) {
  if (!webview())
    return;
  webview()->SetPageScaleFactor(page_scale_factor);
}

void RenderViewImpl::OnSetZoomLevel(
    PageMsg_SetZoomLevel_Command command,
    double zoom_level) {
  switch (command) {
    case PageMsg_SetZoomLevel_Command::CLEAR_TEMPORARY:
      uses_temporary_zoom_level_ = false;
      break;
    case PageMsg_SetZoomLevel_Command::SET_TEMPORARY:
      uses_temporary_zoom_level_ = true;
      break;
    case PageMsg_SetZoomLevel_Command::USE_CURRENT_TEMPORARY_MODE:
      // Don't override a temporary zoom level without an explicit SET.
      if (uses_temporary_zoom_level_)
        return;
      break;
    default:
      NOTIMPLEMENTED();
  }
  webview()->HidePopups();
  SetZoomLevel(zoom_level);
}

void RenderViewImpl::OnUpdateWebPreferences(const WebPreferences& prefs) {
  webkit_preferences_ = prefs;
  ApplyWebPreferencesInternal(webkit_preferences_, webview(), compositor_deps_);
}

void RenderViewImpl::OnEnumerateDirectoryResponse(
    int id,
    const std::vector<base::FilePath>& paths) {
  if (!enumeration_completions_[id])
    return;

  WebVector<WebString> ws_file_names(paths.size());
  for (size_t i = 0; i < paths.size(); ++i)
    ws_file_names[i] = blink::FilePathToWebString(paths[i]);

  enumeration_completions_[id]->DidChooseFile(ws_file_names);
  enumeration_completions_.erase(id);
}

void RenderViewImpl::OnEnableAutoResize(const gfx::Size& min_size,
                                        const gfx::Size& max_size) {
  DCHECK(disable_scrollbars_size_limit_.IsEmpty());
  if (!webview())
    return;

  auto_resize_mode_ = true;
  if (IsUseZoomForDSFEnabled()) {
    webview()->EnableAutoResizeMode(
        gfx::ScaleToCeiledSize(min_size, device_scale_factor_),
        gfx::ScaleToCeiledSize(max_size, device_scale_factor_));
  } else {
    webview()->EnableAutoResizeMode(min_size, max_size);
  }
}

void RenderViewImpl::OnDisableAutoResize(const gfx::Size& new_size) {
  DCHECK(disable_scrollbars_size_limit_.IsEmpty());
  if (!webview())
    return;
  auto_resize_mode_ = false;
  webview()->DisableAutoResizeMode();

  if (!new_size.IsEmpty()) {
    ResizeParams resize_params;
    resize_params.screen_info = screen_info_;
    resize_params.new_size = new_size;
    resize_params.physical_backing_size = physical_backing_size_;
    resize_params.browser_controls_shrink_blink_size =
        browser_controls_shrink_blink_size_;
    resize_params.top_controls_height = top_controls_height_;
    resize_params.visible_viewport_size = visible_viewport_size_;
    resize_params.is_fullscreen_granted = is_fullscreen_granted();
    resize_params.display_mode = display_mode_;
    resize_params.needs_resize_ack = false;
    Resize(resize_params);
  }
}

void RenderViewImpl::OnEnablePreferredSizeChangedMode() {
  if (send_preferred_size_changes_)
    return;
  send_preferred_size_changes_ = true;

  // Start off with an initial preferred size notification (in case
  // |didUpdateLayout| was already called).
  DidUpdateLayout();
}

void RenderViewImpl::OnDisableScrollbarsForSmallWindows(
    const gfx::Size& disable_scrollbar_size_limit) {
  disable_scrollbars_size_limit_ = disable_scrollbar_size_limit;
}

void RenderViewImpl::OnSetRendererPrefs(
    const RendererPreferences& renderer_prefs) {
  std::string old_accept_languages = renderer_preferences_.accept_languages;

  renderer_preferences_ = renderer_prefs;

  UpdateFontRenderingFromRendererPrefs();
  UpdateThemePrefs();
  blink::SetCaretBlinkInterval(renderer_prefs.caret_blink_interval);

#if BUILDFLAG(USE_DEFAULT_RENDER_THEME)
  if (renderer_prefs.use_custom_colors) {
    blink::SetFocusRingColor(renderer_prefs.focus_ring_color);

    if (webview()) {
      webview()->SetSelectionColors(renderer_prefs.active_selection_bg_color,
                                    renderer_prefs.active_selection_fg_color,
                                    renderer_prefs.inactive_selection_bg_color,
                                    renderer_prefs.inactive_selection_fg_color);
      webview()->ThemeChanged();
    }
  }
#endif  // BUILDFLAG(USE_DEFAULT_RENDER_THEME)

  if (webview() &&
      old_accept_languages != renderer_preferences_.accept_languages) {
    webview()->AcceptLanguagesChanged();
  }
}

void RenderViewImpl::OnMediaPlayerActionAt(const gfx::Point& location,
                                           const WebMediaPlayerAction& action) {
  if (webview())
    webview()->PerformMediaPlayerAction(action, location);
}

void RenderViewImpl::OnPluginActionAt(const gfx::Point& location,
                                      const WebPluginAction& action) {
  if (webview())
    webview()->PerformPluginAction(action, location);
}

void RenderViewImpl::OnClosePage() {
  // ViewMsg_ClosePage should only be sent to active, non-swapped-out views.
  DCHECK(webview()->MainFrame()->IsWebLocalFrame());

  // TODO(creis): We'd rather use webview()->Close() here, but that currently
  // sets the WebView's delegate_ to NULL, preventing any JavaScript dialogs
  // in the onunload handler from appearing.  For now, we're bypassing that and
  // calling the FrameLoader's CloseURL method directly.  This should be
  // revisited to avoid having two ways to close a page.  Having a single way
  // to close that can run onunload is also useful for fixing
  // http://b/issue?id=753080.
  webview()->MainFrame()->ToWebLocalFrame()->DispatchUnloadEvent();

  Send(new ViewHostMsg_ClosePage_ACK(GetRoutingID()));
}

void RenderViewImpl::OnClose() {
  if (closing_)
    RenderThread::Get()->Send(new ViewHostMsg_Close_ACK(GetRoutingID()));
  RenderWidget::OnClose();
}

void RenderViewImpl::OnMoveOrResizeStarted() {
  if (webview())
    webview()->HidePopups();
}

void RenderViewImpl::ResizeWebWidget() {
  webview()->ResizeWithBrowserControls(GetSizeForWebWidget(),
                                       top_controls_height_,
                                       browser_controls_shrink_blink_size_);
}

void RenderViewImpl::OnResize(const ResizeParams& params) {
  TRACE_EVENT0("renderer", "RenderViewImpl::OnResize");
  if (webview()) {
    webview()->HidePopups();
    if (send_preferred_size_changes_ &&
        webview()->MainFrame()->IsWebLocalFrame()) {
      webview()->MainFrame()->ToWebLocalFrame()->SetCanHaveScrollbars(
          ShouldDisplayScrollbars(params.new_size.width(),
                                  params.new_size.height()));
    }
    if (display_mode_ != params.display_mode) {
      display_mode_ = params.display_mode;
      webview()->SetDisplayMode(display_mode_);
    }
  }

  gfx::Size old_visible_viewport_size = visible_viewport_size_;

  browser_controls_shrink_blink_size_ =
      params.browser_controls_shrink_blink_size;
  top_controls_height_ = params.top_controls_height;

  RenderWidget::OnResize(params);

  if (old_visible_viewport_size != visible_viewport_size_)
    has_scrolled_focused_editable_node_into_rect_ = false;
}

void RenderViewImpl::OnSetBackgroundOpaque(bool opaque) {
  if (!frame_widget_)
    return;

  if (opaque) {
    frame_widget_->ClearBaseBackgroundColorOverride();
    frame_widget_->ClearBackgroundColorOverride();
  } else {
    frame_widget_->SetBaseBackgroundColorOverride(SK_ColorTRANSPARENT);
    frame_widget_->SetBackgroundColorOverride(SK_ColorTRANSPARENT);
  }
}

void RenderViewImpl::OnSetActive(bool active) {
  if (webview())
    webview()->SetIsActive(active);
}

blink::WebWidget* RenderViewImpl::GetWebWidget() const {
  if (frame_widget_)
    return frame_widget_;

  return RenderWidget::GetWebWidget();
}

void RenderViewImpl::CloseForFrame() {
  DCHECK(frame_widget_);
  frame_widget_->Close();
  frame_widget_ = nullptr;
}

void RenderViewImpl::Close() {
  // We need to grab a pointer to the doomed WebView before we destroy it.
  WebView* doomed = webview_;
  RenderWidget::Close();
  webview_ = nullptr;
  g_view_map.Get().erase(doomed);
  g_routing_id_view_map.Get().erase(GetRoutingID());
  RenderThread::Get()->Send(new ViewHostMsg_Close_ACK(GetRoutingID()));
}

void RenderViewImpl::OnPageWasHidden() {
#if defined(OS_ANDROID)
  SuspendVideoCaptureDevices(true);
#if BUILDFLAG(ENABLE_WEBRTC)
  if (speech_recognition_dispatcher_)
    speech_recognition_dispatcher_->AbortAllRecognitions();
#endif
#endif

  if (webview()) {
    // TODO(lfg): It's not correct to defer the page visibility to the main
    // frame. Currently, this is done because the main frame may override the
    // visibility of the page when prerendering. In order to fix this,
    // prerendering must be made aware of OOPIFs. https://crbug.com/440544
    blink::WebPageVisibilityState visibilityState =
        GetMainRenderFrame() ? GetMainRenderFrame()->VisibilityState()
                             : blink::kWebPageVisibilityStateHidden;
    webview()->SetVisibilityState(visibilityState, false);
  }
}

void RenderViewImpl::OnPageWasShown() {
#if defined(OS_ANDROID)
  SuspendVideoCaptureDevices(false);
#endif

  if (webview()) {
    blink::WebPageVisibilityState visibilityState =
        GetMainRenderFrame() ? GetMainRenderFrame()->VisibilityState()
                             : blink::kWebPageVisibilityStateVisible;
    webview()->SetVisibilityState(visibilityState, false);
  }
}

void RenderViewImpl::OnUpdateScreenInfo(const ScreenInfo& screen_info) {
  // This IPC only updates the screen info on RenderViews that have a remote
  // main frame. For local main frames, the ScreenInfo is updated in
  // ViewMsg_Resize.
  if (!main_render_frame_)
    screen_info_ = screen_info;
}

GURL RenderViewImpl::GetURLForGraphicsContext3D() {
  DCHECK(webview());
  if (webview()->MainFrame()->IsWebLocalFrame())
    return GURL(webview()->MainFrame()->GetDocument().Url());
  else
    return GURL("chrome://gpu/RenderViewImpl::CreateGraphicsContext3D");
}

void RenderViewImpl::OnSetFocus(bool enable) {
  // This message must always be received when the main frame is a
  // WebLocalFrame.
  CHECK(webview()->MainFrame()->IsWebLocalFrame());
  SetFocus(enable);
}

void RenderViewImpl::SetFocus(bool enable) {
  RenderWidget::OnSetFocus(enable);

  // Notify all BrowserPlugins of the RenderView's focus state.
  if (BrowserPluginManager::Get())
    BrowserPluginManager::Get()->UpdateFocusState();
}

void RenderViewImpl::DidCompletePageScaleAnimation() {
  if (auto* focused_frame = GetWebView()->FocusedFrame()) {
    if (focused_frame->AutofillClient())
      focused_frame->AutofillClient()->DidCompleteFocusChangeInFrame();
  }
}

void RenderViewImpl::OnDeviceScaleFactorChanged() {
  RenderWidget::OnDeviceScaleFactorChanged();
  UpdateWebViewWithDeviceScaleFactor();
  if (auto_resize_mode_)
    AutoResizeCompositor();
}

void RenderViewImpl::SetScreenMetricsEmulationParameters(
    bool enabled,
    const blink::WebDeviceEmulationParams& params) {
  if (webview() && compositor()) {
    if (enabled)
      webview()->EnableDeviceEmulation(params);
    else
      webview()->DisableDeviceEmulation();
  }
}

blink::WebSpeechRecognizer* RenderViewImpl::SpeechRecognizer() {
  if (!speech_recognition_dispatcher_)
    speech_recognition_dispatcher_ = new SpeechRecognitionDispatcher(this);
  return speech_recognition_dispatcher_;
}

void RenderViewImpl::ZoomLimitsChanged(double minimum_level,
                                       double maximum_level) {
  // Round the double to avoid returning incorrect minimum/maximum zoom
  // percentages.
  int minimum_percent = round(
      ZoomLevelToZoomFactor(minimum_level) * 100);
  int maximum_percent = round(
      ZoomLevelToZoomFactor(maximum_level) * 100);

  Send(new ViewHostMsg_UpdateZoomLimits(GetRoutingID(), minimum_percent,
                                        maximum_percent));
}

void RenderViewImpl::PageScaleFactorChanged() {
  if (!webview())
    return;

  Send(new ViewHostMsg_PageScaleFactorChanged(GetRoutingID(),
                                              webview()->PageScaleFactor()));
}

double RenderViewImpl::zoomLevelToZoomFactor(double zoom_level) const {
  return ZoomLevelToZoomFactor(zoom_level);
}

double RenderViewImpl::zoomFactorToZoomLevel(double factor) const {
  return ZoomFactorToZoomLevel(factor);
}

void RenderViewImpl::PageImportanceSignalsChanged() {
  if (!webview() || !main_render_frame_)
    return;

  auto* web_signals = webview()->PageImportanceSignals();

  PageImportanceSignals signals;
  signals.had_form_interaction = web_signals->HadFormInteraction();

  main_render_frame_->Send(new FrameHostMsg_UpdatePageImportanceSignals(
      main_render_frame_->GetRoutingID(), signals));
}

#if defined(OS_ANDROID)
bool RenderViewImpl::OpenDateTimeChooser(
    const blink::WebDateTimeChooserParams& params,
    blink::WebDateTimeChooserCompletion* completion) {
  // JavaScript may try to open a date time chooser while one is already open.
  if (date_time_picker_client_)
    return false;
  date_time_picker_client_.reset(
      new RendererDateTimePicker(this, params, completion));
  return date_time_picker_client_->Open();
}

void RenderViewImpl::DismissDateTimeDialog() {
  DCHECK(date_time_picker_client_);
  date_time_picker_client_.reset(NULL);
}

bool RenderViewImpl::DidTapMultipleTargets(
    const WebSize& inner_viewport_offset,
    const WebRect& touch_rect,
    const WebVector<WebRect>& target_rects) {
  // Never show a disambiguation popup when accessibility is enabled,
  // as this interferes with "touch exploration".
  AccessibilityMode accessibility_mode =
      GetMainRenderFrame()->accessibility_mode();
  if (accessibility_mode == kAccessibilityModeComplete)
    return false;

  // The touch_rect, target_rects and zoom_rect are in the outer viewport
  // reference frame.
  gfx::Rect zoom_rect;
  float new_total_scale =
      DisambiguationPopupHelper::ComputeZoomAreaAndScaleFactor(
          touch_rect, target_rects, GetSize(),
          gfx::Rect(webview()->MainFrame()->VisibleContentRect()).size(),
          device_scale_factor_ * webview()->PageScaleFactor(), &zoom_rect);
  if (!new_total_scale || zoom_rect.IsEmpty())
    return false;

  bool handled = false;
  switch (renderer_preferences_.tap_multiple_targets_strategy) {
    case TAP_MULTIPLE_TARGETS_STRATEGY_ZOOM:
      handled = webview()->ZoomToMultipleTargetsRect(zoom_rect);
      break;
    case TAP_MULTIPLE_TARGETS_STRATEGY_POPUP: {
      gfx::Size canvas_size =
          gfx::ScaleToCeiledSize(zoom_rect.size(), new_total_scale);
      cc::SharedBitmapManager* manager =
          RenderThreadImpl::current()->shared_bitmap_manager();
      std::unique_ptr<cc::SharedBitmap> shared_bitmap =
          manager->AllocateSharedBitmap(canvas_size);
      CHECK(!!shared_bitmap);
      {
        SkBitmap bitmap;
        SkImageInfo info = SkImageInfo::MakeN32Premul(canvas_size.width(),
                                                      canvas_size.height());
        bitmap.installPixels(info, shared_bitmap->pixels(), info.minRowBytes());
        cc::SkiaPaintCanvas canvas(bitmap);

        // TODO(trchen): Cleanup the device scale factor mess.
        // device scale will be applied in WebKit
        // --> zoom_rect doesn't include device scale,
        //     but WebKit will still draw on zoom_rect * device_scale_factor_
        canvas.scale(new_total_scale / device_scale_factor_,
                     new_total_scale / device_scale_factor_);
        canvas.translate(-zoom_rect.x() * device_scale_factor_,
                         -zoom_rect.y() * device_scale_factor_);

        DCHECK(webview_->IsAcceleratedCompositingActive());
        webview_->PaintIgnoringCompositing(&canvas, zoom_rect);
      }

      gfx::Rect zoom_rect_in_screen =
          zoom_rect - gfx::Vector2d(inner_viewport_offset.width,
                                    inner_viewport_offset.height);

      gfx::Rect physical_window_zoom_rect = gfx::ToEnclosingRect(
          ClientRectToPhysicalWindowRect(gfx::RectF(zoom_rect_in_screen)));

      Send(new ViewHostMsg_ShowDisambiguationPopup(
          GetRoutingID(), physical_window_zoom_rect, canvas_size,
          shared_bitmap->id()));
      cc::SharedBitmapId id = shared_bitmap->id();
      disambiguation_bitmaps_[id] = shared_bitmap.release();
      handled = true;
      break;
    }
    case TAP_MULTIPLE_TARGETS_STRATEGY_NONE:
      // No-op.
      break;
  }

  return handled;
}

void RenderViewImpl::SuspendVideoCaptureDevices(bool suspend) {
#if BUILDFLAG(ENABLE_WEBRTC)
  if (!main_render_frame_)
    return;

  MediaStreamDispatcher* media_stream_dispatcher =
      main_render_frame_->GetMediaStreamDispatcher();
  if (!media_stream_dispatcher)
    return;

  StreamDeviceInfoArray video_array =
      media_stream_dispatcher->GetNonScreenCaptureDevices();
  RenderThreadImpl::current()->video_capture_impl_manager()->SuspendDevices(
      video_array, suspend);
#endif  // BUILDFLAG(ENABLE_WEBRTC)
}
#endif  // defined(OS_ANDROID)

unsigned RenderViewImpl::GetLocalSessionHistoryLengthForTesting() const {
  return history_list_length_;
}

void RenderViewImpl::SetFocusAndActivateForTesting(bool enable) {
  if (enable) {
    if (has_focus())
      return;
    OnSetActive(true);
    OnSetFocus(true);
  } else {
    if (!has_focus())
      return;
    OnSetFocus(false);
    OnSetActive(false);
  }
}

void RenderViewImpl::SetDeviceScaleFactorForTesting(float factor) {
  ResizeParams params;
  params.screen_info = screen_info_;
  params.screen_info.device_scale_factor = factor;
  params.new_size = size();
  params.visible_viewport_size = visible_viewport_size_;
  params.physical_backing_size = gfx::ScaleToCeiledSize(size(), factor);
  params.browser_controls_shrink_blink_size = false;
  params.top_controls_height = 0.f;
  params.is_fullscreen_granted = is_fullscreen_granted();
  params.display_mode = display_mode_;
  OnResize(params);
}

void RenderViewImpl::SetDeviceColorProfileForTesting(
    const gfx::ICCProfile& icc_profile) {
  ResizeParams params;
  params.screen_info = screen_info_;
  params.screen_info.icc_profile = icc_profile;
  params.new_size = size();
  params.visible_viewport_size = visible_viewport_size_;
  params.physical_backing_size = physical_backing_size_;
  params.browser_controls_shrink_blink_size = false;
  params.top_controls_height = 0.f;
  params.is_fullscreen_granted = is_fullscreen_granted();
  params.display_mode = display_mode_;
  OnResize(params);
}

void RenderViewImpl::ForceResizeForTesting(const gfx::Size& new_size) {
  gfx::Rect new_window_rect(RootWindowRect().x, RootWindowRect().y,
                            new_size.width(), new_size.height());
  SetWindowRectSynchronously(new_window_rect);
}

void RenderViewImpl::UseSynchronousResizeModeForTesting(bool enable) {
  resizing_mode_selector_->set_is_synchronous_mode(enable);
}

void RenderViewImpl::EnableAutoResizeForTesting(const gfx::Size& min_size,
                                                const gfx::Size& max_size) {
  OnEnableAutoResize(min_size, max_size);
}

void RenderViewImpl::DisableAutoResizeForTesting(const gfx::Size& new_size) {
  OnDisableAutoResize(new_size);
}

void RenderViewImpl::OnReleaseDisambiguationPopupBitmap(
    const cc::SharedBitmapId& id) {
  BitmapMap::iterator it = disambiguation_bitmaps_.find(id);
  DCHECK(it != disambiguation_bitmaps_.end());
  delete it->second;
  disambiguation_bitmaps_.erase(it);
}

void RenderViewImpl::OnResolveTapDisambiguation(double timestamp_seconds,
                                                gfx::Point tap_viewport_offset,
                                                bool is_long_press) {
  webview()->ResolveTapDisambiguation(timestamp_seconds, tap_viewport_offset,
                                      is_long_press);
}

void RenderViewImpl::DidCommitCompositorFrame() {
  RenderWidget::DidCommitCompositorFrame();
  for (auto& observer : observers_)
    observer.DidCommitCompositorFrame();
}

void RenderViewImpl::UpdateWebViewWithDeviceScaleFactor() {
  if (!webview())
    return;
  if (IsUseZoomForDSFEnabled()) {
    webview()->SetZoomFactorForDeviceScaleFactor(device_scale_factor_);
  } else {
    webview()->SetDeviceScaleFactor(device_scale_factor_);
  }
  webview()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      PreferCompositingToLCDText(compositor_deps_, device_scale_factor_));
}

void RenderViewImpl::OnDiscardInputEvent(
    const blink::WebInputEvent* input_event,
    const std::vector<const blink::WebInputEvent*>& coalesced_events,
    const ui::LatencyInfo& latency_info,
    InputEventDispatchType dispatch_type) {
  if (!input_event || dispatch_type == DISPATCH_TYPE_NON_BLOCKING) {
    return;
  }

  InputEventAck ack(InputEventAckSource::MAIN_THREAD, input_event->GetType(),
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  Send(new InputHostMsg_HandleInputEvent_ACK(routing_id_, ack));
}

void RenderViewImpl::HandleInputEvent(
    const blink::WebCoalescedInputEvent& input_event,
    const ui::LatencyInfo& latency_info,
    HandledEventCallback callback) {
  if (is_swapped_out_) {
    std::move(callback).Run(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, latency_info,
                            nullptr);
    return;
  }
  idle_user_detector_->ActivityDetected();
  RenderWidget::HandleInputEvent(input_event, latency_info,
                                 std::move(callback));
}

}  // namespace content
