// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_impl.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/hash/hash.h"
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
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/browser/about_url_loader_factory.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/contacts/contacts_manager_impl.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/file_url_loader_factory.h"
#include "content/browser/fileapi/file_system_manager_impl.h"
#include "content/browser/fileapi/file_system_url_loader_factory.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/input/input_injector_impl.h"
#include "content/browser/frame_host/ipc_utils.h"
#include "content/browser/frame_host/keep_alive_handle_factory.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/generic_sensor/sensor_provider_proxy_impl.h"
#include "content/browser/geolocation/geolocation_service_impl.h"
#include "content/browser/image_capture/image_capture_impl.h"
#include "content/browser/installedapp/installed_app_provider_impl_default.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/keyboard_lock/keyboard_lock_service_impl.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/log_console_message.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_interface_proxy.h"
#include "content/browser/media/session/media_session_service_impl.h"
#include "content/browser/media/webaudio/audio_context_manager_impl.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/portal/portal.h"
#include "content/browser/presentation/presentation_service_impl.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/quota_dispatcher_host.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/input/input_router.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/media/audio_input_delegate_impl.h"
#include "content/browser/renderer_host/media/media_devices_dispatcher_host.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/browser/scoped_active_url.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/sms/sms_service.h"
#include "content/browser/speech/speech_recognition_dispatcher_host.h"
#include "content/browser/speech/speech_synthesis_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/wake_lock/wake_lock_service_impl.h"
#include "content/browser/web_package/bundled_exchanges_handle.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_url_loader_factory_internal.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/shared_worker_connector_impl.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/common/accessibility_messages.h"
#include "content/common/associated_interfaces.mojom.h"
#include "content/common/content_security_policy/content_security_policy.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params_mojom_traits.h"
#include "content/common/navigation_params_utils.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/swapped_out_messages.h"
#include "content/common/unfreezable_frame_messages.h"
#include "content/common/widget.mojom.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page_visibility_state.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/webvr_service_provider.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/mime_handler_view_mode.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/referrer_type_converters.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "device/gamepad/gamepad_monitor.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/base/user_input_monitor.h"
#include "media/learning/common/value.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/services/media_interface_provider.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_constants.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_host_test_interface.mojom.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"
#include "third_party/blink/public/mojom/loader/url_loader_factory_bundle.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"
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
#include "content/browser/serial/serial_service.h"
#endif

#if defined(OS_MACOSX)
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#endif

using base::TimeDelta;

namespace content {

namespace {

#if defined(OS_ANDROID)
const void* const kRenderFrameHostAndroidKey = &kRenderFrameHostAndroidKey;
#endif  // OS_ANDROID

// The next value to use for the accessibility reset token.
int g_next_accessibility_reset_token = 1;

#if defined(OS_ANDROID) || defined(OS_FUCHSIA) || defined(IS_CHROMECAST)
// Whether to allow injecting javascript into any kind of frame, for Android
// WebView, Fuchsia web.ContextProvider and CastOS content shell.
bool g_allow_injecting_javascript = false;
#endif

// The (process id, routing id) pair that identifies one RenderFrame.
typedef std::pair<int32_t, int32_t> RenderFrameHostID;
typedef std::unordered_map<RenderFrameHostID,
                           RenderFrameHostImpl*,
                           base::IntPairHash<RenderFrameHostID>>
    RoutingIDFrameMap;
base::LazyInstance<RoutingIDFrameMap>::DestructorAtExit g_routing_id_frame_map =
    LAZY_INSTANCE_INITIALIZER;

RenderFrameHostImpl::CreateNetworkFactoryCallback&
GetCreateNetworkFactoryCallbackForRenderFrame() {
  static base::NoDestructor<RenderFrameHostImpl::CreateNetworkFactoryCallback>
      s_callback;
  return *s_callback;
}

using TokenFrameMap = std::unordered_map<base::UnguessableToken,
                                         RenderFrameHostImpl*,
                                         base::UnguessableTokenHash>;
base::LazyInstance<TokenFrameMap>::Leaky g_token_frame_map =
    LAZY_INSTANCE_INITIALIZER;

// Translate a WebKit text direction into a base::i18n one.
base::i18n::TextDirection WebTextDirectionToChromeTextDirection(
    blink::WebTextDirection dir) {
  switch (dir) {
    case blink::kWebTextDirectionLeftToRight:
      return base::i18n::LEFT_TO_RIGHT;
    case blink::kWebTextDirectionRightToLeft:
      return base::i18n::RIGHT_TO_LEFT;
    default:
      NOTREACHED();
      return base::i18n::UNKNOWN_DIRECTION;
  }
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

  static void Bind(int process_id,
                   int routing_id,
                   media::mojom::RemoterFactoryRequest request) {
    mojo::MakeStrongBinding(
        std::make_unique<RemoterFactoryImpl>(process_id, routing_id),
        std::move(request));
  }

 private:
  void Create(media::mojom::RemotingSourcePtr source,
              media::mojom::RemoterRequest request) final {
    if (auto* host = RenderFrameHostImpl::FromID(process_id_, routing_id_)) {
      GetContentClient()->browser()->CreateMediaRemoter(
          host, std::move(source), std::move(request));
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

void LookupRenderFrameHostOrProxy(int process_id,
                                  int routing_id,
                                  RenderFrameHostImpl** rfh,
                                  RenderFrameProxyHost** rfph) {
  *rfh = RenderFrameHostImpl::FromID(process_id, routing_id);
  if (*rfh == nullptr)
    *rfph = RenderFrameProxyHost::FromID(process_id, routing_id);
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
  base::debug::SetCrashKeyString(site_url_key, site_url.spec());
}

base::Optional<url::Origin> GetOriginForURLLoaderFactoryUnchecked(
    NavigationRequest* navigation_request) {
  // Return a safe opaque origin when there is no |navigation_request| (e.g.
  // when RFHI::CommitNavigation is called via RFHI::NavigateToInterstitialURL).
  if (!navigation_request)
    return url::Origin();

  // GetOriginForURLLoaderFactory should only be called at the ready-to-commit
  // time, when the RFHI and process to commit the navigation are already known.
  DCHECK_LE(NavigationRequest::RESPONSE_STARTED, navigation_request->state());
  RenderFrameHostImpl* target_frame = navigation_request->render_frame_host();

  // Check if this is loadDataWithBaseUrl (which needs special treatment).
  auto& common_params = navigation_request->common_params();
  if (common_params.url.SchemeIs(url::kDataScheme) &&
      !common_params.base_url_for_data_url.is_empty()) {
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

  // TODO(lukasza, nasko): https://crbug.com/888079: Use exact origin, instead
  // of falling back to site URL for about:blank and about:srcdoc.
  if (common_params.url.SchemeIs(url::kAboutScheme)) {
    // |site_instance|'s site URL cannot be used as
    // |request_initiator_site_lock| unless the site requires a dedicated
    // process.  Otherwise b.com may share a process associated with a.com, in
    // a SiteInstance with |site_url| set to "http://a.com" (and/or
    // "http://nonisolated.invalid" in the future) and in that scenario
    // |request_initiator| for requests from b.com should NOT be locked to
    // a.com.
    SiteInstanceImpl* site_instance = target_frame->GetSiteInstance();
    if (!SiteInstanceImpl::ShouldLockToOrigin(
            site_instance->GetIsolationContext(), site_instance->GetSiteURL()))
      return base::nullopt;
    return SiteInstanceImpl::GetRequestInitiatorSiteLock(
        site_instance->GetSiteURL());
  }

  // In cases not covered above, URLLoaderFactory should be associated with the
  // origin of |common_params.url|.  This works fine for all URLs, including
  // data: URLs (which should use an opaque origin for their subresource
  // requests) and blob: URLs (which embed their origin inside the URL).
  return url::Origin::Create(common_params.url);
}

base::Optional<url::Origin> GetOriginForURLLoaderFactory(
    NavigationRequest* navigation_request) {
  base::Optional<url::Origin> result =
      GetOriginForURLLoaderFactoryUnchecked(navigation_request);

  // Any non-opaque |result| must be an origin that is allowed to be accessed
  // from the process that is the target of this navigation.
  if (result.has_value() && !result->opaque()) {
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    CHECK(policy->CanAccessDataForOrigin(
        navigation_request->render_frame_host()->GetProcess()->GetID(),
        *result));
  }

  return result;
}

std::unique_ptr<blink::URLLoaderFactoryBundleInfo> CloneFactoryBundle(
    scoped_refptr<blink::URLLoaderFactoryBundle> bundle) {
  return base::WrapUnique(static_cast<blink::URLLoaderFactoryBundleInfo*>(
      bundle->Clone().release()));
}

void GetRestrictedCookieManager(
    RenderFrameHostImpl* frame_host,
    int process_id,
    int frame_id,
    StoragePartition* storage_partition,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  storage_partition->CreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole::SCRIPT,
      frame_host->GetLastCommittedOrigin(),
      /* is_service_worker = */ false, process_id, frame_id,
      std::move(receiver));
}

// TODO(crbug.com/977040): Remove when no longer needed.
const uint32_t kMaxCookieSameSiteDeprecationUrls = 20;

}  // namespace

class RenderFrameHostImpl::DroppedInterfaceRequestLogger
    : public service_manager::mojom::InterfaceProvider {
 public:
  DroppedInterfaceRequestLogger(
      service_manager::mojom::InterfaceProviderRequest request)
      : binding_(this) {
    binding_.Bind(std::move(request));
  }

  ~DroppedInterfaceRequestLogger() override {
    UMA_HISTOGRAM_EXACT_LINEAR("RenderFrameHostImpl.DroppedInterfaceRequests",
                               num_dropped_requests_, 20);
  }

 protected:
  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle pipe) override {
    ++num_dropped_requests_;
    base::UmaHistogramSparse(
        "RenderFrameHostImpl.DroppedInterfaceRequestName",
        HashInterfaceNameToHistogramSample(interface_name));
    DLOG(WARNING)
        << "InterfaceRequest was dropped, the document is no longer active: "
        << interface_name;
  }

 private:
  mojo::Binding<service_manager::mojom::InterfaceProvider> binding_;
  int num_dropped_requests_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DroppedInterfaceRequestLogger);
};

struct PendingNavigation {
  mojom::CommonNavigationParamsPtr common_params;
  mojom::BeginNavigationParamsPtr begin_navigation_params;
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  mojom::NavigationClientAssociatedPtrInfo navigation_client;
  mojo::PendingRemote<blink::mojom::NavigationInitiator> navigation_initiator;

  PendingNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_navigation_params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojom::NavigationClientAssociatedPtrInfo navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>
          navigation_initiator);
};

PendingNavigation::PendingNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::BeginNavigationParamsPtr begin_navigation_params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojom::NavigationClientAssociatedPtrInfo navigation_client,
    mojo::PendingRemote<blink::mojom::NavigationInitiator> navigation_initiator)
    : common_params(std::move(common_params)),
      begin_navigation_params(std::move(begin_navigation_params)),
      blob_url_loader_factory(std::move(blob_url_loader_factory)),
      navigation_client(std::move(navigation_client)),
      navigation_initiator(std::move(navigation_initiator)) {}

// An implementation of blink::mojom::FileChooser and FileSelectListener
// associated to RenderFrameHost.
class FileChooserImpl : public blink::mojom::FileChooser,
                        public content::WebContentsObserver {
  using FileChooserResult = blink::mojom::FileChooserResult;

 public:
  static void Create(RenderFrameHostImpl* render_frame_host,
                     blink::mojom::FileChooserRequest request) {
    mojo::MakeStrongBinding(
        std::make_unique<FileChooserImpl>(render_frame_host),
        std::move(request));
  }

  FileChooserImpl(RenderFrameHostImpl* render_frame_host)
      : render_frame_host_(render_frame_host) {
    Observe(WebContents::FromRenderFrameHost(render_frame_host));
  }

  ~FileChooserImpl() override {
    if (proxy_)
      proxy_->ResetOwner();
  }

  void OpenFileChooser(blink::mojom::FileChooserParamsPtr params,
                       OpenFileChooserCallback callback) override {
    if (proxy_ || !render_frame_host_) {
      std::move(callback).Run(nullptr);
      return;
    }
    callback_ = std::move(callback);
    auto listener = std::make_unique<ListenerProxy>(this);
    proxy_ = listener.get();
    // Do not allow messages with absolute paths in them as this can permit a
    // renderer to coerce the browser to perform I/O on a renderer controlled
    // path.
    if (params->default_file_name != params->default_file_name.BaseName()) {
      mojo::ReportBadMessage(
          "FileChooser: The default file name should not be an absolute path.");
      listener->FileSelectionCanceled();
      return;
    }
    render_frame_host_->delegate()->RunFileChooser(
        render_frame_host_, std::move(listener), *params);
  }

  void EnumerateChosenDirectory(
      const base::FilePath& directory_path,
      EnumerateChosenDirectoryCallback callback) override {
    if (proxy_ || !render_frame_host_) {
      std::move(callback).Run(nullptr);
      return;
    }
    callback_ = std::move(callback);
    auto listener = std::make_unique<ListenerProxy>(this);
    proxy_ = listener.get();
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    if (policy->CanReadFile(render_frame_host_->GetProcess()->GetID(),
                            directory_path)) {
      render_frame_host_->delegate()->EnumerateDirectory(
          render_frame_host_, std::move(listener), directory_path);
    } else {
      listener->FileSelectionCanceled();
    }
  }

  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) {
    proxy_ = nullptr;
    if (!render_frame_host_)
      return;
    storage::FileSystemContext* file_system_context = nullptr;
    const int pid = render_frame_host_->GetProcess()->GetID();
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    // Grant the security access requested to the given files.
    for (const auto& file : files) {
      if (mode == blink::mojom::FileChooserParams::Mode::kSave) {
        policy->GrantCreateReadWriteFile(pid,
                                         file->get_native_file()->file_path);
      } else {
        if (file->is_file_system()) {
          if (!file_system_context) {
            file_system_context =
                BrowserContext::GetStoragePartition(
                    render_frame_host_->GetProcess()->GetBrowserContext(),
                    render_frame_host_->GetSiteInstance())
                    ->GetFileSystemContext();
          }
          policy->GrantReadFileSystem(
              pid, file_system_context->CrackURL(file->get_file_system()->url)
                       .mount_filesystem_id());
        } else {
          policy->GrantReadFile(pid, file->get_native_file()->file_path);
        }
      }
    }
    std::move(callback_).Run(
        FileChooserResult::New(std::move(files), base_dir));
  }

  void FileSelectionCanceled() {
    proxy_ = nullptr;
    if (!render_frame_host_)
      return;
    std::move(callback_).Run(nullptr);
  }

 private:
  class ListenerProxy : public content::FileSelectListener {
   public:
    explicit ListenerProxy(FileChooserImpl* owner) : owner_(owner) {}
    ~ListenerProxy() override {
#if DCHECK_IS_ON()
      DCHECK(was_file_select_listener_function_called_)
          << "Should call either FileSelectListener::FileSelected() or "
             "FileSelectListener::FileSelectionCanceled()";
#endif
    }
    void ResetOwner() { owner_ = nullptr; }

    // FileSelectListener overrides:

    void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                      const base::FilePath& base_dir,
                      blink::mojom::FileChooserParams::Mode mode) override {
#if DCHECK_IS_ON()
      DCHECK(!was_file_select_listener_function_called_)
          << "Should not call both of FileSelectListener::FileSelected() and "
             "FileSelectListener::FileSelectionCanceled()";
      was_file_select_listener_function_called_ = true;
#endif
      if (owner_)
        owner_->FileSelected(std::move(files), base_dir, mode);
    }

    void FileSelectionCanceled() override {
#if DCHECK_IS_ON()
      DCHECK(!was_file_select_listener_function_called_)
          << "Should not call both of FileSelectListener::FileSelected() and "
             "FileSelectListener::FileSelectionCanceled()";
      was_file_select_listener_function_called_ = true;
#endif
      if (owner_)
        owner_->FileSelectionCanceled();
    }

   private:
    FileChooserImpl* owner_;
#if DCHECK_IS_ON()
    bool was_file_select_listener_function_called_ = false;
#endif
  };

  // content::WebContentsObserver overrides:

  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override {
    if (old_host == render_frame_host_)
      render_frame_host_ = nullptr;
  }

  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    if (render_frame_host == render_frame_host_)
      render_frame_host_ = nullptr;
  }

  void WebContentsDestroyed() override { render_frame_host_ = nullptr; }

  RenderFrameHostImpl* render_frame_host_;
  ListenerProxy* proxy_ = nullptr;
  base::OnceCallback<void(blink::mojom::FileChooserResultPtr)> callback_;
};

// static
RenderFrameHost* RenderFrameHost::FromID(int render_process_id,
                                         int render_frame_id) {
  return RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
}

#if defined(OS_ANDROID) || defined(OS_FUCHSIA) || defined(IS_CHROMECAST)
// static
void RenderFrameHost::AllowInjectingJavaScript() {
  g_allow_injecting_javascript = true;
}
#endif  // defined(OS_ANDROID) || defined(OS_FUCHSIA) || defined(IS_CHROMECAST)

// static
RenderFrameHostImpl* RenderFrameHostImpl::FromID(int process_id,
                                                 int routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RoutingIDFrameMap* frames = g_routing_id_frame_map.Pointer();
  auto it = frames->find(RenderFrameHostID(process_id, routing_id));
  return it == frames->end() ? NULL : it->second;
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
void RenderFrameHostImpl::SetNetworkFactoryForTesting(
    const CreateNetworkFactoryCallback& create_network_factory_callback) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(create_network_factory_callback.is_null() ||
         GetCreateNetworkFactoryCallbackForRenderFrame().is_null())
      << "It is not expected that this is called with non-null callback when "
      << "another overriding callback is already set.";
  GetCreateNetworkFactoryCallbackForRenderFrame() =
      create_network_factory_callback;
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
    int32_t widget_routing_id,
    bool renderer_initiated_creation)
    : render_view_host_(std::move(render_view_host)),
      delegate_(delegate),
      site_instance_(static_cast<SiteInstanceImpl*>(site_instance)),
      process_(site_instance->GetProcess()),
      frame_tree_(frame_tree),
      frame_tree_node_(frame_tree_node),
      parent_(nullptr),
      routing_id_(routing_id),
      is_waiting_for_swapout_ack_(false),
      render_frame_created_(false),
      is_waiting_for_beforeunload_ack_(false),
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
      pending_web_ui_type_(WebUI::kNoWebUI),
      should_reuse_web_ui_(false),
      has_selection_(false),
      is_audible_(false),
      last_navigation_previews_state_(PREVIEWS_UNSPECIFIED),
      frame_host_associated_binding_(this),
      waiting_for_init_(renderer_initiated_creation),
      has_focused_editable_element_(false),
      push_messaging_manager_(
          nullptr,
          base::OnTaskRunnerDeleter(base::CreateSequencedTaskRunner(
              {ServiceWorkerContext::GetCoreThreadId()}))),
      active_sandbox_flags_(blink::WebSandboxFlags::kNone),
      document_scoped_interface_provider_binding_(this),
      keep_alive_timeout_(base::TimeDelta::FromSeconds(30)),
      subframe_unload_timeout_(base::TimeDelta::FromMilliseconds(
          RenderViewHostImpl::kUnloadTimeoutMS)),
      commit_callback_interceptor_(nullptr) {
  GetProcess()->AddRoute(routing_id_, this);
  g_routing_id_frame_map.Get().emplace(
      RenderFrameHostID(GetProcess()->GetID(), routing_id_), this);
  site_instance_->AddObserver(this);
  process_->AddObserver(this);
  GetSiteInstance()->IncrementActiveFrameCount();

  if (frame_tree_node_->parent()) {
    // Keep track of the parent RenderFrameHost, which shouldn't change even if
    // this RenderFrameHost is on the pending deletion list and the parent
    // FrameTreeNode has changed its current RenderFrameHost.
    parent_ = frame_tree_node_->parent()->current_frame_host();

    // All frames in a page are expected to have the same bindings.
    if (parent_->GetEnabledBindings())
      enabled_bindings_ = parent_->GetEnabledBindings();

    // New child frames should inherit the nav_entry_id of their parent.
    set_nav_entry_id(
        frame_tree_node_->parent()->current_frame_host()->nav_entry_id());
  }

  SetUpMojoIfNeeded();

  swapout_event_monitor_timeout_.reset(new TimeoutMonitor(base::Bind(
      &RenderFrameHostImpl::OnSwappedOut, weak_ptr_factory_.GetWeakPtr())));
  beforeunload_timeout_.reset(
      new TimeoutMonitor(base::Bind(&RenderFrameHostImpl::BeforeUnloadTimeout,
                                    weak_ptr_factory_.GetWeakPtr())));

  if (widget_routing_id != MSG_ROUTING_NONE) {
    mojom::WidgetPtr widget;
    GetRemoteInterfaces()->GetInterface(&widget);

    if (!parent_) {
      // For main frames, the RenderWidgetHost is owned by the RenderViewHost.
      // TODO(https://crbug.com/545684): Once RenderViewHostImpl has-a
      // RenderWidgetHostImpl, the main render frame should probably start
      // owning the RenderWidgetHostImpl itself.
      DCHECK(GetLocalRenderWidgetHost());
      DCHECK_EQ(GetLocalRenderWidgetHost()->GetRoutingID(), widget_routing_id);
      DCHECK(!GetLocalRenderWidgetHost()->owned_by_render_frame_host());

      // Make the RenderWidgetHostImpl able to call the mojo Widget interface
      // (implemented by the RenderWidgetImpl).
      GetLocalRenderWidgetHost()->SetWidget(std::move(widget));
    } else {
      // For subframes, the RenderFrameHost directly creates and owns its
      // RenderWidgetHost.
      DCHECK_EQ(nullptr, GetLocalRenderWidgetHost());
      DCHECK_EQ(nullptr, RenderWidgetHostImpl::FromID(GetProcess()->GetID(),
                                                      widget_routing_id));
      owned_render_widget_host_ = RenderWidgetHostFactory::Create(
          frame_tree_->render_widget_delegate(), GetProcess(),
          widget_routing_id, std::move(widget), /*hidden=*/true);
      owned_render_widget_host_->set_owned_by_render_frame_host(true);
    }

    if (!frame_tree_node_->parent())
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
  FrameTreeNode* frame_owner = frame_tree_node_->parent()
                                   ? frame_tree_node_->parent()
                                   : frame_tree_node_->opener();
  if (frame_owner)
    CSPContext::SetSelf(frame_owner->current_origin());
}

RenderFrameHostImpl::~RenderFrameHostImpl() {
  // When a RenderFrameHostImpl is deleted, it may still contain children. This
  // can happen with the swap out timer. It causes a RenderFrameHost to delete
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
  ClearAllWebUI();

  SetLastCommittedSiteUrl(GURL());

  if (overlay_routing_token_)
    g_token_frame_map.Get().erase(*overlay_routing_token_);

  site_instance_->RemoveObserver(this);
  process_->RemoveObserver(this);

  const bool was_created = render_frame_created_;
  render_frame_created_ = false;
  if (delegate_ && was_created)
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
  // 2. The RenderFrame can be swapped out. In this case, the browser sends a
  //    UnfreezableFrameMsg_SwapOut for the RenderFrame to replace itself with a
  //    RenderFrameProxy and release its associated resources. |unload_state_|
  //    is advanced to UnloadState::InProgress to track that this IPC is in
  //    flight.
  // 3. The RenderFrame can be detached, as part of removing a subtree (due to
  //    navigation, swap out, or DOM mutation). In this case, the browser sends
  //    a UnfreezableFrameMsg_Delete for the RenderFrame to detach itself and
  //    release its associated resources. If the subframe contains an unload
  //    handler, |unload_state_| is advanced to UnloadState::InProgress to track
  //    that the detach is in progress; otherwise, it is advanced directly to
  //    UnloadState::Completed.
  //
  // The browser side gives the renderer a small timeout to finish processing
  // swap out / detach messages. When the timeout expires, the RFH will be
  // removed regardless of whether or not the renderer acknowledged that it
  // completed the work, to avoid indefinitely leaking browser-side state. To
  // avoid leaks, ~RenderFrameHostImpl still validates that the appropriate
  // cleanup IPC was sent to the renderer, by checking that |unload_state_| !=
  // UnloadState::NotRun.
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
      RenderFrameHostID(GetProcess()->GetID(), routing_id_));

  // Null out the swapout timer; in crash dumps this member will be null only if
  // the dtor has run.  (It may also be null in tests.)
  swapout_event_monitor_timeout_.reset();

  for (auto& iter : visual_state_callbacks_)
    std::move(iter.second).Run(false);

  // Note: The RenderWidgetHost of the main frame is owned by the RenderViewHost
  // instead. In this case the RenderViewHost is responsible for shutting down
  // its RenderViewHost.
  if (owned_render_widget_host_)
    owned_render_widget_host_->ShutdownAndDestroyWidget(false);

  // TODO(https://crbug.com/1005077): There is no known reason for removing the
  // RenderViewHostImpl here instead of automatically at the end of the
  // destructor. In practise, not doing it here will prevent android WebView to
  // display a new page after a long sequence of WebView creation / deletion.
  // The real reason why this is needed needs to be investigated.
  render_view_host_.reset();

  // If another frame is waiting for a beforeunload ACK from this frame,
  // simulate it now.
  RenderFrameHostImpl* beforeunload_initiator = GetBeforeUnloadInitiator();
  if (beforeunload_initiator && beforeunload_initiator != this) {
    base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
    beforeunload_initiator->ProcessBeforeUnloadACKFromFrame(
        true /* proceed */, false /* treat_as_final_ack */, this,
        true /* is_frame_being_destroyed */, approx_renderer_start_time,
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

const base::UnguessableToken& RenderFrameHostImpl::GetOverlayRoutingToken() {
  if (!overlay_routing_token_) {
    overlay_routing_token_ = base::UnguessableToken::Create();
    g_token_frame_map.Get().emplace(*overlay_routing_token_, this);
  }

  return *overlay_routing_token_;
}

void RenderFrameHostImpl::AudioContextPlaybackStarted(int audio_context_id) {
  delegate_->AudioContextPlaybackStarted(this, audio_context_id);
}

void RenderFrameHostImpl::AudioContextPlaybackStopped(int audio_context_id) {
  delegate_->AudioContextPlaybackStopped(this, audio_context_id);
}

// The current frame went into the BackForwardCache.
void RenderFrameHostImpl::EnterBackForwardCache() {
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(!is_in_back_forward_cache_);
  is_in_back_forward_cache_ = true;
  for (auto& child : children_)
    child->current_frame_host()->EnterBackForwardCache();
}

// The frame as been restored from the BackForwardCache.
void RenderFrameHostImpl::LeaveBackForwardCache() {
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK(is_in_back_forward_cache_);
  is_in_back_forward_cache_ = false;
  for (auto& child : children_)
    child->current_frame_host()->LeaveBackForwardCache();
}

void RenderFrameHostImpl::OnGrantedMediaStreamAccess() {
  was_granted_media_access_ = true;
}

void RenderFrameHostImpl::OnPortalActivated(
    const base::UnguessableToken& portal_token,
    mojo::PendingAssociatedRemote<blink::mojom::Portal> portal,
    mojo::PendingAssociatedReceiver<blink::mojom::PortalClient> portal_client,
    blink::TransferableMessage data,
    base::OnceCallback<void(bool)> callback) {
  GetNavigationControl()->OnPortalActivated(
      portal_token, std::move(portal), std::move(portal_client),
      std::move(data), std::move(callback));
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
    const blink::WebMediaPlayerAction& action) {
  gfx::PointF point_in_view = GetView()->TransformRootPointToViewCoordSpace(
      gfx::PointF(location.x(), location.y()));
  Send(new FrameMsg_MediaPlayerActionAt(routing_id_, point_in_view, action));
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactory(
    network::mojom::URLLoaderFactoryRequest default_factory_request) {
  return CreateNetworkServiceDefaultFactoryInternal(
      last_committed_origin_, network_isolation_key_,
      std::move(default_factory_request));
}

void RenderFrameHostImpl::MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
    base::flat_set<url::Origin> request_initiators,
    bool push_to_renderer_now) {
  size_t old_size = initiators_requiring_separate_url_loader_factory_.size();
  initiators_requiring_separate_url_loader_factory_.insert(
      request_initiators.begin(), request_initiators.end());
  size_t new_size = initiators_requiring_separate_url_loader_factory_.size();
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
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories =
            std::make_unique<blink::URLLoaderFactoryBundleInfo>();
    subresource_loader_factories->pending_initiator_specific_factories() =
        CreateInitiatorSpecificURLLoaderFactories(request_initiators);
    GetNavigationControl()->UpdateSubresourceLoaderFactories(
        std::move(subresource_loader_factories));
  }
}

bool RenderFrameHostImpl::IsSandboxed(blink::WebSandboxFlags flags) {
  if (base::FeatureList::IsEnabled(features::kFeaturePolicyForSandbox)) {
    blink::mojom::FeaturePolicyFeature feature =
        blink::FeaturePolicy::FeatureForSandboxFlag(flags);
    if (feature != blink::mojom::FeaturePolicyFeature::kNotFound)
      return !IsFeatureEnabled(feature);
  }
  return static_cast<int>(active_sandbox_flags_) & static_cast<int>(flags);
}

blink::URLLoaderFactoryBundleInfo::OriginMap
RenderFrameHostImpl::CreateInitiatorSpecificURLLoaderFactories(
    const base::flat_set<url::Origin>& initiator_origins) {
  blink::URLLoaderFactoryBundleInfo::OriginMap result;
  for (const url::Origin& initiator : initiator_origins) {
    network::mojom::URLLoaderFactoryPtrInfo factory_info;
    CreateNetworkServiceDefaultFactoryAndObserve(
        initiator, network_isolation_key_, mojo::MakeRequest(&factory_info));
    result[initiator] = std::move(factory_info);
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

  const bool has_user_gesture = true;
  GetNavigationControl()->JavaScriptExecuteRequestForTests(
      javascript, false, has_user_gesture, world_id, base::NullCallback());
}

void RenderFrameHostImpl::CopyImageAt(int x, int y) {
  gfx::PointF point_in_view =
      GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(x, y));
  Send(new FrameMsg_CopyImageAt(routing_id_, point_in_view.x(),
                                point_in_view.y()));
}

void RenderFrameHostImpl::SaveImageAt(int x, int y) {
  gfx::PointF point_in_view =
      GetView()->TransformRootPointToViewCoordSpace(gfx::PointF(x, y));
  Send(new FrameMsg_SaveImageAt(routing_id_, point_in_view.x(),
                                point_in_view.y()));
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
    blink::mojom::AssociatedInterfaceProviderAssociatedPtr remote_interfaces;
    IPC::ChannelProxy* channel = GetProcess()->GetChannel();
    if (channel) {
      RenderProcessHostImpl* process =
          static_cast<RenderProcessHostImpl*>(GetProcess());
      process->GetRemoteRouteProvider()->GetRoute(
          GetRoutingID(), mojo::MakeRequest(&remote_interfaces));
    } else {
      // The channel may not be initialized in some tests environments. In this
      // case we set up a dummy interface provider.
      mojo::MakeRequestAssociatedWithDedicatedPipe(&remote_interfaces);
    }
    remote_associated_interfaces_ =
        std::make_unique<blink::AssociatedInterfaceProvider>(
            std::move(remote_interfaces));
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
  DCHECK(IPC_MESSAGE_ID_CLASS(message->type()) != InputMsgStart);
  return GetProcess()->Send(message);
}

bool RenderFrameHostImpl::OnMessageReceived(const IPC::Message &msg) {
  // Only process messages if the RenderFrame is alive.
  if (!render_frame_created_)
    return false;

  // Crash reports triggered by IPC messages for this frame should be associated
  // with its URL.
  // TODO(lukasza): Also call SetActiveURL for mojo messages dispatched to
  // either the FrameHost interface or to interfaces bound by this frame.
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
    IPC_MESSAGE_HANDLER(FrameHostMsg_BeforeUnload_ACK, OnBeforeUnloadACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SwapOut_ACK, OnSwapOutACK)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ContextMenu, OnContextMenu)
    IPC_MESSAGE_HANDLER(FrameHostMsg_VisualStateResponse,
                        OnVisualStateResponse)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_RunJavaScriptDialog,
                                    OnRunJavaScriptDialog)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_RunBeforeUnloadConfirm,
                                    OnRunBeforeUnloadConfirm)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidAccessInitialDocument,
                        OnDidAccessInitialDocument)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeOpener, OnDidChangeOpener)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidAddContentSecurityPolicies,
                        OnDidAddContentSecurityPolicies)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeFramePolicy,
                        OnDidChangeFramePolicy)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeFrameOwnerProperties,
                        OnDidChangeFrameOwnerProperties)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateTitle, OnUpdateTitle)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidBlockNavigation, OnDidBlockNavigation)
    IPC_MESSAGE_HANDLER(FrameHostMsg_AbortNavigation, OnAbortNavigation)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DispatchLoad, OnDispatchLoad)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ForwardResourceTimingToParent,
                        OnForwardResourceTimingToParent)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_EventBundle, OnAccessibilityEvents)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_LocationChanges,
                        OnAccessibilityLocationChanges)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_FindInPageResult,
                        OnAccessibilityFindInPageResult)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_FindInPageTermination,
                        OnAccessibilityFindInPageTermination)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_ChildFrameHitTestResult,
                        OnAccessibilityChildFrameHitTestResult)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_SnapshotResponse,
                        OnAccessibilitySnapshotResponse)
    IPC_MESSAGE_HANDLER(FrameHostMsg_EnterFullscreen, OnEnterFullscreen)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ExitFullscreen, OnExitFullscreen)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SuddenTerminationDisablerChanged,
                        OnSuddenTerminationDisablerChanged)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidChangeLoadProgress,
                        OnDidChangeLoadProgress)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SelectionChanged, OnSelectionChanged)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FocusedNodeChanged, OnFocusedNodeChanged)
    IPC_MESSAGE_HANDLER(FrameHostMsg_UpdateUserActivationState,
                        OnUpdateUserActivationState)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SetHasReceivedUserGestureBeforeNavigation,
                        OnSetHasReceivedUserGestureBeforeNavigation)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SetNeedsOcclusionTracking,
                        OnSetNeedsOcclusionTracking);
    IPC_MESSAGE_HANDLER(FrameHostMsg_ScrollRectToVisibleInParentFrame,
                        OnScrollRectToVisibleInParentFrame)
    IPC_MESSAGE_HANDLER(FrameHostMsg_BubbleLogicalScrollInParentFrame,
                        OnBubbleLogicalScrollInParentFrame)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FrameDidCallFocus, OnFrameDidCallFocus)
    IPC_MESSAGE_HANDLER(FrameHostMsg_RenderFallbackContentInParentProcess,
                        OnRenderFallbackContentInParentProcess)
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ShowPopup, OnShowPopup)
    IPC_MESSAGE_HANDLER(FrameHostMsg_HidePopup, OnHidePopup)
#endif
    IPC_MESSAGE_HANDLER(FrameHostMsg_RequestOverlayRoutingToken,
                        OnRequestOverlayRoutingToken)
  IPC_END_MESSAGE_MAP()

  // No further actions here, since we may have been deleted.
  return handled;
}

void RenderFrameHostImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  ContentBrowserClient* browser_client = GetContentClient()->browser();
  if (!associated_registry_->TryBindInterface(interface_name, &handle) &&
      !browser_client->BindAssociatedInterfaceRequestFromFrame(
          this, interface_name, &handle)) {
    delegate_->OnAssociatedInterfaceRequest(this, interface_name,
                                            std::move(handle));
  }
}

void RenderFrameHostImpl::AccessibilityPerformAction(
    const ui::AXActionData& action_data) {
  Send(new AccessibilityMsg_PerformAction(routing_id_, action_data));
}

bool RenderFrameHostImpl::AccessibilityViewHasFocus() const {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return view->HasFocus();
  return false;
}

gfx::Rect RenderFrameHostImpl::AccessibilityGetViewBounds() const {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return view->GetViewBounds();
  return gfx::Rect();
}

float RenderFrameHostImpl::AccessibilityGetDeviceScaleFactor() const {
  RenderWidgetHostView* view = render_view_host_->GetWidget()->GetView();
  if (view)
    return GetScaleFactorForView(view);
  return 1.0f;
}

void RenderFrameHostImpl::AccessibilityReset() {
  accessibility_reset_token_ = g_next_accessibility_reset_token++;
  Send(new AccessibilityMsg_Reset(routing_id_, accessibility_reset_token_));
}

void RenderFrameHostImpl::AccessibilityFatalError() {
  browser_accessibility_manager_.reset(nullptr);
  if (accessibility_reset_token_)
    return;

  accessibility_reset_count_++;
  if (accessibility_reset_count_ >= kMaxAccessibilityResets) {
    Send(new AccessibilityMsg_FatalError(routing_id_));
  } else {
    accessibility_reset_token_ = g_next_accessibility_reset_token++;
    Send(new AccessibilityMsg_Reset(routing_id_, accessibility_reset_token_));
  }
}

gfx::AcceleratedWidget
    RenderFrameHostImpl::AccessibilityGetAcceleratedWidget() {
  // Only the main frame's current frame host is connected to the native
  // widget tree for accessibility, so return null if this is queried on
  // any other frame.
  if (frame_tree_node()->parent() || !IsCurrent())
    return gfx::kNullAcceleratedWidget;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetAcceleratedWidget();
  return gfx::kNullAcceleratedWidget;
}

gfx::NativeViewAccessible
    RenderFrameHostImpl::AccessibilityGetNativeViewAccessible() {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetNativeViewAccessible();
  return nullptr;
}

gfx::NativeViewAccessible
RenderFrameHostImpl::AccessibilityGetNativeViewAccessibleForWindow() {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view)
    return view->AccessibilityGetNativeViewAccessibleForWindow();
  return nullptr;
}

WebContents* RenderFrameHostImpl::AccessibilityWebContents() {
  return delegate()->GetAsWebContents();
}

bool RenderFrameHostImpl::AccessibilityIsMainFrame() const {
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
  document_scoped_interface_provider_binding_.Close();
  document_interface_broker_content_receiver_.reset();
  document_interface_broker_blink_receiver_.reset();
  broker_receiver_.reset();
  SetLastCommittedUrl(GURL());
  bundled_exchanges_handle_.reset();

  // Execute any pending AX tree snapshot callbacks with an empty response,
  // since we're never going to get a response from this renderer.
  for (auto& iter : ax_tree_snapshot_callbacks_)
    std::move(iter.second).Run(ui::AXTreeUpdate());

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

  ax_tree_snapshot_callbacks_.clear();
  visual_state_callbacks_.clear();

  // Ensure that future remote interface requests are associated with the new
  // process's channel.
  remote_associated_interfaces_.reset();

  // Ensure that the AssociatedRemote<blink::mojom::Frame> works after a crash.
  frame_remote_.reset();

  // Any termination disablers in content loaded by the new process will
  // be sent again.
  sudden_termination_disabler_types_enabled_ = 0;

  if (unload_state_ != UnloadState::NotRun) {
    // If the process has died, we don't need to wait for the ACK. Complete the
    // deletion immediately.
    unload_state_ = UnloadState::Completed;
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

void RenderFrameHostImpl::RenderProcessGone(SiteInstanceImpl* site_instance) {
  DCHECK_EQ(site_instance_.get(), site_instance);

  if (is_in_back_forward_cache_) {
    // Note: Evicting from the BackForwardCache deletes |this|.
    EvictFromBackForwardCache();
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
    const CSPViolationParams& violation_params) {
  Send(new FrameMsg_ReportContentSecurityPolicyViolation(routing_id_,
                                                         violation_params));
}

void RenderFrameHostImpl::SanitizeDataForUseInCspViolation(
    bool is_redirect,
    CSPDirective::Name directive,
    GURL* blocked_url,
    SourceLocation* source_location) const {
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
  if (!is_redirect && directive == CSPDirective::FormAction)
    sanitize_blocked_url = false;

  if (sanitize_blocked_url)
    *blocked_url = blocked_url->GetOrigin();
  if (sanitize_source_location) {
    *source_location =
        SourceLocation(source_location_url.GetOrigin().spec(), 0u, 0u);
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

  service_manager::mojom::InterfaceProviderPtr interface_provider;
  BindInterfaceProviderRequest(mojo::MakeRequest(&interface_provider));

  mojo::PendingRemote<blink::mojom::DocumentInterfaceBroker>
      document_interface_broker_content;
  mojo::PendingRemote<blink::mojom::DocumentInterfaceBroker>
      document_interface_broker_blink;
  BindDocumentInterfaceBrokerReceiver(
      document_interface_broker_content.InitWithNewPipeAndPassReceiver(),
      document_interface_broker_blink.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker;
  BindBrowserInterfaceBrokerReceiver(
      browser_interface_broker.InitWithNewPipeAndPassReceiver());

  mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
  params->interface_bundle = mojom::DocumentScopedInterfaceBundle::New(
      interface_provider.PassInterface(),
      std::move(document_interface_broker_content),
      std::move(document_interface_broker_blink),
      std::move(browser_interface_broker));

  params->routing_id = routing_id_;
  params->previous_routing_id = previous_routing_id;
  params->opener_routing_id = opener_routing_id;
  params->parent_routing_id = parent_routing_id;
  params->previous_sibling_routing_id = previous_sibling_routing_id;
  params->replication_state = frame_tree_node()->current_replication_state();
  params->devtools_frame_token = frame_tree_node()->devtools_frame_token();

  // Normally, the replication state contains effective frame policy, excluding
  // sandbox flags and feature policy attributes that were updated but have not
  // taken effect. However, a new RenderFrame should use the pending frame
  // policy, since it is being created as part of the navigation that will
  // commit it. (I.e., the RenderFrame needs to know the policy to use when
  // initializing the new document once it commits).
  params->replication_state.frame_policy =
      frame_tree_node()->pending_frame_policy();

  params->frame_owner_properties =
      FrameOwnerProperties(frame_tree_node()->frame_owner_properties());

  params->has_committed_real_load =
      frame_tree_node()->has_committed_real_load();

  params->widget_params = mojom::CreateFrameWidgetParams::New();
  if (GetLocalRenderWidgetHost()) {
    params->widget_params->routing_id =
        GetLocalRenderWidgetHost()->GetRoutingID();
  } else {
    // MSG_ROUTING_NONE will prevent a new RenderWidget from being created in
    // the renderer process.
    params->widget_params->routing_id = MSG_ROUTING_NONE;
  }

  GetProcess()->GetRendererInterface()->CreateFrame(std::move(params));

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

  if (previous_routing_id != MSG_ROUTING_NONE) {
    RenderFrameProxyHost* proxy = RenderFrameProxyHost::FromID(
        GetProcess()->GetID(), previous_routing_id);
    // We have also created a RenderFrameProxy in CreateFrame above, so
    // remember that.
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
  if (!is_active())
    return;

  if (render_frame_created_) {
    Send(new UnfreezableFrameMsg_Delete(routing_id_, intent));

    if (!frame_tree_node_->IsMainFrame() && IsCurrent()) {
      // If this subframe has an unload handler (and isn't speculative), ensure
      // that it has a chance to execute by delaying process cleanup. This will
      // prevent the process from shutting down immediately in the case where
      // this is the last active frame in the process. See
      // https://crbug.com/852204.
      if (GetSuddenTerminationDisablerState(blink::kUnloadHandler)) {
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

  unload_state_ = GetSuddenTerminationDisablerState(blink::kUnloadHandler)
                      ? UnloadState::InProgress
                      : UnloadState::Completed;
}

void RenderFrameHostImpl::SetRenderFrameCreated(bool created) {
  // We should not create new RenderFrames while our delegate is being destroyed
  // (e.g., via a WebContentsObserver during WebContents shutdown).  This seems
  // to have caused crashes in https://crbug.com/717650.
  if (created && delegate_)
    CHECK(!delegate_->IsBeingDestroyed());

  bool was_created = render_frame_created_;
  render_frame_created_ = created;

  // If the current status is different than the new status, the delegate
  // needs to be notified.
  if (delegate_ && (created != was_created)) {
    if (created) {
      SetUpMojoIfNeeded();
      delegate_->RenderFrameCreated(this);
    } else {
      delegate_->RenderFrameDeleted(this);
    }
  }

  if (created && GetLocalRenderWidgetHost()) {
    mojom::WidgetPtr widget;
    GetRemoteInterfaces()->GetInterface(&widget);
    GetLocalRenderWidgetHost()->SetWidget(std::move(widget));
    GetLocalRenderWidgetHost()->SetFrameInputHandler(
        frame_input_handler_.get());
    GetLocalRenderWidgetHost()->input_router()->SetFrameTreeNodeId(
        frame_tree_node_->frame_tree_node_id());
    viz::mojom::InputTargetClientPtr input_target_client;
    remote_interfaces_->GetInterface(&input_target_client);
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
        EnsurePrefetchedSignedExchangeCache());
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
    service_manager::mojom::InterfaceProviderRequest
        new_interface_provider_provider_request,
    mojo::PendingReceiver<blink::mojom::DocumentInterfaceBroker>
        document_interface_broker_content_receiver,
    mojo::PendingReceiver<blink::mojom::DocumentInterfaceBroker>
        document_interface_broker_blink_receiver,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker_receiver,
    blink::WebTreeScopeType scope,
    const std::string& frame_name,
    const std::string& frame_unique_name,
    bool is_created_by_script,
    const base::UnguessableToken& devtools_frame_token,
    const blink::FramePolicy& frame_policy,
    const FrameOwnerProperties& frame_owner_properties,
    const blink::FrameOwnerElementType owner_type) {
  // TODO(lukasza): Call ReceivedBadMessage when |frame_unique_name| is empty.
  DCHECK(!frame_unique_name.empty());
  DCHECK(new_interface_provider_provider_request.is_pending());
  DCHECK(document_interface_broker_content_receiver.is_valid());
  DCHECK(document_interface_broker_blink_receiver.is_valid());
  DCHECK(browser_interface_broker_receiver.is_valid());
  if (owner_type == blink::FrameOwnerElementType::kNone) {
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

  // |new_routing_id|, |new_interface_provider_provider_request|,
  // |document_interface_broker_content_receiver|,
  // |document_interface_broker_blink_receiver|,
  // |browser_interface_broker_receiver| and |devtools_frame_token| were
  // generated on the browser's IO thread and not taken from the renderer
  // process.
  frame_tree_->AddFrame(frame_tree_node_, GetProcess()->GetID(), new_routing_id,
                        std::move(new_interface_provider_provider_request),
                        std::move(document_interface_broker_content_receiver),
                        std::move(document_interface_broker_blink_receiver),
                        std::move(browser_interface_broker_receiver), scope,
                        frame_name, frame_unique_name, is_created_by_script,
                        devtools_frame_token, frame_policy,
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
  }
}

void RenderFrameHostImpl::SetLastCommittedOrigin(const url::Origin& origin) {
  last_committed_origin_ = origin;
  CSPContext::SetSelf(origin);
}

void RenderFrameHostImpl::SetLastCommittedOriginForTesting(
    const url::Origin& origin) {
  SetLastCommittedOrigin(origin);
}

GURL RenderFrameHostImpl::ComputeSiteForCookiesForNavigation(
    const GURL& destination) const {
  // For top-level navigation, |site_for_cookies| will always be the destination
  // URL.
  if (frame_tree_node_->IsMainFrame())
    return destination;

  GURL base_url;
#if defined(OS_ANDROID)
  // On Android, a base URL can be set for the frame. If this the case, it is
  // the URL to use for cookies.
  NavigationEntry* last_committed_entry =
      frame_tree_node_->navigator()->GetController()->GetLastCommittedEntry();
  if (last_committed_entry)
    base_url = last_committed_entry->GetBaseURLForDataURL();
#endif
  // This is pre-navigation, but since at this point the frame being navigated
  // is known to not be the main frame, it's correct post-navigation as well.
  const GURL& top_document_url =
      !base_url.is_empty() ? base_url : frame_tree_->root()->current_url();

  if (GetContentClient()
          ->browser()
          ->ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
              top_document_url.scheme_piece())) {
    return top_document_url;
  }

  // Check if everything above the frame being navigated is consistent. It's OK
  // to skip checking the frame itself since it will be validated against
  // |site_for_cookies| anyway.
  const FrameTreeNode* current = frame_tree_node_->parent();
  bool ancestors_are_same_site = true;
  while (current && ancestors_are_same_site) {
    // Skip over srcdoc documents, as they are always same-origin with their
    // closest non-srcdoc parent.
    while (current->current_url().IsAboutSrcdoc())
      current = current->parent();

    if (!net::registry_controlled_domains::SameDomainOrHost(
            top_document_url, current->current_url(),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      ancestors_are_same_site = false;
    }
    current = current->parent();
  }

  return (ancestors_are_same_site || !base_url.is_empty()) ? top_document_url
                                                           : GURL::EmptyGURL();
}

void RenderFrameHostImpl::SetOriginOfNewFrame(
    const url::Origin& new_frame_creator) {
  // This method should only be called for *new* frames, that haven't committed
  // a navigation yet.
  DCHECK(!has_committed_any_navigation_);
  DCHECK(GetLastCommittedOrigin().opaque());

  // Calculate and set |new_frame_origin|.
  bool new_frame_should_be_sandboxed =
      blink::WebSandboxFlags::kOrigin ==
      (frame_tree_node()->active_sandbox_flags() &
       blink::WebSandboxFlags::kOrigin);
  url::Origin new_frame_origin = new_frame_should_be_sandboxed
                                     ? new_frame_creator.DeriveNewOpaqueOrigin()
                                     : new_frame_creator;
  SetLastCommittedOrigin(new_frame_origin);
}

FrameTreeNode* RenderFrameHostImpl::AddChild(
    std::unique_ptr<FrameTreeNode> child,
    int process_id,
    int frame_routing_id) {
  // Child frame must always be created in the same process as the parent.
  CHECK_EQ(process_id, GetProcess()->GetID());

  // Initialize the RenderFrameHost for the new node.  We always create child
  // frames in the same SiteInstance as the current frame, and they can swap to
  // a different one if they navigate away.
  child->render_manager()->Init(GetSiteInstance(),
                                render_view_host()->GetRoutingID(),
                                frame_routing_id, MSG_ROUTING_NONE, false);

  // Other renderer processes in this BrowsingInstance may need to find out
  // about the new frame.  Create a proxy for the child frame in all
  // SiteInstances that have a proxy for the frame's parent, since all frames
  // in a frame tree should have the same set of proxies.
  frame_tree_node_->render_manager()->CreateProxiesForChildFrame(child.get());

  // When the child is added, it hasn't committed any navigation yet - its
  // initial empty document should inherit the origin of its parent (the origin
  // may change after the first commit).  See also https://crbug.com/932067.
  child->current_frame_host()->SetOriginOfNewFrame(GetLastCommittedOrigin());

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

void RenderFrameHostImpl::OnDetach() {
  if (!parent_) {
    bad_message::ReceivedBadMessage(GetProcess(),
                                    bad_message::RFH_DETACH_MAIN_FRAME);
    return;
  }

  // A frame is removed while replacing this document with the new one. When it
  // happens, delete the frame and both the new and old documents. Unload
  // handlers aren't guaranteed to run here.
  if (is_waiting_for_swapout_ack_) {
    parent_->RemoveChild(frame_tree_node_);
    return;
  }

  if (unload_state_ != UnloadState::NotRun) {
    // The frame is pending deletion. FrameHostMsg_Detach is used to confirm
    // its unload handlers ran.
    unload_state_ = UnloadState::Completed;
    PendingDeletionCheckCompleted();  // Can delete |this|.
    return;
  }

  // This frame is being removed by the renderer, and it has already executed
  // its unload handler.
  unload_state_ = UnloadState::Completed;
  // Before completing the removal, we still need to wait for all of its
  // descendant frames to execute unload handlers. Start executing those
  // handlers now.
  StartPendingDeletionOnSubtree();
  frame_tree_node_->frame_tree()->FrameUnloading(frame_tree_node_);

  // Some children with no unload handler may be eligible for immediate
  // deletion. Cut the dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();  // Can delete |this|.
}

void RenderFrameHostImpl::FrameFocused() {
  if (!is_active())
    return;

  delegate_->SetFocusedFrame(frame_tree_node_, GetSiteInstance());
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
      this, validated_url, params.initiator_origin, params.uses_post,
      params.resource_request_body, params.extra_headers, params.referrer,
      params.disposition, params.should_replace_current_entry,
      params.user_gesture, params.triggering_event_info, params.href_translate,
      std::move(blob_url_loader_factory));
}

void RenderFrameHostImpl::CancelInitialHistoryLoad() {
  // A Javascript navigation interrupted the initial history load.  Check if an
  // initial subframe cross-process navigation needs to be canceled as a result.
  // TODO(creis, clamy): Cancel any cross-process navigation.
  NOTIMPLEMENTED();
}

void RenderFrameHostImpl::DocumentOnLoadCompleted() {
  // This message is only sent for top-level frames. TODO(avi): when frame tree
  // mirroring works correctly, add a check here to enforce it.
  delegate_->DocumentOnLoadCompleted(this);
}

void RenderFrameHostImpl::UpdateActiveSchedulerTrackedFeatures(
    uint64_t features_mask) {
  TRACE_EVENT0("toplevel", "UpdateActiveSchedulerTrackedFeatures");
  renderer_reported_scheduler_tracked_features_ = features_mask;
}

void RenderFrameHostImpl::OnSchedulerTrackedFeatureUsed(
    blink::scheduler::WebSchedulerTrackedFeature feature) {
  TRACE_EVENT0("toplevel", "OnSchedulerTrackedFeatureUsed");
  browser_reported_scheduler_tracked_features_ |=
      1 << static_cast<uint64_t>(feature);
}

bool RenderFrameHostImpl::IsFrozen() {
  return frame_lifecycle_state_ != blink::mojom::FrameLifecycleState::kRunning;
}

void RenderFrameHostImpl::DidFailProvisionalLoadWithError(
    const GURL& url,
    int error_code,
    const base::string16& error_description,
    bool showing_repost_interstitial) {
  TRACE_EVENT2("navigation",
               "RenderFrameHostImpl::DidFailProvisionalLoadWithError",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "error", error_code);
  // TODO(clamy): Kill the renderer with RFH_FAIL_PROVISIONAL_LOAD_NO_HANDLE and
  // return early if navigation_handle_ is null, once we prevent that case from
  // happening in practice. See https://crbug.com/605289.

  // Update the error code in the NavigationRequest.
  if (navigation_request()) {
    navigation_request()->set_net_error(static_cast<net::Error>(error_code));
  }

  frame_tree_node_->navigator()->DidFailProvisionalLoadWithError(
      this, url, error_code, error_description, showing_repost_interstitial);
}

void RenderFrameHostImpl::DidFailLoadWithError(
    const GURL& url,
    int error_code,
    const base::string16& error_description) {
  TRACE_EVENT2("navigation",
               "RenderFrameHostImpl::DidFailProvisionalLoadWithError",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "error", error_code);

  GURL validated_url(url);
  GetProcess()->FilterURL(false, &validated_url);

  frame_tree_node_->navigator()->DidFailLoadWithError(
      this, validated_url, error_code, error_description);
}

void RenderFrameHostImpl::DidCommitProvisionalLoad(
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
        validated_params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  if (MaybeInterceptCommitCallback(nullptr, validated_params.get(),
                                   &interface_params)) {
    DidCommitNavigation(std::move(navigation_request_),
                        std::move(validated_params),
                        std::move(interface_params));
  }
}

void RenderFrameHostImpl::DidCommitBackForwardCacheNavigation(
    NavigationRequest* committing_navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
        validated_params) {
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

  DidCommitNavigationInternal(std::move(owned_request), validated_params.get(),
                              /*is_same_document_navigation=*/false);

  // Now that the restored frame has been committed, unfreeze it.
  frame_tree_node()->render_manager()->UnfreezeCurrentFrameHost();

  // The page is already loaded since it came from the cache, so fire the stop
  // loading event.
  OnDidStopLoading();
}

void RenderFrameHostImpl::DidCommitPerNavigationMojoInterfaceNavigation(
    NavigationRequest* committing_navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
        validated_params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  DCHECK(committing_navigation_request);
  committing_navigation_request->IgnoreCommitInterfaceDisconnection();
  if (!MaybeInterceptCommitCallback(committing_navigation_request,
                                    validated_params.get(),
                                    &interface_params)) {
    return;
  }

  auto request = navigation_requests_.find(committing_navigation_request);

  // The committing request should be in the map of NavigationRequests for
  // this RenderFrameHost.
  CHECK(request != navigation_requests_.end());

  std::unique_ptr<NavigationRequest> owned_request = std::move(request->second);
  navigation_requests_.erase(committing_navigation_request);
  DidCommitNavigation(std::move(owned_request), std::move(validated_params),
                      std::move(interface_params));
}

void RenderFrameHostImpl::DidCommitSameDocumentNavigation(
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
        validated_params) {
  ScopedActiveURL scoped_active_url(
      validated_params->url,
      frame_tree_node()->frame_tree()->root()->current_origin());
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
               validated_params->url.possibly_invalid_spec());

  // Check if the navigation matches a stored same-document NavigationRequest.
  // In that case it is browser-initiated.
  bool is_browser_initiated =
      same_document_navigation_request_ &&
      (same_document_navigation_request_->commit_params().navigation_token ==
       validated_params->navigation_token);
  if (!DidCommitNavigationInternal(
          is_browser_initiated ? std::move(same_document_navigation_request_)
                               : nullptr,
          validated_params.get(), true /* is_same_document_navigation*/)) {
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

NavigationHandleImpl* RenderFrameHostImpl::GetNavigationHandle() {
  return navigation_request() ? navigation_request()->navigation_handle()
                              : nullptr;
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

void RenderFrameHostImpl::SwapOut(
    RenderFrameProxyHost* proxy,
    bool is_loading) {
  // The end of this event is in OnSwapOutACK when the RenderFrame has completed
  // the operation and sends back an IPC message.
  // The trace event may not end properly if the ACK times out.  We expect this
  // to be fixed when RenderViewHostImpl::OnSwapOut moves to RenderFrameHost.
  TRACE_EVENT_ASYNC_BEGIN1("navigation", "RenderFrameHostImpl::SwapOut", this,
                           "frame_tree_node",
                           frame_tree_node_->frame_tree_node_id());

  // If this RenderFrameHost is already pending deletion, it must have already
  // gone through this, therefore just return.
  if (unload_state_ != UnloadState::NotRun) {
    NOTREACHED() << "RFH should be in default state when calling SwapOut.";
    return;
  }

  if (swapout_event_monitor_timeout_) {
    swapout_event_monitor_timeout_->Start(base::TimeDelta::FromMilliseconds(
        RenderViewHostImpl::kUnloadTimeoutMS));
  }

  // There should always be a proxy to replace the old RenderFrameHost.  If
  // there are no remaining active views in the process, the proxy will be
  // short-lived and will be deleted when the SwapOut ACK is received.
  CHECK(proxy);

  // TODO(nasko): If the frame is not live, the RFH should just be deleted by
  // simulating the receipt of swap out ack.
  is_waiting_for_swapout_ack_ = true;
  unload_state_ = UnloadState::InProgress;

  if (IsRenderFrameLive()) {
    FrameReplicationState replication_state =
        proxy->frame_tree_node()->current_replication_state();
    Send(new UnfreezableFrameMsg_SwapOut(routing_id_, proxy->GetRoutingID(),
                                         is_loading, replication_state));
    // Remember that a RenderFrameProxy was created as part of processing the
    // SwapOut message above.
    proxy->SetRenderFrameProxyCreated(true);

    StartPendingDeletionOnSubtree();
  }

  // Some children with no unload handler may be eligible for deletion. Cut the
  // dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();

  if (web_ui())
    web_ui()->RenderFrameHostSwappingOut();

  web_bluetooth_services_.clear();
#if !defined(OS_ANDROID)
  serial_service_.reset();
#endif
}

void RenderFrameHostImpl::DetachFromProxy() {
  if (unload_state_ != UnloadState::NotRun)
    return;

  // Start pending deletion on this frame and its children.
  DeleteRenderFrame(FrameDeleteIntention::kNotMainFrame);
  StartPendingDeletionOnSubtree();
  frame_tree_node_->frame_tree()->FrameUnloading(frame_tree_node_);

  // Some children with no unload handler may be eligible for immediate
  // deletion. Cut the dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();  // May delete |this|.
}

void RenderFrameHostImpl::OnBeforeUnloadACK(
    bool proceed,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  ProcessBeforeUnloadACK(proceed, false /* treat_as_final_ack */,
                         renderer_before_unload_start_time,
                         renderer_before_unload_end_time);
}

void RenderFrameHostImpl::ProcessBeforeUnloadACK(
    bool proceed,
    bool treat_as_final_ack,
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
  initiator->ProcessBeforeUnloadACKFromFrame(
      proceed, treat_as_final_ack, this, false /* is_frame_being_destroyed */,
      renderer_before_unload_start_time, renderer_before_unload_end_time);
}

RenderFrameHostImpl* RenderFrameHostImpl::GetBeforeUnloadInitiator() {
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->is_waiting_for_beforeunload_ack_)
      return frame;
  }
  return nullptr;
}

void RenderFrameHostImpl::ProcessBeforeUnloadACKFromFrame(
    bool proceed,
    bool treat_as_final_ack,
    RenderFrameHostImpl* frame,
    bool is_frame_being_destroyed,
    const base::TimeTicks& renderer_before_unload_start_time,
    const base::TimeTicks& renderer_before_unload_end_time) {
  // Check if we need to wait for more beforeunload ACKs.  If |proceed| is
  // false, we know the navigation or window close will be aborted, so we don't
  // need to wait for beforeunload ACKs from any other frames.
  // |treat_as_final_ack| also indicates that we shouldn't wait for any other
  // ACKs (e.g., when a beforeunload timeout fires).
  if (!proceed || treat_as_final_ack) {
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
    base::TimeTicks receive_before_unload_ack_time = base::TimeTicks::Now();

    if (!base::TimeTicks::IsConsistentAcrossProcesses()) {
      // TimeTicks is not consistent across processes and we are passing
      // TimeTicks across process boundaries so we need to compensate for any
      // skew between the processes. Here we are converting the renderer's
      // notion of before_unload_end_time to TimeTicks in the browser process.
      // See comments in inter_process_time_ticks_converter.h for more.
      InterProcessTimeTicksConverter converter(
          LocalTimeTicks::FromTimeTicks(send_before_unload_start_time_),
          LocalTimeTicks::FromTimeTicks(receive_before_unload_ack_time),
          RemoteTimeTicks::FromTimeTicks(renderer_before_unload_start_time),
          RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
      LocalTimeTicks browser_before_unload_end_time =
          converter.ToLocalTimeTicks(
              RemoteTimeTicks::FromTimeTicks(renderer_before_unload_end_time));
      before_unload_end_time = browser_before_unload_end_time.ToTimeTicks();
    }

    base::TimeDelta on_before_unload_overhead_time =
        (receive_before_unload_ack_time - send_before_unload_start_time_) -
        (renderer_before_unload_end_time - renderer_before_unload_start_time);
    UMA_HISTOGRAM_TIMES("Navigation.OnBeforeUnloadOverheadTime",
                        on_before_unload_overhead_time);

    frame_tree_node_->navigator()->LogBeforeUnloadTime(
        renderer_before_unload_start_time, renderer_before_unload_end_time);
  }

  // Resets beforeunload waiting state.
  is_waiting_for_beforeunload_ack_ = false;
  has_shown_beforeunload_dialog_ = false;
  if (beforeunload_timeout_)
    beforeunload_timeout_->Stop();
  send_before_unload_start_time_ = base::TimeTicks();

  // If the ACK is for a navigation, send it to the Navigator to have the
  // current navigation stop/proceed. Otherwise, send it to the
  // RenderFrameHostManager which handles closing.
  if (unload_ack_is_for_navigation_) {
    frame_tree_node_->navigator()->OnBeforeUnloadACK(frame_tree_node_, proceed,
                                                     before_unload_end_time);
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
          self->frame_tree_node()->render_manager()->OnBeforeUnloadACK(
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
  return render_view_host_->is_waiting_for_close_ack_ ||
         is_waiting_for_swapout_ack_;
}

void RenderFrameHostImpl::OnSwapOutACK() {
  if (frame_tree_node_->render_manager()->is_attaching_inner_delegate()) {
    // This RFH was swapped out while attaching an inner delegate. The RFH
    // will stay around but it will no longer be associated with a RenderFrame.
    SetRenderFrameCreated(false);
    return;
  }

  // Ignore spurious swap out ack.
  if (!is_waiting_for_swapout_ack_)
    return;

  DCHECK_EQ(UnloadState::InProgress, unload_state_);
  unload_state_ = UnloadState::Completed;
  PendingDeletionCheckCompleted();  // Can delete |this|.
}

void RenderFrameHostImpl::OnSwappedOut() {
  DCHECK(is_waiting_for_swapout_ack_);

  TRACE_EVENT_ASYNC_END0("navigation", "RenderFrameHostImpl::SwapOut", this);
  if (swapout_event_monitor_timeout_)
    swapout_event_monitor_timeout_->Stop();

  ClearAllWebUI();

  // If this is a main frame RFH that's about to be deleted, update its RVH's
  // swapped-out state here. https://crbug.com/505887.  This should only be
  // done if the RVH hasn't been already reused and marked as active by another
  // navigation. See https://crbug.com/823567.
  if (frame_tree_node_->IsMainFrame() && !render_view_host_->is_active())
    render_view_host_->set_is_swapped_out(true);

  bool deleted =
      frame_tree_node_->render_manager()->DeleteFromPendingList(this);
  CHECK(deleted);
}

void RenderFrameHostImpl::DisableSwapOutTimerForTesting() {
  swapout_event_monitor_timeout_.reset();
}

void RenderFrameHostImpl::SetSubframeUnloadTimeoutForTesting(
    const base::TimeDelta& timeout) {
  subframe_unload_timeout_ = timeout;
}

void RenderFrameHostImpl::OnContextMenu(const ContextMenuParams& params) {
  if (!is_active())
    return;

  // Validate the URLs in |params|.  If the renderer can't request the URLs
  // directly, don't show them in the context menu.
  ContextMenuParams validated_params(params);
  RenderProcessHost* process = GetProcess();

  // We don't validate |unfiltered_link_url| so that this field can be used
  // when users want to copy the original link URL.
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

void RenderFrameHostImpl::OnRunJavaScriptDialog(
    const base::string16& message,
    const base::string16& default_prompt,
    JavaScriptDialogType dialog_type,
    IPC::Message* reply_msg) {
  // Don't show the dialog if it's triggered on a frame that's pending deletion
  // (e.g., from an unload handler), or when the tab is being closed.
  if (IsWaitingForUnloadACK()) {
    SendJavaScriptDialogReply(reply_msg, true, base::string16());
    return;
  }

  // While a JS message dialog is showing, tabs in the same process shouldn't
  // process input events.
  GetProcess()->SetBlocked(true);

  delegate_->RunJavaScriptDialog(this, message, default_prompt, dialog_type,
                                 reply_msg);
}

void RenderFrameHostImpl::OnRunBeforeUnloadConfirm(
    bool is_reload,
    IPC::Message* reply_msg) {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OnRunBeforeUnloadConfirm",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  // Allow at most one attempt to show a beforeunload dialog per navigation.
  RenderFrameHostImpl* beforeunload_initiator = GetBeforeUnloadInitiator();
  if (beforeunload_initiator) {
    // If the running beforeunload handler wants to display a dialog and the
    // before-unload type wants to ignore it, then short-circuit the request and
    // respond as if the user decided to stay on the page, canceling the unload.
    if (beforeunload_initiator->beforeunload_dialog_request_cancels_unload_) {
      SendJavaScriptDialogReply(reply_msg, false /* success */,
                                base::string16());
      return;
    }

    if (beforeunload_initiator->has_shown_beforeunload_dialog_) {
      // TODO(alexmos): Pass enough data back to renderer to record histograms
      // for Document.BeforeUnloadDialog and add the intervention console
      // message to match renderer-side behavior in
      // Document::DispatchBeforeUnloadEvent().
      SendJavaScriptDialogReply(reply_msg, true /* success */,
                                base::string16());
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

  delegate_->RunBeforeUnloadConfirm(this, is_reload, reply_msg);
}

void RenderFrameHostImpl::RequestTextSurroundingSelection(
    TextSurroundingSelectionCallback callback,
    int max_length) {
  DCHECK(!callback.is_null());
  GetAssociatedFrameRemote()->GetTextSurroundingSelection(max_length,
                                                          std::move(callback));
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
  if (GetParent())
    DCHECK_EQ(GetParent()->GetEnabledBindings(), GetEnabledBindings());

  if (render_frame_created_) {
    if (!frame_bindings_control_)
      GetRemoteAssociatedInterfaces()->GetInterface(&frame_bindings_control_);
    frame_bindings_control_->AllowBindings(enabled_bindings_);
  }
}

int RenderFrameHostImpl::GetEnabledBindings() {
  return enabled_bindings_;
}

void RenderFrameHostImpl::DisableBeforeUnloadHangMonitorForTesting() {
  beforeunload_timeout_.reset();
}

bool RenderFrameHostImpl::IsBeforeUnloadHangMonitorDisabledForTesting() {
  return !beforeunload_timeout_;
}

bool RenderFrameHostImpl::IsFeatureEnabled(
    blink::mojom::FeaturePolicyFeature feature) {
  blink::mojom::PolicyValueType feature_type =
      feature_policy_->GetFeatureList().at(feature).second;
  return feature_policy_ &&
         feature_policy_->IsFeatureEnabledForOrigin(
             feature, GetLastCommittedOrigin(),
             blink::PolicyValue::CreateMaxPolicyValue(feature_type));
}

bool RenderFrameHostImpl::IsFeatureEnabled(
    blink::mojom::FeaturePolicyFeature feature,
    blink::PolicyValue threshold_value) {
  return feature_policy_ &&
         feature_policy_->IsFeatureEnabledForOrigin(
             feature, GetLastCommittedOrigin(), threshold_value);
}

void RenderFrameHostImpl::ViewSource() {
  delegate_->ViewSource(this);
}

void RenderFrameHostImpl::FlushNetworkAndNavigationInterfacesForTesting() {
  DCHECK(network_service_connection_error_handler_holder_);
  network_service_connection_error_handler_holder_.FlushForTesting();

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
         network_service_connection_error_handler_holder_.is_bound());

  network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
  bool bypass_redirect_checks = false;
  if (recreate_default_url_loader_factory_after_network_service_crash_) {
    bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
        last_committed_origin_, network_isolation_key_,
        mojo::MakeRequest(&default_factory_info));
  }

  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
      subresource_loader_factories =
          std::make_unique<blink::URLLoaderFactoryBundleInfo>(
              std::move(default_factory_info),
              blink::URLLoaderFactoryBundleInfo::SchemeMap(),
              CreateInitiatorSpecificURLLoaderFactories(
                  initiators_requiring_separate_url_loader_factory_),
              bypass_redirect_checks);
  GetNavigationControl()->UpdateSubresourceLoaderFactories(
      std::move(subresource_loader_factories));
}

blink::FrameOwnerElementType RenderFrameHostImpl::GetFrameOwnerElementType() {
  return frame_tree_node_->frame_owner_element_type();
}

bool RenderFrameHostImpl::HasTransientUserActivation() {
  return frame_tree_node_->HasTransientUserActivation();
}

void RenderFrameHostImpl::OnDidAccessInitialDocument() {
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
    blink::WebSandboxFlags sandbox_flags,
    const blink::ParsedFeaturePolicy& parsed_header) {
  if (!is_active())
    return;
  // Rebuild the feature policy for this frame.
  ResetFeaturePolicy();
  feature_policy_->SetHeaderPolicy(parsed_header);

  // Update the feature policy and sandbox flags in the frame tree. This will
  // send any updates to proxies if necessary.
  frame_tree_node()->UpdateFramePolicyHeaders(sandbox_flags, parsed_header);

  // Save a copy of the now-active sandbox flags on this RFHI.
  active_sandbox_flags_ = frame_tree_node()->active_sandbox_flags();
}

void RenderFrameHostImpl::OnDidAddContentSecurityPolicies(
    const std::vector<ContentSecurityPolicy>& policies) {
  TRACE_EVENT1("navigation",
               "RenderFrameHostImpl::OnDidAddContentSecurityPolicies",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());

  std::vector<ContentSecurityPolicyHeader> headers;
  for (const ContentSecurityPolicy& policy : policies) {
    AddContentSecurityPolicy(policy);
    headers.push_back(policy.header);
  }
  frame_tree_node()->AddContentSecurityPolicies(headers);
}

void RenderFrameHostImpl::EnforceInsecureRequestPolicy(
    blink::WebInsecureRequestPolicy policy) {
  frame_tree_node()->SetInsecureRequestPolicy(policy);
}

void RenderFrameHostImpl::EnforceInsecureNavigationsSet(
    const std::vector<uint32_t>& set) {
  frame_tree_node()->SetInsecureNavigationsSet(set);
}

FrameTreeNode* RenderFrameHostImpl::FindAndVerifyChild(
    int32_t child_frame_routing_id,
    bad_message::BadMessageReason reason) {
  FrameTreeNode* child = frame_tree_node()->frame_tree()->FindByRoutingID(
      GetProcess()->GetID(), child_frame_routing_id);
  // A race can result in |child| to be nullptr. Avoid killing the renderer in
  // that case.
  if (child && child->parent() != frame_tree_node()) {
    bad_message::ReceivedBadMessage(GetProcess(), reason);
    return nullptr;
  }
  return child;
}

void RenderFrameHostImpl::OnDidChangeFramePolicy(
    int32_t frame_routing_id,
    const blink::FramePolicy& frame_policy) {
  // Ensure that a frame can only update sandbox flags or feature policy for its
  // immediate children.  If this is not the case, the renderer is considered
  // malicious and is killed.
  FrameTreeNode* child = FindAndVerifyChild(
      // TODO(iclelland): Rename this message
      frame_routing_id, bad_message::RFH_SANDBOX_FLAGS);
  if (!child)
    return;

  child->SetPendingFramePolicy(frame_policy);

  // Notify the RenderFrame if it lives in a different process from its parent.
  // The frame's proxies in other processes also need to learn about the updated
  // flags and policy, but these notifications are sent later in
  // RenderFrameHostManager::CommitPendingFramePolicy(), when the frame
  // navigates and the new policies take effect.
  RenderFrameHost* child_rfh = child->current_frame_host();
  if (child_rfh->GetSiteInstance() != GetSiteInstance()) {
    child_rfh->Send(new FrameMsg_DidUpdateFramePolicy(child_rfh->GetRoutingID(),
                                                      frame_policy));
  }
}

void RenderFrameHostImpl::OnDidChangeFrameOwnerProperties(
    int32_t frame_routing_id,
    const FrameOwnerProperties& properties) {
  FrameTreeNode* child = FindAndVerifyChild(
      frame_routing_id, bad_message::RFH_OWNER_PROPERTY);
  if (!child)
    return;

  bool has_display_none_property_changed =
      properties.is_display_none !=
      child->frame_owner_properties().is_display_none;

  child->set_frame_owner_properties(properties);

  child->render_manager()->OnDidUpdateFrameOwnerProperties(properties);
  if (has_display_none_property_changed) {
    delegate_->DidChangeDisplayState(
        child->current_frame_host(),
        properties.is_display_none /* is_display_none */);
  }
}

void RenderFrameHostImpl::OnUpdateTitle(
    const base::string16& title,
    blink::WebTextDirection title_direction) {
  // This message should only be sent for top-level frames.
  if (frame_tree_node_->parent())
    return;

  if (title.length() > kMaxTitleChars) {
    NOTREACHED() << "Renderer sent too many characters in title.";
    return;
  }

  delegate_->UpdateTitle(
      this, title, WebTextDirectionToChromeTextDirection(title_direction));
}

void RenderFrameHostImpl::UpdateEncoding(const std::string& encoding_name) {
  // This message is only sent for top-level frames. TODO(avi): when frame tree
  // mirroring works correctly, add a check here to enforce it.
  delegate_->UpdateEncoding(this, encoding_name);
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

void RenderFrameHostImpl::SetCommitCallbackInterceptorForTesting(
    CommitCallbackInterceptor* interceptor) {
  // This DCHECK's aims to avoid unexpected replacement of an interceptor.
  // If this becomes a legitimate use case, feel free to remove.
  DCHECK(!commit_callback_interceptor_ || !interceptor);
  commit_callback_interceptor_ = interceptor;
}

void RenderFrameHostImpl::OnDidBlockNavigation(
    const GURL& blocked_url,
    const GURL& initiator_url,
    blink::NavigationBlockedReason reason) {
  delegate_->OnDidBlockNavigation(blocked_url, initiator_url, reason);
}

void RenderFrameHostImpl::OnAbortNavigation() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OnAbortNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());
  if (!is_active())
    return;
  frame_tree_node()->navigator()->OnAbortNavigation(frame_tree_node());
}

void RenderFrameHostImpl::OnForwardResourceTimingToParent(
    const ResourceTimingInfo& resource_timing) {
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
  proxy->Send(new FrameMsg_ForwardResourceTimingToParent(proxy->GetRoutingID(),
                                                         resource_timing));
}

void RenderFrameHostImpl::OnDispatchLoad() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::OnDispatchLoad",
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

  proxy->Send(new FrameMsg_DispatchLoad(proxy->GetRoutingID()));
}

RenderWidgetHostViewBase* RenderFrameHostImpl::GetViewForAccessibility() {
  return static_cast<RenderWidgetHostViewBase*>(
      frame_tree_node_->IsMainFrame()
          ? render_view_host_->GetWidget()->GetView()
          : frame_tree_node_->frame_tree()
                ->GetMainFrame()
                ->render_view_host_->GetWidget()
                ->GetView());
}

void RenderFrameHostImpl::OnAccessibilityEvents(
    const AccessibilityHostMsg_EventBundleParams& bundle,
    int reset_token,
    int ack_token) {
  // Don't process this IPC if either we're waiting on a reset and this
  // IPC doesn't have the matching token ID, or if we're not waiting on a
  // reset but this message includes a reset token.
  if (accessibility_reset_token_ != reset_token) {
    Send(new AccessibilityMsg_EventBundle_ACK(routing_id_, ack_token));
    return;
  }
  accessibility_reset_token_ = 0;

  RenderWidgetHostViewBase* view = GetViewForAccessibility();
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (!accessibility_mode.is_mode_off() && view && is_active()) {
    if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs))
      GetOrCreateBrowserAccessibilityManager();

    AXEventNotificationDetails details;
    details.ax_tree_id = GetAXTreeID();
    details.events = bundle.events;

    details.updates.resize(bundle.updates.size());
    for (size_t i = 0; i < bundle.updates.size(); ++i) {
      const AXContentTreeUpdate& src_update = bundle.updates[i];
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
        AXContentNodeDataToAXNodeData(src_update.nodes[j],
                                      &dst_update->nodes[j]);
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
        for (size_t i = 0; i < details.events.size(); i++) {
          if (static_cast<int>(details.events[i].event_type) < 0)
            continue;

          accessibility_testing_callback_.Run(
              this, details.events[i].event_type, details.events[i].id);
        }
      }
    }
  }

  // Always send an ACK or the renderer can be in a bad state.
  Send(new AccessibilityMsg_EventBundle_ACK(routing_id_, ack_token));
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
  DCHECK(IsBackForwardCacheEnabled());

  bool in_back_forward_cache = is_in_back_forward_cache();

  RenderFrameHostImpl* top_document = this;
  while (top_document->parent_) {
    top_document = top_document->parent_;
    DCHECK_EQ(top_document->is_in_back_forward_cache(), in_back_forward_cache);
  }

  if (!in_back_forward_cache) {
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

  // Hold onto a pointer to the navigation controller, in case we need it to
  // reissue the navigation after |this| has been destroyed.
  NavigationControllerImpl* controller = static_cast<NavigationControllerImpl*>(
      frame_tree_node_->navigator()->GetController());
  frame_tree_node_->render_manager()->EvictFromBackForwardCache(top_document);

  // DO NOT REFERENCER ANY MEMBERS after this. The previous call destroyed
  // |this|.

  if (!is_navigation_to_evicted_frame_in_flight)
    return;

  // If we are currently navigating to the frame that was just destroyed, we
  // must restart the navigation. This is important because restarting the
  // navigation deletes the NavigationRequest associated with the destroyed
  // frame (preventing use-after-free).
  int nav_index = controller->GetEntryIndexWithUniqueID(
      in_flight_navigation_request->nav_entry_id());
  controller->GoToIndex(nav_index);
}

void RenderFrameHostImpl::OnAccessibilityLocationChanges(
    const std::vector<AccessibilityHostMsg_LocationChangeParams>& params) {
  if (accessibility_reset_token_)
    return;

  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      render_view_host_->GetWidget()->GetView());
  if (view && is_active()) {
    ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
    if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
      BrowserAccessibilityManager* manager =
          GetOrCreateBrowserAccessibilityManager();
      if (manager)
        manager->OnLocationChanges(params);
    }

    // Send the updates to the automation extension API.
    std::vector<AXLocationChangeNotificationDetails> details;
    details.reserve(params.size());
    for (size_t i = 0; i < params.size(); ++i) {
      const AccessibilityHostMsg_LocationChangeParams& param = params[i];
      AXLocationChangeNotificationDetails detail;
      detail.id = param.id;
      detail.ax_tree_id = GetAXTreeID();
      detail.new_location = param.new_location;
      details.push_back(detail);
    }
    delegate_->AccessibilityLocationChangesReceived(details);
  }
}

void RenderFrameHostImpl::OnAccessibilityFindInPageResult(
    const AccessibilityHostMsg_FindInPageResultParams& params) {
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager) {
      manager->OnFindInPageResult(
          params.request_id, params.match_index, params.start_id,
          params.start_offset, params.end_id, params.end_offset);
    }
  }
}

void RenderFrameHostImpl::OnAccessibilityFindInPageTermination() {
  ui::AXMode accessibility_mode = delegate_->GetAccessibilityMode();
  if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
    BrowserAccessibilityManager* manager =
        GetOrCreateBrowserAccessibilityManager();
    if (manager)
      manager->OnFindInPageTermination();
  }
}

void RenderFrameHostImpl::OnAccessibilityChildFrameHitTestResult(
    int action_request_id,
    const gfx::Point& point,
    int child_frame_routing_id,
    int child_frame_browser_plugin_instance_id,
    ax::mojom::Event event_to_fire) {
  RenderFrameHostImpl* child_frame = nullptr;
  if (child_frame_routing_id) {
    RenderFrameProxyHost* rfph = nullptr;
    LookupRenderFrameHostOrProxy(GetProcess()->GetID(), child_frame_routing_id,
                                 &child_frame, &rfph);
    if (rfph)
      child_frame = rfph->frame_tree_node()->current_frame_host();
  } else if (child_frame_browser_plugin_instance_id) {
    child_frame =
        static_cast<RenderFrameHostImpl*>(delegate()->GetGuestByInstanceID(
            this, child_frame_browser_plugin_instance_id));
  } else {
    NOTREACHED();
  }

  if (!child_frame)
    return;

  ui::AXActionData action_data;
  action_data.request_id = action_request_id;
  action_data.target_point = point;
  action_data.action = ax::mojom::Action::kHitTest;
  action_data.hit_test_event_to_fire = event_to_fire;

  child_frame->AccessibilityPerformAction(action_data);
}

void RenderFrameHostImpl::OnAccessibilitySnapshotResponse(
    int callback_id,
    const AXContentTreeUpdate& snapshot) {
  const auto& it = ax_tree_snapshot_callbacks_.find(callback_id);
  if (it != ax_tree_snapshot_callbacks_.end()) {
    ui::AXTreeUpdate dst_snapshot;
    dst_snapshot.root_id = snapshot.root_id;
    dst_snapshot.nodes.resize(snapshot.nodes.size());
    for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
      AXContentNodeDataToAXNodeData(snapshot.nodes[i],
                                    &dst_snapshot.nodes[i]);
    }
    if (snapshot.has_tree_data) {
      ax_content_tree_data_ = snapshot.tree_data;
      AXContentTreeDataToAXTreeData(&dst_snapshot.tree_data);
      dst_snapshot.has_tree_data = true;
    }
    std::move(it->second).Run(dst_snapshot);
    ax_tree_snapshot_callbacks_.erase(it);
  } else {
    NOTREACHED() << "Received AX tree snapshot response for unknown id";
  }
}

// TODO(alexmos): When the allowFullscreen flag is known in the browser
// process, use it to double-check that fullscreen can be entered here.
void RenderFrameHostImpl::OnEnterFullscreen(
    const blink::WebFullscreenOptions& options) {
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
  for (FrameTreeNode* node = frame_tree_node_; node->parent();
       node = node->parent()) {
    SiteInstance* parent_site_instance =
        node->parent()->current_frame_host()->GetSiteInstance();
    if (base::Contains(notified_instances, parent_site_instance))
      continue;

    RenderFrameProxyHost* child_proxy =
        node->render_manager()->GetRenderFrameProxyHost(parent_site_instance);
    child_proxy->Send(
        new FrameMsg_WillEnterFullscreen(child_proxy->GetRoutingID()));
    notified_instances.insert(parent_site_instance);
  }

  // TODO(alexmos): See if this can use the last committed origin instead.
  delegate_->EnterFullscreenMode(GetLastCommittedURL().GetOrigin(), options);
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
void RenderFrameHostImpl::OnExitFullscreen() {
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

void RenderFrameHostImpl::OnSuddenTerminationDisablerChanged(
    bool present,
    blink::WebSuddenTerminationDisablerType disabler_type) {
  DCHECK_NE(GetSuddenTerminationDisablerState(disabler_type), present);
  if (present) {
    sudden_termination_disabler_types_enabled_ |= disabler_type;
  } else {
    sudden_termination_disabler_types_enabled_ &= ~disabler_type;
  }
}

bool RenderFrameHostImpl::GetSuddenTerminationDisablerState(
    blink::WebSuddenTerminationDisablerType disabler_type) {
  return (sudden_termination_disabler_types_enabled_ & disabler_type) != 0;
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

  // Only inform the FrameTreeNode of a change in load state if the load state
  // of this RenderFrameHost is being tracked.
  if (is_active())
    frame_tree_node_->DidStopLoading();
}

void RenderFrameHostImpl::OnDidChangeLoadProgress(double load_progress) {
  frame_tree_node_->DidChangeLoadProgress(load_progress);
}

void RenderFrameHostImpl::OnSelectionChanged(const base::string16& text,
                                             uint32_t offset,
                                             const gfx::Range& range) {
  has_selection_ = !text.empty();
  GetRenderWidgetHost()->SelectionChanged(text, offset, range);
}

void RenderFrameHostImpl::OnFocusedNodeChanged(
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

void RenderFrameHostImpl::NotifyUserActivation() {
  Send(new FrameMsg_NotifyUserActivation(routing_id_));
}

void RenderFrameHostImpl::DidReceiveFirstUserActivation() {
  delegate_->DidReceiveFirstUserActivation(this);
}

void RenderFrameHostImpl::OnUpdateUserActivationState(
    blink::UserActivationUpdateType update_type) {
  if (!is_active())
    return;
  frame_tree_node_->UpdateUserActivationState(update_type);
}

void RenderFrameHostImpl::OnSetHasReceivedUserGestureBeforeNavigation(
    bool value) {
  frame_tree_node_->OnSetHasReceivedUserGestureBeforeNavigation(value);
}

void RenderFrameHostImpl::OnSetNeedsOcclusionTracking(bool needs_tracking) {
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

  proxy->Send(new FrameMsg_SetNeedsOcclusionTracking(proxy->GetRoutingID(),
                                                     needs_tracking));
}

void RenderFrameHostImpl::OnScrollRectToVisibleInParentFrame(
    const gfx::Rect& rect_to_scroll,
    const blink::WebScrollIntoViewParams& params) {
  RenderFrameProxyHost* proxy =
      frame_tree_node_->render_manager()->GetProxyToParent();
  if (!proxy)
    return;
  proxy->ScrollRectToVisible(rect_to_scroll, params);
}

void RenderFrameHostImpl::OnBubbleLogicalScrollInParentFrame(
    blink::WebScrollDirection direction,
    ui::input_types::ScrollGranularity granularity) {
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

  proxy->BubbleLogicalScroll(direction, granularity);
}

void RenderFrameHostImpl::OnFrameDidCallFocus() {
  delegate_->DidCallFocus();
}

void RenderFrameHostImpl::OnRenderFallbackContentInParentProcess() {
  bool is_object_type =
      frame_tree_node()->current_replication_state().frame_owner_element_type ==
      blink::FrameOwnerElementType::kObject;
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
    rfh->Send(new FrameMsg_RenderFallbackContent(rfh->GetRoutingID()));
  } else if (auto* proxy =
                 frame_tree_node()->render_manager()->GetProxyToParent()) {
    proxy->Send(new FrameMsg_RenderFallbackContent(proxy->GetRoutingID()));
  }
}

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
void RenderFrameHostImpl::OnShowPopup(
    const FrameHostMsg_ShowPopup_Params& params) {
  RenderViewHostDelegateView* view =
      render_view_host_->delegate_->GetDelegateView();
  if (view) {
    gfx::Point original_point(params.bounds.x(), params.bounds.y());
    gfx::Point transformed_point =
        static_cast<RenderWidgetHostViewBase*>(GetView())
            ->TransformPointToRootCoordSpace(original_point);
    gfx::Rect transformed_bounds(transformed_point.x(), transformed_point.y(),
                                 params.bounds.width(), params.bounds.height());
    view->ShowPopupMenu(this, transformed_bounds, params.item_height,
                        params.item_font_size, params.selected_item,
                        params.popup_items, params.right_aligned,
                        params.allow_multiple_selection);
  }
}

void RenderFrameHostImpl::OnHidePopup() {
  RenderViewHostDelegateView* view =
      render_view_host_->delegate_->GetDelegateView();
  if (view)
    view->HidePopupMenu();
}
#endif

void RenderFrameHostImpl::OnRequestOverlayRoutingToken() {
  // Make sure that we have a token.
  GetOverlayRoutingToken();

  Send(new FrameMsg_SetOverlayRoutingToken(routing_id_,
                                           *overlay_routing_token_));
}

void RenderFrameHostImpl::BindInterfaceProviderRequest(
    service_manager::mojom::InterfaceProviderRequest
        interface_provider_request) {
  DCHECK(!document_scoped_interface_provider_binding_.is_bound());
  DCHECK(interface_provider_request.is_pending());
  document_scoped_interface_provider_binding_.Bind(
      FilterRendererExposedInterfaces(mojom::kNavigation_FrameSpec,
                                      GetProcess()->GetID(),
                                      std::move(interface_provider_request)));
}

void RenderFrameHostImpl::BindDocumentInterfaceBrokerReceiver(
    mojo::PendingReceiver<blink::mojom::DocumentInterfaceBroker>
        content_receiver,
    mojo::PendingReceiver<blink::mojom::DocumentInterfaceBroker>
        blink_receiver) {
  DCHECK(!document_interface_broker_content_receiver_.is_bound());
  DCHECK(content_receiver.is_valid());
  document_interface_broker_content_receiver_.Bind(std::move(content_receiver));
  DCHECK(!document_interface_broker_blink_receiver_.is_bound());
  DCHECK(blink_receiver.is_valid());
  document_interface_broker_blink_receiver_.Bind(std::move(blink_receiver));
}

void RenderFrameHostImpl::BindBrowserInterfaceBrokerReceiver(
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver) {
  DCHECK(receiver.is_valid());
  broker_receiver_.Bind(std::move(receiver));
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

std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
RenderFrameHostImpl::CreateCrossOriginPrefetchLoaderFactoryBundle() {
  DCHECK(base::FeatureList::IsEnabled(
      network::features::kPrefetchMainResourceNetworkIsolationKey));
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  bool bypass_redirect_checks = false;
  // Passing a nullopt NetworkIsolationKey ensures the factory is not
  // initialized with a NetworkIsolationKey. This is necessary for a
  // cross-origin prefetch factory because the factory must use the
  // NetworkIsolationKey provided by requests going through it.
  bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
      last_committed_origin_, base::nullopt /* network_isolation_key */,
      pending_default_factory.InitWithNewPipeAndPassReceiver());

  return std::make_unique<blink::URLLoaderFactoryBundleInfo>(
      std::move(pending_default_factory),
      blink::URLLoaderFactoryBundleInfo::SchemeMap(),
      CreateInitiatorSpecificURLLoaderFactories(
          initiators_requiring_separate_url_loader_factory_),
      bypass_redirect_checks);
}

void RenderFrameHostImpl::TransferUserActivationFrom(
    int32_t source_routing_id) {
  RenderFrameHostImpl* source_rfh =
      RenderFrameHostImpl::FromID(GetProcess()->GetID(), source_routing_id);
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
  if (!GetContentClient()->browser()->ShouldAllowOpenURL(GetSiteInstance(),
                                                         params->target_url)) {
    params->target_url = GURL(url::kAboutBlankURL);
  }

  bool effective_transient_activation_state =
      params->mimic_user_gesture ||
      (base::FeatureList::IsEnabled(features::kUserActivationV2) &&
       frame_tree_node_->HasTransientUserActivation());

  // Ignore window creation when sent from a frame that's not current or
  // created.
  bool can_create_window =
      IsCurrent() && render_frame_created_ &&
      GetContentClient()->browser()->CanCreateWindow(
          this, GetLastCommittedURL(),
          frame_tree_node_->frame_tree()->GetMainFrame()->GetLastCommittedURL(),
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
        blink::UserActivationUpdateType::kConsumeTransientActivation);
  }

  if (!can_create_window) {
    std::move(callback).Run(mojom::CreateNewWindowStatus::kIgnore, nullptr);
    return;
  }

  // For Android WebView, we support a pop-up like behavior for window.open()
  // even if the embedding app doesn't support multiple windows. In this case,
  // window.open() will return "window" and navigate it to whatever URL was
  // passed.
  if (!render_view_host_->GetWebkitPreferences().supports_multiple_windows) {
    std::move(callback).Run(mojom::CreateNewWindowStatus::kReuse, nullptr);
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

  // If the opener is suppressed or script access is disallowed, we should
  // open the window in a new BrowsingInstance, and thus a new process. That
  // means the current renderer process will not be able to route messages to
  // it. Because of this, we will immediately show and navigate the window
  // in OnCreateNewWindowOnUI, using the params provided here.
  int render_view_route_id = MSG_ROUTING_NONE;
  int main_frame_route_id = MSG_ROUTING_NONE;
  int main_frame_widget_route_id = MSG_ROUTING_NONE;
  int render_process_id = GetProcess()->GetID();
  if (!params->opener_suppressed && !no_javascript_access) {
    render_view_route_id = GetProcess()->GetNextRoutingID();
    main_frame_route_id = GetProcess()->GetNextRoutingID();
    main_frame_widget_route_id = GetProcess()->GetNextRoutingID();
  }

  DCHECK(IsRenderFrameLive());

  bool opened_by_user_activation = params->mimic_user_gesture;
  if (base::FeatureList::IsEnabled(features::kUserActivationV2))
    opened_by_user_activation = was_consumed;

  delegate_->CreateNewWindow(this, render_view_route_id, main_frame_route_id,
                             main_frame_widget_route_id, *params,
                             opened_by_user_activation, cloned_namespace.get());

  if (main_frame_route_id == MSG_ROUTING_NONE) {
    // Opener suppressed or Javascript access disabled. Never tell the renderer
    // about the new window.
    std::move(callback).Run(mojom::CreateNewWindowStatus::kIgnore, nullptr);
    return;
  }

  bool succeeded =
      RenderWidgetHost::FromID(render_process_id, main_frame_widget_route_id) !=
      nullptr;
  if (!succeeded) {
    // If we did not create a WebContents to host the renderer-created
    // RenderFrame/RenderView/RenderWidget objects, signal failure to the
    // renderer.
    DCHECK(!RenderFrameHost::FromID(render_process_id, main_frame_route_id));
    DCHECK(!RenderViewHost::FromID(render_process_id, render_view_route_id));
    std::move(callback).Run(mojom::CreateNewWindowStatus::kIgnore, nullptr);
    return;
  }

  // The view, widget, and frame should all be routable now.
  DCHECK(RenderViewHost::FromID(render_process_id, render_view_route_id));
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(GetProcess()->GetID(), main_frame_route_id);
  DCHECK(rfh);

  // When the popup is created, it hasn't committed any navigation yet - its
  // initial empty document should inherit the origin of its opener (the origin
  // may change after the first commit).  See also https://crbug.com/932067.
  //
  // Note that that origin of the new frame might depend on sandbox flags.
  // Checking sandbox flags of the new frame should be safe at this point,
  // because the flags should be already inherited by the CreateNewWindow call
  // above.
  rfh->SetOriginOfNewFrame(GetLastCommittedOrigin());

  if (rfh->waiting_for_init_) {
    // Need to check |waiting_for_init_| as some paths inside CreateNewWindow
    // call above (namely, if WebContentsDelegate::ShouldCreateWebContents
    // returns false) will resume requests by calling RenderFrameHostImpl::Init.
    rfh->frame_->BlockRequests();
  }

  service_manager::mojom::InterfaceProviderPtrInfo
      main_frame_interface_provider_info;
  rfh->BindInterfaceProviderRequest(
      mojo::MakeRequest(&main_frame_interface_provider_info));

  mojo::PendingRemote<blink::mojom::DocumentInterfaceBroker>
      document_interface_broker_content;

  mojo::PendingRemote<blink::mojom::DocumentInterfaceBroker>
      document_interface_broker_blink;
  rfh->BindDocumentInterfaceBrokerReceiver(
      document_interface_broker_content.InitWithNewPipeAndPassReceiver(),
      document_interface_broker_blink.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
      browser_interface_broker;
  rfh->BindBrowserInterfaceBrokerReceiver(
      browser_interface_broker.InitWithNewPipeAndPassReceiver());

  mojom::CreateNewWindowReplyPtr reply = mojom::CreateNewWindowReply::New(
      render_view_route_id, main_frame_route_id, main_frame_widget_route_id,
      mojom::DocumentScopedInterfaceBundle::New(
          std::move(main_frame_interface_provider_info),
          std::move(document_interface_broker_content),
          std::move(document_interface_broker_blink),
          std::move(browser_interface_broker)),
      cloned_namespace->id(), rfh->GetDevToolsFrameToken());
  std::move(callback).Run(mojom::CreateNewWindowStatus::kSuccess,
                          std::move(reply));
}

void RenderFrameHostImpl::CreatePortal(
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> pending_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
    CreatePortalCallback callback) {
  // We don't support attaching a portal inside a nested browsing context.
  if (frame_tree_node()->parent()) {
    mojo::ReportBadMessage(
        "RFHI::CreatePortal called in a nested browsing context");
    return;
  }
  Portal* portal =
      Portal::Create(this, std::move(pending_receiver), std::move(client));
  RenderFrameProxyHost* proxy_host = portal->CreateProxyAndAttachPortal();
  std::move(callback).Run(proxy_host->GetRoutingID(), portal->portal_token(),
                          portal->GetDevToolsFrameToken());
}

void RenderFrameHostImpl::AdoptPortal(
    const base::UnguessableToken& portal_token,
    AdoptPortalCallback callback) {
  Portal* portal = Portal::FromToken(portal_token);
  if (!portal) {
    mojo::ReportBadMessage("Unknown portal_token when adopting portal.");
    return;
  }
  if (portal->owner_render_frame_host() != this) {
    mojo::ReportBadMessage("AdoptPortal called from wrong frame.");
    return;
  }
  RenderFrameProxyHost* proxy_host = portal->CreateProxyAndAttachPortal();
  std::move(callback).Run(
      proxy_host->GetRoutingID(),
      proxy_host->frame_tree_node()->current_replication_state(),
      portal->GetDevToolsFrameToken());
}

void RenderFrameHostImpl::IssueKeepAliveHandle(
    mojom::KeepAliveHandleRequest request) {
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
  keep_alive_handle_factory_->Create(std::move(request));
}

// TODO(ahemery): Move checks to mojo bad message reporting.
void RenderFrameHostImpl::BeginNavigation(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
    mojom::NavigationClientAssociatedPtrInfo navigation_client,
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

  DCHECK(IsPerNavigationMojoInterfaceEnabled() == navigation_client.is_valid());

  mojom::CommonNavigationParamsPtr validated_params = common_params.Clone();
  if (!VerifyBeginNavigationCommonParams(GetSiteInstance(), &*validated_params))
    return;

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
      std::move(navigation_initiator), EnsurePrefetchedSignedExchangeCache());
}

void RenderFrameHostImpl::SubresourceResponseStarted(
    const GURL& url,
    net::CertStatus cert_status) {
  delegate_->SubresourceResponseStarted(url, cert_status);
}

void RenderFrameHostImpl::ResourceLoadComplete(
    mojom::ResourceLoadInfoPtr resource_load_info) {
  GlobalRequestID global_request_id;
  if (main_frame_request_ids_.first == resource_load_info->request_id) {
    global_request_id = main_frame_request_ids_.second;
  } else if (resource_load_info->resource_type ==
             content::ResourceType::kMainFrame) {
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

void RenderFrameHostImpl::RegisterMojoInterfaces() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

#if !defined(OS_ANDROID)
  // The default (no-op) implementation of InstalledAppProvider. On Android, the
  // real implementation is provided in Java.
  registry_->AddInterface(
      base::BindRepeating(&InstalledAppProviderImplDefault::Create));
#endif  // !defined(OS_ANDROID)

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(
          GetProcess()->GetBrowserContext());
  if (delegate_) {
    auto* geolocation_context = delegate_->GetGeolocationContext();
    if (geolocation_context) {
      geolocation_service_.reset(new GeolocationServiceImpl(
          geolocation_context, permission_controller, this));
      // NOTE: Both the |interface_registry_| and |geolocation_service_| are
      // owned by |this|, so their destruction will be triggered together.
      // |interface_registry_| is declared after |geolocation_service_|, so it
      // will be destroyed prior to |geolocation_service_|.
      registry_->AddInterface(
          base::Bind(&GeolocationServiceImpl::Bind,
                     base::Unretained(geolocation_service_.get())));
    }
  }

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebNfc)) {
    registry_->AddInterface<device::mojom::NFC>(base::Bind(
        &RenderFrameHostImpl::BindNFCRequest, base::Unretained(this)));
  }
#endif

  if (!permission_service_context_)
    permission_service_context_.reset(new PermissionServiceContext(this));

  registry_->AddInterface(
      base::BindRepeating(&PermissionServiceContext::CreateService,
                          base::Unretained(permission_service_context_.get())));

  registry_->AddInterface(base::BindRepeating(
      [](RenderFrameHostImpl* frame,
         mojo::PendingReceiver<blink::mojom::PresentationService> receiver) {
        if (!frame->presentation_service_)
          frame->presentation_service_ = PresentationServiceImpl::Create(frame);

        frame->presentation_service_->Bind(std::move(receiver));
      },
      base::Unretained(this)));

  registry_->AddInterface(
      base::Bind(&MediaSessionServiceImpl::Create, base::Unretained(this)));

  registry_->AddInterface(base::Bind(
      base::IgnoreResult(&RenderFrameHostImpl::CreateWebBluetoothService),
      base::Unretained(this)));

  registry_->AddInterface(base::BindRepeating(
      &RenderFrameHostImpl::CreateWebUsbService, base::Unretained(this)));

  registry_->AddInterface<media::mojom::InterfaceFactory>(
      base::Bind(&RenderFrameHostImpl::BindMediaInterfaceFactoryRequest,
                 base::Unretained(this)));

  registry_->AddInterface(base::BindRepeating(
      &RenderFrameHostImpl::CreateWebSocketConnector, base::Unretained(this)));

  registry_->AddInterface(base::BindRepeating(
      &RenderFrameHostImpl::CreateDedicatedWorkerHostFactory,
      base::Unretained(this)));

  registry_->AddInterface(base::BindRepeating(
      &SharedWorkerConnectorImpl::Create, process_->GetID(), routing_id_));

  registry_->AddInterface(base::BindRepeating(&device::GamepadMonitor::Create));

  registry_->AddInterface<device::mojom::VRService>(base::Bind(
      &WebvrServiceProvider::BindWebvrService, base::Unretained(this)));

  registry_->AddInterface(
      base::BindRepeating(&RenderFrameHostImpl::CreateAudioInputStreamFactory,
                          base::Unretained(this)));

  registry_->AddInterface(
      base::BindRepeating(&RenderFrameHostImpl::CreateAudioOutputStreamFactory,
                          base::Unretained(this)));

  // BrowserMainLoop::GetInstance() may be null on unit tests.
  if (BrowserMainLoop::GetInstance()) {
    // BrowserMainLoop, which owns MediaStreamManager, is alive for the lifetime
    // of Mojo communication (see BrowserMainLoop::ShutdownThreadsAndCleanUp(),
    // which shuts down Mojo). Hence, passing that MediaStreamManager instance
    // as a raw pointer here is safe.
    MediaStreamManager* media_stream_manager =
        BrowserMainLoop::GetInstance()->media_stream_manager();
    registry_->AddInterface(
        base::Bind(&MediaDevicesDispatcherHost::Create, GetProcess()->GetID(),
                   GetRoutingID(), base::Unretained(media_stream_manager)),
        base::CreateSingleThreadTaskRunner({BrowserThread::IO}));

    registry_->AddInterface(
        base::BindRepeating(&MediaStreamDispatcherHost::Create,
                            GetProcess()->GetID(), GetRoutingID(),
                            base::Unretained(media_stream_manager)),
        base::CreateSingleThreadTaskRunner({BrowserThread::IO}));
  }

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
  registry_->AddInterface(base::Bind(&RemoterFactoryImpl::Bind,
                                     GetProcess()->GetID(), GetRoutingID()));
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)

  registry_->AddInterface(base::BindRepeating(
      &KeyboardLockServiceImpl::CreateMojoService, base::Unretained(this)));

  registry_->AddInterface(base::Bind(&ImageCaptureImpl::Create));

  sensor_provider_proxy_.reset(
      new SensorProviderProxyImpl(permission_controller, this));
  registry_->AddInterface(
      base::Bind(&SensorProviderProxyImpl::Bind,
                 base::Unretained(sensor_provider_proxy_.get())));

#if !defined(OS_ANDROID)
  if (command_line->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    registry_->AddInterface(
        base::BindRepeating(&RenderFrameHostImpl::BindSerialServiceRequest,
                            base::Unretained(this)));
    registry_->AddInterface(
        base::BindRepeating(&HidService::Create, base::Unretained(this)));
  }
#endif  // !defined(OS_ANDROID)

  // Only save decode stats when BrowserContext provides a VideoPerfHistory.
  // Off-the-record contexts will internally use an ephemeral history DB.
  media::VideoDecodePerfHistory::SaveCallback save_stats_cb;
  if (GetSiteInstance()->GetBrowserContext()->GetVideoDecodePerfHistory()) {
    save_stats_cb = GetSiteInstance()
                        ->GetBrowserContext()
                        ->GetVideoDecodePerfHistory()
                        ->GetSaveCallback();
  }

  registry_->AddInterface(base::BindRepeating(
      &media::MediaMetricsProvider::Create,
      GetProcess()->GetBrowserContext()->IsOffTheRecord()
          ? media::MediaMetricsProvider::BrowsingMode::kIncognito
          : media::MediaMetricsProvider::BrowsingMode::kNormal,
      frame_tree_node_->IsMainFrame()
          ? media::MediaMetricsProvider::FrameStatus::kTopFrame
          : media::MediaMetricsProvider::FrameStatus::kNotTopFrame,
      base::BindRepeating(
          &RenderFrameHostDelegate::GetUkmSourceIdForLastCommittedSource,
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
          weak_ptr_factory_.GetWeakPtr())));

  if (command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking)) {
    registry_->AddInterface(
        base::Bind(&InputInjectorImpl::Create, weak_ptr_factory_.GetWeakPtr()));
  }

  // TODO(crbug.com/775792): Move to RendererInterfaceBinders.
  registry_->AddInterface(base::BindRepeating(
      &QuotaDispatcherHost::CreateForFrame, GetProcess(), routing_id_));

  registry_->AddInterface(
      base::BindRepeating(SpeechRecognitionDispatcherHost::Create,
                          GetProcess()->GetID(), routing_id_),
      base::CreateSingleThreadTaskRunner({BrowserThread::IO}));

  file_system_manager_.reset(new FileSystemManagerImpl(
      GetProcess()->GetID(),
      GetProcess()->GetStoragePartition()->GetFileSystemContext(),
      ChromeBlobStorageContext::GetFor(GetProcess()->GetBrowserContext())));

  registry_->AddInterface(base::BindRepeating(&ContactsManagerImpl::Create,
                                              base::Unretained(this)));

  registry_->AddInterface(
      base::BindRepeating(&FileChooserImpl::Create, base::Unretained(this)));

  registry_->AddInterface(base::BindRepeating(&WakeLockServiceImpl::Create,
                                              base::Unretained(this)));

  registry_->AddInterface(base::BindRepeating(
      &PictureInPictureServiceImpl::Create, base::Unretained(this)));

  if (base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI)) {
    registry_->AddInterface(base::BindRepeating(
        [](RenderFrameHostImpl* frame,
           mojo::PendingReceiver<blink::mojom::NativeFileSystemManager>
               receiver) {
          NativeFileSystemManagerImpl::BindReceiverFromUIThread(
              static_cast<StoragePartitionImpl*>(
                  frame->GetProcess()->GetStoragePartition()),
              NativeFileSystemManagerImpl::BindingContext(
                  frame->GetLastCommittedOrigin(), frame->GetLastCommittedURL(),
                  frame->GetProcess()->GetID(), frame->GetRoutingID()),
              std::move(receiver));
        },
        base::Unretained(this)));
  }

  registry_->AddInterface(base::BindRepeating(
      &GetRestrictedCookieManager, base::Unretained(this),
      GetProcess()->GetID(), routing_id_, GetProcess()->GetStoragePartition()));

  if (base::FeatureList::IsEnabled(features::kSmsReceiver)) {
    registry_->AddInterface(base::BindRepeating(
        &RenderFrameHostImpl::BindSmsReceiverReceiver, base::Unretained(this)));
  }
}

void RenderFrameHostImpl::ResetWaitingState() {
  DCHECK(is_active());

  // Whenever we reset the RFH state, we should not be waiting for beforeunload
  // or close acks.  We clear them here to be safe, since they can cause
  // navigations to be ignored in DidCommitProvisionalLoad.
  if (is_waiting_for_beforeunload_ack_) {
    is_waiting_for_beforeunload_ack_ = false;
    if (beforeunload_timeout_)
      beforeunload_timeout_->Stop();
    has_shown_beforeunload_dialog_ = false;
    beforeunload_pending_replies_.clear();
  }
  send_before_unload_start_time_ = base::TimeTicks();
  render_view_host_->is_waiting_for_close_ack_ = false;
  network_service_connection_error_handler_holder_.reset();
}

RenderFrameHostImpl::CanCommitStatus RenderFrameHostImpl::CanCommitOriginAndUrl(
    const url::Origin& origin,
    const GURL& url) {
  // If the --disable-web-security flag is specified, all bets are off and the
  // renderer process can send any origin it wishes.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebSecurity)) {
    return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;
  }

  // Renderer-debug URLs can never be committed.
  if (IsRendererDebugURL(url))
    return CanCommitStatus::CANNOT_COMMIT_URL;

  // TODO(creis): We should also check for WebUI pages here.  Also, when the
  // out-of-process iframes implementation is ready, we should check for
  // cross-site URLs that are not allowed to commit in this process.

  // MHTML subframes can supply URLs at commit time that do not match the
  // process lock. For example, it can be either "cid:..." or arbitrary URL at
  // which the frame was at the time of generating the MHTML
  // (e.g. "http://localhost"). In such cases, don't verify the URL, but require
  // the URL to commit in the process of the main frame.
  if (!frame_tree_node()->IsMainFrame()) {
    RenderFrameHostImpl* main_frame =
        frame_tree_node()->frame_tree()->GetMainFrame();
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
      return CanCommitStatus::CANNOT_COMMIT_URL;
    }
  }

  // Give the client a chance to disallow URLs from committing.
  if (!GetContentClient()->browser()->CanCommitURL(GetProcess(), url))
    return CanCommitStatus::CANNOT_COMMIT_URL;

  // TODO(nasko): This check should be updated to apply to all URLs, not just
  // standard ones.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (url.IsStandard() &&
      !policy->CanAccessDataForOrigin(GetProcess()->GetID(), url)) {
    return CanCommitStatus::CANNOT_COMMIT_URL;
  }

  // It is safe to commit into a opaque origin, regardless of the URL, as it is
  // restricted from accessing other origins.
  if (origin.opaque())
    return CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL;

  // Standard URLs must match the reported origin.
  if (url.IsStandard() && !origin.IsSameOriginWith(url::Origin::Create(url)))
    return CanCommitStatus::CANNOT_COMMIT_ORIGIN;

  // A non-opaque origin must be a valid URL, which allows us to safely do a
  // conversion to GURL.
  GURL origin_url = origin.GetURL();

  if (!policy->CanAccessDataForOrigin(GetProcess()->GetID(), origin))
    return CanCommitStatus::CANNOT_COMMIT_ORIGIN;

  // Verify that the origin is allowed to commit in this process.
  // Note: This also handles non-standard cases for |url|, such as
  // about:blank, data, and blob URLs.

  // Renderer-debug URLs can never be committed.
  if (IsRendererDebugURL(origin_url))
    return CanCommitStatus::CANNOT_COMMIT_ORIGIN;

  // Give the client a chance to disallow URLs from committing.
  if (!GetContentClient()->browser()->CanCommitURL(GetProcess(), origin_url))
    return CanCommitStatus::CANNOT_COMMIT_ORIGIN;

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
      base::TimeTicks::Now(), "GET", nullptr, base::Optional<SourceLocation>(),
      false /* started_from_context_menu */, false /* has_user_gesture */,
      InitiatorCSPInfo(), std::vector<int>(), std::string(),
      false /* is_history_navigation_in_new_child_frame */, base::TimeTicks());
  CommitNavigation(nullptr /* navigation_request */, std::move(common_params),
                   CreateCommitNavigationParams(), nullptr /* response_head */,
                   mojo::ScopedDataPipeConsumerHandle(),
                   network::mojom::URLLoaderClientEndpointsPtr(), false,
                   base::nullopt, base::nullopt /* subresource_overrides */,
                   nullptr /* provider_info */,
                   base::UnguessableToken::Create() /* not traced */,
                   nullptr /* bundled_exchanges_factory */);
}

void RenderFrameHostImpl::Stop() {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::Stop", "frame_tree_node",
               frame_tree_node_->frame_tree_node_id());
  Send(new FrameMsg_Stop(routing_id_));
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
    // event from wiping out the is_waiting_for_beforeunload_ack_ state.
    if (frame_tree_node_->navigation_request() &&
        frame_tree_node_->navigation_request()->navigation_handle()) {
      frame_tree_node_->navigation_request()->set_net_error(net::ERR_ABORTED);
    }
    frame_tree_node_->ResetNavigationRequest(false, true);
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
          self->frame_tree_node_->render_manager()->OnBeforeUnloadACK(
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
  if (is_waiting_for_beforeunload_ack_) {
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
    is_waiting_for_beforeunload_ack_ = true;
    beforeunload_dialog_request_cancels_unload_ = false;
    unload_ack_is_for_navigation_ = for_navigation;
    send_before_unload_start_time_ = base::TimeTicks::Now();
    if (render_view_host_->GetDelegate()->IsJavaScriptDialogShowing()) {
      // If there is a JavaScript dialog up, don't bother sending the renderer
      // the unload event because it is known unresponsive, waiting for the
      // reply from the dialog. If this incoming request is for a DISCARD be
      // sure to reply with |proceed = false|, because the presence of a dialog
      // indicates that the page can't be discarded.
      SimulateBeforeUnloadAck(type != BeforeUnloadType::DISCARD);
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
    bool should_run_beforeunload =
        rfh->GetSuddenTerminationDisablerState(blink::kBeforeUnloadHandler);
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

    rfh->Send(new FrameMsg_BeforeUnload(rfh->GetRoutingID(), is_reload));
  }

  return found_beforeunload;
}

void RenderFrameHostImpl::SimulateBeforeUnloadAck(bool proceed) {
  DCHECK(is_waiting_for_beforeunload_ack_);
  base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;

  // Dispatch the ACK to prevent re-entrancy.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderFrameHostImpl::ProcessBeforeUnloadACK,
                     weak_ptr_factory_.GetWeakPtr(), proceed,
                     true /* treat_as_final_ack */, approx_renderer_start_time,
                     base::TimeTicks::Now()));
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

  DCHECK_NE(UnloadState::NotRun, unload_state_);
  for (std::unique_ptr<FrameTreeNode>& child_frame : children_) {
    for (FrameTreeNode* node :
         frame_tree_node_->frame_tree()->SubtreeNodes(child_frame.get())) {
      RenderFrameHostImpl* child = node->current_frame_host();
      if (child->unload_state_ != UnloadState::NotRun)
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
        child->unload_state_ =
            child->GetSuddenTerminationDisablerState(blink::kUnloadHandler)
                ? UnloadState::InProgress
                : UnloadState::Completed;
      }

      node->frame_tree()->FrameUnloading(node);
    }
  }
}

void RenderFrameHostImpl::PendingDeletionCheckCompleted() {
  if (unload_state_ == UnloadState::Completed && children_.empty()) {
    if (is_waiting_for_swapout_ack_)
      OnSwappedOut();
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
  frame_tree_node_->ResetNavigationRequest(false, false);
  frame_tree_node_->render_manager()->CleanUpNavigation();
}

void RenderFrameHostImpl::OnUnloadTimeout() {
  DCHECK_NE(unload_state_, UnloadState::NotRun);
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
  Send(new FrameMsg_SetFocusedFrame(routing_id_));
}

void RenderFrameHostImpl::AdvanceFocus(blink::WebFocusType type,
                                       RenderFrameProxyHost* source_proxy) {
  DCHECK(!source_proxy ||
         (source_proxy->GetProcess()->GetID() == GetProcess()->GetID()));
  int32_t source_proxy_routing_id = MSG_ROUTING_NONE;
  if (source_proxy)
    source_proxy_routing_id = source_proxy->GetRoutingID();
  Send(
      new FrameMsg_AdvanceFocus(GetRoutingID(), type, source_proxy_routing_id));
}

void RenderFrameHostImpl::JavaScriptDialogClosed(
    IPC::Message* reply_msg,
    bool success,
    const base::string16& user_input) {
  GetProcess()->SetBlocked(false);

  SendJavaScriptDialogReply(reply_msg, success, user_input);

  // If executing as part of beforeunload event handling, there may have been
  // timers stopped in this frame or a frame up in the frame hierarchy. Restart
  // any timers that were stopped in OnRunBeforeUnloadConfirm().
  for (RenderFrameHostImpl* frame = this; frame; frame = frame->GetParent()) {
    if (frame->is_waiting_for_beforeunload_ack_ &&
        frame->beforeunload_timeout_) {
      frame->beforeunload_timeout_->Start(beforeunload_timeout_delay_);
    }
  }
}

void RenderFrameHostImpl::SendJavaScriptDialogReply(
    IPC::Message* reply_msg,
    bool success,
    const base::string16& user_input) {
  FrameHostMsg_RunJavaScriptDialog::WriteReplyParams(reply_msg, success,
                                                     user_input);
  Send(reply_msg);
}

void RenderFrameHostImpl::CommitNavigation(
    NavigationRequest* navigation_request,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    network::ResourceResponse* response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    bool is_view_source,
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
    const base::UnguessableToken& devtools_navigation_token,
    std::unique_ptr<BundledExchangesHandle> bundled_exchanges_handle) {
  bundled_exchanges_handle_ = std::move(bundled_exchanges_handle);

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
    RenderFrameHostImpl* root =
        frame_tree_node()->frame_tree()->root()->current_frame_host();
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
    if (frame_tree_node_->IsMainFrame()) {
      // Main frame srcdoc navigation are meaningless. They are blocked whenever
      // a navigation attempt is made.
      //
      // TODO(arthursonzogni): Replace DumpWithoutCrashing by a CHECK on M79 if
      // it is never reached.
      NOTREACHED();
      base::debug::DumpWithoutCrashing();
    } else {
      // An about:srcdoc document is always same SiteInstance with its parent.
      // Otherwise, it won't be able to load. The parent's document contains the
      // iframe and its srcdoc attribute.
      //
      // TODO(arthursonzogni): Replace DumpWithoutCrashing by a CHECK on M80 if
      // it is never reached.
      if (GetSiteInstance() != parent_->GetSiteInstance()) {
        NOTREACHED();
        base::debug::DumpWithoutCrashing();
      }
    }
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
        lock_url.spec());
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
    render_view_host()->Send(new FrameMsg_EnableViewSourceMode(routing_id_));
  }

  const network::ResourceResponseHead head =
      response_head ? response_head->head : network::ResourceResponseHead();
  const bool is_same_document =
      NavigationTypeUtils::IsSameDocument(common_params->navigation_type);

  // Network isolation key should be filled before the URLLoaderFactory for
  // sub-resources is created. Only update for cross document navigations since
  // for opaque origin same document navigations, a new origin should not be
  // created as that would be different from the original.
  // TODO(crbug.com/971796): For about:blank and other such urls,
  // common_params->url currently leads to an opaque origin to be created. Once
  // this is fixed, the origin which these navigations eventually get committed
  // to will be available here as well.
  // TODO(crbug.com/979296): Consider changing this code to copy an origin
  // instead of creating one from a URL which lacks opacity information.
  if (!is_same_document) {
    const url::Origin frame_origin = url::Origin::Create(common_params->url);
    const url::Origin top_frame_origin =
        frame_tree_node_->IsMainFrame() ? frame_origin
                                        : frame_tree_->root()->current_origin();
    network_isolation_key_ =
        net::NetworkIsolationKey(top_frame_origin, frame_origin);
  }

  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
      subresource_loader_factories;
  if ((!is_same_document || is_first_navigation) && !is_srcdoc) {
    recreate_default_url_loader_factory_after_network_service_crash_ = false;
    subresource_loader_factories =
        std::make_unique<blink::URLLoaderFactoryBundleInfo>();
    BrowserContext* browser_context = GetSiteInstance()->GetBrowserContext();
    // NOTE: On Network Service navigations, we want to ensure that a frame is
    // given everything it will need to load any accessible subresources. We
    // however only do this for cross-document navigations, because the
    // alternative would be redundant effort.
    if (subresource_loader_params &&
        subresource_loader_params->pending_appcache_loader_factory.is_valid()) {
      // If the caller has supplied a factory for AppCache, use it.
      auto pending_factory =
          std::move(subresource_loader_params->pending_appcache_loader_factory);

#if defined(OS_ANDROID)
      GetContentClient()
          ->browser()
          ->WillCreateURLLoaderFactoryForAppCacheSubresource(
              GetProcess()->GetID(), &pending_factory);
#endif

      subresource_loader_factories->pending_appcache_factory() =
          std::move(pending_factory);

      // Inject test intermediary if needed.
      if (!GetCreateNetworkFactoryCallbackForRenderFrame().is_null()) {
        mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory =
            std::move(subresource_loader_factories->pending_appcache_factory());
        mojo::PendingReceiver<network::mojom::URLLoaderFactory> new_receiver =
            subresource_loader_factories->pending_appcache_factory()
                .InitWithNewPipeAndPassReceiver();
        GetCreateNetworkFactoryCallbackForRenderFrame().Run(
            std::move(new_receiver), GetProcess()->GetID(),
            std::move(original_factory));
      }
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
          GetOriginForURLLoaderFactory(navigation_request)
              .value_or(url::Origin()),
          &factory_receiver, nullptr /* header_client */,
          nullptr /* bypass_redirect_checks */);
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
      recreate_default_url_loader_factory_after_network_service_crash_ = true;
      bool bypass_redirect_checks =
          CreateNetworkServiceDefaultFactoryAndObserve(
              GetOriginForURLLoaderFactory(navigation_request),
              network_isolation_key_,
              pending_default_factory.InitWithNewPipeAndPassReceiver());
      subresource_loader_factories->set_bypass_redirect_checks(
          bypass_redirect_checks);
    }

    if (bundled_exchanges_handle_ &&
        bundled_exchanges_handle_->IsReadyForLoading()) {
      mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory(
          std::move(pending_default_factory));
      bundled_exchanges_handle_->CreateURLLoaderFactory(
          pending_default_factory.InitWithNewPipeAndPassReceiver(),
          std::move(fallback_factory));
    }

    DCHECK(pending_default_factory);
    subresource_loader_factories->pending_default_factory() =
        std::move(pending_default_factory);

    // Only file resources can load file subresources.
    //
    // Other URLs like about:srcdoc or about:blank might be able load files, but
    // only because they will inherit loaders from their parents instead of the
    // ones provided by the browser process here.
    if (common_params->url.SchemeIsFile()) {
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
          base::CreateSequencedTaskRunner(
              {base::ThreadPool(), base::MayBlock(),
               base::TaskPriority::BEST_EFFORT,
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
          GetOriginForURLLoaderFactory(navigation_request)
              .value_or(url::Origin()),
          &factory_receiver, nullptr /* header_client */,
          nullptr /* bypass_redirect_checks */);
      // Keep DevTools proxy last, i.e. closest to the network.
      devtools_instrumentation::WillCreateURLLoaderFactory(
          this, false /* is_navigation */, false /* is_download */,
          &factory_receiver);
      factory.second->Clone(std::move(factory_receiver));
      subresource_loader_factories->pending_scheme_specific_factories().emplace(
          factory.first, std::move(pending_factory_proxy));
    }

    subresource_loader_factories->pending_initiator_specific_factories() =
        CreateInitiatorSpecificURLLoaderFactories(
            initiators_requiring_separate_url_loader_factory_);
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
                       same_document_navigation_request_->navigation_handle()
                           ->GetNavigationId(),
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

    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
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
    if (IsPerNavigationMojoInterfaceEnabled() && navigation_request)
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
    // RenderFrameHostManager::SwapOutOldFrame.
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

    SendCommitNavigation(
        navigation_client, navigation_request, std::move(common_params),
        std::move(commit_params), head, std::move(response_body),
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

  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
      subresource_loader_factories;
  // TODO(domfarolino, crbug.com/955171): Replace this with Remote.
  network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
  bool bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
      origin, network_isolation_key_, mojo::MakeRequest(&default_factory_info));
  subresource_loader_factories =
      std::make_unique<blink::URLLoaderFactoryBundleInfo>(
          std::move(default_factory_info),
          blink::URLLoaderFactoryBundleInfo::SchemeMap(),
          blink::URLLoaderFactoryBundleInfo::OriginMap(),
          bypass_redirect_checks);

  mojom::NavigationClient* navigation_client = nullptr;
  if (IsPerNavigationMojoInterfaceEnabled())
    navigation_client = navigation_request->GetCommitNavigationClient();

  SendCommitFailedNavigation(
      navigation_client, navigation_request, common_params.Clone(),
      commit_params.Clone(), has_stale_copy_in_cache, error_code,
      error_page_content, std::move(subresource_loader_factories));

  // An error page is expected to commit, hence why is_loading_ is set to true.
  is_loading_ = true;
  DCHECK(navigation_request && navigation_request->navigation_handle() &&
         navigation_request->navigation_handle()->GetNetErrorCode() != net::OK);
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
}

void RenderFrameHostImpl::SetUpMojoIfNeeded() {
  if (registry_.get())
    return;

  associated_registry_ = std::make_unique<blink::AssociatedInterfaceRegistry>();
  registry_ = std::make_unique<service_manager::BinderRegistry>();

  auto make_binding = [](RenderFrameHostImpl* impl,
                         mojom::FrameHostAssociatedRequest request) {
    impl->frame_host_associated_binding_.Bind(std::move(request));
  };
  associated_registry_->AddInterface(
      base::BindRepeating(make_binding, base::Unretained(this)));

  associated_registry_->AddInterface(base::BindRepeating(
      [](RenderFrameHostImpl* self,
         mojo::PendingAssociatedReceiver<blink::mojom::PortalHost> receiver) {
        Portal::BindPortalHostReceiver(self, std::move(receiver));
      },
      base::Unretained(this)));

  RegisterMojoInterfaces();
  mojom::FrameFactoryPtr frame_factory;
  BindInterface(GetProcess(), &frame_factory);
  frame_factory->CreateFrame(routing_id_, MakeRequest(&frame_));

  service_manager::mojom::InterfaceProviderPtr remote_interfaces;
  frame_->GetInterfaceProvider(mojo::MakeRequest(&remote_interfaces));
  remote_interfaces_.reset(new service_manager::InterfaceProvider);
  remote_interfaces_->Bind(std::move(remote_interfaces));

  remote_interfaces_->GetInterface(
      frame_input_handler_.BindNewPipeAndPassReceiver());
}

void RenderFrameHostImpl::InvalidateMojoConnection() {
  registry_.reset();

  frame_.reset();
  frame_bindings_control_.reset();
  frame_host_associated_binding_.Close();
  navigation_control_.reset();
  frame_input_handler_.reset();

  // Disconnect with ImageDownloader Mojo service in Blink.
  mojo_image_downloader_.reset();

  // The geolocation service and sensor provider proxy may attempt to cancel
  // permission requests so they must be reset before the routing_id mapping is
  // removed.
  geolocation_service_.reset();
  sensor_provider_proxy_.reset();
}

bool RenderFrameHostImpl::IsFocused() {
  if (!GetRenderWidgetHost()->is_focused() || !frame_tree_->GetFocusedFrame())
    return false;

  RenderFrameHostImpl* focused_rfh =
      frame_tree_->GetFocusedFrame()->current_frame_host();
  return focused_rfh == this || focused_rfh->IsDescendantOf(this);
}

bool RenderFrameHostImpl::UpdatePendingWebUI(const GURL& dest_url,
                                             int entry_bindings) {
  WebUI::TypeID new_web_ui_type =
      WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          GetSiteInstance()->GetBrowserContext(), dest_url);

  // If the required WebUI matches the pending WebUI or if it matches the
  // to-be-reused active WebUI, then leave everything as is.
  if (new_web_ui_type == pending_web_ui_type_ ||
      (should_reuse_web_ui_ && new_web_ui_type == web_ui_type_)) {
    return false;
  }

  // Reset the pending WebUI as from this point it will certainly not be reused.
  ClearPendingWebUI();

  // If error page isolation is enabled and this RenderFrameHost is going to
  // commit an error page, there is no reason to create WebUI and give the
  // process any bindings.
  if (GetSiteInstance()->GetSiteURL() == GURL(kUnreachableWebDataURL))
    return true;

  // If this navigation is not to a WebUI, skip directly to bindings work.
  if (new_web_ui_type != WebUI::kNoWebUI) {
    if (new_web_ui_type == web_ui_type_) {
      // The active WebUI should be reused when dest_url requires a WebUI and
      // its type matches the current.
      DCHECK(web_ui_);
      should_reuse_web_ui_ = true;
    } else {
      // Otherwise create a new pending WebUI.
      pending_web_ui_ = delegate_->CreateWebUIForRenderFrameHost(dest_url);
      DCHECK(pending_web_ui_);
      pending_web_ui_type_ = new_web_ui_type;

      // If we have assigned (zero or more) bindings to the NavigationEntry in
      // the past, make sure we're not granting it different bindings than it
      // had before. If so, note it and don't give it any bindings, to avoid a
      // potential privilege escalation.
      if (entry_bindings != NavigationEntryImpl::kInvalidBindings &&
          pending_web_ui_->GetBindings() != entry_bindings) {
        RecordAction(
            base::UserMetricsAction("ProcessSwapBindingsMismatch_RVHM"));
        ClearPendingWebUI();
      }
    }
  }
  DCHECK_EQ(!pending_web_ui_, pending_web_ui_type_ == WebUI::kNoWebUI);

  // Either grant or check the RenderViewHost with/for proper bindings.
  if (pending_web_ui_ && !render_view_host_->GetProcess()->IsForGuestsOnly()) {
    // If a WebUI was created for the URL and the RenderView is not in a guest
    // process, then enable missing bindings.
    int new_bindings = pending_web_ui_->GetBindings();
    if ((GetEnabledBindings() & new_bindings) != new_bindings) {
      AllowBindings(new_bindings);
    }
  } else if (render_view_host_->is_active()) {
    // If the ongoing navigation is not to a WebUI or the RenderView is in a
    // guest process, ensure that we don't create an unprivileged RenderView in
    // a WebUI-enabled process unless it's swapped out.
    bool url_acceptable_for_webui =
        WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
            GetSiteInstance()->GetBrowserContext(), dest_url);
    if (!url_acceptable_for_webui) {
      CHECK(!ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          GetProcess()->GetID()));
    }
  }
  return true;
}

void RenderFrameHostImpl::CommitPendingWebUI() {
  if (should_reuse_web_ui_) {
    should_reuse_web_ui_ = false;
  } else {
    web_ui_ = std::move(pending_web_ui_);
    web_ui_type_ = pending_web_ui_type_;
    pending_web_ui_type_ = WebUI::kNoWebUI;
  }
  DCHECK(!pending_web_ui_ && pending_web_ui_type_ == WebUI::kNoWebUI &&
         !should_reuse_web_ui_);
}

void RenderFrameHostImpl::ClearPendingWebUI() {
  pending_web_ui_.reset();
  pending_web_ui_type_ = WebUI::kNoWebUI;
  should_reuse_web_ui_ = false;
}

void RenderFrameHostImpl::ClearAllWebUI() {
  ClearPendingWebUI();
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
  if (!find_in_page_ || !find_in_page_.is_bound() ||
      !find_in_page_.is_connected())
    GetRemoteAssociatedInterfaces()->GetInterface(&find_in_page_);
  return find_in_page_;
}

const mojo::AssociatedRemote<blink::mojom::Frame>&
RenderFrameHostImpl::GetAssociatedFrameRemote() {
  if (!frame_remote_)
    GetRemoteAssociatedInterfaces()->GetInterface(&frame_remote_);
  return frame_remote_;
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

void RenderFrameHostImpl::SuppressFurtherDialogs() {
  Send(new FrameMsg_SuppressFurtherDialogs(GetRoutingID()));
}

void RenderFrameHostImpl::ClearFocusedElement() {
  has_focused_editable_element_ = false;
  Send(new FrameMsg_ClearFocusedElement(GetRoutingID()));
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
  Send(new FrameMsg_SetAccessibilityMode(routing_id_,
                                         delegate_->GetAccessibilityMode()));
}

void RenderFrameHostImpl::RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                                                ui::AXMode ax_mode) {
  static int next_id = 1;
  int callback_id = next_id++;
  Send(new AccessibilityMsg_SnapshotTree(routing_id_, callback_id,
                                         ax_mode.mode()));
  ax_tree_snapshot_callbacks_.emplace(callback_id, std::move(callback));
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

void RenderFrameHostImpl::SetTextTrackSettings(
    const FrameMsg_TextTrackSettings_Params& params) {
  DCHECK(!GetParent());
  Send(new FrameMsg_SetTextTrackSettings(routing_id_, params));
}

BrowserAccessibilityManager*
    RenderFrameHostImpl::GetOrCreateBrowserAccessibilityManager() {
  RenderWidgetHostViewBase* view = GetViewForAccessibility();
  if (view &&
      !browser_accessibility_manager_ &&
      !no_create_browser_accessibility_manager_for_testing_) {
    bool is_root_frame = !frame_tree_node()->parent();
    browser_accessibility_manager_.reset(
        view->CreateBrowserAccessibilityManager(this, is_root_frame));
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
  return this == frame_tree_node_->current_frame_host();
}

size_t RenderFrameHostImpl::GetProxyCount() {
  if (!IsCurrent())
    return 0;
  return frame_tree_node_->render_manager()->GetProxyCount();
}

bool RenderFrameHostImpl::HasSelection() {
  return has_selection_;
}

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)

void RenderFrameHostImpl::DidSelectPopupMenuItem(int selected_index) {
  Send(new FrameMsg_SelectPopupMenuItem(routing_id_, selected_index));
}

void RenderFrameHostImpl::DidCancelPopupMenu() {
  Send(new FrameMsg_SelectPopupMenuItem(routing_id_, -1));
}

#else

void RenderFrameHostImpl::DidSelectPopupMenuItems(
    const std::vector<int>& selected_indices) {
  Send(new FrameMsg_SelectPopupMenuItems(routing_id_, false, selected_indices));
}

void RenderFrameHostImpl::DidCancelPopupMenu() {
  Send(new FrameMsg_SelectPopupMenuItems(
      routing_id_, true, std::vector<int>()));
}

#endif
#endif

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
    if (common_params.url.SchemeIs(url::kDataScheme) &&
        !common_params.base_url_for_data_url.is_empty()) {
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
  // earlier, when they are recieved from the renderer (in ShouldServiceRequest
  // called from ResourceDispatcherHostImpl::BeginRequest).
  if (common_params.post_data)
    GrantFileAccessFromResourceRequestBody(*common_params.post_data);
}

std::set<int> RenderFrameHostImpl::GetNavigationEntryIdsPendingCommit() {
  std::set<int> result;
  if (navigation_request_)
    result.insert(navigation_request_->nav_entry_id());
  for (auto const& requests : navigation_requests_)
    result.insert(requests.second->nav_entry_id());
  return result;
}

mojom::NavigationClientAssociatedPtr
RenderFrameHostImpl::GetNavigationClientFromInterfaceProvider() {
  mojom::NavigationClientAssociatedPtr navigation_client_ptr;
  GetRemoteAssociatedInterfaces()->GetInterface(&navigation_client_ptr);
  return navigation_client_ptr;
}

void RenderFrameHostImpl::NavigationRequestCancelled(
    NavigationRequest* navigation_request) {
  OnCrossDocumentCommitProcessed(navigation_request,
                                 blink::mojom::CommitResult::Aborted);
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryAndObserve(
    const base::Optional<url::Origin>& origin,
    base::Optional<net::NetworkIsolationKey> network_isolation_key,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  bool bypass_redirect_checks = CreateNetworkServiceDefaultFactoryInternal(
      origin, network_isolation_key, std::move(default_factory_receiver));

  // Add connection error observer when Network Service is running
  // out-of-process.
  if (IsOutOfProcessNetworkService() &&
      (!network_service_connection_error_handler_holder_ ||
       network_service_connection_error_handler_holder_.encountered_error())) {
    StoragePartition* storage_partition = BrowserContext::GetStoragePartition(
        GetSiteInstance()->GetBrowserContext(), GetSiteInstance());
    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = GetProcess()->GetID();
    storage_partition->GetNetworkContext()->CreateURLLoaderFactory(
        mojo::MakeRequest(&network_service_connection_error_handler_holder_),
        std::move(params));
    network_service_connection_error_handler_holder_
        .set_connection_error_handler(base::BindOnce(
            &RenderFrameHostImpl::UpdateSubresourceLoaderFactories,
            weak_ptr_factory_.GetWeakPtr()));
  }
  return bypass_redirect_checks;
}

bool RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryInternal(
    const base::Optional<url::Origin>& origin,
    base::Optional<net::NetworkIsolationKey> network_isolation_key,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        default_factory_receiver) {
  auto* context = GetSiteInstance()->GetBrowserContext();
  bool bypass_redirect_checks = false;

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      context, this, GetProcess()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      origin.value_or(url::Origin()), &default_factory_receiver, &header_client,
      &bypass_redirect_checks);

  // Keep DevTools proxy last, i.e. closest to the network.
  devtools_instrumentation::WillCreateURLLoaderFactory(
      this, false /* is_navigation */, false /* is_download */,
      &default_factory_receiver);

  WebPreferences preferences = GetRenderViewHost()->GetWebkitPreferences();
  mojo::Remote<network::mojom::URLLoaderFactory> original_factory;

  bool factory_callback_is_null =
      GetCreateNetworkFactoryCallbackForRenderFrame().is_null();
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver =
      factory_callback_is_null ? std::move(default_factory_receiver)
                               : original_factory.BindNewPipeAndPassReceiver();
  // Create the URLLoaderFactory - either via ContentBrowserClient or ourselves.

  // If |network_isolation_key| has a value, create an untrusted
  // URLLoaderFactory with |network_isolation_key|. Otherwise, create a trusted
  // URLLoaderFactory with no NetworkIsolationKey. This is so the factory can
  // consume trusted requests using their NetworkIsolationKey.
  if (network_isolation_key) {
    GetProcess()->CreateURLLoaderFactory(
        origin, cross_origin_embedder_policy_, &preferences,
        network_isolation_key.value(), std::move(header_client),
        std::move(factory_receiver));
  } else {
    // The ability to create a trusted URLLoaderFactory is not exposed on
    // RenderProcessHost's public API.
    static_cast<RenderProcessHostImpl*>(GetProcess())
        ->CreateTrustedURLLoaderFactory(origin, cross_origin_embedder_policy_,
                                        &preferences, std::move(header_client),
                                        std::move(factory_receiver));
  }

  if (!factory_callback_is_null) {
    GetCreateNetworkFactoryCallbackForRenderFrame().Run(
        std::move(default_factory_receiver), GetProcess()->GetID(),
        original_factory.Unbind());
  }

  return bypass_redirect_checks;
}

bool RenderFrameHostImpl::CanExecuteJavaScript() {
#if defined(OS_ANDROID) || defined(OS_FUCHSIA) || defined(IS_CHROMECAST)
  if (g_allow_injecting_javascript)
    return true;
#endif
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
  RenderFrameHostImpl* rfh = nullptr;
  RenderFrameProxyHost* rfph = nullptr;
  LookupRenderFrameHostOrProxy(process_id, routing_id, &rfh, &rfph);
  if (rfh) {
    return rfh->GetFrameTreeNodeId();
  } else if (rfph) {
    return rfph->frame_tree_node()->frame_tree_node_id();
  }
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
  RenderFrameHostImpl* rfh = nullptr;
  RenderFrameProxyHost* rfph = nullptr;
  LookupRenderFrameHostOrProxy(GetProcess()->GetID(), routing_id, &rfh, &rfph);
  if (rfph) {
    rfh = rfph->frame_tree_node()->current_frame_host();
  }

  if (!rfh)
    return ui::AXTreeIDUnknown();

  return rfh->GetAXTreeID();
}

ui::AXTreeID RenderFrameHostImpl::BrowserPluginInstanceIDToAXTreeID(
    int instance_id) {
  RenderFrameHostImpl* guest = static_cast<RenderFrameHostImpl*>(
      delegate()->GetGuestByInstanceID(this, instance_id));
  if (!guest)
    return ui::AXTreeIDUnknown();

  // Create a mapping from the guest to its embedder's AX Tree ID, and
  // explicitly update the guest to propagate that mapping immediately.
  guest->set_browser_plugin_embedder_ax_tree_id(GetAXTreeID());
  guest->UpdateAXTreeData();

  return guest->GetAXTreeID();
}

void RenderFrameHostImpl::AXContentNodeDataToAXNodeData(
    const AXContentNodeData& src,
    ui::AXNodeData* dst) {
  // Copy the common fields.
  *dst = src;

  // Map content-specific attributes based on routing IDs or browser plugin
  // instance IDs to generic attributes with global AXTreeIDs.
  for (auto iter : src.content_int_attributes) {
    AXContentIntAttribute attr = iter.first;
    int32_t value = iter.second;
    switch (attr) {
      case AX_CONTENT_ATTR_CHILD_ROUTING_ID:
        dst->string_attributes.push_back(
            std::make_pair(ax::mojom::StringAttribute::kChildTreeId,
                           RoutingIDToAXTreeID(value).ToString()));
        break;
      case AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID:
        dst->string_attributes.push_back(std::make_pair(
            ax::mojom::StringAttribute::kChildTreeId,
            BrowserPluginInstanceIDToAXTreeID(value).ToString()));
        break;
      case AX_CONTENT_INT_ATTRIBUTE_LAST:
        NOTREACHED();
        break;
    }
  }
}

void RenderFrameHostImpl::AXContentTreeDataToAXTreeData(
    ui::AXTreeData* dst) {
  const AXContentTreeData& src = ax_content_tree_data_;

  // Copy the common fields.
  *dst = src;

  if (src.routing_id != -1)
    dst->tree_id = RoutingIDToAXTreeID(src.routing_id);

  if (src.parent_routing_id != -1)
    dst->parent_tree_id = RoutingIDToAXTreeID(src.parent_routing_id);

  if (browser_plugin_embedder_ax_tree_id_ != ui::AXTreeIDUnknown())
    dst->parent_tree_id = browser_plugin_embedder_ax_tree_id_;

  // If this is not the root frame tree node, we're done.
  if (frame_tree_node()->parent())
    return;

  // For the root frame tree node, also store the AXTreeID of the focused frame.
  auto* focused_frame = static_cast<RenderFrameHostImpl*>(
      delegate_->GetFocusedFrameIncludingInnerWebContents());
  if (!focused_frame)
    return;
  dst->focused_tree_id = focused_frame->GetAXTreeID();
}

WebBluetoothServiceImpl* RenderFrameHostImpl::CreateWebBluetoothService(
    blink::mojom::WebBluetoothServiceRequest request) {
  // RFHI owns |web_bluetooth_services_| and |web_bluetooth_service| owns the
  // |binding_| which may run the error handler. |binding_| can't run the error
  // handler after it's destroyed so it can't run after the RFHI is destroyed.
  auto web_bluetooth_service =
      std::make_unique<WebBluetoothServiceImpl>(this, std::move(request));
  web_bluetooth_service->SetClientConnectionErrorHandler(
      base::BindOnce(&RenderFrameHostImpl::DeleteWebBluetoothService,
                     base::Unretained(this), web_bluetooth_service.get()));
  web_bluetooth_services_.push_back(std::move(web_bluetooth_service));
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
    mojom::RendererAudioInputStreamFactoryRequest request) {
  BrowserMainLoop* browser_main_loop = BrowserMainLoop::GetInstance();
  DCHECK(browser_main_loop);
  if (base::FeatureList::IsEnabled(features::kAudioServiceAudioStreams)) {
    MediaStreamManager* msm = browser_main_loop->media_stream_manager();
    audio_service_audio_input_stream_factory_.emplace(std::move(request), msm,
                                                      this);
  } else {
    in_content_audio_input_stream_factory_ =
        RenderFrameAudioInputStreamFactoryHandle::CreateFactory(
            base::BindRepeating(&AudioInputDelegateImpl::Create,
                                browser_main_loop->audio_manager(),
                                AudioMirroringManager::GetInstance(),
                                browser_main_loop->user_input_monitor(),
                                GetProcess()->GetID(), GetRoutingID()),
            browser_main_loop->media_stream_manager(), GetProcess()->GetID(),
            GetRoutingID(), std::move(request));
  }
}

void RenderFrameHostImpl::CreateAudioOutputStreamFactory(
    mojom::RendererAudioOutputStreamFactoryRequest request) {
  if (base::FeatureList::IsEnabled(features::kAudioServiceAudioStreams)) {
    media::AudioSystem* audio_system =
        BrowserMainLoop::GetInstance()->audio_system();
    MediaStreamManager* media_stream_manager =
        BrowserMainLoop::GetInstance()->media_stream_manager();
    audio_service_audio_output_stream_factory_.emplace(
        this, audio_system, media_stream_manager, std::move(request));
  } else {
    RendererAudioOutputStreamFactoryContext* factory_context =
        GetProcess()->GetRendererAudioOutputStreamFactoryContext();
    DCHECK(factory_context);
    in_content_audio_output_stream_factory_ =
        RenderFrameAudioOutputStreamFactoryHandle::CreateFactory(
            factory_context, GetRoutingID(), std::move(request));
  }
}

void RenderFrameHostImpl::BindMediaInterfaceFactoryRequest(
    media::mojom::InterfaceFactoryRequest request) {
  DCHECK(!media_interface_proxy_);
  media_interface_proxy_.reset(new MediaInterfaceProxy(
      this, std::move(request),
      base::Bind(&RenderFrameHostImpl::OnMediaInterfaceFactoryConnectionError,
                 base::Unretained(this))));
}

void RenderFrameHostImpl::CreateWebSocketConnector(
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebSocketConnectorImpl>(
          GetProcess()->GetID(), routing_id_, last_committed_origin_),
      std::move(receiver));
}

void RenderFrameHostImpl::CreateDedicatedWorkerHostFactory(
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver) {
  content::CreateDedicatedWorkerHostFactory(
      process_->GetID(), /*ancestor_render_frame_id=*/routing_id_,
      /*creator_render_frame_id=*/routing_id_, last_committed_origin_,
      std::move(receiver));
}

void RenderFrameHostImpl::OnMediaInterfaceFactoryConnectionError() {
  DCHECK(media_interface_proxy_);
  media_interface_proxy_.reset();
}

#if defined(OS_ANDROID)
void RenderFrameHostImpl::BindNFCRequest(device::mojom::NFCRequest request) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  if (GetParent()) {
    mojo::ReportBadMessage(
        "WebNFC is only allowed in a top-level browsing context.");
    return;
  }
  if (delegate_)
    delegate_->GetNFC(std::move(request));
}
#endif

#if !defined(OS_ANDROID)
void RenderFrameHostImpl::BindSerialServiceRequest(
    blink::mojom::SerialServiceRequest request) {
  if (!IsFeatureEnabled(blink::mojom::FeaturePolicyFeature::kSerial)) {
    mojo::ReportBadMessage("Feature policy blocks access to Serial.");
    return;
  }

  if (!serial_service_)
    serial_service_ = std::make_unique<SerialService>(this);

  serial_service_->Bind(std::move(request));
}

void RenderFrameHostImpl::BindAuthenticatorRequest(
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver) {
  if (!authenticator_impl_)
    authenticator_impl_.reset(new AuthenticatorImpl(this));

  authenticator_impl_->Bind(std::move(receiver));
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
      ->CreateService(std::move(receiver));
}

void RenderFrameHostImpl::GetSpeechSynthesis(
    mojo::PendingReceiver<blink::mojom::SpeechSynthesis> receiver) {
  if (!speech_synthesis_impl_) {
    speech_synthesis_impl_ = std::make_unique<SpeechSynthesisImpl>(
        GetProcess()->GetBrowserContext());
  }
  speech_synthesis_impl_->AddReceiver(std::move(receiver));
}

blink::mojom::FileChooserPtr RenderFrameHostImpl::BindFileChooserForTesting() {
  blink::mojom::FileChooserPtr chooser;
  FileChooserImpl::Create(this, mojo::MakeRequest(&chooser));
  return chooser;
}

void RenderFrameHostImpl::BindSmsReceiverReceiver(
    mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver) {
  if (GetParent() && !WebContents::FromRenderFrameHost(this)
                          ->GetMainFrame()
                          ->GetLastCommittedOrigin()
                          .IsSameOriginWith(GetLastCommittedOrigin())) {
    mojo::ReportBadMessage("Must have the same origin as the top-level frame.");
    return;
  }
  auto* provider = BrowserMainLoop::GetInstance()->GetSmsProvider();
  SmsService::Create(provider, this, std::move(receiver));
}

void RenderFrameHostImpl::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // Requests are serviced on |document_scoped_interface_provider_binding_|. It
  // is therefore safe to assume that every incoming interface request is coming
  // from the currently active document in the corresponding RenderFrame.
  if (!registry_ ||
      !registry_->TryBindInterface(interface_name, &interface_pipe)) {
    delegate_->OnInterfaceRequest(this, interface_name, &interface_pipe);
    if (interface_pipe.is_valid() &&
        !TryBindFrameInterface(interface_name, &interface_pipe, this)) {
      GetContentClient()->browser()->BindInterfaceRequestFromFrame(
          this, interface_name, std::move(interface_pipe));
    }
  }
}

// This is a test-only interface, not exposed in production.
void RenderFrameHostImpl::GetFrameHostTestInterface(
    mojo::PendingReceiver<blink::mojom::FrameHostTestInterface> receiver) {
  class FrameHostTestInterfaceImpl
      : public blink::mojom::FrameHostTestInterface {
   public:
    void Ping(const GURL& url, const std::string& event) override {}
    void GetName(GetNameCallback callback) override {
      std::move(callback).Run("RenderFrameHostImpl");
    }
  };

  mojo::MakeSelfOwnedReceiver(std::make_unique<FrameHostTestInterfaceImpl>(),
                              std::move(receiver));
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

void RenderFrameHostImpl::GetFileSystemManager(
    mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) {
  // This is safe because file_system_manager_ is deleted on the IO thread
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&FileSystemManagerImpl::BindRequest,
                                base::Unretained(file_system_manager_.get()),
                                std::move(receiver)));
}

void RenderFrameHostImpl::GetAuthenticator(
    mojo::PendingReceiver<blink::mojom::Authenticator> receiver) {
#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebAuth)) {
    BindAuthenticatorRequest(std::move(receiver));
  }
#else
  GetJavaInterfaces()->GetInterface(std::move(receiver));
#endif  // !defined(OS_ANDROID)
}

void RenderFrameHostImpl::GetCredentialManager(
    mojo::PendingReceiver<blink::mojom::CredentialManager> receiver) {
  GetContentClient()->browser()->BindCredentialManagerReceiver(
      this, std::move(receiver));
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
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableWebAuthTestingAPI)) {
      auto* environment_singleton = AuthenticatorEnvironmentImpl::GetInstance();
      environment_singleton->EnableVirtualAuthenticatorFor(frame_tree_node_);
      environment_singleton->AddVirtualAuthenticatorReceiver(
          frame_tree_node_, std::move(receiver));
    }
  }
#endif  // !defined(OS_ANDROID)
}

std::unique_ptr<NavigationRequest>
RenderFrameHostImpl::CreateNavigationRequestForCommit(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document,
    NavigationEntryImpl* entry_for_request) {
  bool is_renderer_initiated =
      entry_for_request ? entry_for_request->is_renderer_initiated() : true;
  return NavigationRequest::CreateForCommit(
      frame_tree_node_, this, entry_for_request, params, is_renderer_initiated,
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

  SimulateBeforeUnloadAck(true /* proceed */);
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
      base::Callback<void(const std::string&, mojo::ScopedMessagePipeHandle)>;

  JavaInterfaceProvider(
      const BindCallback& bind_callback,
      service_manager::mojom::InterfaceProviderRequest request)
      : bind_callback_(bind_callback), binding_(this, std::move(request)) {}
  ~JavaInterfaceProvider() override = default;

 private:
  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle handle) override {
    bind_callback_.Run(interface_name, std::move(handle));
  }

  const BindCallback bind_callback_;
  mojo::Binding<service_manager::mojom::InterfaceProvider> binding_;

  DISALLOW_COPY_AND_ASSIGN(JavaInterfaceProvider);
};

base::android::ScopedJavaLocalRef<jobject>
RenderFrameHostImpl::GetJavaRenderFrameHost() {
  RenderFrameHostAndroid* render_frame_host_android =
      static_cast<RenderFrameHostAndroid*>(
          GetUserData(kRenderFrameHostAndroidKey));
  if (!render_frame_host_android) {
    service_manager::mojom::InterfaceProviderPtr interface_provider_ptr;
    java_interface_registry_ = std::make_unique<JavaInterfaceProvider>(
        base::Bind(&RenderFrameHostImpl::ForwardGetInterfaceToRenderFrame,
                   weak_ptr_factory_.GetWeakPtr()),
        mojo::MakeRequest(&interface_provider_ptr));
    render_frame_host_android =
        new RenderFrameHostAndroid(this, std::move(interface_provider_ptr));
    SetUserData(kRenderFrameHostAndroidKey,
                base::WrapUnique(render_frame_host_android));
  }
  return render_frame_host_android->GetJavaObject();
}

service_manager::InterfaceProvider* RenderFrameHostImpl::GetJavaInterfaces() {
  if (!java_interfaces_) {
    service_manager::mojom::InterfaceProviderPtr provider;
    BindInterfaceRegistryForRenderFrameHost(mojo::MakeRequest(&provider), this);
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
    const base::Callback<void(RenderFrameHostImpl*)>& callback) {
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
  ForEachImmediateLocalRoot(base::Bind(
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
    FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params,
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
      if (!validated_params->url_is_unreachable) {
        DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, validated_params->origin);
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
        navigation_request->navigation_handle()->GetNetErrorCode() ==
            net::ERR_BLOCKED_BY_CLIENT) {
      bypass_checks_for_error_page = true;
    }
  }

  // Error pages must commit in a opaque origin. Terminate the renderer
  // process if this is violated.
  if (bypass_checks_for_error_page && !validated_params->origin.opaque()) {
    DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, validated_params->origin);
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_ERROR_PROCESS_NON_UNIQUE_ORIGIN_COMMIT);
    return false;
  }

  // file: URLs can be allowed to access any other origin, based on settings.
  bool bypass_checks_for_file_scheme = false;
  if (validated_params->origin.scheme() == url::kFileScheme) {
    WebPreferences prefs = render_view_host_->GetWebkitPreferences();
    if (prefs.allow_universal_access_from_file_urls)
      bypass_checks_for_file_scheme = true;
  }

  if (!bypass_checks_for_error_page && !bypass_checks_for_file_scheme) {
    // Attempts to commit certain off-limits URL should be caught more strictly
    // than our FilterURL checks.  If a renderer violates this policy, it
    // should be killed.
    switch (CanCommitOriginAndUrl(validated_params->origin,
                                  validated_params->url)) {
      case CanCommitStatus::CAN_COMMIT_ORIGIN_AND_URL:
        // The origin and URL are safe to commit.
        break;
      case CanCommitStatus::CANNOT_COMMIT_URL:
        VLOG(1) << "Blocked URL " << validated_params->url.spec();
        LogCannotCommitUrlCrashKeys(validated_params->url,
                                    is_same_document_navigation,
                                    navigation_request);

        // Kills the process.
        bad_message::ReceivedBadMessage(
            process, bad_message::RFH_CAN_COMMIT_URL_BLOCKED);
        return false;
      case CanCommitStatus::CANNOT_COMMIT_ORIGIN:
        DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, validated_params->origin);
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
  // requires |validated_params| to be mutable. Once we kill the renderer
  // instead, the signature of RenderFrameHostImpl::DidCommitProvisionalLoad can
  // be modified to take |validated_params| by const reference.
  process->FilterURL(false, &validated_params->url);
  process->FilterURL(true, &validated_params->referrer.url);
  for (auto it(validated_params->redirects.begin());
       it != validated_params->redirects.end(); ++it) {
    process->FilterURL(false, &(*it));
  }

  // Without this check, the renderer can trick the browser into using
  // filenames it can't access in a future session restore.
  if (!CanAccessFilesOfPageState(validated_params->page_state)) {
    bad_message::ReceivedBadMessage(
        process, bad_message::RFH_CAN_ACCESS_FILES_OF_PAGE_STATE);
    return false;
  }

  return true;
}

void RenderFrameHostImpl::UpdateSiteURL(const GURL& url,
                                        bool url_is_unreachable) {
  if (url_is_unreachable || delegate_->GetAsInterstitialPage()) {
    SetLastCommittedSiteUrl(GURL());
  } else {
    SetLastCommittedSiteUrl(url);
  }
}

bool RenderFrameHostImpl::DidCommitNavigationInternal(
    std::unique_ptr<NavigationRequest> navigation_request,
    FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params,
    bool is_same_document_navigation) {
  // Sanity-check the page transition for frame type.
  DCHECK_EQ(ui::PageTransitionIsMainFrame(validated_params->transition),
            !GetParent());

  // Check that the committing navigation token matches the navigation request.
  if (navigation_request &&
      navigation_request->commit_params().navigation_token !=
          validated_params->navigation_token) {
    navigation_request.reset();
  }

  if (!navigation_request) {
    // A matching NavigationRequest should have been found, unless in a few very
    // specific cases. Check if this is one of those cases.
    bool is_commit_allowed_to_proceed = false;

    //  1) This was a renderer-initiated navigation to an empty document. Most
    //  of the time: about:blank.
    is_commit_allowed_to_proceed |=
        validated_params->url.SchemeIs(url::kAboutScheme) &&
        validated_params->url != GURL(url::kAboutSrcdocURL);

    //  2) This was a same-document navigation.
    //  TODO(clamy): We should enforce having a request on browser-initiated
    //  same-document navigations.
    is_commit_allowed_to_proceed |= is_same_document_navigation;

    //  3) Transient interstitial page commits will not have a matching
    //  NavigationRequest.
    //  TODO(clamy): Enforce having a NavigationRequest for data URLs when
    //  committed interstitials have launched or interstitials create
    //  NavigationRequests.
    is_commit_allowed_to_proceed |= !!delegate_->GetAsInterstitialPage();

    //  4) Error pages implementations in Chrome can commit twice.
    //  TODO(clamy): Fix this.
    is_commit_allowed_to_proceed |= validated_params->url_is_unreachable;

    //  5) Special case for DOMSerializerBrowsertests which are implemented
    //  entirely renderer-side and unlike normal RenderView based tests load
    //  file URLs instead of data URLs.
    //  TODO(clamy): Rework the tests to remove this exception.
    is_commit_allowed_to_proceed |= validated_params->url.SchemeIsFile();

    if (!is_commit_allowed_to_proceed) {
      bad_message::ReceivedBadMessage(
          GetProcess(),
          bad_message::RFH_NO_MATCHING_NAVIGATION_REQUEST_ON_COMMIT);
      return false;
    }
  }

  if (!ValidateDidCommitParams(navigation_request.get(), validated_params,
                               is_same_document_navigation)) {
    return false;
  }

  // TODO(clamy): We should stop having a special case for same-document
  // navigation and just put them in the general map of NavigationRequests.
  if (navigation_request &&
      navigation_request->common_params().url != validated_params->url &&
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
    navigation_request = CreateNavigationRequestForCommit(
        *validated_params, is_same_document_navigation, nullptr);
  }

  DCHECK(navigation_request);
  DCHECK(navigation_request->navigation_handle());

  // Update the page transition. For subframe navigations, the renderer process
  // only gives the correct page transition at commit time.
  // TODO(clamy): We should get the correct page transition when starting the
  // request.
  navigation_request->set_transition(validated_params->transition);

  navigation_request->set_has_user_gesture(validated_params->gesture ==
                                           NavigationGestureUser);

  last_http_status_code_ = validated_params->http_status_code;
  UpdateSiteURL(validated_params->url, validated_params->url_is_unreachable);

  // Set the state whether this navigation is to an MHTML document, since there
  // are certain security checks that we cannot apply to subframes in MHTML
  // documents. Do not trust renderer data when determining that, rather use
  // the |navigation_request|, which was generated and stays browser side.
  is_mhtml_document_ =
      (navigation_request->GetMimeType() == "multipart/related" ||
       navigation_request->GetMimeType() == "message/rfc822");

  accessibility_reset_count_ = 0;
  appcache_handle_ = navigation_request->TakeAppCacheHandle();
  frame_tree_node()->navigator()->DidNavigate(this, *validated_params,
                                              std::move(navigation_request),
                                              is_same_document_navigation);

  if (!is_same_document_navigation) {
    cookie_no_samesite_deprecation_url_hashes_.clear();
    cookie_samesite_none_insecure_deprecation_url_hashes_.clear();
    renderer_reported_scheduler_tracked_features_ = 0;
    browser_reported_scheduler_tracked_features_ = 0;
  }

  return true;
}

void RenderFrameHostImpl::OnSameDocumentCommitProcessed(
    int64_t navigation_id,
    bool should_replace_current_entry,
    blink::mojom::CommitResult result) {
  // If the NavigationRequest was deleted, another navigation commit started to
  // be processed. Let the latest commit go through and stop doing anything.
  if (!same_document_navigation_request_ ||
      same_document_navigation_request_->navigation_handle()
              ->GetNavigationId() != navigation_id) {
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
    FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params) const {
  auto value = std::make_unique<base::trace_event::TracedValue>();

  value->SetInteger("frame_tree_node", frame_tree_node_->frame_tree_node_id());
  value->SetInteger("site id", site_instance_->GetId());
  value->SetString("process lock", ChildProcessSecurityPolicyImpl::GetInstance()
                                       ->GetOriginLock(process_->GetID())
                                       .spec());
  value->SetString("origin", validated_params->origin.Serialize());
  value->SetInteger("transition", validated_params->transition);

  if (!validated_params->base_url.is_empty()) {
    value->SetString("base_url",
                     validated_params->base_url.possibly_invalid_spec());
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
    const network::ResourceResponseHead& response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    base::Optional<std::vector<::content::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory,
    const base::UnguessableToken& devtools_navigation_token) {
  if (navigation_client) {
    navigation_client->CommitNavigation(
        std::move(common_params), std::move(commit_params), response_head,
        std::move(response_body), std::move(url_loader_client_endpoints),
        std::move(subresource_loader_factories),
        std::move(subresource_overrides), std::move(controller),
        std::move(provider_info), std::move(prefetch_loader_factory),
        devtools_navigation_token,
        BuildNavigationClientCommitNavigationCallback(navigation_request));
  } else {
    GetNavigationControl()->CommitNavigation(
        std::move(common_params), std::move(commit_params), response_head,
        std::move(response_body), std::move(url_loader_client_endpoints),
        std::move(subresource_loader_factories),
        std::move(subresource_overrides), std::move(controller),
        std::move(provider_info), std::move(prefetch_loader_factory),
        devtools_navigation_token,
        BuildCommitNavigationCallback(navigation_request));
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
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories) {
  if (navigation_client) {
    navigation_client->CommitFailedNavigation(
        std::move(common_params), std::move(commit_params),
        has_stale_copy_in_cache, error_code, error_page_content,
        std::move(subresource_loader_factories),
        BuildNavigationClientCommitFailedNavigationCallback(
            navigation_request));
  } else {
    GetNavigationControl()->CommitFailedNavigation(
        std::move(common_params), std::move(commit_params),
        has_stale_copy_in_cache, error_code, error_page_content,
        std::move(subresource_loader_factories),
        BuildCommitFailedNavigationCallback(navigation_request));
  }
}

// Called when the renderer navigates. For every frame loaded, we'll get this
// notification containing parameters identifying the navigation.
void RenderFrameHostImpl::DidCommitNavigation(
    std::unique_ptr<NavigationRequest> committing_navigation_request,
    std::unique_ptr<FrameHostMsg_DidCommitProvisionalLoad_Params>
        validated_params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  NavigationHandleImpl* navigation_handle;
  if (committing_navigation_request) {
    navigation_handle = committing_navigation_request->navigation_handle();
  } else {
    navigation_handle = GetNavigationHandle();
  }

  if (navigation_handle) {
    main_frame_request_ids_ = {validated_params->request_id,
                               navigation_handle->GetGlobalRequestID()};
    if (deferred_main_frame_load_info_)
      ResourceLoadComplete(std::move(deferred_main_frame_load_info_));
  }
  // DidCommitProvisionalLoad IPC should be associated with the URL being
  // committed (not with the *last* committed URL that most other IPCs are
  // associated with).
  ScopedActiveURL scoped_active_url(
      validated_params->url,
      frame_tree_node()->frame_tree()->root()->current_origin());

  ScopedCommitStateResetter commit_state_resetter(this);
  RenderProcessHost* process = GetProcess();

  TRACE_EVENT2("navigation", "RenderFrameHostImpl::DidCommitProvisionalLoad",
               "url", validated_params->url.possibly_invalid_spec(), "details",
               CommitAsTracedValue(validated_params.get()));

  // If we're waiting for a cross-site beforeunload ack from this renderer and
  // we receive a Navigate message from the main frame, then the renderer was
  // navigating already and sent it before hearing the FrameMsg_Stop message.
  // Treat this as an implicit beforeunload ack to allow the pending navigation
  // to continue.
  if (is_waiting_for_beforeunload_ack_ && unload_ack_is_for_navigation_ &&
      !GetParent()) {
    base::TimeTicks approx_renderer_start_time = send_before_unload_start_time_;
    ProcessBeforeUnloadACK(true /* proceed */, true /* treat_as_final_ack */,
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
  DCHECK(document_scoped_interface_provider_binding_.is_bound());
  if (interface_params) {
    // As a general rule, expect the RenderFrame to have supplied the
    // request end of a new InterfaceProvider connection that will be used by
    // the new document to issue interface requests to access RenderFrameHost
    // services.
    auto interface_provider_request_of_previous_document =
        document_scoped_interface_provider_binding_.Unbind();
    dropped_interface_request_logger_ =
        std::make_unique<DroppedInterfaceRequestLogger>(
            std::move(interface_provider_request_of_previous_document));
    BindInterfaceProviderRequest(
        std::move(interface_params->interface_provider_request));

    document_interface_broker_content_receiver_.reset();
    document_interface_broker_blink_receiver_.reset();
    broker_receiver_.reset();
    BindDocumentInterfaceBrokerReceiver(
        std::move(interface_params->document_interface_broker_content_receiver),
        std::move(interface_params->document_interface_broker_blink_receiver));
    BindBrowserInterfaceBrokerReceiver(
        std::move(interface_params->browser_interface_broker_receiver));
  } else {
    // If there had already been a real load committed in the frame, and this is
    // not a same-document navigation, then both the active document as well as
    // the global object was replaced in this browsing context. The RenderFrame
    // should have rebound its InterfaceProvider to a new pipe, but failed to do
    // so. Kill the renderer, and close the old binding to ensure that any
    // pending interface requests originating from the previous document, hence
    // possibly from a different security origin, will no longer be dispatched.
    if (frame_tree_node_->has_committed_real_load()) {
      document_scoped_interface_provider_binding_.Close();
      document_interface_broker_content_receiver_.reset();
      document_interface_broker_blink_receiver_.reset();
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
                                   validated_params.get(),
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

mojom::FrameNavigationControl::CommitNavigationCallback
RenderFrameHostImpl::BuildCommitNavigationCallback(
    NavigationRequest* navigation_request) {
  if (!navigation_request)
    return content::mojom::FrameNavigationControl::CommitNavigationCallback();

  return base::BindOnce(&RenderFrameHostImpl::OnCrossDocumentCommitProcessed,
                        base::Unretained(this), navigation_request);
}

mojom::FrameNavigationControl::CommitFailedNavigationCallback
RenderFrameHostImpl::BuildCommitFailedNavigationCallback(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  return base::BindOnce(&RenderFrameHostImpl::OnCrossDocumentCommitProcessed,
                        base::Unretained(this), navigation_request);
}

mojom::NavigationClient::CommitNavigationCallback
RenderFrameHostImpl::BuildNavigationClientCommitNavigationCallback(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  return base::BindOnce(
      &RenderFrameHostImpl::DidCommitPerNavigationMojoInterfaceNavigation,
      base::Unretained(this), navigation_request);
}

mojom::NavigationClient::CommitFailedNavigationCallback
RenderFrameHostImpl::BuildNavigationClientCommitFailedNavigationCallback(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  return base::BindOnce(
      &RenderFrameHostImpl::DidCommitPerNavigationMojoInterfaceNavigation,
      base::Unretained(this), navigation_request);
}

void RenderFrameHostImpl::UpdateFrameFrozenState() {
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
    FrameHostMsg_DidCommitProvisionalLoad_Params* validated_params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params) {
  if (commit_callback_interceptor_) {
    return commit_callback_interceptor_->WillProcessDidCommitNavigation(
        navigation_request, validated_params, interface_params);
  }
  return true;
}

void RenderFrameHostImpl::PostMessageEvent(int32_t source_routing_id,
                                           const base::string16& source_origin,
                                           const base::string16& target_origin,
                                           blink::TransferableMessage message) {
  DCHECK(render_frame_created_);

  GetNavigationControl()->PostMessageEvent(source_routing_id, source_origin,
                                           target_origin, std::move(message));
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
      if (child->unload_state_ == UnloadState::NotRun)
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
  Send(new FrameMsg_AddMessageToConsole(routing_id_, level, message,
                                        discard_duplicates));
}

void RenderFrameHostImpl::AddSameSiteCookieDeprecationMessage(
    const std::string& cookie_url,
    net::CanonicalCookie::CookieInclusionStatus::WarningReason warning) {
  std::string deprecation_message;
  if (warning == net::CanonicalCookie::CookieInclusionStatus::WarningReason::
                     WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT) {
    if (!ShouldAddCookieSameSiteDeprecationMessage(
            cookie_url, &cookie_no_samesite_deprecation_url_hashes_)) {
      return;
    }
    deprecation_message =
        "A cookie associated with a cross-site resource at " + cookie_url +
        " was set without the `SameSite` attribute. "
        "A future release of Chrome will only deliver cookies with "
        "cross-site requests if they are set with `SameSite=None` and "
        "`Secure`. You can review cookies in developer tools under "
        "Application>Storage>Cookies and see more details at "
        "https://www.chromestatus.com/feature/5088147346030592 and "
        "https://www.chromestatus.com/feature/5633521622188032.";
  } else if (warning == net::CanonicalCookie::CookieInclusionStatus::
                            WarningReason::WARN_SAMESITE_NONE_INSECURE) {
    if (!ShouldAddCookieSameSiteDeprecationMessage(
            cookie_url,
            &cookie_samesite_none_insecure_deprecation_url_hashes_)) {
      return;
    }
    deprecation_message =
        "A cookie associated with a resource at " + cookie_url +
        " was set with `SameSite=None` but without `Secure`. "
        "A future release of Chrome will only deliver cookies marked "
        "`SameSite=None` if they are also marked `Secure`. You "
        "can review cookies in developer tools under "
        "Application>Storage>Cookies and see more details at "
        "https://www.chromestatus.com/feature/5633521622188032.";
  } else if (warning ==
             net::CanonicalCookie::CookieInclusionStatus::WarningReason::
                 WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE) {
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
      GetSiteInstance()->lock_url().spec());

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("original_url_origin",
                                          base::debug::CrashKeySize::Size256),
      GetSiteInstance()->original_url().GetOrigin().spec());

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("is_transfer_needed",
                                          base::debug::CrashKeySize::Size32),
      bool_to_crash_key(
          frame_tree_node_->render_manager()
              ->IsRendererTransferNeededForNavigation(this, url)));

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

  if (navigation_request && navigation_request->navigation_handle()) {
    NavigationHandleImpl* handle = navigation_request->navigation_handle();
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_renderer_initiated",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(handle->IsRendererInitiated()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_server_redirect",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(handle->WasServerRedirect()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_form_submission",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(handle->IsFormSubmission()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_error_page",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(handle->IsErrorPage()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("from_begin_navigation",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(navigation_request->from_begin_navigation()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("net_error",
                                            base::debug::CrashKeySize::Size32),
        base::NumberToString(navigation_request->net_error()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("initiator_origin",
                                            base::debug::CrashKeySize::Size64),
        handle->GetInitiatorOrigin()
            ? handle->GetInitiatorOrigin()->GetDebugString()
            : "none");

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("starting_site_instance",
                                            base::debug::CrashKeySize::Size64),
        handle->GetStartingSiteInstance()->GetSiteURL().spec());

    // Recompute the target SiteInstance to see if it matches the current
    // one at commit time.
    scoped_refptr<SiteInstance> dest_instance =
        frame_tree_node_->render_manager()->GetSiteInstanceForNavigationRequest(
            *navigation_request);
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString(
            "does_recomputed_site_instance_match",
            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(dest_instance == GetSiteInstance()));
  }
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

  if (navigation_request && navigation_request->navigation_handle()) {
    NavigationHandleImpl* handle = navigation_request->navigation_handle();
    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_renderer_initiated",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(handle->IsRendererInitiated()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_server_redirect",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(handle->WasServerRedirect()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_form_submission",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(handle->IsFormSubmission()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("is_error_page",
                                            base::debug::CrashKeySize::Size32),
        bool_to_crash_key(handle->IsErrorPage()));

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("initiator_origin",
                                            base::debug::CrashKeySize::Size64),
        handle->GetInitiatorOrigin()
            ? handle->GetInitiatorOrigin()->GetDebugString()
            : "none");

    base::debug::SetCrashKeyString(
        base::debug::AllocateCrashKeyString("starting_site_instance",
                                            base::debug::CrashKeySize::Size64),
        handle->GetStartingSiteInstance()->GetSiteURL().spec());
  }
}

}  // namespace content
