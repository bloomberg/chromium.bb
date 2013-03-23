// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "cc/base/switches.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_guest_manager.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/download/save_package.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/interstitial_page_impl.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_view_guest.h"
#include "content/browser/webui/generic_handler.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/icon_messages.h"
#include "content/common/ssl_status_serialization.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/compositor_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/load_from_memory_cache_details.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_restriction.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/layout.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gl/gl_switches.h"
#include "webkit/glue/webpreferences.h"

#if defined(OS_ANDROID)
#include "content/browser/android/date_time_chooser_android.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "ui/surface/io_surface_support_mac.h"
#endif

#if defined(ENABLE_JAVA_BRIDGE)
#include "content/browser/renderer_host/java/java_bridge_dispatcher_host_manager.h"
#endif

// Cross-Site Navigations
//
// If a WebContentsImpl is told to navigate to a different web site (as
// determined by SiteInstance), it will replace its current RenderViewHost with
// a new RenderViewHost dedicated to the new SiteInstance.  This works as
// follows:
//
// - Navigate determines whether the destination is cross-site, and if so,
//   it creates a pending_render_view_host_.
// - The pending RVH is "suspended," so that no navigation messages are sent to
//   its renderer until the onbeforeunload JavaScript handler has a chance to
//   run in the current RVH.
// - The pending RVH tells CrossSiteRequestManager (a thread-safe singleton)
//   that it has a pending cross-site request.  ResourceDispatcherHost will
//   check for this when the response arrives.
// - The current RVH runs its onbeforeunload handler.  If it returns false, we
//   cancel all the pending logic.  Otherwise we allow the pending RVH to send
//   the navigation request to its renderer.
// - ResourceDispatcherHost receives a ResourceRequest on the IO thread for the
//   main resource load on the pending RVH. It checks CrossSiteRequestManager
//   to see that it is a cross-site request, and installs a
//   CrossSiteResourceHandler.
// - When RDH receives a response, the BufferedResourceHandler determines
//   whether it is a download.  If so, it sends a message to the new renderer
//   causing it to cancel the request, and the download proceeds. For now, the
//   pending RVH remains until the next DidNavigate event for this
//   WebContentsImpl. This isn't ideal, but it doesn't affect any functionality.
// - After RDH receives a response and determines that it is safe and not a
//   download, it pauses the response to first run the old page's onunload
//   handler.  It does this by asynchronously calling the OnCrossSiteResponse
//   method of WebContentsImpl on the UI thread, which sends a SwapOut message
//   to the current RVH.
// - Once the onunload handler is finished, a SwapOut_ACK message is sent to
//   the ResourceDispatcherHost, who unpauses the response.  Data is then sent
//   to the pending RVH.
// - The pending renderer sends a FrameNavigate message that invokes the
//   DidNavigate method.  This replaces the current RVH with the
//   pending RVH.
// - The previous renderer is kept swapped out in RenderViewHostManager in case
//   the user goes back.  The process only stays live if another tab is using
//   it, but if so, the existing frame relationships will be maintained.

using webkit_glue::WebPreferences;

namespace content {
namespace {

// Amount of time we wait between when a key event is received and the renderer
// is queried for its state and pushed to the NavigationEntry.
const int kQueryStateDelay = 5000;

const int kSyncWaitDelay = 40;

const char kDotGoogleDotCom[] = ".google.com";

static int StartDownload(content::RenderViewHost* rvh,
                         const GURL& url,
                         bool is_favicon,
                         int image_size) {
  static int g_next_favicon_download_id = 0;
  rvh->Send(new IconMsg_DownloadFavicon(rvh->GetRoutingID(),
                                        ++g_next_favicon_download_id,
                                        url,
                                        is_favicon,
                                        image_size));
  return g_next_favicon_download_id;
}

ViewMsg_Navigate_Type::Value GetNavigationType(
    BrowserContext* browser_context, const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type) {
  switch (reload_type) {
    case NavigationControllerImpl::RELOAD:
      return ViewMsg_Navigate_Type::RELOAD;
    case NavigationControllerImpl::RELOAD_IGNORING_CACHE:
      return ViewMsg_Navigate_Type::RELOAD_IGNORING_CACHE;
    case NavigationControllerImpl::RELOAD_ORIGINAL_REQUEST_URL:
      return ViewMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
    case NavigationControllerImpl::NO_RELOAD:
      break;  // Fall through to rest of function.
  }

  // |RenderViewImpl::PopulateStateFromPendingNavigationParams| differentiates
  // between |RESTORE_WITH_POST| and |RESTORE|.
  if (entry.restore_type() ==
      NavigationEntryImpl::RESTORE_LAST_SESSION_EXITED_CLEANLY) {
    if (entry.GetHasPostData())
      return ViewMsg_Navigate_Type::RESTORE_WITH_POST;
    return ViewMsg_Navigate_Type::RESTORE;
  }

  return ViewMsg_Navigate_Type::NORMAL;
}

void MakeNavigateParams(const NavigationEntryImpl& entry,
                        const NavigationControllerImpl& controller,
                        WebContentsDelegate* delegate,
                        NavigationController::ReloadType reload_type,
                        ViewMsg_Navigate_Params* params) {
  params->page_id = entry.GetPageID();
  params->pending_history_list_offset = controller.GetIndexOfEntry(&entry);
  params->current_history_list_offset = controller.GetLastCommittedEntryIndex();
  params->current_history_list_length = controller.GetEntryCount();
  if (!entry.GetBaseURLForDataURL().is_empty()) {
    params->base_url_for_data_url = entry.GetBaseURLForDataURL();
    params->history_url_for_data_url = entry.GetVirtualURL();
  }
  params->referrer = entry.GetReferrer();
  params->transition = entry.GetTransitionType();
  params->state = entry.GetContentState();
  params->navigation_type =
      GetNavigationType(controller.GetBrowserContext(), entry, reload_type);
  params->request_time = base::Time::Now();
  params->extra_headers = entry.extra_headers();
  params->transferred_request_child_id =
      entry.transferred_global_request_id().child_id;
  params->transferred_request_request_id =
      entry.transferred_global_request_id().request_id;
  params->is_overriding_user_agent = entry.GetIsOverridingUserAgent();
  // Avoid downloading when in view-source mode.
  params->allow_download = !entry.IsViewSourceMode();
  params->is_post = entry.GetHasPostData();
  if(entry.GetBrowserInitiatedPostData()) {
      params->browser_initiated_post_data.assign(
          entry.GetBrowserInitiatedPostData()->front(),
          entry.GetBrowserInitiatedPostData()->front() +
              entry.GetBrowserInitiatedPostData()->size());

  }

  if (reload_type == NavigationControllerImpl::RELOAD_ORIGINAL_REQUEST_URL &&
      entry.GetOriginalRequestURL().is_valid() && !entry.GetHasPostData()) {
    // We may have been redirected when navigating to the current URL.
    // Use the URL the user originally intended to visit, if it's valid and if a
    // POST wasn't involved; the latter case avoids issues with sending data to
    // the wrong page.
    params->url = entry.GetOriginalRequestURL();
  } else {
    params->url = entry.GetURL();
  }

  params->can_load_local_resources = entry.GetCanLoadLocalResources();
  params->frame_to_navigate = entry.GetFrameToNavigate();

  if (delegate)
    delegate->AddNavigationHeaders(params->url, &params->extra_headers);
}

}  // namespace

WebContents* WebContents::Create(const WebContents::CreateParams& params) {
  return WebContentsImpl::CreateWithOpener(params, NULL);
}

WebContents* WebContents::CreateWithSessionStorage(
    const WebContents::CreateParams& params,
    const SessionStorageNamespaceMap& session_storage_namespace_map) {
  WebContentsImpl* new_contents = new WebContentsImpl(
      params.browser_context, NULL);

  for (SessionStorageNamespaceMap::const_iterator it =
           session_storage_namespace_map.begin();
       it != session_storage_namespace_map.end();
       ++it) {
    new_contents->GetController().SetSessionStorageNamespace(it->first,
                                                             it->second);
  }

  new_contents->Init(params);
  return new_contents;
}

WebContents* WebContents::FromRenderViewHost(const RenderViewHost* rvh) {
  return rvh->GetDelegate()->GetAsWebContents();
}

// WebContentsImpl -------------------------------------------------------------

WebContentsImpl::WebContentsImpl(
    BrowserContext* browser_context,
    WebContentsImpl* opener)
    : delegate_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(controller_(this, browser_context)),
      render_view_host_delegate_view_(NULL),
      opener_(opener),
      ALLOW_THIS_IN_INITIALIZER_LIST(render_manager_(this, this, this)),
      is_loading_(false),
      crashed_status_(base::TERMINATION_STATUS_STILL_RUNNING),
      crashed_error_code_(0),
      waiting_for_response_(false),
      load_state_(net::LOAD_STATE_IDLE, string16()),
      upload_size_(0),
      upload_position_(0),
      displayed_insecure_content_(false),
      capturer_count_(0),
      should_normally_be_visible_(true),
      is_being_destroyed_(false),
      notify_disconnection_(false),
      dialog_manager_(NULL),
      is_showing_before_unload_dialog_(false),
      closed_by_user_gesture_(false),
      minimum_zoom_percent_(static_cast<int>(kMinimumZoomFactor * 100)),
      maximum_zoom_percent_(static_cast<int>(kMaximumZoomFactor * 100)),
      temporary_zoom_settings_(false),
      content_restrictions_(0),
      color_chooser_(NULL),
      message_source_(NULL),
      fullscreen_widget_routing_id_(MSG_ROUTING_NONE) {
}

WebContentsImpl::~WebContentsImpl() {
  is_being_destroyed_ = true;

  for (std::set<RenderWidgetHostImpl*>::iterator iter =
           created_widgets_.begin(); iter != created_widgets_.end(); ++iter) {
    (*iter)->DetachDelegate();
  }
  created_widgets_.clear();

  // Clear out any JavaScript state.
  if (dialog_manager_)
    dialog_manager_->ResetJavaScriptState(this);

  if (color_chooser_)
    color_chooser_->End();

  NotifyDisconnected();

  // Notify any observer that have a reference on this WebContents.
  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_DESTROYED,
      Source<WebContents>(this),
      NotificationService::NoDetails());

  // TODO(brettw) this should be moved to the view.
#if defined(OS_WIN) && !defined(USE_AURA)
  // If we still have a window handle, destroy it. GetNativeView can return
  // NULL if this contents was part of a window that closed.
  if (view_->GetNativeView()) {
    RenderViewHost* host = GetRenderViewHost();
    if (host && host->GetView())
      RenderWidgetHostViewPort::FromRWHV(host->GetView())->WillWmDestroy();
  }
#endif

  // OnCloseStarted isn't called in unit tests.
  if (!close_start_time_.is_null()) {
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeTicks unload_start_time = close_start_time_;
    if (!before_unload_end_time_.is_null())
      unload_start_time = before_unload_end_time_;
    UMA_HISTOGRAM_TIMES("Tab.Close", now - close_start_time_);
    UMA_HISTOGRAM_TIMES("Tab.Close.UnloadTime", now - unload_start_time);
  }

  FOR_EACH_OBSERVER(WebContentsObserver,
                    observers_,
                    WebContentsImplDestroyed());

  SetDelegate(NULL);
}

WebContentsImpl* WebContentsImpl::CreateWithOpener(
    const WebContents::CreateParams& params,
    WebContentsImpl* opener) {
  WebContentsImpl* new_contents = new WebContentsImpl(
      params.browser_context, opener);

  new_contents->Init(params);
  return new_contents;
}

// static
BrowserPluginGuest* WebContentsImpl::CreateGuest(
    BrowserContext* browser_context,
    SiteInstance* site_instance,
    int guest_instance_id) {
  WebContentsImpl* new_contents = new WebContentsImpl(browser_context, NULL);

  // This makes |new_contents| act as a guest.
  // For more info, see comment above class BrowserPluginGuest.
  new_contents->browser_plugin_guest_.reset(
      BrowserPluginGuest::Create(guest_instance_id, new_contents));

  WebContents::CreateParams create_params(browser_context, site_instance);
  new_contents->Init(create_params);

  // We are instantiating a WebContents for browser plugin. Set its subframe bit
  // to true.
  static_cast<RenderViewHostImpl*>(
      new_contents->GetRenderViewHost())->set_is_subframe(true);

  return new_contents->browser_plugin_guest_.get();
}

WebPreferences WebContentsImpl::GetWebkitPrefs(RenderViewHost* rvh,
                                               const GURL& url) {
  WebPreferences prefs;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  prefs.developer_extras_enabled = true;
  prefs.javascript_enabled =
      !command_line.HasSwitch(switches::kDisableJavaScript);
  prefs.web_security_enabled =
      !command_line.HasSwitch(switches::kDisableWebSecurity);
  prefs.plugins_enabled =
      !command_line.HasSwitch(switches::kDisablePlugins);
  prefs.java_enabled =
      !command_line.HasSwitch(switches::kDisableJava);

  prefs.uses_page_cache =
      command_line.HasSwitch(switches::kEnableFastback);
  prefs.remote_fonts_enabled =
      !command_line.HasSwitch(switches::kDisableRemoteFonts);
  prefs.xss_auditor_enabled =
      !command_line.HasSwitch(switches::kDisableXSSAuditor);
  prefs.application_cache_enabled =
      !command_line.HasSwitch(switches::kDisableApplicationCache);

  prefs.local_storage_enabled =
      !command_line.HasSwitch(switches::kDisableLocalStorage);
  prefs.databases_enabled =
      !command_line.HasSwitch(switches::kDisableDatabases);
#if defined(OS_ANDROID)
  prefs.webaudio_enabled =
      command_line.HasSwitch(switches::kEnableWebAudio);
#else
  prefs.webaudio_enabled =
      !command_line.HasSwitch(switches::kDisableWebAudio);
#endif

  prefs.experimental_webgl_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisable3DAPIs) &&
#if defined(OS_ANDROID)
      command_line.HasSwitch(switches::kEnableExperimentalWebGL);
#else
      !command_line.HasSwitch(switches::kDisableExperimentalWebGL);
#endif

  prefs.flash_3d_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlash3d);
  prefs.flash_stage3d_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlashStage3d);
  prefs.flash_stage3d_baseline_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableFlashStage3d);

  prefs.gl_multisampling_enabled =
      !command_line.HasSwitch(switches::kDisableGLMultisampling);
  prefs.privileged_webgl_extensions_enabled =
      command_line.HasSwitch(switches::kEnablePrivilegedWebGLExtensions);
  prefs.site_specific_quirks_enabled =
      !command_line.HasSwitch(switches::kDisableSiteSpecificQuirks);
  prefs.allow_file_access_from_file_urls =
      command_line.HasSwitch(switches::kAllowFileAccessFromFiles);

  prefs.accelerated_compositing_for_overflow_scroll_enabled = false;
  if (command_line.HasSwitch(switches::kEnableAcceleratedOverflowScroll))
    prefs.accelerated_compositing_for_overflow_scroll_enabled = true;
  if (command_line.HasSwitch(switches::kDisableAcceleratedOverflowScroll))
    prefs.accelerated_compositing_for_overflow_scroll_enabled = false;

  prefs.accelerated_compositing_for_scrollable_frames_enabled =
      command_line.HasSwitch(switches::kEnableAcceleratedScrollableFrames);
  prefs.composited_scrolling_for_frames_enabled =
      command_line.HasSwitch(switches::kEnableCompositedScrollingForFrames);
  prefs.show_paint_rects =
      command_line.HasSwitch(switches::kShowPaintRects);
  prefs.accelerated_compositing_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableAcceleratedCompositing);
  prefs.force_compositing_mode =
      content::IsForceCompositingModeEnabled() &&
      !command_line.HasSwitch(switches::kDisableForceCompositingMode);
  prefs.accelerated_2d_canvas_enabled =
      GpuProcessHost::gpu_enabled() &&
      !command_line.HasSwitch(switches::kDisableAccelerated2dCanvas);
  prefs.deferred_2d_canvas_enabled =
      !command_line.HasSwitch(switches::kDisableDeferred2dCanvas);
  prefs.antialiased_2d_canvas_disabled =
      command_line.HasSwitch(switches::kDisable2dCanvasAntialiasing);
  prefs.accelerated_filters_enabled =
      GpuProcessHost::gpu_enabled() &&
      command_line.HasSwitch(switches::kEnableAcceleratedFilters);
  prefs.accelerated_compositing_for_3d_transforms_enabled =
      prefs.accelerated_compositing_for_animation_enabled =
          !command_line.HasSwitch(switches::kDisableAcceleratedLayers);
  prefs.accelerated_compositing_for_plugins_enabled =
      !command_line.HasSwitch(switches::kDisableAcceleratedPlugins);
  prefs.accelerated_compositing_for_video_enabled =
      !command_line.HasSwitch(switches::kDisableAcceleratedVideo);
  prefs.fullscreen_enabled =
      !command_line.HasSwitch(switches::kDisableFullScreen);
  prefs.css_sticky_position_enabled =
      command_line.HasSwitch(switches::kEnableExperimentalWebKitFeatures);
  prefs.css_shaders_enabled =
      command_line.HasSwitch(switches::kEnableCssShaders);
  prefs.css_variables_enabled =
      command_line.HasSwitch(switches::kEnableExperimentalWebKitFeatures);
  prefs.css_grid_layout_enabled =
      command_line.HasSwitch(switches::kEnableExperimentalWebKitFeatures);
  prefs.threaded_html_parser =
      !command_line.HasSwitch(switches::kDisableThreadedHTMLParser);
#if defined(OS_ANDROID)
  prefs.user_gesture_required_for_media_playback = !command_line.HasSwitch(
      switches::kDisableGestureRequirementForMediaPlayback);
#endif

  bool touch_device_present = false;
  touch_device_present = ui::IsTouchDevicePresent();
  const std::string touch_enabled_switch =
      command_line.HasSwitch(switches::kTouchEvents) ?
      command_line.GetSwitchValueASCII(switches::kTouchEvents) :
      switches::kTouchEventsAuto;

  if (touch_enabled_switch.empty() ||
      touch_enabled_switch == switches::kTouchEventsEnabled) {
    prefs.touch_enabled = true;
  } else if (touch_enabled_switch == switches::kTouchEventsAuto) {
    prefs.touch_enabled = touch_device_present;
  } else if (touch_enabled_switch != switches::kTouchEventsDisabled) {
    LOG(ERROR) << "Invalid --touch-events option: " << touch_enabled_switch;
  }

  prefs.device_supports_touch = prefs.touch_enabled && touch_device_present;
#if defined(OS_ANDROID)
  prefs.device_supports_mouse = false;
#endif

   prefs.touch_adjustment_enabled =
       !command_line.HasSwitch(switches::kDisableTouchAdjustment);

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  bool default_enable_scroll_animator = true;
#else
  bool default_enable_scroll_animator = false;
#endif
  prefs.enable_scroll_animator = default_enable_scroll_animator;
  if (command_line.HasSwitch(switches::kEnableSmoothScrolling))
    prefs.enable_scroll_animator = true;
  if (command_line.HasSwitch(switches::kDisableSmoothScrolling))
    prefs.enable_scroll_animator = false;

  prefs.visual_word_movement_enabled =
      command_line.HasSwitch(switches::kEnableVisualWordMovement);

  {  // Certain GPU features might have been blacklisted.
    GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
    DCHECK(gpu_data_manager);
    uint32 blacklist_type = gpu_data_manager->GetBlacklistedFeatures();
    if (blacklist_type & GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING)
      prefs.accelerated_compositing_enabled = false;
    if (blacklist_type & GPU_FEATURE_TYPE_WEBGL)
      prefs.experimental_webgl_enabled = false;
    if (blacklist_type & GPU_FEATURE_TYPE_FLASH3D)
      prefs.flash_3d_enabled = false;
    if (blacklist_type & GPU_FEATURE_TYPE_FLASH_STAGE3D) {
      prefs.flash_stage3d_enabled = false;
      prefs.flash_stage3d_baseline_enabled = false;
    }
    if (blacklist_type & GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE)
      prefs.flash_stage3d_baseline_enabled = false;
    if (blacklist_type & GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS)
      prefs.accelerated_2d_canvas_enabled = false;
    if (blacklist_type & GPU_FEATURE_TYPE_MULTISAMPLING)
      prefs.gl_multisampling_enabled = false;
    if (blacklist_type & GPU_FEATURE_TYPE_3D_CSS) {
      prefs.accelerated_compositing_for_3d_transforms_enabled = false;
      prefs.accelerated_compositing_for_animation_enabled = false;
    }
    if (blacklist_type & GPU_FEATURE_TYPE_ACCELERATED_VIDEO)
      prefs.accelerated_compositing_for_video_enabled = false;

    // Accelerated video and animation are slower than regular when using a
    // software 3d rasterizer. 3D CSS may also be too slow to be worthwhile.
    if (gpu_data_manager->ShouldUseSoftwareRendering()) {
      prefs.accelerated_compositing_for_video_enabled = false;
      prefs.accelerated_compositing_for_animation_enabled = false;
      prefs.accelerated_compositing_for_3d_transforms_enabled = false;
      prefs.accelerated_compositing_for_plugins_enabled = false;
    }
  }

  if (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          rvh->GetProcess()->GetID())) {
    prefs.loads_images_automatically = true;
    prefs.javascript_enabled = true;
  }

  prefs.is_online = !net::NetworkChangeNotifier::IsOffline();

  // Force accelerated compositing and 2d canvas off for chrome: and about:
  // pages (unless it's specifically allowed).
  if ((url.SchemeIs(chrome::kChromeUIScheme) ||
      (url.SchemeIs(chrome::kAboutScheme) &&
       url.spec() != chrome::kAboutBlankURL)) &&
      !command_line.HasSwitch(switches::kAllowWebUICompositing)) {
    prefs.accelerated_compositing_enabled = false;
    prefs.accelerated_2d_canvas_enabled = false;
  }

  prefs.apply_default_device_scale_factor_in_compositor = true;
  prefs.apply_page_scale_factor_in_compositor = true;

  prefs.fixed_position_creates_stacking_context = !command_line.HasSwitch(
      switches::kDisableFixedPositionCreatesStackingContext);

#if defined(OS_CHROMEOS)
  prefs.gesture_tap_highlight_enabled = !command_line.HasSwitch(
      switches::kDisableGestureTapHighlight);
#else
  prefs.gesture_tap_highlight_enabled = command_line.HasSwitch(
      switches::kEnableGestureTapHighlight);
#endif

  prefs.number_of_cpu_cores = base::SysInfo::NumberOfProcessors();

  prefs.viewport_enabled = command_line.HasSwitch(switches::kEnableViewport);

  prefs.deferred_image_decoding_enabled =
      command_line.HasSwitch(switches::kEnableDeferredImageDecoding) ||
      cc::switches::IsImplSidePaintingEnabled();

  GetContentClient()->browser()->OverrideWebkitPrefs(rvh, url, &prefs);

  // Disable compositing in guests until we have compositing path implemented
  // for guests.
  bool guest_compositing_enabled = !command_line.HasSwitch(
      switches::kDisableBrowserPluginCompositing);
  if (rvh->GetProcess()->IsGuest() && !guest_compositing_enabled) {
    prefs.force_compositing_mode = false;
    prefs.accelerated_compositing_enabled = false;
  }

  return prefs;
}

RenderViewHostManager* WebContentsImpl::GetRenderManagerForTesting() {
  return &render_manager_;
}

bool WebContentsImpl::OnMessageReceived(RenderViewHost* render_view_host,
                                        const IPC::Message& message) {
  if (GetWebUI() &&
      static_cast<WebUIImpl*>(GetWebUI())->OnMessageReceived(message)) {
    return true;
  }

  ObserverListBase<WebContentsObserver>::Iterator it(observers_);
  WebContentsObserver* observer;
  while ((observer = it.GetNext()) != NULL)
    if (observer->OnMessageReceived(message))
      return true;

  // Message handlers should be aware of which RenderViewHost sent the
  // message, which is temporarily stored in message_source_.
  message_source_ = render_view_host;
  bool handled = true;
  bool message_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WebContentsImpl, message, message_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidLoadResourceFromMemoryCache,
                        OnDidLoadResourceFromMemoryCache)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidDisplayInsecureContent,
                        OnDidDisplayInsecureContent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidRunInsecureContent,
                        OnDidRunInsecureContent)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentLoadedInFrame,
                        OnDocumentLoadedInFrame)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidFinishLoad, OnDidFinishLoad)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidFailLoadWithError,
                        OnDidFailLoadWithError)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateContentRestrictions,
                        OnUpdateContentRestrictions)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GoToEntryAtOffset, OnGoToEntryAtOffset)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateZoomLimits, OnUpdateZoomLimits)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SaveURLAs, OnSaveURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_EnumerateDirectory, OnEnumerateDirectory)
    IPC_MESSAGE_HANDLER(ViewHostMsg_JSOutOfMemory, OnJSOutOfMemory)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RegisterProtocolHandler,
                        OnRegisterProtocolHandler)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Find_Reply, OnFindReply)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CrashedPlugin, OnCrashedPlugin)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AppCacheAccessed, OnAppCacheAccessed)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenColorChooser, OnOpenColorChooser)
    IPC_MESSAGE_HANDLER(ViewHostMsg_EndColorChooser, OnEndColorChooser)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetSelectedColorInColorChooser,
                        OnSetSelectedColorInColorChooser)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PepperPluginHung, OnPepperPluginHung)
    IPC_MESSAGE_HANDLER(ViewHostMsg_WebUISend, OnWebUISend)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestPpapiBrokerPermission,
                        OnRequestPpapiBrokerPermission)
    IPC_MESSAGE_HANDLER_GENERIC(BrowserPluginHostMsg_AllocateInstanceID,
                                OnBrowserPluginMessage(message))
    IPC_MESSAGE_HANDLER_GENERIC(BrowserPluginHostMsg_Attach,
                                OnBrowserPluginMessage(message))
    IPC_MESSAGE_HANDLER(IconHostMsg_DidDownloadFavicon, OnDidDownloadFavicon)
    IPC_MESSAGE_HANDLER(IconHostMsg_UpdateFaviconURL, OnUpdateFaviconURL)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FindMatchRects_Reply,
                        OnFindMatchRectsReply)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenDateTimeDialog,
                        OnOpenDateTimeDialog)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_FrameDetached, OnFrameDetached)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  message_source_ = NULL;

  if (!message_is_ok) {
    RecordAction(UserMetricsAction("BadMessageTerminate_RVD"));
    GetRenderProcessHost()->ReceivedBadMessage();
  }

  return handled;
}

void WebContentsImpl::RunFileChooser(
    RenderViewHost* render_view_host,
    const FileChooserParams& params) {
  if (delegate_)
    delegate_->RunFileChooser(this, params);
}

NavigationControllerImpl& WebContentsImpl::GetController() {
  return controller_;
}

const NavigationControllerImpl& WebContentsImpl::GetController() const {
  return controller_;
}

BrowserContext* WebContentsImpl::GetBrowserContext() const {
  return controller_.GetBrowserContext();
}

const GURL& WebContentsImpl::GetURL() const {
  // We may not have a navigation entry yet
  NavigationEntry* entry = controller_.GetActiveEntry();
  return entry ? entry->GetVirtualURL() : GURL::EmptyGURL();
}

WebContentsDelegate* WebContentsImpl::GetDelegate() {
  return delegate_;
}

void WebContentsImpl::SetDelegate(WebContentsDelegate* delegate) {
  // TODO(cbentzel): remove this debugging code?
  if (delegate == delegate_)
    return;
  if (delegate_)
    delegate_->Detach(this);
  delegate_ = delegate;
  if (delegate_) {
    delegate_->Attach(this);
    // Ensure the visible RVH reflects the new delegate's preferences.
    if (view_)
      view_->SetOverscrollControllerEnabled(delegate->CanOverscrollContent());
  }
}

RenderProcessHost* WebContentsImpl::GetRenderProcessHost() const {
  RenderViewHostImpl* host = render_manager_.current_host();
  return host ? host->GetProcess() : NULL;
}

RenderViewHost* WebContentsImpl::GetRenderViewHost() const {
  return render_manager_.current_host();
}

void WebContentsImpl::GetRenderViewHostAtPosition(
    int x,
    int y,
    const base::Callback<void(RenderViewHost*, int, int)>& callback) {
  BrowserPluginEmbedder* embedder = GetBrowserPluginEmbedder();
  if (embedder)
    embedder->GetRenderViewHostAtPosition(x, y, callback);
  else
    callback.Run(GetRenderViewHost(), x, y);
}

WebContents* WebContentsImpl::GetEmbedderWebContents() const {
  BrowserPluginGuest* guest = GetBrowserPluginGuest();
  if (guest)
    return guest->embedder_web_contents();
  return NULL;
}

int WebContentsImpl::GetEmbeddedInstanceID() const {
  BrowserPluginGuest* guest = GetBrowserPluginGuest();
  if (guest)
    return guest->instance_id();
  return 0;
}

int WebContentsImpl::GetRoutingID() const {
  if (!GetRenderViewHost())
    return MSG_ROUTING_NONE;

  return GetRenderViewHost()->GetRoutingID();
}

int WebContentsImpl::GetFullscreenWidgetRoutingID() const {
  return fullscreen_widget_routing_id_;
}

RenderWidgetHostView* WebContentsImpl::GetRenderWidgetHostView() const {
  return render_manager_.GetRenderWidgetHostView();
}

RenderWidgetHostViewPort* WebContentsImpl::GetRenderWidgetHostViewPort() const {
  BrowserPluginGuest* guest = GetBrowserPluginGuest();
  if (guest && guest->embedder_web_contents()) {
    return guest->embedder_web_contents()->GetRenderWidgetHostViewPort();
  }
  return RenderWidgetHostViewPort::FromRWHV(GetRenderWidgetHostView());
}

WebContentsView* WebContentsImpl::GetView() const {
  return view_.get();
}

WebUI* WebContentsImpl::CreateWebUI(const GURL& url) {
  WebUIImpl* web_ui = new WebUIImpl(this);
  WebUIController* controller = WebUIControllerFactoryRegistry::GetInstance()->
      CreateWebUIControllerForURL(web_ui, url);
  if (controller) {
    web_ui->AddMessageHandler(new GenericHandler());
    web_ui->SetController(controller);
    return web_ui;
  }

  delete web_ui;
  return NULL;
}

WebUI* WebContentsImpl::GetWebUI() const {
  return render_manager_.web_ui() ? render_manager_.web_ui()
      : render_manager_.pending_web_ui();
}

WebUI* WebContentsImpl::GetCommittedWebUI() const {
  return render_manager_.web_ui();
}

void WebContentsImpl::SetUserAgentOverride(const std::string& override) {
  if (GetUserAgentOverride() == override)
    return;

  renderer_preferences_.user_agent_override = override;

  // Send the new override string to the renderer.
  RenderViewHost* host = GetRenderViewHost();
  if (host)
    host->SyncRendererPrefs();

  // Reload the page if a load is currently in progress to avoid having
  // different parts of the page loaded using different user agents.
  NavigationEntry* entry = controller_.GetActiveEntry();
  if (is_loading_ && entry != NULL && entry->GetIsOverridingUserAgent())
    controller_.ReloadIgnoringCache(true);

  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    UserAgentOverrideSet(override));
}

const std::string& WebContentsImpl::GetUserAgentOverride() const {
  return renderer_preferences_.user_agent_override;
}

const string16& WebContentsImpl::GetTitle() const {
  // Transient entries take precedence. They are used for interstitial pages
  // that are shown on top of existing pages.
  NavigationEntry* entry = controller_.GetTransientEntry();
  std::string accept_languages =
      GetContentClient()->browser()->GetAcceptLangs(
          GetBrowserContext());
  if (entry) {
    return entry->GetTitleForDisplay(accept_languages);
  }
  WebUI* our_web_ui = render_manager_.pending_web_ui() ?
      render_manager_.pending_web_ui() : render_manager_.web_ui();
  if (our_web_ui) {
    // Don't override the title in view source mode.
    entry = controller_.GetActiveEntry();
    if (!(entry && entry->IsViewSourceMode())) {
      // Give the Web UI the chance to override our title.
      const string16& title = our_web_ui->GetOverriddenTitle();
      if (!title.empty())
        return title;
    }
  }

  // We use the title for the last committed entry rather than a pending
  // navigation entry. For example, when the user types in a URL, we want to
  // keep the old page's title until the new load has committed and we get a new
  // title.
  entry = controller_.GetLastCommittedEntry();
  if (entry) {
    return entry->GetTitleForDisplay(accept_languages);
  }

  // |page_title_when_no_navigation_entry_| is finally used
  // if no title cannot be retrieved.
  return page_title_when_no_navigation_entry_;
}

int32 WebContentsImpl::GetMaxPageID() {
  return GetMaxPageIDForSiteInstance(GetSiteInstance());
}

int32 WebContentsImpl::GetMaxPageIDForSiteInstance(
    SiteInstance* site_instance) {
  if (max_page_ids_.find(site_instance->GetId()) == max_page_ids_.end())
    max_page_ids_[site_instance->GetId()] = -1;

  return max_page_ids_[site_instance->GetId()];
}

void WebContentsImpl::UpdateMaxPageID(int32 page_id) {
  UpdateMaxPageIDForSiteInstance(GetSiteInstance(), page_id);
}

void WebContentsImpl::UpdateMaxPageIDForSiteInstance(
    SiteInstance* site_instance, int32 page_id) {
  if (GetMaxPageIDForSiteInstance(site_instance) < page_id)
    max_page_ids_[site_instance->GetId()] = page_id;
}

void WebContentsImpl::CopyMaxPageIDsFrom(WebContentsImpl* web_contents) {
  max_page_ids_ = web_contents->max_page_ids_;
}

SiteInstance* WebContentsImpl::GetSiteInstance() const {
  return render_manager_.current_host()->GetSiteInstance();
}

SiteInstance* WebContentsImpl::GetPendingSiteInstance() const {
  RenderViewHost* dest_rvh = render_manager_.pending_render_view_host() ?
      render_manager_.pending_render_view_host() :
      render_manager_.current_host();
  return dest_rvh->GetSiteInstance();
}

bool WebContentsImpl::IsLoading() const {
  return is_loading_;
}

bool WebContentsImpl::IsWaitingForResponse() const {
  return waiting_for_response_;
}

const net::LoadStateWithParam& WebContentsImpl::GetLoadState() const {
  return load_state_;
}

const string16& WebContentsImpl::GetLoadStateHost() const {
  return load_state_host_;
}

uint64 WebContentsImpl::GetUploadSize() const {
  return upload_size_;
}

uint64 WebContentsImpl::GetUploadPosition() const {
  return upload_position_;
}

const std::string& WebContentsImpl::GetEncoding() const {
  return encoding_;
}

bool WebContentsImpl::DisplayedInsecureContent() const {
  return displayed_insecure_content_;
}

void WebContentsImpl::IncrementCapturerCount() {
  DCHECK(!is_being_destroyed_);
  ++capturer_count_;
  DVLOG(1) << "There are now " << capturer_count_
           << " capturing(s) of WebContentsImpl@" << this;
}

void WebContentsImpl::DecrementCapturerCount() {
  --capturer_count_;
  DVLOG(1) << "There are now " << capturer_count_
           << " capturing(s) of WebContentsImpl@" << this;
  DCHECK_LE(0, capturer_count_);

  if (is_being_destroyed_)
    return;

  // While capturer_count_ was greater than zero, the WasHidden() calls to RWHV
  // were being prevented.  If there are no more capturers, make the call now.
  if (capturer_count_ == 0 && !should_normally_be_visible_) {
    DVLOG(1) << "Executing delayed WasHidden().";
    WasHidden();
  }
}

bool WebContentsImpl::IsCrashed() const {
  return (crashed_status_ == base::TERMINATION_STATUS_PROCESS_CRASHED ||
          crashed_status_ == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
          crashed_status_ == base::TERMINATION_STATUS_PROCESS_WAS_KILLED);
}

void WebContentsImpl::SetIsCrashed(base::TerminationStatus status,
                                   int error_code) {
  if (status == crashed_status_)
    return;

  crashed_status_ = status;
  crashed_error_code_ = error_code;
  NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
}

base::TerminationStatus WebContentsImpl::GetCrashedStatus() const {
  return crashed_status_;
}

bool WebContentsImpl::IsBeingDestroyed() const {
  return is_being_destroyed_;
}

void WebContentsImpl::NotifyNavigationStateChanged(unsigned changed_flags) {
  if (delegate_)
    delegate_->NavigationStateChanged(this, changed_flags);
}

base::TimeTicks WebContentsImpl::GetLastSelectedTime() const {
  return last_selected_time_;
}

void WebContentsImpl::WasShown() {
  controller_.SetActive(true);
  RenderWidgetHostViewPort* rwhv =
      RenderWidgetHostViewPort::FromRWHV(GetRenderWidgetHostView());
  if (rwhv) {
    rwhv->WasShown();
#if defined(OS_MACOSX)
    rwhv->SetActive(true);
#endif
  }

  last_selected_time_ = base::TimeTicks::Now();

  FOR_EACH_OBSERVER(WebContentsObserver, observers_, WasShown());

  // The resize rect might have changed while this was inactive -- send the new
  // one to make sure it's up to date.
  RenderViewHostImpl* rvh =
      static_cast<RenderViewHostImpl*>(GetRenderViewHost());
  if (rvh) {
    rvh->ResizeRectChanged(GetRootWindowResizerRect());
  }

  should_normally_be_visible_ = true;
  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
      Source<WebContents>(this),
      Details<const bool>(&should_normally_be_visible_));
}

void WebContentsImpl::WasHidden() {
  // If there are entities capturing screenshots or video (e.g., mirroring),
  // don't activate the "disable rendering" optimization.
  if (capturer_count_ == 0) {
    // |GetRenderViewHost()| can be NULL if the user middle clicks a link to
    // open a tab in the background, then closes the tab before selecting it.
    // This is because closing the tab calls WebContentsImpl::Destroy(), which
    // removes the |GetRenderViewHost()|; then when we actually destroy the
    // window, OnWindowPosChanged() notices and calls WasHidden() (which
    // calls us).
    RenderWidgetHostViewPort* rwhv =
        RenderWidgetHostViewPort::FromRWHV(GetRenderWidgetHostView());
    if (rwhv)
      rwhv->WasHidden();
  }

  should_normally_be_visible_ = false;
  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_VISIBILITY_CHANGED,
      Source<WebContents>(this),
      Details<const bool>(&should_normally_be_visible_));
}

bool WebContentsImpl::NeedToFireBeforeUnload() {
  // TODO(creis): Should we fire even for interstitial pages?
  return WillNotifyDisconnection() &&
      !ShowingInterstitialPage() &&
      !static_cast<RenderViewHostImpl*>(
          GetRenderViewHost())->SuddenTerminationAllowed();
}

void WebContentsImpl::Stop() {
  render_manager_.Stop();
  FOR_EACH_OBSERVER(WebContentsObserver, observers_, StopNavigation());
}

WebContents* WebContentsImpl::Clone() {
  // We use our current SiteInstance since the cloned entry will use it anyway.
  // We pass our own opener so that the cloned page can access it if it was
  // before.
  CreateParams create_params(GetBrowserContext(), GetSiteInstance());
  create_params.initial_size = view_->GetContainerSize();
  WebContentsImpl* tc = CreateWithOpener(create_params, opener_);
  tc->GetController().CopyStateFrom(controller_);
  FOR_EACH_OBSERVER(WebContentsObserver,
                    observers_,
                    DidCloneToNewWebContents(this, tc));
  return tc;
}

void WebContentsImpl::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_WEB_CONTENTS_DESTROYED:
      OnWebContentsDestroyed(Source<WebContents>(source).ptr());
      break;
    case NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED: {
      RenderWidgetHost* host = Source<RenderWidgetHost>(source).ptr();
      for (PendingWidgetViews::iterator i = pending_widget_views_.begin();
           i != pending_widget_views_.end(); ++i) {
        if (host->GetView() == i->second) {
          pending_widget_views_.erase(i);
          break;
        }
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void WebContentsImpl::Init(const WebContents::CreateParams& params) {
  render_manager_.Init(
      params.browser_context, params.site_instance, params.routing_id);

  view_.reset(GetContentClient()->browser()->
      OverrideCreateWebContentsView(this, &render_view_host_delegate_view_));
  if (view_.get()) {
    CHECK(render_view_host_delegate_view_);
  } else {
    WebContentsViewDelegate* delegate =
        GetContentClient()->browser()->GetWebContentsViewDelegate(this);

    if (browser_plugin_guest_.get()) {
      WebContentsViewPort* platform_view = CreateWebContentsView(
          this, delegate, &render_view_host_delegate_view_);

      WebContentsViewGuest* rv = new WebContentsViewGuest(
          this, browser_plugin_guest_.get(), platform_view);
      render_view_host_delegate_view_ = rv;
      view_.reset(rv);
    } else {
      // Regular WebContentsView.
      view_.reset(CreateWebContentsView(
          this, delegate, &render_view_host_delegate_view_));
    }
    CHECK(render_view_host_delegate_view_);
  }
  CHECK(view_.get());

  gfx::Size initial_size = params.initial_size;
  view_->CreateView(initial_size, params.context);

  // Listen for whether our opener gets destroyed.
  if (opener_) {
    registrar_.Add(this, NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   Source<WebContents>(opener_));
  }

  registrar_.Add(this,
                 NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
                 NotificationService::AllBrowserContextsAndSources());
#if defined(ENABLE_JAVA_BRIDGE)
  java_bridge_dispatcher_host_manager_.reset(
      new JavaBridgeDispatcherHostManager(this));
#endif

#if defined(OS_ANDROID)
  date_time_chooser_.reset(new DateTimeChooserAndroid());
#endif
}

void WebContentsImpl::OnWebContentsDestroyed(WebContents* web_contents) {
  // Clear the opener if it has been closed.
  if (web_contents == opener_) {
    registrar_.Remove(this, NOTIFICATION_WEB_CONTENTS_DESTROYED,
                      Source<WebContents>(opener_));
    opener_ = NULL;
    return;
  }
  // Clear a pending contents that has been closed before being shown.
  for (PendingContents::iterator iter = pending_contents_.begin();
       iter != pending_contents_.end();
       ++iter) {
    if (iter->second != web_contents)
      continue;
    pending_contents_.erase(iter);
    registrar_.Remove(this, NOTIFICATION_WEB_CONTENTS_DESTROYED,
                      Source<WebContents>(web_contents));
    return;
  }
  NOTREACHED();
}

void WebContentsImpl::AddObserver(WebContentsObserver* observer) {
  observers_.AddObserver(observer);
}

void WebContentsImpl::RemoveObserver(WebContentsObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebContentsImpl::Activate() {
  if (delegate_)
    delegate_->ActivateContents(this);
}

void WebContentsImpl::Deactivate() {
  if (delegate_)
    delegate_->DeactivateContents(this);
}

void WebContentsImpl::LostCapture() {
  if (delegate_)
    delegate_->LostCapture();
}

void WebContentsImpl::RenderWidgetDeleted(
    RenderWidgetHostImpl* render_widget_host) {
  if (is_being_destroyed_) {
    // |created_widgets_| might have been destroyed.
    return;
  }

  std::set<RenderWidgetHostImpl*>::iterator iter =
      created_widgets_.find(render_widget_host);
  if (iter != created_widgets_.end())
    created_widgets_.erase(iter);

  if (render_widget_host &&
      render_widget_host->GetRoutingID() == fullscreen_widget_routing_id_) {
    FOR_EACH_OBSERVER(WebContentsObserver,
                      observers_,
                      DidDestroyFullscreenWidget(
                          fullscreen_widget_routing_id_));
    fullscreen_widget_routing_id_ = MSG_ROUTING_NONE;
  }
}

bool WebContentsImpl::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return delegate_ &&
      delegate_->PreHandleKeyboardEvent(this, event, is_keyboard_shortcut);
}

void WebContentsImpl::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  if (delegate_)
    delegate_->HandleKeyboardEvent(this, event);
}

bool WebContentsImpl::PreHandleWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  if (delegate_ &&
      event.wheelTicksY &&
      (event.modifiers & WebKit::WebInputEvent::ControlKey)) {
    delegate_->ContentsZoomChange(event.wheelTicksY > 0);
    return true;
  }

  return false;
}

void WebContentsImpl::HandleMouseDown() {
  if (delegate_)
    delegate_->HandleMouseDown();
}

void WebContentsImpl::HandleMouseUp() {
  if (delegate_)
    delegate_->HandleMouseUp();
}

void WebContentsImpl::HandlePointerActivate() {
  if (delegate_)
    delegate_->HandlePointerActivate();
}

void WebContentsImpl::HandleGestureBegin() {
  if (delegate_)
    delegate_->HandleGestureBegin();
}

void WebContentsImpl::HandleGestureEnd() {
  if (delegate_)
    delegate_->HandleGestureEnd();
}

void WebContentsImpl::ToggleFullscreenMode(bool enter_fullscreen) {
  if (delegate_)
    delegate_->ToggleFullscreenModeForTab(this, enter_fullscreen);
}

bool WebContentsImpl::IsFullscreenForCurrentTab() const {
  return delegate_ ? delegate_->IsFullscreenForTabOrPending(this) : false;
}

void WebContentsImpl::RequestToLockMouse(bool user_gesture,
                                         bool last_unlocked_by_target) {
  if (delegate_) {
    delegate_->RequestToLockMouse(this, user_gesture, last_unlocked_by_target);
  } else {
    GotResponseToLockMouseRequest(false);
  }
}

void WebContentsImpl::LostMouseLock() {
  if (delegate_)
    delegate_->LostMouseLock();
}

void WebContentsImpl::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params,
    SessionStorageNamespace* session_storage_namespace) {
  if (delegate_ && !delegate_->ShouldCreateWebContents(
          this, route_id, params.window_container_type, params.frame_name,
          params.target_url)) {
    GetRenderViewHost()->GetProcess()->ResumeRequestsForView(route_id);
    return;
  }

  // We usually create the new window in the same BrowsingInstance (group of
  // script-related windows), by passing in the current SiteInstance.  However,
  // if the opener is being suppressed (in a non-guest), we create a new
  // SiteInstance in its own BrowsingInstance.
  bool is_guest = GetRenderProcessHost()->IsGuest();

  scoped_refptr<SiteInstance> site_instance =
      params.opener_suppressed && !is_guest ?
      SiteInstance::CreateForURL(GetBrowserContext(), params.target_url) :
      GetSiteInstance();

  // Create the new web contents. This will automatically create the new
  // WebContentsView. In the future, we may want to create the view separately.
  WebContentsImpl* new_contents =
      new WebContentsImpl(GetBrowserContext(),
                          params.opener_suppressed ? NULL : this);

  // We must assign the SessionStorageNamespace before calling Init().
  //
  // http://crbug.com/142685
  const std::string& partition_id =
      GetContentClient()->browser()->
          GetStoragePartitionIdForSite(GetBrowserContext(),
                                       site_instance->GetSiteURL());
  StoragePartition* partition =
      BrowserContext::GetStoragePartition(GetBrowserContext(),
                                          site_instance);
  DOMStorageContextImpl* dom_storage_context =
      static_cast<DOMStorageContextImpl*>(partition->GetDOMStorageContext());
  SessionStorageNamespaceImpl* session_storage_namespace_impl =
      static_cast<SessionStorageNamespaceImpl*>(session_storage_namespace);
  CHECK(session_storage_namespace_impl->IsFromContext(dom_storage_context));
  new_contents->GetController().SetSessionStorageNamespace(
      partition_id,
      session_storage_namespace);
  CreateParams create_params(GetBrowserContext(), site_instance);
  create_params.routing_id = route_id;
  if (!is_guest) {
    create_params.context = view_->GetNativeView();
    create_params.initial_size = view_->GetContainerSize();
  } else {
    // This makes |new_contents| act as a guest.
    // For more info, see comment above class BrowserPluginGuest.
    int instance_id = GetBrowserPluginGuestManager()->get_next_instance_id();
    WebContentsImpl* new_contents_impl =
        static_cast<WebContentsImpl*>(new_contents);
    new_contents_impl->browser_plugin_guest_.reset(
        BrowserPluginGuest::Create(instance_id, new_contents_impl));
  }
  new_contents->Init(create_params);

  // Save the window for later if we're not suppressing the opener (since it
  // will be shown immediately) and if it's not a guest (since we separately
  // track when to show guests).
  if (!params.opener_suppressed && !is_guest) {
    WebContentsViewPort* new_view = new_contents->view_.get();

    // TODO(brettw): It seems bogus that we have to call this function on the
    // newly created object and give it one of its own member variables.
    new_view->CreateViewForWidget(new_contents->GetRenderViewHost());

    // Save the created window associated with the route so we can show it
    // later.
    DCHECK_NE(MSG_ROUTING_NONE, route_id);
    pending_contents_[route_id] = new_contents;
    registrar_.Add(this, NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   Source<WebContents>(new_contents));
  }

  if (delegate_) {
    delegate_->WebContentsCreated(
        this, params.opener_frame_id, params.frame_name,
        params.target_url, new_contents);
  }

  if (params.opener_suppressed) {
    // When the opener is suppressed, the original renderer cannot access the
    // new window.  As a result, we need to show and navigate the window here.
    bool was_blocked = false;
    if (delegate_) {
      gfx::Rect initial_pos;
      delegate_->AddNewContents(
          this, new_contents, params.disposition, initial_pos,
          params.user_gesture, &was_blocked);
    }
    if (!was_blocked) {
      OpenURLParams open_params(params.target_url,
                                Referrer(),
                                CURRENT_TAB,
                                PAGE_TRANSITION_LINK,
                                true /* is_renderer_initiated */);
      new_contents->OpenURL(open_params);
    }
  }
}

void WebContentsImpl::CreateNewWidget(int route_id,
                                      WebKit::WebPopupType popup_type) {
  CreateNewWidget(route_id, false, popup_type);
}

void WebContentsImpl::CreateNewFullscreenWidget(int route_id) {
  CreateNewWidget(route_id, true, WebKit::WebPopupTypeNone);
}

void WebContentsImpl::CreateNewWidget(int route_id,
                                      bool is_fullscreen,
                                      WebKit::WebPopupType popup_type) {
  RenderProcessHost* process = GetRenderProcessHost();
  RenderWidgetHostImpl* widget_host =
      new RenderWidgetHostImpl(this, process, route_id);
  created_widgets_.insert(widget_host);

  RenderWidgetHostViewPort* widget_view = RenderWidgetHostViewPort::FromRWHV(
      view_->CreateViewForPopupWidget(widget_host));
  if (!widget_view)
    return;
  if (!is_fullscreen) {
    // Popups should not get activated.
    widget_view->SetPopupType(popup_type);
  }
  // Save the created widget associated with the route so we can show it later.
  pending_widget_views_[route_id] = widget_view;

#if defined(OS_MACOSX)
  // A RenderWidgetHostViewMac has lifetime scoped to the view. We'll retain it
  // to allow it to survive the trip without being hosted.
  base::mac::NSObjectRetain(widget_view->GetNativeView());
#endif
}

void WebContentsImpl::ShowCreatedWindow(int route_id,
                                        WindowOpenDisposition disposition,
                                        const gfx::Rect& initial_pos,
                                        bool user_gesture) {
  WebContentsImpl* contents = GetCreatedWindow(route_id);
  if (contents) {
    WebContentsDelegate* delegate = GetDelegate();
    if (delegate) {
      delegate->AddNewContents(
          this, contents, disposition, initial_pos, user_gesture, NULL);
    }
  }
}

void WebContentsImpl::ShowCreatedWidget(int route_id,
                                        const gfx::Rect& initial_pos) {
  ShowCreatedWidget(route_id, false, initial_pos);
}

void WebContentsImpl::ShowCreatedFullscreenWidget(int route_id) {
  ShowCreatedWidget(route_id, true, gfx::Rect());

  DCHECK_EQ(MSG_ROUTING_NONE, fullscreen_widget_routing_id_);
  fullscreen_widget_routing_id_ = route_id;
  FOR_EACH_OBSERVER(WebContentsObserver,
                    observers_,
                    DidShowFullscreenWidget(route_id));
}

void WebContentsImpl::ShowCreatedWidget(int route_id,
                                        bool is_fullscreen,
                                        const gfx::Rect& initial_pos) {
  if (delegate_)
    delegate_->RenderWidgetShowing();

  RenderWidgetHostViewPort* widget_host_view =
      RenderWidgetHostViewPort::FromRWHV(GetCreatedWidget(route_id));
  if (!widget_host_view)
    return;
  if (is_fullscreen)
    widget_host_view->InitAsFullscreen(GetRenderWidgetHostViewPort());
  else
    widget_host_view->InitAsPopup(GetRenderWidgetHostViewPort(), initial_pos);

  RenderWidgetHostImpl* render_widget_host_impl =
      RenderWidgetHostImpl::From(widget_host_view->GetRenderWidgetHost());
  render_widget_host_impl->Init();
  // Only allow privileged mouse lock for fullscreen render widget, which is
  // used to implement Pepper Flash fullscreen.
  render_widget_host_impl->set_allow_privileged_mouse_lock(is_fullscreen);

#if defined(OS_MACOSX)
  // A RenderWidgetHostViewMac has lifetime scoped to the view. Now that it's
  // properly embedded (or purposefully ignored) we can release the retain we
  // took in CreateNewWidget().
  base::mac::NSObjectRelease(widget_host_view->GetNativeView());
#endif
}

WebContentsImpl* WebContentsImpl::GetCreatedWindow(int route_id) {
  PendingContents::iterator iter = pending_contents_.find(route_id);

  // Certain systems can block the creation of new windows. If we didn't succeed
  // in creating one, just return NULL.
  if (iter == pending_contents_.end()) {
    return NULL;
  }

  WebContentsImpl* new_contents = iter->second;
  pending_contents_.erase(route_id);
  registrar_.Remove(this, NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    Source<WebContents>(new_contents));

  if (!new_contents->GetRenderProcessHost()->HasConnection() ||
      !new_contents->GetRenderViewHost()->GetView())
    return NULL;

  // TODO(brettw): It seems bogus to reach into here and initialize the host.
  static_cast<RenderViewHostImpl*>(new_contents->GetRenderViewHost())->Init();
  return new_contents;
}

RenderWidgetHostView* WebContentsImpl::GetCreatedWidget(int route_id) {
  PendingWidgetViews::iterator iter = pending_widget_views_.find(route_id);
  if (iter == pending_widget_views_.end()) {
    DCHECK(false);
    return NULL;
  }

  RenderWidgetHostView* widget_host_view = iter->second;
  pending_widget_views_.erase(route_id);

  RenderWidgetHost* widget_host = widget_host_view->GetRenderWidgetHost();
  if (!widget_host->GetProcess()->HasConnection()) {
    // The view has gone away or the renderer crashed. Nothing to do.
    return NULL;
  }

  return widget_host_view;
}

void WebContentsImpl::ShowContextMenu(
    const ContextMenuParams& params,
    ContextMenuSourceType type) {
  // Allow WebContentsDelegates to handle the context menu operation first.
  if (delegate_ && delegate_->HandleContextMenu(params))
    return;

  render_view_host_delegate_view_->ShowContextMenu(params, type);
}

void WebContentsImpl::RequestMediaAccessPermission(
    const MediaStreamRequest& request,
    const MediaResponseCallback& callback) {
  if (delegate_)
    delegate_->RequestMediaAccessPermission(this, request, callback);
  else
    callback.Run(MediaStreamDevices());
}

void WebContentsImpl::UpdatePreferredSize(const gfx::Size& pref_size) {
  preferred_size_ = pref_size;
  if (delegate_)
    delegate_->UpdatePreferredSize(this, pref_size);
}

void WebContentsImpl::ResizeDueToAutoResize(const gfx::Size& new_size) {
  if (delegate_)
    delegate_->ResizeDueToAutoResize(this, new_size);
}

WebContents* WebContentsImpl::OpenURL(const OpenURLParams& params) {
  if (!delegate_)
    return NULL;

  WebContents* new_contents = delegate_->OpenURLFromTab(this, params);
  return new_contents;
}

bool WebContentsImpl::Send(IPC::Message* message) {
  if (!GetRenderViewHost()) {
    delete message;
    return false;
  }

  return GetRenderViewHost()->Send(message);
}

bool WebContentsImpl::NavigateToPendingEntry(
    NavigationController::ReloadType reload_type) {
  return NavigateToEntry(
      *NavigationEntryImpl::FromNavigationEntry(controller_.GetPendingEntry()),
      reload_type);
}

void WebContentsImpl::RenderViewForInterstitialPageCreated(
    RenderViewHost* render_view_host) {
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    RenderViewForInterstitialPageCreated(render_view_host));
}

void WebContentsImpl::AttachInterstitialPage(
    InterstitialPageImpl* interstitial_page) {
  DCHECK(interstitial_page);
  render_manager_.set_interstitial_page(interstitial_page);
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidAttachInterstitialPage());
}

void WebContentsImpl::DetachInterstitialPage() {
  if (GetInterstitialPage())
    render_manager_.remove_interstitial_page();
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidDetachInterstitialPage());
}

bool WebContentsImpl::NavigateToEntry(
    const NavigationEntryImpl& entry,
    NavigationController::ReloadType reload_type) {
  // The renderer will reject IPC messages with URLs longer than
  // this limit, so don't attempt to navigate with a longer URL.
  if (entry.GetURL().spec().size() > kMaxURLChars)
    return false;

  RenderViewHostImpl* dest_render_view_host =
      static_cast<RenderViewHostImpl*>(render_manager_.Navigate(entry));
  if (!dest_render_view_host)
    return false;  // Unable to create the desired render view host.

  // For security, we should never send non-Web-UI URLs to a Web UI renderer.
  // Double check that here.
  int enabled_bindings = dest_render_view_host->GetEnabledBindings();
  bool data_urls_allowed = delegate_ && delegate_->CanLoadDataURLsInWebUI();
  bool is_allowed_in_web_ui_renderer =
      WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
          GetBrowserContext(), entry.GetURL(), data_urls_allowed);
  if ((enabled_bindings & BINDINGS_POLICY_WEB_UI) &&
      !is_allowed_in_web_ui_renderer) {
    // Log the URL to help us diagnose any future failures of this CHECK.
    GetContentClient()->SetActiveURL(entry.GetURL());
    CHECK(0);
  }

  // Notify observers that we will navigate in this RV.
  FOR_EACH_OBSERVER(WebContentsObserver,
                    observers_,
                    AboutToNavigateRenderView(dest_render_view_host));

  // Used for page load time metrics.
  current_load_start_ = base::TimeTicks::Now();

  // Navigate in the desired RenderViewHost.
  ViewMsg_Navigate_Params navigate_params;
  MakeNavigateParams(entry, controller_, delegate_, reload_type,
                     &navigate_params);
  dest_render_view_host->Navigate(navigate_params);

  if (entry.GetPageID() == -1) {
    // HACK!!  This code suppresses javascript: URLs from being added to
    // session history, which is what we want to do for javascript: URLs that
    // do not generate content.  What we really need is a message from the
    // renderer telling us that a new page was not created.  The same message
    // could be used for mailto: URLs and the like.
    if (entry.GetURL().SchemeIs(chrome::kJavaScriptScheme))
      return false;
  }

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(WebContentsObserver,
                    observers_,
                    NavigateToPendingEntry(entry.GetURL(), reload_type));

  if (delegate_)
    delegate_->DidNavigateToPendingEntry(this);

  return true;
}

void WebContentsImpl::SetHistoryLengthAndPrune(
    const SiteInstance* site_instance,
    int history_length,
    int32 minimum_page_id) {
  // SetHistoryLengthAndPrune doesn't work when there are pending cross-site
  // navigations. Callers should ensure that this is the case.
  if (render_manager_.pending_render_view_host()) {
    NOTREACHED();
    return;
  }
  RenderViewHostImpl* rvh = GetRenderViewHostImpl();
  if (!rvh) {
    NOTREACHED();
    return;
  }
  if (site_instance && rvh->GetSiteInstance() != site_instance) {
    NOTREACHED();
    return;
  }
  Send(new ViewMsg_SetHistoryLengthAndPrune(GetRoutingID(),
                                            history_length,
                                            minimum_page_id));
}

void WebContentsImpl::FocusThroughTabTraversal(bool reverse) {
  if (ShowingInterstitialPage()) {
    render_manager_.interstitial_page()->FocusThroughTabTraversal(reverse);
    return;
  }
  GetRenderViewHostImpl()->SetInitialFocus(reverse);
}

bool WebContentsImpl::ShowingInterstitialPage() const {
  return render_manager_.interstitial_page() != NULL;
}

InterstitialPage* WebContentsImpl::GetInterstitialPage() const {
  return render_manager_.interstitial_page();
}

bool WebContentsImpl::IsSavable() {
  // WebKit creates Document object when MIME type is application/xhtml+xml,
  // so we also support this MIME type.
  return contents_mime_type_ == "text/html" ||
         contents_mime_type_ == "text/xml" ||
         contents_mime_type_ == "application/xhtml+xml" ||
         contents_mime_type_ == "text/plain" ||
         contents_mime_type_ == "text/css" ||
         net::IsSupportedJavascriptMimeType(contents_mime_type_.c_str());
}

void WebContentsImpl::OnSavePage() {
  // If we can not save the page, try to download it.
  if (!IsSavable()) {
    RecordDownloadSource(INITIATED_BY_SAVE_PACKAGE_ON_NON_HTML);
    SaveURL(GetURL(), Referrer(), true);
    return;
  }

  Stop();

  // Create the save package and possibly prompt the user for the name to save
  // the page as. The user prompt is an asynchronous operation that runs on
  // another thread.
  save_package_ = new SavePackage(this);
  save_package_->GetSaveInfo();
}

// Used in automated testing to bypass prompting the user for file names.
// Instead, the names and paths are hard coded rather than running them through
// file name sanitation and extension / mime checking.
bool WebContentsImpl::SavePage(const base::FilePath& main_file,
                               const base::FilePath& dir_path,
                               SavePageType save_type) {
  // Stop the page from navigating.
  Stop();

  save_package_ = new SavePackage(this, save_type, main_file, dir_path);
  return save_package_->Init(SavePackageDownloadCreatedCallback());
}

void WebContentsImpl::GenerateMHTML(
    const base::FilePath& file,
    const base::Callback<void(const base::FilePath&, int64)>& callback) {
  MHTMLGenerationManager::GetInstance()->GenerateMHTML(this, file, callback);
}

bool WebContentsImpl::IsActiveEntry(int32 page_id) {
  NavigationEntryImpl* active_entry =
      NavigationEntryImpl::FromNavigationEntry(controller_.GetActiveEntry());
  return (active_entry != NULL &&
          active_entry->site_instance() == GetSiteInstance() &&
          active_entry->GetPageID() == page_id);
}

const std::string& WebContentsImpl::GetContentsMimeType() const {
  return contents_mime_type_;
}

bool WebContentsImpl::WillNotifyDisconnection() const {
  return notify_disconnection_;
}

void WebContentsImpl::SetOverrideEncoding(const std::string& encoding) {
  SetEncoding(encoding);
  Send(new ViewMsg_SetPageEncoding(GetRoutingID(), encoding));
}

void WebContentsImpl::ResetOverrideEncoding() {
  encoding_.clear();
  Send(new ViewMsg_ResetPageEncodingToDefault(GetRoutingID()));
}

RendererPreferences* WebContentsImpl::GetMutableRendererPrefs() {
  return &renderer_preferences_;
}

void WebContentsImpl::SetNewTabStartTime(const base::TimeTicks& time) {
  new_tab_start_time_ = time;
}

base::TimeTicks WebContentsImpl::GetNewTabStartTime() const {
  return new_tab_start_time_;
}

void WebContentsImpl::Close() {
  Close(GetRenderViewHost());
}

void WebContentsImpl::OnCloseStarted() {
  if (close_start_time_.is_null())
    close_start_time_ = base::TimeTicks::Now();
}

bool WebContentsImpl::ShouldAcceptDragAndDrop() const {
#if defined(OS_CHROMEOS)
  // ChromeOS panels (pop-ups) do not take drag-n-drop.
  // See http://crosbug.com/2413
  if (delegate_ && delegate_->IsPopupOrPanel(this))
    return false;
  return true;
#else
  return true;
#endif
}

void WebContentsImpl::SystemDragEnded() {
  if (GetRenderViewHost())
    GetRenderViewHostImpl()->DragSourceSystemDragEnded();
  if (delegate_)
    delegate_->DragEnded();
}

void WebContentsImpl::UserGestureDone() {
  OnUserGesture();
}

void WebContentsImpl::SetClosedByUserGesture(bool value) {
  closed_by_user_gesture_ = value;
}

bool WebContentsImpl::GetClosedByUserGesture() const {
  return closed_by_user_gesture_;
}

double WebContentsImpl::GetZoomLevel() const {
  HostZoomMapImpl* zoom_map = static_cast<HostZoomMapImpl*>(
      HostZoomMap::GetForBrowserContext(GetBrowserContext()));
  if (!zoom_map)
    return 0;

  double zoom_level;
  if (temporary_zoom_settings_) {
    zoom_level = zoom_map->GetTemporaryZoomLevel(
        GetRenderProcessHost()->GetID(), GetRenderViewHost()->GetRoutingID());
  } else {
    GURL url;
    NavigationEntry* active_entry = GetController().GetActiveEntry();
    // Since zoom map is updated using rewritten URL, use rewritten URL
    // to get the zoom level.
    url = active_entry ? active_entry->GetURL() : GURL::EmptyGURL();
    zoom_level = zoom_map->GetZoomLevelForHostAndScheme(url.scheme(),
        net::GetHostOrSpecFromURL(url));
  }
  return zoom_level;
}

int WebContentsImpl::GetZoomPercent(bool* enable_increment,
                                    bool* enable_decrement) const {
  *enable_decrement = *enable_increment = false;
  // Calculate the zoom percent from the factor. Round up to the nearest whole
  // number.
  int percent = static_cast<int>(
      WebKit::WebView::zoomLevelToZoomFactor(GetZoomLevel()) * 100 + 0.5);
  *enable_decrement = percent > minimum_zoom_percent_;
  *enable_increment = percent < maximum_zoom_percent_;
  return percent;
}

void WebContentsImpl::ViewSource() {
  if (!delegate_)
    return;

  NavigationEntry* active_entry = GetController().GetActiveEntry();
  if (!active_entry)
    return;

  delegate_->ViewSourceForTab(this, active_entry->GetURL());
}

void WebContentsImpl::ViewFrameSource(const GURL& url,
                                      const std::string& content_state) {
  if (!delegate_)
    return;

  delegate_->ViewSourceForFrame(this, url, content_state);
}

int WebContentsImpl::GetMinimumZoomPercent() const {
  return minimum_zoom_percent_;
}

int WebContentsImpl::GetMaximumZoomPercent() const {
  return maximum_zoom_percent_;
}

gfx::Size WebContentsImpl::GetPreferredSize() const {
  return preferred_size_;
}

int WebContentsImpl::GetContentRestrictions() const {
  return content_restrictions_;
}

WebUI* WebContentsImpl::GetWebUIForCurrentState() {
  // When there is a pending navigation entry, we want to use the pending WebUI
  // that goes along with it to control the basic flags. For example, we want to
  // show the pending URL in the URL bar, so we want the display_url flag to
  // be from the pending entry.
  //
  // The confusion comes because there are multiple possibilities for the
  // initial load in a tab as a side effect of the way the RenderViewHostManager
  // works.
  //
  //  - For the very first tab the load looks "normal". The new tab Web UI is
  //    the pending one, and we want it to apply here.
  //
  //  - For subsequent new tabs, they'll get a new SiteInstance which will then
  //    get switched to the one previously associated with the new tab pages.
  //    This switching will cause the manager to commit the RVH/WebUI. So we'll
  //    have a committed Web UI in this case.
  //
  // This condition handles all of these cases:
  //
  //  - First load in first tab: no committed nav entry + pending nav entry +
  //    pending dom ui:
  //    -> Use pending Web UI if any.
  //
  //  - First load in second tab: no committed nav entry + pending nav entry +
  //    no pending Web UI:
  //    -> Use the committed Web UI if any.
  //
  //  - Second navigation in any tab: committed nav entry + pending nav entry:
  //    -> Use pending Web UI if any.
  //
  //  - Normal state with no load: committed nav entry + no pending nav entry:
  //    -> Use committed Web UI.
  if (controller_.GetPendingEntry() &&
      (controller_.GetLastCommittedEntry() ||
       render_manager_.pending_web_ui()))
    return render_manager_.pending_web_ui();
  return render_manager_.web_ui();
}

bool WebContentsImpl::GotResponseToLockMouseRequest(bool allowed) {
  return GetRenderViewHost() ?
      GetRenderViewHostImpl()->GotResponseToLockMouseRequest(allowed) : false;
}

bool WebContentsImpl::HasOpener() const {
  return opener_ != NULL;
}

void WebContentsImpl::DidChooseColorInColorChooser(int color_chooser_id,
                                                   SkColor color) {
  Send(new ViewMsg_DidChooseColorResponse(
      GetRoutingID(), color_chooser_id, color));
}

void WebContentsImpl::DidEndColorChooser(int color_chooser_id) {
  Send(new ViewMsg_DidEndColorChooser(GetRoutingID(), color_chooser_id));
  if (delegate_)
    delegate_->DidEndColorChooser();
  color_chooser_ = NULL;
}

int WebContentsImpl::DownloadFavicon(const GURL& url,
                              bool is_favicon,
                              int image_size,
                              const FaviconDownloadCallback& callback) {
  RenderViewHost* host = GetRenderViewHost();
  int id = StartDownload(host, url, is_favicon, image_size);
  favicon_download_map_[id] = callback;
  return id;
}

bool WebContentsImpl::FocusLocationBarByDefault() {
  WebUI* web_ui = GetWebUIForCurrentState();
  if (web_ui)
    return web_ui->ShouldFocusLocationBarByDefault();
  NavigationEntry* entry = controller_.GetActiveEntry();
  if (entry && entry->GetURL() == GURL(chrome::kAboutBlankURL))
    return true;
  return delegate_ && delegate_->ShouldFocusLocationBarByDefault(this);
}

void WebContentsImpl::SetFocusToLocationBar(bool select_all) {
  if (delegate_)
    delegate_->SetFocusToLocationBar(select_all);
}

void WebContentsImpl::DidStartProvisionalLoadForFrame(
    RenderViewHost* render_view_host,
    int64 frame_id,
    int64 parent_frame_id,
    bool is_main_frame,
    const GURL& url) {
  bool is_error_page = (url.spec() == kUnreachableWebDataURL);
  bool is_iframe_srcdoc = (url.spec() == chrome::kAboutSrcDocURL);
  GURL validated_url(url);
  RenderProcessHost* render_process_host =
      render_view_host->GetProcess();
  RenderViewHost::FilterURL(render_process_host, false, &validated_url);

  if (is_main_frame)
    DidChangeLoadProgress(0);

  // Notify observers about the start of the provisional load.
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidStartProvisionalLoadForFrame(frame_id, parent_frame_id,
                    is_main_frame, validated_url, is_error_page,
                    is_iframe_srcdoc, render_view_host));

  if (is_main_frame) {
    // Notify observers about the provisional change in the main frame URL.
    FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                      ProvisionalChangeToMainFrameUrl(validated_url,
                                                      render_view_host));
  }
}

void WebContentsImpl::DidRedirectProvisionalLoad(
    RenderViewHost* render_view_host,
    int32 page_id,
    const GURL& source_url,
    const GURL& target_url) {
  // TODO(creis): Remove this method and have the pre-rendering code listen to
  // the ResourceDispatcherHost's RESOURCE_RECEIVED_REDIRECT notification
  // instead.  See http://crbug.com/78512.
  GURL validated_source_url(source_url);
  GURL validated_target_url(target_url);
  RenderProcessHost* render_process_host =
      render_view_host->GetProcess();
  RenderViewHost::FilterURL(render_process_host, false, &validated_source_url);
  RenderViewHost::FilterURL(render_process_host, false, &validated_target_url);
  NavigationEntry* entry;
  if (page_id == -1) {
    entry = controller_.GetPendingEntry();
  } else {
    entry = controller_.GetEntryWithPageID(render_view_host->GetSiteInstance(),
                                           page_id);
  }
  if (!entry || entry->GetURL() != validated_source_url)
    return;

  // Notify observers about the provisional change in the main frame URL.
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    ProvisionalChangeToMainFrameUrl(validated_target_url,
                                                    render_view_host));
}

void WebContentsImpl::DidFailProvisionalLoadWithError(
    RenderViewHost* render_view_host,
    const ViewHostMsg_DidFailProvisionalLoadWithError_Params& params) {
  VLOG(1) << "Failed Provisional Load: " << params.url.possibly_invalid_spec()
          << ", error_code: " << params.error_code
          << ", error_description: " << params.error_description
          << ", is_main_frame: " << params.is_main_frame
          << ", showing_repost_interstitial: " <<
            params.showing_repost_interstitial
          << ", frame_id: " << params.frame_id;
  GURL validated_url(params.url);
  RenderProcessHost* render_process_host =
      render_view_host->GetProcess();
  RenderViewHost::FilterURL(render_process_host, false, &validated_url);

  if (net::ERR_ABORTED == params.error_code) {
    // EVIL HACK ALERT! Ignore failed loads when we're showing interstitials.
    // This means that the interstitial won't be torn down properly, which is
    // bad. But if we have an interstitial, go back to another tab type, and
    // then load the same interstitial again, we could end up getting the first
    // interstitial's "failed" message (as a result of the cancel) when we're on
    // the second one.
    //
    // We can't tell this apart, so we think we're tearing down the current page
    // which will cause a crash later one. There is also some code in
    // RenderViewHostManager::RendererAbortedProvisionalLoad that is commented
    // out because of this problem.
    //
    // http://code.google.com/p/chromium/issues/detail?id=2855
    // Because this will not tear down the interstitial properly, if "back" is
    // back to another tab type, the interstitial will still be somewhat alive
    // in the previous tab type. If you navigate somewhere that activates the
    // tab with the interstitial again, you'll see a flash before the new load
    // commits of the interstitial page.
    if (ShowingInterstitialPage()) {
      LOG(WARNING) << "Discarding message during interstitial.";
      return;
    }

    // Do not clear the pending entry if one exists, so that the user's typed
    // URL is not lost when a navigation fails or is aborted.  We'll allow
    // the view to clear the pending entry and typed URL if the user requests.

    render_manager_.RendererAbortedProvisionalLoad(render_view_host);
  }

  FOR_EACH_OBSERVER(WebContentsObserver,
                    observers_,
                    DidFailProvisionalLoad(params.frame_id,
                                           params.is_main_frame,
                                           validated_url,
                                           params.error_code,
                                           params.error_description,
                                           render_view_host));
}

void WebContentsImpl::OnDidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& security_info,
    const std::string& http_method,
    const std::string& mime_type,
    ResourceType::Type resource_type) {
  base::StatsCounter cache("WebKit.CacheHit");
  cache.Increment();

  // Send out a notification that we loaded a resource from our memory cache.
  int cert_id = 0;
  net::CertStatus cert_status = 0;
  int security_bits = -1;
  int connection_status = 0;
  DeserializeSecurityInfo(security_info, &cert_id, &cert_status,
                          &security_bits, &connection_status);
  LoadFromMemoryCacheDetails details(
      url, GetRenderProcessHost()->GetID(), cert_id, cert_status, http_method,
      mime_type, resource_type);

  NotificationService::current()->Notify(
      NOTIFICATION_LOAD_FROM_MEMORY_CACHE,
      Source<NavigationController>(&controller_),
      Details<LoadFromMemoryCacheDetails>(&details));
}

void WebContentsImpl::OnDidDisplayInsecureContent() {
  RecordAction(UserMetricsAction("SSL.DisplayedInsecureContent"));
  displayed_insecure_content_ = true;
  SSLManager::NotifySSLInternalStateChanged(
      GetController().GetBrowserContext());
}

void WebContentsImpl::OnDidRunInsecureContent(
    const std::string& security_origin, const GURL& target_url) {
  LOG(INFO) << security_origin << " ran insecure content from "
            << target_url.possibly_invalid_spec();
  RecordAction(UserMetricsAction("SSL.RanInsecureContent"));
  if (EndsWith(security_origin, kDotGoogleDotCom, false))
    RecordAction(UserMetricsAction("SSL.RanInsecureContentGoogle"));
  controller_.ssl_manager()->DidRunInsecureContent(security_origin);
  displayed_insecure_content_ = true;
  SSLManager::NotifySSLInternalStateChanged(
      GetController().GetBrowserContext());
}

void WebContentsImpl::OnDocumentLoadedInFrame(int64 frame_id) {
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DocumentLoadedInFrame(frame_id, message_source_));
}

void WebContentsImpl::OnDidFinishLoad(
    int64 frame_id,
    const GURL& url,
    bool is_main_frame) {
  GURL validated_url(url);
  RenderProcessHost* render_process_host = message_source_->GetProcess();
  RenderViewHost::FilterURL(render_process_host, false, &validated_url);
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidFinishLoad(frame_id, validated_url, is_main_frame,
                                  message_source_));
}

void WebContentsImpl::OnDidFailLoadWithError(
    int64 frame_id,
    const GURL& url,
    bool is_main_frame,
    int error_code,
    const string16& error_description) {
  GURL validated_url(url);
  RenderProcessHost* render_process_host = message_source_->GetProcess();
  RenderViewHost::FilterURL(render_process_host, false, &validated_url);
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidFailLoad(frame_id, validated_url, is_main_frame,
                                error_code, error_description,
                                message_source_));
}

void WebContentsImpl::OnUpdateContentRestrictions(int restrictions) {
  content_restrictions_ = restrictions;
  if (delegate_)
    delegate_->ContentRestrictionsChanged(this);
}

void WebContentsImpl::OnGoToEntryAtOffset(int offset) {
  if (!delegate_ || delegate_->OnGoToEntryOffset(offset)) {
    NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
        controller_.GetEntryAtOffset(offset));
    if (!entry)
      return;
    // Note that we don't call NavigationController::GotToOffset() as we don't
    // want to create a pending navigation entry (it might end up lingering
    // http://crbug.com/51680).
    entry->SetTransitionType(
        PageTransitionFromInt(
            entry->GetTransitionType() |
            PAGE_TRANSITION_FORWARD_BACK));
    NavigateToEntry(*entry, NavigationControllerImpl::NO_RELOAD);

    // If the entry is being restored and doesn't have a SiteInstance yet, fill
    // it in now that we know. This allows us to find the entry when it commits.
    if (!entry->site_instance() &&
        entry->restore_type() != NavigationEntryImpl::RESTORE_NONE) {
      entry->set_site_instance(
          static_cast<SiteInstanceImpl*>(GetPendingSiteInstance()));
    }
  }
}

void WebContentsImpl::OnUpdateZoomLimits(int minimum_percent,
                                         int maximum_percent,
                                         bool remember) {
  minimum_zoom_percent_ = minimum_percent;
  maximum_zoom_percent_ = maximum_percent;
  temporary_zoom_settings_ = !remember;
}

void WebContentsImpl::OnSaveURL(const GURL& url,
                                const Referrer& referrer) {
  RecordDownloadSource(INITIATED_BY_PEPPER_SAVE);
  // Check if the URL to save matches the URL of the main frame. Since this
  // message originates from Pepper plugins, it may not be the case if the
  // plugin is an embedded element.
  GURL main_frame_url = GetURL();
  if (!main_frame_url.is_valid())
    return;
  bool is_main_frame = (url == main_frame_url);
  SaveURL(url, referrer, is_main_frame);
}

void WebContentsImpl::OnEnumerateDirectory(int request_id,
                                           const base::FilePath& path) {
  if (!delegate_)
    return;

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->CanReadDirectory(GetRenderProcessHost()->GetID(), path))
    delegate_->EnumerateDirectory(this, request_id, path);
}

void WebContentsImpl::OnJSOutOfMemory() {
  if (delegate_)
    delegate_->JSOutOfMemory(this);
}

void WebContentsImpl::OnRegisterProtocolHandler(const std::string& protocol,
                                                const GURL& url,
                                                const string16& title,
                                                bool user_gesture) {
  if (!delegate_)
    return;

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->IsPseudoScheme(protocol) || policy->IsDisabledScheme(protocol))
    return;
  delegate_->RegisterProtocolHandler(this, protocol, url, title, user_gesture);
}

void WebContentsImpl::OnFindReply(int request_id,
                                  int number_of_matches,
                                  const gfx::Rect& selection_rect,
                                  int active_match_ordinal,
                                  bool final_update) {
  if (delegate_) {
    delegate_->FindReply(this, request_id, number_of_matches, selection_rect,
                         active_match_ordinal, final_update);
  }
}

#if defined(OS_ANDROID)
void WebContentsImpl::OnFindMatchRectsReply(
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect) {
  if (delegate_)
    delegate_->FindMatchRectsReply(this, version, rects, active_rect);
}

void WebContentsImpl::OnOpenDateTimeDialog(
    const ViewHostMsg_DateTimeDialogValue_Params& value) {
  date_time_chooser_->ShowDialog(
      view_->GetContentNativeView(), GetRenderViewHost(), value.dialog_type,
      value.year, value.month, value.day, value.hour,
      value.minute, value.second);
}

#endif

void WebContentsImpl::OnCrashedPlugin(const base::FilePath& plugin_path,
                                      base::ProcessId plugin_pid) {
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    PluginCrashed(plugin_path, plugin_pid));
}

void WebContentsImpl::OnAppCacheAccessed(const GURL& manifest_url,
                                         bool blocked_by_policy) {
  // Notify observers about navigation.
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    AppCacheAccessed(manifest_url, blocked_by_policy));
}

void WebContentsImpl::OnOpenColorChooser(int color_chooser_id,
                                         SkColor color) {
  color_chooser_ = delegate_ ?
      delegate_->OpenColorChooser(this, color_chooser_id, color) : NULL;
}

void WebContentsImpl::OnEndColorChooser(int color_chooser_id) {
  if (color_chooser_ &&
      color_chooser_id == color_chooser_->identifier())
    color_chooser_->End();
}

void WebContentsImpl::OnSetSelectedColorInColorChooser(int color_chooser_id,
                                                       SkColor color) {
  if (color_chooser_ &&
      color_chooser_id == color_chooser_->identifier())
    color_chooser_->SetSelectedColor(color);
}

void WebContentsImpl::OnPepperPluginHung(int plugin_child_id,
                                         const base::FilePath& path,
                                         bool is_hung) {
  UMA_HISTOGRAM_COUNTS("Pepper.PluginHung", 1);

  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    PluginHungStatusChanged(plugin_child_id, path, is_hung));
}

// This exists for render views that don't have a WebUI, but do have WebUI
// bindings enabled.
void WebContentsImpl::OnWebUISend(const GURL& source_url,
                                  const std::string& name,
                                  const base::ListValue& args) {
  if (delegate_)
    delegate_->WebUISend(this, source_url, name, args);
}

void WebContentsImpl::OnRequestPpapiBrokerPermission(
    int request_id,
    const GURL& url,
    const base::FilePath& plugin_path) {
  if (!delegate_) {
    OnPpapiBrokerPermissionResult(request_id, false);
    return;
  }

  if (!delegate_->RequestPpapiBrokerPermission(
      this, url, plugin_path,
      base::Bind(&WebContentsImpl::OnPpapiBrokerPermissionResult,
                 base::Unretained(this), request_id))) {
    NOTIMPLEMENTED();
    OnPpapiBrokerPermissionResult(request_id, false);
  }
}

void WebContentsImpl::OnPpapiBrokerPermissionResult(int request_id,
                                                    bool result) {
  RenderViewHostImpl* rvh = GetRenderViewHostImpl();
  rvh->Send(new ViewMsg_PpapiBrokerPermissionResult(rvh->GetRoutingID(),
                                                    request_id,
                                                    result));
}

void WebContentsImpl::OnBrowserPluginMessage(const IPC::Message& message) {
  // This creates a BrowserPluginEmbedder, which handles all the BrowserPlugin
  // specific messages for this WebContents. This means that any message from
  // a BrowserPlugin prior to this will be ignored.
  // For more info, see comment above classes BrowserPluginEmbedder and
  // BrowserPluginGuest.
  CHECK(!browser_plugin_embedder_.get());
  browser_plugin_embedder_.reset(BrowserPluginEmbedder::Create(this));
  browser_plugin_embedder_->OnMessageReceived(message);
}

void WebContentsImpl::OnDidDownloadFavicon(
    int id,
    const GURL& image_url,
    int requested_size,
    const std::vector<SkBitmap>& bitmaps) {
  FaviconDownloadMap::iterator iter = favicon_download_map_.find(id);
  if (iter == favicon_download_map_.end()) {
    // Currently WebContents notifies us of ANY downloads so that it is
    // possible to get here.
    return;
  }
  if (!iter->second.is_null()) {
    iter->second.Run(id, image_url, requested_size, bitmaps);
  }
  favicon_download_map_.erase(id);
}

void WebContentsImpl::OnUpdateFaviconURL(
    int32 page_id,
    const std::vector<FaviconURL>& candidates) {
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidUpdateFaviconURL(page_id, candidates));
}

void WebContentsImpl::OnFrameDetached(int64 frame_id) {
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    FrameDetached(message_source_, frame_id));
}

void WebContentsImpl::DidChangeVisibleSSLState() {
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidChangeVisibleSSLState());
}

void WebContentsImpl::NotifyBeforeFormRepostWarningShow() {
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    BeforeFormRepostWarningShow());
}

// Notifies the RenderWidgetHost instance about the fact that the page is
// loading, or done loading and calls the base implementation.
void WebContentsImpl::SetIsLoading(bool is_loading,
                                   LoadNotificationDetails* details) {
  if (is_loading == is_loading_)
    return;

  if (!is_loading) {
    load_state_ = net::LoadStateWithParam(net::LOAD_STATE_IDLE, string16());
    load_state_host_.clear();
    upload_size_ = 0;
    upload_position_ = 0;
  }

  render_manager_.SetIsLoading(is_loading);

  is_loading_ = is_loading;
  waiting_for_response_ = is_loading;

  if (delegate_)
    delegate_->LoadingStateChanged(this);
  NotifyNavigationStateChanged(INVALIDATE_TYPE_LOAD);

  if (is_loading)
    TRACE_EVENT_ASYNC_BEGIN0("browser", "WebContentsImpl Loading", this);
  else
    TRACE_EVENT_ASYNC_END0("browser", "WebContentsImpl Loading", this);
  int type = is_loading ? NOTIFICATION_LOAD_START : NOTIFICATION_LOAD_STOP;
  NotificationDetails det = NotificationService::NoDetails();
  if (details)
      det = Details<LoadNotificationDetails>(details);
  NotificationService::current()->Notify(
      type, Source<NavigationController>(&controller_), det);
}

void WebContentsImpl::DidNavigateMainFramePostCommit(
    const LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (details.is_navigation_to_different_page()) {
    // Clear the status bubble. This is a workaround for a bug where WebKit
    // doesn't let us know that the cursor left an element during a
    // transition (this is also why the mouse cursor remains as a hand after
    // clicking on a link); see bugs 1184641 and 980803. We don't want to
    // clear the bubble when a user navigates to a named anchor in the same
    // page.
    UpdateTargetURL(details.entry->GetPageID(), GURL());
  }

  if (!details.is_in_page) {
    // Once the main frame is navigated, we're no longer considered to have
    // displayed insecure content.
    displayed_insecure_content_ = false;
  }

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidNavigateMainFrame(details, params));
}

void WebContentsImpl::DidNavigateAnyFramePostCommit(
    RenderViewHost* render_view_host,
    const LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  // If we navigate off the page, reset JavaScript state. This does nothing
  // to prevent a malicious script from spamming messages, since the script
  // could just reload the page to stop blocking.
  if (dialog_manager_ && !details.is_in_page) {
    dialog_manager_->ResetJavaScriptState(this);
    dialog_manager_ = NULL;
  }

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidNavigateAnyFrame(details, params));
}

void WebContentsImpl::UpdateMaxPageIDIfNecessary(RenderViewHost* rvh) {
  // If we are creating a RVH for a restored controller, then we need to make
  // sure the RenderView starts with a next_page_id_ larger than the number
  // of restored entries.  This must be called before the RenderView starts
  // navigating (to avoid a race between the browser updating max_page_id and
  // the renderer updating next_page_id_).  Because of this, we only call this
  // from CreateRenderView and allow that to notify the RenderView for us.
  int max_restored_page_id = controller_.GetMaxRestoredPageID();
  if (max_restored_page_id >
      GetMaxPageIDForSiteInstance(rvh->GetSiteInstance()))
    UpdateMaxPageIDForSiteInstance(rvh->GetSiteInstance(),
                                   max_restored_page_id);
}

bool WebContentsImpl::UpdateTitleForEntry(NavigationEntryImpl* entry,
                                          const string16& title) {
  // For file URLs without a title, use the pathname instead. In the case of a
  // synthesized title, we don't want the update to count toward the "one set
  // per page of the title to history."
  string16 final_title;
  bool explicit_set;
  if (entry && entry->GetURL().SchemeIsFile() && title.empty()) {
    final_title = UTF8ToUTF16(entry->GetURL().ExtractFileName());
    explicit_set = false;  // Don't count synthetic titles toward the set limit.
  } else {
    TrimWhitespace(title, TRIM_ALL, &final_title);
    explicit_set = true;
  }

  // If a page is created via window.open and never navigated,
  // there will be no navigation entry. In this situation,
  // |page_title_when_no_navigation_entry_| will be used for page title.
  if (entry) {
    if (final_title == entry->GetTitle())
      return false;  // Nothing changed, don't bother.

    entry->SetTitle(final_title);
  } else {
    if (page_title_when_no_navigation_entry_ == final_title)
      return false;  // Nothing changed, don't bother.

    page_title_when_no_navigation_entry_ = final_title;
  }

  // Lastly, set the title for the view.
  view_->SetPageTitle(final_title);

  std::pair<NavigationEntry*, bool> details =
      std::make_pair(entry, explicit_set);

  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
      Source<WebContents>(this),
      Details<std::pair<NavigationEntry*, bool> >(&details));

  return true;
}

void WebContentsImpl::NotifySwapped(RenderViewHost* old_render_view_host) {
  // After sending out a swap notification, we need to send a disconnect
  // notification so that clients that pick up a pointer to |this| can NULL the
  // pointer.  See Bug 1230284.
  notify_disconnection_ = true;
  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_SWAPPED,
      Source<WebContents>(this),
      Details<RenderViewHost>(old_render_view_host));

  // Ensure that the associated embedder gets cleared after a RenderViewHost
  // gets swapped, so we don't reuse the same embedder next time a
  // RenderViewHost is attached to this WebContents.
  RemoveBrowserPluginEmbedder();
}

void WebContentsImpl::NotifyConnected() {
  notify_disconnection_ = true;
  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_CONNECTED,
      Source<WebContents>(this),
      NotificationService::NoDetails());
}

void WebContentsImpl::NotifyDisconnected() {
  if (!notify_disconnection_)
    return;

  notify_disconnection_ = false;
  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      Source<WebContents>(this),
      NotificationService::NoDetails());
}

RenderViewHostDelegateView* WebContentsImpl::GetDelegateView() {
  return render_view_host_delegate_view_;
}

RenderViewHostDelegate::RendererManagement*
WebContentsImpl::GetRendererManagementDelegate() {
  return &render_manager_;
}

RendererPreferences WebContentsImpl::GetRendererPrefs(
    BrowserContext* browser_context) const {
  return renderer_preferences_;
}

WebContents* WebContentsImpl::GetAsWebContents() {
  return this;
}

gfx::Rect WebContentsImpl::GetRootWindowResizerRect() const {
  if (delegate_)
    return delegate_->GetRootWindowResizerRect();
  return gfx::Rect();
}

void WebContentsImpl::RemoveBrowserPluginEmbedder() {
  if (browser_plugin_embedder_.get())
    browser_plugin_embedder_.reset();
}

void WebContentsImpl::RenderViewCreated(RenderViewHost* render_view_host) {
  // Don't send notifications if we are just creating a swapped-out RVH for
  // the opener chain.  These won't be used for view-source or WebUI, so it's
  // ok to return early.
  if (static_cast<RenderViewHostImpl*>(render_view_host)->is_swapped_out())
    return;

  if (delegate_)
    view_->SetOverscrollControllerEnabled(delegate_->CanOverscrollContent());

  NotificationService::current()->Notify(
      NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
      Source<WebContents>(this),
      Details<RenderViewHost>(render_view_host));
  NavigationEntry* entry = controller_.GetActiveEntry();
  if (!entry)
    return;

  // When we're creating views, we're still doing initial setup, so we always
  // use the pending Web UI rather than any possibly existing committed one.
  if (render_manager_.pending_web_ui())
    render_manager_.pending_web_ui()->RenderViewCreated(render_view_host);

  if (entry->IsViewSourceMode()) {
    // Put the renderer in view source mode.
    render_view_host->Send(
        new ViewMsg_EnableViewSourceMode(render_view_host->GetRoutingID()));
  }

  view_->RenderViewCreated(render_view_host);

  FOR_EACH_OBSERVER(
      WebContentsObserver, observers_, RenderViewCreated(render_view_host));
}

void WebContentsImpl::RenderViewReady(RenderViewHost* rvh) {
  if (rvh != GetRenderViewHost()) {
    // Don't notify the world, since this came from a renderer in the
    // background.
    return;
  }

  NotifyConnected();
  bool was_crashed = IsCrashed();
  SetIsCrashed(base::TERMINATION_STATUS_STILL_RUNNING, 0);

  // Restore the focus to the tab (otherwise the focus will be on the top
  // window).
  if (was_crashed && !FocusLocationBarByDefault() &&
      (!delegate_ || delegate_->ShouldFocusPageAfterCrash())) {
    view_->Focus();
  }

  FOR_EACH_OBSERVER(WebContentsObserver, observers_, RenderViewReady());
}

void WebContentsImpl::RenderViewGone(RenderViewHost* rvh,
                                     base::TerminationStatus status,
                                     int error_code) {
  if (rvh != GetRenderViewHost()) {
    // The pending page's RenderViewHost is gone.
    return;
  }

  SetIsLoading(false, NULL);
  NotifyDisconnected();
  SetIsCrashed(status, error_code);
  GetView()->OnTabCrashed(GetCrashedStatus(), crashed_error_code_);

  FOR_EACH_OBSERVER(WebContentsObserver,
                    observers_,
                    RenderViewGone(GetCrashedStatus()));
}

void WebContentsImpl::RenderViewDeleted(RenderViewHost* rvh) {
  render_manager_.RenderViewDeleted(rvh);
  FOR_EACH_OBSERVER(WebContentsObserver, observers_, RenderViewDeleted(rvh));
}

void WebContentsImpl::DidNavigate(
    RenderViewHost* rvh,
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (PageTransitionIsMainFrame(params.transition)) {
    // When overscroll navigation gesture is enabled, a screenshot of the page
    // in its current state is taken so that it can be used during the
    // nav-gesture. It is necessary to take the screenshot here, before calling
    // RenderViewHostManager::DidNavigateMainFrame, because that can change
    // WebContents::GetRenderViewHost to return the new host, instead of the one
    // that may have just been swapped out.
    if (delegate_ && delegate_->CanOverscrollContent())
      controller_.TakeScreenshot();

    render_manager_.DidNavigateMainFrame(rvh);
  }

  // Update the site of the SiteInstance if it doesn't have one yet, unless
  // this is for about:blank.  In that case, the SiteInstance can still be
  // considered unused until a navigation to a real page.
  if (!static_cast<SiteInstanceImpl*>(GetSiteInstance())->HasSite() &&
      params.url != GURL(chrome::kAboutBlankURL)) {
    static_cast<SiteInstanceImpl*>(GetSiteInstance())->SetSite(params.url);
  }

  // Need to update MIME type here because it's referred to in
  // UpdateNavigationCommands() called by RendererDidNavigate() to
  // determine whether or not to enable the encoding menu.
  // It's updated only for the main frame. For a subframe,
  // RenderView::UpdateURL does not set params.contents_mime_type.
  // (see http://code.google.com/p/chromium/issues/detail?id=2929 )
  // TODO(jungshik): Add a test for the encoding menu to avoid
  // regressing it again.
  if (PageTransitionIsMainFrame(params.transition))
    contents_mime_type_ = params.contents_mime_type;

  LoadCommittedDetails details;
  bool did_navigate = controller_.RendererDidNavigate(params, &details);

  // Send notification about committed provisional loads. This notification is
  // different from the NAV_ENTRY_COMMITTED notification which doesn't include
  // the actual URL navigated to and isn't sent for AUTO_SUBFRAME navigations.
  if (details.type != NAVIGATION_TYPE_NAV_IGNORE) {
    // For AUTO_SUBFRAME navigations, an event for the main frame is generated
    // that is not recorded in the navigation history. For the purpose of
    // tracking navigation events, we treat this event as a sub frame navigation
    // event.
    bool is_main_frame = did_navigate ? details.is_main_frame : false;
    PageTransition transition_type = params.transition;
    // Whether or not a page transition was triggered by going backward or
    // forward in the history is only stored in the navigation controller's
    // entry list.
    if (did_navigate &&
        (controller_.GetActiveEntry()->GetTransitionType() &
            PAGE_TRANSITION_FORWARD_BACK)) {
      transition_type = PageTransitionFromInt(
          params.transition | PAGE_TRANSITION_FORWARD_BACK);
    }
    // Notify observers about the commit of the provisional load.
    FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                      DidCommitProvisionalLoadForFrame(params.frame_id,
                      is_main_frame, params.url, transition_type, rvh));
  }

  if (!did_navigate)
    return;  // No navigation happened.

  // DO NOT ADD MORE STUFF TO THIS FUNCTION! Your component should either listen
  // for the appropriate notification (best) or you can add it to
  // DidNavigateMainFramePostCommit / DidNavigateAnyFramePostCommit (only if
  // necessary, please).

  // Run post-commit tasks.
  if (details.is_main_frame) {
    DidNavigateMainFramePostCommit(details, params);
    if (delegate_) {
      delegate_->DidNavigateMainFramePostCommit(this);
      view_->SetOverscrollControllerEnabled(delegate_->CanOverscrollContent());
    }
  }
  DidNavigateAnyFramePostCommit(rvh, details, params);
}

void WebContentsImpl::UpdateState(RenderViewHost* rvh,
                                  int32 page_id,
                                  const std::string& state) {
  // Ensure that this state update comes from either the active RVH or one of
  // the swapped out RVHs.  We don't expect to hear from any other RVHs.
  DCHECK(rvh == GetRenderViewHost() || render_manager_.IsSwappedOut(rvh));

  // We must be prepared to handle state updates for any page, these occur
  // when the user is scrolling and entering form data, as well as when we're
  // leaving a page, in which case our state may have already been moved to
  // the next page. The navigation controller will look up the appropriate
  // NavigationEntry and update it when it is notified via the delegate.

  int entry_index = controller_.GetEntryIndexWithPageID(
      rvh->GetSiteInstance(), page_id);
  if (entry_index < 0)
    return;
  NavigationEntry* entry = controller_.GetEntryAtIndex(entry_index);

  if (state == entry->GetContentState())
    return;  // Nothing to update.
  entry->SetContentState(state);
  controller_.NotifyEntryChanged(entry, entry_index);
}

void WebContentsImpl::UpdateTitle(RenderViewHost* rvh,
                                  int32 page_id,
                                  const string16& title,
                                  base::i18n::TextDirection title_direction) {
  // If we have a title, that's a pretty good indication that we've started
  // getting useful data.
  SetNotWaitingForResponse();

  // Try to find the navigation entry, which might not be the current one.
  // For example, it might be from a pending RVH for the pending entry.
  NavigationEntryImpl* entry = controller_.GetEntryWithPageID(
      rvh->GetSiteInstance(), page_id);

  // We can handle title updates when we don't have an entry in
  // UpdateTitleForEntry, but only if the update is from the current RVH.
  if (!entry && rvh != GetRenderViewHost())
    return;

  // TODO(evan): make use of title_direction.
  // http://code.google.com/p/chromium/issues/detail?id=27094
  if (!UpdateTitleForEntry(entry, title))
    return;

  // Broadcast notifications when the UI should be updated.
  if (entry == controller_.GetEntryAtOffset(0))
    NotifyNavigationStateChanged(INVALIDATE_TYPE_TITLE);
}

void WebContentsImpl::UpdateEncoding(RenderViewHost* render_view_host,
                                     const std::string& encoding) {
  SetEncoding(encoding);
}

void WebContentsImpl::UpdateTargetURL(int32 page_id, const GURL& url) {
  if (delegate_)
    delegate_->UpdateTargetURL(this, page_id, url);
}

void WebContentsImpl::Close(RenderViewHost* rvh) {
#if defined(OS_MACOSX)
  // The UI may be in an event-tracking loop, such as between the
  // mouse-down and mouse-up in text selection or a button click.
  // Defer the close until after tracking is complete, so that we
  // don't free objects out from under the UI.
  // TODO(shess): This could get more fine-grained.  For instance,
  // closing a tab in another window while selecting text in the
  // current window's Omnibox should be just fine.
  if (view_->IsEventTracking()) {
    view_->CloseTabAfterEventTracking();
    return;
  }
#endif

  // Ignore this if it comes from a RenderViewHost that we aren't showing.
  if (delegate_ && rvh == GetRenderViewHost())
    delegate_->CloseContents(this);
}

void WebContentsImpl::SwappedOut(RenderViewHost* rvh) {
  if (delegate_ && rvh == GetRenderViewHost())
    delegate_->SwappedOut(this);
}

void WebContentsImpl::RequestMove(const gfx::Rect& new_bounds) {
  if (delegate_ && delegate_->IsPopupOrPanel(this))
    delegate_->MoveContents(this, new_bounds);
}

void WebContentsImpl::DidStartLoading(RenderViewHost* render_view_host) {
  SetIsLoading(true, NULL);

  if (delegate_ && content_restrictions_)
    OnUpdateContentRestrictions(0);

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidStartLoading(render_view_host));
}

void WebContentsImpl::DidStopLoading(RenderViewHost* render_view_host) {
  scoped_ptr<LoadNotificationDetails> details;

  NavigationEntry* entry = controller_.GetActiveEntry();
  // An entry may not exist for a stop when loading an initial blank page or
  // if an iframe injected by script into a blank page finishes loading.
  if (entry) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - current_load_start_;

    details.reset(new LoadNotificationDetails(
        entry->GetVirtualURL(),
        entry->GetTransitionType(),
        elapsed,
        &controller_,
        controller_.GetCurrentEntryIndex()));
  }

  SetIsLoading(false, details.get());

  // Notify observers about navigation.
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DidStopLoading(render_view_host));
}

void WebContentsImpl::DidCancelLoading() {
  controller_.DiscardNonCommittedEntries();

  // Update the URL display.
  NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
}

void WebContentsImpl::DidChangeLoadProgress(double progress) {
#if defined(OS_ANDROID)
  if (delegate_)
    delegate_->LoadProgressChanged(this, progress);
#endif
}

void WebContentsImpl::DidDisownOpener(RenderViewHost* rvh) {
  // Clear our opener so that future cross-process navigations don't have an
  // opener assigned.
  registrar_.Remove(this, NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    Source<WebContents>(opener_));
  opener_ = NULL;

  // Notify all swapped out RenderViewHosts for this tab.  This is important
  // in case we go back to them, or if another window in those processes tries
  // to access window.opener.
  render_manager_.DidDisownOpener(rvh);
}

void WebContentsImpl::DidUpdateFrameTree(RenderViewHost* rvh) {
  render_manager_.DidUpdateFrameTree(rvh);
}

void WebContentsImpl::DocumentAvailableInMainFrame(
    RenderViewHost* render_view_host) {
  FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                    DocumentAvailableInMainFrame());
}

void WebContentsImpl::DocumentOnLoadCompletedInMainFrame(
    RenderViewHost* render_view_host,
    int32 page_id) {
  NotificationService::current()->Notify(
      NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      Source<WebContents>(this),
      Details<int>(&page_id));
}

void WebContentsImpl::RequestOpenURL(RenderViewHost* rvh,
                                     const GURL& url,
                                     const Referrer& referrer,
                                     WindowOpenDisposition disposition,
                                     int64 source_frame_id,
                                     bool is_cross_site_redirect) {
  // If this came from a swapped out RenderViewHost, we only allow the request
  // if we are still in the same BrowsingInstance.
  if (static_cast<RenderViewHostImpl*>(rvh)->is_swapped_out() &&
      !rvh->GetSiteInstance()->IsRelatedSiteInstance(GetSiteInstance())) {
    return;
  }

  // Delegate to RequestTransferURL because this is just the generic
  // case where |old_request_id| is empty.
  RequestTransferURL(url, referrer, disposition, source_frame_id,
                     GlobalRequestID(), is_cross_site_redirect);
}

void WebContentsImpl::RequestTransferURL(
    const GURL& url,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    int64 source_frame_id,
    const GlobalRequestID& old_request_id,
    bool is_cross_site_redirect) {
  WebContents* new_contents = NULL;
  PageTransition transition_type = PAGE_TRANSITION_LINK;
  if (render_manager_.web_ui()) {
    // When we're a Web UI, it will provide a page transition type for us (this
    // is so the new tab page can specify AUTO_BOOKMARK for automatically
    // generated suggestions).
    //
    // Note also that we hide the referrer for Web UI pages. We don't really
    // want web sites to see a referrer of "chrome://blah" (and some
    // chrome: URLs might have search terms or other stuff we don't want to
    // send to the site), so we send no referrer.
    OpenURLParams params(url, Referrer(), source_frame_id, disposition,
        render_manager_.web_ui()->GetLinkTransitionType(),
        false /* is_renderer_initiated */);
    params.transferred_global_request_id = old_request_id;
    new_contents = OpenURL(params);
    transition_type = render_manager_.web_ui()->GetLinkTransitionType();
  } else {
    OpenURLParams params(url, referrer, source_frame_id, disposition,
        PAGE_TRANSITION_LINK, true /* is_renderer_initiated */);
    params.transferred_global_request_id = old_request_id;
    params.is_cross_site_redirect = is_cross_site_redirect;
    new_contents = OpenURL(params);
  }
  if (new_contents) {
    // Notify observers.
    FOR_EACH_OBSERVER(WebContentsObserver, observers_,
                      DidOpenRequestedURL(new_contents,
                                          url,
                                          referrer,
                                          disposition,
                                          transition_type,
                                          source_frame_id));
  }
}

void WebContentsImpl::RouteCloseEvent(RenderViewHost* rvh) {
  // Tell the active RenderViewHost to run unload handlers and close, as long
  // as the request came from a RenderViewHost in the same BrowsingInstance.
  // In most cases, we receive this from a swapped out RenderViewHost.
  // It is possible to receive it from one that has just been swapped in,
  // in which case we might as well deliver the message anyway.
  if (rvh->GetSiteInstance()->IsRelatedSiteInstance(GetSiteInstance()))
    GetRenderViewHost()->ClosePage();
}

void WebContentsImpl::RouteMessageEvent(
    RenderViewHost* rvh,
    const ViewMsg_PostMessage_Params& params) {
  // Only deliver the message to the active RenderViewHost if the request
  // came from a RenderViewHost in the same BrowsingInstance or if this
  // WebContents is dedicated to a browser plugin guest.
  // Note: This check means that an embedder could theoretically receive a
  // postMessage from anyone (not just its own guests). However, this is
  // probably not a risk for apps since other pages won't have references
  // to App windows.
  if (!rvh->GetSiteInstance()->IsRelatedSiteInstance(GetSiteInstance()) &&
      !GetBrowserPluginGuest() && !GetBrowserPluginEmbedder())
    return;

  ViewMsg_PostMessage_Params new_params(params);

  // If the renderer has changed while the post message is being routed,
  // drop the message, as it will not be delivered to the right target.
  // TODO(nasko): Check for process ID and target frame id mismatch, once
  // http://crbug.com/153701 is fixed.

  // If there is a source_routing_id, translate it to the routing ID for
  // the equivalent swapped out RVH in the target process.  If we need
  // to create a swapped out RVH for the source tab, we create its opener
  // chain as well, since those will also be accessible to the target page.
  if (new_params.source_routing_id != MSG_ROUTING_NONE) {
    // Try to look up the WebContents for the source page.
    WebContentsImpl* source_contents = NULL;
    RenderViewHostImpl* source_rvh = RenderViewHostImpl::FromID(
        rvh->GetProcess()->GetID(), params.source_routing_id);
    if (source_rvh) {
      source_contents = static_cast<WebContentsImpl*>(
          source_rvh->GetDelegate()->GetAsWebContents());
    }

    if (source_contents) {
      if (GetBrowserPluginGuest()) {
        // We create a swapped out RenderView for the embedder in the guest's
        // render process but we intentionally do not expose the embedder's
        // opener chain to it.
        new_params.source_routing_id =
            source_contents->CreateSwappedOutRenderView(GetSiteInstance());
      } else {
        new_params.source_routing_id =
            source_contents->CreateOpenerRenderViews(GetSiteInstance());
      }
    } else {
      // We couldn't find it, so don't pass a source frame.
      new_params.source_routing_id = MSG_ROUTING_NONE;
    }
  }

  // In most cases, we receive this from a swapped out RenderViewHost.
  // It is possible to receive it from one that has just been swapped in,
  // in which case we might as well deliver the message anyway.
  Send(new ViewMsg_PostMessageEvent(GetRoutingID(), new_params));
}

void WebContentsImpl::RunJavaScriptMessage(
    RenderViewHost* rvh,
    const string16& message,
    const string16& default_prompt,
    const GURL& frame_url,
    JavaScriptMessageType javascript_message_type,
    IPC::Message* reply_msg,
    bool* did_suppress_message) {
  // Suppress JavaScript dialogs when requested. Also suppress messages when
  // showing an interstitial as it's shown over the previous page and we don't
  // want the hidden page's dialogs to interfere with the interstitial.
  bool suppress_this_message =
      static_cast<RenderViewHostImpl*>(rvh)->is_swapped_out() ||
      ShowingInterstitialPage() ||
      !delegate_ ||
      delegate_->ShouldSuppressDialogs() ||
      !delegate_->GetJavaScriptDialogManager();

  if (!suppress_this_message) {
    std::string accept_lang = GetContentClient()->browser()->
      GetAcceptLangs(GetBrowserContext());
    dialog_manager_ = delegate_->GetJavaScriptDialogManager();
    dialog_manager_->RunJavaScriptDialog(
        this,
        frame_url.GetOrigin(),
        accept_lang,
        javascript_message_type,
        message,
        default_prompt,
        base::Bind(&WebContentsImpl::OnDialogClosed,
                   base::Unretained(this),
                   rvh,
                   reply_msg),
        &suppress_this_message);
  }

  if (suppress_this_message) {
    // If we are suppressing messages, just reply as if the user immediately
    // pressed "Cancel".
    OnDialogClosed(rvh, reply_msg, false, string16());
  }

  *did_suppress_message = suppress_this_message;
}

void WebContentsImpl::RunBeforeUnloadConfirm(RenderViewHost* rvh,
                                             const string16& message,
                                             bool is_reload,
                                             IPC::Message* reply_msg) {
  RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(rvh);
  if (delegate_)
    delegate_->WillRunBeforeUnloadConfirm();

  bool suppress_this_message =
      rvhi->is_swapped_out() ||
      !delegate_ ||
      delegate_->ShouldSuppressDialogs() ||
      !delegate_->GetJavaScriptDialogManager();
  if (suppress_this_message) {
    // The reply must be sent to the RVH that sent the request.
    rvhi->JavaScriptDialogClosed(reply_msg, true, string16());
    return;
  }

  is_showing_before_unload_dialog_ = true;
  dialog_manager_ = delegate_->GetJavaScriptDialogManager();
  dialog_manager_->RunBeforeUnloadDialog(
      this, message, is_reload,
      base::Bind(&WebContentsImpl::OnDialogClosed, base::Unretained(this), rvh,
                 reply_msg));
}

bool WebContentsImpl::AddMessageToConsole(int32 level,
                                          const string16& message,
                                          int32 line_no,
                                          const string16& source_id) {
  if (!delegate_)
    return false;
  return delegate_->AddMessageToConsole(this, level, message, line_no,
                                        source_id);
}

WebPreferences WebContentsImpl::GetWebkitPrefs() {
  // We want to base the page config off of the real URL, rather than the
  // display URL.
  GURL url = controller_.GetActiveEntry()
      ? controller_.GetActiveEntry()->GetURL() : GURL::EmptyGURL();
  return GetWebkitPrefs(GetRenderViewHost(), url);
}

int WebContentsImpl::CreateSwappedOutRenderView(
    SiteInstance* instance) {
  return render_manager_.CreateRenderView(instance, MSG_ROUTING_NONE, true);
}

void WebContentsImpl::OnUserGesture() {
  // Notify observers.
  FOR_EACH_OBSERVER(WebContentsObserver, observers_, DidGetUserGesture());

  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  if (rdh)  // NULL in unittests.
    rdh->OnUserGesture(this);
}

void WebContentsImpl::OnIgnoredUIEvent() {
  // Notify observers.
  FOR_EACH_OBSERVER(WebContentsObserver, observers_, DidGetIgnoredUIEvent());
}

void WebContentsImpl::RendererUnresponsive(RenderViewHost* rvh,
                                           bool is_during_unload) {
  // Don't show hung renderer dialog for a swapped out RVH.
  if (rvh != GetRenderViewHost())
    return;

  RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(rvh);

  // Ignore renderer unresponsive event if debugger is attached to the tab
  // since the event may be a result of the renderer sitting on a breakpoint.
  // See http://crbug.com/65458
  if (DevToolsAgentHost::IsDebuggerAttached(this))
    return;

  if (is_during_unload) {
    // Hang occurred while firing the beforeunload/unload handler.
    // Pretend the handler fired so tab closing continues as if it had.
    rvhi->set_sudden_termination_allowed(true);

    if (!render_manager_.ShouldCloseTabOnUnresponsiveRenderer())
      return;

    // If the tab hangs in the beforeunload/unload handler there's really
    // nothing we can do to recover. Pretend the unload listeners have
    // all fired and close the tab. If the hang is in the beforeunload handler
    // then the user will not have the option of cancelling the close.
    Close(rvh);
    return;
  }

  if (!GetRenderViewHostImpl() || !GetRenderViewHostImpl()->IsRenderViewLive())
    return;

  if (delegate_)
    delegate_->RendererUnresponsive(this);
}

void WebContentsImpl::RendererResponsive(RenderViewHost* render_view_host) {
  if (delegate_)
    delegate_->RendererResponsive(this);
}

void WebContentsImpl::LoadStateChanged(
    const GURL& url,
    const net::LoadStateWithParam& load_state,
    uint64 upload_position,
    uint64 upload_size) {
  load_state_ = load_state;
  upload_position_ = upload_position;
  upload_size_ = upload_size;
  load_state_host_ = net::IDNToUnicode(url.host(),
      GetContentClient()->browser()->GetAcceptLangs(
          GetBrowserContext()));
  if (load_state_.state == net::LOAD_STATE_READING_RESPONSE)
    SetNotWaitingForResponse();
  if (IsLoading()) {
    NotifyNavigationStateChanged(INVALIDATE_TYPE_LOAD | INVALIDATE_TYPE_TAB);
  }
}

void WebContentsImpl::WorkerCrashed() {
  if (delegate_)
    delegate_->WorkerCrashed(this);
}

void WebContentsImpl::BeforeUnloadFiredFromRenderManager(
    bool proceed, const base::TimeTicks& proceed_time,
    bool* proceed_to_fire_unload) {
  before_unload_end_time_ = proceed_time;
  if (delegate_)
    delegate_->BeforeUnloadFired(this, proceed, proceed_to_fire_unload);
}

void WebContentsImpl::RenderViewGoneFromRenderManager(
    RenderViewHost* render_view_host) {
  DCHECK(crashed_status_ != base::TERMINATION_STATUS_STILL_RUNNING);
  RenderViewGone(render_view_host, crashed_status_, crashed_error_code_);
}

void WebContentsImpl::UpdateRenderViewSizeForRenderManager() {
  // TODO(brettw) this is a hack. See WebContentsView::SizeContents.
  gfx::Size size = view_->GetContainerSize();
  // 0x0 isn't a valid window size (minimal window size is 1x1) but it may be
  // here during container initialization and normal window size will be set
  // later. In case of tab duplication this resizing to 0x0 prevents setting
  // normal size later so just ignore it.
  if (!size.IsEmpty())
    view_->SizeContents(size);
}

void WebContentsImpl::NotifySwappedFromRenderManager(RenderViewHost* rvh) {
  NotifySwapped(rvh);

  // Make sure the visible RVH reflects the new delegate's preferences.
  if (delegate_)
    view_->SetOverscrollControllerEnabled(delegate_->CanOverscrollContent());

  view_->RenderViewSwappedIn(render_manager_.current_host());
}

int WebContentsImpl::CreateOpenerRenderViewsForRenderManager(
    SiteInstance* instance) {
  if (!opener_)
    return MSG_ROUTING_NONE;

  // Recursively create RenderViews for anything else in the opener chain.
  return opener_->CreateOpenerRenderViews(instance);
}

int WebContentsImpl::CreateOpenerRenderViews(SiteInstance* instance) {
  int opener_route_id = MSG_ROUTING_NONE;

  // If this tab has an opener, ensure it has a RenderView in the given
  // SiteInstance as well.
  if (opener_)
    opener_route_id = opener_->CreateOpenerRenderViews(instance);

  // If any of the renderers (current, pending, or swapped out) for this
  // WebContents has the same SiteInstance, use it.
  if (render_manager_.current_host()->GetSiteInstance() == instance)
    return render_manager_.current_host()->GetRoutingID();

  if (render_manager_.pending_render_view_host() &&
      render_manager_.pending_render_view_host()->GetSiteInstance() == instance)
    return render_manager_.pending_render_view_host()->GetRoutingID();

  RenderViewHostImpl* rvh = render_manager_.GetSwappedOutRenderViewHost(
      instance);
  if (rvh)
    return rvh->GetRoutingID();

  // Create a swapped out RenderView in the given SiteInstance if none exists,
  // setting its opener to the given route_id.  Return the new view's route_id.
  return render_manager_.CreateRenderView(instance, opener_route_id, true);
}

NavigationControllerImpl& WebContentsImpl::GetControllerForRenderManager() {
  return GetController();
}

WebUIImpl* WebContentsImpl::CreateWebUIForRenderManager(const GURL& url) {
  return static_cast<WebUIImpl*>(CreateWebUI(url));
}

NavigationEntry*
    WebContentsImpl::GetLastCommittedNavigationEntryForRenderManager() {
  return controller_.GetLastCommittedEntry();
}

bool WebContentsImpl::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host, int opener_route_id) {
  // Can be NULL during tests.
  RenderWidgetHostView* rwh_view = view_->CreateViewForWidget(render_view_host);

  // Now that the RenderView has been created, we need to tell it its size.
  if (rwh_view)
    rwh_view->SetSize(view_->GetContainerSize());

  // Make sure we use the correct starting page_id in the new RenderView.
  UpdateMaxPageIDIfNecessary(render_view_host);
  int32 max_page_id =
      GetMaxPageIDForSiteInstance(render_view_host->GetSiteInstance());

  if (!static_cast<RenderViewHostImpl*>(
          render_view_host)->CreateRenderView(string16(),
                                              opener_route_id,
                                              max_page_id)) {
    return false;
  }

#if defined(OS_LINUX) || defined(OS_OPENBSD)
  // Force a ViewMsg_Resize to be sent, needed to make plugins show up on
  // linux. See crbug.com/83941.
  if (rwh_view) {
    if (RenderWidgetHost* render_widget_host = rwh_view->GetRenderWidgetHost())
      render_widget_host->WasResized();
  }
#endif

  return true;
}

void WebContentsImpl::OnDialogClosed(RenderViewHost* rvh,
                                     IPC::Message* reply_msg,
                                     bool success,
                                     const string16& user_input) {
  if (is_showing_before_unload_dialog_ && !success) {
    // If a beforeunload dialog is canceled, we need to stop the throbber from
    // spinning, since we forced it to start spinning in Navigate.
    DidStopLoading(rvh);
    controller_.DiscardNonCommittedEntries();

    close_start_time_ = base::TimeTicks();
    before_unload_end_time_ = base::TimeTicks();
  }
  is_showing_before_unload_dialog_ = false;
  static_cast<RenderViewHostImpl*>(
      rvh)->JavaScriptDialogClosed(reply_msg, success, user_input);
}

void WebContentsImpl::SetEncoding(const std::string& encoding) {
  encoding_ = GetContentClient()->browser()->
      GetCanonicalEncodingNameByAliasName(encoding);
}

void WebContentsImpl::SaveURL(const GURL& url,
                              const Referrer& referrer,
                              bool is_main_frame) {
  DownloadManager* dlm =
      BrowserContext::GetDownloadManager(GetBrowserContext());
  if (!dlm)
    return;
  int64 post_id = -1;
  if (is_main_frame) {
    const NavigationEntry* entry = controller_.GetActiveEntry();
    if (entry)
      post_id = entry->GetPostID();
  }
  scoped_ptr<DownloadUrlParameters> params(
      DownloadUrlParameters::FromWebContents(this, url));
  params->set_referrer(referrer);
  params->set_post_id(post_id);
  params->set_prefer_cache(true);
  if (post_id >= 0)
    params->set_method("POST");
  params->set_prompt(true);
  dlm->DownloadUrl(params.Pass());
}

void WebContentsImpl::CreateViewAndSetSizeForRVH(RenderViewHost* rvh) {
  RenderWidgetHostView* rwh_view = view_->CreateViewForWidget(rvh);
  // Can be NULL during tests.
  if (rwh_view)
    rwh_view->SetSize(GetView()->GetContainerSize());
}

RenderViewHostImpl* WebContentsImpl::GetRenderViewHostImpl() {
  return static_cast<RenderViewHostImpl*>(GetRenderViewHost());
}

BrowserPluginGuest* WebContentsImpl::GetBrowserPluginGuest() const {
  return browser_plugin_guest_.get();
}

BrowserPluginEmbedder* WebContentsImpl::GetBrowserPluginEmbedder() const {
  return browser_plugin_embedder_.get();
}

BrowserPluginGuestManager*
    WebContentsImpl::GetBrowserPluginGuestManager() const {
  return static_cast<BrowserPluginGuestManager*>(
      GetBrowserContext()->GetUserData(
          browser_plugin::kBrowserPluginGuestManagerKeyName));
}

}  // namespace content
