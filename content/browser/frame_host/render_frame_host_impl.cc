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
#include "content/browser/background_fetch/background_fetch_service_impl.h"
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
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/log_console_message.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_interface_proxy.h"
#include "content/browser/media/session/media_session_service_impl.h"
#include "content/browser/media/webaudio/audio_context_manager_impl.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/portal/portal.h"
#include "content/browser/presentation/presentation_service_impl.h"
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
#include "content/browser/speech/speech_recognition_dispatcher_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/wake_lock/wake_lock_service_impl.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/browser/webauth/scoped_virtual_authenticator_environment.h"
#include "content/browser/websockets/websocket_manager.h"
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
#include "content/common/navigation_subresource_loader_params.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/common/swapped_out_messages.h"
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
#include "content/public/browser/stream_handle.h"
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
#include "media/mojo/interfaces/remoting.mojom.h"
#include "media/mojo/services/media_interface_provider.h"
#include "media/mojo/services/media_metrics_provider.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/url_request/url_request_context.h"
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
#include "third_party/blink/public/mojom/frame/frame_host_test_interface.mojom.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"
#include "third_party/blink/public/mojom/loader/url_loader_factory_bundle.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
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

#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
// Whether to allow injecting javascript into any kind of frame, for Android
// WebView and Fuchsia web.ContextProvider.
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

using FrameNotifyCallback =
    base::RepeatingCallback<void(ResourceDispatcherHostImpl*,
                                 const GlobalFrameRoutingId&)>;

// The following functions simplify code paths where the UI thread notifies the
// ResourceDispatcherHostImpl of information pertaining to loading behavior of
// frame hosts.
void NotifyRouteChangesOnIO(
    const FrameNotifyCallback& frame_callback,
    std::unique_ptr<std::set<GlobalFrameRoutingId>> routing_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  if (!rdh)
    return;
  for (const auto& routing_id : *routing_ids)
    frame_callback.Run(rdh, routing_id);
}

void NotifyForEachFrameFromUI(RenderFrameHostImpl* root_frame_host,
                              const FrameNotifyCallback& frame_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  FrameTree* frame_tree = root_frame_host->frame_tree_node()->frame_tree();
  DCHECK_EQ(root_frame_host, frame_tree->GetMainFrame());

  auto routing_ids = std::make_unique<std::set<GlobalFrameRoutingId>>();
  for (FrameTreeNode* node : frame_tree->Nodes()) {
    RenderFrameHostImpl* frame_host = node->current_frame_host();
    RenderFrameHostImpl* pending_frame_host =
        node->render_manager()->speculative_frame_host();
    if (frame_host)
      routing_ids->insert(frame_host->GetGlobalFrameRoutingId());
    if (pending_frame_host)
      routing_ids->insert(pending_frame_host->GetGlobalFrameRoutingId());
  }
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&NotifyRouteChangesOnIO, frame_callback,
                     std::move(routing_ids)));
}

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

base::Optional<url::Origin> GetOriginForURLLoaderFactory(
    GURL target_url,
    SiteInstanceImpl* site_instance) {
  // TODO(lukasza, nasko): https://crbug.com/888079: Use exact origin, instead
  // of falling back to site URL for about:blank and about:srcdoc.
  if (target_url.SchemeIs(url::kAboutScheme)) {
    // |site_instance|'s site URL cannot be used as
    // |request_initiator_site_lock| unless the site requires a dedicated
    // process.  Otherwise b.com may share a process associated with a.com, in
    // a SiteInstance with |site_url| set to "http://a.com" (and/or
    // "http://nonisolated.invalid" in the future) and in that scenario
    // |request_initiator| for requests from b.com should NOT be locked to
    // a.com.
    if (!SiteInstanceImpl::ShouldLockToOrigin(
            site_instance->GetIsolationContext(), site_instance->GetSiteURL()))
      return base::nullopt;

    return SiteInstanceImpl::GetRequestInitiatorSiteLock(
        site_instance->GetSiteURL());
  }

  // In cases not covered above, URLLoaderFactory should be associated with the
  // origin of |target_url|.  This works fine for all URLs, including data: URLs
  // (which should use an opaque origin for their subresource requests) and
  // blob: URLs (which embed their origin inside the |target_url|).
  return url::Origin::Create(target_url);
}

std::unique_ptr<blink::URLLoaderFactoryBundleInfo> CloneFactoryBundle(
    scoped_refptr<blink::URLLoaderFactoryBundle> bundle) {
  return base::WrapUnique(static_cast<blink::URLLoaderFactoryBundleInfo*>(
      bundle->Clone().release()));
}

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
  CommonNavigationParams common_params;
  mojom::BeginNavigationParamsPtr begin_navigation_params;
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  mojom::NavigationClientAssociatedPtrInfo navigation_client;
  blink::mojom::NavigationInitiatorPtr navigation_initiator;

  PendingNavigation(
      CommonNavigationParams common_params,
      mojom::BeginNavigationParamsPtr begin_navigation_params,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojom::NavigationClientAssociatedPtrInfo navigation_client,
      blink::mojom::NavigationInitiatorPtr navigation_initiator);
};

PendingNavigation::PendingNavigation(
    CommonNavigationParams common_params,
    mojom::BeginNavigationParamsPtr begin_navigation_params,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojom::NavigationClientAssociatedPtrInfo navigation_client,
    blink::mojom::NavigationInitiatorPtr navigation_initiator)
    : common_params(common_params),
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

#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
// static
void RenderFrameHost::AllowInjectingJavaScript() {
  g_allow_injecting_javascript = true;
}
#endif  // defined(OS_ANDROID) || defined(OS_FUCHSIA)

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

RenderFrameHostImpl::RenderFrameHostImpl(SiteInstance* site_instance,
                                         RenderViewHostImpl* render_view_host,
                                         RenderFrameHostDelegate* delegate,
                                         FrameTree* frame_tree,
                                         FrameTreeNode* frame_tree_node,
                                         int32_t routing_id,
                                         int32_t widget_routing_id,
                                         bool hidden,
                                         bool renderer_initiated_creation)
    : render_view_host_(render_view_host),
      delegate_(delegate),
      site_instance_(static_cast<SiteInstanceImpl*>(site_instance)),
      process_(site_instance->GetProcess()),
      frame_tree_(frame_tree),
      frame_tree_node_(frame_tree_node),
      parent_(nullptr),
      render_widget_host_(nullptr),
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
      active_sandbox_flags_(blink::WebSandboxFlags::kNone),
      document_scoped_interface_provider_binding_(this),
      document_interface_broker_content_binding_(this),
      document_interface_broker_blink_binding_(this),
      keep_alive_timeout_(base::TimeDelta::FromSeconds(30)),
      subframe_unload_timeout_(base::TimeDelta::FromMilliseconds(
          RenderViewHostImpl::kUnloadTimeoutMS)),
      commit_callback_interceptor_(nullptr),
      weak_ptr_factory_(this) {
  frame_tree_->AddRenderViewHostRef(render_view_host_);
  GetProcess()->AddRoute(routing_id_, this);
  g_routing_id_frame_map.Get().emplace(
      RenderFrameHostID(GetProcess()->GetID(), routing_id_), this);
  site_instance_->AddObserver(this);
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

    // TODO(avi): Once RenderViewHostImpl has-a RenderWidgetHostImpl, the main
    // render frame should probably start owning the RenderWidgetHostImpl,
    // so this logic checking for an already existing RWHI should be removed.
    // https://crbug.com/545684
    render_widget_host_ =
        RenderWidgetHostImpl::FromID(GetProcess()->GetID(), widget_routing_id);

    mojom::WidgetInputHandlerAssociatedPtr widget_handler;
    mojom::WidgetInputHandlerHostRequest host_request;
    if (frame_input_handler_) {
      mojom::WidgetInputHandlerHostPtr host;
      host_request = mojo::MakeRequest(&host);
      frame_input_handler_->GetWidgetInputHandler(
          mojo::MakeRequest(&widget_handler), std::move(host));
    }
    if (!render_widget_host_) {
      DCHECK(frame_tree_node->parent());
      render_widget_host_ = RenderWidgetHostFactory::Create(
          frame_tree_->render_widget_delegate(), GetProcess(),
          widget_routing_id, std::move(widget), hidden);
      render_widget_host_->set_owned_by_render_frame_host(true);
    } else {
      DCHECK(!render_widget_host_->owned_by_render_frame_host());
      render_widget_host_->SetWidget(std::move(widget));
    }
    if (!frame_tree_node_->parent())
      render_widget_host_->SetIntersectsViewport(true);
    render_widget_host_->SetFrameDepth(frame_tree_node_->depth());
    render_widget_host_->SetWidgetInputHandler(std::move(widget_handler),
                                               std::move(host_request));
    render_widget_host_->input_router()->SetFrameTreeNodeId(
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
  //    FrameMsg_SwapOut for the RenderFrame to replace itself with a
  //    RenderFrameProxy and release its associated resources. |unload_state_|
  //    is advanced to UnloadState::InProgress to track that this IPC is in
  //    flight.
  // 3. The RenderFrame can be detached, as part of removing a subtree (due to
  //    navigation, swap out, or DOM mutation). In this case, the browser sends
  //    a FrameMsg_Delete for the RenderFrame to detach itself and release its
  //    associated resources. If the subframe contains an unload handler,
  //    |unload_state_| is advanced to UnloadState::InProgress to track that the
  //    detach is in progress; otherwise, it is advanced directly to
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
  // bottom up, so the replicated FrameMsg_Delete messages actually operate on a
  // node-by-node basis rather than detaching an entire subtree at once...
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

  if (render_widget_host_ &&
      render_widget_host_->owned_by_render_frame_host()) {
    // Shutdown causes the RenderWidgetHost to delete itself.
    render_widget_host_->ShutdownAndDestroyWidget(true);
  }

  // Notify the FrameTree that this RFH is going away, allowing it to shut down
  // the corresponding RenderViewHost if it is no longer needed.
  frame_tree_->ReleaseRenderViewHostRef(render_view_host_);

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

void RenderFrameHostImpl::OnPortalActivated(
    const base::UnguessableToken& portal_token,
    blink::mojom::PortalAssociatedPtrInfo portal,
    blink::TransferableMessage data,
    base::OnceCallback<void(bool)> callback) {
  GetNavigationControl()->OnPortalActivated(
      portal_token, std::move(portal), std::move(data), std::move(callback));
}

void RenderFrameHostImpl::ForwardMessageToPortalHost(
    blink::TransferableMessage message,
    const url::Origin& source_origin,
    const base::Optional<url::Origin>& target_origin) {
  // The target origin check needs to be done here in case the frame has
  // navigated after the postMessage call, or if the renderer is compromised and
  // the check done in PortalHost::ReceiveMessage is bypassed.
  if (target_origin) {
    DCHECK(!target_origin->opaque());
    if (target_origin != GetLastCommittedOrigin())
      return;
  }
  GetNavigationControl()->ForwardMessageToPortalHost(
      std::move(message), source_origin, target_origin);
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

blink::mojom::PauseSubresourceLoadingHandlePtr
RenderFrameHostImpl::PauseSubresourceLoading() {
  DCHECK(frame_);
  blink::mojom::PauseSubresourceLoadingHandlePtr
      pause_subresource_loading_handle;
  GetRemoteInterfaces()->GetInterface(&pause_subresource_loading_handle);

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
      last_committed_origin_, std::move(default_factory_request));
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
    subresource_loader_factories->initiator_specific_factory_infos() =
        CreateInitiatorSpecificURLLoaderFactories(request_initiators);
    GetNavigationControl()->UpdateSubresourceLoaderFactories(
        std::move(subresource_loader_factories));
  }
}

bool RenderFrameHostImpl::IsSandboxed(blink::WebSandboxFlags flags) {
  return static_cast<int>(active_sandbox_flags_) & static_cast<int>(flags);
}

blink::URLLoaderFactoryBundleInfo::OriginMap
RenderFrameHostImpl::CreateInitiatorSpecificURLLoaderFactories(
    const base::flat_set<url::Origin>& initiator_origins) {
  blink::URLLoaderFactoryBundleInfo::OriginMap result;
  for (const url::Origin& initiator : initiator_origins) {
    network::mojom::URLLoaderFactoryPtrInfo factory_info;
    CreateNetworkServiceDefaultFactoryAndObserve(
        initiator, mojo::MakeRequest(&factory_info));
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
  Send(new FrameMsg_AddMessageToConsole(routing_id_, level, message));
}

void RenderFrameHostImpl::ExecuteJavaScript(const base::string16& javascript,
                                            JavaScriptResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(CanExecuteJavaScript());

  GetNavigationControl()->JavaScriptExecuteRequest(javascript,
                                                   std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld(
    const base::string16& javascript,
    JavaScriptResultCallback callback,
    int world_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(world_id, ISOLATED_WORLD_ID_GLOBAL);
  DCHECK_LE(world_id, ISOLATED_WORLD_ID_MAX);

  GetNavigationControl()->JavaScriptExecuteRequestInIsolatedWorld(
      javascript, world_id, std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptForTests(
    const base::string16& javascript,
    JavaScriptResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const bool has_user_gesture = false;
  GetNavigationControl()->JavaScriptExecuteRequestForTests(
      javascript, has_user_gesture, std::move(callback));
}

void RenderFrameHostImpl::ExecuteJavaScriptWithUserGestureForTests(
    const base::string16& javascript) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const bool has_user_gesture = true;
  GetNavigationControl()->JavaScriptExecuteRequestForTests(
      javascript, has_user_gesture, base::NullCallback());
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
  return render_view_host_;
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
    if (frame->render_widget_host_)
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

  // This message map is for handling internal IPC messages which should not
  // be dispatched to other objects.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameHostImpl, msg)
    // This message is synthetic and doesn't come from RenderFrame, but from
    // RenderProcessHost.
    IPC_MESSAGE_HANDLER(FrameHostMsg_RenderProcessGone, OnRenderProcessGone)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Internal IPCs should not be leaked outside of this object, so return
  // early.
  if (handled)
    return true;

  if (delegate_->OnMessageReceived(this, msg))
    return true;

  RenderFrameProxyHost* proxy =
      frame_tree_node_->render_manager()->GetProxyToParent();
  if (proxy && proxy->cross_process_frame_connector() &&
      proxy->cross_process_frame_connector()->OnMessageReceived(msg))
    return true;

  handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameHostImpl, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidAddMessageToConsole,
                        OnDidAddMessageToConsole)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(FrameHostMsg_FrameFocused, OnFrameFocused)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidFailProvisionalLoadWithError,
                        OnDidFailProvisionalLoadWithError)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidFailLoadWithError,
                        OnDidFailLoadWithError)
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
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidBlockFramebust, OnDidBlockFramebust)
    IPC_MESSAGE_HANDLER(FrameHostMsg_AbortNavigation, OnAbortNavigation)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DispatchLoad, OnDispatchLoad)
    IPC_MESSAGE_HANDLER(FrameHostMsg_ForwardResourceTimingToParent,
                        OnForwardResourceTimingToParent)
    IPC_MESSAGE_HANDLER(FrameHostMsg_TextSurroundingSelectionResponse,
                        OnTextSurroundingSelectionResponse)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_EventBundle, OnAccessibilityEvents)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_LocationChanges,
                        OnAccessibilityLocationChanges)
    IPC_MESSAGE_HANDLER(AccessibilityHostMsg_FindInPageResult,
                        OnAccessibilityFindInPageResult)
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
    IPC_MESSAGE_HANDLER(FrameHostMsg_ShowCreatedWindow, OnShowCreatedWindow)
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

bool RenderFrameHostImpl::AccessibilityIsMainFrame() {
  return frame_tree_node()->IsMainFrame();
}

void RenderFrameHostImpl::RenderProcessGone(SiteInstanceImpl* site_instance) {
  DCHECK_EQ(site_instance_.get(), site_instance);

  // The renderer process is gone, so this frame can no longer be loading.
  if (GetNavigationHandle())
    GetNavigationHandle()->set_net_error_code(net::ERR_ABORTED);
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
  return base::ContainsValue(bypassing_schemes, scheme);
}

mojom::FrameInputHandler* RenderFrameHostImpl::GetFrameInputHandler() {
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

  blink::mojom::DocumentInterfaceBrokerPtrInfo
      document_interface_broker_content_info;
  blink::mojom::DocumentInterfaceBrokerPtrInfo
      document_interface_broker_blink_info;
  BindDocumentInterfaceBrokerRequest(
      mojo::MakeRequest(&document_interface_broker_content_info),
      mojo::MakeRequest(&document_interface_broker_blink_info));

  mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
  params->interface_bundle = mojom::DocumentScopedInterfaceBundle::New(
      interface_provider.PassInterface(),
      std::move(document_interface_broker_content_info),
      std::move(document_interface_broker_blink_info));

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
  if (render_widget_host_) {
    params->widget_params->routing_id = render_widget_host_->GetRoutingID();
    params->widget_params->hidden = render_widget_host_->is_hidden();
  } else {
    // MSG_ROUTING_NONE will prevent a new RenderWidget from being created in
    // the renderer process.
    params->widget_params->routing_id = MSG_ROUTING_NONE;
    params->widget_params->hidden = true;
  }

  GetProcess()->GetRendererInterface()->CreateFrame(std::move(params));

  // The RenderWidgetHost takes ownership of its view. It is tied to the
  // lifetime of the current RenderProcessHost for this RenderFrameHost.
  // TODO(avi): This will need to change to initialize a
  // RenderWidgetHostViewAura for the main frame once RenderViewHostImpl has-a
  // RenderWidgetHostImpl. https://crbug.com/545684
  if (parent_routing_id != MSG_ROUTING_NONE && render_widget_host_) {
    RenderWidgetHostView* rwhv =
        RenderWidgetHostViewChildFrame::Create(render_widget_host_);
    rwhv->Hide();
  }

  if (previous_routing_id != MSG_ROUTING_NONE) {
    RenderFrameProxyHost* proxy = RenderFrameProxyHost::FromID(
        GetProcess()->GetID(), previous_routing_id);
    // We have also created a RenderFrameProxy in CreateFrame above, so
    // remember that.
    proxy->set_render_frame_proxy_created(true);
  }

  // The renderer now has a RenderFrame for this RenderFrameHost.  Note that
  // this path is only used for out-of-process iframes.  Main frame RenderFrames
  // are created with their RenderView, and same-site iframes are created at the
  // time of OnCreateChildFrame.
  SetRenderFrameCreated(true);

  return true;
}

void RenderFrameHostImpl::DeleteRenderFrame() {
  if (!is_active())
    return;

  if (render_frame_created_) {
    Send(new FrameMsg_Delete(routing_id_));

    // If this subframe has an unload handler (and isn't speculative), ensure
    // that it has a chance to execute by delaying process cleanup. This will
    // prevent the process from shutting down immediately in the case where this
    // is the last active frame in the process. See https://crbug.com/852204.
    if (!frame_tree_node_->IsMainFrame() && IsCurrent() &&
        GetSuddenTerminationDisablerState(blink::kUnloadHandler)) {
      RenderProcessHostImpl* process =
          static_cast<RenderProcessHostImpl*>(GetProcess());
      process->DelayProcessShutdownForUnload(subframe_unload_timeout_);
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

  if (created && render_widget_host_) {
    mojom::WidgetPtr widget;
    GetRemoteInterfaces()->GetInterface(&widget);
    render_widget_host_->SetWidget(std::move(widget));

    if (frame_input_handler_) {
      mojom::WidgetInputHandlerAssociatedPtr widget_handler;
      mojom::WidgetInputHandlerHostPtr host;
      mojom::WidgetInputHandlerHostRequest host_request =
          mojo::MakeRequest(&host);
      frame_input_handler_->GetWidgetInputHandler(
          mojo::MakeRequest(&widget_handler), std::move(host));
      render_widget_host_->SetWidgetInputHandler(std::move(widget_handler),
                                                 std::move(host_request));
    }
    render_widget_host_->input_router()->SetFrameTreeNodeId(
        frame_tree_node_->frame_tree_node_id());
    viz::mojom::InputTargetClientPtr input_target_client;
    remote_interfaces_->GetInterface(&input_target_client);
    input_target_client_ = input_target_client.get();
    render_widget_host_->SetInputTargetClient(std::move(input_target_client));
    render_widget_host_->InitForFrame();
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
        frame_tree_node(), pending_navigate_->common_params,
        std::move(pending_navigate_->begin_navigation_params),
        std::move(pending_navigate_->blob_url_loader_factory),
        std::move(pending_navigate_->navigation_client),
        std::move(pending_navigate_->navigation_initiator));
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

void RenderFrameHostImpl::OnDidAddMessageToConsole(
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  if (level < logging::LOG_VERBOSE || level > logging::LOG_FATAL) {
    bad_message::ReceivedBadMessage(
        GetProcess(), bad_message::RFH_DID_ADD_CONSOLE_MESSAGE_BAD_SEVERITY);
    return;
  }

  if (delegate_->DidAddMessageToConsole(level, message, line_no, source_id))
    return;

  // Pass through log level only on builtin components pages to limit console
  // spew.
  const bool is_builtin_component =
      HasWebUIScheme(delegate_->GetMainFrameLastCommittedURL()) ||
      GetContentClient()->browser()->IsBuiltinComponent(
          GetProcess()->GetBrowserContext(), GetLastCommittedOrigin());
  const bool is_off_the_record =
      GetSiteInstance()->GetBrowserContext()->IsOffTheRecord();

  LogConsoleMessage(level, message, line_no, is_builtin_component,
                    is_off_the_record, source_id);
}

void RenderFrameHostImpl::OnCreateChildFrame(
    int new_routing_id,
    service_manager::mojom::InterfaceProviderRequest
        new_interface_provider_provider_request,
    blink::mojom::DocumentInterfaceBrokerRequest
        document_interface_broker_content_request,
    blink::mojom::DocumentInterfaceBrokerRequest
        document_interface_broker_blink_request,
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
  DCHECK(document_interface_broker_content_request.is_pending());
  DCHECK(document_interface_broker_blink_request.is_pending());
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
  // |document_interface_broker_content_handle|,
  // |document_interface_broker_blink_handle| and |devtools_frame_token| were
  // generated on the browser's IO thread and not taken from the renderer
  // process.
  frame_tree_->AddFrame(
      frame_tree_node_, GetProcess()->GetID(), new_routing_id,
      std::move(new_interface_provider_provider_request),
      std::move(document_interface_broker_content_request),
      std::move(document_interface_broker_blink_request), scope, frame_name,
      frame_unique_name, is_created_by_script, devtools_frame_token,
      frame_policy, frame_owner_properties, was_discarded_, owner_type);
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
  // TODO(creis): Remove this block and always set the URL once transfers handle
  // network errors or PlzNavigate is enabled.  See https://crbug.com/588314.
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
      node_to_delete->current_frame_host()->DeleteRenderFrame();
      // Speculative RenderFrameHosts are deleted by the FrameTreeNode's
      // RenderFrameHostManager's destructor. RenderFrameProxyHosts send
      // FrameMsg_Delete automatically in the destructor.
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
    child->current_frame_host()->DeleteRenderFrame();
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
  // Some children with no unload handler may be eligible for immediate
  // deletion. Cut the dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();  // Can delete |this|.
}

void RenderFrameHostImpl::OnFrameFocused() {
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

  if (params.is_history_navigation_in_new_child) {
    // Try to find a FrameNavigationEntry that matches this frame instead, based
    // on the frame's unique name.  If this can't be found, fall back to the
    // default params using RequestOpenURL below.
    if (frame_tree_node_->navigator()->StartHistoryNavigationInNewSubframe(
            this, validated_url)) {
      return;
    }
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
  scheduler_tracked_features_ = features_mask;
}

void RenderFrameHostImpl::OnDidFailProvisionalLoadWithError(
    const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {
  TRACE_EVENT2("navigation",
               "RenderFrameHostImpl::OnDidFailProvisionalLoadWithError",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(),
               "error", params.error_code);
  // TODO(clamy): Kill the renderer with RFH_FAIL_PROVISIONAL_LOAD_NO_HANDLE and
  // return early if navigation_handle_ is null, once we prevent that case from
  // happening in practice. See https://crbug.com/605289.

  // Update the error code in the NavigationHandle of the navigation.
  if (GetNavigationHandle()) {
    GetNavigationHandle()->set_net_error_code(
        static_cast<net::Error>(params.error_code));
  }

  frame_tree_node_->navigator()->DidFailProvisionalLoadWithError(this, params);
}

void RenderFrameHostImpl::OnDidFailLoadWithError(
    const GURL& url,
    int error_code,
    const base::string16& error_description) {
  TRACE_EVENT2("navigation",
               "RenderFrameHostImpl::OnDidFailProvisionalLoadWithError",
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
    if (frame->render_widget_host_)
      return frame->render_widget_host_;
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
  if (FrameMsg_Navigate_Type::IsSameDocument(
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
    Send(new FrameMsg_SwapOut(routing_id_, proxy->GetRoutingID(), is_loading,
                              replication_state));
    // Remember that a RenderFrameProxy was created as part of processing the
    // SwapOut message above.
    proxy->set_render_frame_proxy_created(true);

    StartPendingDeletionOnSubtree();
  }

  // Some children with no unload handler may be eligible for deletion. Cut the
  // dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();

  if (web_ui())
    web_ui()->RenderFrameHostSwappingOut();

  web_bluetooth_services_.clear();
}

void RenderFrameHostImpl::DetachFromProxy() {
  if (unload_state_ != UnloadState::NotRun)
    return;

  // Start pending deletion on this frame and its children.
  DeleteRenderFrame();
  StartPendingDeletionOnSubtree();
  // Some children with no unload handler may be eligible for immediate
  // deletion. Cut the dead branches now. This is a performance optimization.
  PendingDeletionCheckCompletedOnSubtree();
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

void RenderFrameHostImpl::OnRenderProcessGone(int status, int exit_code) {
  base::TerminationStatus termination_status =
      static_cast<base::TerminationStatus>(status);

  if (frame_tree_node_->IsMainFrame()) {
    // Keep the termination status so we can get at it later when we
    // need to know why it died.
    render_view_host_->render_view_termination_status_ = termination_status;
  }

  if (base::FeatureList::IsEnabled(features::kCrashReporting))
    MaybeGenerateCrashReport(termination_status, exit_code);

  // When a frame's process dies, its RenderFrame no longer exists, which means
  // that its child frames must be cleaned up as well.
  ResetChildren();

  // Reset state for the current RenderFrameHost once the FrameTreeNode has been
  // reset.
  SetRenderFrameCreated(false);
  InvalidateMojoConnection();
  document_scoped_interface_provider_binding_.Close();
  document_interface_broker_content_binding_.Close();
  document_interface_broker_blink_binding_.Close();
  SetLastCommittedUrl(GURL());

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
    const TextSurroundingSelectionCallback& callback,
    int max_length) {
  DCHECK(!callback.is_null());
  // Only one outstanding request is allowed at any given time.
  // If already one request is in progress, then immediately release callback
  // with empty result.
  if (!text_surrounding_selection_callback_.is_null()) {
    callback.Run(base::string16(), 0, 0);
    return;
  }
  text_surrounding_selection_callback_ = callback;
  Send(
      new FrameMsg_TextSurroundingSelectionRequest(GetRoutingID(), max_length));
}

void RenderFrameHostImpl::OnTextSurroundingSelectionResponse(
    const base::string16& content,
    uint32_t start_offset,
    uint32_t end_offset) {
  // text_surrounding_selection_callback_ should not be null, but don't trust
  // the renderer.
  if (text_surrounding_selection_callback_.is_null())
    return;

  // Just Run the callback instead of propagating further.
  text_surrounding_selection_callback_.Run(content, start_offset, end_offset);
  // Reset the callback for enabling early exit from future request.
  text_surrounding_selection_callback_.Reset();
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
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
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
        last_committed_origin_, mojo::MakeRequest(&default_factory_info));
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

void RenderFrameHostImpl::OnDidBlockFramebust(const GURL& url) {
  delegate_->OnDidBlockFramebust(url);
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

    if (accessibility_mode.has_mode(ui::AXMode::kNativeAPIs)) {
      if (browser_accessibility_manager_)
        browser_accessibility_manager_->OnAccessibilityEvents(details);
    }

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
    if (ContainsKey(notified_instances, parent_site_instance))
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

  UpdateFrameFrozenState();
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
    blink::WebScrollGranularity granularity) {
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

void RenderFrameHostImpl::BindDocumentInterfaceBrokerRequest(
    blink::mojom::DocumentInterfaceBrokerRequest content_request,
    blink::mojom::DocumentInterfaceBrokerRequest blink_request) {
  DCHECK(!document_interface_broker_content_binding_.is_bound());
  DCHECK(content_request.is_pending());
  document_interface_broker_content_binding_.Bind(std::move(content_request));
  DCHECK(!document_interface_broker_blink_binding_.is_bound());
  DCHECK(blink_request.is_pending());
  document_interface_broker_blink_binding_.Bind(std::move(blink_request));
}

void RenderFrameHostImpl::SetKeepAliveTimeoutForTesting(
    base::TimeDelta timeout) {
  keep_alive_timeout_ = timeout;
  if (keep_alive_handle_factory_)
    keep_alive_handle_factory_->SetTimeout(keep_alive_timeout_);
}

void RenderFrameHostImpl::OnShowCreatedWindow(int pending_widget_routing_id,
                                              WindowOpenDisposition disposition,
                                              const gfx::Rect& initial_rect,
                                              bool user_gesture) {
  delegate_->ShowCreatedWindow(GetProcess()->GetID(), pending_widget_routing_id,
                               disposition, initial_rect, user_gesture);
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
    // Block resource requests until the frame is created, since the HWND might
    // be needed if a response ends up creating a plugin. We'll only have a
    // single frame at this point. These requests will be resumed either in
    // WebContentsImpl::CreateNewWindow or RenderFrameHost::Init.
    // TODO(crbug.com/581037): Even though NPAPI is deleted, other features
    // depend on this behavior. See the bug for more information.
    if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      auto block_requests_for_route = base::Bind(
          [](const GlobalFrameRoutingId& id) {
            auto* rdh = ResourceDispatcherHostImpl::Get();
            if (rdh)
              rdh->BlockRequestsForRoute(id);
          },
          GlobalFrameRoutingId(render_process_id, main_frame_route_id));
      base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                               std::move(block_requests_for_route));
    }
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

  if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
      rfh->waiting_for_init_) {
    // Need to check |waiting_for_init_| as some paths inside CreateNewWindow
    // call above (namely, if WebContentsDelegate::ShouldCreateWebContents
    // returns false) will resume requests by calling RenderFrameHostImpl::Init.
    rfh->frame_->BlockRequests();
  }

  service_manager::mojom::InterfaceProviderPtrInfo
      main_frame_interface_provider_info;
  rfh->BindInterfaceProviderRequest(
      mojo::MakeRequest(&main_frame_interface_provider_info));

  blink::mojom::DocumentInterfaceBrokerPtrInfo
      document_interface_broker_content_info;

  blink::mojom::DocumentInterfaceBrokerPtrInfo
      document_interface_broker_blink_info;
  rfh->BindDocumentInterfaceBrokerRequest(
      mojo::MakeRequest(&document_interface_broker_content_info),
      mojo::MakeRequest(&document_interface_broker_blink_info));

  mojom::CreateNewWindowReplyPtr reply = mojom::CreateNewWindowReply::New(
      render_view_route_id, main_frame_route_id, main_frame_widget_route_id,
      mojom::DocumentScopedInterfaceBundle::New(
          std::move(main_frame_interface_provider_info),
          std::move(document_interface_broker_content_info),
          std::move(document_interface_broker_blink_info)),
      cloned_namespace->id(), rfh->GetDevToolsFrameToken());
  std::move(callback).Run(mojom::CreateNewWindowStatus::kSuccess,
                          std::move(reply));
}

void RenderFrameHostImpl::CreatePortal(
    blink::mojom::PortalAssociatedRequest request,
    CreatePortalCallback callback) {
  if (frame_tree_node()->parent()) {
    mojo::ReportBadMessage(
        "RFHI::CreatePortal called in a nested browsing context");
    return;
  }
  Portal* portal = Portal::Create(this, std::move(request));
  RenderFrameProxyHost* proxy_host = portal->CreateProxyAndAttachPortal();
  std::move(callback).Run(proxy_host->GetRoutingID(), portal->portal_token());
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
  std::move(callback).Run(proxy_host->GetRoutingID());
}

void RenderFrameHostImpl::IssueKeepAliveHandle(
    mojom::KeepAliveHandleRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (GetProcess()->IsKeepAliveRefCountDisabled())
    return;

  if (!keep_alive_handle_factory_) {
    keep_alive_handle_factory_ =
        std::make_unique<KeepAliveHandleFactory>(GetProcess());
    keep_alive_handle_factory_->SetTimeout(keep_alive_timeout_);
  }
  keep_alive_handle_factory_->Create(std::move(request));
}

// TODO(ahemery): Move checks to mojo bad message reporting.
void RenderFrameHostImpl::BeginNavigation(
    const CommonNavigationParams& common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    blink::mojom::BlobURLTokenPtr blob_url_token,
    mojom::NavigationClientAssociatedPtrInfo navigation_client,
    blink::mojom::NavigationInitiatorPtr navigation_initiator) {
  if (frame_tree_node_->render_manager()->is_attaching_inner_delegate()) {
    // Avoid starting any new navigations since this frame is in the process of
    // attaching an inner delegate.
    return;
  }

  if (!is_active())
    return;

  TRACE_EVENT2("navigation", "RenderFrameHostImpl::BeginNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(), "url",
               common_params.url.possibly_invalid_spec());

  DCHECK(IsPerNavigationMojoInterfaceEnabled() == navigation_client.is_valid());

  CommonNavigationParams validated_params = common_params;
  if (!VerifyBeginNavigationCommonParams(GetSiteInstance(), &validated_params))
    return;

  GetProcess()->FilterURL(true, &begin_params->searchable_form_url);

  // If the request was for a blob URL, but the validated URL is no longer a
  // blob URL, reset the blob_url_token to prevent hitting the ReportBadMessage
  // below, and to make sure we don't incorrectly try to use the blob_url_token.
  if (common_params.url.SchemeIsBlob() && !validated_params.url.SchemeIsBlob())
    blob_url_token = nullptr;

  if (blob_url_token && !validated_params.url.SchemeIsBlob()) {
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
  if (blink::BlobUtils::MojoBlobURLsEnabled() &&
      validated_params.url.SchemeIsBlob() && !blob_url_loader_factory) {
    blob_url_loader_factory = ChromeBlobStorageContext::URLLoaderFactoryForUrl(
        GetSiteInstance()->GetBrowserContext(), validated_params.url);
  }

  if (waiting_for_init_) {
    pending_navigate_ = std::make_unique<PendingNavigation>(
        validated_params, std::move(begin_params),
        std::move(blob_url_loader_factory), std::move(navigation_client),
        std::move(navigation_initiator));
    return;
  }

  frame_tree_node()->navigator()->OnBeginNavigation(
      frame_tree_node(), validated_params, std::move(begin_params),
      std::move(blob_url_loader_factory), std::move(navigation_client),
      std::move(navigation_initiator));
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
#if !defined(OS_ANDROID)
  // The default (no-op) implementation of InstalledAppProvider. On Android, the
  // real implementation is provided in Java.
  registry_->AddInterface(base::Bind(&InstalledAppProviderImplDefault::Create));
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

  registry_->AddInterface<device::mojom::WakeLock>(base::Bind(
      &RenderFrameHostImpl::BindWakeLockRequest, base::Unretained(this)));

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebNfc)) {
    registry_->AddInterface<device::mojom::NFC>(base::Bind(
        &RenderFrameHostImpl::BindNFCRequest, base::Unretained(this)));
  }
#endif

  if (!permission_service_context_)
    permission_service_context_.reset(new PermissionServiceContext(this));

  registry_->AddInterface(
      base::Bind(&PermissionServiceContext::CreateService,
                 base::Unretained(permission_service_context_.get())));

  registry_->AddInterface(
      base::Bind(&RenderFrameHostImpl::BindPresentationServiceRequest,
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
      &RenderFrameHostImpl::CreateWebSocket, base::Unretained(this)));

  registry_->AddInterface(base::BindRepeating(
      &RenderFrameHostImpl::CreateDedicatedWorkerHostFactory,
      base::Unretained(this)));

  registry_->AddInterface(base::Bind(&SharedWorkerConnectorImpl::Create,
                                     process_->GetID(), routing_id_));

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
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));

    registry_->AddInterface(
        base::BindRepeating(&MediaStreamDispatcherHost::Create,
                            GetProcess()->GetID(), GetRoutingID(),
                            base::Unretained(media_stream_manager)),
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
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
  registry_->AddInterface(base::BindRepeating(
      &RenderFrameHostImpl::BindSerialServiceRequest, base::Unretained(this)));
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
      &media::MediaMetricsProvider::Create, frame_tree_node_->IsMainFrame(),
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
      std::move(save_stats_cb)));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kEnableGpuBenchmarking)) {
    registry_->AddInterface(
        base::Bind(&InputInjectorImpl::Create, weak_ptr_factory_.GetWeakPtr()));
  }

  // TODO(crbug.com/775792): Move to RendererInterfaceBinders.
  registry_->AddInterface(base::BindRepeating(
      &QuotaDispatcherHost::CreateForFrame, GetProcess(), routing_id_));

  registry_->AddInterface(
      base::BindRepeating(SpeechRecognitionDispatcherHost::Create,
                          GetProcess()->GetID(), routing_id_),
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));

  file_system_manager_.reset(new FileSystemManagerImpl(
      GetProcess()->GetID(), routing_id_,
      GetProcess()->GetStoragePartition()->GetFileSystemContext(),
      ChromeBlobStorageContext::GetFor(GetProcess()->GetBrowserContext())));
  registry_->AddInterface(
      base::BindRepeating(&FileSystemManagerImpl::BindRequest,
                          base::Unretained(file_system_manager_.get())),
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));

  registry_->AddInterface(base::BindRepeating(
      &BackgroundFetchServiceImpl::CreateForFrame, GetProcess(), routing_id_));

  registry_->AddInterface(base::BindRepeating(&ContactsManagerImpl::Create,
                                              base::Unretained(this)));

  registry_->AddInterface(
      base::BindRepeating(&FileChooserImpl::Create, base::Unretained(this)));

  registry_->AddInterface(base::BindRepeating(&WakeLockServiceImpl::Create,
                                              base::Unretained(this)));

  registry_->AddInterface(base::BindRepeating(
      &PictureInPictureServiceImpl::Create, base::Unretained(this)));
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

bool RenderFrameHostImpl::CanCommitOrigin(
    const url::Origin& origin,
    const GURL& url) {
  // If the --disable-web-security flag is specified, all bets are off and the
  // renderer process can send any origin it wishes.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebSecurity)) {
    return true;
  }

  // It is safe to commit into a unique origin, regardless of the URL, as it is
  // restricted from accessing other origins.
  if (origin.opaque())
    return true;

  // Standard URLs must match the reported origin.
  if (url.IsStandard() && !origin.IsSameOriginWith(url::Origin::Create(url)))
    return false;

  // A non-unique origin must be a valid URL, which allows us to safely do a
  // conversion to GURL.
  GURL origin_url = origin.GetURL();

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(GetProcess()->GetID(), origin))
    return false;

  // Verify that the origin is allowed to commit in this process.
  // Note: This also handles non-standard cases for |url|, such as
  // about:blank, data, and blob URLs.
  return CanCommitURL(origin_url);
}

void RenderFrameHostImpl::NavigateToInterstitialURL(const GURL& data_url) {
  TRACE_EVENT1("navigation", "RenderFrameHostImpl::NavigateToInterstitialURL",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id());
  DCHECK(data_url.SchemeIs(url::kDataScheme));
  NavigationDownloadPolicy download_policy;
  download_policy.SetDisallowed(NavigationDownloadType::kInterstitial);

  CommonNavigationParams common_params(
      data_url, base::nullopt, Referrer(), ui::PAGE_TRANSITION_LINK,
      FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT, download_policy, false,
      GURL(), GURL(), PREVIEWS_OFF, base::TimeTicks::Now(), "GET", nullptr,
      base::Optional<SourceLocation>(), false /* started_from_context_menu */,
      false /* has_user_gesture */, InitiatorCSPInfo(), std::string());
  CommitNavigation(nullptr /* navigation_request */, nullptr /* response */,
                   network::mojom::URLLoaderClientEndpointsPtr(), common_params,
                   CommitNavigationParams(), false, base::nullopt,
                   base::nullopt /* subresource_overrides */,
                   nullptr /* provider_info */,
                   base::UnguessableToken::Create() /* not traced */);
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
      frame_tree_node_->navigation_request()
          ->navigation_handle()
          ->set_net_error_code(net::ERR_ABORTED);
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
    if (base::ContainsKey(beforeunload_pending_replies_, rfh))
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

      local_ancestor->DeleteRenderFrame();
      if (local_ancestor != child) {
        child->unload_state_ =
            child->GetSuddenTerminationDisablerState(blink::kUnloadHandler)
                ? UnloadState::InProgress
                : UnloadState::Completed;
      }
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
    network::ResourceResponse* response,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
    bool is_view_source,
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ServiceWorkerProviderInfoForWindowPtr provider_info,
    const base::UnguessableToken& devtools_navigation_token) {
  TRACE_EVENT2("navigation", "RenderFrameHostImpl::CommitNavigation",
               "frame_tree_node", frame_tree_node_->frame_tree_node_id(), "url",
               common_params.url.possibly_invalid_spec());
  DCHECK(!IsRendererDebugURL(common_params.url));
  DCHECK(
      (response && url_loader_client_endpoints) ||
      common_params.url.SchemeIs(url::kDataScheme) ||
      FrameMsg_Navigate_Type::IsSameDocument(common_params.navigation_type) ||
      !IsURLHandledByNetworkStack(common_params.url));

  const bool is_first_navigation = !has_committed_any_navigation_;
  has_committed_any_navigation_ = true;

  UpdatePermissionsForNavigation(common_params, commit_params);

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
      response ? response->head : network::ResourceResponseHead();
  const bool is_same_document =
      FrameMsg_Navigate_Type::IsSameDocument(common_params.navigation_type);

  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
      subresource_loader_factories;
  if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
      (!is_same_document || is_first_navigation)) {
    recreate_default_url_loader_factory_after_network_service_crash_ = false;
    subresource_loader_factories =
        std::make_unique<blink::URLLoaderFactoryBundleInfo>();
    BrowserContext* browser_context = GetSiteInstance()->GetBrowserContext();
    // NOTE: On Network Service navigations, we want to ensure that a frame is
    // given everything it will need to load any accessible subresources. We
    // however only do this for cross-document navigations, because the
    // alternative would be redundant effort.
    if (subresource_loader_params &&
        subresource_loader_params->appcache_loader_factory_info.is_valid()) {
      // If the caller has supplied a factory for AppCache, use it.
      subresource_loader_factories->appcache_factory_info() =
          std::move(subresource_loader_params->appcache_loader_factory_info);

      // Inject test intermediary if needed.
      if (!GetCreateNetworkFactoryCallbackForRenderFrame().is_null()) {
        network::mojom::URLLoaderFactoryPtrInfo original_factory =
            std::move(subresource_loader_factories->appcache_factory_info());
        network::mojom::URLLoaderFactoryRequest new_request = mojo::MakeRequest(
            &subresource_loader_factories->appcache_factory_info());
        GetCreateNetworkFactoryCallbackForRenderFrame().Run(
            std::move(new_request), GetProcess()->GetID(),
            std::move(original_factory));
      }
    }

    non_network_url_loader_factories_.clear();

    // Set up the default factory.
    network::mojom::URLLoaderFactoryPtrInfo default_factory_info;

    // See if this is for WebUI.
    std::string scheme = common_params.url.scheme();
    const auto& webui_schemes = URLDataManagerBackend::GetWebUISchemes();
    if (base::ContainsValue(webui_schemes, scheme)) {
      network::mojom::URLLoaderFactoryPtr factory_for_webui =
          CreateWebUIURLLoaderBinding(this, scheme);
      // If the renderer has webui bindings, then don't give it access to
      // network loader for security reasons.
      // http://crbug.com/829412: make an exception for a small whitelist
      // of WebUIs that need to be fixed to not make network requests in JS.
      if ((enabled_bindings_ & kWebUIBindingsPolicyMask) &&
          !GetContentClient()->browser()->IsWebUIAllowedToMakeNetworkRequests(
              url::Origin::Create(common_params.url.GetOrigin()))) {
        default_factory_info = factory_for_webui.PassInterface();
        // WebUIURLLoaderFactory will kill the renderer if it sees a request
        // with a non-chrome scheme. Register a URLLoaderFactory for the about
        // scheme so about:blank doesn't kill the renderer.
        non_network_url_loader_factories_[url::kAboutScheme] =
            std::make_unique<AboutURLLoaderFactory>();
      } else {
        // This is a webui scheme that doesn't have webui bindings. Give it
        // access to the network loader as it might require it.
        subresource_loader_factories->scheme_specific_factory_infos().emplace(
            scheme, factory_for_webui.PassInterface());
      }
    }

    if (!default_factory_info) {
      // Otherwise default to a Network Service-backed loader from the
      // appropriate NetworkContext.
      recreate_default_url_loader_factory_after_network_service_crash_ = true;
      bool bypass_redirect_checks =
          CreateNetworkServiceDefaultFactoryAndObserve(
              GetOriginForURLLoaderFactory(common_params.url,
                                           GetSiteInstance()),
              mojo::MakeRequest(&default_factory_info));
      subresource_loader_factories->set_bypass_redirect_checks(
          bypass_redirect_checks);
    }

    DCHECK(default_factory_info);
    subresource_loader_factories->default_factory_info() =
        std::move(default_factory_info);


    if (common_params.url.SchemeIsFile()) {
      // Only file resources can load file subresources
      auto file_factory = std::make_unique<FileURLLoaderFactory>(
          browser_context->GetPath(),
          browser_context->GetSharedCorsOriginAccessList(),
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
      non_network_url_loader_factories_.emplace(url::kFileScheme,
                                                std::move(file_factory));
    }

#if defined(OS_ANDROID)
    if (common_params.url.SchemeIs(url::kContentScheme)) {
      // Only content:// URLs can load content:// subresources
      auto content_factory = std::make_unique<ContentURLLoaderFactory>(
          base::CreateSequencedTaskRunnerWithTraits(
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
            this, /*is_navigation=*/false, partition->GetFileSystemContext(),
            storage_domain));

    non_network_url_loader_factories_.emplace(
        url::kDataScheme, std::make_unique<DataURLLoaderFactory>());

    GetContentClient()
        ->browser()
        ->RegisterNonNetworkSubresourceURLLoaderFactories(
            process_->GetID(), routing_id_, &non_network_url_loader_factories_);

    for (auto& factory : non_network_url_loader_factories_) {
      network::mojom::URLLoaderFactoryPtrInfo factory_proxy_info;
      auto factory_request = mojo::MakeRequest(&factory_proxy_info);
      GetContentClient()->browser()->WillCreateURLLoaderFactory(
          browser_context, this, GetProcess()->GetID(),
          false /* is_navigation */, false /* is_download */,
          GetOriginForURLLoaderFactory(common_params.url, GetSiteInstance())
              .value_or(url::Origin()),
          &factory_request, nullptr /* header_client */,
          nullptr /* bypass_redirect_checks */);
      // Keep DevTools proxy last, i.e. closest to the network.
      devtools_instrumentation::WillCreateURLLoaderFactory(
          this, false /* is_navigation */, false /* is_download */,
          &factory_request);
      factory.second->Clone(std::move(factory_request));
      subresource_loader_factories->scheme_specific_factory_infos().emplace(
          factory.first, std::move(factory_proxy_info));
    }

    subresource_loader_factories->initiator_specific_factory_infos() =
        CreateInitiatorSpecificURLLoaderFactories(
            initiators_requiring_separate_url_loader_factory_);
  }

  // It is imperative that cross-document navigations always provide a set of
  // subresource ULFs when the Network Service is enabled.
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService) ||
         is_same_document || !is_first_navigation ||
         subresource_loader_factories);

  if (is_same_document) {
    DCHECK(same_document_navigation_request_);
    GetNavigationControl()->CommitSameDocumentNavigation(
        common_params, commit_params,
        base::BindOnce(&RenderFrameHostImpl::OnSameDocumentCommitProcessed,
                       base::Unretained(this),
                       same_document_navigation_request_->navigation_handle()
                           ->GetNavigationId(),
                       common_params.should_replace_current_entry));
  } else {
    // Pass the controller service worker info if we have one.
    blink::mojom::ControllerServiceWorkerInfoPtr controller;
    blink::mojom::ServiceWorkerObjectAssociatedPtrInfo remote_object;
    blink::mojom::ServiceWorkerState sent_state;
    if (subresource_loader_params &&
        subresource_loader_params->controller_service_worker_info) {
      controller =
          std::move(subresource_loader_params->controller_service_worker_info);
      if (controller->object_info) {
        controller->object_info->request = mojo::MakeRequest(&remote_object);
        sent_state = controller->object_info->state;
      }
    }

    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        factory_bundle_for_prefetch;
    network::mojom::URLLoaderFactoryPtr prefetch_loader_factory;
    if (subresource_loader_factories) {
      // Clone the factory bundle for prefetch.
      auto bundle = base::MakeRefCounted<blink::URLLoaderFactoryBundle>(
          std::move(subresource_loader_factories));
      subresource_loader_factories = CloneFactoryBundle(bundle);
      factory_bundle_for_prefetch = CloneFactoryBundle(bundle);
    } else if (!is_same_document || is_first_navigation) {
      DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
      factory_bundle_for_prefetch =
          std::make_unique<blink::URLLoaderFactoryBundleInfo>();
      network::mojom::URLLoaderFactoryPtrInfo factory_info;
      CreateNetworkServiceDefaultFactoryInternal(
          url::Origin(), mojo::MakeRequest(&factory_info));
      factory_bundle_for_prefetch->default_factory_info() =
          std::move(factory_info);
    }

    if (factory_bundle_for_prefetch) {
      // Also set-up URLLoaderFactory for prefetch using the same loader
      // factories. TODO(kinuko): Consider setting this up only when prefetch
      // is used. Currently we have this here to make sure we have non-racy
      // situation (https://crbug.com/849929).
      auto* storage_partition = static_cast<StoragePartitionImpl*>(
          BrowserContext::GetStoragePartition(
              GetSiteInstance()->GetBrowserContext(), GetSiteInstance()));
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&PrefetchURLLoaderService::GetFactory,
                         storage_partition->GetPrefetchURLLoaderService(),
                         mojo::MakeRequest(&prefetch_loader_factory),
                         frame_tree_node_->frame_tree_node_id(),
                         std::move(factory_bundle_for_prefetch)));
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

    SendCommitNavigation(
        navigation_client, navigation_request, head, common_params,
        commit_params, std::move(url_loader_client_endpoints),
        std::move(subresource_loader_factories),
        std::move(subresource_overrides), std::move(controller),
        std::move(provider_info), std::move(prefetch_loader_factory),
        devtools_navigation_token);

    // |remote_object| is an associated interface ptr, so calls can't be made on
    // it until its request endpoint is sent. Now that the request endpoint was
    // sent, it can be used, so add it to ServiceWorkerObjectHost.
    if (remote_object.is_valid()) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(
              &ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState,
              subresource_loader_params->controller_service_worker_object_host,
              std::move(remote_object), sent_state));
    }

    // If a network request was made, update the Previews state.
    if (IsURLHandledByNetworkStack(common_params.url))
      last_navigation_previews_state_ = common_params.previews_state;
  }

  is_loading_ = true;
}

void RenderFrameHostImpl::FailedNavigation(
    NavigationRequest* navigation_request,
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
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
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
    bool bypass_redirect_checks = CreateNetworkServiceDefaultFactoryAndObserve(
        origin, mojo::MakeRequest(&default_factory_info));
    subresource_loader_factories =
        std::make_unique<blink::URLLoaderFactoryBundleInfo>(
            std::move(default_factory_info),
            blink::URLLoaderFactoryBundleInfo::SchemeMap(),
            blink::URLLoaderFactoryBundleInfo::OriginMap(),
            bypass_redirect_checks);
  }

  mojom::NavigationClient* navigation_client = nullptr;
  if (IsPerNavigationMojoInterfaceEnabled())
    navigation_client = navigation_request->GetCommitNavigationClient();

  SendCommitFailedNavigation(
      navigation_client, navigation_request, common_params, commit_params,
      has_stale_copy_in_cache, error_code, error_page_content,
      std::move(subresource_loader_factories));

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

  RegisterMojoInterfaces();
  mojom::FrameFactoryPtr frame_factory;
  BindInterface(GetProcess(), &frame_factory);
  frame_factory->CreateFrame(routing_id_, MakeRequest(&frame_));

  service_manager::mojom::InterfaceProviderPtr remote_interfaces;
  frame_->GetInterfaceProvider(mojo::MakeRequest(&remote_interfaces));
  remote_interfaces_.reset(new service_manager::InterfaceProvider);
  remote_interfaces_->Bind(std::move(remote_interfaces));

  remote_interfaces_->GetInterface(&frame_input_handler_);
}

void RenderFrameHostImpl::InvalidateMojoConnection() {
  registry_.reset();

  frame_.reset();
  frame_bindings_control_.reset();
  frame_host_associated_binding_.Close();
  navigation_control_.reset();

  // Disconnect with ImageDownloader Mojo service in RenderFrame.
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

const content::mojom::ImageDownloaderPtr&
RenderFrameHostImpl::GetMojoImageDownloader() {
  if (!mojo_image_downloader_.get() && GetRemoteInterfaces())
    GetRemoteInterfaces()->GetInterface(&mojo_image_downloader_);
  return mojo_image_downloader_;
}

const blink::mojom::FindInPageAssociatedPtr&
RenderFrameHostImpl::GetFindInPage() {
  if (!find_in_page_ || !find_in_page_.is_bound() ||
      find_in_page_.encountered_error())
    GetRemoteAssociatedInterfaces()->GetInterface(&find_in_page_);
  return find_in_page_;
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

bool RenderFrameHostImpl::CanCommitURL(const GURL& url) {
  // Renderer-debug URLs can never be committed.
  if (IsRendererDebugURL(url))
    return false;

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
        return true;

      // If an MHTML subframe commits in a different process (even one that
      // appears correct for the subframe's URL), then we aren't correctly
      // loading it from the archive and should kill the renderer.
      base::debug::SetCrashKeyString(
          base::debug::AllocateCrashKeyString(
              "oopif_in_mhtml_page", base::debug::CrashKeySize::Size32),
          is_mhtml_document() ? "is_mhtml_doc" : "not_mhtml_doc");
      return false;
    }
  }

  // Give the client a chance to disallow URLs from committing.
  if (!GetContentClient()->browser()->CanCommitURL(GetProcess(), url))
    return false;

  // TODO(nasko): Consider checking the |url| against CanAccessDataForOrigin.

  return true;
}

void RenderFrameHostImpl::BlockRequestsForFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    ForEachFrame(
        this, base::BindRepeating([](RenderFrameHostImpl* render_frame_host) {
          if (render_frame_host->frame_)
            render_frame_host->frame_->BlockRequests();
        }));
  } else {
    NotifyForEachFrameFromUI(
        this, base::BindRepeating(
                  &ResourceDispatcherHostImpl::BlockRequestsForRoute));
  }
}

void RenderFrameHostImpl::ResumeBlockedRequestsForFrame() {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    ForEachFrame(
        this, base::BindRepeating([](RenderFrameHostImpl* render_frame_host) {
          if (render_frame_host->frame_)
            render_frame_host->frame_->ResumeBlockedRequests();
        }));
  } else {
    NotifyForEachFrameFromUI(
        this, base::BindRepeating(
                  &ResourceDispatcherHostImpl::ResumeBlockedRequestsForRoute));
  }
}

void RenderFrameHostImpl::CancelBlockedRequestsForFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    ForEachFrame(
        this, base::BindRepeating([](RenderFrameHostImpl* render_frame_host) {
          if (render_frame_host->frame_)
            render_frame_host->frame_->CancelBlockedRequests();
        }));
  } else {
    NotifyForEachFrameFromUI(
        this, base::BindRepeating(
                  &ResourceDispatcherHostImpl::CancelBlockedRequestsForRoute));
  }
}

void RenderFrameHostImpl::BindDevToolsAgent(
    blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
    blink::mojom::DevToolsAgentAssociatedRequest request) {
  GetNavigationControl()->BindDevToolsAgent(std::move(host),
                                            std::move(request));
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

  if (browser_accessibility_manager_)
    browser_accessibility_manager_->OnAccessibilityEvents(detail);

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
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params) {
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
    network::mojom::URLLoaderFactoryRequest default_factory_request) {
  bool bypass_redirect_checks = CreateNetworkServiceDefaultFactoryInternal(
      origin, std::move(default_factory_request));

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
    network::mojom::URLLoaderFactoryRequest default_factory_request) {
  auto* context = GetSiteInstance()->GetBrowserContext();
  bool bypass_redirect_checks = false;

  network::mojom::TrustedURLLoaderHeaderClientPtrInfo header_client;
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    GetContentClient()->browser()->WillCreateURLLoaderFactory(
        context, this, GetProcess()->GetID(), false /* is_navigation */,
        false /* is_download */, origin.value_or(url::Origin()),
        &default_factory_request, &header_client, &bypass_redirect_checks);
  }

  // Keep DevTools proxy last, i.e. closest to the network.
  devtools_instrumentation::WillCreateURLLoaderFactory(
      this, false /* is_navigation */, false /* is_download */,
      &default_factory_request);

  // Create the URLLoaderFactory - either via ContentBrowserClient or ourselves.
  if (GetCreateNetworkFactoryCallbackForRenderFrame().is_null()) {
    GetProcess()->CreateURLLoaderFactory(origin, std::move(header_client),
                                         std::move(default_factory_request));
  } else {
    network::mojom::URLLoaderFactoryPtr original_factory;
    GetProcess()->CreateURLLoaderFactory(origin, std::move(header_client),
                                         mojo::MakeRequest(&original_factory));
    GetCreateNetworkFactoryCallbackForRenderFrame().Run(
        std::move(default_factory_request), GetProcess()->GetID(),
        original_factory.PassInterface());
  }

  return bypass_redirect_checks;
}

bool RenderFrameHostImpl::CanExecuteJavaScript() {
#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
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
    blink::mojom::WebUsbServiceRequest request) {
  GetContentClient()->browser()->CreateWebUsbService(this, std::move(request));
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

void RenderFrameHostImpl::CreateWebSocket(
    network::mojom::WebSocketRequest request) {
  network::mojom::AuthenticationHandlerPtr auth_handler;

  network::mojom::TrustedHeaderClientPtr header_client;
  uint32_t options = network::mojom::kWebSocketOptionNone;
  GetContentClient()->browser()->WillCreateWebSocket(
      this, &request, &auth_handler, &header_client, &options);

  // This is to support usage of WebSockets in cases in which there is an
  // associated RenderFrame. This is important for showing the correct security
  // state of the page and also honoring user override of bad certificates.
  WebSocketManager::CreateWebSocket(
      process_->GetID(), routing_id_, last_committed_origin_, options,
      std::move(auth_handler), std::move(header_client), std::move(request));
}

void RenderFrameHostImpl::CreateDedicatedWorkerHostFactory(
    blink::mojom::DedicatedWorkerHostFactoryRequest request) {
  content::CreateDedicatedWorkerHostFactory(process_->GetID(), routing_id_,
                                            last_committed_origin_,
                                            std::move(request));
}

void RenderFrameHostImpl::OnMediaInterfaceFactoryConnectionError() {
  DCHECK(media_interface_proxy_);
  media_interface_proxy_.reset();
}

void RenderFrameHostImpl::BindWakeLockRequest(
    device::mojom::WakeLockRequest request) {
  device::mojom::WakeLock* renderer_wake_lock =
      delegate_ ? delegate_->GetRendererWakeLock() : nullptr;
  if (renderer_wake_lock)
    renderer_wake_lock->AddClient(std::move(request));
}

#if defined(OS_ANDROID)
void RenderFrameHostImpl::BindNFCRequest(device::mojom::NFCRequest request) {
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
    blink::mojom::AuthenticatorRequest request) {
  if (!authenticator_impl_)
    authenticator_impl_.reset(new AuthenticatorImpl(this));

  authenticator_impl_->Bind(std::move(request));
}
#endif

void RenderFrameHostImpl::BindPresentationServiceRequest(
    blink::mojom::PresentationServiceRequest request) {
  if (!presentation_service_)
    presentation_service_ = PresentationServiceImpl::Create(this);

  presentation_service_->Bind(std::move(request));
}

blink::mojom::FileChooserPtr RenderFrameHostImpl::BindFileChooserForTesting() {
  blink::mojom::FileChooserPtr chooser;
  FileChooserImpl::Create(this, mojo::MakeRequest(&chooser));
  return chooser;
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
    blink::mojom::FrameHostTestInterfaceRequest request) {
  class FrameHostTestInterfaceImpl
      : public blink::mojom::FrameHostTestInterface {
   public:
    void Ping(const GURL& url, const std::string& event) override {}
    void GetName(GetNameCallback callback) override {
      std::move(callback).Run("RenderFrameHostImpl");
    }
  };

  mojo::MakeStrongBinding(std::make_unique<FrameHostTestInterfaceImpl>(),
                          std::move(request));
}

void RenderFrameHostImpl::GetAudioContextManager(
    blink::mojom::AudioContextManagerRequest request) {
  AudioContextManagerImpl::Create(this, std::move(request));
}

void RenderFrameHostImpl::GetAuthenticator(
    blink::mojom::AuthenticatorRequest request) {
#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebAuth)) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableWebAuthTestingAPI)) {
      ScopedVirtualAuthenticatorEnvironment::GetInstance();
    }

    BindAuthenticatorRequest(std::move(request));
  }
#else
  GetJavaInterfaces()->GetInterface(std::move(request));
#endif  // !defined(OS_ANDROID)
}

void RenderFrameHostImpl::GetCredentialManager(
    blink::mojom::CredentialManagerRequest request) {
  GetContentClient()->browser()->BindCredentialManagerRequest(
      this, std::move(request));
}

void RenderFrameHostImpl::GetVirtualAuthenticatorManager(
    blink::test::mojom::VirtualAuthenticatorManagerRequest request) {
#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebAuth)) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableWebAuthTestingAPI)) {
      auto* environment_singleton =
          ScopedVirtualAuthenticatorEnvironment::GetInstance();
      environment_singleton->AddBinding(std::move(request));
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
  // an exception for the CanCommitURL checks.  This is ok as long as the origin
  // is unique.
  bool is_permitted_error_page = false;
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

      // Error pages must commit in a unique origin. Terminate the renderer
      // process if this is violated.
      if (!validated_params->origin.opaque()) {
        DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, validated_params->origin);
        bad_message::ReceivedBadMessage(
            process, bad_message::RFH_ERROR_PROCESS_NON_UNIQUE_ORIGIN_COMMIT);
        return false;
      }

      // With error page isolation, any URL can commit in an error page process.
      is_permitted_error_page = true;
    }
  } else {
    // Without error page isolation, a blocked navigation is expected to
    // commit in the old renderer process.  This may be true for subframe
    // navigations even when error page isolation is enabled for main frames.
    if (navigation_request &&
        navigation_request->navigation_handle()->GetNetErrorCode() ==
            net::ERR_BLOCKED_BY_CLIENT) {
      // Since this is known to be an error page commit, verify it happened in
      // a unique origin, terminating the renderer process otherwise.
      if (!validated_params->origin.opaque()) {
        DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, validated_params->origin);
        bad_message::ReceivedBadMessage(
            process, bad_message::RFH_ERROR_PROCESS_NON_UNIQUE_ORIGIN_COMMIT);
        return false;
      }

      is_permitted_error_page = true;
    }
  }

  // file: URLs can be allowed to access any other origin, based on settings.
  bool bypass_checks_for_file_scheme = false;
  if (validated_params->origin.scheme() == url::kFileScheme) {
    WebPreferences prefs = render_view_host_->GetWebkitPreferences();
    if (prefs.allow_universal_access_from_file_urls)
      bypass_checks_for_file_scheme = true;
  }

  // Attempts to commit certain off-limits URL should be caught more strictly
  // than our FilterURL checks.  If a renderer violates this policy, it
  // should be killed.
  if (!is_permitted_error_page && !bypass_checks_for_file_scheme &&
      !CanCommitURL(validated_params->url)) {
    VLOG(1) << "Blocked URL " << validated_params->url.spec();
    LogRendererKillCrashKeys(GetSiteInstance()->GetSiteURL());

    // Kills the process.
    bad_message::ReceivedBadMessage(process,
                                    bad_message::RFH_CAN_COMMIT_URL_BLOCKED);
    return false;
  }

  // Verify that the origin passed from the renderer process is valid and can
  // be allowed to commit in this RenderFrameHost.
  if (!bypass_checks_for_file_scheme &&
      !CanCommitOrigin(validated_params->origin, validated_params->url)) {
    DEBUG_ALIAS_FOR_ORIGIN(origin_debug_alias, validated_params->origin);
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
          base::debug::AllocateCrashKeyString(
              "is_renderer_initiated", base::debug::CrashKeySize::Size32),
          bool_to_crash_key(handle->IsRendererInitiated()));

      base::debug::SetCrashKeyString(
          base::debug::AllocateCrashKeyString(
              "is_server_redirect", base::debug::CrashKeySize::Size32),
          bool_to_crash_key(handle->WasServerRedirect()));

      base::debug::SetCrashKeyString(
          base::debug::AllocateCrashKeyString(
              "is_form_submission", base::debug::CrashKeySize::Size32),
          bool_to_crash_key(handle->IsFormSubmission()));

      base::debug::SetCrashKeyString(
          base::debug::AllocateCrashKeyString(
              "is_error_page", base::debug::CrashKeySize::Size32),
          bool_to_crash_key(handle->IsErrorPage()));

      base::debug::SetCrashKeyString(
          base::debug::AllocateCrashKeyString(
              "initiator_origin", base::debug::CrashKeySize::Size64),
          handle->GetInitiatorOrigin()
              ? handle->GetInitiatorOrigin()->GetDebugString()
              : "none");

      base::debug::SetCrashKeyString(
          base::debug::AllocateCrashKeyString(
              "starting_site_instance", base::debug::CrashKeySize::Size64),
          handle->GetStartingSiteInstance()->GetSiteURL().spec());
    }

    // Kills the process.
    bad_message::ReceivedBadMessage(process,
                                    bad_message::RFH_INVALID_ORIGIN_ON_COMMIT);
    return false;
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
  std::unique_ptr<NavigationRequest> invalid_request = nullptr;
  if (navigation_request &&
      navigation_request->commit_params().navigation_token !=
          validated_params->navigation_token) {
    navigation_request.reset();
    // TODO(clamy): We should kill the renderer in all cases where we expect to
    // have a NavigationRequest matching the commit URL.
  }

  if (!ValidateDidCommitParams(navigation_request.get(), validated_params,
                               is_same_document_navigation)) {
    return false;
  }

  if (navigation_request &&
      navigation_request->common_params().url != validated_params->url) {
    // At this point we know that the right/matching |navigation_request| has
    // already been found based on token matching performed or the commit
    // originated from a NavigationClient owned by the NavigationRequest. OTOH,
    // we cannot use its NavigationHandle, because it has a mismatched URL
    // (which would cause DCHECKs - for example in
    // NavigationHandleImpl::DidCommitNavigation).
    //
    // Because of the above, if the URL does not match what the NavigationHandle
    // expects, we want to treat the commit as a new navigation.
    //
    // The URL mismatch can happen when loading a Data navigation with
    // LoadDataWithBaseURL.
    // TODO(csharrison): Data navigations loaded with LoadDataWithBaseURL get
    // reset here, because the NavigationHandle tracks the URL but the
    // params.url tracks the data. The trick of saving the old entry ids for
    // these navigations should go away when this is properly handled. See
    // https://crbug.com/588317.
    //
    // Other cases are where URL mismatch can happen is when committing an error
    // page - for example this can happen during CSP/frame-ancestors checks (see
    // https://crbug.com/759184).
    //
    // TODO(clamy): We should support the URL filtering without deleting the
    // request.

    // Note: the NavigationRequest is not reset here, as this could potentially
    // lead to the deletion of the pending NavigationEntry.
    invalid_request = std::move(navigation_request);
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
    // First check if there was a request for this navigation that cannot be
    // used due to URL mismatch. If that's the case and it corresponds to a
    // navigation to the pending NavigationEntry, the new request should be
    // associated with the pending NavigationEntry as well so that the pending
    // NavigationEntry is properly committed.
    NavigationEntryImpl* entry_for_navigation = nullptr;
    if (invalid_request && NavigationRequestWasIntendedForPendingEntry(
                               invalid_request.get(), *validated_params,
                               is_same_document_navigation)) {
      entry_for_navigation = NavigationEntryImpl::FromNavigationEntry(
          frame_tree_node()->navigator()->GetController()->GetPendingEntry());
    }

    navigation_request = CreateNavigationRequestForCommit(
        *validated_params, is_same_document_navigation, entry_for_navigation);
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

  UpdateSiteURL(validated_params->url, validated_params->url_is_unreachable);

  // Set the state whether this navigation is to an MHTML document, since there
  // are certain security checks that we cannot apply to subframes in MHTML
  // documents. Do not trust renderer data when determining that, rather use
  // the |navigation_request|, which was generated and stays browser side.
  is_mhtml_document_ =
      (navigation_request->GetMimeType() == "multipart/related" ||
       navigation_request->GetMimeType() == "message/rfc822");

  accessibility_reset_count_ = 0;
  appcache_handle_ =
      navigation_request->navigation_handle()->TakeAppCacheHandle();
  frame_tree_node()->navigator()->DidNavigate(this, *validated_params,
                                              std::move(navigation_request),
                                              is_same_document_navigation);

  // TODO(clamy): We should stop having a special case for same-document
  // navigation and just put them in the general map of NavigationRequests.
  if (is_same_document_navigation && invalid_request)
    same_document_navigation_request_ = std::move(invalid_request);

  if (!is_same_document_navigation)
    scheduler_tracked_features_ = 0;

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
    const network::ResourceResponseHead& head,
    const content::CommonNavigationParams& common_params,
    const content::CommitNavigationParams& commit_params,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    base::Optional<std::vector<::content::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    blink::mojom::ServiceWorkerProviderInfoForWindowPtr provider_info,
    network::mojom::URLLoaderFactoryPtr prefetch_loader_factory,
    const base::UnguessableToken& devtools_navigation_token) {
  if (navigation_client) {
    navigation_client->CommitNavigation(
        head, common_params, commit_params,
        std::move(url_loader_client_endpoints),
        std::move(subresource_loader_factories),
        std::move(subresource_overrides), std::move(controller),
        std::move(provider_info), std::move(prefetch_loader_factory),
        devtools_navigation_token,
        BuildNavigationClientCommitNavigationCallback(navigation_request));
  } else {
    GetNavigationControl()->CommitNavigation(
        head, common_params, commit_params,
        std::move(url_loader_client_endpoints),
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
    const content::CommonNavigationParams& common_params,
    const content::CommitNavigationParams& commit_params,
    bool has_stale_copy_in_cache,
    int32_t error_code,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories) {
  if (navigation_client) {
    navigation_client->CommitFailedNavigation(
        common_params, commit_params, has_stale_copy_in_cache, error_code,
        error_page_content, std::move(subresource_loader_factories),
        BuildNavigationClientCommitFailedNavigationCallback(
            navigation_request));
  } else {
    GetNavigationControl()->CommitFailedNavigation(
        common_params, commit_params, has_stale_copy_in_cache, error_code,
        error_page_content, std::move(subresource_loader_factories),
        BuildCommitFailedNavigationCallback(navigation_request));
  }
}

// Called when the renderer navigates.  For every frame loaded, we'll get this
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

    document_interface_broker_content_binding_.Close();
    document_interface_broker_blink_binding_.Close();
    BindDocumentInterfaceBrokerRequest(
        std::move(interface_params->document_interface_broker_content_request),
        std::move(interface_params->document_interface_broker_blink_request));

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
      document_interface_broker_content_binding_.Close();
      document_interface_broker_blink_binding_.Close();
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
    RenderWidgetHostImpl::From(GetView()->GetRenderWidgetHost())
        ->DidNavigate(validated_params->content_source_id);
  }
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
  if (!base::FeatureList::IsEnabled(features::kFreezeFramesOnVisibility))
    return;

  // If the document is in the loading state keep it still loading.
  if (is_loading_)
    return;

  // TODO(dtapuska): Adjust these based on feature policies when
  // they are available.
  // Feature policies don't support parameterized values yet.
  // crbug.com/924568, crbug.com/907125
  if (visibility_ == blink::mojom::FrameVisibility::kNotRendered) {
    frame_->SetLifecycleState(blink::mojom::FrameLifecycleState::kFrozen);
  } else if (visibility_ ==
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
  GetNavigationControl()->PostMessageEvent(source_routing_id, source_origin,
                                           target_origin, std::move(message));
}

bool RenderFrameHostImpl::IsTestRenderFrameHost() const {
  return false;
}

}  // namespace content
