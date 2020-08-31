// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/hash/hash.h"
#include "base/i18n/character_encoding.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/kill.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/about_url_loader_factory.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/bad_message.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/contacts/contacts_manager_impl.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/download/data_url_blob_reader.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/file_system/file_system_manager_impl.h"
#include "content/browser/file_system/file_system_url_loader_factory.h"
#include "content/browser/frame_host/back_forward_cache_impl.h"
#include "content/browser/frame_host/cookie_utils.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/file_chooser_impl.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/input/input_injector_impl.h"
#include "content/browser/frame_host/ipc_utils.h"
#include "content/browser/frame_host/keep_alive_handle_factory.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/generic_sensor/sensor_provider_proxy_impl.h"
#include "content/browser/geolocation/geolocation_service_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/installedapp/installed_app_provider_impl.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/log_console_message.h"
#include "content/browser/manifest/manifest_manager_host.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_interface_proxy.h"
#include "content/browser/media/webaudio/audio_context_manager_impl.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/net/cross_origin_embedder_policy_reporter.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/portal/portal.h"
#include "content/browser/presentation/presentation_service_impl.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/scoped_active_url.h"
#include "content/browser/screen_enumeration/screen_enumeration_impl.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/sms/sms_service.h"
#include "content/browser/speech/speech_synthesis_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/web_bundle_handle.h"
#include "content/browser/web_package/web_bundle_handle_tracker.h"
#include "content/browser/web_package/web_bundle_navigation_info.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/browser/webauth/webauth_request_security_checker.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_url_loader_factory_internal.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params_mojom_traits.h"
#include "content/common/navigation_params_utils.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/unfreezable_frame_messages.h"
#include "content/common/widget.mojom.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/idle_manager.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/referrer_type_converters.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "device/gamepad/gamepad_monitor.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/base/user_input_monitor.h"
#include "media/learning/common/value.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_constants.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/feature_policy/document_policy_features.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/geolocation/geolocation_service.mojom.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/loader/url_loader_factory_bundle.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/quad_f.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include "content/browser/android/content_url_loader_factory.h"
#include "content/browser/android/java_interfaces_impl.h"
#include "content/browser/frame_host/render_frame_host_android.h"
#include "content/public/browser/android/java_interfaces.h"
#else
#include "content/browser/hid/hid_service.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/serial/serial_service.h"
#endif

#if defined(OS_MACOSX)
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#endif

using base::TimeDelta;

namespace content {

struct RenderFrameHostOrProxy {
  RenderFrameHostImpl* const frame;
  RenderFrameProxyHost* const proxy;

  RenderFrameHostOrProxy(RenderFrameHostImpl* frame,
                         RenderFrameProxyHost* proxy)
      : frame(frame), proxy(proxy) {
    DCHECK(!frame || !proxy)
        << "Both frame and proxy can't be non-null at the same time";
  }

  explicit operator bool() { return frame || proxy; }

  FrameTreeNode* GetFrameTreeNode() {
    if (frame)
      return frame->frame_tree_node();
    if (proxy)
      return proxy->frame_tree_node();
    return nullptr;
  }
};

namespace {

#if defined(OS_ANDROID)
const void* const kRenderFrameHostAndroidKey = &kRenderFrameHostAndroidKey;
#endif  // OS_ANDROID

// The next value to use for the accessibility reset token.
int g_next_accessibility_reset_token = 1;

// Whether to allow injecting javascript into any kind of frame, for Android
// WebView, WebLayer, Fuchsia web.ContextProvider and CastOS content shell.
bool g_allow_injecting_javascript = false;

typedef std::unordered_map<GlobalFrameRoutingId,
                           RenderFrameHostImpl*,
                           GlobalFrameRoutingIdHasher>
    RoutingIDFrameMap;
base::LazyInstance<RoutingIDFrameMap>::DestructorAtExit g_routing_id_frame_map =
    LAZY_INSTANCE_INITIALIZER;

using TokenFrameMap = std::unordered_map<base::UnguessableToken,
                                         RenderFrameHostImpl*,
                                         base::UnguessableTokenHash>;
base::LazyInstance<TokenFrameMap>::Leaky g_token_frame_map =
    LAZY_INSTANCE_INITIALIZER;

// Returns true if |url| & |base_url| represents a WebView loadDataWithBaseUrl
// navigation.
bool IsLoadDataWithBaseURL(const GURL& url, const GURL& base_url) {
  return url.SchemeIs(url::kDataScheme) && !base_url.is_empty();
}

// Returns true if |common_params| represents a WebView loadDataWithBaseUrl
// navigation.
bool IsLoadDataWithBaseURL(const mojom::CommonNavigationParams& common_params) {
  return IsLoadDataWithBaseURL(common_params.url,
                               common_params.base_url_for_data_url);
}

// Returns true if |validated_params| represents a WebView loadDataWithBaseUrl
// navigation.
bool IsLoadDataWithBaseURL(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& validated_params) {
  return IsLoadDataWithBaseURL(validated_params.url, validated_params.base_url);
}

// Ensure that we reset nav_entry_id_ in DidCommitProvisionalLoad if any of
// the validations fail and lead to an early return.  Call disable() once we
// know the commit will be successful.  Resetting nav_entry_id_ avoids acting on
// any UpdateState or UpdateTitle messages after an ignored commit.
class ScopedCommitStateResetter {
 public:
  explicit ScopedCommitStateResetter(RenderFrameHostImpl* render_frame_host)
      : render_frame_host_(render_frame_host), disabled_(false) {}

  ~ScopedCommitStateResetter() {
    if (!disabled_) {
      render_frame_host_->set_nav_entry_id(0);
    }
  }

  void disable() { disabled_ = true; }

 private:
  RenderFrameHostImpl* render_frame_host_;
  bool disabled_;
};

class ActiveURLMessageFilter : public mojo::MessageFilter {
 public:
  explicit ActiveURLMessageFilter(RenderFrameHostImpl* render_frame_host)
      : render_frame_host_(render_frame_host) {}

  ~ActiveURLMessageFilter() override {
    if (debug_url_set_) {
      GetContentClient()->SetActiveURL(GURL(), "");
    }
  }

  // mojo::MessageFilter overrides.
  bool WillDispatch(mojo::Message* message) override {
    debug_url_set_ = true;
    auto* frame_tree_node = render_frame_host_->frame_tree_node();
    GetContentClient()->SetActiveURL(frame_tree_node->current_url(),
                                     frame_tree_node->frame_tree()
                                         ->root()
                                         ->current_origin()
                                         .GetDebugString());
    return true;
  }

  void DidDispatchOrReject(mojo::Message* message, bool accepted) override {
    GetContentClient()->SetActiveURL(GURL(), "");
    debug_url_set_ = false;
  }

 private:
  RenderFrameHostImpl* render_frame_host_;
  bool debug_url_set_ = false;
};

void GrantFileAccess(int child_id,
                     const std::vector<base::FilePath>& file_paths) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  for (const auto& file : file_paths) {
    if (!policy->CanReadFile(child_id, file))
      policy->GrantReadFile(child_id, file);
  }
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
// RemoterFactory that delegates Create() calls to the ContentBrowserClient.
//
// Since Create() could be called at any time, perhaps by a stray task being run
// after a RenderFrameHost has been destroyed, the RemoterFactoryImpl uses the
// process/routing IDs as a weak reference to the RenderFrameHostImpl.
class RemoterFactoryImpl final : public media::mojom::RemoterFactory {
 public:
  RemoterFactoryImpl(int process_id, int routing_id)
      : process_id_(process_id), routing_id_(routing_id) {}

 private:
  void Create(mojo::PendingRemote<media::mojom::RemotingSource> source,
              mojo::PendingReceiver<media::mojom::Remoter> receiver) final {
    if (auto* host = RenderFrameHostImpl::FromID(process_id_, routing_id_)) {
      GetContentClient()->browser()->CreateMediaRemoter(host, std::move(source),
                                                        std::move(receiver));
    }
  }

  const int process_id_;
  const int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RemoterFactoryImpl);
};
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)

using FrameCallback = base::RepeatingCallback<void(RenderFrameHostImpl*)>;
void ForEachFrame(RenderFrameHostImpl* root_frame_host,
                  const FrameCallback& frame_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  FrameTree* frame_tree = root_frame_host->frame_tree_node()->frame_tree();
  DCHECK_EQ(root_frame_host, frame_tree->GetMainFrame());

  for (FrameTreeNode* node : frame_tree->Nodes()) {
    RenderFrameHostImpl* frame_host = node->current_frame_host();
    RenderFrameHostImpl* pending_frame_host =
        node->render_manager()->speculative_frame_host();
    if (frame_host)
      frame_callback.Run(frame_host);
    if (pending_frame_host)
      frame_callback.Run(pending_frame_host);
  }
}

RenderFrameHostOrProxy LookupRenderFrameHostOrProxy(int process_id,
                                                    int routing_id) {
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(process_id, routing_id);
  RenderFrameProxyHost* proxy = nullptr;
  if (!rfh)
    proxy = RenderFrameProxyHost::FromID(process_id, routing_id);
  return RenderFrameHostOrProxy(rfh, proxy);
}

RenderFrameHostOrProxy LookupRenderFrameHostOrProxy(
    int process_id,
    const base::UnguessableToken& frame_token) {
  auto it = g_token_frame_map.Get().find(frame_token);
  RenderFrameHostImpl* rfh = nullptr;
  RenderFrameProxyHost* proxy = nullptr;
  if (it != g_token_frame_map.Get().end()) {
    // The check against |process_id| isn't strictly necessary, but represents
    // an extra level of protection against a renderer trying to force a frame
    // token.
    rfh =
        process_id == it->second->GetProcess()->GetID() ? it->second : nullptr;
  } else {
    proxy = RenderFrameProxyHost::FromFrameToken(process_id, frame_token);
  }
  return RenderFrameHostOrProxy(rfh, proxy);
}

// Takes the lower 31 bits of the metric-name-hash of a Mojo interface |name|.
base::Histogram::Sample HashInterfaceNameToHistogramSample(
    base::StringPiece name) {
  return base::strict_cast<base::Histogram::Sample>(
      static_cast<int32_t>(base::HashMetricName(name) & 0x7fffffffull));
}

// Set crash keys that will help understand the circumstances of a renderer
// kill.  Note that the commit URL is already reported in a crash key, and
// additional keys are logged in RenderProcessHostImpl::ShutdownForBadMessage.
void LogRendererKillCrashKeys(const GURL& site_url) {
  static auto* site_url_key = base::debug::AllocateCrashKeyString(
      "current_site_url", base::debug::CrashKeySize::Size64);
  base::debug::SetCrashKeyString(site_url_key,
                                 site_url.possibly_invalid_spec());
}

void LogCanCommitOriginAndUrlFailureReason(const std::string& failure_reason) {
  static auto* failure_reason_key = base::debug::AllocateCrashKeyString(
      "rfhi_can_commit_failure_reason", base::debug::CrashKeySize::Size64);
  base::debug::SetCrashKeyString(failure_reason_key, failure_reason);
}

url::Origin GetOriginForURLLoaderFactoryUnchecked(
    NavigationRequest* navigation_request) {
  // Return a safe opaque origin when there is no |navigation_request| (e.g.
  // when RFHI::CommitNavigation is called via RFHI::NavigateToInterstitialURL).
  if (!navigation_request)
    return url::Origin();

  // GetOriginForURLLoaderFactory should only be called at the ready-to-commit
  // time, when the RFHI to commit the navigation is already known.
  DCHECK_LE(NavigationRequest::READY_TO_COMMIT, navigation_request->state());
  RenderFrameHostImpl* target_frame = navigation_request->GetRenderFrameHost();
  DCHECK(target_frame);

  // Check if this is loadDataWithBaseUrl (which needs special treatment).
  auto& common_params = navigation_request->common_params();
  if (IsLoadDataWithBaseURL(common_params)) {
    // A (potentially attacker-controlled) renderer process should not be able
    // to use loadDataWithBaseUrl code path to initiate fetches on behalf of a
    // victim origin (fetches controlled by attacker-provided
    // |common_params.url| data: URL in a victim's origin from the
    // attacker-provided |common_params.base_url_for_data_url|).  Browser
    // process should verify that |common_params.base_url_for_data_url| is empty
    // for all renderer-initiated navigations (e.g. see
    // VerifyBeginNavigationCommonParams), but as a defense-in-depth this is
    // also asserted below.
    CHECK(navigation_request->browser_initiated());

    // loadDataWithBaseUrl submits a data: |common_params.url| (which has a
    // opaque origin), but commits that URL as if it came from
    // |common_params.base_url_for_data_url|.  See also
    // https://crbug.com/976253.
    return url::Origin::Create(common_params.base_url_for_data_url);
  }

  // MHTML frames should commit as a opaque origin (and should not be able to
  // make network requests on behalf of the real origin).
  //
  // TODO(lukasza): Cover MHTML main frames here.
  if (navigation_request->IsForMhtmlSubframe())
    return url::Origin();

  // Srcdoc subframes need to inherit their origin from their parent frame.
  if (navigation_request->GetURL().IsAboutSrcdoc()) {
    // Srcdoc navigations in main frames should be blocked before this function
    // is called.  This should guarantee existence of a parent here.
    RenderFrameHostImpl* parent = target_frame->GetParent();
    DCHECK(parent);

    return parent->GetLastCommittedOrigin();
  }

  // In cases not covered above, URLLoaderFactory should be associated with the
  // origin of |common_params.url| and/or |common_params.initiator_origin|.
  return url::Origin::Resolve(
      common_params.url,
      common_params.initiator_origin.value_or(url::Origin()));
}

url::Origin GetOriginForURLLoaderFactory(
    NavigationRequest* navigation_request) {
  url::Origin result =
      GetOriginForURLLoaderFactoryUnchecked(navigation_request);

  // |result| must be an origin that is allowed to be accessed from the process
  // that is the target of this navigation.
  if (navigation_request) {
    int process_id =
        navigation_request->GetRenderFrameHost()->GetProcess()->GetID();
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    CHECK(policy->CanAccessDataForOrigin(process_id, result));
  }

  return result;
}

std::unique_ptr<blink::PendingURLLoaderFactoryBundle> CloneFactoryBundle(
    scoped_refptr<blink::URLLoaderFactoryBundle> bundle) {
  return base::WrapUnique(static_cast<blink::PendingURLLoaderFactoryBundle*>(
      bundle->Clone().release()));
}

// Helper method to download a URL on UI thread.
void StartDownload(
    std::unique_ptr<download::DownloadUrlParameters> parameters,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(parameters->render_process_host_id());
  if (!render_process_host)
    return;

  BrowserContext* browser_context = render_process_host->GetBrowserContext();

  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (blob_url_token) {
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            browser_context, std::move(blob_url_token));
  }

  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(browser_context);
  parameters->set_download_source(download::DownloadSource::FROM_RENDERER);
  download_manager->DownloadUrl(std::move(parameters),
                                std::move(blob_url_loader_factory));
}

// Called on the UI thread when the data URL in the BlobDataHandle
// is read.
void OnDataURLRetrieved(
    std::unique_ptr<download::DownloadUrlParameters> parameters,
    GURL data_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!data_url.is_valid())
    return;
  parameters->set_url(std::move(data_url));
  StartDownload(std::move(parameters), mojo::NullRemote());
}

// TODO(crbug.com/977040): Remove when no longer needed.
const uint32_t kMaxCookieSameSiteDeprecationUrls = 20;

void RecordCrossOriginIsolationMetrics(RenderFrameHostImpl* rfh) {
  ContentBrowserClient* client = GetContentClient()->browser();
  if (rfh->cross_origin_opener_policy().value ==
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin) {
    client->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCrossOriginOpenerPolicySameOrigin);
  }
  if (rfh->cross_origin_opener_policy().value ==
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups) {
    client->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::
                 kCrossOriginOpenerPolicySameOriginAllowPopups);
  }

  if (rfh->cross_origin_embedder_policy().value ==
      network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp) {
    client->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCrossOriginEmbedderPolicyRequireCorp);
  }

  if ((rfh->cross_origin_opener_policy().value ==
       network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin) &&
      (rfh->cross_origin_embedder_policy().value ==
       network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp)) {
    client->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kCoopAndCoepIsolated);
  }
}

// Subframe navigations can optionally have associated Trust Tokens operations
// (https://github.com/wicg/trust-token-api). If the operation's type is
// "redemption" or "signing" (as opposed to "issuance"), the parent's frame
// needs to have the trust-token-redemption Feature Policy feature enabled.
bool ParentNeedsTrustTokenFeaturePolicy(
    const mojom::BeginNavigationParams& begin_params) {
  if (!begin_params.trust_token_params)
    return false;

  switch (begin_params.trust_token_params->type) {
    case network::mojom::TrustTokenOperationType::kRedemption:
    case network::mojom::TrustTokenOperationType::kSigning:
      return true;
    case network::mojom::TrustTokenOperationType::kIssuance:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace

bool CreateNewHostForCrashedFrame() {
  return GetRenderDocumentLevel() >= RenderDocumentLevel::kCrashedFrame;
}

class RenderFrameHostImpl::DroppedInterfaceRequestLogger
    : public blink::mojom::BrowserInterfaceBroker {
 public:
  explicit DroppedInterfaceRequestLogger(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  ~DroppedInterfaceRequestLogger() override {
    UMA_HISTOGRAM_EXACT_LINEAR("RenderFrameHostImpl.DroppedInterfaceRequests",
                               num_dropped_requests_, 20);
  }

 protected:
  // blink::mojom::BrowserInterfaceBroker
  void GetInterface(mojo::GenericPendingReceiver receiver) override {
    ++num_dropped_requests_;
    auto interface_name = receiver.interface_name().value_or("");
    base::UmaHistogramSparse(
        "RenderFrameHostImpl.DroppedInterfaceRequestName",
        HashInterfaceNameToHistogramSample(interface_name));
    DLOG(WARNING)
        << "InterfaceRequest was dropped, the document is no longer active: "
        << interface_name;
  }

 private:
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> receiver_{this};
  int num_dropped_requests_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DroppedInterfaceRequestLogger);
};

struct PendingNavigation {
  mojom::CommonNavigationParamsPtr common_params;
  mojom::BeginNavigationParamsPtr begin_navigation_params;
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client;
  mojo::PendingRemote<blink::mojom::NavigationInitiator> navigation_initiator;

  PendingNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_navigation_params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>
          navigation_initiator);
};

PendingNavigation::PendingNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::BeginNavigationParamsPtr begin_navigation_params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    mojo::PendingRemote<blink::mojom::NavigationInitiator> navigation_initiator)
    : common_params(std::move(common_params)),
      begin_navigation_params(std::move(begin_navigation_params)),
      blob_url_loader_factory(std::move(blob_url_loader_factory)),
      navigation_client(std::move(navigation_client)),
      navigation_initiator(std::move(navigation_initiator)) {}

// static
RenderFrameHost* RenderFrameHost::FromID(GlobalFrameRoutingId id) {
  return RenderFrameHostImpl::FromID(id);
}

// static
RenderFrameHost* RenderFrameHost::FromID(int render_process_id,
                                         int render_frame_id) {
  return RenderFrameHostImpl::FromID(
      GlobalFrameRoutingId(render_process_id, render_frame_id));
}

// static
void RenderFrameHost::AllowInjectingJavaScript() {
  g_allow_injecting_javascript = true;
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(GlobalFrameRoutingId id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  auto it = frames->find(id);
  return it == frames->end() ? NULL : it->second;
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(int render_process_id,
                                                 int render_frame_id) {
  return RenderFrameHostImpl::FromID(
      GlobalFrameRoutingId(render_process_id, render_frame_id));
}
// static
RenderFrameHost* RenderFrameHost::FromAXTreeID(ui::AXTreeID ax_tree_id) {
  return RenderFrameHostImpl::FromAXTreeID(ax_tree_id);
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromAXTreeID(
    ui::AXTreeID ax_tree_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ui::AXTreeIDRegistry::FrameID frame_id =
      ui::AXTreeIDRegistry::GetInstance()->GetFrameID(ax_tree_id);
  return RenderFrameHostImpl::FromID(frame_id.first, frame_id.second);
}

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromOverlayRoutingToken(
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = g_token_frame_map.Get().find(token);
  return it == g_token_frame_map.Get().end() ? nullptr : it->second;
}

// static
void RenderFrameHostImpl::ClearAllPrefetchedSignedExchangeCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  for (auto it : *frames)
    it.second->ClearPrefetchedSignedExchangeCache();
}

RenderFrameHostImpl::RenderFrameHostImpl(
    SiteInstance* site_instance,
    scoped_refptr<RenderViewHostImpl> render_view_host,
    RenderFrameHostDelegate* delegate,
    FrameTree* frame_tree,
    FrameTreeNode* frame_tree_node,
    int32_t routing_id,
    const base::UnguessableToken& frame_token,
    bool renderer_initiated_creation,
    LifecycleState lifecycle_state)
    : render_view_host_(std::move(render_view_host)),
      delegate_(delegate),
      site_instance_(static_cast<SiteInstanceImpl*>(site_instance)),
      process_(site_instance->GetProcess()),
      frame_tree_(frame_tree),
      frame_tree_node_(frame_tree_node),
      parent_(frame_tree_node_->parent()),
      routing_id_(routing_id),
      is_waiting_for_unload_ack_(false),
      render_frame_created_(false),
      is_waiting_for_beforeunload_completion_(false),
      beforeunload_dialog_request_cancels_unload_(false),
      unload_ack_is_for_navigation_(false),
      beforeunload_timeout_delay_(base::TimeDelta::FromMilliseconds(
          RenderViewHostImpl::kUnloadTimeoutMS)),
      was_discarded_(false),
      is_loading_(false),
      nav_entry_id_(0),
      accessibility_reset_token_(0),
      accessibility_reset_count_(0),
      browser_plugin_embedder_ax_tree_id_(ui::AXTreeIDUnknown()),
      no_create_browser_accessibility_manager_for_testing_(false),
      web_ui_type_(WebUI::kNoWebUI),
      has_selection_(false),
      is_audible_(false),
      last_navigation_previews_state_(PREVIEWS_UNSPECIFIED),
      waiting_for_init_(renderer_initiated_creation),
      has_focused_editable_element_(false),
      push_messaging_manager_(
          nullptr,
          base::OnTaskRunnerDeleter(base::CreateSequencedTaskRunner(
              {ServiceWorkerContext::GetCoreThreadId()}))),
      active_sandbox_flags_(network::mojom::WebSandboxFlags::kNone),
      frame_token_(frame_token),
      keep_alive_timeout_(base::TimeDelta::FromSeconds(30)),
      subframe_unload_timeout_(base::TimeDelta::FromMilliseconds(
          RenderViewHostImpl::kUnloadTimeoutMS)),
      commit_callback_interceptor_(nullptr),
      media_device_id_salt_base_(
          BrowserContext::CreateRandomMediaDeviceIDSalt()),
      lifecycle_state_(lifecycle_state) {
  DCHECK(delegate_);
  DCHECK(lifecycle_state_ == LifecycleState::kSpeculative ||
         lifecycle_state_ == LifecycleState::kActive);

  GetProcess()->AddRoute(routing_id_, this);
  g_routing_id_frame_map.Get().emplace(
      GlobalFrameRoutingId(GetProcess()->GetID(), routing_id_), this);
  g_token_frame_map.Get().insert(std::make_pair(frame_token_, this));
  site_instance_->AddObserver(this);
  process_->AddObserver(this);
  GetSiteInstance()->IncrementActiveFrameCount();

  if (parent_) {
    cross_origin_embedder_policy_ = parent_->cross_origin_embedder_policy();

    // New child frames should inherit the nav_entry_id of their parent.
    set_nav_entry_id(parent_->nav_entry_id());
  }

  SetUpMojoIfNeeded();

  unload_event_monitor_timeout_ =
      std::make_unique<TimeoutMonitor>(base::BindRepeating(
          &RenderFrameHostImpl::OnUnloaded, weak_ptr_factory_.GetWeakPtr()));
  beforeunload_timeout_.reset(new TimeoutMonitor(
      base::BindRepeating(&RenderFrameHostImpl::BeforeUnloadTimeout,
                          weak_ptr_factory_.GetWeakPtr())));

  // Local roots are:
  // - main frames; or
  // - subframes in a different SiteInstance from their parent.
  //
  // Local roots require a RenderWidget for input/layout/painting.
  if (!parent_ || IsCrossProcessSubframe()) {
    mojo::PendingRemote<mojom::Widget> widget;
    GetRemoteInterfaces()->GetInterface(
        widget.InitWithNewPipeAndPassReceiver());

    if (!parent_) {
      // For main frames, the RenderWidgetHost is owned by the RenderViewHost.
      // TODO(https://crbug.com/545684): Once RenderViewHostImpl has-a
      // RenderWidgetHostImpl, the main render frame should probably start
      // owning the RenderWidgetHostImpl itself.
      DCHECK(GetLocalRenderWidgetHost());
      DCHECK(!GetLocalRenderWidgetHost()->owned_by_render_frame_host());

      // Make the RenderWidgetHostImpl able to call the mojo Widget interface
      // (implemented by the RenderWidgetImpl).
      GetLocalRenderWidgetHost()->SetWidget(std::move(widget));
    } else {
      // For local child roots, the RenderFrameHost directly creates and owns
      // its RenderWidgetHost.
      int32_t widget_routing_id =
          site_instance->GetProcess()->GetNextRoutingID();
      DCHECK_EQ(nullptr, GetLocalRenderWidgetHost());
      owned_render_widget_host_ = RenderWidgetHostFactory::Create(
          frame_tree_->render_widget_delegate(), GetProcess(),
          widget_routing_id, std::move(widget), /*hidden=*/true);
      owned_render_widget_host_->set_owned_by_render_frame_host(true);
#if defined(OS_ANDROID)
      owned_render_widget_host_->SetForceEnableZoom(
          render_view_host_->GetWebkitPreferences().force_enable_zoom);
#endif  // defined(OS_ANDROID)
    }

    if (is_main_frame())
      GetLocalRenderWidgetHost()->SetIntersectsViewport(true);
    GetLocalRenderWidgetHost()->SetFrameDepth(frame_tree_node_->depth());
    GetLocalRenderWidgetHost()->SetFrameInputHandler(
        frame_input_handler_.get());
    GetLocalRenderWidgetHost()->input_router()->SetFrameTreeNodeId(
        frame_tree_node_->frame_tree_node_id());
  }
  ResetFeaturePolicy();

  ui::AXTreeIDRegistry::GetInstance()->SetFrameIDForAXTreeID(
      ui::AXTreeIDRegistry::FrameID(GetProcess()->GetID(), routing_id_),
      GetAXTreeID());

  // Content-Security-Policy: The CSP source 'self' is usually the origin of the
  // current document, set by SetLastCommittedOrigin(). However, before a new
  // frame commits its first navigation, 'self' should correspond to the origin
  // of the parent (in case of a new iframe) or the opener (in case of a new
  // window). This is necessary to correctly enforce CSP during the initial
  // navigation.
  FrameTreeNode* frame_owner =
      frame_tree_node_->parent() ? frame_tree_node_->parent()->frame_tree_node()
                                 : frame_tree_node_->opener();
  if (frame_owner)
    CSPContext::SetSelf(frame_owner->current_origin());
}

RenderFrameHostImpl::~RenderFrameHostImpl() {
  // When a RenderFrameHostImpl is deleted, it may still contain children. This
  // can happen with the unload timer. It causes a RenderFrameHost to delete
  // itself even if it is still waiting for its children to complete their
  // unload handlers.
  //
  // Observers expect children to be deleted first. Do it now before notifying
  // them.
  ResetChildren();

  // Destroying |navigation_request_| may call into delegates/observers,
  // so we do it early while |this| object is still in a sane state.
  ResetNavigationRequests();

  // Release the WebUI instances before all else as the WebUI may accesses the
  // RenderFrameHost during cleanup.
  ClearWebUI();

  SetLastCommittedSiteUrl(GURL());
  if (last_committed_document_priority_) {
    GetProcess()->UpdateFrameWithPriority(last_committed_document_priority_,
                                          base::nullopt);
  }

  g_token_frame_map.Get().erase(frame_token_);

  site_instance_->RemoveObserver(this);
  process_->RemoveObserver(this);

  const bool was_created = render_frame_created_;
  render_frame_created_ = false;
  if (was_created)
    delegate_->RenderFrameDeleted(this);

  // Ensure that the render process host has been notified that all audio
  // streams from this frame have terminated. This is required to ensure the
  // process host has the correct media stream count, which affects its
  // background priority.
  OnAudibleStateChanged(false);

  // If this was the last active frame in the SiteInstance, the
  // DecrementActiveFrameCount call will trigger the deletion of the
  // SiteInstance's proxies.
  GetSiteInstance()->DecrementActiveFrameCount();

  // Once a RenderFrame is created in the renderer, there are three possible
  // clean-up paths:
  // 1. The RenderFrame can be the main frame. In this case, closing the
  //    associated RenderView will clean up the resources associated with the
  //    main RenderFrame.
  // 2. The RenderFrame can be unloaded. In this case, the browser sends a
  //    UnfreezableFrameMsg_Unload for the RenderFrame to replace itself with a
  //    RenderFrameProxy and release its associated resources.
  //    |lifecycle_state_| is advanced to LifeCycleState::kRunningUnloadHandlers
  //    to track that this IPC is in flight.
  // 3. The RenderFrame can be detached, as part of removing a subtree (due to
  //    navigation, unload, or DOM mutation). In this case, the browser sends
  //    a UnfreezableFrameMsg_Delete for the RenderFrame to detach itself and
  //    release its associated resources. If the subframe contains an unload
  //    handler, |lifecycle_state_| is advanced to
  //    LifeCycleState::kRunningUnloadHandlers to track that the detach is in
  //    progress; otherwise, it is advanced directly to
  //    LifeCycleState::kReadyToBeDeleted.
  //
  // The browser side gives the renderer a small timeout to finish processing
  // unload / detach messages. When the timeout expires, the RFH will be
  // removed regardless of whether or not the renderer acknowledged that it
  // completed the work, to avoid indefinitely leaking browser-side state. To
  // avoid leaks, ~RenderFrameHostImpl still validates that the appropriate
  // cleanup IPC was sent to the renderer, by checking IsPendingDeletion().
  //
  // TODO(dcheng): Due to how frame detach is signalled today, there are some
  // bugs in this area. In particular, subtree detach is reported from the
  // bottom up, so the replicated UnfreezableFrameMsg_Delete messages actually
  // operate on a node-by-node basis rather than detaching an entire subtree at
  // once...
  //
  // Note that this logic is fairly subtle. It needs to include all subframes
  // and all speculative frames, but it should exclude case #1 (a main
  // RenderFrame owned by the RenderView). It can't simply:
  // - check |frame_tree_node_->render_manager()->speculative_frame_host()| for
  //   equality against |this|. The speculative frame host is unset before the
  //   speculative frame host is destroyed, so this condition would never be
  //   matched for a speculative RFH that needs to be destroyed.
  // - check |IsCurrent()|, because the RenderFrames in an InterstitialPageImpl
  //   are never considered current.
  //
  // Directly comparing against |RenderViewHostImpl::GetMainFrame()| still has
  // one additional subtlety though: |GetMainFrame()| can sometimes return a
  // speculative RFH! For subframes, this obviously does not matter: a subframe
  // will always pass the condition |render_view_host_->GetMainFrame() != this|.
  // However, it turns out that a speculative main frame being deleted will
  // *always* pass this condition as well: a speculative RFH being deleted will
  // *always* first be unassociated from its corresponding RFHM. Thus, it
  // follows that |GetMainFrame()| will never return the speculative main frame
  // being deleted, since it must have already been unset.
  if (was_created && render_view_host_->GetMainFrame() != this)
    CHECK(!is_active());

  GetProcess()->RemoveRoute(routing_id_);
  g_routing_id_frame_map.Get().erase(
      GlobalFrameRoutingId(GetProcess()->GetID(), routing_id_));

  // Null out the unload timer; in crash dumps this member will be null only if
  // the dtor has run.  (It may also be null in tests.)
  unload_event_monitor_timeout_.reset();

  for (auto& iter : visual_state_callbacks_)
    std::move(iter.second).Run(false);

  // Delete this before destroying the widget, to guard against reentrancy
  // by in-process screen readers such as JAWS.
  browser_accessibility_manager_.reset();

  // Note: The RenderWidgetHost of the main frame is owned by the RenderViewHost
  // instead. In this case the RenderViewHost is responsible for shutting down
  // its RenderViewHost.
  if (owned_render_widget_host_)
    owned_render_widget_host_->ShutdownAndDestroyWidget(false);

  // This needs to be deleted before |frame_input_handler_| so associated
  // remotes can send messages during shutdown. See crbug.com/1010478 for
  // details.
  render_view_host_.reset();

  // If another frame is waiting for a beforeunload completion callback from
  // this frame, simulate it now.
  RenderFrameHostImpl* beforeunload_initiator = GetBeforeUnloadInitiator();
  if (beforeunload_initiator && beforeunload_initiator != this) {
    base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
    beforeunload_initiator->ProcessBeforeUnloadCompletedFromFrame(
        true /* proceed */, false /* treat_as_final_completion_callback */,
        this, true /* is_frame_being_destroyed */, approx_renderer_start_time,
        base::TimeTicks::Now());
  }

  if (prefetched_signed_exchange_cache_)
    prefetched_signed_exchange_cache_->RecordHistograms();
}

int RenderFrameHostImpl::GetRoutingID() {
  return routing_id_;
}

ui::AXTreeID RenderFrameHostImpl::GetAXTreeID() {
  return ax_tree_id();
}

const base::UnguessableToken& RenderFrameHostImpl::GetTopFrameToken() const {
  const RenderFrameHostImpl* frame = this;
  while (frame->parent_) {
    frame = frame->parent_;
  }
  return frame->frame_token();
}

void RenderFrameHostImpl::AudioContextPlaybackStarted(int audio_context_id) {
  delegate_->AudioContextPlaybackStarted(this, audio_context_id);
}

void RenderFrameHostImpl::AudioContextPlaybackStopped(int audio_context_id) {
  delegate_->AudioContextPlaybackStopped(this, audio_context_id);
}

// The current frame went into the BackForwardCache.
void RenderFrameHostImpl::DidEnterBackForwardCache() {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::EnterBackForwardCache");
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK_EQ(lifecycle_state_, LifecycleState::kActive);
  SetLifecycleState(LifecycleState::kInBackForwardCache);
  // Pages in the back-forward cache are automatically evicted after a certain
  // time.
  if (!GetParent())
    StartBackForwardCacheEvictionTimer();
  for (auto& child : children_)
    child->current_frame_host()->DidEnterBackForwardCache();

  if (service_worker_container_hosts_.empty())
    return;
  std::vector<base::WeakPtr<ServiceWorkerContainerHost>> hosts;
  for (auto host : service_worker_container_hosts_)
    hosts.push_back(host.second);
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          [](std::vector<base::WeakPtr<ServiceWorkerContainerHost>> hosts) {
            for (auto host : hosts) {
              if (host)
                host->OnEnterBackForwardCache();
            }
          },
          std::move(hosts)));
}

// The frame as been restored from the BackForwardCache.
void RenderFrameHostImpl::WillLeaveBackForwardCache() {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::LeaveBackForwardCache");
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK_EQ(lifecycle_state_, LifecycleState::kInBackForwardCache);
  if (back_forward_cache_eviction_timer_.IsRunning())
    back_forward_cache_eviction_timer_.Stop();
  for (auto& child : children_)
    child->current_frame_host()->WillLeaveBackForwardCache();

  if (service_worker_container_hosts_.empty())
    return;
  std::vector<base::WeakPtr<ServiceWorkerContainerHost>> hosts;
  for (auto host : service_worker_container_hosts_)
    hosts.push_back(host.second);
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          [](std::vector<base::WeakPtr<ServiceWorkerContainerHost>> hosts) {
            for (auto host : hosts) {
              if (host)
                host->OnRestoreFromBackForwardCache();
            }
          },
          std::move(hosts)));
}

std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
RenderFrameHostImpl::TakeLastCommitParams() {
  return std::move(last_commit_params_);
}

void RenderFrameHostImpl::StartBackForwardCacheEvictionTimer() {
  DCHECK(IsInBackForwardCache());
  base::TimeDelta evict_after =
      BackForwardCacheImpl::GetTimeToLiveInBackForwardCache();
  NavigationControllerImpl* controller = static_cast<NavigationControllerImpl*>(
      frame_tree_node_->navigator()->GetController());

  back_forward_cache_eviction_timer_.SetTaskRunner(
      controller->GetBackForwardCache().GetTaskRunner());

  back_forward_cache_eviction_timer_.Start(
      FROM_HERE, evict_after,
      base::BindOnce(&RenderFrameHostImpl::EvictFromBackForwardCacheWithReason,
                     weak_ptr_factory_.GetWeakPtr(),
                     BackForwardCacheMetrics::NotRestoredReason::kTimeout));
}

void RenderFrameHostImpl::DisableBackForwardCache(base::StringPiece reason) {
  back_forward_cache_disabled_reasons_.insert(reason.as_string());
  MaybeEvictFromBackForwardCache();
}

void RenderFrameHostImpl::OnGrantedMediaStreamAccess() {
  was_granted_media_access_ = true;
  MaybeEvictFromBackForwardCache();
}

void RenderFrameHostImpl::OnPortalActivated(
    std::unique_ptr<WebContents> predecessor_web_contents,
    blink::TransferableMessage data,
    base::OnceCallback<void(blink::mojom::PortalActivateResult)> callback) {
  mojo::PendingAssociatedRemote<blink::mojom::Portal> pending_portal;
  auto portal_receiver = pending_portal.InitWithNewEndpointAndPassReceiver();
  mojo::PendingAssociatedRemote<blink::mojom::PortalClient> pending_client;
  auto client_receiver = pending_client.InitWithNewEndpointAndPassReceiver();

  auto predecessor =
      std::make_unique<Portal>(this, std::move(predecessor_web_contents));
  predecessor->Bind(std::move(portal_receiver), std::move(pending_client));
  auto it = portals_.insert(std::move(predecessor)).first;

  GetNavigationControl()->OnPortalActivated(
      (*it)->portal_token(), std::move(pending_portal),
      std::move(client_receiver), std::move(data),
      base::BindOnce(
          [](base::OnceCallback<void(blink::mojom::PortalActivateResult)>
                 callback,
             blink::mojom::PortalActivateResult result) {
            switch (result) {
              case blink::mojom::PortalActivateResult::kPredecessorWillUnload:
              case blink::mojom::PortalActivateResult::kPredecessorWasAdopted:
                // These values are acceptable from the renderer.
                break;
              case blink::mojom::PortalActivateResult::
                  kRejectedDueToPredecessorNavigation:
              case blink::mojom::PortalActivateResult::
                  kRejectedDueToPortalNotReady:
              case blink::mojom::PortalActivateResult::
                  kRejectedDueToErrorInPortal:
              case blink::mojom::PortalActivateResult::kDisconnected:
              case blink::mojom::PortalActivateResult::kAbortedDueToBug:
                // The renderer is misbehaving.
                mojo::ReportBadMessage(
                    "Unexpected PortalActivateResult from renderer");
                result = blink::mojom::PortalActivateResult::kAbortedDueToBug;
                break;
            }
            std::move(callback).Run(result);
          },
          std::move(callback)));
}

void RenderFrameHostImpl::OnPortalCreatedForTesting(
    std::unique_ptr<Portal> portal) {
  portals_.insert(std::move(portal));
}

Portal* RenderFrameHostImpl::FindPortalByToken(
    const base::UnguessableToken& portal_token) {
  auto it =
      std::find_if(portals_.begin(), portals_.end(), [&](const auto& portal) {
        return portal->portal_token() == portal_token;
      });
  return it == portals_.end() ? nullptr : it->get();
}

void RenderFrameHostImpl::DestroyPortal(Portal* portal) {
  auto it = portals_.find(portal);
  CHECK(it != portals_.end());
  std::unique_ptr<Portal> owned_portal(std::move(*it));
  portals_.erase(it);
  // |owned_portal| is deleted as it goes out of scope.
}

void RenderFrameHostImpl::ForwardMessageFromHost(
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const base::Optional<url::Origin>& target_origin) {
  // The target origin check needs to be done here in case the frame has
  // navigated after the postMessage call, or if the renderer is compromised and
  // the check done in PortalHost::ReceiveMessage is bypassed.
  if (target_origin) {
    if (target_origin != GetLastCommittedOrigin())
      return;
  }
  GetNavigationControl()->ForwardMessageFromHost(std::move(message),
                                                 source_origin, target_origin);
}

SiteInstanceImpl* RenderFrameHostImpl::GetSiteInstance() {
  return site_instance_.get();
}

RenderProcessHost* RenderFrameHostImpl::GetProcess() {
  return process_;
}

RenderFrameHostImpl* RenderFrameHostImpl::GetParent() {
  return parent_;
}

std::vector<RenderFrameHost*> RenderFrameHostImpl::GetFramesInSubtree() {
  std::vector<RenderFrameHost*> frame_hosts;
  for (FrameTreeNode* node : frame_tree_->SubtreeNodes(frame_tree_node()))
    frame_hosts.push_back(node->current_frame_host());
  return frame_hosts;
}

bool RenderFrameHostImpl::IsDescendantOf(RenderFrameHost* ancestor) {
  if (!ancestor || !static_cast<RenderFrameHostImpl*>(ancestor)->child_count())
    return false;

  for (RenderFrameHostImpl* current = GetParent(); current;
       current = current->GetParent()) {
    if (current == ancestor)
      return true;
  }
  return false;
}

int RenderFrameHostImpl::GetFrameTreeNodeId() {
  return frame_tree_node_->frame_tree_node_id();
}

base::UnguessableToken RenderFrameHostImpl::GetDevToolsFrameToken() {
  return frame_tree_node_->devtools_frame_token();
}

base::Optional<base::UnguessableToken>
RenderFrameHostImpl::GetEmbeddingToken() {
  return embedding_token_;
}

const std::string& RenderFrameHostImpl::GetFrameName() {
  return frame_tree_node_->frame_name();
}

bool RenderFrameHostImpl::IsFrameDisplayNone() {
  return frame_tree_node()->frame_owner_properties().is_display_none;
}

const base::Optional<gfx::Size>& RenderFrameHostImpl::GetFrameSize() {
  return frame_size_;
}

size_t RenderFrameHostImpl::GetFrameDepth() {
  return frame_tree_node()->depth();
}

bool RenderFrameHostImpl::IsCrossProcessSubframe() {
  if (!parent_)
    return false;
  return GetSiteInstance() != parent_->GetSiteInstance();
}

const GURL& RenderFrameHostImpl::GetLastCommittedURL() {
  return last_committed_url_;
}

const url::Origin& RenderFrameHostImpl::GetLastCommittedOrigin() {
  return last_committed_origin_;
}

const net::NetworkIsolationKey& RenderFrameHostImpl::GetNetworkIsolationKey() {
  DCHECK(!isolation_info_.IsEmpty());
  return isolation_info_.network_isolation_key();
}

const net::IsolationInfo&
RenderFrameHostImpl::GetIsolationInfoForSubresources() {
  DCHECK(!isolation_info_.IsEmpty());
  return isolation_info_;
}

void RenderFrameHostImpl::GetCanonicalUrlForSharing(
    mojom::Frame::GetCanonicalUrlForSharingCallback callback) {
  // TODO(https://crbug.com/859110): Remove this once frame_ can no longer be
  // null.
  if (IsRenderFrameLive()) {
    frame_->GetCanonicalUrlForSharing(std::move(callback));
  } else {
    std::move(callback).Run(base::nullopt);
  }
}

mojo::Remote<blink::mojom::PauseSubresourceLoadingHandle>
RenderFrameHostImpl::PauseSubresourceLoading() {
  DCHECK(frame_);
  mojo::Remote<blink::mojom::PauseSubresourceLoadingHandle>
      pause_subresource_loading_handle;
  GetRemoteInterfaces()->GetInterface(
      pause_subresource_loading_handle.BindNewPipeAndPassReceiver());

  return pause_subresource_loading_handle;
}

void RenderFrameHostImpl::ExecuteMediaPlayerActionAtLocation(
    const gfx::Point& location,
    const blink::mojom::MediaPlayerAction& action) {
  auto media_player_action = blink::mojom::MediaPlayerAction::New();
  media_player_action->type = action.type;
  media_player_action->enable = action.enable;
  gfx::PointF point_in_view = GetView()->TransformRootPointToViewCoordSpace(
      gfx::PointF(location.x(), location.y()));
  GetAssociatedLocalFrame()->MediaPlayerActionAt(
      gfx::Point(point_in_view.x(), point_in_view.y()),
      std::move(media_player_action));
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_remote;
  if (coep_reporter_) {
    coep_reporter_->Clone(
        coep_reporter_remote.InitWithNewPipeAndPassReceiver());
  }

  // We use the last committed Origin and ClientSecurityState. If the caller
  // wanted a factory associated to a navigation about to commit, the params
  // generated won't be correct. There is no good way of fixing this before
  // RenderDocumentHost (ie swapping RenderFrameHost on each navigation).
  return CreateNetworkServiceDefaultFactoryInternal(
      CreateURLLoaderFactoryParamsForMainWorld(
          last_committed_origin_,
          mojo::Clone(last_committed_client_security_state_),
          std::move(coep_reporter_remote)),
      std::move(default_factory_receiver));
}

void RenderFrameHostImpl::MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
    base::flat_set<url::Origin> isolated_world_origins,
    bool push_to_renderer_now) {
  size_t old_size =
      isolated_worlds_requiring_separate_url_loader_factory_.size();
  isolated_worlds_requiring_separate_url_loader_factory_.insert(
      isolated_world_origins.begin(), isolated_world_origins.end());
  size_t new_size =
      isolated_worlds_requiring_separate_url_loader_factory_.size();
  bool insertion_took_place = (old_size != new_size);

  // Push the updated set of factories to the renderer, but only if
  // 1) the caller requested an immediate push (e.g. for content scripts
  //    injected programmatically chrome.tabs.executeCode, but not for content
  //    scripts declared in the manifest - the difference is that the latter
  //    happen at a commit and the factories can just be send in the commit
  //    IPC).
  // 2) an insertion actually took place / the factories have been modified
  // 3) a commit has taken place before (i.e. the frame has received a factory
  //    bundle before).
  if (push_to_renderer_now && insertion_took_place &&
      has_committed_any_navigation_) {
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories =
            std::make_unique<blink::PendingURLLoaderFactoryBundle>();
    subresource_loader_factories->pending_isolated_world_factories() =
        CreateURLLoaderFactoriesForIsolatedWorlds(
            GetExpectedMainWorldOriginForUrlLoaderFactory(),
            isolated_world_origins,
            mojo::Clone(last_committed_client_security_state_));
    GetNavigationControl()->UpdateSubresourceLoaderFactories(
        std::move(subresource_loader_factories));
  }
}

bool RenderFrameHostImpl::IsSandboxed(network::mojom::WebSandboxFlags flags) {
  if (base::FeatureList::IsEnabled(features::kFeaturePolicyForSandbox)) {
    blink::mojom::FeaturePolicyFeature feature =
        blink::FeaturePolicy::FeatureForSandboxFlag(flags);
    if (feature != blink::mojom::FeaturePolicyFeature::kNotFound)
      return !IsFeatureEnabled(feature);
  }
  return static_cast<int>(active_sandbox_flags_) & static_cast<int>(flags);
}

blink::PendingURLLoaderFactoryBundle::OriginMap
RenderFrameHostImpl::CreateURLLoaderFactoriesForIsolatedWorlds(
    const url::Origin& main_world_origin,
    const base::flat_set<url::Origin>& isolated_world_origins,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  WebPreferences preferences = GetRenderViewHost()->GetWebkitPreferences();

  blink::PendingURLLoaderFactoryBundle::OriginMap result;
  for (const url::Origin& isolated_world_origin : isolated_world_origins) {
    network::mojom::URLLoaderFactoryParamsPtr factory_params =
        URLLoaderFactoryParamsHelper::CreateForIsolatedWorld(
            this, isolated_world_origin, main_world_origin,
            mojo::Clone(client_security_state));

    mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
    CreateNetworkServiceDefaultFactoryAndObserve(
        std::move(factory_params),
        factory_remote.InitWithNewPipeAndPassReceiver());
    result[isolated_world_origin] = std::move(factory_remote);
  }
  return result;
}

gfx::NativeView RenderFrameHostImpl::GetNativeView() {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (!view)
    return nullptr;
  return view->GetNativeView();
}

void RenderFrameHostImpl::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  AddMessageToConsoleImpl(level, message, false /* discard_duplicates */);
}

void RenderFrameHostImpl::ExecuteJavaScript(const base::string16& javascript,
                                            JavaScriptResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(CanExecuteJavaScript());

  const bool wants_result = !callback.is_null();
  GetNavigationControl()->JavaScriptExecuteRequest(javascript, wants_result,
                                                   std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld(
    const base::string16& javascript,
    JavaScriptResultCallback callback,
    int32_t world_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(world_id, ISOLATED_WORLD_ID_GLOBAL);
  DCHECK_LE(world_id, ISOLATED_WORLD_ID_MAX);

  const bool wants_result = !callback.is_null();
  GetNavigationControl()->JavaScriptExecuteRequestInIsolatedWorld(
      javascript, wants_result, world_id, std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptForTests(
    const base::string16& javascript,
    JavaScriptResultCallback callback,
    int32_t world_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const bool has_user_gesture = false;
  const bool wants_result = !callback.is_null();
  GetNavigationControl()->JavaScriptExecuteRequestForTests(
      javascript, wants_result, has_user_gesture, world_id,
      std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptWithUserGestureForTests(
    const base::string16& javascript,
    int32_t world_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(mustaq): The render-to-browser state update caused by the below
  // JavaScriptExecuteRequestsForTests call is redundant with this update. We
  // should determine if the redundancy can be removed.
  frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation);

  const bool has_user_gesture = true;
  GetNavigationControl()->JavaScriptExecuteRequestForTests(
      javascript, false, has_user_gesture, world_id, base::NullCallback());
}

void RenderFrameHostImpl::CopyImageAt(int x, int y) {
  gfx::PointF point_in_view =
      GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(x, y));
  GetAssociatedLocalFrame()->CopyImageAt(
      gfx::Point(point_in_view.x(), point_in_view.y()));
}

void RenderFrameHostImpl::SaveImageAt(int x, int y) {
  gfx::PointF point_in_view =
      GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(x, y));
  GetAssociatedLocalFrame()->SaveImageAt(
      gfx::Point(point_in_view.x(), point_in_view.y()));
}

RenderViewHost* RenderFrameHostImpl::GetRenderViewHost() {
  return render_view_host_.get();
}

service_manager::InterfaceProvider* RenderFrameHostImpl::GetRemoteInterfaces() {
  return remote_interfaces_.get();
}

blink::AssociatedInterfaceProvider*
RenderFrameHostImpl::GetRemoteAssociatedInterfaces() {
  if (!remote_associated_interfaces_) {
    mojo::AssociatedRemote<blink::mojom::AssociatedInterfaceProvider>
        remote_interfaces;
    IPC::ChannelProxy* channel = GetProcess()->GetChannel();
    if (channel) {
      RenderProcessHostImpl* process =
          static_cast<RenderProcessHostImpl*>(GetProcess());
      process->GetRemoteRouteProvider()->GetRoute(
          GetRoutingID(), remote_interfaces.BindNewEndpointAndPassReceiver());
    } else {
      // The channel may not be initialized in some tests environments. In this
      // case we set up a dummy interface provider.
      ignore_result(remote_interfaces
                        .BindNewEndpointAndPassDedicatedReceiverForTesting());
    }
    remote_associated_interfaces_ =
        std::make_unique<blink::AssociatedInterfaceProvider>(
            remote_interfaces.Unbind());
  }
  return remote_associated_interfaces_.get();
}

PageVisibilityState RenderFrameHostImpl::GetVisibilityState() {
  // Works around the crashes seen in https://crbug.com/501863, where the
  // active WebContents from a browser iterator may contain a render frame
  // detached from the frame tree. This tries to find a RenderWidgetHost
  // attached to an ancestor frame, and defaults to visibility hidden if
  // it fails.
  // TODO(yfriedman, peter): Ideally this would never be called on an
  // unattached frame and we could omit this check. See
  // https://crbug.com/615867.
  RenderFrameHostImpl* frame = this;
  while (frame) {
    if (frame->GetLocalRenderWidgetHost())
      break;
    frame = frame->GetParent();
  }
  if (!frame)
    return PageVisibilityState::kHidden;

  PageVisibilityState visibility_state = GetRenderWidgetHost()->is_hidden()
                                             ? PageVisibilityState::kHidden
                                             : PageVisibilityState::kVisible;
  GetContentClient()->browser()->OverridePageVisibilityState(this,
                                                             &visibility_state);
  return visibility_state;
}

bool RenderFrameHostImpl::Send(IPC::Message* message) {
  return GetProcess()->Send(message);
}

bool RenderFrameHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // Only process messages if the RenderFrame is alive.
  if (!render_frame_created_)
    return false;

  // Crash reports triggered by IPC messages for this frame should be associated
  // with its URL.
  ScopedActiveURL scoped_active_url(this);

  if (delegate_->OnMessageReceived(this, msg))
    return true;

  RenderFrameProxyHost* proxy =
      frame_tree_node_->render_manager()->GetProxyToParent();
  if (proxy && proxy->cross_process_frame_connector() &&
      proxy->cross_process_frame_connector()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameHostImpl, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateState, OnUpdateState)
    IPC_MESSAGE_HANDLER(FrameHostMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Unload_ACK, OnUnloadACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ContextMenu, OnContextMenu)
    IPC_MESSAGE_HANDLER(FrameHostMsg_VisualStateResponse, OnVisualStateResponse)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeOpener, OnDidChangeOpener)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeFramePolicy,
                        OnDidChangeFramePolicy)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SelectionChanged, OnSelectionChanged)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FrameDidCallFocus, OnFrameDidCallFocus)
  IPC_END_MESSAGE_MAP()

  // No further actions here, since we may have been deleted.
  return handled;
}

void RenderFrameHostImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  ContentBrowserClient* browser_client = GetContentClient()->browser();
  if (!associated_registry_->TryBindInterface(interface_name, &handle) &&
      !browser_client->BindAssociatedReceiverFromFrame(this, interface_name,
                                                       &handle)) {
    delegate_->OnAssociatedInterfaceRequest(this, interface_name,
                                            std::move(handle));
  }
}

void RenderFrameHostImpl::AccessibilityPerformAction(
    const ui::AXActionData& action_data) {
  if (!is_active() || !render_accessibility_)
    return;

  // Use the dedicated HitTest method so that we can handle its response via
  // mojo callback once it's been handled in the renderer process.
  if (action_data.action == ax::mojom::Action::kHitTest) {
    render_accessibility_->HitTest(
        action_data,
        base::BindOnce(&RenderFrameHostImpl::RequestAXHitTestCallback,
                       weak_ptr_factory_.GetWeakPtr(), action_data.request_id));
    return;
  }

  render_accessibility_->PerformAction(action_data);
}

bool RenderFrameHostImpl::AccessibilityViewHasFocus() {
  if (!is_active())
    return false;

  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return view->HasFocus();
  return false;
}

void RenderFrameHostImpl::AccessibilityViewSetFocus() {
  if (!is_active())
    return;

  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    view->Focus();
}

gfx::Rect RenderFrameHostImpl::AccessibilityGetViewBounds() {
  if (!is_active())
    return gfx::Rect();

  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return view->GetViewBounds();
  return gfx::Rect();
}

float RenderFrameHostImpl::AccessibilityGetDeviceScaleFactor() {
  if (!is_active())
    return 1.0f;

  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return GetScaleFactorForView(view);
  return 1.0f;
}

void RenderFrameHostImpl::AccessibilityReset() {
  if (!render_accessibility_)
    return;

  accessibility_reset_token_ = g_next_accessibility_reset_token++;
  render_accessibility_->Reset(accessibility_reset_token_);
}

void RenderFrameHostImpl::AccessibilityFatalError() {
  browser_accessibility_manager_.reset(nullptr);
  if (accessibility_reset_token_ || !render_accessibility_)
    return;

  accessibility_reset_count_++;
  if (accessibility_reset_count_ >= kMaxAccessibilityResets) {
    render_accessibility_->FatalError();
  } else {
    AccessibilityReset();
  }
}

gfx::AcceleratedWidget
RenderFrameHostImpl::AccessibilityGetAcceleratedWidget() {
  // Only the main frame's current frame host is connected to the native
  // widget tree for accessibility, so return null if this is queried on
  // any other frame.
  if (!is_active() || !is_main_frame() || !IsCurrent())
    return gfx::kNullAcceleratedWidget;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetAcceleratedWidget();
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
RenderFrameHostImpl::AccessibilityGetNativeViewAccessible() {
  if (!is_active())
    return nullptr;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetNativeViewAccessible();
  return nullptr;
}

gfx::NativeViewAccessible
RenderFrameHostImpl::AccessibilityGetNativeViewAccessibleForWindow() {
  if (!is_active())
    return nullptr;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetNativeViewAccessibleForWindow();
  return nullptr;
}

WebContents* RenderFrameHostImpl::AccessibilityWebContents() {
  if (!is_active())
    return nullptr;
  return delegate()->GetAsWebContents();
}

bool RenderFrameHostImpl::AccessibilityIsMainFrame() {
  if (!is_active())
    return false;
  return frame_tree_node()->IsMainFrame();
}

void RenderFrameHostImpl::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  if (base::FeatureList::IsEnabled(features::kCrashReporting))
    MaybeGenerateCrashReport(info.status, info.exit_code);

  // When a frame's process dies, its RenderFrame no longer exists, which means
  // that its child frames must be cleaned up as well.
  ResetChildren();

  // Reset state for the current RenderFrameHost once the FrameTreeNode has been
  // reset.
  SetRenderFrameCreated(false);
  InvalidateMojoConnection();
  document_scoped_interface_provider_receiver_.reset();
  broker_receiver_.reset();
  SetLastCommittedUrl(GURL());
  web_bundle_handle_.reset();

  must_be_replaced_ = CreateNewHostForCrashedFrame();
  has_committed_any_navigation_ = false;

#if defined(OS_ANDROID)
  // Execute any pending Samsung smart clip callbacks.
  for (base::IDMap<std::unique_ptr<ExtractSmartClipDataCallback>>::iterator
           iter(&smart_clip_callbacks_);
       !iter.IsAtEnd(); iter.Advance()) {
    std::move(*iter.GetCurrentValue())
        .Run(base::string16(), base::string16(), gfx::Rect());
  }
  smart_clip_callbacks_.Clear();
#endif  // defined(OS_ANDROID)

  visual_state_callbacks_.clear();

  // Ensure that future remote interface requests are associated with the new
  // process's channel.
  remote_associated_interfaces_.reset();

  // Any termination disablers in content loaded by the new process will
  // be sent again.
  has_before_unload_handler_ = false;
  has_unload_handler_ = false;

  if (IsPendingDeletion()) {
    // If the process has died, we don't need to wait for the ACK. Complete the
    // deletion immediately.
    SetLifecycleState(LifecycleState::kReadyToBeDeleted);
    DCHECK(children_.empty());
    PendingDeletionCheckCompleted();
    // |this| is deleted. Don't add any more code at this point in the function.
    return;
  }

  // If this was the current pending or speculative RFH dying, cancel and
  // destroy it.
  frame_tree_node_->render_manager()->CancelPendingIfNecessary(this);

  // Note: don't add any more code at this point in the function because
  // |this| may be deleted. Any additional cleanup should happen before
  // the last block of code here.
}

void RenderFrameHostImpl::RenderProcessGone(
    SiteInstanceImpl* site_instance,
    const ChildProcessTerminationInfo& info) {
  DCHECK_EQ(site_instance_.get(), site_instance);

  if (IsInBackForwardCache()) {
    EvictFromBackForwardCacheWithReason(
        info.status == base::TERMINATION_STATUS_PROCESS_CRASHED
            ? BackForwardCacheMetrics::NotRestoredReason::
                  kRendererProcessCrashed
            : BackForwardCacheMetrics::NotRestoredReason::
                  kRendererProcessKilled);
    return;
  }

  if (owned_render_widget_host_)
    owned_render_widget_host_->RendererExited();

  // The renderer process is gone, so this frame can no longer be loading.
  if (navigation_request())
    navigation_request()->set_net_error(net::ERR_ABORTED);
  ResetNavigationRequests();
  ResetLoadingState();

  // Any future UpdateState or UpdateTitle messages from this or a recreated
  // process should be ignored until the next commit.
  set_nav_entry_id(0);

  OnAudibleStateChanged(false);
}

void RenderFrameHostImpl::ReportContentSecurityPolicyViolation(
    network::mojom::CSPViolationPtr violation_params) {
  GetAssociatedLocalFrame()->ReportContentSecurityPolicyViolation(
      std::move(violation_params));
}

void RenderFrameHostImpl::SanitizeDataForUseInCspViolation(
    bool is_redirect,
    network::mojom::CSPDirectiveName directive,
    GURL* blocked_url,
    network::mojom::SourceLocation* source_location) const {
  DCHECK(blocked_url);
  DCHECK(source_location);
  GURL source_location_url(source_location->url);

  // The main goal of this is to avoid leaking information between potentially
  // separate renderers, in the event of one of them being compromised.
  // See https://crbug.com/633306.
  bool sanitize_blocked_url = true;
  bool sanitize_source_location = true;

  // There is no need to sanitize data when it is same-origin with the current
  // url of the renderer.
  if (url::Origin::Create(*blocked_url)
          .IsSameOriginWith(last_committed_origin_))
    sanitize_blocked_url = false;
  if (url::Origin::Create(source_location_url)
          .IsSameOriginWith(last_committed_origin_))
    sanitize_source_location = false;

  // When a renderer tries to do a form submission, it already knows the url of
  // the blocked url, except when it is redirected.
  if (!is_redirect && directive == network::mojom::CSPDirectiveName::FormAction)
    sanitize_blocked_url = false;

  if (sanitize_blocked_url)
    *blocked_url = blocked_url->GetOrigin();
  if (sanitize_source_location) {
    source_location->url = source_location_url.GetOrigin().spec();
    source_location->line = 0u;
    source_location->column = 0u;
  }
}

void RenderFrameHostImpl::PerformAction(const ui::AXActionData& data) {
  AccessibilityPerformAction(data);
}

bool RenderFrameHostImpl::RequiresPerformActionPointInPixels() const {
  return true;
}

bool RenderFrameHostImpl::SchemeShouldBypassCSP(
    const base::StringPiece& scheme) {
  // Blink uses its SchemeRegistry to check if a scheme should be bypassed.
  // It can't be used on the browser process. It is used for two things:
  // 1) Bypassing the "chrome-extension" scheme when chrome is built with the
  //    extensions support.
  // 2) Bypassing arbitrary scheme for testing purpose only in blink and in V8.
  // TODO(arthursonzogni): url::GetBypassingCSPScheme() is used instead of the
  // blink::SchemeRegistry. It contains 1) but not 2).
  const auto& bypassing_schemes = url::GetCSPBypassingSchemes();
  return base::Contains(bypassing_schemes, scheme);
}

mojom::FrameInputHandler* RenderFrameHostImpl::GetFrameInputHandler() {
  if (!frame_input_handler_)
    return nullptr;
  return frame_input_handler_.get();
}

bool RenderFrameHostImpl::CreateRenderFrame(int previous_routing_id,
                                            int opener_routing_id,
                                            int parent_routing_id,
                                            int previous_sibling_routing_id) {
  TRACE_EVENT0("navigation", "RenderFrameHostImpl::CreateRenderFrame");
  DCHECK(!IsRenderFrameLive()) << "Creating frame twice";

  // The process may (if we're sharing a process with another host that already
  // initialized it) or may not (we have our own process or the old process
  // crashed) have been initialized. Calling Init multiple times will be
  // ignored, so this is safe.
  if (!GetProcess()->Init())
    return false;

  DCHECK(GetProcess()->IsInitializedAndNotDead());

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      interface_provider;
  BindInterfaceProviderReceiver(
      interface_provider.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker;
  BindBrowserInterfaceBrokerReceiver(
      browser_interface_broker.InitWithNewPipeAndPassReceiver());

  mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
  params->interface_bundle = mojom::DocumentScopedInterfaceBundle::New(
      std::move(interface_provider), std::move(browser_interface_broker));

  params->routing_id = routing_id_;
  params->previous_routing_id = previous_routing_id;
  params->opener_routing_id = opener_routing_id;
  params->parent_routing_id = parent_routing_id;
  params->previous_sibling_routing_id = previous_sibling_routing_id;
  params->replication_state = frame_tree_node()->current_replication_state();
  params->frame_token = frame_token_;
  params->devtools_frame_token = frame_tree_node()->devtools_frame_token();

  // Normally, the replication state contains effective frame policy, excluding
  // sandbox flags and feature policy attributes that were updated but have not
  // taken effect. However, a new RenderFrame should use the pending frame
  // policy, since it is being created as part of the navigation that will
  // commit it. (I.e., the RenderFrame needs to know the policy to use when
  // initializing the new document once it commits).
  params->replication_state.frame_policy =
      frame_tree_node()->pending_frame_policy();

  // If we switched BrowsingInstances because of the COOP header, we should
  // clear the frame name. This below informs the renderer at frame creation.
  NavigationRequest* navigation_request =
      frame_tree_node()->navigation_request();
  if (navigation_request &&
      navigation_request->require_coop_browsing_instance_swap()) {
    params->replication_state.name = "";
    // "COOP swaps" only affect main frames, that have an empty unique name.
    DCHECK(params->replication_state.unique_name.empty());
  }

  params->frame_owner_properties =
      frame_tree_node()->frame_owner_properties().Clone();

  params->has_committed_real_load =
      frame_tree_node()->has_committed_real_load();

  // The RenderWidgetHost takes ownership of its view. It is tied to the
  // lifetime of the current RenderProcessHost for this RenderFrameHost.
  // TODO(avi): This will need to change to initialize a
  // RenderWidgetHostViewAura for the main frame once RenderViewHostImpl has-a
  // RenderWidgetHostImpl. https://crbug.com/545684
  if (owned_render_widget_host_) {
    DCHECK(parent_);
    DCHECK_NE(parent_routing_id, MSG_ROUTING_NONE);
    RenderWidgetHostView* rwhv =
        RenderWidgetHostViewChildFrame::Create(owned_render_widget_host_.get());
    // The child frame should be created hidden.
    DCHECK(!rwhv->IsShowing());
  }

  if (auto* rwh = GetLocalRenderWidgetHost()) {
    params->widget_params = mojom::CreateFrameWidgetParams::New();
    params->widget_params->routing_id =
        GetLocalRenderWidgetHost()->GetRoutingID();
    params->widget_params->visual_properties =
        GetLocalRenderWidgetHost()->GetInitialVisualProperties();

    std::tie(params->widget_params->widget_host,
             params->widget_params->widget) = rwh->BindNewWidgetInterfaces();
    std::tie(params->widget_params->frame_widget_host,
             params->widget_params->frame_widget) =
        rwh->BindNewFrameWidgetInterfaces();
  }

  // TODO(https://crbug.com/1006814): Remove this.
  if (params->previous_routing_id == MSG_ROUTING_NONE &&
      params->parent_routing_id == MSG_ROUTING_NONE) {
    base::debug::DumpWithoutCrashing();
    NOTREACHED();
  }
  GetProcess()->GetRendererInterface()->CreateFrame(std::move(params));

  if (previous_routing_id != MSG_ROUTING_NONE) {
    RenderFrameProxyHost* proxy = RenderFrameProxyHost::FromID(
        GetProcess()->GetID(), previous_routing_id);
    // We have also created a RenderFrameProxy in CreateFrame above, so
    // remember that.
    //
    // RenderDocument: |proxy| can be null, when |previous_routing_id| refers to
    // a RenderFrameHost instead of a RenderFrameProxy.
    if (proxy)
      proxy->SetRenderFrameProxyCreated(true);
  }

  // The renderer now has a RenderFrame for this RenderFrameHost.  Note that
  // this path is only used for out-of-process iframes.  Main frame RenderFrames
  // are created with their RenderView, and same-site iframes are created at the
  // time of OnCreateChildFrame.
  SetRenderFrameCreated(true);

  return true;
}

void RenderFrameHostImpl::DeleteRenderFrame(FrameDeleteIntention intent) {
  if (IsPendingDeletion())
    return;

  if (render_frame_created_) {
    Send(new UnfreezableFrameMsg_Delete(routing_id_, intent));

    if (!frame_tree_node_->IsMainFrame() && IsCurrent()) {
      // If this subframe has an unload handler (and isn't speculative), ensure
      // that it has a chance to execute by delaying process cleanup. This will
      // prevent the process from shutting down immediately in the case where
      // this is the last active frame in the process. See
      // https://crbug.com/852204.
      if (has_unload_handler()) {
        RenderProcessHostImpl* process =
            static_cast<RenderProcessHostImpl*>(GetProcess());
        process->DelayProcessShutdownForUnload(subframe_unload_timeout_);
      }

      // If the subframe takes too long to unload, force its removal from the
      // tree.  See https://crbug.com/950625.
      subframe_unload_timer_.Start(FROM_HERE, subframe_unload_timeout_, this,
                                   &RenderFrameHostImpl::OnUnloadTimeout);
    }
  }

  LifecycleState lifecycle_state = has_unload_handler()
                                       ? LifecycleState::kRunningUnloadHandlers
                                       : LifecycleState::kReadyToBeDeleted;
  SetLifecycleState(lifecycle_state);
}

void RenderFrameHostImpl::SetRenderFrameCreated(bool created) {
  // We should not create new RenderFrames while our delegate is being destroyed
  // (e.g., via a WebContentsObserver during WebContents shutdown).  This seems
  // to have caused crashes in https://crbug.com/717650.
  if (created)
    CHECK(!delegate_->IsBeingDestroyed());

  bool was_created = render_frame_created_;
  render_frame_created_ = created;

  // If the current status is different than the new status, the delegate
  // needs to be notified.
  if (created != was_created) {
    if (created) {
      SetUpMojoIfNeeded();
      delegate_->RenderFrameCreated(this);
    } else {
      delegate_->RenderFrameDeleted(this);
    }
  }
  // TODO(http://crbug.com/1014212): Change to DCHECK.
  if (created)
    CHECK(frame_);

  if (created && GetLocalRenderWidgetHost()) {
    mojo::PendingRemote<mojom::Widget> widget;
    GetRemoteInterfaces()->GetInterface(
        widget.InitWithNewPipeAndPassReceiver());
    GetLocalRenderWidgetHost()->SetWidget(std::move(widget));
    GetLocalRenderWidgetHost()->SetFrameInputHandler(
        frame_input_handler_.get());
    GetLocalRenderWidgetHost()->input_router()->SetFrameTreeNodeId(
        frame_tree_node_->frame_tree_node_id());
    mojo::Remote<viz::mojom::InputTargetClient> input_target_client;
    remote_interfaces_->GetInterface(
        input_target_client.BindNewPipeAndPassReceiver());
    input_target_client_ = input_target_client.get();
    GetLocalRenderWidgetHost()->SetInputTargetClient(
        std::move(input_target_client));
    GetLocalRenderWidgetHost()->InitForFrame();
  }

  if (enabled_bindings_ && created) {
    if (!frame_bindings_control_)
      GetRemoteAssociatedInterfaces()->GetInterface(&frame_bindings_control_);
    frame_bindings_control_->AllowBindings(enabled_bindings_);
  }

  // Clear all the user data associated with this RenderFrameHost in case if
  // the renderer crashes and the RenderFrameHost still stays alive.
  if (!created)
    document_associated_data_.ClearAllUserData();
}

void RenderFrameHostImpl::SwapIn() {
  GetNavigationControl()->SwapIn();
}

void RenderFrameHostImpl::Init() {
  ResumeBlockedRequestsForFrame();
  if (!waiting_for_init_)
    return;

  waiting_for_init_ = false;
  if (pending_navigate_) {
    frame_tree_node()->navigator()->OnBeginNavigation(
        frame_tree_node(), std::move(pending_navigate_->common_params),
        std::move(pending_navigate_->begin_navigation_params),
        std::move(pending_navigate_->blob_url_loader_factory),
        std::move(pending_navigate_->navigation_client),
        std::move(pending_navigate_->navigation_initiator),
        EnsurePrefetchedSignedExchangeCache(),
        MaybeCreateWebBundleHandleTracker());
    pending_navigate_.reset();
  }
}

void RenderFrameHostImpl::OnAudibleStateChanged(bool is_audible) {
  if (is_audible_ == is_audible)
    return;
  if (is_audible)
    GetProcess()->OnMediaStreamAdded();
  else
    GetProcess()->OnMediaStreamRemoved();
  is_audible_ = is_audible;
}

void RenderFrameHostImpl::DidAddMessageToConsole(
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  if (delegate_->DidAddMessageToConsole(log_level, message, line_no,
                                        source_id)) {
    return;
  }

  // Pass through log severity only on builtin components pages to limit console
  // spew.
  const bool is_builtin_component =
      HasWebUIScheme(delegate_->GetMainFrameLastCommittedURL()) ||
      GetContentClient()->browser()->IsBuiltinComponent(
          GetProcess()->GetBrowserContext(), GetLastCommittedOrigin());
  const bool is_off_the_record =
      GetSiteInstance()->GetBrowserContext()->IsOffTheRecord();

  LogConsoleMessage(log_level, message, line_no, is_builtin_component,
                    is_off_the_record, source_id);
}

void RenderFrameHostImpl::OnCreateChildFrame(
    int new_routing_id,
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
        new_interface_provider_provider_receiver,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker_receiver,
    blink::mojom::TreeScopeType scope,
    const std::string& frame_name,
    const std::string& frame_unique_name,
    bool is_created_by_script,
    const base::UnguessableToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const blink::FramePolicy& frame_policy,
    const blink::mojom::FrameOwnerProperties& frame_owner_properties,
    const blink::mojom::FrameOwnerElementType owner_type) {
  // TODO(lukasza): Call ReceivedBadMessage when |frame_unique_name| is empty.
  DCHECK(!frame_unique_name.empty());
  DCHECK(new_interface_provider_provider_receiver.is_valid());
  DCHECK(browser_interface_broker_receiver.is_valid());
  if (owner_type == blink::mojom::FrameOwnerElementType::kNone) {
    // Any child frame must have a HTMLFrameOwnerElement in its parent document
    // and therefore the corresponding type of kNone (specific to main frames)
    // is invalid.
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CHILD_FRAME_NEEDS_OWNER_ELEMENT_TYPE);
  }

  // The RenderFrame corresponding to this host sent an IPC message to create a
  // child, but by the time we get here, it's possible for the host to have been
  // swapped out, or for its process to have disconnected (maybe due to browser
  // shutdown). Ignore such messages.
  if (!is_active() || !IsCurrent() || !render_frame_created_)
    return;

  // |new_routing_id|, |new_interface_provider_provider_receiver|,
  // |browser_interface_broker_receiver| and |devtools_frame_token| were
  // generated on the browser's IO thread and not taken from the renderer
  // process.
  frame_tree_->AddFrame(this, GetProcess()->GetID(), new_routing_id,
                        std::move(new_interface_provider_provider_receiver),
                        std::move(browser_interface_broker_receiver), scope,
                        frame_name, frame_unique_name, is_created_by_script,
                        frame_token, devtools_frame_token, frame_policy,
                        frame_owner_properties, was_discarded_, owner_type);
}

void RenderFrameHostImpl::DidNavigate(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document_navigation) {
  // Keep track of the last committed URL and origin in the RenderFrameHost
  // itself.  These allow GetLastCommittedURL and GetLastCommittedOrigin to
  // stay correct even if the render_frame_host later becomes pending deletion.
  // The URL is set regardless of whether it's for a net error or not.
  frame_tree_node_->SetCurrentURL(params.url);
  SetLastCommittedOrigin(params.origin);

  // Separately, update the frame's last successful URL except for net error
  // pages, since those do not end up in the correct process after transfers
  // (see https://crbug.com/560511).  Instead, the next cross-process navigation
  // or transfer should decide whether to swap as if the net error had not
  // occurred.
  // TODO(creis): Remove this block and always set the URL.
  // See https://crbug.com/588314.
  if (!params.url_is_unreachable)
    last_successful_url_ = params.url;

  // After setting the last committed origin, reset the feature policy and
  // sandbox flags in the RenderFrameHost to a blank policy based on the parent
  // frame.
  if (!is_same_document_navigation) {
    ResetFeaturePolicy();
    active_sandbox_flags_ = frame_tree_node()->active_sandbox_flags();
    document_policy_ = blink::DocumentPolicy::CreateWithHeaderPolicy({});
  }

  // Reset the salt so that media device IDs are reset after the new navigation
  // if necessary.
  media_device_id_salt_base_ = BrowserContext::CreateRandomMediaDeviceIDSalt();
}

void RenderFrameHostImpl::SetLastCommittedOrigin(const url::Origin& origin) {
  last_committed_origin_ = origin;
  CSPContext::SetSelf(origin);
}

void RenderFrameHostImpl::SetLastCommittedOriginForTesting(
    const url::Origin& origin) {
  SetLastCommittedOrigin(origin);
}

const url::Origin& RenderFrameHostImpl::ComputeTopFrameOrigin(
    const url::Origin& frame_origin) const {
  if (frame_tree_node_->IsMainFrame()) {
    return frame_origin;
  }

  DCHECK(parent_);
  // It's important to go through parent_ rather than via
  // frame_free_->root() here in case we're in process of being deleted, as the
  // latter might point to what our ancestor is being replaced with rather than
  // the actual ancestor.
  RenderFrameHostImpl* host = parent_;
  while (host->parent_) {
    host = host->parent_;
  }
  return host->GetLastCommittedOrigin();
}

net::IsolationInfo RenderFrameHostImpl::ComputeIsolationInfoForNavigation(
    const GURL& destination) const {
  net::IsolationInfo::RedirectMode redirect_mode =
      frame_tree_node_->IsMainFrame()
          ? net::IsolationInfo::RedirectMode::kUpdateTopFrame
          : net::IsolationInfo::RedirectMode::kUpdateFrameOnly;
  return ComputeIsolationInfoInternal(url::Origin::Create(destination),
                                      redirect_mode);
}

net::SiteForCookies RenderFrameHostImpl::ComputeSiteForCookies() {
  return isolation_info_.site_for_cookies();
}

net::IsolationInfo RenderFrameHostImpl::ComputeIsolationInfoInternal(
    const url::Origin& frame_origin,
    net::IsolationInfo::RedirectMode redirect_mode) const {
  url::Origin top_frame_origin = ComputeTopFrameOrigin(frame_origin);

  net::SiteForCookies candidate_site_for_cookies =
      net::SiteForCookies::FromOrigin(top_frame_origin);

  if (GetContentClient()
          ->browser()
          ->ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
              top_frame_origin.scheme(),
              GURL::SchemeIsCryptographic(frame_origin.scheme()))) {
    return net::IsolationInfo::Create(redirect_mode, top_frame_origin,
                                      frame_origin, candidate_site_for_cookies);
  }

  // Make sure all ancestors have origins consistent with the candidate
  // SiteForCookies of the main document. Otherwise, SameSite cookies may not be
  // used. For frame requests, it's OK to skip checking the frame itself since
  // each request will be validated against |site_for_cookies| anyway.
  for (const RenderFrameHostImpl* rfh = this->parent_; rfh;
       rfh = rfh->parent_) {
    if (!candidate_site_for_cookies.IsEquivalent(
            net::SiteForCookies::FromOrigin(rfh->last_committed_origin_))) {
      return net::IsolationInfo::Create(redirect_mode, top_frame_origin,
                                        frame_origin, net::SiteForCookies());
    }
    candidate_site_for_cookies.MarkIfCrossScheme(rfh->last_committed_origin_);
  }

  // If |redirect_mode| is kUpdateNothing, then IsolationInfo is being computed
  // for subresource requests. In that case, also need to check the
  // SiteForCookies against the frame origin.
  if (redirect_mode == net::IsolationInfo::RedirectMode::kUpdateNothing &&
      !candidate_site_for_cookies.IsEquivalent(
          net::SiteForCookies::FromOrigin(frame_origin))) {
    return net::IsolationInfo::Create(redirect_mode, top_frame_origin,
                                      frame_origin, net::SiteForCookies());
  }

  return net::IsolationInfo::Create(redirect_mode, top_frame_origin,
                                    frame_origin, candidate_site_for_cookies);
}

void RenderFrameHostImpl::SetOriginAndIsolationInfoOfNewFrame(
    const url::Origin& new_frame_creator) {
  // This method should only be called for *new* frames, that haven't committed
  // a navigation yet.
  DCHECK(!has_committed_any_navigation_);
  DCHECK(GetLastCommittedOrigin().opaque());
  DCHECK(isolation_info_.IsEmpty());

  // Calculate and set |new_frame_origin|.
  bool new_frame_should_be_sandboxed =
      network::mojom::WebSandboxFlags::kOrigin ==
      (frame_tree_node()->active_sandbox_flags() &
       network::mojom::WebSandboxFlags::kOrigin);
  url::Origin new_frame_origin = new_frame_should_be_sandboxed
                                     ? new_frame_creator.DeriveNewOpaqueOrigin()
                                     : new_frame_creator;
  isolation_info_ = ComputeIsolationInfoInternal(
      new_frame_origin, net::IsolationInfo::RedirectMode::kUpdateNothing);
  SetLastCommittedOrigin(new_frame_origin);
}

FrameTreeNode* RenderFrameHostImpl::AddChild(
    std::unique_ptr<FrameTreeNode> child,
    int process_id,
    int frame_routing_id,
    const base::UnguessableToken& frame_token) {
  // Child frame must always be created in the same process as the parent.
  CHECK_EQ(process_id, GetProcess()->GetID());

  // Initialize the RenderFrameHost for the new node.  We always create child
  // frames in the same SiteInstance as the current frame, and they can swap to
  // a different one if they navigate away.
  child->render_manager()->InitChild(GetSiteInstance(), frame_routing_id,
                                     frame_token);

  // Other renderer processes in this BrowsingInstance may need to find out
  // about the new frame.  Create a proxy for the child frame in all
  // SiteInstances that have a proxy for the frame's parent, since all frames
  // in a frame tree should have the same set of proxies.
  frame_tree_node_->render_manager()->CreateProxiesForChildFrame(child.get());

  // When the child is added, it hasn't committed any navigation yet - its
  // initial empty document should inherit the origin of its parent (the origin
  // may change after the first commit). See also https://crbug.com/932067.
  child->current_frame_host()->SetOriginAndIsolationInfoOfNewFrame(
      GetLastCommittedOrigin());

  children_.push_back(std::move(child));
  return children_.back().get();
}

void RenderFrameHostImpl::RemoveChild(FrameTreeNode* child) {
  for (auto iter = children_.begin(); iter != children_.end(); ++iter) {
    if (iter->get() == child) {
      // Subtle: we need to make sure the node is gone from the tree before
      // observers are notified of its deletion.
      std::unique_ptr<FrameTreeNode> node_to_delete(std::move(*iter));
      children_.erase(iter);
      node_to_delete->current_frame_host()->DeleteRenderFrame(
          FrameDeleteIntention::kNotMainFrame);
      // Speculative RenderFrameHosts are deleted by the FrameTreeNode's
      // RenderFrameHostManager's destructor. RenderFrameProxyHosts send
      // UnfreezableFrameMsg_Delete automatically in the destructor.
      // TODO(dcheng): This is horribly confusing. Refactor this logic so it's
      // more understandable.
      node_to_delete.reset();
      PendingDeletionCheckCompleted();
      return;
    }
  }
}

void RenderFrameHostImpl::ResetChildren() {
  // Remove child nodes from the tree, then delete them. This destruction
  // operation will notify observers. See https://crbug.com/612450 for
  // explanation why we don't just call the std::vector::clear method.
  std::vector<std::unique_ptr<FrameTreeNode>> children;
  children.swap(children_);
  // TODO(dcheng): Ideally, this would be done by messaging all the proxies of
  // this RenderFrameHostImpl to detach the current frame's children, rather
  // than messaging each child's current frame host...
  for (auto& child : children)
    child->current_frame_host()->DeleteRenderFrame(
        FrameDeleteIntention::kNotMainFrame);
}

void RenderFrameHostImpl::SetLastCommittedUrl(const GURL& url) {
  last_committed_url_ = url;
}

void RenderFrameHostImpl::UpdateRenderProcessHostFramePriorities() {
  const auto new_committed_document_priority =
      delegate_->IsFrameLowPriority(this)
          ? RenderProcessHostImpl::FramePriority::kLow
          : RenderProcessHostImpl::FramePriority::kNormal;
  if (last_committed_document_priority_ != new_committed_document_priority) {
    GetProcess()->UpdateFrameWithPriority(last_committed_document_priority_,
                                          new_committed_document_priority);
    last_committed_document_priority_ = new_committed_document_priority;
  }
}

void RenderFrameHostImpl::OnDetach() {
  if (!parent_) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_DETACH_MAIN_FRAME);
    return;
  }

  // A frame is removed while replacing this document with the new one. When it
  // happens, delete the frame and both the new and old documents. Unload
  // handlers aren't guaranteed to run here.
  if (is_waiting_for_unload_ack_) {
    parent_->RemoveChild(frame_tree_node_);
    return;
  }

  if (IsPendingDeletion()) {
    // The frame is pending deletion. FrameHostMsg_Detach is used to confirm
    // its unload handlers ran. Note that it is possible for a frame to already
    // be in kReadyToBeDeleted. This happens when this RenderFrameHost is
    // pending deletion and is waiting on one of its children to run its unload
    // handler. While running it, it can request its parent to detach itself.
    // See test: SitePerProcessBrowserTest.PartialUnloadHandler.
    if (lifecycle_state_ != LifecycleState::kReadyToBeDeleted)
      SetLifecycleState(LifecycleState::kReadyToBeDeleted);
    PendingDeletionCheckCompleted();  // Can delete |this|.
    return;
  }

  // This frame is being removed by the renderer, and it has already executed
  // its unload handler.
  SetLifecycleState(LifecycleState::kReadyToBeDeleted);

  // Before completing the removal, we still need to wait for all of its
  // descendant frames to execute unload handlers. Start executing those
  // handlers now.
  StartPendingDeletionOnSubtree();
  frame_tree_node_->frame_tree()->FrameUnloading(frame_tree_node_);

  // Some children with no unload handler may be eligible for immediate
  // deletion. Cut the dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();  // Can delete |this|.
}

void RenderFrameHostImpl::DidFailLoadWithError(const GURL& url,
                                               int32_t error_code) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::DidFailLoadWithError",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "error", error_code);

  GURL validated_url(url);
  GetProcess()->FilterURL(false, &validated_url);

  frame_tree_node_->navigator()->DidFailLoadWithError(this, validated_url,
                                                      error_code);
}

void RenderFrameHostImpl::DidFocusFrame() {
  if (!is_active())
    return;

  // We need to handle receiving this IPC from a frame that is inside a portal
  // despite there being a renderer side check (see Document::IsFocusAllowed).
  // This is because the IPC to notify a page that it is inside a portal (see
  // WebContentsImpl::NotifyInsidePortal) may race with portal activation, and
  // we may run into a situation where a frame inside a portal doesn't know it's
  // inside a portal yet and allows focus.
  if (InsidePortal())
    return;

  delegate_->SetFocusedFrame(frame_tree_node_, GetSiteInstance());
}

void RenderFrameHostImpl::DidAddContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyPtr> policies) {
  TRACE_EVENT1("navigation",
               "RenderFrameHostImpl::OnDidAddContentSecurityPolicies",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  std::vector<network::mojom::ContentSecurityPolicyHeaderPtr> headers;
  for (auto& policy : policies) {
    headers.push_back(policy->header.Clone());
    AddContentSecurityPolicy(std::move(policy));
  }
  frame_tree_node()->AddContentSecurityPolicies(std::move(headers));
}

void RenderFrameHostImpl::OnOpenURL(const FrameHostMsg_OpenURL_Params& params) {
  // Verify and unpack IPC payload.
  GURL validated_url;
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (!VerifyOpenURLParams(GetSiteInstance(), params, &validated_url,
                           &blob_url_loader_factory)) {
    return;
  }

  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OpenURL", "url",
               validated_url.possibly_invalid_spec());

  frame_tree_node_->navigator()->RequestOpenURL(
      this, validated_url,
      GlobalFrameRoutingId(GetProcess()->GetID(), params.initiator_routing_id),
      params.initiator_origin, params.post_body, params.extra_headers,
      params.referrer, params.disposition, params.should_replace_current_entry,
      params.user_gesture, params.triggering_event_info, params.href_translate,
      std::move(blob_url_loader_factory), params.impression);
}

void RenderFrameHostImpl::CancelInitialHistoryLoad() {
  // A Javascript navigation interrupted the initial history load.  Check if an
  // initial subframe cross-process navigation needs to be canceled as a result.
  // TODO(creis, clamy): Cancel any cross-process navigation.
  NOTIMPLEMENTED();
}

void RenderFrameHostImpl::DidChangeActiveSchedulerTrackedFeatures(
    uint64_t features_mask) {
  renderer_reported_scheduler_tracked_features_ = features_mask;

  MaybeEvictFromBackForwardCache();
}

void RenderFrameHostImpl::OnSchedulerTrackedFeatureUsed(
    blink::scheduler::WebSchedulerTrackedFeature feature) {
  browser_reported_scheduler_tracked_features_ |=
      1 << static_cast<uint64_t>(feature);

  MaybeEvictFromBackForwardCache();
}

bool RenderFrameHostImpl::IsFrozen() {
  return frame_lifecycle_state_ != blink::mojom::FrameLifecycleState::kRunning;
}

void RenderFrameHostImpl::DidCommitProvisionalLoad(
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  if (MaybeInterceptCommitCallback(nullptr, params.get(), &interface_params)) {
    DidCommitNavigation(std::move(navigation_request_), std::move(params),
                        std::move(interface_params));
  }
}

void RenderFrameHostImpl::DidCommitBackForwardCacheNavigation(
    NavigationRequest* committing_navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params) {
  auto request = navigation_requests_.find(committing_navigation_request);
  CHECK(request != navigation_requests_.end());

  std::unique_ptr<NavigationRequest> owned_request = std::move(request->second);
  navigation_requests_.erase(committing_navigation_request);

  // During a normal (uncached) navigation, is_loading_ is set to true in
  // CommitNavigation(). When navigating to a document in the BackForwardCache,
  // CommitNavigation() is never called, so we have to set is_loading_ to true
  // ourselves.
  //
  // If is_start_loading_ is set to false, DidCommitNavigationInternal will
  // re-fire the DidStartLoading event, which we don't want since it has already
  // been fired.
  is_loading_ = true;

  DidCommitNavigationInternal(std::move(owned_request), std::move(params),
                              /*is_same_document_navigation=*/false);

  // The page is already loaded since it came from the cache, so fire the stop
  // loading event.
  OnDidStopLoading();
}

void RenderFrameHostImpl::SetEmbeddingToken(
    const base::Optional<base::UnguessableToken>& embedding_token) {
  embedding_token_ = embedding_token;
  if (!embedding_token_.has_value())
    return;

  // Only non-null tokens are propagated to the parent document. The token is
  // automatically reset in the parent document when the child document is
  // navigated cross-origin.
  RenderFrameProxyHost* proxy_to_parent =
      frame_tree_node()->render_manager()->GetProxyToParent();
  DCHECK(proxy_to_parent);
  proxy_to_parent->GetAssociatedRemoteFrame()->SetEmbeddingToken(
      embedding_token_.value());
}

void RenderFrameHostImpl::DidCommitPerNavigationMojoInterfaceNavigation(
    NavigationRequest* committing_navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  DCHECK(committing_navigation_request);
  committing_navigation_request->IgnoreCommitInterfaceDisconnection();
  if (!MaybeInterceptCommitCallback(committing_navigation_request, params.get(),
                                    &interface_params)) {
    return;
  }

  auto request = navigation_requests_.find(committing_navigation_request);

  // The committing request should be in the map of NavigationRequests for
  // this RenderFrameHost.
  CHECK(request != navigation_requests_.end());

  std::unique_ptr<NavigationRequest> owned_request = std::move(request->second);
  navigation_requests_.erase(committing_navigation_request);
  DidCommitNavigation(std::move(owned_request), std::move(params),
                      std::move(interface_params));
}

void RenderFrameHostImpl::DidCommitSameDocumentNavigation(
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params) {
  ScopedActiveURL scoped_active_url(
      params->url, frame_tree_node()->frame_tree()->root()->current_origin());
  ScopedCommitStateResetter commit_state_resetter(this);

  // When the frame is pending deletion, the browser is waiting for it to unload
  // properly. In the meantime, because of race conditions, it might tries to
  // commit a same-document navigation before unloading. Similarly to what is
  // done with cross-document navigations, such navigation are ignored. The
  // browser already committed to destroying this RenderFrameHost.
  // See https://crbug.com/805705 and https://crbug.com/930132.
  // TODO(ahemery): Investigate to see if this can be removed when the
  // NavigationClient interface is implemented.
  if (!is_active())
    return;

  TRACE_EVENT2("navigation",
               "RenderFrameHostImpl::DidCommitSameDocumentNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(), "url",
               params->url.possibly_invalid_spec());

  // Check if the navigation matches a stored same-document NavigationRequest.
  // In that case it is browser-initiated.
  bool is_browser_initiated =
      same_document_navigation_request_ &&
      (same_document_navigation_request_->commit_params().navigation_token ==
       params->navigation_token);
  if (!DidCommitNavigationInternal(
          is_browser_initiated ? std::move(same_document_navigation_request_)
                               : nullptr,
          std::move(params), true /* is_same_document_navigation*/)) {
    return;
  }

  // Since we didn't early return, it's safe to keep the commit state.
  commit_state_resetter.disable();
}

void RenderFrameHostImpl::OnUpdateState(const PageState& state) {
  // TODO(creis): Verify the state's ISN matches the last committed FNE.

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(state)) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return;
  }

  delegate_->UpdateStateForFrame(this, state);
}

RenderWidgetHostImpl* RenderFrameHostImpl::GetRenderWidgetHost() {
  RenderFrameHostImpl* frame = this;
  while (frame) {
    if (frame->GetLocalRenderWidgetHost())
      return frame->GetLocalRenderWidgetHost();
    frame = frame->GetParent();
  }

  NOTREACHED();
  return nullptr;
}

RenderWidgetHostView* RenderFrameHostImpl::GetView() {
  return GetRenderWidgetHost()->GetView();
}

GlobalFrameRoutingId RenderFrameHostImpl::GetGlobalFrameRoutingId() {
  return GlobalFrameRoutingId(GetProcess()->GetID(), GetRoutingID());
}

bool RenderFrameHostImpl::HasPendingCommitNavigation() const {
  return navigation_request_ || same_document_navigation_request_ ||
         !navigation_requests_.empty();
}

void RenderFrameHostImpl::ResetNavigationRequests() {
  navigation_request_.reset();
  same_document_navigation_request_.reset();
  navigation_requests_.clear();
}

void RenderFrameHostImpl::SetNavigationRequest(
    std::unique_ptr<NavigationRequest> navigation_request) {
  DCHECK(navigation_request);
  if (NavigationTypeUtils::IsSameDocument(
          navigation_request->common_params().navigation_type)) {
    same_document_navigation_request_ = std::move(navigation_request);
    return;
  }
  navigation_requests_[navigation_request.get()] =
      std::move(navigation_request);
}

void RenderFrameHostImpl::Unload(RenderFrameProxyHost* proxy, bool is_loading) {
  // The end of this event is in OnUnloadACK when the RenderFrame has completed
  // the operation and sends back an IPC message.
  TRACE_EVENT_ASYNC_BEGIN1("navigation", "RenderFrameHostImpl::Unload", this,
                           "frame_tree_node",
                           frame_tree_node_->frame_tree_node_id());

  // If this RenderFrameHost is already pending deletion, it must have already
  // gone through this, therefore just return.
  if (IsPendingDeletion()) {
    NOTREACHED() << "RFH should be in default state when calling Unload.";
    return;
  }

  if (unload_event_monitor_timeout_ && !do_not_delete_for_testing_) {
    unload_event_monitor_timeout_->Start(base::TimeDelta::FromMilliseconds(
        RenderViewHostImpl::kUnloadTimeoutMS));
  }

  // TODO(nasko): If the frame is not live, the RFH should just be deleted by
  // simulating the receipt of unload ack.
  is_waiting_for_unload_ack_ = true;

  if (proxy) {
    SetLifecycleState(LifecycleState::kRunningUnloadHandlers);
    if (IsRenderFrameLive()) {
      Send(new UnfreezableFrameMsg_Unload(
          routing_id_, proxy->GetRoutingID(), is_loading,
          proxy->frame_tree_node()->current_replication_state(),
          proxy->GetFrameToken()));
      // Remember that a RenderFrameProxy was created as part of processing the
      // Unload message above.
      proxy->SetRenderFrameProxyCreated(true);
    }
  } else {
    // RenderDocument: After a local<->local swap, this function is called with
    // a null |proxy|.
    CHECK(CreateNewHostForSameSiteSubframe());

    // The unload handlers already ran for this document during the
    // local<->local swap. Hence, there is no need to send
    // UnfreezableFrameMsg_Unload here. It can be marked at completed.
    SetLifecycleState(LifecycleState::kReadyToBeDeleted);
  }

  if (web_ui())
    web_ui()->RenderFrameHostUnloading();

  web_bluetooth_services_.clear();
#if !defined(OS_ANDROID)
  serial_service_.reset();
#endif

  StartPendingDeletionOnSubtree();
  // Some children with no unload handler may be eligible for deletion. Cut the
  // dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();
  // |this| is potentially deleted. Do not add code after this.
}

void RenderFrameHostImpl::DetachFromProxy() {
  if (IsPendingDeletion())
    return;

  // Start pending deletion on this frame and its children.
  DeleteRenderFrame(FrameDeleteIntention::kNotMainFrame);
  StartPendingDeletionOnSubtree();
  frame_tree_node_->frame_tree()->FrameUnloading(frame_tree_node_);

  // Some children with no unload handler may be eligible for immediate
  // deletion. Cut the dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();  // May delete |this|.
}

void RenderFrameHostImpl::ProcessBeforeUnloadCompleted(
    bool proceed,
    bool treat_as_final_completion_callback,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  TRACE_EVENT_ASYNC_END1("navigation", "RenderFrameHostImpl BeforeUnload", this,
                         "FrameTreeNode id",
                         frame_tree_node_->frame_tree_node_id());
  // If this renderer navigated while the beforeunload request was in flight, we
  // may have cleared this state in DidCommitProvisionalLoad, in which case we
  // can ignore this message.
  // However renderer might also be swapped out but we still want to proceed
  // with navigation, otherwise it would block future navigations. This can
  // happen when pending cross-site navigation is canceled by a second one just
  // before DidCommitProvisionalLoad while current RVH is waiting for commit
  // but second navigation is started from the beginning.
  RenderFrameHostImpl* initiator = GetBeforeUnloadInitiator();
  if (!initiator)
    return;

  // Continue processing the ACK in the frame that triggered beforeunload in
  // this frame.  This could be either this frame itself or an ancestor frame.
  initiator->ProcessBeforeUnloadCompletedFromFrame(
      proceed, treat_as_final_completion_callback, this,
      false /* is_frame_being_destroyed */, renderer_before_unload_start_time,
      renderer_before_unload_end_time);
}

RenderFrameHostImpl* RenderFrameHostImpl::GetBeforeUnloadInitiator() {
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->is_waiting_for_beforeunload_completion_)
      return frame;
  }
  return nullptr;
}

void RenderFrameHostImpl::ProcessBeforeUnloadCompletedFromFrame(
    bool proceed,
    bool treat_as_final_completion_callback,
    RenderFrameHostImpl* frame,
    bool is_frame_being_destroyed,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  // Check if we need to wait for more beforeunload completion callbacks. If
  // |proceed| is false, we know the navigation or window close will be aborted,
  // so we don't need to wait for beforeunload completion callbacks from any
  // other frames. |treat_as_final_completion_callback| also indicates that we
  // shouldn't wait for any other ACKs (e.g., when a beforeunload timeout
  // fires).
  if (!proceed || treat_as_final_completion_callback) {
    beforeunload_pending_replies_.clear();
  } else {
    beforeunload_pending_replies_.erase(frame);
    if (!beforeunload_pending_replies_.empty())
      return;
  }

  DCHECK(!send_before_unload_start_time_.is_null());

  // Sets a default value for before_unload_end_time so that the browser
  // survives a hacked renderer.
  base::TimeTicks before_unload_end_time = renderer_before_unload_end_time;
  if (!renderer_before_unload_start_time.is_null() &&
      !renderer_before_unload_end_time.is_null()) {
    base::TimeTicks before_unload_completed_time = base::TimeTicks::Now();

    if (!base::TimeTicks::IsConsistentAcrossProcesses()) {
      // TimeTicks is not consistent across processes and we are passing
      // TimeTicks across process boundaries so we need to compensate for any
      // skew between the processes. Here we are converting the renderer's
      // notion of before_unload_end_time to TimeTicks in the browser process.
      // See comments in inter_process_time_ticks_converter.h for more.
      InterProcessTimeTicksConverter converter(
          LocalTimeTicks::FromTimeTicks(send_before_unload_start_time_),
          LocalTimeTicks::FromTimeTicks(before_unload_completed_time),
          RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time),
          RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
      LocalTimeTicks browser_before_unload_end_time =
          converter.ToLocalTimeTicks(
              RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
      before_unload_end_time = browser_before_unload_end_time.ToTimeTicks();
    }

    base::TimeDelta on_before_unload_overhead_time =
        (before_unload_completed_time - send_before_unload_start_time_) -
        (renderer_before_unload_end_time - renderer_before_unload_start_time);
    UMA_HISTOGRAM_TIMES("Navigation.OnBeforeUnloadOverheadTime",
                        on_before_unload_overhead_time);

    frame_tree_node_->navigator()->LogBeforeUnloadTime(
        renderer_before_unload_start_time, renderer_before_unload_end_time);
  }

  // Resets beforeunload waiting state.
  is_waiting_for_beforeunload_completion_ = false;
  has_shown_beforeunload_dialog_ = false;
  if (beforeunload_timeout_)
    beforeunload_timeout_->Stop();
  send_before_unload_start_time_ = base::TimeTicks();

  // If the ACK is for a navigation, send it to the Navigator to have the
  // current navigation stop/proceed. Otherwise, send it to the
  // RenderFrameHostManager which handles closing.
  if (unload_ack_is_for_navigation_) {
    frame_tree_node_->navigator()->BeforeUnloadCompleted(
        frame_tree_node_, proceed, before_unload_end_time);
  } else {
    // We could reach this from a subframe destructor for |frame| while we're
    // in the middle of closing the current tab.  In that case, dispatch the
    // ACK to prevent re-entrancy and a potential nested attempt to free the
    // current frame.  See https://crbug.com/866382.
    base::OnceClosure task = base::BindOnce(
        [](base::WeakPtr<RenderFrameHostImpl> self,
           const base::TimeTicks& before_unload_end_time, bool proceed) {
          if (!self)
            return;
          self->frame_tree_node()->render_manager()->BeforeUnloadCompleted(
              proceed, before_unload_end_time);
        },
        weak_ptr_factory_.GetWeakPtr(), before_unload_end_time, proceed);
    if (is_frame_being_destroyed) {
      DCHECK(proceed);
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(task));
    } else {
      std::move(task).Run();
    }
  }

  // If canceled, notify the delegate to cancel its pending navigation entry.
  // This is usually redundant with the dialog closure code in WebContentsImpl's
  // OnDialogClosed, but there may be some cases that Blink returns !proceed
  // without showing the dialog. We also update the address bar here to be safe.
  if (!proceed)
    delegate_->DidCancelLoading();
}

bool RenderFrameHostImpl::IsWaitingForUnloadACK() const {
  return render_view_host_->is_waiting_for_page_close_completion_ ||
         is_waiting_for_unload_ack_;
}

bool RenderFrameHostImpl::BeforeUnloadTimedOut() const {
  return beforeunload_timeout_ &&
         (send_before_unload_start_time_ != base::TimeTicks()) &&
         (base::TimeTicks::Now() - send_before_unload_start_time_) >
             beforeunload_timeout_delay_;
}

void RenderFrameHostImpl::OnUnloadACK() {
  if (frame_tree_node_->render_manager()->is_attaching_inner_delegate()) {
    // This RFH was unloaded while attaching an inner delegate. The RFH
    // will stay around but it will no longer be associated with a RenderFrame.
    SetRenderFrameCreated(false);
    return;
  }

  // Ignore spurious unload ack.
  if (!is_waiting_for_unload_ack_)
    return;

  // Ignore OnUnloadACK if the RenderFrameHost should be left in pending
  // deletion state.
  if (do_not_delete_for_testing_)
    return;

  DCHECK_EQ(LifecycleState::kRunningUnloadHandlers, lifecycle_state_);
  SetLifecycleState(LifecycleState::kReadyToBeDeleted);
  PendingDeletionCheckCompleted();  // Can delete |this|.
}

void RenderFrameHostImpl::OnUnloaded() {
  DCHECK(is_waiting_for_unload_ack_);

  TRACE_EVENT_ASYNC_END0("navigation", "RenderFrameHostImpl::Unload", this);
  if (unload_event_monitor_timeout_)
    unload_event_monitor_timeout_->Stop();

  ClearWebUI();

  bool deleted =
      frame_tree_node_->render_manager()->DeleteFromPendingList(this);
  CHECK(deleted);
}

void RenderFrameHostImpl::DisableUnloadTimerForTesting() {
  unload_event_monitor_timeout_.reset();
}

void RenderFrameHostImpl::SetSubframeUnloadTimeoutForTesting(
    const base::TimeDelta& timeout) {
  subframe_unload_timeout_ = timeout;
}

void RenderFrameHostImpl::OnContextMenu(
    const UntrustworthyContextMenuParams& params) {
  if (!is_active())
    return;

  // Validate the URLs in |params|.  If the renderer can't request the URLs
  // directly, don't show them in the context menu.
  ContextMenuParams validated_params(params);
  validated_params.page_url = GetMainFrame()->GetLastCommittedURL();
  if (GetParent())  // Only populate |frame_url| for subframes.
    validated_params.frame_url = GetLastCommittedURL();

  // We don't validate |unfiltered_link_url| so that this field can be used
  // when users want to copy the original link URL.
  RenderProcessHost* process = GetProcess();
  process->FilterURL(true, &validated_params.link_url);
  process->FilterURL(true, &validated_params.src_url);
  process->FilterURL(false, &validated_params.page_url);
  process->FilterURL(true, &validated_params.frame_url);

  // It is necessary to transform the coordinates to account for nested
  // RenderWidgetHosts, such as with out-of-process iframes.
  gfx::Point original_point(validated_params.x, validated_params.y);
  gfx::Point transformed_point =
      static_cast<RenderWidgetHostViewBase*>(GetView())
          ->TransformPointToRootCoordSpace(original_point);
  validated_params.x = transformed_point.x();
  validated_params.y = transformed_point.y();

  if (validated_params.selection_start_offset < 0) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_NEGATIVE_SELECTION_START_OFFSET);
  }

  delegate_->ShowContextMenu(this, validated_params);
}

#if defined(OS_ANDROID)
void RenderFrameHostImpl::RequestSmartClipExtract(
    ExtractSmartClipDataCallback callback,
    gfx::Rect rect) {
  int32_t callback_id = smart_clip_callbacks_.Add(
      std::make_unique<ExtractSmartClipDataCallback>(std::move(callback)));
  frame_->ExtractSmartClipData(
      rect, base::BindOnce(&RenderFrameHostImpl::OnSmartClipDataExtracted,
                           base::Unretained(this), callback_id));
}

void RenderFrameHostImpl::OnSmartClipDataExtracted(int32_t callback_id,
                                                   const base::string16& text,
                                                   const base::string16& html,
                                                   const gfx::Rect& clip_rect) {
  std::move(*smart_clip_callbacks_.Lookup(callback_id))
      .Run(text, html, clip_rect);
  smart_clip_callbacks_.Remove(callback_id);
}
#endif  // defined(OS_ANDROID)

void RenderFrameHostImpl::OnVisualStateResponse(uint64_t id) {
  auto it = visual_state_callbacks_.find(id);
  if (it != visual_state_callbacks_.end()) {
    std::move(it->second).Run(true);
    visual_state_callbacks_.erase(it);
  } else {
    NOTREACHED() << "Received script response for unknown request";
  }
}

void RenderFrameHostImpl::RunModalAlertDialog(
    const base::string16& alert_message,
    RunModalAlertDialogCallback response_callback) {
  auto dialog_closed_callback = base::BindOnce(
      [](RunModalAlertDialogCallback response_callback, bool success,
         const base::string16& response) {
        // The response string is unused but we use a generic mechanism for
        // closing the javascript dialog that returns two arguments.
        std::move(response_callback).Run();
      },
      std::move(response_callback));
  RunJavaScriptDialog(alert_message, base::string16(),
                      JAVASCRIPT_DIALOG_TYPE_ALERT,
                      std::move(dialog_closed_callback));
}

void RenderFrameHostImpl::RunModalConfirmDialog(
    const base::string16& alert_message,
    RunModalConfirmDialogCallback response_callback) {
  auto dialog_closed_callback = base::BindOnce(
      [](RunModalConfirmDialogCallback response_callback, bool success,
         const base::string16& response) {
        // The response string is unused but we use a generic mechanism for
        // closing the javascript dialog that returns two arguments.
        std::move(response_callback).Run(success);
      },
      std::move(response_callback));
  RunJavaScriptDialog(alert_message, base::string16(),
                      JAVASCRIPT_DIALOG_TYPE_CONFIRM,
                      std::move(dialog_closed_callback));
}

void RenderFrameHostImpl::RunModalPromptDialog(
    const base::string16& alert_message,
    const base::string16& default_value,
    RunModalPromptDialogCallback response_callback) {
  RunJavaScriptDialog(alert_message, default_value,
                      JAVASCRIPT_DIALOG_TYPE_PROMPT,
                      std::move(response_callback));
}

void RenderFrameHostImpl::RunJavaScriptDialog(
    const base::string16& message,
    const base::string16& default_prompt,
    JavaScriptDialogType dialog_type,
    JavaScriptDialogCallback ipc_response_callback) {
  // Don't show the dialog if it's triggered on a frame that's pending deletion
  // (e.g., from an unload handler), or when the tab is being closed.
  if (!is_active()) {
    std::move(ipc_response_callback).Run(true, base::string16());
    return;
  }

  // While a JS message dialog is showing, tabs in the same process shouldn't
  // process input events.
  GetProcess()->SetBlocked(true);

  delegate_->RunJavaScriptDialog(
      this, message, default_prompt, dialog_type,
      base::BindOnce(&RenderFrameHostImpl::JavaScriptDialogClosed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(ipc_response_callback)));
}

void RenderFrameHostImpl::RunBeforeUnloadConfirm(
    bool is_reload,
    RunBeforeUnloadConfirmCallback ipc_response_callback) {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OnRunBeforeUnloadConfirm",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  // Allow at most one attempt to show a beforeunload dialog per navigation.
  RenderFrameHostImpl* beforeunload_initiator = GetBeforeUnloadInitiator();
  if (beforeunload_initiator) {
    // If the running beforeunload handler wants to display a dialog and the
    // before-unload type wants to ignore it, then short-circuit the request and
    // respond as if the user decided to stay on the page, canceling the unload.
    if (beforeunload_initiator->beforeunload_dialog_request_cancels_unload_) {
      std::move(ipc_response_callback).Run(/*success=*/false);
      return;
    }

    if (beforeunload_initiator->has_shown_beforeunload_dialog_) {
      // TODO(alexmos): Pass enough data back to renderer to record histograms
      // for Document.BeforeUnloadDialog and add the intervention console
      // message to match renderer-side behavior in
      // Document::DispatchBeforeUnloadEvent().
      std::move(ipc_response_callback).Run(/*success=*/true);
      return;
    }
    beforeunload_initiator->has_shown_beforeunload_dialog_ = true;
  } else {
    // TODO(alexmos): If a renderer-initiated beforeunload shows a dialog, it
    // won't find a |beforeunload_initiator|. This can happen for a
    // renderer-initiated navigation or window.close().  We should ensure that
    // when the browser process later kicks off subframe unload handlers (if
    // any), they won't be able to show additional dialogs. However, we can't
    // just set |has_shown_beforeunload_dialog_| because we don't know which
    // frame is navigating/closing here.  Plumb enough information here to fix
    // this.
  }

  // While a JS beforeunload dialog is showing, tabs in the same process
  // shouldn't process input events.
  GetProcess()->SetBlocked(true);

  // The beforeunload dialog for this frame may have been triggered by a
  // browser-side request to this frame or a frame up in the frame hierarchy.
  // Stop any timers that are waiting.
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->beforeunload_timeout_)
      frame->beforeunload_timeout_->Stop();
  }

  auto ipc_callback_wrapper = base::BindOnce(
      [](RunBeforeUnloadConfirmCallback response_callback, bool success,
         const base::string16& response) {
        // The response string is unused but we use a generic mechanism for
        // closing the javascript dialog that returns two arguments.
        std::move(response_callback).Run(success);
      },
      std::move(ipc_response_callback));
  auto dialog_closed_callback = base::BindOnce(
      &RenderFrameHostImpl::JavaScriptDialogClosed,
      weak_ptr_factory_.GetWeakPtr(), std::move(ipc_callback_wrapper));

  delegate_->RunBeforeUnloadConfirm(this, is_reload,
                                    std::move(dialog_closed_callback));
}

void RenderFrameHostImpl::Are3DAPIsBlocked(Are3DAPIsBlockedCallback callback) {
  bool blocked = GpuDataManagerImpl::GetInstance()->Are3DAPIsBlocked(
      GetMainFrame()->GetLastCommittedURL(), GetProcess()->GetID(),
      GetRoutingID(), THREE_D_API_TYPE_WEBGL);
  std::move(callback).Run(blocked);
}

void RenderFrameHostImpl::ScaleFactorChanged(float scale) {
  delegate_->OnPageScaleFactorChanged(this, scale);
}

void RenderFrameHostImpl::ContentsPreferredSizeChanged(
    const gfx::Size& pref_size) {
  render_view_host_->OnDidContentsPreferredSizeChange(pref_size);
}

void RenderFrameHostImpl::TextAutosizerPageInfoChanged(
    blink::mojom::TextAutosizerPageInfoPtr page_info) {
  delegate_->OnTextAutosizerPageInfoChanged(this, std::move(page_info));
}

void RenderFrameHostImpl::UpdateFaviconURL(
    std::vector<blink::mojom::FaviconURLPtr> favicon_urls) {
  delegate_->UpdateFaviconURL(this, std::move(favicon_urls));
}

void RenderFrameHostImpl::DownloadURL(
    blink::mojom::DownloadURLParamsPtr blink_parameters) {
  mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token;
  if (!VerifyDownloadUrlParams(GetSiteInstance(), blink_parameters.get(),
                               &blob_url_token))
    return;

  mojo::PendingRemote<blink::mojom::Blob> blob_data_remote(
      std::move(blink_parameters->data_url_blob), blink::mojom::Blob::Version_);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("renderer_initiated_download", R"(
        semantics {
          sender: "Download from Renderer"
          description:
            "The frame has either navigated to a URL that was determined to be "
            "a download via one of the renderer's classification mechanisms, "
            "or WebView has requested a <canvas> or <img> element at a "
            "specific location be to downloaded."
          trigger:
            "The user navigated to a destination that was categorized as a "
            "download, or WebView triggered saving a <canvas> or <img> tag."
          data: "Only the URL we are attempting to download."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            DownloadRestrictions {
              DownloadRestrictions: 3
            }
          }
        })");
  std::unique_ptr<download::DownloadUrlParameters> parameters(
      new download::DownloadUrlParameters(blink_parameters->url,
                                          GetProcess()->GetID(),
                                          GetRenderViewHost()->GetRoutingID(),
                                          GetRoutingID(), traffic_annotation));
  parameters->set_content_initiated(true);
  parameters->set_suggested_name(
      blink_parameters->suggested_name.value_or(base::string16()));
  parameters->set_prompt(false);
  parameters->set_cross_origin_redirects(
      blink_parameters->cross_origin_redirects);
  parameters->set_referrer(
      blink_parameters->referrer ? blink_parameters->referrer->url : GURL());
  parameters->set_referrer_policy(Referrer::ReferrerPolicyForUrlRequest(
      blink_parameters->referrer ? blink_parameters->referrer->policy
                                 : network::mojom::ReferrerPolicy::kDefault));
  parameters->set_initiator(
      blink_parameters->initiator_origin.value_or(url::Origin()));
  parameters->set_download_source(download::DownloadSource::FROM_RENDERER);

  if (blob_data_remote) {
    DataURLBlobReader::ReadDataURLFromBlob(
        std::move(blob_data_remote),
        base::BindOnce(&OnDataURLRetrieved, std::move(parameters)));
    return;
  }

  StartDownload(std::move(parameters), std::move(blob_url_token));
}

void RenderFrameHostImpl::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the frame/document scope");
}

ukm::SourceId RenderFrameHostImpl::GetPageUkmSourceId() {
  int64_t navigation_id =
      GetMainFrame()->last_committed_cross_document_navigation_id_;
  if (navigation_id == -1)
    return ukm::kInvalidSourceId;
  return ukm::ConvertToSourceId(navigation_id,
                                ukm::SourceIdType::NAVIGATION_ID);
}

BrowserContext* RenderFrameHostImpl::GetBrowserContext() {
  return GetProcess()->GetBrowserContext();
}

StoragePartition* RenderFrameHostImpl::GetStoragePartition() {
  return BrowserContext::GetStoragePartition(GetBrowserContext(),
                                             GetSiteInstance());
}

void RenderFrameHostImpl::RequestTextSurroundingSelection(
    blink::mojom::LocalFrame::GetTextSurroundingSelectionCallback callback,
    int max_length) {
  DCHECK(!callback.is_null());
  GetAssociatedLocalFrame()->GetTextSurroundingSelection(max_length,
                                                         std::move(callback));
}

bool RenderFrameHostImpl::HasCommittingNavigationRequestForOrigin(
    const url::Origin& origin,
    NavigationRequest* navigation_request_to_exclude) {
  if (navigation_request_ &&
      navigation_request_.get() != navigation_request_to_exclude &&
      navigation_request_->HasCommittingOrigin(origin)) {
    return true;
  }

  for (const auto& it : navigation_requests_) {
    NavigationRequest* request = it.first;
    if (request != navigation_request_to_exclude &&
        request->HasCommittingOrigin(origin)) {
      return true;
    }
  }

  // Note: this function excludes |same_document_navigation_request_|, which
  // should be ok since these cannot change the origin.
  return false;
}

void RenderFrameHostImpl::SendInterventionReport(const std::string& id,
                                                 const std::string& message) {
  GetAssociatedLocalFrame()->SendInterventionReport(id, message);
}

WebUI* RenderFrameHostImpl::GetWebUI() {
  return web_ui();
}

void RenderFrameHostImpl::AllowBindings(int bindings_flags) {
  // Never grant any bindings to browser plugin guests.
  if (GetProcess()->IsForGuestsOnly()) {
    NOTREACHED() << "Never grant bindings to a guest process.";
    return;
  }
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::AllowBindings",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "bindings flags", bindings_flags);

  int webui_bindings = bindings_flags & kWebUIBindingsPolicyMask;

  // TODO(nasko): Ensure callers that specify non-zero WebUI bindings are
  // doing so on a RenderFrameHost that has WebUI associated with it.

  // The bindings being granted here should not differ from the bindings that
  // the associated WebUI requires.
  if (web_ui_)
    CHECK_EQ(web_ui_->GetBindings(), webui_bindings);

  // Ensure we aren't granting WebUI bindings to a process that has already
  // been used for non-privileged views.
  if (webui_bindings && GetProcess()->IsInitializedAndNotDead() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID())) {
    // This process has no bindings yet. Make sure it does not have more
    // than this single active view.
    // --single-process only has one renderer.
    if (GetProcess()->GetActiveViewCount() > 1 &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSingleProcess))
      return;
  }

  if (webui_bindings) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
        GetProcess()->GetID(), webui_bindings);
  }

  enabled_bindings_ |= bindings_flags;

  if (render_frame_created_) {
    if (!frame_bindings_control_)
      GetRemoteAssociatedInterfaces()->GetInterface(&frame_bindings_control_);
    frame_bindings_control_->AllowBindings(enabled_bindings_);
  }
}

int RenderFrameHostImpl::GetEnabledBindings() {
  return enabled_bindings_;
}

void RenderFrameHostImpl::SetWebUIProperty(const std::string& name,
                                           const std::string& value) {
  // This is a sanity check before telling the renderer to enable the property.
  // It could lie and send the corresponding IPC messages anyway, but we will
  // not act on them if enabled_bindings_ doesn't agree. If we get here without
  // WebUI bindings, terminate the renderer process.
  if (enabled_bindings_ & BINDINGS_POLICY_WEB_UI)
    Send(new FrameMsg_SetWebUIProperty(routing_id_, name, value));
  else
    ReceivedBadMessage(GetProcess(), bad_message::RVH_WEB_UI_BINDINGS_MISMATCH);
}

void RenderFrameHostImpl::DisableBeforeUnloadHangMonitorForTesting() {
  beforeunload_timeout_.reset();
}

bool RenderFrameHostImpl::IsBeforeUnloadHangMonitorDisabledForTesting() {
  return !beforeunload_timeout_;
}

void RenderFrameHostImpl::DoNotDeleteForTesting() {
  do_not_delete_for_testing_ = true;
}

bool RenderFrameHostImpl::IsFeatureEnabled(
    blink::mojom::FeaturePolicyFeature feature) {
  return feature_policy_ && feature_policy_->IsFeatureEnabledForOrigin(
                                feature, GetLastCommittedOrigin());
}

bool RenderFrameHostImpl::IsFeatureEnabled(
    blink::mojom::DocumentPolicyFeature feature) {
  blink::mojom::PolicyValueType feature_type =
      blink::GetDocumentPolicyFeatureInfoMap().at(feature).default_value.Type();
  return IsFeatureEnabled(
      feature, blink::PolicyValue::CreateMaxPolicyValue(feature_type));
}

bool RenderFrameHostImpl::IsFeatureEnabled(
    blink::mojom::DocumentPolicyFeature feature,
    blink::PolicyValue threshold_value) {
  return document_policy_ &&
         document_policy_->IsFeatureEnabled(feature, threshold_value);
}

void RenderFrameHostImpl::ViewSource() {
  delegate_->ViewSource(this);
}

void RenderFrameHostImpl::FlushNetworkAndNavigationInterfacesForTesting() {
  DCHECK(network_service_disconnect_handler_holder_);
  network_service_disconnect_handler_holder_.FlushForTesting();

  if (!navigation_control_)
    GetNavigationControl();
  DCHECK(navigation_control_);
  navigation_control_.FlushForTesting();
}

void RenderFrameHostImpl::PrepareForInnerWebContentsAttach(
    PrepareForInnerWebContentsAttachCallback callback) {
  frame_tree_node_->render_manager()->PrepareForInnerDelegateAttach(
      std::move(callback));
}

void RenderFrameHostImpl::UpdateSubresourceLoaderFactories() {
  // We only send loader factory bundle upon navigation, so
  // bail out if the frame hasn't committed any yet.
  if (!has_committed_any_navigation_)
    return;
  DCHECK(!IsOutOfProcessNetworkService() ||
         network_service_disconnect_handler_holder_.is_bound());

  // We use the last committed Origin and ClientSecurityState. If the caller
  // wanted a factory associated to a navigation about to commit, the params
  // generated won't be correct. There is no good way of fixing this before
  // RenderDocumentHost (ie swapping RenderFrameHost on each navigation).
  mojo::PendingRemote<network::mojom::URLLoaderFactory> default_factory_remote;
  bool bypass_redirect_checks = false;
  if (recreate_default_url_loader_factory_after_network_service_crash_) {
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter_remote;
    if (coep_reporter_) {
      coep_reporter_->Clone(
          coep_reporter_remote.InitWithNewPipeAndPassReceiver());
    }
    bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
        CreateURLLoaderFactoryParamsForMainWorld(
            last_committed_origin_,
            mojo::Clone(last_committed_client_security_state_),
            std::move(coep_reporter_remote)),
        default_factory_remote.InitWithNewPipeAndPassReceiver());
  }

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories =
          std::make_unique<blink::PendingURLLoaderFactoryBundle>(
              std::move(default_factory_remote),
              blink::PendingURLLoaderFactoryBundle::SchemeMap(),
              CreateURLLoaderFactoriesForIsolatedWorlds(
                  GetExpectedMainWorldOriginForUrlLoaderFactory(),
                  isolated_worlds_requiring_separate_url_loader_factory_,
                  mojo::Clone(last_committed_client_security_state_)),
              bypass_redirect_checks);
  GetNavigationControl()->UpdateSubresourceLoaderFactories(
      std::move(subresource_loader_factories));
}

blink::mojom::FrameOwnerElementType
RenderFrameHostImpl::GetFrameOwnerElementType() {
  return frame_tree_node_->frame_owner_element_type();
}

bool RenderFrameHostImpl::HasTransientUserActivation() {
  return frame_tree_node_->HasTransientUserActivation();
}

void RenderFrameHostImpl::DidAccessInitialDocument() {
  delegate_->DidAccessInitialDocument();
}

void RenderFrameHostImpl::OnDidChangeOpener(int32_t opener_routing_id) {
  frame_tree_node_->render_manager()->DidChangeOpener(opener_routing_id,
                                                      GetSiteInstance());
}

void RenderFrameHostImpl::DidChangeName(const std::string& name,
                                        const std::string& unique_name) {
  if (GetParent() != nullptr) {
    // TODO(lukasza): Call ReceivedBadMessage when |unique_name| is empty.
    DCHECK(!unique_name.empty());
  }
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::OnDidChangeName",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "name length", name.length());

  std::string old_name = frame_tree_node()->frame_name();
  frame_tree_node()->SetFrameName(name, unique_name);
  if (old_name.empty() && !name.empty())
    frame_tree_node_->render_manager()->CreateProxiesForNewNamedFrame();
  delegate_->DidChangeName(this, name);
}

void RenderFrameHostImpl::DidSetFramePolicyHeaders(
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::ParsedFeaturePolicy& feature_policy_header,
    const blink::DocumentPolicy::FeatureState& document_policy_header) {
  if (!is_active())
    return;
  // Rebuild |feature_policy_| for this frame.
  ResetFeaturePolicy();
  feature_policy_->SetHeaderPolicy(feature_policy_header);

  // Rebuild |document_policy_| for this frame.
  // Note: document_policy_header is the document policy state used to
  // initialize |document_policy_| in SecurityContext on renderer side. It is
  // supposed to be compatible with required_document_policy. If not, kill the
  // renderer.
  if (blink::DocumentPolicy::IsPolicyCompatible(
          frame_tree_node()->effective_frame_policy().required_document_policy,
          document_policy_header)) {
    document_policy_ = blink::DocumentPolicy::CreateWithHeaderPolicy(
        {document_policy_header, {} /* endpoint_map */});
  } else {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_BAD_DOCUMENT_POLICY_HEADER);
    return;
  }

  // Update the feature policy and sandbox flags in the frame tree. This will
  // send any updates to proxies if necessary.
  //
  // Feature policy's inheritance from parent frame's feature policy is through
  // accessing parent frame's security context(either remote or local) when
  // initializing child's security context, so the update to proxies is needed.
  //
  // Document policy's inheritance from parent frame's required document policy
  // is done at |HTMLFrameOwnerElement::UpdateRequiredPolicy|. Parent frame owns
  // both parent required document policy and child frame's frame owner element
  // which contains child's required document policy, so there is no need to
  // store required document policy in proxies.
  frame_tree_node()->UpdateFramePolicyHeaders(sandbox_flags,
                                              feature_policy_header);

  // Save a copy of the now-active sandbox flags on this RFHI.
  active_sandbox_flags_ = frame_tree_node()->active_sandbox_flags();
}

void RenderFrameHostImpl::EnforceInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {
  frame_tree_node()->SetInsecureRequestPolicy(policy);
}

void RenderFrameHostImpl::EnforceInsecureNavigationsSet(
    const std::vector<uint32_t>& set) {
  frame_tree_node()->SetInsecureNavigationsSet(set);
}

RenderFrameHostImpl* RenderFrameHostImpl::FindAndVerifyChild(
    int32_t child_frame_routing_id,
    bad_message::BadMessageReason reason) {
  auto child_frame_or_proxy = LookupRenderFrameHostOrProxy(
      GetProcess()->GetID(), child_frame_routing_id);
  return FindAndVerifyChildInternal(child_frame_or_proxy, reason);
}

RenderFrameHostImpl* RenderFrameHostImpl::FindAndVerifyChild(
    const base::UnguessableToken& child_frame_token,
    bad_message::BadMessageReason reason) {
  auto child_frame_or_proxy =
      LookupRenderFrameHostOrProxy(GetProcess()->GetID(), child_frame_token);
  return FindAndVerifyChildInternal(child_frame_or_proxy, reason);
}

RenderFrameHostImpl* RenderFrameHostImpl::FindAndVerifyChildInternal(
    RenderFrameHostOrProxy child_frame_or_proxy,
    bad_message::BadMessageReason reason) {
  // A race can result in |child| to be nullptr. Avoid killing the renderer in
  // that case.
  if (!child_frame_or_proxy)
    return nullptr;

  if (child_frame_or_proxy.GetFrameTreeNode()->frame_tree() !=
      frame_tree_node()->frame_tree()) {
    // Ignore the cases when the child lives in a different frame tree.
    // This is possible when we create a proxy for inner WebContents (e.g.
    // for portals) so the |child_frame_or_proxy| points to the root frame
    // of the nested WebContents, which is in a different tree.
    // TODO(altimin, lfg): Reconsider what the correct behaviour here should be.
    return nullptr;
  }

  if (child_frame_or_proxy.GetFrameTreeNode()->parent() != this) {
    bad_message::ReceivedBadMessage(GetProcess(), reason);
    return nullptr;
  }
  return child_frame_or_proxy.proxy
             ? child_frame_or_proxy.proxy->frame_tree_node()
                   ->current_frame_host()
             : child_frame_or_proxy.frame;
}

void RenderFrameHostImpl::OnDidChangeFramePolicy(
    int32_t frame_routing_id,
    const blink::FramePolicy& frame_policy) {
  // Ensure that a frame can only update sandbox flags or feature policy for its
  // immediate children.  If this is not the case, the renderer is considered
  // malicious and is killed.
  RenderFrameHostImpl* child = FindAndVerifyChild(
      // TODO(iclelland): Rename this message
      frame_routing_id, bad_message::RFH_SANDBOX_FLAGS);
  if (!child)
    return;

  child->frame_tree_node()->SetPendingFramePolicy(frame_policy);

  // Notify the RenderFrame if it lives in a different process from its parent.
  // The frame's proxies in other processes also need to learn about the updated
  // flags and policy, but these notifications are sent later in
  // RenderFrameHostManager::CommitPendingFramePolicy(), when the frame
  // navigates and the new policies take effect.
  if (child->GetSiteInstance() != GetSiteInstance()) {
    child->GetAssociatedLocalFrame()->DidUpdateFramePolicy(frame_policy);
  }
}

void RenderFrameHostImpl::UpdateTitle(
    const base::Optional<::base::string16>& title,
    base::i18n::TextDirection title_direction) {
  // This message should only be sent for top-level frames.
  if (!is_main_frame())
    return;

  base::string16 received_title;
  if (title.has_value())
    received_title = title.value();

  if (received_title.length() > blink::mojom::kMaxTitleChars) {
    mojo::ReportBadMessage("Renderer sent too many characters in title.");
    return;
  }

  delegate_->UpdateTitle(this, received_title, title_direction);
}

void RenderFrameHostImpl::UpdateEncoding(const std::string& encoding_name) {
  // This message is only sent for top-level frames. TODO(avi): when frame tree
  // mirroring works correctly, add a check here to enforce it.
  if (encoding_name == last_reported_encoding_)
    return;
  last_reported_encoding_ = encoding_name;

  canonical_encoding_ =
      base::GetCanonicalEncodingNameByAliasName(encoding_name);
}

void RenderFrameHostImpl::FrameSizeChanged(const gfx::Size& frame_size) {
  frame_size_ = frame_size;
  delegate_->FrameSizeChanged(this, frame_size);
}

void RenderFrameHostImpl::FullscreenStateChanged(bool is_fullscreen) {
  if (!is_active())
    return;
  delegate_->FullscreenStateChanged(this, is_fullscreen);
}

void RenderFrameHostImpl::RegisterProtocolHandler(const std::string& scheme,
                                                  const GURL& url,
                                                  const base::string16& title,
                                                  bool user_gesture) {
  delegate_->RegisterProtocolHandler(this, scheme, url, title, user_gesture);
}

void RenderFrameHostImpl::UnregisterProtocolHandler(const std::string& scheme,
                                                    const GURL& url,
                                                    bool user_gesture) {
  delegate_->UnregisterProtocolHandler(this, scheme, url, user_gesture);
}

void RenderFrameHostImpl::DidDisplayInsecureContent() {
  delegate_->DidDisplayInsecureContent();
}

void RenderFrameHostImpl::DidContainInsecureFormAction() {
  delegate_->DidContainInsecureFormAction();
}

void RenderFrameHostImpl::DocumentAvailableInMainFrame(
    bool uses_temporary_zoom_level) {
  if (!frame_tree_node_->IsMainFrame()) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_INVALID_CALL_FROM_NOT_MAIN_FRAME);
    return;
  }
  delegate_->DocumentAvailableInMainFrame();

  if (!uses_temporary_zoom_level)
    return;

#if !defined(OS_ANDROID)
  HostZoomMapImpl* host_zoom_map =
      static_cast<HostZoomMapImpl*>(HostZoomMap::Get(GetSiteInstance()));
  host_zoom_map->SetTemporaryZoomLevel(GetProcess()->GetID(),
                                       render_view_host()->GetRoutingID(),
                                       host_zoom_map->GetDefaultZoomLevel());
#endif  // !defined(OS_ANDROID)
}

void RenderFrameHostImpl::SetNeedsOcclusionTracking(bool needs_tracking) {
  // Don't process the IPC if this RFH is pending deletion.  See also
  // https://crbug.com/972566.
  if (!is_active())
    return;

  RenderFrameProxyHost* proxy =
      frame_tree_node()->render_manager()->GetProxyToParent();
  if (!proxy) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }

  proxy->GetAssociatedRemoteFrame()->SetNeedsOcclusionTracking(needs_tracking);
}

void RenderFrameHostImpl::LifecycleStateChanged(
    blink::mojom::FrameLifecycleState state) {
  frame_lifecycle_state_ = state;
}

#if defined(OS_ANDROID)
void RenderFrameHostImpl::UpdateUserGestureCarryoverInfo() {
  delegate_->UpdateUserGestureCarryoverInfo();
}
#endif

void RenderFrameHostImpl::VisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  visibility_ = visibility;
  UpdateFrameFrozenState();
}

void RenderFrameHostImpl::DidChangeThemeColor(
    const base::Optional<SkColor>& theme_color) {
  render_view_host_->OnThemeColorChanged(this, theme_color);
}

void RenderFrameHostImpl::SetCommitCallbackInterceptorForTesting(
    CommitCallbackInterceptor* interceptor) {
  // This DCHECK's aims to avoid unexpected replacement of an interceptor.
  // If this becomes a legitimate use case, feel free to remove.
  DCHECK(!commit_callback_interceptor_ || !interceptor);
  commit_callback_interceptor_ = interceptor;
}

void RenderFrameHostImpl::DidBlockNavigation(
    const GURL& blocked_url,
    const GURL& initiator_url,
    blink::mojom::NavigationBlockedReason reason) {
  delegate_->OnDidBlockNavigation(blocked_url, initiator_url, reason);
}

void RenderFrameHostImpl::DidChangeLoadProgress(double load_progress) {
  frame_tree_node_->DidChangeLoadProgress(load_progress);
}

void RenderFrameHostImpl::DidFinishLoad(const GURL& validated_url) {
  delegate_->OnDidFinishLoad(this, validated_url);
}

void RenderFrameHostImpl::DispatchLoad() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::DispatchLoad",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  // Don't forward the load event if this RFH is pending deletion. This can
  // happen in a race where this RenderFrameHost finishes loading just after
  // the frame navigates away. See https://crbug.com/626802.
  if (!is_active())
    return;

  // We should never be receiving this message from a speculative RFH.
  DCHECK(IsCurrent());

  // Only frames with an out-of-process parent frame should be sending this
  // message.
  RenderFrameProxyHost* proxy =
      frame_tree_node()->render_manager()->GetProxyToParent();
  if (!proxy) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }

  proxy->GetAssociatedRemoteFrame()->DispatchLoadEventForFrameOwner();
}

void RenderFrameHostImpl::GoToEntryAtOffset(int32_t offset,
                                            bool has_user_gesture) {
  delegate_->OnGoToEntryAtOffset(this, offset, has_user_gesture);
}

void RenderFrameHostImpl::HandleAccessibilityFindInPageResult(
    blink::mojom::FindInPageResultAXParamsPtr params) {
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs) && is_active()) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager) {
      manager->OnFindInPageResult(params->request_id, params->match_index,
                                  params->start_id, params->start_offset,
                                  params->end_id, params->end_offset);
    }
  }
}

void RenderFrameHostImpl::HandleAccessibilityFindInPageTermination() {
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs) && is_active()) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager)
      manager->OnFindInPageTermination();
  }
}

void RenderFrameHostImpl::DocumentOnLoadCompleted() {
  // This message is only sent for top-level frames.
  //
  // TODO(avi): when frame tree mirroring works correctly, add a check here
  // to enforce it.
  delegate_->DocumentOnLoadCompleted(this);
}

void RenderFrameHostImpl::ForwardResourceTimingToParent(
    blink::mojom::ResourceTimingInfoPtr timing) {
  // Don't forward the resource timing if this RFH is pending deletion. This can
  // happen in a race where this RenderFrameHost finishes loading just after
  // the frame navigates away. See https://crbug.com/626802.
  if (!is_active())
    return;

  // We should never be receiving this message from a speculative RFH.
  DCHECK(IsCurrent());

  RenderFrameProxyHost* proxy =
      frame_tree_node()->render_manager()->GetProxyToParent();
  if (!proxy) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }
  proxy->GetAssociatedRemoteFrame()->AddResourceTimingFromChild(
      std::move(timing));
}

RenderWidgetHostViewBase* RenderFrameHostImpl::GetViewForAccessibility() {
  return static_cast<RenderWidgetHostViewBase*>(
      frame_tree_node_->IsMainFrame()
          ? render_view_host_->GetWidget()->GetView()
          : GetMainFrame()->render_view_host_->GetWidget()->GetView());
}

void RenderFrameHostImpl::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  if (frame_)
    frame_->UpdateBrowserControlsState(constraints, current, animate);
}

void RenderFrameHostImpl::Reload() {
  if (!IsRenderFrameLive())
    return;

  // TODO(https://crbug.com/995428): This IPC is deprecated. Navigations are
  // handled from the browser process. There is no need to send an IPC to the
  // renderer process for this.
  Send(new FrameMsg_Reload(GetRoutingID()));
}

void RenderFrameHostImpl::SendAccessibilityEventsToManager(
    const AXEventNotificationDetails& details) {
  if (browser_accessibility_manager_ &&
      !browser_accessibility_manager_->OnAccessibilityEvents(details)) {
    // OnAccessibilityEvents returns false in IPC error conditions
    AccessibilityFatalError();
  }
}

void RenderFrameHostImpl::EvictFromBackForwardCache() {
  // TODO(hajimehoshi): This function should take the reason from the renderer
  // side.
  EvictFromBackForwardCacheWithReason(
      BackForwardCacheMetrics::NotRestoredReason::kJavaScriptExecution);
}

void RenderFrameHostImpl::EvictFromBackForwardCacheWithReason(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  BackForwardCacheCanStoreDocumentResult can_store;
  can_store.No(reason);
  EvictFromBackForwardCacheWithReasons(can_store);
}

void RenderFrameHostImpl::EvictFromBackForwardCacheWithReasons(
    const BackForwardCacheCanStoreDocumentResult& can_store) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::EvictFromBackForwardCache",
               "can_store", can_store.ToString(), "rfh", this);
  DCHECK(IsBackForwardCacheEnabled());

  if (is_evicted_from_back_forward_cache_)
    return;

  bool in_back_forward_cache = IsInBackForwardCache();

  RenderFrameHostImpl* top_document = this;
  while (top_document->parent_) {
    top_document = top_document->parent_;
    DCHECK_EQ(top_document->IsInBackForwardCache(), in_back_forward_cache);
  }

  // TODO(hajimehoshi): Record the 'race condition' by JavaScript execution when
  // |in_back_forward_cache| is false.
  BackForwardCacheMetrics* metrics = top_document->GetBackForwardCacheMetrics();
  if (in_back_forward_cache && metrics)
    metrics->MarkNotRestoredWithReason(can_store);

  if (!in_back_forward_cache) {
    TRACE_EVENT0("navigation", "BackForwardCache_EvictAfterDocumentRestored");

    BackForwardCacheMetrics::RecordEvictedAfterDocumentRestored(
        BackForwardCacheMetrics::EvictedAfterDocumentRestoredReason::
            kByJavaScript);

    // A document is evicted from the BackForwardCache, but it has already been
    // restored. The current document should be reloaded, because it is not
    // salvageable.
    frame_tree_node_->navigator()->GetController()->Reload(ReloadType::NORMAL,
                                                           false);
    return;
  }

  // Check if there is an in-flight navigation restoring the frame that
  // is being evicted.
  NavigationRequest* in_flight_navigation_request =
      top_document->frame_tree_node()->navigation_request();
  bool is_navigation_to_evicted_frame_in_flight =
      (in_flight_navigation_request &&
       in_flight_navigation_request->rfh_restored_from_back_forward_cache() ==
           top_document);

  if (is_navigation_to_evicted_frame_in_flight) {
    // If we are currently navigating to the frame that was just evicted, we
    // must restart the navigation. This is important because restarting the
    // navigation deletes the NavigationRequest associated with the evicted
    // frame (preventing use-after-free).
    // This should also happen asynchronously as eviction might happen in the
    // middle of another navigation — we should not try to restart the
    // navigation in that case.
    // NOTE: Here we rely on the PostTask inside this function running before
    // the task posted to destroy the evicted frames below.
    in_flight_navigation_request->RestartBackForwardCachedNavigation();
  }

  NavigationControllerImpl* controller = static_cast<NavigationControllerImpl*>(
      frame_tree_node_->navigator()->GetController());

  // Evict the frame and schedule it to be destroyed. Eviction happens
  // immediately, but destruction is delayed, so that callers don't have to
  // worry about use-after-free of |this|.
  top_document->is_evicted_from_back_forward_cache_ = true;
  controller->GetBackForwardCache().PostTaskToDestroyEvictedFrames();
}

bool RenderFrameHostImpl::HasSeenRecentXrOverlaySetup() {
  static constexpr base::TimeDelta kMaxInterval =
      base::TimeDelta::FromSeconds(1);
  base::TimeDelta delta = base::TimeTicks::Now() - last_xr_overlay_setup_time_;
  DVLOG(2) << __func__ << ": return " << (delta <= kMaxInterval);
  return delta <= kMaxInterval;
}

void RenderFrameHostImpl::SetIsXrOverlaySetup() {
  DVLOG(2) << __func__;
  last_xr_overlay_setup_time_ = base::TimeTicks::Now();
}

// TODO(alexmos): When the allowFullscreen flag is known in the browser
// process, use it to double-check that fullscreen can be entered here.
void RenderFrameHostImpl::EnterFullscreen(
    blink::mojom::FullscreenOptionsPtr options,
    EnterFullscreenCallback callback) {
  // Consume the user activation when entering fullscreen mode in the browser
  // side when the renderer is compromised and the fullscreen is not triggered
  // by a user generated orientation change, because the fullscreen can be
  // triggered by either a user activation or a user generated orientation
  // change.
  // CanEnterFullscreenWithoutUserActivation is always false by default, so it
  // keeps the current logic that we can enter fullscreen mode either by the
  // orientation change or successfully consuming the user activation. This
  // function is used for layout tests to allow fullscreen when mocking screen
  // screen orientation changes.
  // TODO(lanwei): Investigate whether we can terminate the renderer when the
  // user activation has already been consumed.
  if (!delegate_->HasSeenRecentScreenOrientationChange() &&
      !HasSeenRecentXrOverlaySetup() &&
      !GetContentClient()
           ->browser()
           ->CanEnterFullscreenWithoutUserActivation()) {
    bool is_consumed = frame_tree_node_->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kConsumeTransientActivation);
    if (!is_consumed) {
      DLOG(ERROR) << "Cannot enter fullscreen because there is no transient "
                  << "user activation, orientation change, or XR overlay.";
      std::move(callback).Run(/*granted=*/false);
      return;
    }
  }

  if (!delegate_->CanEnterFullscreenMode()) {
    std::move(callback).Run(/*granted=*/false);
    return;
  }
  std::move(callback).Run(/*granted=*/true);

  // Entering fullscreen from a cross-process subframe also affects all
  // renderers for ancestor frames, which will need to apply fullscreen CSS to
  // appropriate ancestor <iframe> elements, fire fullscreenchange events, etc.
  // Thus, walk through the ancestor chain of this frame and for each (parent,
  // child) pair, send a message about the pending fullscreen change to the
  // child's proxy in parent's SiteInstance. The renderer process will use this
  // to find the <iframe> element in the parent frame that will need fullscreen
  // styles. This is done at most once per SiteInstance: for example, with a
  // A-B-A-B hierarchy, if the bottom frame goes fullscreen, this only needs to
  // notify its parent, and Blink-side logic will take care of applying
  // necessary changes to the other two ancestors.
  std::set<SiteInstance*> notified_instances;
  notified_instances.insert(GetSiteInstance());
  for (RenderFrameHostImpl* rfh = this; rfh->GetParent();
       rfh = rfh->GetParent()) {
    SiteInstance* parent_site_instance = rfh->GetParent()->GetSiteInstance();
    if (base::Contains(notified_instances, parent_site_instance))
      continue;

    RenderFrameProxyHost* child_proxy =
        rfh->frame_tree_node()->render_manager()->GetRenderFrameProxyHost(
            parent_site_instance);
    child_proxy->GetAssociatedRemoteFrame()->WillEnterFullscreen();
    notified_instances.insert(parent_site_instance);
  }

  // TODO(alexmos): See if this can use the last committed origin instead.
  delegate_->EnterFullscreenMode(GetLastCommittedURL().GetOrigin(), *options);
  delegate_->FullscreenStateChanged(this, true /* is_fullscreen */);

  // The previous call might change the fullscreen state. We need to make sure
  // the renderer is aware of that, which is done via the resize message.
  // Typically, this will be sent as part of the call on the |delegate_| above
  // when resizing the native windows, but sometimes fullscreen can be entered
  // without causing a resize, so we need to ensure that the resize message is
  // sent in that case. We always send this to the main frame's widget, and if
  // there are any OOPIF widgets, this will also trigger them to resize via
  // frameRectsChanged.
  render_view_host_->GetWidget()->SynchronizeVisualProperties();
}

// TODO(alexmos): When the allowFullscreen flag is known in the browser
// process, use it to double-check that fullscreen can be entered here.
void RenderFrameHostImpl::ExitFullscreen() {
  delegate_->ExitFullscreenMode(/* will_cause_resize */ true);

  // The previous call might change the fullscreen state. We need to make sure
  // the renderer is aware of that, which is done via the resize message.
  // Typically, this will be sent as part of the call on the |delegate_| above
  // when resizing the native windows, but sometimes fullscreen can be entered
  // without causing a resize, so we need to ensure that the resize message is
  // sent in that case. We always send this to the main frame's widget, and if
  // there are any OOPIF widgets, this will also trigger them to resize via
  // frameRectsChanged.
  render_view_host_->GetWidget()->SynchronizeVisualProperties();
}

void RenderFrameHostImpl::SuddenTerminationDisablerChanged(
    bool present,
    blink::mojom::SuddenTerminationDisablerType disabler_type) {
  switch (disabler_type) {
    case blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler:
      DCHECK_NE(has_before_unload_handler_, present);
      has_before_unload_handler_ = present;
      break;
    case blink::mojom::SuddenTerminationDisablerType::kUnloadHandler:
      DCHECK_NE(has_unload_handler_, present);
      has_unload_handler_ = present;
      break;
  }
}

bool RenderFrameHostImpl::GetSuddenTerminationDisablerState(
    blink::mojom::SuddenTerminationDisablerType disabler_type) {
  switch (disabler_type) {
    case blink::mojom::SuddenTerminationDisablerType::kBeforeUnloadHandler:
      return has_before_unload_handler_;
    case blink::mojom::SuddenTerminationDisablerType::kUnloadHandler:
      return has_unload_handler();
  }
}

bool RenderFrameHostImpl::InsidePortal() {
  return GetRenderViewHost()->GetDelegate()->IsPortal();
}

void RenderFrameHostImpl::DidFinishDocumentLoad() {
  dom_content_loaded_ = true;
  delegate_->DOMContentLoaded(this);
}

void RenderFrameHostImpl::OnDidStopLoading() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OnDidStopLoading",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  // This method should never be called when the frame is not loading.
  // Unfortunately, it can happen if a history navigation happens during a
  // BeforeUnload or Unload event.
  // TODO(fdegans): Change this to a DCHECK after LoadEventProgress has been
  // refactored in Blink. See crbug.com/466089
  if (!is_loading_)
    return;

  was_discarded_ = false;
  is_loading_ = false;

  // If we have a PeakGpuMemoryTrack, close it as loading as stopped. It will
  // asynchronously receive the statistics from the GPU process, and update
  // UMA stats.
  if (loading_mem_tracker_)
    loading_mem_tracker_.reset();

  // Only inform the FrameTreeNode of a change in load state if the load state
  // of this RenderFrameHost is being tracked.
  if (is_active())
    frame_tree_node_->DidStopLoading();
}

void RenderFrameHostImpl::OnSelectionChanged(const base::string16& text,
                                             uint32_t offset,
                                             const gfx::Range& range) {
  has_selection_ = !text.empty();
  GetRenderWidgetHost()->SelectionChanged(text, offset, range);
}

void RenderFrameHostImpl::FocusedElementChanged(
    bool is_editable_element,
    const gfx::Rect& bounds_in_frame_widget) {
  if (!GetView())
    return;

  has_focused_editable_element_ = is_editable_element;
  // First convert the bounds to root view.
  delegate_->OnFocusedElementChangedInFrame(
      this, gfx::Rect(GetView()->TransformPointToRootCoordSpace(
                          bounds_in_frame_widget.origin()),
                      bounds_in_frame_widget.size()));
}

void RenderFrameHostImpl::DidReceiveFirstUserActivation() {
  delegate_->DidReceiveFirstUserActivation(this);
}

void RenderFrameHostImpl::UpdateUserActivationState(
    blink::mojom::UserActivationUpdateType update_type) {
  if (!is_active())
    return;
  frame_tree_node_->UpdateUserActivationState(update_type);
}

void RenderFrameHostImpl::HadStickyUserActivationBeforeNavigationChanged(
    bool value) {
  frame_tree_node_->OnSetHadStickyUserActivationBeforeNavigation(value);
}

void RenderFrameHostImpl::ScrollRectToVisibleInParentFrame(
    const gfx::Rect& rect_to_scroll,
    blink::mojom::ScrollIntoViewParamsPtr params) {
  RenderFrameProxyHost* proxy =
      frame_tree_node_->render_manager()->GetProxyToParent();
  if (!proxy)
    return;
  proxy->ScrollRectToVisible(rect_to_scroll, std::move(params));
}

void RenderFrameHostImpl::BubbleLogicalScrollInParentFrame(
    blink::mojom::ScrollDirection direction,
    ui::ScrollGranularity granularity) {
  if (!is_active())
    return;

  RenderFrameProxyHost* proxy =
      frame_tree_node()->render_manager()->GetProxyToParent();
  if (!proxy) {
    // Only frames with an out-of-process parent frame should be sending this
    // message.
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_NO_PROXY_TO_PARENT);
    return;
  }

  proxy->GetAssociatedRemoteFrame()->BubbleLogicalScroll(direction,
                                                         granularity);
}

void RenderFrameHostImpl::OnFrameDidCallFocus() {
  delegate_->DidCallFocus();
}

void RenderFrameHostImpl::RenderFallbackContentInParentProcess() {
  bool is_object_type =
      frame_tree_node()->current_replication_state().frame_owner_element_type ==
      blink::mojom::FrameOwnerElementType::kObject;
  if (!is_object_type) {
    // Only object elements are expected to render their own fallback content
    // and since the owner type is set at the creation time of the
    // FrameTreeNode, this received IPC makes no sense here.
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_CANNOT_RENDER_FALLBACK_CONTENT);
    return;
  }

  // The ContentFrame() of the owner element in parent process could be either
  // a frame or a proxy. When navigating cross-site from a frame which is same-
  // site with its parent, the frame is still local (e.g., about:blank).
  // However, navigating from an origin which is cross-site with parent, the
  // frame of the owner is a proxy.
  auto* rfh = frame_tree_node()->current_frame_host();
  if (rfh->GetSiteInstance() == rfh->GetParent()->GetSiteInstance()) {
    rfh->GetAssociatedLocalFrame()->RenderFallbackContent();
  } else if (auto* proxy =
                 frame_tree_node()->render_manager()->GetProxyToParent()) {
    proxy->GetAssociatedRemoteFrame()->RenderFallbackContent();
  }
}

void RenderFrameHostImpl::ShowPopupMenu(
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    int32_t item_height,
    double font_size,
    int32_t selected_item,
    std::vector<blink::mojom::MenuItemPtr> menu_items,
    bool right_aligned,
    bool allow_multiple_selection) {
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  if (delegate()->ShowPopupMenu(this, &popup_client, bounds, item_height,
                                font_size, selected_item, &menu_items,
                                right_aligned, allow_multiple_selection)) {
    return;
  }

  auto* view = render_view_host()->delegate_->GetDelegateView();
  if (!view)
    return;

  gfx::Point original_point(bounds.x(), bounds.y());
  gfx::Point transformed_point =
      static_cast<RenderWidgetHostViewBase*>(GetView())
          ->TransformPointToRootCoordSpace(original_point);
  gfx::Rect transformed_bounds(transformed_point.x(), transformed_point.y(),
                               bounds.width(), bounds.height());
  view->ShowPopupMenu(this, std::move(popup_client), transformed_bounds,
                      item_height, font_size, selected_item,
                      std::move(menu_items), right_aligned,
                      allow_multiple_selection);
#endif
}

void RenderFrameHostImpl::DidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& http_method,
    const std::string& mime_type,
    network::mojom::RequestDestination request_destination) {
  delegate_->DidLoadResourceFromMemoryCache(this, url, http_method, mime_type,
                                            request_destination);
}

void RenderFrameHostImpl::DidChangeFrameOwnerProperties(
    const base::UnguessableToken& child_frame_token,
    blink::mojom::FrameOwnerPropertiesPtr properties) {
  auto* child =
      FindAndVerifyChild(child_frame_token, bad_message::RFH_OWNER_PROPERTY);
  if (!child)
    return;

  bool has_display_none_property_changed =
      properties->is_display_none !=
      child->frame_tree_node()->frame_owner_properties().is_display_none;

  child->frame_tree_node()->set_frame_owner_properties(*properties);

  child->frame_tree_node()->render_manager()->OnDidUpdateFrameOwnerProperties(
      *properties);
  if (has_display_none_property_changed) {
    delegate_->DidChangeDisplayState(
        child, properties->is_display_none /* is_display_none */);
  }
}

void RenderFrameHostImpl::BindInterfaceProviderReceiver(
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
        interface_provider_receiver) {
  DCHECK(!document_scoped_interface_provider_receiver_.is_bound());
  DCHECK(interface_provider_receiver.is_valid());
  document_scoped_interface_provider_receiver_.Bind(
      FilterRendererExposedInterfaces(mojom::kNavigation_FrameSpec,
                                      GetProcess()->GetID(),
                                      std::move(interface_provider_receiver)));
  document_scoped_interface_provider_receiver_.SetFilter(
      std::make_unique<ActiveURLMessageFilter>(this));
}

void RenderFrameHostImpl::BindBrowserInterfaceBrokerReceiver(
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
  DCHECK(receiver.is_valid());
  broker_receiver_.Bind(std::move(receiver));
  broker_receiver_.SetFilter(std::make_unique<ActiveURLMessageFilter>(this));
}

void RenderFrameHostImpl::BindDomOperationControllerHostReceiver(
    mojo::PendingAssociatedReceiver<mojom::DomAutomationControllerHost>
        receiver) {
  DCHECK(receiver.is_valid());
  dom_automation_controller_receiver_.Bind(std::move(receiver));
}

void RenderFrameHostImpl::SetKeepAliveTimeoutForTesting(
    base::TimeDelta timeout) {
  keep_alive_timeout_ = timeout;
  if (keep_alive_handle_factory_)
    keep_alive_handle_factory_->SetTimeout(keep_alive_timeout_);
}

void RenderFrameHostImpl::ShowCreatedWindow(int pending_widget_routing_id,
                                            WindowOpenDisposition disposition,
                                            const gfx::Rect& initial_rect,
                                            bool user_gesture) {
  delegate_->ShowCreatedWindow(GetProcess()->GetID(), pending_widget_routing_id,
                               disposition, initial_rect, user_gesture);
}

void RenderFrameHostImpl::RequestOverlayRoutingToken(
    RequestOverlayRoutingTokenCallback callback) {
  std::move(callback).Run(frame_token_);
}

void RenderFrameHostImpl::DomOperationResponse(const std::string& json_string) {
  delegate_->DomOperationResponse(json_string);
}

std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
RenderFrameHostImpl::CreateCrossOriginPrefetchLoaderFactoryBundle() {
  network::mojom::URLLoaderFactoryParamsPtr factory_params =
      URLLoaderFactoryParamsHelper::CreateForPrefetch(
          this, mojo::Clone(last_committed_client_security_state_));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  bool bypass_redirect_checks = false;
  // Passing an empty IsolationInfo ensures the factory is not initialized with
  // a IsolationInfo. This is necessary for a cross-origin prefetch factory
  // because the factory must use the value provided by requests going through
  // it.
  bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
      std::move(factory_params),
      pending_default_factory.InitWithNewPipeAndPassReceiver());

  return std::make_unique<blink::PendingURLLoaderFactoryBundle>(
      std::move(pending_default_factory),
      blink::PendingURLLoaderFactoryBundle::SchemeMap(),
      CreateURLLoaderFactoriesForIsolatedWorlds(
          GetExpectedMainWorldOriginForUrlLoaderFactory(),
          isolated_worlds_requiring_separate_url_loader_factory_,
          mojo::Clone(last_committed_client_security_state_)),
      bypass_redirect_checks);
}

base::WeakPtr<RenderFrameHostImpl> RenderFrameHostImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void RenderFrameHostImpl::TransferUserActivationFrom(
    int32_t source_routing_id) {
  RenderFrameHostImpl* source_rfh = RenderFrameHostImpl::FromID(
      GlobalFrameRoutingId(GetProcess()->GetID(), source_routing_id));
  if (source_rfh &&
      source_rfh->frame_tree_node()->HasTransientUserActivation()) {
    frame_tree_node()->TransferUserActivationFrom(source_rfh);
  }
}

void RenderFrameHostImpl::CreateNewWindow(
    mojom::CreateNewWindowParamsPtr params,
    CreateNewWindowCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::CreateNewWindow",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(), "url",
               params->target_url.possibly_invalid_spec());

  bool no_javascript_access = false;

  // Filter out URLs to which navigation is disallowed from this context.
  //
  // Note that currently, "javascript:" URLs and empty strings (both of which
  // are legal arguments to window.open) make it here; FilterURL rewrites them
  // to "about:blank" -- they shouldn't be cancelled.
  GetProcess()->FilterURL(false, &params->target_url);

  bool effective_transient_activation_state =
      params->allow_popup || frame_tree_node_->HasTransientUserActivation();

  // Ignore window creation when sent from a frame that's not current or
  // created.
  bool can_create_window =
      IsCurrent() && render_frame_created_ &&
      GetContentClient()->browser()->CanCreateWindow(
          this, GetLastCommittedURL(), GetMainFrame()->GetLastCommittedURL(),
          last_committed_origin_, params->window_container_type,
          params->target_url, params->referrer.To<Referrer>(),
          params->frame_name, params->disposition, *params->features,
          effective_transient_activation_state, params->opener_suppressed,
          &no_javascript_access);

  bool was_consumed = false;
  if (can_create_window) {
    // Consume activation even w/o User Activation v2, to sync other renderers
    // with calling renderer.
    was_consumed = frame_tree_node_->UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kConsumeTransientActivation);
  } else {
    std::move(callback).Run(mojom::CreateNewWindowStatus::kIgnore, nullptr);
    return;
  }

  // For Android WebView, we support a pop-up like behavior for window.open()
  // even if the embedding app doesn't support multiple windows. In this case,
  // window.open() will return "window" and navigate it to whatever URL was
  // passed.
  if (!render_view_host_->GetWebkitPreferences().supports_multiple_windows) {
    // See crbug.com/1083819, we should ignore if the URL is javascript: scheme,
    // previously we already filtered out javascript: scheme and replace the
    // URL with |kBlockedURL|, so we check against |kBlockedURL| here.
    if (params->target_url == GURL(kBlockedURL)) {
      std::move(callback).Run(mojom::CreateNewWindowStatus::kIgnore, nullptr);
    } else {
      std::move(callback).Run(mojom::CreateNewWindowStatus::kReuse, nullptr);
    }
    return;
  }

  // This will clone the sessionStorage for namespace_id_to_clone.
  StoragePartition* storage_partition = BrowserContext::GetStoragePartition(
      GetSiteInstance()->GetBrowserContext(), GetSiteInstance());
  DOMStorageContextWrapper* dom_storage_context =
      static_cast<DOMStorageContextWrapper*>(
          storage_partition->GetDOMStorageContext());

  scoped_refptr<SessionStorageNamespaceImpl> cloned_namespace;
  if (!params->clone_from_session_storage_namespace_id.empty()) {
    cloned_namespace = SessionStorageNamespaceImpl::CloneFrom(
        dom_storage_context, params->session_storage_namespace_id,
        params->clone_from_session_storage_namespace_id);
  } else {
    cloned_namespace = SessionStorageNamespaceImpl::Create(
        dom_storage_context, params->session_storage_namespace_id);
  }

  network::CrossOriginOpenerPolicy popup_coop;
  network::CrossOriginEmbedderPolicy popup_coep;
  if (base::FeatureList::IsEnabled(
          network::features::kCrossOriginOpenerPolicy)) {
    // On popup creation, if the opener and the openers's top-level document
    // are same origin, then the popup's initial empty document inherits its
    // COOP policy from the opener's top-level document. See
    // https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e#model
    RenderFrameHostImpl* top_level_opener = GetMainFrame();
    // Verify that they are same origin.
    if (top_level_opener->GetLastCommittedOrigin().IsSameOriginWith(
            GetLastCommittedOrigin())) {
      popup_coop = top_level_opener->cross_origin_opener_policy();
    } else {
      // The documents are cross origin, leave COOP of the popup to the default
      // unsafe-none.
      // Then set the popup to noopener if the top level COOP is same origin.
      if (top_level_opener->cross_origin_opener_policy().value ==
          network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin) {
        params->opener_suppressed = true;
        // The frame name should not be forwarded to a noopener popup.
        // TODO(https://crbug.com/1060691) This should be applied to all
        // popups opened with noopener.
        params->frame_name.clear();
      }
    }
  }

  // The initial empty document in the popup inherits the COEP of its opener (if
  // any).
  if (!params->opener_suppressed)
    popup_coep = cross_origin_embedder_policy();

  // If the opener is suppressed or script access is disallowed, we should
  // open the window in a new BrowsingInstance, and thus a new process. That
  // means the current renderer process will not be able to route messages to
  // it. Because of this, we will immediately show and navigate the window
  // in OnCreateNewWindowOnUI, using the params provided here.
  bool is_new_browsing_instance =
      params->opener_suppressed || no_javascript_access;

  DCHECK(IsRenderFrameLive());

  // The non-owning pointer |new_window| is valid in this stack frame since
  // nothing can delete it until this thread is freed up again.
  RenderFrameHostDelegate* new_window =
      delegate_->CreateNewWindow(this, *params, is_new_browsing_instance,
                                 was_consumed, cloned_namespace.get());

  if (is_new_browsing_instance || !new_window) {
    // Opener suppressed, Javascript access disabled, or delegate did not
    // provide a handle to any windows it created. In these cases, never tell
    // the renderer about the new window.
    std::move(callback).Run(mojom::CreateNewWindowStatus::kIgnore, nullptr);
    return;
  }

  RenderFrameHostImpl* main_frame = new_window->GetMainFrame();

  // When the popup is created, it hasn't committed any navigation yet - its
  // initial empty document should inherit the origin of its opener (the origin
  // may change after the first commit). See also https://crbug.com/932067.
  //
  // Note that that origin of the new frame might depend on sandbox flags.
  // Checking sandbox flags of the new frame should be safe at this point,
  // because the flags should be already inherited by the CreateNewWindow call
  // above.
  main_frame->SetOriginAndIsolationInfoOfNewFrame(GetLastCommittedOrigin());
  main_frame->cross_origin_opener_policy_ = popup_coop;
  main_frame->cross_origin_embedder_policy_ = popup_coep;

  if (main_frame->waiting_for_init_) {
    // Need to check |waiting_for_init_| as some paths inside CreateNewWindow
    // call above (eg if WebContentsDelegate::IsWebContentsCreationOverridden()
    // returns true) will resume requests by calling RenderFrameHostImpl::Init.
    main_frame->frame_->BlockRequests();
  }

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      main_frame_interface_provider_info;
  main_frame->BindInterfaceProviderReceiver(
      main_frame_interface_provider_info.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker;
  main_frame->BindBrowserInterfaceBrokerReceiver(
      browser_interface_broker.InitWithNewPipeAndPassReceiver());

  mojo::PendingAssociatedRemote<blink::mojom::FrameWidget> blink_frame_widget;
  mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
      blink_frame_widget_receiver =
          blink_frame_widget.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::FrameWidgetHost>
      blink_frame_widget_host;
  mojo::PendingAssociatedReceiver<blink::mojom::FrameWidgetHost>
      blink_frame_widget_host_receiver =
          blink_frame_widget_host.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget;
  mojo::PendingAssociatedReceiver<blink::mojom::Widget> blink_widget_receiver =
      blink_widget.InitWithNewEndpointAndPassReceiver();

  mojo::PendingAssociatedRemote<blink::mojom::WidgetHost> blink_widget_host;
  mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost>
      blink_widget_host_receiver =
          blink_widget_host.InitWithNewEndpointAndPassReceiver();

  // TODO(danakj): The main frame's RenderWidgetHost has no RenderWidgetHostView
  // yet here. It seems like it should though? In the meantime we send some
  // nonsense with a semi-valid but incorrect ScreenInfo (it needs a
  // RenderWidgetHostView to be correct). An updates VisualProperties will get
  // to the RenderWidget eventually.
  VisualProperties visual_properties;
  main_frame->GetLocalRenderWidgetHost()->GetScreenInfo(
      &visual_properties.screen_info);
  main_frame->GetLocalRenderWidgetHost()->BindFrameWidgetInterfaces(
      std::move(blink_frame_widget_host_receiver),
      std::move(blink_frame_widget));
  main_frame->GetLocalRenderWidgetHost()->BindWidgetInterfaces(
      std::move(blink_widget_host_receiver), std::move(blink_widget));

  bool wait_for_debugger =
      devtools_instrumentation::ShouldWaitForDebuggerInWindowOpen();
  mojom::CreateNewWindowReplyPtr reply = mojom::CreateNewWindowReply::New(
      main_frame->GetRenderViewHost()->GetRoutingID(),
      main_frame->GetRoutingID(), main_frame->frame_token(),
      main_frame->GetLocalRenderWidgetHost()->GetRoutingID(), visual_properties,
      std::move(blink_frame_widget_host),
      std::move(blink_frame_widget_receiver), std::move(blink_widget_host),
      std::move(blink_widget_receiver),
      mojom::DocumentScopedInterfaceBundle::New(
          std::move(main_frame_interface_provider_info),
          std::move(browser_interface_broker)),
      cloned_namespace->id(), main_frame->GetDevToolsFrameToken(),
      wait_for_debugger);

  std::move(callback).Run(mojom::CreateNewWindowStatus::kSuccess,
                          std::move(reply));
}

void RenderFrameHostImpl::CreatePortal(
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> pending_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
    CreatePortalCallback callback) {
  if (!Portal::IsEnabled()) {
    mojo::ReportBadMessage(
        "blink.mojom.Portal can only be used if the Portals feature is "
        "enabled.");
    frame_host_associated_receiver_.reset();
    return;
  }

  // We don't support attaching a portal inside a nested browsing context.
  if (!is_main_frame()) {
    mojo::ReportBadMessage(
        "RFHI::CreatePortal called in a nested browsing context");
    frame_host_associated_receiver_.reset();
    return;
  }

  // Note that we don't check |GetLastCommittedOrigin|, since that is inherited
  // by the initial empty document of a new frame.
  // TODO(1008989): Once issue 1008989 is fixed we could move this check into
  // |Portal::Create|.
  if (!GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    mojo::ReportBadMessage("Portal creation is restricted to the HTTP family.");
    frame_host_associated_receiver_.reset();
    return;
  }

  auto portal = std::make_unique<Portal>(this);
  portal->Bind(std::move(pending_receiver), std::move(client));
  auto it = portals_.insert(std::move(portal)).first;

  RenderFrameProxyHost* proxy_host = (*it)->CreateProxyAndAttachPortal();

  // Since the portal is newly created and has yet to commit a navigation, this
  // state is trivial.
  const FrameReplicationState& initial_replicated_state =
      proxy_host->frame_tree_node()->current_replication_state();
  DCHECK(initial_replicated_state.origin.opaque());

  std::move(callback).Run(proxy_host->GetRoutingID(), initial_replicated_state,
                          proxy_host->GetFrameToken(), (*it)->portal_token(),
                          (*it)->GetDevToolsFrameToken());
}

void RenderFrameHostImpl::AdoptPortal(
    const base::UnguessableToken& portal_token,
    AdoptPortalCallback callback) {
  Portal* portal = FindPortalByToken(portal_token);
  if (!portal) {
    mojo::ReportBadMessage("Unknown portal_token when adopting portal.");
    frame_host_associated_receiver_.reset();
    return;
  }
  DCHECK_EQ(portal->owner_render_frame_host(), this);
  RenderFrameProxyHost* proxy_host = portal->CreateProxyAndAttachPortal();
  std::move(callback).Run(
      proxy_host->GetRoutingID(),
      static_cast<RenderWidgetHostViewBase*>(proxy_host->frame_tree_node()
                                                 ->render_manager()
                                                 ->GetRenderWidgetHostView())
          ->GetFrameSinkId(),
      proxy_host->frame_tree_node()->current_replication_state(),
      proxy_host->GetFrameToken(), portal->GetDevToolsFrameToken());
}

void RenderFrameHostImpl::CreateNewWidget(
    mojo::PendingRemote<mojom::Widget> widget,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget,
    CreateNewWidgetCallback callback) {
  int32_t widget_route_id = GetProcess()->GetNextRoutingID();
  std::move(callback).Run(widget_route_id);
  delegate_->CreateNewWidget(GetProcess()->GetID(), widget_route_id,
                             std::move(widget), std::move(blink_widget_host),
                             std::move(blink_widget));
}

void RenderFrameHostImpl::CreateNewFullscreenWidget(
    mojo::PendingRemote<mojom::Widget> widget,
    mojo::PendingAssociatedReceiver<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedRemote<blink::mojom::Widget> blink_widget,
    CreateNewFullscreenWidgetCallback callback) {
  int32_t widget_route_id = GetProcess()->GetNextRoutingID();
  std::move(callback).Run(widget_route_id);
  delegate_->CreateNewFullscreenWidget(
      GetProcess()->GetID(), widget_route_id, std::move(widget),
      std::move(blink_widget_host), std::move(blink_widget));
}

void RenderFrameHostImpl::IssueKeepAliveHandle(
    mojo::PendingReceiver<mojom::KeepAliveHandle> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (GetProcess()->IsKeepAliveRefCountDisabled())
    return;

  if (base::FeatureList::IsEnabled(network::features::kDisableKeepaliveFetch)) {
    return;
  }

  if (!keep_alive_handle_factory_) {
    keep_alive_handle_factory_ =
        std::make_unique<KeepAliveHandleFactory>(GetProcess());
    keep_alive_handle_factory_->SetTimeout(keep_alive_timeout_);
  }
  keep_alive_handle_factory_->Create(std::move(receiver));
}

// TODO(ahemery): Move checks to mojo bad message reporting.
void RenderFrameHostImpl::BeginNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    mojo::PendingRemote<blink::mojom::NavigationInitiator>
        navigation_initiator) {
  if (frame_tree_node_->render_manager()->is_attaching_inner_delegate()) {
    // Avoid starting any new navigations since this frame is in the process of
    // attaching an inner delegate.
    return;
  }

  if (!is_active())
    return;

  TRACE_EVENT2("navigation", "RenderFrameHostImpl::BeginNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(), "url",
               common_params->url.possibly_invalid_spec());

  DCHECK(navigation_client.is_valid());

  mojom::CommonNavigationParamsPtr validated_params = common_params.Clone();
  if (!VerifyBeginNavigationCommonParams(GetSiteInstance(), &*validated_params))
    return;

  // If the request is bearing Trust Tokens parameters:
  // - it must not be a main-frame navigation, and
  // - for certain Trust Tokens operations, the frame's parent needs the
  // trust-token-redemption Feature Policy feature.
  if (begin_params->trust_token_params && !GetParent()) {
    mojo::ReportBadMessage("RFHI: Trust Token params in main frame nav");
    return;
  }
  if (begin_params->trust_token_params &&
      ParentNeedsTrustTokenFeaturePolicy(*begin_params)) {
    RenderFrameHostImpl* parent = GetParent();
    if (!parent->IsFeatureEnabled(
            blink::mojom::FeaturePolicyFeature::kTrustTokenRedemption)) {
      mojo::ReportBadMessage(
          "RFHI: Mandatory Trust Tokens Feature Policy feature is absent");
      return;
    }
  }

  GetProcess()->FilterURL(true, &begin_params->searchable_form_url);

  // If the request was for a blob URL, but the validated URL is no longer a
  // blob URL, reset the blob_url_token to prevent hitting the ReportBadMessage
  // below, and to make sure we don't incorrectly try to use the blob_url_token.
  if (common_params->url.SchemeIsBlob() &&
      !validated_params->url.SchemeIsBlob()) {
    blob_url_token = mojo::NullRemote();
  }

  if (blob_url_token && !validated_params->url.SchemeIsBlob()) {
    mojo::ReportBadMessage("Blob URL Token, but not a blob: URL");
    return;
  }
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (blob_url_token) {
    blob_url_loader_factory =
        ChromeBlobStorageContext::URLLoaderFactoryForToken(
            GetSiteInstance()->GetBrowserContext(), std::move(blob_url_token));
  }

  // If the request was for a blob URL, but no blob_url_token was passed in this
  // is not necessarily an error. Renderer initiated reloads for example are one
  // situation where a renderer currently doesn't have an easy way of resolving
  // the blob URL. For those situations resolve the blob URL here, as we don't
  // care about ordering with other blob URL manipulation anyway.
  if (validated_params->url.SchemeIsBlob() && !blob_url_loader_factory) {
    blob_url_loader_factory = ChromeBlobStorageContext::URLLoaderFactoryForUrl(
        GetSiteInstance()->GetBrowserContext(), validated_params->url);
  }

  if (waiting_for_init_) {
    pending_navigate_ = std::make_unique<PendingNavigation>(
        std::move(validated_params), std::move(begin_params),
        std::move(blob_url_loader_factory), std::move(navigation_client),
        std::move(navigation_initiator));
    return;
  }

  frame_tree_node()->navigator()->OnBeginNavigation(
      frame_tree_node(), std::move(validated_params), std::move(begin_params),
      std::move(blob_url_loader_factory), std::move(navigation_client),
      std::move(navigation_initiator), EnsurePrefetchedSignedExchangeCache(),
      MaybeCreateWebBundleHandleTracker());
}

void RenderFrameHostImpl::SubresourceResponseStarted(
    const GURL& url,
    net::CertStatus cert_status) {
  delegate_->SubresourceResponseStarted(url, cert_status);
}

void RenderFrameHostImpl::ResourceLoadComplete(
    blink::mojom::ResourceLoadInfoPtr resource_load_info) {
  GlobalRequestID global_request_id;
  const bool is_frame_request =
      blink::IsRequestDestinationFrame(resource_load_info->request_destination);
  if (main_frame_request_ids_.first == resource_load_info->request_id) {
    global_request_id = main_frame_request_ids_.second;
  } else if (is_frame_request) {
    // The load complete message for the main resource arrived before
    // |DidCommitProvisionalLoad()|. We save the load info so
    // |ResourceLoadComplete()| can be called later in
    // |DidCommitProvisionalLoad()| when we can map to the global request ID.
    deferred_main_frame_load_info_ = std::move(resource_load_info);
    return;
  }
  delegate_->ResourceLoadComplete(this, global_request_id,
                                  std::move(resource_load_info));
}

void RenderFrameHostImpl::HandleAXEvents(
    const std::vector<AXContentTreeUpdate>& updates,
    const std::vector<ui::AXEvent>& events,
    int32_t reset_token,
    HandleAXEventsCallback callback) {
  // Don't process this IPC if either we're waiting on a reset and this IPC
  // doesn't have the matching token ID, or if we're not waiting on a reset but
  // this message includes a reset token.
  if (accessibility_reset_token_ != reset_token) {
    std::move(callback).Run();
    return;
  }
  accessibility_reset_token_ = 0;

  RenderWidgetHostViewBase* view = GetViewForAccessibility();
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.is_mode_off() || !view || !is_active()) {
    std::move(callback).Run();
    return;
  }

  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs))
    GetOrCreateBrowserAccessibilityManager();

  AXEventNotificationDetails details;
  details.ax_tree_id = GetAXTreeID();
  details.events = events;

  details.updates.resize(updates.size());
  for (size_t i = 0; i < updates.size(); ++i) {
    const AXContentTreeUpdate& src_update = updates[i];
    ui::AXTreeUpdate* dst_update = &details.updates[i];
    if (src_update.has_tree_data) {
      dst_update->has_tree_data = true;
      ax_content_tree_data_ = src_update.tree_data;
      AXContentTreeDataToAXTreeData(&dst_update->tree_data);
    }
    dst_update->root_id = src_update.root_id;
    dst_update->node_id_to_clear = src_update.node_id_to_clear;
    dst_update->event_from = src_update.event_from;
    dst_update->nodes.resize(src_update.nodes.size());
    for (size_t j = 0; j < src_update.nodes.size(); ++j) {
      AXContentNodeDataToAXNodeData(src_update.nodes[j], &dst_update->nodes[j]);
    }
  }

  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs))
    SendAccessibilityEventsToManager(details);

  delegate_->AccessibilityEventReceived(details);

  // For testing only.
  if (!accessibility_testing_callback_.is_null()) {
    if (details.events.empty()) {
      // Objects were marked dirty but no events were provided.
      // The callback must still run, otherwise dump event tests can hang.
      accessibility_testing_callback_.Run(this, ax::mojom::Event::kNone, 0);
    } else {
      // Call testing callback functions for each event to fire.
      for (auto& event : details.events) {
        if (static_cast<int>(event.event_type) < 0)
          continue;

        accessibility_testing_callback_.Run(this, event.event_type, event.id);
      }
    }
  }

  // Always send an ACK or the renderer can be in a bad state.
  std::move(callback).Run();
}

void RenderFrameHostImpl::HandleAXLocationChanges(
    std::vector<mojom::LocationChangesPtr> changes) {
  if (accessibility_reset_token_ || !is_active())
    return;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view) {
    ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
    if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
      BrowserAccessibilityManager* manager =
          GetOrCreateBrowserAccessibilityManager();
      if (manager)
        manager->OnLocationChanges(changes);
    }

    // Send the updates to the automation extension API.
    std::vector<AXLocationChangeNotificationDetails> details;
    details.reserve(changes.size());
    for (auto& change : changes) {
      AXLocationChangeNotificationDetails detail;
      detail.id = change->id;
      detail.ax_tree_id = GetAXTreeID();
      detail.new_location = change->new_location;
      details.push_back(detail);
    }
    delegate_->AccessibilityLocationChangesReceived(details);
  }
}

media::MediaMetricsProvider::RecordAggregateWatchTimeCallback
RenderFrameHostImpl::GetRecordAggregateWatchTimeCallback() {
  return delegate_->GetRecordAggregateWatchTimeCallback();
}

void RenderFrameHostImpl::ResetWaitingState() {
  DCHECK(is_active());

  // Whenever we reset the RFH state, we should not be waiting for beforeunload
  // or close acks.  We clear them here to be safe, since they can cause
  // navigations to be ignored in DidCommitProvisionalLoad.
  if (is_waiting_for_beforeunload_completion_) {
    is_waiting_for_beforeunload_completion_ = false;
    if (beforeunload_timeout_)
      beforeunload_timeout_->Stop();
    has_shown_beforeunload_dialog_ = false;
    beforeunload_pending_replies_.clear();
  }
  send_before_unload_start_time_ = base::TimeTicks();
  render_view_host_->is_waiting_for_page_close_completion_ = false;
}

CanCommitStatus RenderFrameHostImpl::CanCommitOriginAndUrl(
    const url::Origin& origin,
    const GURL& url) {
  // If the --disable-web-security flag is specified, all bets are off and the
  // renderer process can send any origin it wishes.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity)) {
    return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
  }

  // Renderer-debug URLs can never be committed.
  if (IsRendererDebugURL(url)) {
    LogCanCommitOriginAndUrlFailureReason("is_renderer_debug_url");
    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  // Verify that if this RenderFrameHost is for a WebUI it is not committing a
  // URL which is not allowed in a WebUI process.
  if (!Navigator::CheckWebUIRendererDoesNotDisplayNormalURL(
          this, url,
          /* is_renderer_initiated_check */ true)) {
    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  // MHTML subframes can supply URLs at commit time that do not match the
  // process lock. For example, it can be either "cid:..." or arbitrary URL at
  // which the frame was at the time of generating the MHTML
  // (e.g. "http://localhost"). In such cases, don't verify the URL, but require
  // the URL to commit in the process of the main frame.
  if (!frame_tree_node()->IsMainFrame()) {
    RenderFrameHostImpl* main_frame = GetMainFrame();
    if (main_frame->is_mhtml_document()) {
      if (IsSameSiteInstance(main_frame))
        return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;

      // If an MHTML subframe commits in a different process (even one that
      // appears correct for the subframe's URL), then we aren't correctly
      // loading it from the archive and should kill the renderer.
      base::debug::SetCrashKeyString(
          base::debug::AllocateCrashKeyString(
              "oopif_in_mhtml_page", base::debug::CrashKeySize::Size32),
          is_mhtml_document() ? "is_mhtml_doc" : "not_mhtml_doc");
      LogCanCommitOriginAndUrlFailureReason("oopif_in_mhtml_page");
      return CanCommitStatus::CANNOT_COMMIT_URL;
    }
  }

  // Give the client a chance to disallow URLs from committing.
  if (!GetContentClient()->browser()->CanCommitURL(GetProcess(), url)) {
    LogCanCommitOriginAndUrlFailureReason(
        "content_client_disallowed_commit_url");
    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  const CanCommitStatus can_commit_status = policy->CanCommitOriginAndUrl(
      GetProcess()->GetID(), GetSiteInstance()->GetIsolationContext(), origin,
      url);
  if (can_commit_status != CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL) {
    LogCanCommitOriginAndUrlFailureReason("cpspi_disallowed_commit");
    return can_commit_status;
  }

  const auto origin_tuple_or_precursor_tuple =
      origin.GetTupleOrPrecursorTupleIfOpaque();
  if (origin_tuple_or_precursor_tuple.IsValid()) {
    // Verify that the origin/precursor is allowed to commit in this process.
    // Note: This also handles non-standard cases for |url|, such as
    // about:blank, data, and blob URLs.

    // Renderer-debug URLs can never be committed.
    if (IsRendererDebugURL(origin_tuple_or_precursor_tuple.GetURL())) {
      LogCanCommitOriginAndUrlFailureReason(
          "origin_or_precursor_is_renderer_debug_url");
      return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
    }

    // Give the client a chance to disallow origin URLs from committing.
    // TODO(acolwell): Fix this code to work with opaque origins. Currently
    // some opaque origin precursors, like chrome-extension schemes, can trigger
    // the commit to fail. These need to be investigated.
    if (!origin.opaque() && !GetContentClient()->browser()->CanCommitURL(
                                GetProcess(), origin.GetURL())) {
      LogCanCommitOriginAndUrlFailureReason(
          "content_client_disallowed_commit_origin");
      return CanCommitStatus::CANNOT_COMMIT_ORIGIN;
    }
  }

  return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
}

void RenderFrameHostImpl::NavigateToInterstitialURL(const GURL& data_url) {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::NavigateToInterstitialURL",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());
  DCHECK(data_url.SchemeIs(url::kDataScheme));
  NavigationDownloadPolicy download_policy;
  download_policy.SetDisallowed(NavigationDownloadType::kInterstitial);

  auto common_params = mojom::CommonNavigationParams::New(
      data_url, base::nullopt, blink::mojom::Referrer::New(),
      ui::PAGE_TRANSITION_LINK, mojom::NavigationType::DIFFERENT_DOCUMENT,
      download_policy, false, GURL(), GURL(), PREVIEWS_OFF,
      base::TimeTicks::Now(), "GET", nullptr,
      network::mojom::SourceLocation::New(),
      false /* started_from_context_menu */, false /* has_user_gesture */,
      CreateInitiatorCSPInfo(), std::vector<int>(), std::string(),
      false /* is_history_navigation_in_new_child_frame */, base::TimeTicks());
  CommitNavigation(nullptr /* navigation_request */, std::move(common_params),
                   CreateCommitNavigationParams(), nullptr /* response_head */,
                   mojo::ScopedDataPipeConsumerHandle(),
                   network::mojom::URLLoaderClientEndpointsPtr(), false,
                   base::nullopt, base::nullopt /* subresource_overrides */,
                   nullptr /* provider_info */,
                   base::UnguessableToken::Create() /* not traced */,
                   nullptr /* web_bundle_factory */);
}

void RenderFrameHostImpl::Stop() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::Stop", "frame_tree_node",
               frame_tree_node_->frame_tree_node_id());
  GetAssociatedLocalFrame()->StopLoading();
}

void RenderFrameHostImpl::DispatchBeforeUnload(BeforeUnloadType type,
                                               bool is_reload) {
  bool for_navigation =
      type == BeforeUnloadType::BROWSER_INITIATED_NAVIGATION ||
      type == BeforeUnloadType::RENDERER_INITIATED_NAVIGATION;
  bool for_inner_delegate_attach =
      type == BeforeUnloadType::INNER_DELEGATE_ATTACH;
  DCHECK(for_navigation || for_inner_delegate_attach || !is_reload);

  // TAB_CLOSE and DISCARD should only dispatch beforeunload on main frames.
  DCHECK(type == BeforeUnloadType::BROWSER_INITIATED_NAVIGATION ||
         type == BeforeUnloadType::RENDERER_INITIATED_NAVIGATION ||
         type == BeforeUnloadType::INNER_DELEGATE_ATTACH ||
         frame_tree_node_->IsMainFrame());

  if (!for_navigation) {
    // Cancel any pending navigations, to avoid their navigation commit/fail
    // event from wiping out the is_waiting_for_beforeunload_completion_ state.
    if (frame_tree_node_->navigation_request() &&
        frame_tree_node_->navigation_request()->IsNavigationStarted()) {
      frame_tree_node_->navigation_request()->set_net_error(net::ERR_ABORTED);
    }
    frame_tree_node_->ResetNavigationRequest(false);
  }

  // In renderer-initiated navigations, don't check for beforeunload in the
  // navigating frame, as it has already run beforeunload before it sent the
  // BeginNavigation IPC.
  bool check_subframes_only =
      type == BeforeUnloadType::RENDERER_INITIATED_NAVIGATION;
  if (!ShouldDispatchBeforeUnload(check_subframes_only)) {
    // When running beforeunload for navigations, ShouldDispatchBeforeUnload()
    // is checked earlier and we would only get here if it had already returned
    // true.
    DCHECK(!for_navigation);

    // Dispatch the ACK to prevent re-entrancy.
    base::OnceClosure task = base::BindOnce(
        [](base::WeakPtr<RenderFrameHostImpl> self) {
          if (!self)
            return;
          self->frame_tree_node_->render_manager()->BeforeUnloadCompleted(
              true, base::TimeTicks::Now());
        },
        weak_ptr_factory_.GetWeakPtr());
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(task));
    return;
  }
  TRACE_EVENT_ASYNC_BEGIN1("navigation", "RenderFrameHostImpl BeforeUnload",
                           this, "&RenderFrameHostImpl", (void*)this);

  // This may be called more than once (if the user clicks the tab close button
  // several times, or if they click the tab close button then the browser close
  // button), and we only send the message once.
  if (is_waiting_for_beforeunload_completion_) {
    // Some of our close messages could be for the tab, others for cross-site
    // transitions. We always want to think it's for closing the tab if any
    // of the messages were, since otherwise it might be impossible to close
    // (if there was a cross-site "close" request pending when the user clicked
    // the close button). We want to keep the "for cross site" flag only if
    // both the old and the new ones are also for cross site.
    unload_ack_is_for_navigation_ =
        unload_ack_is_for_navigation_ && for_navigation;
  } else {
    // Start the hang monitor in case the renderer hangs in the beforeunload
    // handler.
    is_waiting_for_beforeunload_completion_ = true;
    beforeunload_dialog_request_cancels_unload_ = false;
    unload_ack_is_for_navigation_ = for_navigation;
    send_before_unload_start_time_ = base::TimeTicks::Now();
    if (render_view_host_->GetDelegate()->IsJavaScriptDialogShowing()) {
      // If there is a JavaScript dialog up, don't bother sending the renderer
      // the unload event because it is known unresponsive, waiting for the
      // reply from the dialog. If this incoming request is for a DISCARD be
      // sure to reply with |proceed = false|, because the presence of a dialog
      // indicates that the page can't be discarded.
      SimulateBeforeUnloadCompleted(type != BeforeUnloadType::DISCARD);
    } else {
      // Start a timer that will be shared by all frames that need to run
      // beforeunload in the current frame's subtree.
      if (beforeunload_timeout_)
        beforeunload_timeout_->Start(beforeunload_timeout_delay_);

      beforeunload_pending_replies_.clear();
      beforeunload_dialog_request_cancels_unload_ =
          (type == BeforeUnloadType::DISCARD);

      // Run beforeunload in this frame and its cross-process descendant
      // frames, in parallel.
      CheckOrDispatchBeforeUnloadForSubtree(check_subframes_only,
                                            true /* send_ipc */, is_reload);
    }
  }
}

bool RenderFrameHostImpl::CheckOrDispatchBeforeUnloadForSubtree(
    bool subframes_only,
    bool send_ipc,
    bool is_reload) {
  bool found_beforeunload = false;
  for (FrameTreeNode* node :
       frame_tree_node_->frame_tree()->SubtreeNodes(frame_tree_node_)) {
    RenderFrameHostImpl* rfh = node->current_frame_host();

    // If |subframes_only| is true, skip this frame and its same-site
    // descendants.  This happens for renderer-initiated navigations, where
    // these frames have already run beforeunload.
    if (subframes_only && rfh->GetSiteInstance() == GetSiteInstance())
      continue;

    // No need to run beforeunload if the RenderFrame isn't live.
    if (!rfh->IsRenderFrameLive())
      continue;

    // Only run beforeunload in frames that have registered a beforeunload
    // handler.
    bool should_run_beforeunload = rfh->has_before_unload_handler_;
    // TODO(alexmos): Many tests, as well as some DevTools cases, currently
    // assume that beforeunload for a navigating/closing frame is always sent
    // to the renderer. For now, keep this assumption by checking |rfh ==
    // this|. In the future, this condition should be removed, and beforeunload
    // should only be sent when a handler is registered.  For subframes of a
    // navigating/closing frame, this assumption was never present, so
    // subframes are included only if they have registered a beforeunload
    // handler.
    if (rfh == this)
      should_run_beforeunload = true;

    if (!should_run_beforeunload)
      continue;

    // If we're only checking whether there's at least one frame with
    // beforeunload, then we've just found one, so we can return now.
    found_beforeunload = true;
    if (!send_ipc)
      return true;

    // Otherwise, figure out whether we need to send the IPC, or whether this
    // beforeunload was already triggered by an ancestor frame's IPC.

    // Only send beforeunload to local roots, and let Blink handle any
    // same-site frames under them. That is, if a frame has a beforeunload
    // handler, ask its local root to run it. If we've already sent the message
    // to that local root, skip this frame. For example, in A1(A2,A3), if A2
    // and A3 contain beforeunload handlers, and all three frames are
    // same-site, we ask A1 to run beforeunload for all three frames, and only
    // ask it once.
    while (!rfh->is_local_root() && rfh != this)
      rfh = rfh->GetParent();
    if (base::Contains(beforeunload_pending_replies_, rfh))
      continue;

    // For a case like A(B(A)), it's not necessary to send an IPC for the
    // innermost frame, as Blink will walk all same-site (local)
    // descendants. Detect cases like this and skip them.
    bool has_same_site_ancestor = false;
    for (auto* added_rfh : beforeunload_pending_replies_) {
      if (rfh->IsDescendantOf(added_rfh) &&
          rfh->GetSiteInstance() == added_rfh->GetSiteInstance()) {
        has_same_site_ancestor = true;
        break;
      }
    }
    if (has_same_site_ancestor)
      continue;

    // Add |rfh| to the list of frames that need to receive beforeunload
    // ACKs.
    beforeunload_pending_replies_.insert(rfh);

    SendBeforeUnload(is_reload, rfh->GetWeakPtr());
  }

  return found_beforeunload;
}

void RenderFrameHostImpl::SimulateBeforeUnloadCompleted(bool proceed) {
  DCHECK(is_waiting_for_beforeunload_completion_);
  base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;

  // Dispatch the ACK to prevent re-entrancy.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderFrameHostImpl::ProcessBeforeUnloadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), proceed,
                     true /* treat_as_final_completion_callback */,
                     approx_renderer_start_time, base::TimeTicks::Now()));
}

bool RenderFrameHostImpl::ShouldDispatchBeforeUnload(
    bool check_subframes_only) {
  return CheckOrDispatchBeforeUnloadForSubtree(
      check_subframes_only, false /* send_ipc */, false /* is_reload */);
}

void RenderFrameHostImpl::SetBeforeUnloadTimeoutDelayForTesting(
    const base::TimeDelta& timeout) {
  beforeunload_timeout_delay_ = timeout;
}

void RenderFrameHostImpl::StartPendingDeletionOnSubtree() {
  ResetNavigationsForPendingDeletion();

  DCHECK(IsPendingDeletion());
  for (std::unique_ptr<FrameTreeNode>& child_frame : children_) {
    for (FrameTreeNode* node :
         frame_tree_node_->frame_tree()->SubtreeNodes(child_frame.get())) {
      RenderFrameHostImpl* child = node->current_frame_host();
      if (child->IsPendingDeletion())
        continue;

      // Blink handles deletion of all same-process descendants, running their
      // unload handler if necessary. So delegate sending IPC on the topmost
      // ancestor using the same process.
      RenderFrameHostImpl* local_ancestor = child;
      for (auto* rfh = child->parent_; rfh != parent_; rfh = rfh->parent_) {
        if (rfh->GetSiteInstance() == child->GetSiteInstance())
          local_ancestor = rfh;
      }

      local_ancestor->DeleteRenderFrame(FrameDeleteIntention::kNotMainFrame);
      if (local_ancestor != child) {
        LifecycleState child_lifecycle_state =
            child->has_unload_handler() ? LifecycleState::kRunningUnloadHandlers
                                        : LifecycleState::kReadyToBeDeleted;
        child->SetLifecycleState(child_lifecycle_state);
      }

      node->frame_tree()->FrameUnloading(node);
    }
  }
}

void RenderFrameHostImpl::PendingDeletionCheckCompleted() {
  if (lifecycle_state_ == LifecycleState::kReadyToBeDeleted &&
      children_.empty()) {
    if (is_waiting_for_unload_ack_)
      OnUnloaded();
    else
      parent_->RemoveChild(frame_tree_node_);
  }
}

void RenderFrameHostImpl::PendingDeletionCheckCompletedOnSubtree() {
  if (children_.empty()) {
    PendingDeletionCheckCompleted();
    return;
  }

  // Collect children first before calling PendingDeletionCheckCompleted() on
  // them, because it may delete them.
  std::vector<RenderFrameHostImpl*> children_rfh;
  for (std::unique_ptr<FrameTreeNode>& child : children_)
    children_rfh.push_back(child->current_frame_host());

  for (RenderFrameHostImpl* child_rfh : children_rfh)
    child_rfh->PendingDeletionCheckCompletedOnSubtree();
}

void RenderFrameHostImpl::ResetNavigationsForPendingDeletion() {
  for (auto& child : children_)
    child->current_frame_host()->ResetNavigationsForPendingDeletion();
  ResetNavigationRequests();
  frame_tree_node_->ResetNavigationRequest(false);
  frame_tree_node_->render_manager()->CleanUpNavigation();
}

void RenderFrameHostImpl::OnUnloadTimeout() {
  DCHECK(IsPendingDeletion());
  parent_->RemoveChild(frame_tree_node_);
}

void RenderFrameHostImpl::UpdateOpener() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::UpdateOpener",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  // This frame (the frame whose opener is being updated) might not have had
  // proxies for the new opener chain in its SiteInstance.  Make sure they
  // exist.
  if (frame_tree_node_->opener()) {
    frame_tree_node_->opener()->render_manager()->CreateOpenerProxies(
        GetSiteInstance(), frame_tree_node_);
  }

  int opener_routing_id =
      frame_tree_node_->render_manager()->GetOpenerRoutingID(GetSiteInstance());
  Send(new FrameMsg_UpdateOpener(GetRoutingID(), opener_routing_id));
}

void RenderFrameHostImpl::SetFocusedFrame() {
  GetAssociatedLocalFrame()->Focus();
}

void RenderFrameHostImpl::AdvanceFocus(blink::mojom::FocusType type,
                                       RenderFrameProxyHost* source_proxy) {
  DCHECK(!source_proxy ||
         (source_proxy->GetProcess()->GetID() == GetProcess()->GetID()));

  base::Optional<base::UnguessableToken> frame_token;
  if (source_proxy)
    frame_token = source_proxy->GetFrameToken();

  GetAssociatedLocalFrame()->AdvanceFocusInFrame(type, frame_token);
}

void RenderFrameHostImpl::JavaScriptDialogClosed(
    JavaScriptDialogCallback dialog_closed_callback,
    bool success,
    const base::string16& user_input) {
  GetProcess()->SetBlocked(false);
  std::move(dialog_closed_callback).Run(success, user_input);
  // If executing as part of beforeunload event handling, there may have been
  // timers stopped in this frame or a frame up in the frame hierarchy. Restart
  // any timers that were stopped in OnRunBeforeUnloadConfirm().
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->is_waiting_for_beforeunload_completion_ &&
        frame->beforeunload_timeout_) {
      frame->beforeunload_timeout_->Start(beforeunload_timeout_delay_);
    }
  }
}

void RenderFrameHostImpl::CommitNavigation(
    NavigationRequest* navigation_request,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    bool is_view_source,
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
    const base::UnguessableToken& devtools_navigation_token,
    std::unique_ptr<WebBundleHandle> web_bundle_handle) {
  web_bundle_handle_ = std::move(web_bundle_handle);

  TRACE_EVENT2("navigation", "RenderFrameHostImpl::CommitNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(), "url",
               common_params->url.possibly_invalid_spec());
  DCHECK(!IsRendererDebugURL(common_params->url));

  bool is_mhtml_iframe =
      navigation_request && navigation_request->IsForMhtmlSubframe();

  // A |response| and a |url_loader_client_endpoints| must always be provided,
  // except for edge cases, where another way to load the document exist.
  DCHECK((response_head && url_loader_client_endpoints) ||
         common_params->url.SchemeIs(url::kDataScheme) ||
         NavigationTypeUtils::IsSameDocument(common_params->navigation_type) ||
         !IsURLHandledByNetworkStack(common_params->url) || is_mhtml_iframe);

  // All children of MHTML documents must be MHTML documents.
  // As a defensive measure, crash the browser if something went wrong.
  if (!frame_tree_node()->IsMainFrame()) {
    RenderFrameHostImpl* root = GetMainFrame();
    if (root->is_mhtml_document_ &&
        !common_params->url.SchemeIs(url::kDataScheme)) {
      bool loaded_from_outside_the_archive =
          response_head || url_loader_client_endpoints;
      CHECK(!loaded_from_outside_the_archive);
      CHECK(is_mhtml_iframe);
      CHECK_EQ(GetSiteInstance(), root->GetSiteInstance());
      CHECK_EQ(GetProcess(), root->GetProcess());
    } else {
      DCHECK(!is_mhtml_iframe);
    }
  }

  bool is_srcdoc = common_params->url.IsAboutSrcdoc();
  if (is_srcdoc) {
    // Main frame srcdoc navigation are meaningless. They are blocked whenever a
    // navigation attempt is made. It shouldn't reach CommitNavigation.
    CHECK(!frame_tree_node_->IsMainFrame());

    // An about:srcdoc document is always same SiteInstance with its parent.
    // Otherwise, it won't be able to load. The parent's document contains the
    // iframe and its srcdoc attribute.
    CHECK_EQ(GetSiteInstance(), parent_->GetSiteInstance());
  }

  // If this is an attempt to commit a URL in an incompatible process, capture a
  // crash dump to diagnose why it is occurring.
  // TODO(creis): Remove this check after we've gathered enough information to
  // debug issues with browser-side security checks. https://crbug.com/931895.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  const GURL& lock_url = GetSiteInstance()->lock_url();
  if (lock_url != GURL(kUnreachableWebDataURL) &&
      common_params->url.IsStandard() &&
      !policy->CanAccessDataForOrigin(GetProcess()->GetID(),
                                      common_params->url) &&
      !is_mhtml_iframe) {
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("lock_url",
                                            base::debug::CrashKeySize::Size64),
        lock_url.possibly_invalid_spec());
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("commit_origin",
                                            base::debug::CrashKeySize::Size64),
        common_params->url.GetOrigin().spec());
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_main_frame",
                                            base::debug::CrashKeySize::Size32),
        frame_tree_node_->IsMainFrame() ? "true" : "false");
    NOTREACHED() << "Commiting in incompatible process for URL: " << lock_url
                 << " lock vs " << common_params->url.GetOrigin();
    base::debug::DumpWithoutCrashing();
  }

  const bool is_first_navigation = !has_committed_any_navigation_;
  has_committed_any_navigation_ = true;

  UpdatePermissionsForNavigation(*common_params, *commit_params);

  // Get back to a clean state, in case we start a new navigation without
  // completing an unload handler.
  ResetWaitingState();

  // The renderer can exit view source mode when any error or cancellation
  // happen. When reusing the same renderer, overwrite to recover the mode.
  if (is_view_source && IsCurrent()) {
    DCHECK(!GetParent());
    GetAssociatedLocalFrame()->EnableViewSourceMode();
  }

  // TODO(lfg): The renderer is not able to handle a null response, so the
  // browser provides an empty response instead. See the DCHECK in the beginning
  // of this method for the edge cases where a response isn't provided.
  network::mojom::URLResponseHeadPtr head =
      response_head ? std::move(response_head)
                    : network::mojom::URLResponseHead::New();
  const bool is_same_document =
      NavigationTypeUtils::IsSameDocument(common_params->navigation_type);

  // TODO(crbug.com/979296): Consider changing this code to copy an origin
  // instead of creating one from a URL which lacks opacity information.
  url::Origin main_world_origin_for_url_loader_factory =
      GetOriginForURLLoaderFactory(navigation_request);

  // IsolationInfo should be filled before the URLLoaderFactory for
  // sub-resources is created. Only update for cross document navigations since
  // for opaque origin same document navigations, a new origin should not be
  // created as that would be different from the original.
  if (!is_same_document) {
    isolation_info_ = ComputeIsolationInfoInternal(
        main_world_origin_for_url_loader_factory,
        net::IsolationInfo::RedirectMode::kUpdateNothing);
  }
  DCHECK(!isolation_info_.IsEmpty());

  if (navigation_request && navigation_request->appcache_handle()) {
    // AppCache may create a subresource URLLoaderFactory later, so make sure it
    // has the correct origin to use when calling
    // ContentBrowserClient::WillCreateURLLoaderFactory().
    navigation_request->appcache_handle()
        ->host()
        ->set_origin_for_url_loader_factory(
            main_world_origin_for_url_loader_factory);
  }
  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories;
  if ((!is_same_document || is_first_navigation) && !is_srcdoc) {
    recreate_default_url_loader_factory_after_network_service_crash_ = false;
    subresource_loader_factories =
        std::make_unique<blink::PendingURLLoaderFactoryBundle>();
    BrowserContext* browser_context = GetSiteInstance()->GetBrowserContext();

    // NOTE: On Network Service navigations, we want to ensure that a frame is
    // given everything it will need to load any accessible subresources. We
    // however only do this for cross-document navigations, because the
    // alternative would be redundant effort.
    if (subresource_loader_params &&
        subresource_loader_params->pending_appcache_loader_factory.is_valid()) {
      // If the caller has supplied a factory for AppCache, use it.
      mojo::Remote<network::mojom::URLLoaderFactory> appcache_remote(std::move(
          subresource_loader_params->pending_appcache_loader_factory));

      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          appcache_proxied_remote;
      auto appcache_proxied_receiver =
          appcache_proxied_remote.InitWithNewPipeAndPassReceiver();
      bool use_proxy =
          GetContentClient()->browser()->WillCreateURLLoaderFactory(
              browser_context, this, GetProcess()->GetID(),
              ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
              main_world_origin_for_url_loader_factory,
              base::nullopt /* navigation_id */, &appcache_proxied_receiver,
              nullptr /* header_client */, nullptr /* bypass_redirect_checks */,
              nullptr /* disable_secure_dns */, nullptr /* factory_override */);
      if (use_proxy) {
        appcache_remote->Clone(std::move(appcache_proxied_receiver));
        appcache_remote.reset();
        appcache_remote.Bind(std::move(appcache_proxied_remote));
      }

      subresource_loader_factories->pending_appcache_factory() =
          appcache_remote.Unbind();
    }

    non_network_url_loader_factories_.clear();

    // Set up the default factory.
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_default_factory;

    // See if this is for WebUI.
    std::string scheme = common_params->url.scheme();
    const auto& webui_schemes = URLDataManagerBackend::GetWebUISchemes();
    if (base::Contains(webui_schemes, scheme)) {
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_for_webui;
      auto factory_receiver =
          factory_for_webui.InitWithNewPipeAndPassReceiver();
      GetContentClient()->browser()->WillCreateURLLoaderFactory(
          browser_context, this, GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
          main_world_origin_for_url_loader_factory,
          base::nullopt /* navigation_id */, &factory_receiver,
          nullptr /* header_client */, nullptr /* bypass_redirect_checks */,
          nullptr /* disable_secure_dns */, nullptr /* factory_override */);
      CreateWebUIURLLoaderBinding(this, scheme, std::move(factory_receiver));
      // If the renderer has webui bindings, then don't give it access to
      // network loader for security reasons.
      // http://crbug.com/829412: make an exception for a small whitelist
      // of WebUIs that need to be fixed to not make network requests in JS.
      if ((enabled_bindings_ & kWebUIBindingsPolicyMask) &&
          !GetContentClient()->browser()->IsWebUIAllowedToMakeNetworkRequests(
              url::Origin::Create(common_params->url.GetOrigin()))) {
        pending_default_factory = std::move(factory_for_webui);
        // WebUIURLLoaderFactory will kill the renderer if it sees a request
        // with a non-chrome scheme. Register a URLLoaderFactory for the about
        // scheme so about:blank doesn't kill the renderer.
        non_network_url_loader_factories_[url::kAboutScheme] =
            std::make_unique<AboutURLLoaderFactory>();
      } else {
        // This is a webui scheme that doesn't have webui bindings. Give it
        // access to the network loader as it might require it.
        subresource_loader_factories->pending_scheme_specific_factories()
            .emplace(scheme, std::move(factory_for_webui));
      }
    }

    if (!pending_default_factory) {
      // Otherwise default to a Network Service-backed loader from the
      // appropriate NetworkContext.
      // TODO(clamy): Always use the NavigationRequest's ClientSecurityState
      // when all interstitials are committed and we are guaranteed to have a
      // NavigationRequest in this function.
      recreate_default_url_loader_factory_after_network_service_crash_ = true;
      CrossOriginEmbedderPolicyReporter* const coep_reporter =
          navigation_request ? navigation_request->coep_reporter() : nullptr;
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter_remote;
      if (coep_reporter) {
        coep_reporter->Clone(
            coep_reporter_remote.InitWithNewPipeAndPassReceiver());
      }
      bool bypass_redirect_checks =
          CreateNetworkServiceDefaultFactoryAndObserve(
              CreateURLLoaderFactoryParamsForMainWorld(
                  main_world_origin_for_url_loader_factory,
                  navigation_request
                      ? mojo::Clone(navigation_request->client_security_state())
                      : network::mojom::ClientSecurityState::New(),
                  std::move(coep_reporter_remote)),
              pending_default_factory.InitWithNewPipeAndPassReceiver());
      subresource_loader_factories->set_bypass_redirect_checks(
          bypass_redirect_checks);
    }

    bool navigation_to_web_bundle = false;

    if (web_bundle_handle_ && web_bundle_handle_->IsReadyForLoading()) {
      navigation_to_web_bundle = true;
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory(
          std::move(pending_default_factory));
      web_bundle_handle_->CreateURLLoaderFactory(
          pending_default_factory.InitWithNewPipeAndPassReceiver(),
          std::move(fallback_factory));
      DCHECK(web_bundle_handle_->navigation_info());
      commit_params->web_bundle_physical_url =
          web_bundle_handle_->navigation_info()->source().url();
      if (web_bundle_handle_->claimed_url().is_valid()) {
        commit_params->web_bundle_claimed_url =
            web_bundle_handle_->claimed_url();
      }
    }

    DCHECK(pending_default_factory);
    subresource_loader_factories->pending_default_factory() =
        std::move(pending_default_factory);

    // Only file resources and about:blank with an initiator that can load files
    // can load file subresources.
    //
    // Other URLs like about:srcdoc might be able load files, but only because
    // they will inherit loaders from their parents instead of the ones
    // provided by the browser process here.
    // TODO(crbug.com/949510): Make about:srcdoc also use this path instead of
    // inheriting loaders from the parent.
    //
    // For loading Web Bundle files, we don't set FileURLLoaderFactory.
    // Because loading local files from a Web Bundle file is prohibited.
    //
    // TODO(crbug.com/888079): In the future, use
    // GetOriginForURLLoaderFactory/GetOriginToCommit.
    if ((common_params->url.SchemeIsFile() ||
         (common_params->url.IsAboutBlank() &&
          common_params->initiator_origin &&
          common_params->initiator_origin->scheme() == url::kFileScheme)) &&
        !navigation_to_web_bundle) {
      auto file_factory = std::make_unique<FileURLLoaderFactory>(
          browser_context->GetPath(),
          browser_context->GetSharedCorsOriginAccessList(),
          // A user-initiated navigation is USER_BLOCKING.
          base::TaskPriority::USER_BLOCKING);
      non_network_url_loader_factories_.emplace(url::kFileScheme,
                                                std::move(file_factory));
    }

#if defined(OS_ANDROID)
    if (common_params->url.SchemeIs(url::kContentScheme)) {
      // Only content:// URLs can load content:// subresources
      auto content_factory = std::make_unique<ContentURLLoaderFactory>(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
      non_network_url_loader_factories_.emplace(url::kContentScheme,
                                                std::move(content_factory));
    }
#endif

    StoragePartition* partition =
        BrowserContext::GetStoragePartition(browser_context, GetSiteInstance());
    std::string storage_domain;
    if (site_instance_) {
      std::string partition_name;
      bool in_memory;
      GetContentClient()->browser()->GetStoragePartitionConfigForSite(
          browser_context, site_instance_->GetSiteURL(), true, &storage_domain,
          &partition_name, &in_memory);
    }
    non_network_url_loader_factories_.emplace(
        url::kFileSystemScheme,
        content::CreateFileSystemURLLoaderFactory(
            process_->GetID(), GetFrameTreeNodeId(),
            partition->GetFileSystemContext(), storage_domain));

    non_network_url_loader_factories_.emplace(
        url::kDataScheme, std::make_unique<DataURLLoaderFactory>());

    GetContentClient()
        ->browser()
        ->RegisterNonNetworkSubresourceURLLoaderFactories(
            process_->GetID(), routing_id_, &non_network_url_loader_factories_);

    for (auto& factory : non_network_url_loader_factories_) {
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_factory_proxy;
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver =
          pending_factory_proxy.InitWithNewPipeAndPassReceiver();
      GetContentClient()->browser()->WillCreateURLLoaderFactory(
          browser_context, this, GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
          main_world_origin_for_url_loader_factory,
          base::nullopt /* navigation_id */, &factory_receiver,
          nullptr /* header_client */, nullptr /* bypass_redirect_checks */,
          nullptr /* disable_secure_dns */, nullptr /* factory_override */);
      // Keep DevTools proxy last, i.e. closest to the network.
      devtools_instrumentation::WillCreateURLLoaderFactory(
          this, false /* is_navigation */, false /* is_download */,
          &factory_receiver, nullptr /* factory_override */);
      factory.second->Clone(std::move(factory_receiver));
      subresource_loader_factories->pending_scheme_specific_factories().emplace(
          factory.first, std::move(pending_factory_proxy));
    }

    // TODO(clamy): Always use the NavigationRequest's ClientSecurityState
    // when all interstitials are committed and we are guaranteed to have a
    // NavigationRequest in this function.
    subresource_loader_factories->pending_isolated_world_factories() =
        CreateURLLoaderFactoriesForIsolatedWorlds(
            main_world_origin_for_url_loader_factory,
            isolated_worlds_requiring_separate_url_loader_factory_,
            navigation_request
                ? mojo::Clone(navigation_request->client_security_state())
                : network::mojom::ClientSecurityState::New());
  }

  // It is imperative that cross-document navigations always provide a set of
  // subresource ULFs.
  DCHECK(is_same_document || !is_first_navigation || is_srcdoc ||
         subresource_loader_factories);

  if (is_same_document) {
    bool should_replace_current_entry =
        common_params->should_replace_current_entry;
    DCHECK(same_document_navigation_request_);
    GetNavigationControl()->CommitSameDocumentNavigation(
        std::move(common_params), std::move(commit_params),
        base::BindOnce(&RenderFrameHostImpl::OnSameDocumentCommitProcessed,
                       base::Unretained(this),
                       same_document_navigation_request_->GetNavigationId(),
                       should_replace_current_entry));
  } else {
    // Pass the controller service worker info if we have one.
    blink::mojom::ControllerServiceWorkerInfoPtr controller;
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerObject>
        remote_object;
    blink::mojom::ServiceWorkerState sent_state;
    if (subresource_loader_params &&
        subresource_loader_params->controller_service_worker_info) {
      controller =
          std::move(subresource_loader_params->controller_service_worker_info);
      if (controller->object_info) {
        controller->object_info->receiver =
            remote_object.InitWithNewEndpointAndPassReceiver();
        sent_state = controller->object_info->state;
      }
    }

    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        factory_bundle_for_prefetch;
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory;
    if (subresource_loader_factories) {
      // Clone the factory bundle for prefetch.
      auto bundle = base::MakeRefCounted<blink::URLLoaderFactoryBundle>(
          std::move(subresource_loader_factories));
      subresource_loader_factories = CloneFactoryBundle(bundle);
      factory_bundle_for_prefetch = CloneFactoryBundle(bundle);
    }

    if (factory_bundle_for_prefetch) {
      if (prefetched_signed_exchange_cache_) {
        prefetched_signed_exchange_cache_->RecordHistograms();
        // Reset |prefetched_signed_exchange_cache_|, not to reuse the cached
        // signed exchange which was prefetched in the previous page.
        prefetched_signed_exchange_cache_.reset();
      }

      // Also set-up URLLoaderFactory for prefetch using the same loader
      // factories. TODO(kinuko): Consider setting this up only when prefetch
      // is used. Currently we have this here to make sure we have non-racy
      // situation (https://crbug.com/849929).
      auto* storage_partition = static_cast<StoragePartitionImpl*>(
          BrowserContext::GetStoragePartition(
              GetSiteInstance()->GetBrowserContext(), GetSiteInstance()));
      storage_partition->GetPrefetchURLLoaderService()->GetFactory(
          prefetch_loader_factory.InitWithNewPipeAndPassReceiver(),
          frame_tree_node_->frame_tree_node_id(),
          std::move(factory_bundle_for_prefetch),
          weak_ptr_factory_.GetWeakPtr(),
          EnsurePrefetchedSignedExchangeCache());
    }

    mojom::NavigationClient* navigation_client = nullptr;
    if (navigation_request)
      navigation_client = navigation_request->GetCommitNavigationClient();

    // Record the metrics about the state of the old main frame at the moment
    // when we navigate away from it as it matters for whether the page
    // is eligible for being put into back-forward cache.
    //
    // Ideally we would do this when we are just about to swap out the old
    // render frame and swap in the new one, but we can't do this for
    // same-process navigations yet as we are reusing the RenderFrameHost and
    // as the local frame navigates it overrides the values that we are
    // interested in. The cross-process navigation case is handled in
    // RenderFrameHostManager::UnloadOldFrame.
    //
    // Here we are recording the metrics for same-process navigations at the
    // point just before the navigation commits.
    // TODO(altimin, crbug.com/933147): Remove this logic after we are done with
    // implementing back-forward cache.
    if (!GetParent() && frame_tree_node()->current_frame_host() == this) {
      if (NavigationEntryImpl* last_committed_entry =
              NavigationEntryImpl::FromNavigationEntry(
                  frame_tree_node()
                      ->navigator()
                      ->GetController()
                      ->GetLastCommittedEntry())) {
        if (last_committed_entry->back_forward_cache_metrics()) {
          last_committed_entry->back_forward_cache_metrics()
              ->RecordFeatureUsage(this);
        }
      }
    }

    // about:srcdoc "inherits" loaders from its parent in the renderer process,
    // There are no need to provide new ones here.
    // TODO(arthursonzogni): What about about:blank URLs?
    // TODO(arthursonzogni): What about data-URLs?
    //
    // Note: Inheriting loaders could be done in the browser process, but we
    //       aren't confident there are enough reliable information in the
    //       browser process to always make the correct decision. "Inheriting"
    //       in the renderer process is slightly less problematic in that it
    //       guarantees the renderer won't have higher privileges than it
    //       originally had (since it will inherit loader factories it already
    //       had access to).
    if (is_srcdoc) {
      DCHECK(!subresource_loader_factories);
      DCHECK(!subresource_overrides);
      DCHECK(!prefetch_loader_factory);
    }

    // If a network request was made, update the Previews state.
    if (IsURLHandledByNetworkStack(common_params->url))
      last_navigation_previews_state_ = common_params->previews_state;

    dom_content_loaded_ = false;
    SendCommitNavigation(
        navigation_client, navigation_request, std::move(common_params),
        std::move(commit_params), std::move(head), std::move(response_body),
        std::move(url_loader_client_endpoints),
        std::move(subresource_loader_factories),
        std::move(subresource_overrides), std::move(controller),
        std::move(provider_info), std::move(prefetch_loader_factory),
        devtools_navigation_token);

    // |remote_object| is an associated interface ptr, so calls can't be made on
    // it until its request endpoint is sent. Now that the request endpoint was
    // sent, it can be used, so add it to ServiceWorkerObjectHost.
    if (remote_object.is_valid()) {
      RunOrPostTaskOnThread(
          FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
          base::BindOnce(
              &ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState,
              subresource_loader_params->controller_service_worker_object_host,
              std::move(remote_object), sent_state));
    }
  }

  is_loading_ = true;
}

void RenderFrameHostImpl::FailedNavigation(
    NavigationRequest* navigation_request,
    const mojom::CommonNavigationParams& common_params,
    const mojom::CommitNavigationParams& commit_params,
    bool has_stale_copy_in_cache,
    int error_code,
    const base::Optional<std::string>& error_page_content) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::FailedNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "error", error_code);

  DCHECK(navigation_request);

  // Update renderer permissions even for failed commits, so that for example
  // the URL bar correctly displays privileged URLs instead of filtering them.
  UpdatePermissionsForNavigation(common_params, commit_params);

  // Get back to a clean state, in case a new navigation started without
  // completing an unload handler.
  ResetWaitingState();

  // Error page will commit in an opaque origin.
  //
  // TODO(lukasza): https://crbug.com/888079: Use this origin when committing
  // later on.
  url::Origin origin = url::Origin();
  isolation_info_ = net::IsolationInfo::CreateTransient();

  std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
      subresource_loader_factories;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> default_factory_remote;
  bool bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
      CreateURLLoaderFactoryParamsForMainWorld(
          origin, mojo::Clone(navigation_request->client_security_state()),
          /*coep_reporter=*/mojo::NullRemote()),
      default_factory_remote.InitWithNewPipeAndPassReceiver());
  subresource_loader_factories =
      std::make_unique<blink::PendingURLLoaderFactoryBundle>(
          std::move(default_factory_remote),
          blink::PendingURLLoaderFactoryBundle::SchemeMap(),
          blink::PendingURLLoaderFactoryBundle::OriginMap(),
          bypass_redirect_checks);

  mojom::NavigationClient* navigation_client =
      navigation_request->GetCommitNavigationClient();

  SendCommitFailedNavigation(
      navigation_client, navigation_request, common_params.Clone(),
      commit_params.Clone(), has_stale_copy_in_cache, error_code,
      error_page_content, std::move(subresource_loader_factories));

  // An error page is expected to commit, hence why is_loading_ is set to true.
  is_loading_ = true;
  dom_content_loaded_ = false;
  DCHECK(navigation_request && navigation_request->IsNavigationStarted() &&
         navigation_request->GetNetErrorCode() != net::OK);
}

void RenderFrameHostImpl::HandleRendererDebugURL(const GURL& url) {
  DCHECK(IsRendererDebugURL(url));

  // Several tests expect a load of Chrome Debug URLs to send a DidStopLoading
  // notification, so set is loading to true here to properly surface it when
  // the renderer process is done handling the URL.
  // TODO(clamy): Remove the test dependency on this behavior.
  if (!url.SchemeIs(url::kJavaScriptScheme)) {
    bool was_loading = frame_tree_node()->frame_tree()->IsLoading();
    is_loading_ = true;
    frame_tree_node()->DidStartLoading(true, was_loading);
  }

  GetNavigationControl()->HandleRendererDebugURL(url);

  // Ensure that the renderer process is marked as used after processing a
  // renderer debug URL, since this process is now unsafe to be reused by sites
  // that require a dedicated process.  Usually this happens at ready-to-commit
  // (NavigationRequest::OnResponseStarted) time for regular navigations, but
  // renderer debug URLs don't go through that path.  This matters for initial
  // navigations to renderer debug URLs.  See https://crbug.com/1074108.
  GetProcess()->SetIsUsed();
}

void RenderFrameHostImpl::SetUpMojoIfNeeded() {
  if (associated_registry_.get())
    return;

  associated_registry_ = std::make_unique<blink::AssociatedInterfaceRegistry>();

  auto bind_frame_host_receiver =
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<mojom::FrameHost> receiver) {
        impl->frame_host_associated_receiver_.Bind(std::move(receiver));
        impl->frame_host_associated_receiver_.SetFilter(
            std::make_unique<ActiveURLMessageFilter>(impl));
      };
  associated_registry_->AddInterface(
      base::BindRepeating(bind_frame_host_receiver, base::Unretained(this)));

  associated_registry_->AddInterface(base::BindRepeating(
      [](RenderFrameHostImpl* self,
         mojo::PendingAssociatedReceiver<blink::mojom::PortalHost> receiver) {
        Portal::BindPortalHostReceiver(self, std::move(receiver));
      },
      base::Unretained(this)));

  associated_registry_->AddInterface(base::BindRepeating(
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<blink::mojom::LocalFrameHost>
             receiver) {
        impl->local_frame_host_receiver_.Bind(std::move(receiver));
        impl->local_frame_host_receiver_.SetFilter(
            std::make_unique<ActiveURLMessageFilter>(impl));
      },
      base::Unretained(this)));

  if (frame_tree_node_->IsMainFrame()) {
    associated_registry_->AddInterface(base::BindRepeating(
        [](RenderFrameHostImpl* impl,
           mojo::PendingAssociatedReceiver<blink::mojom::LocalMainFrameHost>
               receiver) {
          impl->local_main_frame_host_receiver_.Bind(std::move(receiver));
          impl->local_main_frame_host_receiver_.SetFilter(
              std::make_unique<ActiveURLMessageFilter>(impl));
        },
        base::Unretained(this)));

    associated_registry_->AddInterface(base::BindRepeating(
        [](RenderFrameHostImpl* impl,
           mojo::PendingAssociatedReceiver<
               blink::mojom::ManifestUrlChangeObserver> receiver) {
          ManifestManagerHost::GetOrCreateForCurrentDocument(impl)
              ->BindObserver(std::move(receiver));
        },
        base::Unretained(this)));
  }

  associated_registry_->AddInterface(base::BindRepeating(
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<mojom::RenderAccessibilityHost>
             receiver) {
        impl->render_accessibility_host_receiver_.Bind(std::move(receiver));
      },
      base::Unretained(this)));

  // TODO(crbug.com/1047354): How to avoid binding if the
  // BINDINGS_POLICY_DOM_AUTOMATION policy is not set?
  associated_registry_->AddInterface(base::BindRepeating(
      [](RenderFrameHostImpl* impl,
         mojo::PendingAssociatedReceiver<mojom::DomAutomationControllerHost>
             receiver) {
        impl->BindDomOperationControllerHostReceiver(std::move(receiver));
      },
      base::Unretained(this)));

  file_system_manager_.reset(new FileSystemManagerImpl(
      GetProcess()->GetID(),
      GetProcess()->GetStoragePartition()->GetFileSystemContext(),
      ChromeBlobStorageContext::GetFor(GetProcess()->GetBrowserContext())));

  mojo::PendingRemote<mojom::FrameFactory> frame_factory;
  GetProcess()->BindReceiver(frame_factory.InitWithNewPipeAndPassReceiver());
  mojo::Remote<mojom::FrameFactory>(std::move(frame_factory))
      ->CreateFrame(routing_id_, frame_.BindNewPipeAndPassReceiver());

  // TODO(http://crbug.com/1014212): Change to DCHECK.
  CHECK(frame_);

  mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
      remote_interfaces;
  frame_->GetInterfaceProvider(
      remote_interfaces.InitWithNewPipeAndPassReceiver());
  remote_interfaces_.reset(new service_manager::InterfaceProvider);
  remote_interfaces_->Bind(std::move(remote_interfaces));

  remote_interfaces_->GetInterface(
      frame_input_handler_.BindNewPipeAndPassReceiver());
}

void RenderFrameHostImpl::InvalidateMojoConnection() {
  frame_.reset();
  frame_bindings_control_.reset();
  frame_host_associated_receiver_.reset();
  local_frame_.reset();
  local_main_frame_.reset();
  navigation_control_.reset();
  frame_input_handler_.reset();
  find_in_page_.reset();
  render_accessibility_.reset();

  // Disconnect with ImageDownloader Mojo service in Blink.
  mojo_image_downloader_.reset();

  // The geolocation service and sensor provider proxy may attempt to cancel
  // permission requests so they must be reset before the routing_id mapping is
  // removed.
  geolocation_service_.reset();
  sensor_provider_proxy_.reset();

  render_accessibility_host_receiver_.reset();
  local_frame_host_receiver_.reset();
  local_main_frame_host_receiver_.reset();
  associated_registry_.reset();
}

bool RenderFrameHostImpl::IsFocused() {
  if (!GetRenderWidgetHost()->is_focused() || !frame_tree_->GetFocusedFrame())
    return false;

  RenderFrameHostImpl* focused_rfh =
      frame_tree_->GetFocusedFrame()->current_frame_host();
  return focused_rfh == this || focused_rfh->IsDescendantOf(this);
}

bool RenderFrameHostImpl::CreateWebUI(const GURL& dest_url,
                                      int entry_bindings) {
  // Verify expectation that WebUI should not be created for error pages.
  DCHECK_NE(GetSiteInstance()->GetSiteURL(), GURL(kUnreachableWebDataURL));

  WebUI::TypeID new_web_ui_type =
      WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          GetSiteInstance()->GetBrowserContext(), dest_url);
  CHECK_NE(new_web_ui_type, WebUI::kNoWebUI);

  // If |web_ui_| already exists, there is no need to create a new one. However,
  // it is useful to verify that its type hasn't changed. Site isolation
  // guarantees that RenderFrameHostImpl will be changed if the WebUI type
  // differs.
  if (web_ui_) {
    CHECK_EQ(new_web_ui_type, web_ui_type_);
    return false;
  }

  web_ui_ = delegate_->CreateWebUIForRenderFrameHost(dest_url);
  if (!web_ui_)
    return false;

  // If we have assigned (zero or more) bindings to the NavigationEntry in
  // the past, make sure we're not granting it different bindings than it
  // had before. If so, note it and don't give it any bindings, to avoid a
  // potential privilege escalation.
  if (entry_bindings != FrameNavigationEntry::kInvalidBindings &&
      web_ui_->GetBindings() != entry_bindings) {
    RecordAction(base::UserMetricsAction("ProcessSwapBindingsMismatch_RVHM"));
    ClearWebUI();
    return false;
  }

  // It is not expected for GuestView to be able to navigate to WebUI.
  DCHECK(!GetProcess()->IsForGuestsOnly());

  web_ui_type_ = new_web_ui_type;

  // WebUIs need the ability to request certain schemes.
  for (const auto& scheme : web_ui_->GetRequestableSchemes()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantRequestScheme(
        GetProcess()->GetID(), scheme);
  }

  // Since this is new WebUI instance, this RenderFrameHostImpl should not
  // have had any bindings. Verify that and grant the required bindings.
  DCHECK_EQ(0, GetEnabledBindings());
  AllowBindings(web_ui_->GetBindings());

  return true;
}

void RenderFrameHostImpl::ClearWebUI() {
  web_ui_type_ = WebUI::kNoWebUI;
  web_ui_.reset();
}

const mojo::Remote<blink::mojom::ImageDownloader>&
RenderFrameHostImpl::GetMojoImageDownloader() {
  if (!mojo_image_downloader_.is_bound() && GetRemoteInterfaces()) {
    GetRemoteInterfaces()->GetInterface(
        mojo_image_downloader_.BindNewPipeAndPassReceiver());
  }
  return mojo_image_downloader_;
}

const mojo::AssociatedRemote<blink::mojom::FindInPage>&
RenderFrameHostImpl::GetFindInPage() {
  if (!find_in_page_)
    GetRemoteAssociatedInterfaces()->GetInterface(&find_in_page_);
  return find_in_page_;
}

const mojo::AssociatedRemote<blink::mojom::LocalFrame>&
RenderFrameHostImpl::GetAssociatedLocalFrame() {
  if (!local_frame_)
    GetRemoteAssociatedInterfaces()->GetInterface(&local_frame_);
  return local_frame_;
}

const mojo::AssociatedRemote<blink::mojom::LocalMainFrame>&
RenderFrameHostImpl::GetAssociatedLocalMainFrame() {
  DCHECK(frame_tree_node_->IsMainFrame());
  if (!local_main_frame_)
    GetRemoteAssociatedInterfaces()->GetInterface(&local_main_frame_);
  return local_main_frame_;
}

void RenderFrameHostImpl::ResetLoadingState() {
  if (is_loading()) {
    // When pending deletion, just set the loading state to not loading.
    // Otherwise, OnDidStopLoading will take care of that, as well as sending
    // notification to the FrameTreeNode about the change in loading state.
    if (!is_active())
      is_loading_ = false;
    else
      OnDidStopLoading();
  }
}

void RenderFrameHostImpl::ClearFocusedElement() {
  has_focused_editable_element_ = false;
  GetAssociatedLocalFrame()->ClearFocusedElement();
}

void RenderFrameHostImpl::BlockRequestsForFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ForEachFrame(this,
               base::BindRepeating([](RenderFrameHostImpl* render_frame_host) {
                 if (render_frame_host->frame_)
                   render_frame_host->frame_->BlockRequests();
               }));
}

void RenderFrameHostImpl::ResumeBlockedRequestsForFrame() {
  ForEachFrame(this,
               base::BindRepeating([](RenderFrameHostImpl* render_frame_host) {
                 if (render_frame_host->frame_)
                   render_frame_host->frame_->ResumeBlockedRequests();
               }));
}

void RenderFrameHostImpl::CancelBlockedRequestsForFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ForEachFrame(this,
               base::BindRepeating([](RenderFrameHostImpl* render_frame_host) {
                 if (render_frame_host->frame_)
                   render_frame_host->frame_->CancelBlockedRequests();
               }));
}

void RenderFrameHostImpl::BindDevToolsAgent(
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgentHost> host,
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> receiver) {
  GetNavigationControl()->BindDevToolsAgent(std::move(host),
                                            std::move(receiver));
}

bool RenderFrameHostImpl::IsSameSiteInstance(
    RenderFrameHostImpl* other_render_frame_host) {
  // As a sanity check, make sure the frame belongs to the same BrowserContext.
  CHECK_EQ(GetSiteInstance()->GetBrowserContext(),
           other_render_frame_host->GetSiteInstance()->GetBrowserContext());
  return GetSiteInstance() == other_render_frame_host->GetSiteInstance();
}

void RenderFrameHostImpl::UpdateAccessibilityMode() {
  // Don't update accessibility mode for a frame that hasn't been created yet.
  if (!IsRenderFrameCreated())
    return;

  ui::AXMode ax_mode = delegate_->GetAccessibilityMode();
  if (!ax_mode.has_mode(ui::AXMode::kWebContents)) {
    // Resetting the Remote signals the renderer to shutdown accessibility
    // in the renderer.
    render_accessibility_.reset();
    return;
  }

  if (!render_accessibility_) {
    // Render accessibility is not enabled yet, so bind the interface first.
    GetRemoteAssociatedInterfaces()->GetInterface(&render_accessibility_);
  }

  render_accessibility_->SetMode(ax_mode.mode());
}

void RenderFrameHostImpl::RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                                                ui::AXMode ax_mode) {
  // TODO(https://crbug.com/859110): Remove once frame_ can no longer be null.
  if (!IsRenderFrameLive())
    return;

  frame_->SnapshotAccessibilityTree(
      ax_mode.mode(),
      base::BindOnce(&RenderFrameHostImpl::RequestAXTreeSnapshotCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RenderFrameHostImpl::SetAccessibilityCallbackForTesting(
    const AccessibilityCallbackForTesting& callback) {
  accessibility_testing_callback_ = callback;
}

void RenderFrameHostImpl::UpdateAXTreeData() {
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.is_mode_off() || !is_active()) {
    return;
  }

  AXEventNotificationDetails detail;
  detail.ax_tree_id = GetAXTreeID();
  detail.updates.resize(1);
  detail.updates[0].has_tree_data = true;
  AXContentTreeDataToAXTreeData(&detail.updates[0].tree_data);

  SendAccessibilityEventsToManager(detail);
  delegate_->AccessibilityEventReceived(detail);
}

BrowserAccessibilityManager*
RenderFrameHostImpl::GetOrCreateBrowserAccessibilityManager() {
  RenderWidgetHostViewBase* view = GetViewForAccessibility();
  if (view && !browser_accessibility_manager_ &&
      !no_create_browser_accessibility_manager_for_testing_) {
    browser_accessibility_manager_.reset(
        view->CreateBrowserAccessibilityManager(this, is_main_frame()));
  }
  return browser_accessibility_manager_.get();
}

void RenderFrameHostImpl::ActivateFindInPageResultForAccessibility(
    int request_id) {
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager)
      manager->ActivateFindInPageResult(request_id);
  }
}

void RenderFrameHostImpl::InsertVisualStateCallback(
    VisualStateCallback callback) {
  static uint64_t next_id = 1;
  uint64_t key = next_id++;
  Send(new FrameMsg_VisualStateRequest(routing_id_, key));
  visual_state_callbacks_.emplace(key, std::move(callback));
}

bool RenderFrameHostImpl::IsRenderFrameCreated() {
  return render_frame_created_;
}

bool RenderFrameHostImpl::IsRenderFrameLive() {
  bool is_live =
      GetProcess()->IsInitializedAndNotDead() && render_frame_created_;

  // Sanity check: the RenderView should always be live if the RenderFrame is.
  DCHECK(!is_live || render_view_host_->IsRenderViewLive());

  return is_live;
}

bool RenderFrameHostImpl::IsCurrent() {
  RenderFrameHostImpl* rfh = this;
  // Check this RenderFrameHost and all its ancestors to see if they are the
  // current ones in their respective FrameTreeNodes.
  // It is important to check for all ancestors as when navigation commits a new
  // RenderFrameHost may replace one of the parents, swapping out the old with
  // its entire subtree but |this| will still be a current one in its
  // FrameTreeNode.
  while (rfh) {
    if (rfh->frame_tree_node()->current_frame_host() != rfh)
      return false;
    rfh = rfh->GetParent();
  }
  return true;
}

size_t RenderFrameHostImpl::GetProxyCount() {
  if (!IsCurrent())
    return 0;
  return frame_tree_node_->render_manager()->GetProxyCount();
}

bool RenderFrameHostImpl::HasSelection() {
  return has_selection_;
}

RenderFrameHostImpl* RenderFrameHostImpl::GetMainFrame() {
  // Iteration over the GetParent() chain is used below, because returning
  // |frame_tree_node()->frame_tree()->root()->current_frame_host()| might
  // give an incorrect result after |this| has been detached from the frame
  // tree.
  RenderFrameHostImpl* main_frame = this;
  while (main_frame->GetParent())
    main_frame = main_frame->GetParent();
  return main_frame;
}

bool RenderFrameHostImpl::CanAccessFilesOfPageState(const PageState& state) {
  return ChildProcessSecurityPolicyImpl::GetInstance()->CanReadAllFiles(
      GetProcess()->GetID(), state.GetReferencedFiles());
}

void RenderFrameHostImpl::GrantFileAccessFromPageState(const PageState& state) {
  GrantFileAccess(GetProcess()->GetID(), state.GetReferencedFiles());
}

void RenderFrameHostImpl::GrantFileAccessFromResourceRequestBody(
    const network::ResourceRequestBody& body) {
  GrantFileAccess(GetProcess()->GetID(), body.GetReferencedFiles());
}

void RenderFrameHostImpl::UpdatePermissionsForNavigation(
    const mojom::CommonNavigationParams& common_params,
    const mojom::CommitNavigationParams& commit_params) {
  // Browser plugin guests are not allowed to navigate outside web-safe schemes,
  // so do not grant them the ability to commit additional URLs.
  if (!GetProcess()->IsForGuestsOnly()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantCommitURL(
        GetProcess()->GetID(), common_params.url);
    if (IsLoadDataWithBaseURL(common_params)) {
      // When there's a base URL specified for the data URL, we also need to
      // grant access to the base URL. This allows file: and other unexpected
      // schemes to be accepted at commit time and during CORS checks (e.g., for
      // font requests).
      ChildProcessSecurityPolicyImpl::GetInstance()->GrantCommitURL(
          GetProcess()->GetID(), common_params.base_url_for_data_url);
    }
  }

  // We may be returning to an existing NavigationEntry that had been granted
  // file access.  If this is a different process, we will need to grant the
  // access again.  Abuse is prevented, because the files listed in the page
  // state are validated earlier, when they are received from the renderer (in
  // RenderFrameHostImpl::CanAccessFilesOfPageState).
  if (commit_params.page_state.IsValid())
    GrantFileAccessFromPageState(commit_params.page_state);

  // We may be here after transferring navigation to a different renderer
  // process.  In this case, we need to ensure that the new renderer retains
  // ability to access files that the old renderer could access.  Abuse is
  // prevented, because the files listed in ResourceRequestBody are validated
  // earlier, when they are received from the renderer.
  if (common_params.post_data)
    GrantFileAccessFromResourceRequestBody(*common_params.post_data);
}

mojo::AssociatedRemote<mojom::NavigationClient>
RenderFrameHostImpl::GetNavigationClientFromInterfaceProvider() {
  mojo::AssociatedRemote<mojom::NavigationClient> navigation_client_remote;
  GetRemoteAssociatedInterfaces()->GetInterface(&navigation_client_remote);
  return navigation_client_remote;
}

void RenderFrameHostImpl::NavigationRequestCancelled(
    NavigationRequest* navigation_request) {
  OnCrossDocumentCommitProcessed(navigation_request,
                                 blink::mojom::CommitResult::Aborted);
}

url::Origin
RenderFrameHostImpl::GetExpectedMainWorldOriginForUrlLoaderFactory() {
  // Find the most recent NavigationRequest that has triggered a Commit IPC to
  // the renderer process.  Once the renderer process handles the IPC, it may
  // possibly change the origin from |last_committed_origin_| to another origin.
  NavigationRequest* found_request = nullptr;
  for (const auto& it : navigation_requests_) {
    NavigationRequest* candidate = it.first;
    DCHECK_EQ(candidate, it.second.get());

    if (candidate->state() < NavigationRequest::READY_TO_COMMIT)
      continue;
    if (candidate->state() >= NavigationRequest::DID_COMMIT)
      continue;

    if (!found_request ||
        found_request->NavigationStart() < candidate->NavigationStart()) {
      found_request = candidate;
    }
  }

  // If there are no pending navigation requests then the URLLoaderFactory sent
  // now to the renderer process will end up being used by
  // |last_committed_origin_|.
  if (!found_request) {
    DCHECK(has_committed_any_navigation_);
    return last_committed_origin_;
  }

  // URLLoaderFactory sent now to the renderer process will end up being used by
  // the origin associated the navigation that is getting committed in that
  // renderer process.
  return GetOriginForURLLoaderFactory(found_request);
}

network::mojom::URLLoaderFactoryParamsPtr
RenderFrameHostImpl::CreateURLLoaderFactoryParamsForMainWorld(
    const url::Origin& main_world_origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter) {
  return URLLoaderFactoryParamsHelper::CreateForFrame(
      this, main_world_origin, std::move(client_security_state),
      std::move(coep_reporter), GetProcess());
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryAndObserve(
    network::mojom::URLLoaderFactoryParamsPtr params,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  bool bypass_redirect_checks = CreateNetworkServiceDefaultFactoryInternal(
      std::move(params), std::move(default_factory_receiver));

  // Add a disconnect handler when Network Service is running
  // out-of-process.
  if (IsOutOfProcessNetworkService() &&
      (!network_service_disconnect_handler_holder_ ||
       !network_service_disconnect_handler_holder_.is_connected())) {
    network_service_disconnect_handler_holder_.reset();
    StoragePartition* storage_partition = BrowserContext::GetStoragePartition(
        GetSiteInstance()->GetBrowserContext(), GetSiteInstance());
    network::mojom::URLLoaderFactoryParamsPtr monitoring_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    monitoring_factory_params->process_id = GetProcess()->GetID();

    // This factory should never be used to issue actual requests (i.e. it
    // should only be used to monitor for Network Service crashes).  Below is an
    // attempt to enforce that the factory cannot be used in practice.
    monitoring_factory_params->request_initiator_site_lock =
        url::Origin::Create(
            GURL("https://monitoring.url.loader.factory.invalid"));

    storage_partition->GetNetworkContext()->CreateURLLoaderFactory(
        network_service_disconnect_handler_holder_.BindNewPipeAndPassReceiver(),
        std::move(monitoring_factory_params));
    network_service_disconnect_handler_holder_.set_disconnect_handler(
        base::BindOnce(&RenderFrameHostImpl::UpdateSubresourceLoaderFactories,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  return bypass_redirect_checks;
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryInternal(
    network::mojom::URLLoaderFactoryParamsPtr params,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  auto* context = GetSiteInstance()->GetBrowserContext();

  DCHECK(params->request_initiator_site_lock.has_value());
  const url::Origin& origin = params->request_initiator_site_lock.value();

  bool bypass_redirect_checks = false;
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      context, this, GetProcess()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource, origin,
      base::nullopt /* navigation_id */, &default_factory_receiver,
      &params->header_client, &bypass_redirect_checks,
      &params->disable_secure_dns, &params->factory_override);

  // Keep DevTools proxy last, i.e. closest to the network.
  devtools_instrumentation::WillCreateURLLoaderFactory(
      this, false /* is_navigation */, false /* is_download */,
      &default_factory_receiver, &params->factory_override);

  GetProcess()->CreateURLLoaderFactory(std::move(default_factory_receiver),
                                       std::move(params));

  return bypass_redirect_checks;
}

bool RenderFrameHostImpl::CanExecuteJavaScript() {
  if (g_allow_injecting_javascript)
    return true;
  return !frame_tree_node_->current_url().is_valid() ||
         frame_tree_node_->current_url().SchemeIs(kChromeDevToolsScheme) ||
         ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
             GetProcess()->GetID()) ||
         // It's possible to load about:blank in a Web UI renderer.
         // See http://crbug.com/42547
         (frame_tree_node_->current_url().spec() == url::kAboutBlankURL) ||
         // InterstitialPageImpl should be the only case matching this.
         (delegate_->GetAsWebContents() == nullptr);
}

// static
int RenderFrameHost::GetFrameTreeNodeIdForRoutingId(int process_id,
                                                    int routing_id) {
  auto frame_or_proxy = LookupRenderFrameHostOrProxy(process_id, routing_id);
  if (frame_or_proxy)
    return frame_or_proxy.GetFrameTreeNode()->frame_tree_node_id();
  return kNoFrameTreeNodeId;
}

// static
RenderFrameHost* RenderFrameHost::FromPlaceholderId(
    int render_process_id,
    int placeholder_routing_id) {
  RenderFrameProxyHost* rfph =
      RenderFrameProxyHost::FromID(render_process_id, placeholder_routing_id);
  FrameTreeNode* node = rfph ? rfph->frame_tree_node() : nullptr;
  return node ? node->current_frame_host() : nullptr;
}

ui::AXTreeID RenderFrameHostImpl::RoutingIDToAXTreeID(int routing_id) {
  auto frame_or_proxy =
      LookupRenderFrameHostOrProxy(GetProcess()->GetID(), routing_id);
  if (frame_or_proxy.frame)
    return frame_or_proxy.frame->GetAXTreeID();
  if (frame_or_proxy.proxy) {
    return frame_or_proxy.proxy->frame_tree_node()
        ->current_frame_host()
        ->GetAXTreeID();
  }
  return ui::AXTreeIDUnknown();
}

void RenderFrameHostImpl::AXContentNodeDataToAXNodeData(
    const AXContentNodeData& src,
    ui::AXNodeData* dst) {
  // Copy the common fields.
  *dst = src;

  // Map content-specific's |child_routing_id| attribute to a generic attribute
  // with a global AXTreeID.
  if (src.child_routing_id != MSG_ROUTING_NONE) {
    DCHECK_EQ(dst->child_ids.size(), 0U)
        << "A node should not have both children and a child tree.";
    dst->string_attributes.push_back(
        std::make_pair(ax::mojom::StringAttribute::kChildTreeId,
                       RoutingIDToAXTreeID(src.child_routing_id).ToString()));
  }
}

void RenderFrameHostImpl::AXContentTreeDataToAXTreeData(ui::AXTreeData* dst) {
  const AXContentTreeData& src = ax_content_tree_data_;

  // Copy the common fields.
  *dst = src;

  if (src.routing_id != MSG_ROUTING_NONE)
    dst->tree_id = RoutingIDToAXTreeID(src.routing_id);

  if (src.parent_routing_id != MSG_ROUTING_NONE)
    dst->parent_tree_id = RoutingIDToAXTreeID(src.parent_routing_id);

  if (browser_plugin_embedder_ax_tree_id_ != ui::AXTreeIDUnknown())
    dst->parent_tree_id = browser_plugin_embedder_ax_tree_id_;

  // If this is not the root frame tree node, we're done.
  if (!is_main_frame())
    return;

  // For the root frame tree node, also store the AXTreeID of the focused frame.
  auto* focused_frame = static_cast<RenderFrameHostImpl*>(
      delegate_->GetFocusedFrameIncludingInnerWebContents());
  if (!focused_frame)
    return;
  dst->focused_tree_id = focused_frame->GetAXTreeID();
}

void RenderFrameHostImpl::RequestAXHitTestCallback(
    int action_request_id,
    mojom::ChildFrameHitTestInfoPtr child_frame_hit_test_info) {
  // Not receiving a child_frame_hit_test_info means that the renderer has
  // already handled this by emitting the requested event over the object found
  // as the result of the hit testing process, so nothing to do.
  if (!child_frame_hit_test_info)
    return;

  // A child frame was found while hit testing on this frame, and so we need to
  // request a new hit test over such child frame now.
  auto frame_or_proxy = LookupRenderFrameHostOrProxy(
      GetProcess()->GetID(), child_frame_hit_test_info->child_frame_routing_id);
  RenderFrameHostImpl* child_frame =
      frame_or_proxy.proxy
          ? frame_or_proxy.proxy->frame_tree_node()->current_frame_host()
          : frame_or_proxy.frame;

  if (!child_frame || !child_frame->is_active())
    return;

  ui::AXActionData action_data;
  action_data.request_id = action_request_id;
  action_data.target_point = child_frame_hit_test_info->transformed_point;
  action_data.action = ax::mojom::Action::kHitTest;
  action_data.hit_test_event_to_fire = child_frame_hit_test_info->event_to_fire;

  child_frame->AccessibilityPerformAction(action_data);
}

void RenderFrameHostImpl::RequestAXTreeSnapshotCallback(
    AXTreeSnapshotCallback callback,
    const AXContentTreeUpdate& snapshot) {
  ui::AXTreeUpdate dst_snapshot;
  dst_snapshot.root_id = snapshot.root_id;
  dst_snapshot.nodes.resize(snapshot.nodes.size());
  for (size_t i = 0; i < snapshot.nodes.size(); ++i)
    AXContentNodeDataToAXNodeData(snapshot.nodes[i], &dst_snapshot.nodes[i]);

  if (snapshot.has_tree_data) {
    ax_content_tree_data_ = snapshot.tree_data;
    AXContentTreeDataToAXTreeData(&dst_snapshot.tree_data);
    dst_snapshot.has_tree_data = true;
  }
  std::move(callback).Run(dst_snapshot);
}

void RenderFrameHostImpl::CreatePaymentManager(
    mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) {
  if (!IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::kPayment)) {
    mojo::ReportBadMessage("Feature policy blocks Payment");
    return;
  }
  GetProcess()->CreatePaymentManagerForOrigin(GetLastCommittedOrigin(),
                                              std::move(receiver));
}

void RenderFrameHostImpl::CreateWebBluetoothService(
    mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver) {
  BackForwardCache::DisableForRenderFrameHost(this, "WebBluetooth");
  // RFHI owns |web_bluetooth_services_| and |web_bluetooth_service| owns the
  // |receiver_| which may run the error handler. |receiver_| can't run the
  // error handler after it's destroyed so it can't run after the RFHI is
  // destroyed.
  auto web_bluetooth_service =
      std::make_unique<WebBluetoothServiceImpl>(this, std::move(receiver));
  web_bluetooth_service->SetClientConnectionErrorHandler(
      base::BindOnce(&RenderFrameHostImpl::DeleteWebBluetoothService,
                     base::Unretained(this), web_bluetooth_service.get()));
  web_bluetooth_services_.push_back(std::move(web_bluetooth_service));
}

WebBluetoothServiceImpl*
RenderFrameHostImpl::GetWebBluetoothServiceForTesting() {
  DCHECK(web_bluetooth_services_.back());
  return web_bluetooth_services_.back().get();
}

void RenderFrameHostImpl::DeleteWebBluetoothService(
    WebBluetoothServiceImpl* web_bluetooth_service) {
  auto it = std::find_if(
      web_bluetooth_services_.begin(), web_bluetooth_services_.end(),
      [web_bluetooth_service](
          const std::unique_ptr<WebBluetoothServiceImpl>& service) {
        return web_bluetooth_service == service.get();
      });
  DCHECK(it != web_bluetooth_services_.end());
  web_bluetooth_services_.erase(it);
}

void RenderFrameHostImpl::CreateWebUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  BackForwardCache::DisableForRenderFrameHost(this, "WebUSB");
  GetContentClient()->browser()->CreateWebUsbService(this, std::move(receiver));
}

void RenderFrameHostImpl::ResetFeaturePolicy() {
  RenderFrameHostImpl* parent_frame_host = GetParent();
  if (!parent_frame_host && !frame_tree_node_->current_replication_state()
                                 .opener_feature_state.empty()) {
    DCHECK(base::FeatureList::IsEnabled(features::kFeaturePolicyForSandbox));
    feature_policy_ = blink::FeaturePolicy::CreateWithOpenerPolicy(
        frame_tree_node_->current_replication_state().opener_feature_state,
        last_committed_origin_);
    return;
  }
  const blink::FeaturePolicy* parent_policy =
      parent_frame_host ? parent_frame_host->feature_policy() : nullptr;
  blink::ParsedFeaturePolicy container_policy =
      frame_tree_node()->effective_frame_policy().container_policy;
  feature_policy_ = blink::FeaturePolicy::CreateFromParentPolicy(
      parent_policy, container_policy, last_committed_origin_);
}

void RenderFrameHostImpl::CreateAudioInputStreamFactory(
    mojo::PendingReceiver<mojom::RendererAudioInputStreamFactory> receiver) {
  BrowserMainLoop* browser_main_loop = BrowserMainLoop::GetInstance();
  DCHECK(browser_main_loop);
  MediaStreamManager* msm = browser_main_loop->media_stream_manager();
  audio_service_audio_input_stream_factory_.emplace(std::move(receiver), msm,
                                                    this);
}

void RenderFrameHostImpl::CreateAudioOutputStreamFactory(
    mojo::PendingReceiver<mojom::RendererAudioOutputStreamFactory> receiver) {
  media::AudioSystem* audio_system =
      BrowserMainLoop::GetInstance()->audio_system();
  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  audio_service_audio_output_stream_factory_.emplace(
      this, audio_system, media_stream_manager, std::move(receiver));
}

void RenderFrameHostImpl::GetFeatureObserver(
    mojo::PendingReceiver<blink::mojom::FeatureObserver> receiver) {
  if (!feature_observer_) {
    // Lazy initialize because tests sets the overridden content client
    // after the RFHI constructor.
    auto* client = GetContentClient()->browser()->GetFeatureObserverClient();
    if (!client)
      return;
    feature_observer_ = std::make_unique<FeatureObserver>(
        client, GlobalFrameRoutingId(GetProcess()->GetID(), routing_id_));
  }
  feature_observer_->GetFeatureObserver(std::move(receiver));
}

void RenderFrameHostImpl::BindScreenEnumerationReceiver(
    mojo::PendingReceiver<blink::mojom::ScreenEnumeration> receiver) {
  if (!screen_enumeration_impl_)
    screen_enumeration_impl_ = std::make_unique<ScreenEnumerationImpl>(this);
  screen_enumeration_impl_->Bind(std::move(receiver));
}

void RenderFrameHostImpl::BindMediaInterfaceFactoryReceiver(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver) {
  DCHECK(!media_interface_proxy_);
  media_interface_proxy_.reset(new MediaInterfaceProxy(
      this, std::move(receiver),
      base::BindOnce(
          &RenderFrameHostImpl::OnMediaInterfaceFactoryConnectionError,
          base::Unretained(this))));
}

void RenderFrameHostImpl::BindMediaMetricsProviderReceiver(
    mojo::PendingReceiver<media::mojom::MediaMetricsProvider> receiver) {
  // Only save decode stats when BrowserContext provides a VideoPerfHistory.
  // Off-the-record contexts will internally use an ephemeral history DB.
  media::VideoDecodePerfHistory::SaveCallback save_stats_cb;
  if (GetSiteInstance()->GetBrowserContext()->GetVideoDecodePerfHistory()) {
    save_stats_cb = GetSiteInstance()
                        ->GetBrowserContext()
                        ->GetVideoDecodePerfHistory()
                        ->GetSaveCallback();
  }

  media::MediaMetricsProvider::Create(
      GetProcess()->GetBrowserContext()->IsOffTheRecord()
          ? media::MediaMetricsProvider::BrowsingMode::kIncognito
          : media::MediaMetricsProvider::BrowsingMode::kNormal,
      frame_tree_node_->IsMainFrame()
          ? media::MediaMetricsProvider::FrameStatus::kTopFrame
          : media::MediaMetricsProvider::FrameStatus::kNotTopFrame,
      base::BindRepeating(
          &RenderFrameHostDelegate::
              GetUkmSourceIdForLastCommittedSourceIncludingSameDocument,
          // This callback is only executed when Create() is called, during
          // which the lifetime of the |delegate_| is guaranteed.
          base::Unretained(delegate_)),
      base::BindRepeating(
          [](RenderFrameHostImpl* frame) {
            return ::media::learning::FeatureValue(
                frame->GetLastCommittedOrigin().host());
          },
          // Same as above.
          base::Unretained(this)),
      std::move(save_stats_cb),
      base::BindRepeating(
          [](base::WeakPtr<RenderFrameHostImpl> frame)
              -> media::learning::LearningSession* {
            if (!base::FeatureList::IsEnabled(media::kMediaLearningFramework) ||
                !frame) {
              return nullptr;
            }

            return frame->GetProcess()
                ->GetBrowserContext()
                ->GetLearningSession();
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &RenderFrameHostImpl::GetRecordAggregateWatchTimeCallback,
          base::Unretained(this)),
      std::move(receiver));
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
void RenderFrameHostImpl::BindMediaRemoterFactoryReceiver(
    mojo::PendingReceiver<media::mojom::RemoterFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<RemoterFactoryImpl>(GetProcess()->GetID(), routing_id_),
      std::move(receiver));
}
#endif

void RenderFrameHostImpl::CreateWebSocketConnector(
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebSocketConnectorImpl>(
                                  GetProcess()->GetID(), routing_id_,
                                  last_committed_origin_, isolation_info_),
                              std::move(receiver));
}

void RenderFrameHostImpl::CreateQuicTransportConnector(
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<QuicTransportConnectorImpl>(
          GetProcess()->GetID(), weak_ptr_factory_.GetWeakPtr(),
          last_committed_origin_, isolation_info_.network_isolation_key()),
      std::move(receiver));
}

void RenderFrameHostImpl::CreateNotificationService(
    mojo::PendingReceiver<blink::mojom::NotificationService> receiver) {
  GetProcess()->CreateNotificationService(GetLastCommittedOrigin(),
                                          std::move(receiver));
}

void RenderFrameHostImpl::CreateInstalledAppProvider(
    mojo::PendingReceiver<blink::mojom::InstalledAppProvider> receiver) {
  InstalledAppProviderImpl::Create(this, std::move(receiver));
}

void RenderFrameHostImpl::CreateDedicatedWorkerHostFactory(
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver) {
  // Allocate the worker in the same process as the creator.
  int worker_process_id = process_->GetID();

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  auto coep_reporter_endpoint = coep_reporter.InitWithNewPipeAndPassReceiver();
  if (coep_reporter_)
    coep_reporter_->Clone(std::move(coep_reporter_endpoint));

  // When a dedicated worker is created from the frame script, the frame is both
  // the creator and the ancestor.
  content::CreateDedicatedWorkerHostFactory(
      worker_process_id,
      /*creator_render_frame_host_id=*/GetGlobalFrameRoutingId(),
      /*ancestor_render_frame_host_id=*/GetGlobalFrameRoutingId(),
      last_committed_origin_, cross_origin_embedder_policy_,
      std::move(coep_reporter), std::move(receiver));
}

void RenderFrameHostImpl::OnMediaInterfaceFactoryConnectionError() {
  DCHECK(media_interface_proxy_);
  media_interface_proxy_.reset();
}

#if defined(OS_ANDROID)
void RenderFrameHostImpl::BindNFCReceiver(
    mojo::PendingReceiver<device::mojom::NFC> receiver) {
  delegate_->GetNFC(this, std::move(receiver));
}
#endif

#if !defined(OS_ANDROID)
void RenderFrameHostImpl::BindSerialService(
    mojo::PendingReceiver<blink::mojom::SerialService> receiver) {
  if (!IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::kSerial)) {
    mojo::ReportBadMessage("Feature policy blocks access to Serial.");
    return;
  }

  if (!serial_service_)
    serial_service_ = std::make_unique<SerialService>(this);

  serial_service_->Bind(std::move(receiver));
}

void RenderFrameHostImpl::BindAuthenticatorReceiver(
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver) {
  if (!authenticator_impl_)
    authenticator_impl_.reset(new AuthenticatorImpl(this));

  authenticator_impl_->Bind(std::move(receiver));
}

void RenderFrameHostImpl::GetHidService(
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  HidService::Create(this, std::move(receiver));
}
#endif

void RenderFrameHostImpl::GetIdleManager(
    mojo::PendingReceiver<blink::mojom::IdleManager> receiver) {
  if (!IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::kIdleDetection)) {
    mojo::ReportBadMessage("Feature policy blocks access to IdleDetection.");
    return;
  }
  static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition())
      ->GetIdleManager()
      ->CreateService(std::move(receiver),
                      GetMainFrame()->GetLastCommittedOrigin());
}

void RenderFrameHostImpl::GetPresentationService(
    mojo::PendingReceiver<blink::mojom::PresentationService> receiver) {
  if (!presentation_service_)
    presentation_service_ = PresentationServiceImpl::Create(this);
  presentation_service_->Bind(std::move(receiver));
}

PresentationServiceImpl&
RenderFrameHostImpl::GetPresentationServiceForTesting() {
  DCHECK(presentation_service_);
  return *presentation_service_.get();
}

void RenderFrameHostImpl::GetSpeechSynthesis(
    mojo::PendingReceiver<blink::mojom::SpeechSynthesis> receiver) {
  if (!speech_synthesis_impl_) {
    speech_synthesis_impl_ = std::make_unique<SpeechSynthesisImpl>(
        GetProcess()->GetBrowserContext());
  }
  speech_synthesis_impl_->AddReceiver(std::move(receiver));
}

void RenderFrameHostImpl::GetFileChooser(
    mojo::PendingReceiver<blink::mojom::FileChooser> receiver) {
  FileChooserImpl::Create(this, std::move(receiver));
}

void RenderFrameHostImpl::GetSensorProvider(
    mojo::PendingReceiver<device::mojom::SensorProvider> receiver) {
  if (!sensor_provider_proxy_) {
    sensor_provider_proxy_.reset(new SensorProviderProxyImpl(
        PermissionControllerImpl::FromBrowserContext(
            GetProcess()->GetBrowserContext()),
        this));
  }
  sensor_provider_proxy_->Bind(std::move(receiver));
}

mojo::Remote<blink::mojom::FileChooser>
RenderFrameHostImpl::BindFileChooserForTesting() {
  mojo::Remote<blink::mojom::FileChooser> chooser;
  FileChooserImpl::Create(this, chooser.BindNewPipeAndPassReceiver());
  return chooser;
}

void RenderFrameHostImpl::BindCacheStorage(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_remote;
  if (coep_reporter_) {
    coep_reporter_->Clone(
        coep_reporter_remote.InitWithNewPipeAndPassReceiver());
  }
  GetProcess()->BindCacheStorage(cross_origin_embedder_policy_,
                                 std::move(coep_reporter_remote),
                                 GetLastCommittedOrigin(), std::move(receiver));
}

void RenderFrameHostImpl::BindInputInjectorReceiver(
    mojo::PendingReceiver<mojom::InputInjector> receiver) {
  InputInjectorImpl::Create(weak_ptr_factory_.GetWeakPtr(),
                            std::move(receiver));
}

void RenderFrameHostImpl::BindSmsReceiverReceiver(
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver) {
  if (GetParent() && !GetMainFrame()->GetLastCommittedOrigin().IsSameOriginWith(
                         GetLastCommittedOrigin())) {
    mojo::ReportBadMessage("Must have the same origin as the top-level frame.");
    return;
  }
  auto* fetcher = SmsFetcher::Get(GetProcess()->GetBrowserContext());
  SmsService::Create(fetcher, this, std::move(receiver));
}

void RenderFrameHostImpl::BindRestrictedCookieManager(
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition())
      ->CreateRestrictedCookieManager(
          network::mojom::RestrictedCookieManagerRole::SCRIPT,
          GetLastCommittedOrigin(), isolation_info_.site_for_cookies(),
          ComputeTopFrameOrigin(GetLastCommittedOrigin()),
          /* is_service_worker = */ false, GetProcess()->GetID(), routing_id(),
          std::move(receiver), CreateCookieAccessObserver());
}

void RenderFrameHostImpl::BindHasTrustTokensAnswerer(
    mojo::PendingReceiver<network::mojom::HasTrustTokensAnswerer> receiver) {
  auto top_frame_origin = ComputeTopFrameOrigin(GetLastCommittedOrigin());

  // A check at the callsite in the renderer ensures a correctly functioning
  // renderer will only request this Mojo handle if the top-frame origin is
  // potentially trustworthy and has scheme HTTP or HTTPS.
  if ((top_frame_origin.scheme() != url::kHttpScheme &&
       top_frame_origin.scheme() != url::kHttpsScheme) ||
      !network::IsOriginPotentiallyTrustworthy(top_frame_origin)) {
    mojo::ReportBadMessage(
        "Attempted to get a HasTrustTokensAnswerer for a non-trustworthy or "
        "non-HTTP/HTTPS top-frame origin.");
    return;
  }

  // This is enforced in benign renderers by the [SecureContext] IDL
  // attribute on Document::hasTrustToken.
  if (!network::IsOriginPotentiallyTrustworthy(GetLastCommittedOrigin())) {
    mojo::ReportBadMessage(
        "Attempted to get a HasTrustTokensAnswerer from an insecure context.");
    return;
  }

  // This is enforced in benign renderers by the RuntimeEnabled=TrustTokens IDL
  // attribute (the base::Feature's value is tied to the
  // RuntimeEnabledFeature's).
  if (!base::FeatureList::IsEnabled(network::features::kTrustTokens)) {
    mojo::ReportBadMessage(
        "Attempted to get a HasTrustTokensAnswerer with Trust Tokens "
        "disabled.");
    return;
  }

  GetProcess()->GetStoragePartition()->CreateHasTrustTokensAnswerer(
      std::move(receiver), ComputeTopFrameOrigin(GetLastCommittedOrigin()));
}

void RenderFrameHostImpl::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // Requests are serviced on |document_scoped_interface_provider_receiver_|. It
  // is therefore safe to assume that every incoming interface request is coming
  // from the currently active document in the corresponding RenderFrame.
  // NOTE: this way of acquiring interfaces has been replaced with
  // BrowserInterfaceBroker (see https://crbug.com/718652). The code below is
  // left to support the CastWebContentsImpl use case.
  delegate_->OnInterfaceRequest(this, interface_name, &interface_pipe);
}

void RenderFrameHostImpl::CreateAppCacheBackend(
    mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition());
  storage_partition_impl->GetAppCacheService()->CreateBackend(
      GetProcess()->GetID(), routing_id_, std::move(receiver));
}

void RenderFrameHostImpl::GetAudioContextManager(
    mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver) {
  AudioContextManagerImpl::Create(this, std::move(receiver));
}

void RenderFrameHostImpl::GetContactsManager(
    mojo::PendingReceiver<blink::mojom::ContactsManager> receiver) {
  ContactsManagerImpl::Create(this, std::move(receiver));
}

void RenderFrameHostImpl::GetFileSystemManager(
    mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) {
  // This is safe because file_system_manager_ is deleted on the IO thread
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&FileSystemManagerImpl::BindReceiver,
                                base::Unretained(file_system_manager_.get()),
                                std::move(receiver)));
}

void RenderFrameHostImpl::GetGeolocationService(
    mojo::PendingReceiver<blink::mojom::GeolocationService> receiver) {
  if (!geolocation_service_) {
    auto* geolocation_context = delegate_->GetGeolocationContext();
    if (!geolocation_context)
      return;
    geolocation_service_ =
        std::make_unique<GeolocationServiceImpl>(geolocation_context, this);
  }
  geolocation_service_->Bind(std::move(receiver));
}

void RenderFrameHostImpl::GetNativeFileSystemManager(
    mojo::PendingReceiver<blink::mojom::NativeFileSystemManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition =
      static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition());
  auto* manager = storage_partition->GetNativeFileSystemManager();
  manager->BindReceiver(NativeFileSystemManagerImpl::BindingContext(
                            GetLastCommittedOrigin(), GetLastCommittedURL(),
                            GetProcess()->GetID(), routing_id_),
                        std::move(receiver));
}

void RenderFrameHostImpl::CreateLockManager(
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  GetProcess()->CreateLockManager(GetRoutingID(), GetLastCommittedOrigin(),
                                  std::move(receiver));
}

void RenderFrameHostImpl::CreateIDBFactory(
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  GetProcess()->BindIndexedDB(GetLastCommittedOrigin(), std::move(receiver));
}

void RenderFrameHostImpl::CreatePermissionService(
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver) {
  if (!permission_service_context_)
    permission_service_context_.reset(new PermissionServiceContext(this));

  permission_service_context_->CreateService(std::move(receiver));
}

void RenderFrameHostImpl::GetAuthenticator(
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver) {
#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebAuth)) {
    BindAuthenticatorReceiver(std::move(receiver));
  }
#else
  GetJavaInterfaces()->GetInterface(std::move(receiver));
#endif  // !defined(OS_ANDROID)
}

void RenderFrameHostImpl::GetPushMessaging(
    mojo::PendingReceiver<blink::mojom::PushMessaging> receiver) {
  if (!push_messaging_manager_) {
    push_messaging_manager_.reset(new PushMessagingManager(
        GetProcess()->GetID(), routing_id_,
        static_cast<StoragePartitionImpl*>(GetProcess()->GetStoragePartition())
            ->GetServiceWorkerContext()));
  }

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&PushMessagingManager::AddPushMessagingReceiver,
                     push_messaging_manager_->AsWeakPtr(),
                     std::move(receiver)));
}

void RenderFrameHostImpl::GetVirtualAuthenticatorManager(
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
        receiver) {
#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebAuth)) {
    auto* environment_singleton = AuthenticatorEnvironmentImpl::GetInstance();
    environment_singleton->EnableVirtualAuthenticatorFor(frame_tree_node_);
    environment_singleton->AddVirtualAuthenticatorReceiver(frame_tree_node_,
                                                           std::move(receiver));
  }
#endif  // !defined(OS_ANDROID)
}

std::unique_ptr<NavigationRequest>
RenderFrameHostImpl::CreateNavigationRequestForCommit(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document) {
  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter;
  // We don't switch the COEP reporter on same-document navigations, so create
  // one only for cross-document navigations.
  if (!is_same_document) {
    coep_reporter = std::make_unique<CrossOriginEmbedderPolicyReporter>(
        process_->GetStoragePartition(), params.url,
        cross_origin_embedder_policy_.reporting_endpoint,
        cross_origin_embedder_policy_.report_only_reporting_endpoint);
  }
  return NavigationRequest::CreateForCommit(frame_tree_node_, this, params,
                                            std::move(coep_reporter),
                                            is_same_document);
}

bool RenderFrameHostImpl::NavigationRequestWasIntendedForPendingEntry(
    NavigationRequest* request,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool same_document) {
  NavigationEntryImpl* pending_entry = NavigationEntryImpl::FromNavigationEntry(
      frame_tree_node()->navigator()->GetController()->GetPendingEntry());
  if (!pending_entry)
    return false;
  if (request->nav_entry_id() != pending_entry->GetUniqueID())
    return false;
  if (!same_document) {
    // Make sure that the pending entry was really loaded via
    // LoadDataWithBaseURL and that it matches this handle.
    // TODO(csharrison): The pending entry's base url should equal
    // |params.base_url|. This is not the case for loads with invalid base urls.
    if (request->common_params().url != params.base_url ||
        pending_entry->GetBaseURLForDataURL().is_empty()) {
      return false;
    }
  }
  return true;
}

void RenderFrameHostImpl::BeforeUnloadTimeout() {
  if (render_view_host_->GetDelegate()->ShouldIgnoreUnresponsiveRenderer())
    return;

  SimulateBeforeUnloadCompleted(true /* proceed */);
}

void RenderFrameHostImpl::SetLastCommittedSiteUrl(const GURL& url) {
  GURL site_url = url.is_empty()
                      ? GURL()
                      : SiteInstanceImpl::GetSiteForURL(
                            GetSiteInstance()->GetIsolationContext(), url);

  if (last_committed_site_url_ == site_url)
    return;

  if (!last_committed_site_url_.is_empty()) {
    RenderProcessHostImpl::RemoveFrameWithSite(
        frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
        GetProcess(), last_committed_site_url_);
  }

  last_committed_site_url_ = site_url;

  if (!last_committed_site_url_.is_empty()) {
    RenderProcessHostImpl::AddFrameWithSite(
        frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
        GetProcess(), last_committed_site_url_);
  }
}

#if defined(OS_ANDROID)

class RenderFrameHostImpl::JavaInterfaceProvider
    : public service_manager::mojom::InterfaceProvider {
 public:
  using BindCallback =
      base::RepeatingCallback<void(const std::string&,
                                   mojo::ScopedMessagePipeHandle)>;

  JavaInterfaceProvider(
      const BindCallback& bind_callback,
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver)
      : bind_callback_(bind_callback), receiver_(this, std::move(receiver)) {}
  ~JavaInterfaceProvider() override = default;

 private:
  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle handle) override {
    bind_callback_.Run(interface_name, std::move(handle));
  }

  const BindCallback bind_callback_;
  mojo::Receiver<service_manager::mojom::InterfaceProvider> receiver_;

  DISALLOW_COPY_AND_ASSIGN(JavaInterfaceProvider);
};

base::android::ScopedJavaLocalRef<jobject>
RenderFrameHostImpl::GetJavaRenderFrameHost() {
  RenderFrameHostAndroid* render_frame_host_android =
      static_cast<RenderFrameHostAndroid*>(
          GetUserData(kRenderFrameHostAndroidKey));
  if (!render_frame_host_android) {
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        interface_provider_remote;
    java_interface_registry_ = std::make_unique<JavaInterfaceProvider>(
        base::BindRepeating(
            &RenderFrameHostImpl::ForwardGetInterfaceToRenderFrame,
            weak_ptr_factory_.GetWeakPtr()),
        interface_provider_remote.InitWithNewPipeAndPassReceiver());
    render_frame_host_android =
        new RenderFrameHostAndroid(this, std::move(interface_provider_remote));
    SetUserData(kRenderFrameHostAndroidKey,
                base::WrapUnique(render_frame_host_android));
  }
  return render_frame_host_android->GetJavaObject();
}

service_manager::InterfaceProvider* RenderFrameHostImpl::GetJavaInterfaces() {
  if (!java_interfaces_) {
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider> provider;
    BindInterfaceRegistryForRenderFrameHost(
        provider.InitWithNewPipeAndPassReceiver(), this);
    java_interfaces_.reset(new service_manager::InterfaceProvider);
    java_interfaces_->Bind(std::move(provider));
  }
  return java_interfaces_.get();
}

void RenderFrameHostImpl::ForwardGetInterfaceToRenderFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle pipe) {
  GetRemoteInterfaces()->GetInterfaceByName(interface_name, std::move(pipe));
}
#endif

void RenderFrameHostImpl::ForEachImmediateLocalRoot(
    const base::RepeatingCallback<void(RenderFrameHostImpl*)>& callback) {
  if (!frame_tree_node_->child_count())
    return;

  base::queue<FrameTreeNode*> queue;
  for (size_t index = 0; index < frame_tree_node_->child_count(); ++index)
    queue.push(frame_tree_node_->child_at(index));
  while (queue.size()) {
    FrameTreeNode* current = queue.front();
    queue.pop();
    if (current->current_frame_host()->is_local_root()) {
      callback.Run(current->current_frame_host());
    } else {
      for (size_t index = 0; index < current->child_count(); ++index)
        queue.push(current->child_at(index));
    }
  }
}

void RenderFrameHostImpl::SetVisibilityForChildViews(bool visible) {
  ForEachImmediateLocalRoot(base::BindRepeating(
      [](bool is_visible, RenderFrameHostImpl* frame_host) {
        if (auto* view = frame_host->GetView())
          return is_visible ? view->Show() : view->Hide();
      },
      visible));
}

mojom::FrameNavigationControl* RenderFrameHostImpl::GetNavigationControl() {
  if (!navigation_control_)
    GetRemoteAssociatedInterfaces()->GetInterface(&navigation_control_);
  return navigation_control_.get();
}

bool RenderFrameHostImpl::ValidateDidCommitParams(
    NavigationRequest* navigation_request,
    FrameHostMsg_DidCommitProvisionalLoad_Params* params,
    bool is_same_document_navigation) {
  RenderProcessHost* process = GetProcess();

  // Error pages may sometimes commit a URL in the wrong process, which requires
  // an exception for the CanCommitOriginAndUrl() checks.  This is ok as long
  // as the origin is opaque.
  bool bypass_checks_for_error_page = false;
  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          frame_tree_node_->IsMainFrame())) {
    if (site_instance_->GetSiteURL() == GURL(content::kUnreachableWebDataURL)) {
      // Commits in the error page process must only be failures, otherwise
      // successful navigations could commit documents from origins different
      // than the chrome-error://chromewebdata/ one and violate expectations.
      if (!params->url_is_unreachable) {
        DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, params->origin);
        bad_message::ReceivedBadMessage(
            process, bad_message::RFH_ERROR_PROCESS_NON_ERROR_COMMIT);
        return false;
      }
      // With error page isolation, any URL can commit in an error page process.
      bypass_checks_for_error_page = true;
    }
  } else {
    // Without error page isolation, a blocked navigation is expected to
    // commit in the old renderer process.  This may be true for subframe
    // navigations even when error page isolation is enabled for main frames.
    if (navigation_request &&
        navigation_request->GetNetErrorCode() == net::ERR_BLOCKED_BY_CLIENT) {
      bypass_checks_for_error_page = true;
    }
  }

  // Error pages must commit in a opaque origin. Terminate the renderer
  // process if this is violated.
  if (bypass_checks_for_error_page && !params->origin.opaque()) {
    DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, params->origin);
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_ERROR_PROCESS_NON_UNIQUE_ORIGIN_COMMIT);
    return false;
  }

  // file: URLs can be allowed to access any other origin, based on settings.
  bool bypass_checks_for_file_scheme = false;
  if (params->origin.scheme() == url::kFileScheme) {
    WebPreferences prefs = render_view_host_->GetWebkitPreferences();
    if (prefs.allow_universal_access_from_file_urls)
      bypass_checks_for_file_scheme = true;
  }

  // WebView's loadDataWithBaseURL API is allowed to bypass normal commit
  // checks because it is allowed to commit anything into its unlocked process
  // and its data: URL and non-opaque origin would fail the normal commit
  // checks.
  bool bypass_checks_for_webview = false;
  if ((navigation_request &&
       IsLoadDataWithBaseURL(navigation_request->common_params())) ||
      (is_same_document_navigation && IsLoadDataWithBaseURL(*params))) {
    // Allow bypass if the process isn't locked. Otherwise run normal checks.
    bypass_checks_for_webview = ChildProcessSecurityPolicyImpl::GetInstance()
                                    ->GetOriginLock(process->GetID())
                                    .is_empty();
  }

  if (!bypass_checks_for_error_page && !bypass_checks_for_file_scheme &&
      !bypass_checks_for_webview) {
    // Attempts to commit certain off-limits URL should be caught more strictly
    // than our FilterURL checks.  If a renderer violates this policy, it
    // should be killed.
    switch (CanCommitOriginAndUrl(params->origin, params->url)) {
      case CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL:
        // The origin and URL are safe to commit.
        break;
      case CanCommitStatus::CANNOT_COMMIT_URL:
        DLOG(ERROR) << "CANNOT_COMMIT_URL url '" << params->url << "'"
                    << " origin '" << params->origin << "'"
                    << " lock '"
                    << ChildProcessSecurityPolicyImpl::GetInstance()
                           ->GetOriginLock(process->GetID())
                    << "'";
        VLOG(1) << "Blocked URL " << params->url.spec();
        LogCannotCommitUrlCrashKeys(params->url, is_same_document_navigation,
                                    navigation_request);

        // Kills the process.
        bad_message::ReceivedBadMessage(
            process, bad_message::RFH_CAN_COMMIT_URL_BLOCKED);
        return false;
      case CanCommitStatus::CANNOT_COMMIT_ORIGIN:
        DLOG(ERROR) << "CANNOT_COMMIT_ORIGIN url '" << params->url << "'"
                    << " origin '" << params->origin << "'"
                    << " lock '"
                    << ChildProcessSecurityPolicyImpl::GetInstance()
                           ->GetOriginLock(process->GetID())
                    << "'";
        DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, params->origin);
        LogCannotCommitOriginCrashKeys(is_same_document_navigation,
                                       navigation_request);

        // Kills the process.
        bad_message::ReceivedBadMessage(
            process, bad_message::RFH_INVALID_ORIGIN_ON_COMMIT);
        return false;
    }
  }

  // Without this check, an evil renderer can trick the browser into creating
  // a navigation entry for a banned URL.  If the user clicks the back button
  // followed by the forward button (or clicks reload, or round-trips through
  // session restore, etc), we'll think that the browser commanded the
  // renderer to load the URL and grant the renderer the privileges to request
  // the URL.  To prevent this attack, we block the renderer from inserting
  // banned URLs into the navigation controller in the first place.
  //
  // TODO(https://crbug.com/172694): Currently, when FilterURL detects a bad URL
  // coming from the renderer, it overwrites that URL to about:blank, which
  // requires |params| to be mutable. Once we kill the renderer
  // instead, the signature of RenderFrameHostImpl::DidCommitProvisionalLoad can
  // be modified to take |params| by const reference.
  process->FilterURL(false, &params->url);
  process->FilterURL(true, &params->referrer.url);
  for (auto it(params->redirects.begin()); it != params->redirects.end();
       ++it) {
    process->FilterURL(false, &(*it));
  }

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(params->page_state)) {
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return false;
  }

  // A cross-document navigation requires an embedding token for all embedded
  // frames (a child frame to a remote parent). Embedding tokens should not
  // exist for other cases.
  if (!is_same_document_navigation) {
    if (frame_tree_node()->IsMainFrame() &&
        params->embedding_token.has_value()) {
      bad_message::ReceivedBadMessage(
          process, bad_message::RFH_UNEXPECTED_EMBEDDING_TOKEN);
      return false;
    } else if (IsCrossProcessSubframe() &&
               !params->embedding_token.has_value()) {
      bad_message::ReceivedBadMessage(process,
                                      bad_message::RFH_MISSING_EMBEDDING_TOKEN);
      return false;
    }
  } else if (params->embedding_token.has_value()) {
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_UNEXPECTED_EMBEDDING_TOKEN);
    return false;
  }

  return true;
}

void RenderFrameHostImpl::UpdateSiteURL(const GURL& url,
                                        bool url_is_unreachable) {
  if (url_is_unreachable) {
    SetLastCommittedSiteUrl(GURL());
  } else {
    SetLastCommittedSiteUrl(url);
  }
}

bool RenderFrameHostImpl::DidCommitNavigationInternal(
    std::unique_ptr<NavigationRequest> navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    bool is_same_document_navigation) {
  // Sanity-check the page transition for frame type.
  DCHECK_EQ(ui::PageTransitionIsMainFrame(params->transition), !GetParent());

  // Check that the committing navigation token matches the navigation request.
  if (navigation_request &&
      navigation_request->commit_params().navigation_token !=
          params->navigation_token) {
    navigation_request.reset();
  }

  if (!navigation_request) {
    // A matching NavigationRequest should have been found, unless in a few very
    // specific cases. Check if this is one of those cases.
    bool is_commit_allowed_to_proceed = false;

    // 1) This was a renderer-initiated navigation to an empty document. Most
    // of the time: about:blank.
    is_commit_allowed_to_proceed |= params->url.SchemeIs(url::kAboutScheme) &&
                                    params->url != GURL(url::kAboutSrcdocURL);

    // 2) This was a same-document navigation.
    // TODO(clamy): We should enforce having a request on browser-initiated
    // same-document navigations.
    is_commit_allowed_to_proceed |= is_same_document_navigation;

    // 3) Error pages implementations in Chrome can commit twice.
    // TODO(clamy): Fix this.
    is_commit_allowed_to_proceed |= params->url_is_unreachable;

    // 4) Special case for DOMSerializerBrowsertests which are implemented
    // entirely renderer-side and unlike normal RenderView based tests load
    // file URLs instead of data URLs.
    // TODO(clamy): Rework the tests to remove this exception.
    is_commit_allowed_to_proceed |= params->url.SchemeIsFile();

    if (!is_commit_allowed_to_proceed) {
      bad_message::ReceivedBadMessage(
          GetProcess(),
          bad_message::RFH_NO_MATCHING_NAVIGATION_REQUEST_ON_COMMIT);
      return false;
    }
  }

  if (!ValidateDidCommitParams(navigation_request.get(), params.get(),
                               is_same_document_navigation)) {
    return false;
  }

  // TODO(clamy): We should stop having a special case for same-document
  // navigation and just put them in the general map of NavigationRequests.
  if (navigation_request &&
      navigation_request->common_params().url != params->url &&
      is_same_document_navigation) {
    same_document_navigation_request_ = std::move(navigation_request);
  }

  // Set is loading to true now if it has not been set yet. This happens for
  // renderer-initiated same-document navigations. It can also happen when a
  // racy DidStopLoading IPC resets the loading state that was set to true in
  // CommitNavigation.
  if (!is_loading()) {
    bool was_loading = frame_tree_node()->frame_tree()->IsLoading();
    is_loading_ = true;
    frame_tree_node()->DidStartLoading(!is_same_document_navigation,
                                       was_loading);
  }

  if (navigation_request)
    was_discarded_ = navigation_request->commit_params().was_discarded;

  // If there is no valid NavigationRequest corresponding to this commit, create
  // one in order to properly issue DidFinishNavigation calls to
  // WebContentsObservers.
  if (!navigation_request) {
    navigation_request =
        CreateNavigationRequestForCommit(*params, is_same_document_navigation);
  }

  DCHECK(navigation_request);
  DCHECK(navigation_request->IsNavigationStarted());

  // Update the page transition. For subframe navigations, the renderer process
  // only gives the correct page transition at commit time.
  // TODO(clamy): We should get the correct page transition when starting the
  // request.
  navigation_request->set_transition(params->transition);

  navigation_request->set_has_user_gesture(params->gesture ==
                                           NavigationGestureUser);

  last_http_status_code_ = params->http_status_code;
  last_http_method_ = params->method;
  UpdateSiteURL(params->url, params->url_is_unreachable);
  if (!is_same_document_navigation)
    UpdateRenderProcessHostFramePriorities();

  // Set the state whether this navigation is to an MHTML document, since there
  // are certain security checks that we cannot apply to subframes in MHTML
  // documents. Do not trust renderer data when determining that, rather use
  // the |navigation_request|, which was generated and stays browser side.
  is_mhtml_document_ =
      (navigation_request->GetMimeType() == "multipart/related" ||
       navigation_request->GetMimeType() == "message/rfc822");

  accessibility_reset_count_ = 0;
  appcache_handle_ = navigation_request->TakeAppCacheHandle();

  if (!is_same_document_navigation &&
      !navigation_request->IsServedFromBackForwardCache()) {
    if (navigation_request->IsInMainFrame()) {
      render_view_host_->ResetPerPageState();
    }
    last_committed_cross_document_navigation_id_ =
        navigation_request->GetNavigationId();

    if (IsCurrent()) {
      // Clear all the user data associated with the non speculative
      // RenderFrameHost when the navigation is a cross-document navigation not
      // served from the back-forward cache.
      document_associated_data_.ClearAllUserData();
    }

    // Continue observing the events for the committed navigation.
    for (auto& receiver : navigation_request->TakeCookieObservers()) {
      cookie_observers_.Add(this, std::move(receiver));
    }
  }

  // If we still have a PeakGpuMemoryTracker, then the loading it was observing
  // never completed. Cancel it's callback so that we don't report partial
  // loads to UMA.
  if (loading_mem_tracker_)
    loading_mem_tracker_->Cancel();
  // Main Frames will create the tracker, which will be triggered after we
  // receive OnDidStopLoading.
  loading_mem_tracker_ = navigation_request->TakePeakGpuMemoryTracker();

  network::mojom::ClientSecurityStatePtr client_security_state =
      navigation_request->TakeClientSecurityState();
  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter =
      navigation_request->TakeCoepReporter();
  if (coep_reporter) {
    mojo::PendingRemote<blink::mojom::ReportingObserver> remote;
    mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver =
        remote.InitWithNewPipeAndPassReceiver();
    coep_reporter->BindObserver(std::move(remote));
    // As some tests overrides the associated frame after commit, do not
    // call GetAssociatedLocalFrame now.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&RenderFrameHostImpl::BindReportingObserver,
                       weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
  }

  frame_tree_node()->navigator()->DidNavigate(this, *params,
                                              std::move(navigation_request),
                                              is_same_document_navigation);

  if (IsBackForwardCacheEnabled()) {
    // Store the Commit params so they can be reused if the page is ever
    // restored from the BackForwardCache.
    last_commit_params_ = std::move(params);
  }

  if (!is_same_document_navigation) {
    cookie_no_samesite_deprecation_url_hashes_.clear();
    cookie_samesite_none_insecure_deprecation_url_hashes_.clear();
    renderer_reported_scheduler_tracked_features_ = 0;
    browser_reported_scheduler_tracked_features_ = 0;
    last_committed_client_security_state_ = std::move(client_security_state);
    coep_reporter_ = std::move(coep_reporter);
  }

  RecordCrossOriginIsolationMetrics(this);

  return true;
}

void RenderFrameHostImpl::OnSameDocumentCommitProcessed(
    int64_t navigation_id,
    bool should_replace_current_entry,
    blink::mojom::CommitResult result) {
  // If the NavigationRequest was deleted, another navigation commit started to
  // be processed. Let the latest commit go through and stop doing anything.
  if (!same_document_navigation_request_ ||
      same_document_navigation_request_->GetNavigationId() != navigation_id) {
    return;
  }

  if (result == blink::mojom::CommitResult::RestartCrossDocument) {
    // The navigation could not be committed as a same-document navigation.
    // Restart the navigation cross-document.
    frame_tree_node_->navigator()->RestartNavigationAsCrossDocument(
        std::move(same_document_navigation_request_));
  }

  if (result == blink::mojom::CommitResult::Aborted) {
    // Note: if the commit was successful, navigation_request_ is reset in
    // DidCommitProvisionalLoad.
    same_document_navigation_request_.reset();
  }
}

void RenderFrameHostImpl::OnCrossDocumentCommitProcessed(
    NavigationRequest* navigation_request,
    blink::mojom::CommitResult result) {
  DCHECK_NE(blink::mojom::CommitResult::RestartCrossDocument, result);
  if (result == blink::mojom::CommitResult::Ok) {
    // The navigation will soon be committed. Move it out of the map to the
    // NavigationRequest that is about to commit.
    auto find_request = navigation_requests_.find(navigation_request);
    if (find_request != navigation_requests_.end()) {
      navigation_request_ = std::move(find_request->second);
    } else {
      // This frame might be used for attaching an inner WebContents in which
      // case |navigation_requests_| are deleted during attaching.
      // TODO(ekaramad): Add a DCHECK to ensure the FrameTreeNode is attaching
      // an inner delegate (https://crbug.com/911161).
    }
  }
  // Remove the requests from the list of NavigationRequests waiting to commit.
  navigation_requests_.erase(navigation_request);
}

std::unique_ptr<base::trace_event::TracedValue>
RenderFrameHostImpl::CommitAsTracedValue(
    FrameHostMsg_DidCommitProvisionalLoad_Params* params) const {
  auto value = std::make_unique<base::trace_event::TracedValue>();

  value->SetInteger("frame_tree_node", frame_tree_node_->frame_tree_node_id());
  value->SetInteger("site id", site_instance_->GetId());
  value->SetString("process lock", ChildProcessSecurityPolicyImpl::GetInstance()
                                       ->GetOriginLock(process_->GetID())
                                       .possibly_invalid_spec());
  value->SetString("origin", params->origin.Serialize());
  value->SetInteger("transition", params->transition);

  if (!params->base_url.is_empty()) {
    value->SetString("base_url", params->base_url.possibly_invalid_spec());
  }

  return value;
}

void RenderFrameHostImpl::MaybeGenerateCrashReport(
    base::TerminationStatus status,
    int exit_code) {
  if (!last_committed_url_.SchemeIsHTTPOrHTTPS())
    return;

  // Only generate reports for local root frames.
  if (!frame_tree_node_->IsMainFrame() && !IsCrossProcessSubframe())
    return;

  // Check the termination status to see if a crash occurred (and potentially
  // determine the |reason| for the crash).
  std::string reason;
  switch (status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      break;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      if (exit_code == RESULT_CODE_HUNG)
        reason = "unresponsive";
      break;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      if (exit_code == RESULT_CODE_HUNG)
        reason = "unresponsive";
      else
        return;
      break;
    case base::TERMINATION_STATUS_OOM:
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
#endif
      reason = "oom";
      break;
    default:
      // Other termination statuses do not indicate a crash.
      return;
  }

  // Construct the crash report.
  auto body = base::DictionaryValue();
  if (!reason.empty())
    body.SetString("reason", reason);

  // Send the crash report to the Reporting API.
  GetProcess()->GetStoragePartition()->GetNetworkContext()->QueueReport(
      "crash" /* type */, "default" /* group */, last_committed_url_,
      base::nullopt, std::move(body));
}

void RenderFrameHostImpl::SendCommitNavigation(
    mojom::NavigationClient* navigation_client,
    NavigationRequest* navigation_request,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    base::Optional<std::vector<::content::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory,
    const base::UnguessableToken& devtools_navigation_token) {
  // For committed interstitials, we do not have a NavigationRequest and use the
  // old NavigationControl mojo interface. For anything else we should have a
  // NavigationRequest, containing a NavigationClient, and commit through it.
  // TODO(ahemery): Update when https://crbug.com/448486 is done.
  if (navigation_client) {
    navigation_client->CommitNavigation(
        std::move(common_params), std::move(commit_params),
        std::move(response_head), std::move(response_body),
        std::move(url_loader_client_endpoints),
        std::move(subresource_loader_factories),
        std::move(subresource_overrides), std::move(controller),
        std::move(provider_info), std::move(prefetch_loader_factory),
        devtools_navigation_token,
        BuildCommitNavigationCallback(navigation_request));
  } else {
    DCHECK(!navigation_request);
    GetNavigationControl()->CommitNavigation(
        std::move(common_params), std::move(commit_params),
        std::move(response_head), std::move(response_body),
        std::move(url_loader_client_endpoints),
        std::move(subresource_loader_factories),
        std::move(subresource_overrides), std::move(controller),
        std::move(provider_info), std::move(prefetch_loader_factory),
        devtools_navigation_token,
        mojom::FrameNavigationControl::CommitNavigationCallback());
  }
}

void RenderFrameHostImpl::SendCommitFailedNavigation(
    mojom::NavigationClient* navigation_client,
    NavigationRequest* navigation_request,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    bool has_stale_copy_in_cache,
    int32_t error_code,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories) {
  DCHECK(navigation_client && navigation_request);
  navigation_client->CommitFailedNavigation(
      std::move(common_params), std::move(commit_params),
      has_stale_copy_in_cache, error_code,
      navigation_request->GetResolveErrorInfo(), error_page_content,
      std::move(subresource_loader_factories),
      BuildCommitFailedNavigationCallback(navigation_request));
}

// Called when the renderer navigates. For every frame loaded, we'll get this
// notification containing parameters identifying the navigation.
void RenderFrameHostImpl::DidCommitNavigation(
    std::unique_ptr<NavigationRequest> committing_navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  NavigationRequest* request;
  if (committing_navigation_request) {
    request = committing_navigation_request.get();
  } else {
    request = navigation_request();
  }

  if (request && request->IsNavigationStarted()) {
    main_frame_request_ids_ = {params->request_id,
                               request->GetGlobalRequestID()};
    if (deferred_main_frame_load_info_)
      ResourceLoadComplete(std::move(deferred_main_frame_load_info_));
  }
  // DidCommitProvisionalLoad IPC should be associated with the URL being
  // committed (not with the *last* committed URL that most other IPCs are
  // associated with).
  ScopedActiveURL scoped_active_url(
      params->url, frame_tree_node()->frame_tree()->root()->current_origin());

  ScopedCommitStateResetter commit_state_resetter(this);
  RenderProcessHost* process = GetProcess();

  TRACE_EVENT2("navigation", "RenderFrameHostImpl::DidCommitProvisionalLoad",
               "url", params->url.possibly_invalid_spec(), "details",
               CommitAsTracedValue(params.get()));

  // If we're waiting for a cross-site beforeunload completion callback from
  // this renderer and we receive a Navigate message from the main frame, then
  // the renderer was navigating already and sent it before hearing the
  // blink::mojom::LocalFrame::StopLoading() message. Treat this as an implicit
  // beforeunload completion callback to allow the pending navigation to
  // continue.
  if (is_waiting_for_beforeunload_completion_ &&
      unload_ack_is_for_navigation_ && !GetParent()) {
    base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
    ProcessBeforeUnloadCompleted(
        true /* proceed */, true /* treat_as_final_completion_callback */,
        approx_renderer_start_time, base::TimeTicks::Now());
  }

  // When a frame enters pending deletion, it waits for itself and its children
  // to properly unload. Receiving DidCommitProvisionalLoad() here while the
  // frame is not active means it comes from a navigation that reached the
  // ReadyToCommit stage just before the frame entered pending deletion.
  //
  // We should ignore this message, because we have already committed to
  // destroying this RenderFrameHost. Note that we intentionally do not ignore
  // commits that happen while the current tab is being closed - see
  // https://crbug.com/805705.
  if (!is_active())
    return;

  // Retroactive sanity check:
  // - If this is the first real load committing in this frame, then by this
  //   time the RenderFrameHost's InterfaceProvider implementation should have
  //   already been bound to a message pipe whose client end is used to service
  //   interface requests from the initial empty document.
  // - Otherwise, the InterfaceProvider implementation should at this point be
  //   bound to an interface connection servicing interface requests coming from
  //   the document of the previously committed navigation.
  DCHECK(document_scoped_interface_provider_receiver_.is_bound());
  if (interface_params) {
    // As a general rule, expect the RenderFrame to have supplied the
    // receiver end of a new InterfaceProvider connection that will be used by
    // the new document to issue interface receivers to access RenderFrameHost
    // services.
    document_scoped_interface_provider_receiver_.reset();
    BindInterfaceProviderReceiver(
        std::move(interface_params->interface_provider_receiver));
    if (broker_receiver_.is_bound()) {
      auto broker_receiver_of_previous_document = broker_receiver_.Unbind();
      dropped_interface_request_logger_ =
          std::make_unique<DroppedInterfaceRequestLogger>(
              std::move(broker_receiver_of_previous_document));
    }
    BindBrowserInterfaceBrokerReceiver(
        std::move(interface_params->browser_interface_broker_receiver));
  } else {
    // If there had already been a real load committed in the frame, and this is
    // not a same-document navigation, then both the active document as well as
    // the global object was replaced in this browsing context. The RenderFrame
    // should have rebound its InterfaceProvider to a new pipe, but failed to do
    // so. Kill the renderer, and reset the old receiver to ensure that any
    // pending interface requests originating from the previous document, hence
    // possibly from a different security origin, will no longer be dispatched.
    if (frame_tree_node_->has_committed_real_load()) {
      document_scoped_interface_provider_receiver_.reset();
      broker_receiver_.reset();
      bad_message::ReceivedBadMessage(
          process, bad_message::RFH_INTERFACE_PROVIDER_MISSING);
      return;
    }

    // Otherwise, it is the first real load committed, for which the RenderFrame
    // is allowed to, and will re-use the existing InterfaceProvider connection
    // if the new document is same-origin with the initial empty document, and
    // therefore the global object is not replaced.
  }

  if (!DidCommitNavigationInternal(std::move(committing_navigation_request),
                                   std::move(params),
                                   false /* is_same_document_navigation */)) {
    return;
  }

  // Since we didn't early return, it's safe to keep the commit state.
  commit_state_resetter.disable();

  // For a top-level frame, there are potential security concerns associated
  // with displaying graphics from a previously loaded page after the URL in
  // the omnibar has been changed. It is unappealing to clear the page
  // immediately, but if the renderer is taking a long time to issue any
  // compositor output (possibly because of script deliberately creating this
  // situation) then we clear it after a while anyway.
  // See https://crbug.com/497588.
  if (frame_tree_node_->IsMainFrame() && GetView()) {
    RenderWidgetHostImpl::From(GetView()->GetRenderWidgetHost())->DidNavigate();
  }

  // TODO(arthursonzogni): This can be removed when RenderDocument will be
  // implemented. See https://crbug.com/936696.
  EnsureDescendantsAreUnloading();
}

mojom::NavigationClient::CommitNavigationCallback
RenderFrameHostImpl::BuildCommitNavigationCallback(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  return base::BindOnce(
      &RenderFrameHostImpl::DidCommitPerNavigationMojoInterfaceNavigation,
      base::Unretained(this), navigation_request);
}

mojom::NavigationClient::CommitFailedNavigationCallback
RenderFrameHostImpl::BuildCommitFailedNavigationCallback(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  return base::BindOnce(
      &RenderFrameHostImpl::DidCommitPerNavigationMojoInterfaceNavigation,
      base::Unretained(this), navigation_request);
}

void RenderFrameHostImpl::SendBeforeUnload(
    bool is_reload,
    base::WeakPtr<RenderFrameHostImpl> rfh) {
  auto before_unload_closure = base::BindOnce(
      [](base::WeakPtr<RenderFrameHostImpl> impl, bool proceed,
         base::TimeTicks renderer_before_unload_start_time,
         base::TimeTicks renderer_before_unload_end_time) {
        if (!impl)
          return;
        impl->ProcessBeforeUnloadCompleted(
            proceed, false /* treat_as_final_completion_callback */,
            renderer_before_unload_start_time, renderer_before_unload_end_time);
      },
      rfh);
  rfh->GetAssociatedLocalFrame()->BeforeUnload(
      is_reload, std::move(before_unload_closure));
}

void RenderFrameHostImpl::AddServiceWorkerContainerHost(
    const std::string& uuid,
    base::WeakPtr<content::ServiceWorkerContainerHost> host) {
  if (IsInBackForwardCache()) {
    // RenderFrameHost entered BackForwardCache before adding
    // ServiceWorkerContainerHost. In this case, evict the entry from the cache.
    EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::
            kEnteredBackForwardCacheBeforeServiceWorkerHostAdded);
  }
  DCHECK(!base::Contains(service_worker_container_hosts_, uuid));
  service_worker_container_hosts_[uuid] = std::move(host);
}

void RenderFrameHostImpl::RemoveServiceWorkerContainerHost(
    const std::string& uuid) {
  DCHECK(base::Contains(service_worker_container_hosts_, uuid));
  service_worker_container_hosts_.erase(uuid);
}

void RenderFrameHostImpl::UpdateFrameFrozenState() {
  // TODO(http://crbug.com/1014212): remove this.
  CHECK(frame_);
  if (!IsFeatureEnabled(
          blink::mojom::FeaturePolicyFeature::kExecutionWhileNotRendered) &&
      visibility_ == blink::mojom::FrameVisibility::kNotRendered) {
    frame_->SetLifecycleState(blink::mojom::FrameLifecycleState::kFrozen);
  } else if (!IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::
                                   kExecutionWhileOutOfViewport) &&
             visibility_ ==
                 blink::mojom::FrameVisibility::kRenderedOutOfViewport) {
    frame_->SetLifecycleState(
        blink::mojom::FrameLifecycleState::kFrozenAutoResumeMedia);
  } else {
    frame_->SetLifecycleState(blink::mojom::FrameLifecycleState::kRunning);
  }
}

bool RenderFrameHostImpl::MaybeInterceptCommitCallback(
    NavigationRequest* navigation_request,
    FrameHostMsg_DidCommitProvisionalLoad_Params* params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params) {
  if (commit_callback_interceptor_) {
    return commit_callback_interceptor_->WillProcessDidCommitNavigation(
        navigation_request, params, interface_params);
  }
  return true;
}

void RenderFrameHostImpl::PostMessageEvent(
    const base::Optional<base::UnguessableToken>& source_token,
    const base::string16& source_origin,
    const base::string16& target_origin,
    blink::TransferableMessage message) {
  DCHECK(render_frame_created_);

  GetAssociatedLocalFrame()->PostMessageEvent(
      source_token, source_origin, target_origin, std::move(message));
}

bool RenderFrameHostImpl::IsTestRenderFrameHost() const {
  return false;
}

scoped_refptr<PrefetchedSignedExchangeCache>
RenderFrameHostImpl::EnsurePrefetchedSignedExchangeCache() {
  if (!prefetched_signed_exchange_cache_) {
    prefetched_signed_exchange_cache_ =
        base::MakeRefCounted<PrefetchedSignedExchangeCache>();
  }
  return prefetched_signed_exchange_cache_;
}

void RenderFrameHostImpl::ClearPrefetchedSignedExchangeCache() {
  if (prefetched_signed_exchange_cache_)
    prefetched_signed_exchange_cache_->Clear();
}

std::unique_ptr<WebBundleHandleTracker>
RenderFrameHostImpl::MaybeCreateWebBundleHandleTracker() {
  if (web_bundle_handle_)
    return web_bundle_handle_->MaybeCreateTracker();
  FrameTreeNode* frame_owner =
      frame_tree_node_->parent() ? frame_tree_node_->parent()->frame_tree_node()
                                 : frame_tree_node_->opener();
  if (!frame_owner)
    return nullptr;
  RenderFrameHostImpl* frame_owner_host = frame_owner->current_frame_host();
  if (!frame_owner_host->web_bundle_handle_)
    return nullptr;
  return frame_owner_host->web_bundle_handle_->MaybeCreateTracker();
}

RenderWidgetHostImpl* RenderFrameHostImpl::GetLocalRenderWidgetHost() const {
  if (!parent_)
    return render_view_host_->GetWidget();
  else
    return owned_render_widget_host_.get();
}

void RenderFrameHostImpl::EnsureDescendantsAreUnloading() {
  std::vector<RenderFrameHostImpl*> parents_to_be_checked = {this};
  std::vector<RenderFrameHostImpl*> rfhs_to_be_checked;
  while (!parents_to_be_checked.empty()) {
    RenderFrameHostImpl* document = parents_to_be_checked.back();
    parents_to_be_checked.pop_back();

    for (auto& subframe : document->children_) {
      RenderFrameHostImpl* child = subframe->current_frame_host();
      // Every child is expected to be pending deletion. If it isn't the
      // case, their FrameTreeNode is immediately removed from the tree.
      if (!child->IsPendingDeletion())
        rfhs_to_be_checked.push_back(child);
      else
        parents_to_be_checked.push_back(child);
    }
  }
  for (RenderFrameHostImpl* document : rfhs_to_be_checked)
    document->parent_->RemoveChild(document->frame_tree_node());
}

void RenderFrameHostImpl::AddUniqueMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  AddMessageToConsoleImpl(level, message, true /* discard_duplicates */);
}

void RenderFrameHostImpl::AddMessageToConsoleImpl(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message,
    bool discard_duplicates) {
  GetAssociatedLocalFrame()->AddMessageToConsole(level, message,
                                                 discard_duplicates);
}

void RenderFrameHostImpl::AddSameSiteCookieDeprecationMessage(
    const std::string& cookie_url,
    net::CanonicalCookie::CookieInclusionStatus status,
    bool is_lax_by_default_enabled,
    bool is_none_requires_secure_enabled) {
  std::string deprecation_message;
  // The status will have, at most, one of these warning messages at any given
  // time.
  if (status.HasWarningReason(
          net::CanonicalCookie::CookieInclusionStatus::WarningReason::
              WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT)) {
    if (!ShouldAddCookieSameSiteDeprecationMessage(
            cookie_url, &cookie_no_samesite_deprecation_url_hashes_)) {
      return;
    }
    std::string warning_or_blocked_message =
        (is_lax_by_default_enabled
             ? "It has been blocked, as Chrome now only delivers "
             : "A future release of Chrome will only deliver ");
    deprecation_message =
        "A cookie associated with a cross-site resource at " + cookie_url +
        " was set without the `SameSite` attribute. " +
        warning_or_blocked_message +
        "cookies with "
        "cross-site requests if they are set with `SameSite=None` and "
        "`Secure`. You can review cookies in developer tools under "
        "Application>Storage>Cookies and see more details at "
        "https://www.chromestatus.com/feature/5088147346030592 and "
        "https://www.chromestatus.com/feature/5633521622188032.";
  } else if (status.HasWarningReason(
                 net::CanonicalCookie::CookieInclusionStatus::WarningReason::
                     WARN_SAMESITE_NONE_INSECURE)) {
    if (!ShouldAddCookieSameSiteDeprecationMessage(
            cookie_url,
            &cookie_samesite_none_insecure_deprecation_url_hashes_)) {
      return;
    }
    std::string warning_or_blocked_message =
        (is_none_requires_secure_enabled
             ? "It has been blocked, as Chrome now only delivers "
             : "A future release of Chrome will only deliver ");
    deprecation_message =
        "A cookie associated with a resource at " + cookie_url +
        " was set with `SameSite=None` but without `Secure`. " +
        warning_or_blocked_message +
        "cookies marked "
        "`SameSite=None` if they are also marked `Secure`. You "
        "can review cookies in developer tools under "
        "Application>Storage>Cookies and see more details at "
        "https://www.chromestatus.com/feature/5633521622188032.";
  } else if (status.HasWarningReason(
                 net::CanonicalCookie::CookieInclusionStatus::WarningReason::
                     WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE)) {
    if (!ShouldAddCookieSameSiteDeprecationMessage(
            cookie_url, &cookie_lax_allow_unsafe_deprecation_url_hashes_)) {
      return;
    }
    deprecation_message =
        "A cookie associated with a resource at " + cookie_url +
        " set without a `SameSite` attribute was sent with a non-idempotent "
        "top-level cross-site request because it was less than " +
        base::NumberToString(net::kLaxAllowUnsafeMaxAge.InMinutes()) +
        " minutes old. A future release of Chrome will treat such cookies as "
        "if they were set with `SameSite=Lax` and will only allow them to be "
        "sent with top-level cross-site requests if the HTTP method is safe. "
        "See more details at "
        "https://www.chromestatus.com/feature/5088147346030592.";
  }

  if (deprecation_message.empty())
    return;

  AddUniqueMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                            deprecation_message);
}

void RenderFrameHostImpl::AddInspectorIssue(
    blink::mojom::InspectorIssueInfoPtr info) {
  GetAssociatedLocalFrame()->AddInspectorIssue(std::move(info));
}

bool RenderFrameHostImpl::ShouldAddCookieSameSiteDeprecationMessage(
    const std::string& cookie_url,
    base::circular_deque<size_t>* already_seen_url_hashes) {
  DCHECK_LE(already_seen_url_hashes->size(), kMaxCookieSameSiteDeprecationUrls);
  size_t cookie_url_hash = base::FastHash(cookie_url);
  if (base::Contains(*already_seen_url_hashes, cookie_url_hash))
    return false;

  // Put most recent ones at the front because we are likely to have multiple
  // consecutive cookies with the same URL.
  if (already_seen_url_hashes->size() == kMaxCookieSameSiteDeprecationUrls)
    already_seen_url_hashes->pop_back();
  already_seen_url_hashes->push_front(cookie_url_hash);
  return true;
}

void RenderFrameHostImpl::LogCannotCommitUrlCrashKeys(
    const GURL& url,
    bool is_same_document_navigation,
    NavigationRequest* navigation_request) {
  LogRendererKillCrashKeys(GetSiteInstance()->GetSiteURL());

  // Temporary instrumentation to debug the root cause of renderer process
  // terminations. See https://crbug.com/931895.
  auto bool_to_crash_key = [](bool b) { return b ? "true" : "false"; };
  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_same_document",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(is_same_document_navigation));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_main_frame",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(frame_tree_node_->IsMainFrame()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_cross_process_subframe",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(IsCrossProcessSubframe()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("site_lock",
                                          base::debug::CrashKeySize::Size256),
      GetSiteInstance()->lock_url().possibly_invalid_spec());

  if (!GetSiteInstance()->IsDefaultSiteInstance()) {
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("original_url_origin",
                                            base::debug::CrashKeySize::Size256),
        GetSiteInstance()->original_url().GetOrigin().spec());
  }

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_mhtml_document",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(is_mhtml_document()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("last_committed_url_origin",
                                          base::debug::CrashKeySize::Size256),
      GetLastCommittedURL().GetOrigin().spec());

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("last_successful_url_origin",
                                          base::debug::CrashKeySize::Size256),
      last_successful_url().GetOrigin().spec());

  if (navigation_request && navigation_request->IsNavigationStarted()) {
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_renderer_initiated",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsRendererInitiated()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_server_redirect",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->WasServerRedirect()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_form_submission",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsFormSubmission()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_error_page",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsErrorPage()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("from_begin_navigation",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->from_begin_navigation()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("net_error",
                                            base::debug::CrashKeySize::Size32),
        base::NumberToString(navigation_request->GetNetErrorCode()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("initiator_origin",
                                            base::debug::CrashKeySize::Size64),
        navigation_request->GetInitiatorOrigin()
            ? navigation_request->GetInitiatorOrigin()->GetDebugString()
            : "none");

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("starting_site_instance",
                                            base::debug::CrashKeySize::Size64),
        navigation_request->GetStartingSiteInstance()
            ->GetSiteURL()
            .possibly_invalid_spec());

    // Recompute the target SiteInstance to see if it matches the current
    // one at commit time.
    scoped_refptr<SiteInstance> dest_instance =
        frame_tree_node_->render_manager()->GetSiteInstanceForNavigationRequest(
            navigation_request);
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString(
            "does_recomputed_site_instance_match",
            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(dest_instance == GetSiteInstance()));
  }
}

void RenderFrameHostImpl::MaybeEvictFromBackForwardCache() {
  if (!IsInBackForwardCache())
    return;

  RenderFrameHostImpl* top_document = this;
  while (RenderFrameHostImpl* parent = top_document->GetParent())
    top_document = parent;

  NavigationControllerImpl* controller = static_cast<NavigationControllerImpl*>(
      frame_tree_node_->navigator()->GetController());
  auto can_store =
      controller->GetBackForwardCache().CanStoreDocument(top_document);
  TRACE_EVENT1("navigation",
               "RenderFrameHostImpl::MaybeEvictFromBackForwardCache",
               "can_store", can_store.ToString());

  if (can_store)
    return;

  EvictFromBackForwardCacheWithReasons(can_store);
}

void RenderFrameHostImpl::LogCannotCommitOriginCrashKeys(
    bool is_same_document_navigation,
    NavigationRequest* navigation_request) {
  LogRendererKillCrashKeys(GetSiteInstance()->GetSiteURL());

  // Temporary instrumentation to debug the root cause of
  // https://crbug.com/923144.
  auto bool_to_crash_key = [](bool b) { return b ? "true" : "false"; };
  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_same_document",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(is_same_document_navigation));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_subframe",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(!frame_tree_node_->IsMainFrame()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_active",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(is_active()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_current",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(IsCurrent()));

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_cross_process_subframe",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(IsCrossProcessSubframe()));

  if (navigation_request && navigation_request->IsNavigationStarted()) {
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_renderer_initiated",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsRendererInitiated()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_server_redirect",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->WasServerRedirect()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_form_submission",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsFormSubmission()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_error_page",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->IsErrorPage()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("initiator_origin",
                                            base::debug::CrashKeySize::Size64),
        navigation_request->GetInitiatorOrigin()
            ? navigation_request->GetInitiatorOrigin()->GetDebugString()
            : "none");

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("starting_site_instance",
                                            base::debug::CrashKeySize::Size64),
        navigation_request->GetStartingSiteInstance()
            ->GetSiteURL()
            .possibly_invalid_spec());
  }
}

void RenderFrameHostImpl::EnableMojoJsBindings() {
  // This method should only be called on RenderFrameHost which is for a WebUI.
  DCHECK_NE(WebUI::kNoWebUI,
            WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
                GetSiteInstance()->GetBrowserContext(),
                site_instance_->GetSiteURL()));

  if (!frame_bindings_control_)
    GetRemoteAssociatedInterfaces()->GetInterface(&frame_bindings_control_);
  frame_bindings_control_->EnableMojoJsBindings();
}

BackForwardCacheMetrics* RenderFrameHostImpl::GetBackForwardCacheMetrics() {
  NavigationEntryImpl* navigation_entry =
      static_cast<NavigationControllerImpl*>(
          frame_tree_node_->navigator()->GetController())
          ->GetEntryWithUniqueID(nav_entry_id());
  if (!navigation_entry)
    return nullptr;
  return navigation_entry->back_forward_cache_metrics();
}

bool RenderFrameHostImpl::IsBackForwardCacheDisabled() const {
  return back_forward_cache_disabled_reasons_.size();
}

bool RenderFrameHostImpl::IsDOMContentLoaded() {
  return dom_content_loaded_;
}

void RenderFrameHostImpl::UpdateAdFrameType(
    blink::mojom::AdFrameType ad_frame_type) {
  frame_tree_node_->SetAdFrameType(ad_frame_type);
}

blink::mojom::AuthenticatorStatus
RenderFrameHostImpl::PerformGetAssertionWebAuthSecurityChecks(
    const std::string& relying_party_id,
    const url::Origin& effective_origin) {
  bool is_cross_origin;
  blink::mojom::AuthenticatorStatus status =
      GetWebAuthRequestSecurityChecker()->ValidateAncestorOrigins(
          effective_origin,
          WebAuthRequestSecurityChecker::RequestType::kGetAssertion,
          &is_cross_origin);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return status;
  }

  status = GetWebAuthRequestSecurityChecker()->ValidateDomainAndRelyingPartyID(
      effective_origin, relying_party_id);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return status;
  }

  return blink::mojom::AuthenticatorStatus::SUCCESS;
}

blink::mojom::AuthenticatorStatus
RenderFrameHostImpl::PerformMakeCredentialWebAuthSecurityChecks(
    const std::string& relying_party_id,
    const url::Origin& effective_origin) {
  bool is_cross_origin;
  blink::mojom::AuthenticatorStatus status =
      GetWebAuthRequestSecurityChecker()->ValidateAncestorOrigins(
          effective_origin,
          WebAuthRequestSecurityChecker::RequestType::kMakeCredential,
          &is_cross_origin);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return status;
  }

  status = GetWebAuthRequestSecurityChecker()->ValidateDomainAndRelyingPartyID(
      effective_origin, relying_party_id);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return status;
  }

  return blink::mojom::AuthenticatorStatus::SUCCESS;
}

void RenderFrameHostImpl::IsClipboardPasteAllowed(
    const ui::ClipboardFormatType& data_type,
    const std::string& data,
    IsClipboardPasteAllowedCallback callback) {
  delegate_->IsClipboardPasteAllowed(GetLastCommittedURL(), data_type, data,
                                     std::move(callback));
}

RenderFrameHostImpl* RenderFrameHostImpl::ParentOrOuterDelegateFrame() {
  // Find the parent in the FrameTree (iframe).
  if (parent_)
    return parent_;

  // Find the parent in the WebContentsTree (GuestView or Portal).
  FrameTreeNode* frame_in_embedder =
      frame_tree_node()->render_manager()->GetOuterDelegateNode();
  if (frame_in_embedder)
    return frame_in_embedder->current_frame_host()->GetParent();

  // No parent found.
  return nullptr;
}

scoped_refptr<WebAuthRequestSecurityChecker>
RenderFrameHostImpl::GetWebAuthRequestSecurityChecker() {
  if (!webauth_request_security_checker_)
    webauth_request_security_checker_ =
        base::MakeRefCounted<WebAuthRequestSecurityChecker>(this);

  return webauth_request_security_checker_;
}

bool RenderFrameHostImpl::IsInBackForwardCache() {
  return lifecycle_state_ == LifecycleState::kInBackForwardCache;
}

bool RenderFrameHostImpl::IsPendingDeletion() {
  return lifecycle_state_ == LifecycleState::kRunningUnloadHandlers ||
         lifecycle_state_ == LifecycleState::kReadyToBeDeleted;
}

void RenderFrameHostImpl::SetLifecycleStateToActive() {
  // If the RenderFrameHost is restored from BackForwardCache, update states of
  // all the children to kActive. This is called from
  // RenderFrameHostManager::SetRenderFrameHost which happens after commit.
  if (IsInBackForwardCache()) {
    for (auto& child : children_) {
      DCHECK_EQ(child->current_frame_host()->lifecycle_state_,
                LifecycleState::kInBackForwardCache);
      child->current_frame_host()->SetLifecycleStateToActive();
    }
  }

  SetLifecycleState(LifecycleState::kActive);
}

void RenderFrameHostImpl::SetLifecycleState(LifecycleState state) {
  // Cross-verify that |lifecycle_state_| transition happens correctly.
  switch (state) {
    case LifecycleState::kSpeculative:
      // RenderFrameHost is only set speculative during its creation and no
      // transitions happen to this state during its lifetime.
      NOTREACHED();
      break;
    case LifecycleState::kActive:
      DCHECK(lifecycle_state_ == LifecycleState::kSpeculative ||
             lifecycle_state_ == LifecycleState::kInBackForwardCache)
          << "Unexpected LifeCycleState " << int(lifecycle_state_);
      break;
    case LifecycleState::kInBackForwardCache:
      DCHECK_EQ(lifecycle_state_, LifecycleState::kActive)
          << "Unexpected LifeCycleState " << int(lifecycle_state_);
      break;
    case LifecycleState::kRunningUnloadHandlers:
      DCHECK_EQ(lifecycle_state_, LifecycleState::kActive)
          << "Unexpected LifeCycleState " << int(lifecycle_state_);
      break;
    case LifecycleState::kReadyToBeDeleted:
      DCHECK_NE(lifecycle_state_, LifecycleState::kReadyToBeDeleted)
          << "Unexpected LifeCycleState " << int(lifecycle_state_);
      break;
  }
  lifecycle_state_ = state;
}

void RenderFrameHostImpl::BindReportingObserver(
    mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver) {
  GetAssociatedLocalFrame()->BindReportingObserver(std::move(receiver));
}

mojo::PendingRemote<network::mojom::CookieAccessObserver>
RenderFrameHostImpl::CreateCookieAccessObserver() {
  mojo::PendingRemote<network::mojom::CookieAccessObserver> remote;
  cookie_observers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void RenderFrameHostImpl::Clone(
    mojo::PendingReceiver<network::mojom::CookieAccessObserver> observer) {
  cookie_observers_.Add(this, std::move(observer));
}

void RenderFrameHostImpl::OnCookiesAccessed(
    network::mojom::CookieAccessDetailsPtr details) {
  EmitSameSiteCookiesDeprecationWarning(this, details);

  CookieAccessDetails allowed;
  CookieAccessDetails blocked;
  SplitCookiesIntoAllowedAndBlocked(details, &allowed, &blocked);
  if (!allowed.cookie_list.empty())
    delegate_->OnCookiesAccessed(this, allowed);
  if (!blocked.cookie_list.empty())
    delegate_->OnCookiesAccessed(this, blocked);
}

}  // namespace content
