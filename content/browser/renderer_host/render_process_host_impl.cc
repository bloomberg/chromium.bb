// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "content/browser/renderer_host/render_process_host_impl.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/user_metrics.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/tracked_objects.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/metrics/single_sample_metrics.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/viz/common/resources/buffer_to_texture_target_map.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "content/browser/appcache/appcache_dispatcher_host.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/background_fetch/background_fetch_service_impl.h"
#include "content/browser/background_sync/background_sync_service_impl.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/blob_dispatcher_host.h"
#include "content/browser/blob_storage/blob_registry_wrapper.h"
#include "content/browser/blob_storage/blob_url_loader_factory.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/browser_main.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/browser_plugin/browser_plugin_message_filter.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/dom_storage_message_filter.h"
#include "content/browser/field_trial_recorder.h"
#include "content/browser/fileapi/fileapi_message_filter.h"
#include "content/browser/frame_host/render_frame_message_filter.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_client.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/browser/histogram_message_filter.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_scheduler_filter.h"
#include "content/browser/loader/url_loader_factory_impl.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/media/midi_host.h"
#include "content/browser/memory/memory_coordinator_impl.h"
#include "content/browser/mime_registry_impl.h"
#include "content/browser/net/reporting_service_proxy.h"
#include "content/browser/notifications/notification_message_filter.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/payments/payment_manager.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/profiler_message_filter.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/quota_dispatcher_host.h"
#include "content/browser/renderer_host/clipboard_message_filter.h"
#include "content/browser/renderer_host/database_message_filter.h"
#include "content/browser/renderer_host/file_utilities_message_filter.h"
#include "content/browser/renderer_host/media/audio_input_renderer_host.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"
#include "content/browser/renderer_host/media/video_capture_host.h"
#include "content/browser/renderer_host/offscreen_canvas_provider_impl.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_frame_subscriber.h"
#include "content/browser/renderer_host/text_input_client_message_filter.h"
#include "content/browser/resolve_proxy_msg_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/speech/speech_recognition_dispatcher_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/browser/websockets/websocket_manager.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/render_process_messages.h"
#include "content/common/resource_messages.h"
#include "content/common/service_manager/child_connection.h"
#include "content/common/service_manager/service_manager_connection_impl.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/worker_service.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/common/process_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "device/gamepad/gamepad_monitor.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "media/base/media_switches.h"
#include "media/media_features.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/features/features.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_interface.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/shape_detection/public/interfaces/barcodedetection.mojom.h"
#include "services/shape_detection/public/interfaces/constants.mojom.h"
#include "services/shape_detection/public/interfaces/facedetection_provider.mojom.h"
#include "services/shape_detection/public/interfaces/textdetection.mojom.h"
#include "storage/browser/fileapi/sandbox_file_system_backend.h"
#include "third_party/WebKit/public/public_features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/java_interfaces.h"
#include "ipc/ipc_sync_channel.h"
#include "media/audio/android/audio_manager_android.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "content/browser/renderer_host/dwrite_font_proxy_message_filter_win.h"
#include "content/common/font_cache_dispatcher_win.h"
#include "content/common/sandbox_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "ui/display/win/dpi.h"
#endif

#if defined(OS_MACOSX)
#include "content/browser/bootstrap_sandbox_manager_mac.h"
#include "content/browser/mach_broker_mac.h"
#endif

#if defined(OS_POSIX)
#include "content/public/browser/zygote_handle_linux.h"
#endif  // defined(OS_POSIX)

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#include "ppapi/shared_impl/ppapi_switches.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
#include "content/browser/renderer_host/media/media_stream_track_metrics_host.h"
#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/common/media/aec_dump_messages.h"
#endif

#if BUILDFLAG(USE_MINIKIN_HYPHENATION)
#include "content/browser/hyphenation/hyphenation_impl.h"
#endif

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace content {
namespace {

const RenderProcessHostFactory* g_render_process_host_factory_ = nullptr;
const char kSiteProcessMapKeyName[] = "content_site_process_map";

#if defined(OS_ANDROID)
// This matches Android's ChildProcessConnection state before OnProcessLaunched.
constexpr bool kLaunchingProcessIsBackgrounded = true;
constexpr bool kLaunchingProcessIsBoostedForPendingView = true;
#else
constexpr bool kLaunchingProcessIsBackgrounded = false;
constexpr bool kLaunchingProcessIsBoostedForPendingView = false;
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
const base::FilePath::CharType kAecDumpFileNameAddition[] =
    FILE_PATH_LITERAL("aec_dump");
#endif

void CacheShaderInfo(int32_t id, base::FilePath path) {
  if (GetShaderCacheFactorySingleton())
    GetShaderCacheFactorySingleton()->SetCacheInfo(id, path);
}

void RemoveShaderInfo(int32_t id) {
  if (GetShaderCacheFactorySingleton())
    GetShaderCacheFactorySingleton()->RemoveCacheInfo(id);
}

net::URLRequestContext* GetRequestContext(
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_refptr<net::URLRequestContextGetter> media_request_context,
    ResourceType resource_type) {
  // If the request has resource type of RESOURCE_TYPE_MEDIA, we use a request
  // context specific to media for handling it because these resources have
  // specific needs for caching.
  if (resource_type == RESOURCE_TYPE_MEDIA)
    return media_request_context->GetURLRequestContext();
  return request_context->GetURLRequestContext();
}

void GetContexts(
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_refptr<net::URLRequestContextGetter> media_request_context,
    ResourceType resource_type,
    ResourceContext** resource_context_out,
    net::URLRequestContext** request_context_out) {
  *resource_context_out = resource_context;
  *request_context_out =
      GetRequestContext(request_context, media_request_context, resource_type);
}

#if BUILDFLAG(ENABLE_WEBRTC)

// Creates a file used for handing over to the renderer.
IPC::PlatformFileForTransit CreateFileForProcess(base::FilePath file_path) {
  base::File dump_file(file_path,
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!dump_file.IsValid()) {
    VLOG(1) << "Could not open AEC dump file, error="
            << dump_file.error_details();
    return IPC::InvalidPlatformFileForTransit();
  }
  return IPC::TakePlatformFileForTransit(std::move(dump_file));
}

// Allow us to only run the trial in the first renderer.
bool has_done_stun_trials = false;

#endif

// the global list of all renderer processes
base::LazyInstance<base::IDMap<RenderProcessHost*>>::Leaky g_all_hosts =
    LAZY_INSTANCE_INITIALIZER;

// Map of site to process, to ensure we only have one RenderProcessHost per
// site in process-per-site mode.  Each map is specific to a BrowserContext.
class SiteProcessMap : public base::SupportsUserData::Data {
 public:
  typedef base::hash_map<std::string, RenderProcessHost*> SiteToProcessMap;
  SiteProcessMap() {}

  void RegisterProcess(const std::string& site, RenderProcessHost* process) {
    // There could already exist a site to process mapping due to races between
    // two WebContents with blank SiteInstances. If that occurs, keeping the
    // exising entry and not overwriting it is a predictable behavior that is
    // safe.
    SiteToProcessMap::iterator i = map_.find(site);
    if (i == map_.end())
      map_[site] = process;
  }

  RenderProcessHost* FindProcess(const std::string& site) {
    SiteToProcessMap::iterator i = map_.find(site);
    if (i != map_.end())
      return i->second;
    return NULL;
  }

  void RemoveProcess(RenderProcessHost* host) {
    // Find all instances of this process in the map, then separately remove
    // them.
    std::set<std::string> sites;
    for (SiteToProcessMap::const_iterator i = map_.begin(); i != map_.end();
         i++) {
      if (i->second == host)
        sites.insert(i->first);
    }
    for (std::set<std::string>::iterator i = sites.begin(); i != sites.end();
         i++) {
      SiteToProcessMap::iterator iter = map_.find(*i);
      if (iter != map_.end()) {
        DCHECK_EQ(iter->second, host);
        map_.erase(iter);
      }
    }
  }

 private:
  SiteToProcessMap map_;
};

// Find the SiteProcessMap specific to the given context.
SiteProcessMap* GetSiteProcessMapForBrowserContext(BrowserContext* context) {
  DCHECK(context);
  SiteProcessMap* map = static_cast<SiteProcessMap*>(
      context->GetUserData(kSiteProcessMapKeyName));
  if (!map) {
    map = new SiteProcessMap();
    context->SetUserData(kSiteProcessMapKeyName, base::WrapUnique(map));
  }
  return map;
}

// NOTE: changes to this class need to be reviewed by the security team.
class RendererSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  RendererSandboxedProcessLauncherDelegate() {}

  ~RendererSandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    AddBaseHandleClosePolicy(policy);

    const base::string16& sid =
        GetContentClient()->browser()->GetAppContainerSidForSandboxType(
            GetSandboxType());
    if (!sid.empty())
      AddAppContainerPolicy(policy, sid.c_str());

    return GetContentClient()->browser()->PreSpawnRenderer(policy);
  }

#elif defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID) && \
    !defined(OS_FUCHSIA)
  ZygoteHandle GetZygote() override {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringType renderer_prefix =
        browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);
    if (!renderer_prefix.empty())
      return nullptr;
    return GetGenericZygote();
  }
#endif  // OS_WIN

  SandboxType GetSandboxType() override { return SANDBOX_TYPE_RENDERER; }
};

const char kSessionStorageHolderKey[] = "kSessionStorageHolderKey";

class SessionStorageHolder : public base::SupportsUserData::Data {
 public:
  SessionStorageHolder()
      : session_storage_namespaces_awaiting_close_(
            new std::map<int, SessionStorageNamespaceMap>) {
  }

  ~SessionStorageHolder() override {
    // Its important to delete the map on the IO thread to avoid deleting
    // the underlying namespaces prior to processing ipcs referring to them.
    BrowserThread::DeleteSoon(
        BrowserThread::IO, FROM_HERE,
        session_storage_namespaces_awaiting_close_.release());
  }

  void Hold(const SessionStorageNamespaceMap& sessions, int view_route_id) {
    (*session_storage_namespaces_awaiting_close_)[view_route_id] = sessions;
  }

  void Release(int old_route_id) {
    session_storage_namespaces_awaiting_close_->erase(old_route_id);
  }

 private:
  std::unique_ptr<std::map<int, SessionStorageNamespaceMap>>
      session_storage_namespaces_awaiting_close_;
  DISALLOW_COPY_AND_ASSIGN(SessionStorageHolder);
};

// This class manages spare RenderProcessHosts.
//
// There is a singleton instance of this class which manages a single spare
// renderer (g_spare_render_process_host_manager, below). This class
// encapsulates the implementation of
// RenderProcessHost::WarmupSpareRenderProcessHost()
//
// RenderProcessHostImpl should call
// SpareRenderProcessHostManager::MaybeTakeSpareRenderProcessHost when creating
// a new RPH. In this implementation, the spare renderer is bound to a
// BrowserContext and its default StoragePartition. If
// MaybeTakeSpareRenderProcessHost is called with a BrowserContext and
// StoragePartition that does not match, the spare renderer is discarded. In
// particular, only the default StoragePartition will be able to use a spare
// renderer. The spare renderer will also not be used as a guest renderer
// (is_for_guests_ == true).
//
// It is safe to call WarmupSpareRenderProcessHost multiple times, although if
// called in a context where the spare renderer is not likely to be used
// performance may suffer due to the unnecessary RPH creation.
class SpareRenderProcessHostManager : public RenderProcessHostObserver {
 public:
  SpareRenderProcessHostManager() {}

  void WarmupSpareRenderProcessHost(BrowserContext* browser_context) {
    StoragePartitionImpl* current_partition =
        static_cast<StoragePartitionImpl*>(
            BrowserContext::GetStoragePartition(browser_context, nullptr));

    if (spare_render_process_host_ &&
        matching_browser_context_ == browser_context &&
        matching_storage_partition_ == current_partition)
      return;  // Nothing to warm up.

    CleanupSpareRenderProcessHost();

    // Don't create a spare renderer if we're using --single-process or if we've
    // got too many processes. See also ShouldTryToUseExistingProcessHost in
    // this file.
    if (RenderProcessHost::run_renderer_in_process() ||
        g_all_hosts.Get().size() >=
            RenderProcessHostImpl::GetMaxRendererProcessCount())
      return;

    matching_browser_context_ = browser_context;
    matching_storage_partition_ = current_partition;

    spare_render_process_host_ = RenderProcessHostImpl::CreateRenderProcessHost(
        browser_context, current_partition, nullptr,
        false /* is_for_guests_only */);
    spare_render_process_host_->AddObserver(this);
    spare_render_process_host_->Init();
  }

  // If |partition| is null, this gets the default partition from the browser
  // context.
  RenderProcessHost* MaybeTakeSpareRenderProcessHost(
      BrowserContext* browser_context,
      StoragePartition* partition,
      SiteInstance* site_instance,
      bool is_for_guests_only) {
    if (spare_render_process_host_ &&
        browser_context == matching_browser_context_ && !is_for_guests_only &&
        !partition) {
      // If the spare renderer matches for everything but possibly the storage
      // partition, and the passed-in partition is null, get the default storage
      // partition. If this is the case, the default storage partition will
      // already have been created and there is no possibility of breaking tests
      // by GetDefaultStoragePartition prematurely creating one.
      partition =
          BrowserContext::GetStoragePartition(browser_context, site_instance);
    }

    if (!spare_render_process_host_ ||
        browser_context != matching_browser_context_ ||
        partition != matching_storage_partition_ || is_for_guests_only) {
      // As a new RenderProcessHost will almost certainly be created, we cleanup
      // the non-matching one so as not to waste resources.
      CleanupSpareRenderProcessHost();
      return nullptr;
    }

    CHECK(spare_render_process_host_->HostHasNotBeenUsed());
    RenderProcessHost* rph = spare_render_process_host_;
    DropSpareRenderProcessHost(spare_render_process_host_);
    return rph;
  }

  // Remove |host| as a possible spare renderer. Does not shut it down cleanly;
  // the assumption is that the host was shutdown somewhere else and has
  // notifying the SpareRenderProcessHostManager.
  void DropSpareRenderProcessHost(RenderProcessHost* host) {
    if (spare_render_process_host_ && spare_render_process_host_ == host) {
      spare_render_process_host_->RemoveObserver(this);
      spare_render_process_host_ = nullptr;
    }
  }

  // Remove |host| as a possible spare renderer. If |host| is not the spare
  // renderer, then shut down the spare renderer. The idea is that a navigation
  // was just made to |host|, and we do not expect another immediate navigation,
  // so that the spare renderer can be dropped in order to free up resources.
  void DropSpareOnProcessCreation(RenderProcessHost* new_host) {
    if (spare_render_process_host_ == new_host) {
      DropSpareRenderProcessHost(new_host);
    } else {
      CleanupSpareRenderProcessHost();
    }
  }

  // Gracefully remove and cleanup a spare RenderProcessHost if it exists.
  void CleanupSpareRenderProcessHost() {
    if (spare_render_process_host_) {
      spare_render_process_host_->Cleanup();
      DropSpareRenderProcessHost(spare_render_process_host_);
    }
  }

  RenderProcessHost* spare_render_process_host() {
    return spare_render_process_host_;
  }

 private:
  // RenderProcessHostObserver
  void RenderProcessWillExit(RenderProcessHost* host) override {
    DropSpareRenderProcessHost(host);
  }

  void RenderProcessExited(RenderProcessHost* host,
                           base::TerminationStatus unused_status,
                           int unused_exit_code) override {
    DropSpareRenderProcessHost(host);
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    DropSpareRenderProcessHost(host);
  }

  // This is a bare pointer, because RenderProcessHost manages the lifetime of
  // all its instances; see g_all_hosts, above.
  RenderProcessHost* spare_render_process_host_ = nullptr;

  // Used only to check if a creation request matches the spare, and not
  // accessed.
  const BrowserContext* matching_browser_context_ = nullptr;
  const StoragePartition* matching_storage_partition_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SpareRenderProcessHostManager);
};

base::LazyInstance<SpareRenderProcessHostManager>::Leaky
    g_spare_render_process_host_manager = LAZY_INSTANCE_INITIALIZER;

const void* const kDefaultSubframeProcessHostHolderKey =
    &kDefaultSubframeProcessHostHolderKey;

class DefaultSubframeProcessHostHolder : public base::SupportsUserData::Data,
                                         public RenderProcessHostObserver {
 public:
  explicit DefaultSubframeProcessHostHolder(BrowserContext* browser_context)
      : browser_context_(browser_context) {}
  ~DefaultSubframeProcessHostHolder() override {}

  // Gets the correct render process to use for this SiteInstance.
  RenderProcessHost* GetProcessHost(SiteInstance* site_instance,
                                    bool is_for_guests_only) {
    StoragePartitionImpl* default_partition =
        static_cast<StoragePartitionImpl*>(
            BrowserContext::GetDefaultStoragePartition(browser_context_));
    StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetStoragePartition(browser_context_, site_instance));

    // Is this the default storage partition? If it isn't, then just give it its
    // own non-shared process.
    if (partition != default_partition || is_for_guests_only) {
      RenderProcessHost* host = RenderProcessHostImpl::CreateRenderProcessHost(
          browser_context_, partition, site_instance, is_for_guests_only);
      host->SetIsNeverSuitableForReuse();
      return host;
    }

    // If we already have a shared host for the default storage partition, use
    // it.
    if (host_)
      return host_;

    host_ = RenderProcessHostImpl::CreateOrUseSpareRenderProcessHost(
        browser_context_, partition, site_instance,
        false /* is for guests only */);
    host_->SetIsNeverSuitableForReuse();
    host_->AddObserver(this);

    return host_;
  }

  // Implementation of RenderProcessHostObserver.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    DCHECK_EQ(host_, host);
    host_->RemoveObserver(this);
    host_ = nullptr;
  }

 private:
  BrowserContext* browser_context_;

  // The default subframe render process used for the default storage partition
  // of this BrowserContext.
  RenderProcessHost* host_ = nullptr;
};

void CreateMemoryCoordinatorHandle(
    int render_process_id,
    mojom::MemoryCoordinatorHandleRequest request) {
  MemoryCoordinatorImpl::GetInstance()->CreateHandle(render_process_id,
                                                     std::move(request));
}

void CreateResourceCoordinatorProcessInterface(
    RenderProcessHostImpl* render_process_host,
    resource_coordinator::mojom::CoordinationUnitRequest request) {
  render_process_host->GetProcessResourceCoordinator()->AddBinding(
      std::move(request));
}

// Forwards service requests to Service Manager since the renderer cannot launch
// out-of-process services on is own.
template <typename R>
void ForwardShapeDetectionRequest(R request) {
  // TODO(beng): This should really be using the per-profile connector.
  service_manager::Connector* connector =
      ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(shape_detection::mojom::kServiceName,
                           std::move(request));
}

class RenderProcessHostIsReadyObserver : public RenderProcessHostObserver {
 public:
  RenderProcessHostIsReadyObserver(RenderProcessHost* render_process_host,
                                   base::OnceClosure task)
      : render_process_host_(render_process_host),
        task_(std::move(task)),
        weak_factory_(this) {
    render_process_host_->AddObserver(this);
    if (render_process_host_->IsReady())
      PostTask();
  }

  ~RenderProcessHostIsReadyObserver() override {
    render_process_host_->RemoveObserver(this);
  }

  void RenderProcessReady(RenderProcessHost* host) override { PostTask(); }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    delete this;
  }

 private:
  void PostTask() {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&RenderProcessHostIsReadyObserver::CallTask,
                       weak_factory_.GetWeakPtr()));
  }

  void CallTask() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (render_process_host_->IsReady())
      std::move(task_).Run();

    delete this;
  }

  RenderProcessHost* render_process_host_;
  base::OnceClosure task_;
  base::WeakPtrFactory<RenderProcessHostIsReadyObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostIsReadyObserver);
};

// The following class is used to track the sites each RenderProcessHost is
// hosting frames for and expecting navigations to. There are two of them per
// BrowserContext: one for frames and one for navigations.
//
// For each site, the SiteProcessCountTracker keeps a map of counts per
// RenderProcessHost, which represents the number of frames/navigations
// for this site that are associated with the RenderProcessHost. This allows to
// quickly lookup a list of RenderProcessHost that can be used by a particular
// SiteInstance. On the other hand, it does not allow to quickly lookup which
// sites are hosted by a RenderProcessHost. This class is meant to help reusing
// RenderProcessHosts among SiteInstances, not to perform security checks for a
// RenderProcessHost.
//
// TODO(alexmos): Currently, the tracking in this class and in
// UnmatchedServiceWorkerProcessTracker is associated with a BrowserContext,
// but it needs to also consider StoragePartitions, so that process reuse is
// allowed only within the same StoragePartition.  For now, the tracking is
// done only for the default StoragePartition.  See https://crbug.com/752667.
const void* const kCommittedSiteProcessCountTrackerKey =
    "CommittedSiteProcessCountTrackerKey";
const void* const kPendingSiteProcessCountTrackerKey =
    "PendingSiteProcessCountTrackerKey";
class SiteProcessCountTracker : public base::SupportsUserData::Data,
                                public RenderProcessHostObserver {
 public:
  SiteProcessCountTracker() {}
  ~SiteProcessCountTracker() override { DCHECK(map_.empty()); }

  void IncrementSiteProcessCount(const GURL& site_url,
                                 int render_process_host_id) {
    std::map<ProcessID, Count>& counts_per_process = map_[site_url];
    ++counts_per_process[render_process_host_id];

#ifndef NDEBUG
    // In debug builds, observe the RenderProcessHost destruction, to check
    // that it is properly removed from the map.
    RenderProcessHost* host = RenderProcessHost::FromID(render_process_host_id);
    if (!HasProcess(host))
      host->AddObserver(this);
#endif
  }

  void DecrementSiteProcessCount(const GURL& site_url,
                                 int render_process_host_id) {
    auto result = map_.find(site_url);
    DCHECK(result != map_.end());
    std::map<ProcessID, Count>& counts_per_process = result->second;

    --counts_per_process[render_process_host_id];
    DCHECK(counts_per_process[render_process_host_id] >= 0);

    if (counts_per_process[render_process_host_id] == 0)
      counts_per_process.erase(render_process_host_id);

    if (counts_per_process.empty())
      map_.erase(site_url);
  }

  void FindRenderProcessesForSite(
      const GURL& site_url,
      std::set<RenderProcessHost*>* foreground_processes,
      std::set<RenderProcessHost*>* background_processes) {
    auto result = map_.find(site_url);
    if (result == map_.end())
      return;

    std::map<ProcessID, Count>& counts_per_process = result->second;
    for (auto iter : counts_per_process) {
      RenderProcessHost* host = RenderProcessHost::FromID(iter.first);
      if (!host) {
        // TODO(clamy): This shouldn't happen but we are getting reports from
        // the field that this is happening. We need to figure out why some
        // RenderProcessHosts are not taken out of the map when they're
        // destroyed.
        NOTREACHED();
        continue;
      }
      if (host->VisibleWidgetCount())
        foreground_processes->insert(host);
      else
        background_processes->insert(host);
    }
  }

 private:
  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
#ifndef NDEBUG
    host->RemoveObserver(this);
    DCHECK(!HasProcess(host));
#endif
  }

#ifndef NDEBUG
  // Used in debug builds to ensure that RenderProcessHost don't persist in the
  // map after they've been destroyed.
  bool HasProcess(RenderProcessHost* process) {
    for (auto iter : map_) {
      std::map<ProcessID, Count>& counts_per_process = iter.second;
      for (auto iter_process : counts_per_process) {
        if (iter_process.first == process->GetID())
          return true;
      }
    }
    return false;
  }
#endif

  using ProcessID = int;
  using Count = int;
  using CountPerProcessPerSiteMap = std::map<GURL, std::map<ProcessID, Count>>;
  CountPerProcessPerSiteMap map_;
};

bool ShouldUseSiteProcessTracking(BrowserContext* browser_context,
                                  StoragePartition* dest_partition,
                                  const GURL& site_url) {
  if (site_url.is_empty())
    return false;

  // TODO(alexmos): Sites should be tracked separately for each
  // StoragePartition.  For now, track them only in the default one.
  StoragePartition* default_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);
  if (dest_partition != default_partition)
    return false;

  return true;
}

bool ShouldTrackProcessForSite(BrowserContext* browser_context,
                               RenderProcessHost* render_process_host,
                               const GURL& site_url) {
  return ShouldUseSiteProcessTracking(
      browser_context, render_process_host->GetStoragePartition(), site_url);
}

bool ShouldFindReusableProcessHostForSite(BrowserContext* browser_context,
                                          const GURL& site_url) {
  return ShouldUseSiteProcessTracking(
      browser_context,
      BrowserContext::GetStoragePartitionForSite(browser_context, site_url),
      site_url);
}

const void* const kUnmatchedServiceWorkerProcessTrackerKey =
    "UnmatchedServiceWorkerProcessTrackerKey";

// This class tracks 'unmatched' service worker processes. When a service worker
// is started after a navigation to the site, SiteProcessCountTracker that is
// implemented above is used to find the matching renderer process which is used
// for the navigation. But a service worker may be started before a navigation
// (ex: Push notification -> show the page of the notification).
// This class tracks processes with 'unmatched' service workers until the
// processes are reused for a navigation to a matching site. After a single
// matching navigation is put into the process, all service workers for that
// site in that process are considered 'matched.'
//
// TODO(alexmos): Currently, the tracking in this class and in
// SiteProcessCountTracker is associated with a BrowserContext, but it needs to
// also consider StoragePartitions, so that process reuse is allowed only
// within the same StoragePartition.  For now, the tracking is done only for
// the default StoragePartition.  See https://crbug.com/752667.
class UnmatchedServiceWorkerProcessTracker
    : public base::SupportsUserData::Data,
      public RenderProcessHostObserver {
 public:
  // Registers |render_process_host| as having an unmatched service worker for
  // |site_url|.
  static void Register(BrowserContext* browser_context,
                       RenderProcessHost* render_process_host,
                       const GURL& site_url) {
    DCHECK(!site_url.is_empty());
    if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                   site_url))
      return;

    UnmatchedServiceWorkerProcessTracker* tracker =
        static_cast<UnmatchedServiceWorkerProcessTracker*>(
            browser_context->GetUserData(
                kUnmatchedServiceWorkerProcessTrackerKey));
    if (!tracker) {
      tracker = new UnmatchedServiceWorkerProcessTracker();
      browser_context->SetUserData(kUnmatchedServiceWorkerProcessTrackerKey,
                                   base::WrapUnique(tracker));
    }
    tracker->RegisterProcessForSite(render_process_host, site_url);
  }

  // Find a process with an unmatched service worker for |site_url| and removes
  // the process from the tracker if it exists.
  static RenderProcessHost* MatchWithSite(BrowserContext* browser_context,
                                          const GURL& site_url) {
    if (!ShouldFindReusableProcessHostForSite(browser_context, site_url))
      return nullptr;

    UnmatchedServiceWorkerProcessTracker* tracker =
        static_cast<UnmatchedServiceWorkerProcessTracker*>(
            browser_context->GetUserData(
                kUnmatchedServiceWorkerProcessTrackerKey));
    if (!tracker)
      return nullptr;
    return tracker->TakeFreshestProcessForSite(site_url);
  }

  UnmatchedServiceWorkerProcessTracker() {}

  ~UnmatchedServiceWorkerProcessTracker() override {
    DCHECK(site_process_set_.empty());
  }

  // Implementation of RenderProcessHostObserver.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    DCHECK(HasProcess(host));
    int process_id = host->GetID();
    for (auto it = site_process_set_.begin(); it != site_process_set_.end();) {
      if (it->second == process_id) {
        it = site_process_set_.erase(it);
      } else {
        ++it;
      }
    }
    host->RemoveObserver(this);
  }

 private:
  void RegisterProcessForSite(RenderProcessHost* host, const GURL& site_url) {
    if (!HasProcess(host))
      host->AddObserver(this);
    site_process_set_.insert(SiteProcessIDPair(site_url, host->GetID()));
  }

  RenderProcessHost* TakeFreshestProcessForSite(const GURL& site_url) {
    RenderProcessHost* host = FindFreshestProcessForSite(site_url);
    if (!host)
      return nullptr;
    site_process_set_.erase(SiteProcessIDPair(site_url, host->GetID()));
    if (!HasProcess(host))
      host->RemoveObserver(this);
    return host;
  }

  RenderProcessHost* FindFreshestProcessForSite(const GURL& site_url) const {
    for (const auto& site_process_pair : base::Reversed(site_process_set_)) {
      if (site_process_pair.first == site_url)
        return RenderProcessHost::FromID(site_process_pair.second);
    }
    return nullptr;
  }

  // Returns true if this tracker contains the process ID |host->GetID()|.
  bool HasProcess(RenderProcessHost* host) const {
    int process_id = host->GetID();
    for (const auto& site_process_id : site_process_set_) {
      if (site_process_id.second == process_id)
        return true;
    }
    return false;
  }

  using ProcessID = int;
  using SiteProcessIDPair = std::pair<GURL, ProcessID>;
  using SiteProcessIDPairSet = std::set<SiteProcessIDPair>;

  // Use std::set because duplicates don't need to be tracked separately (eg.,
  // service workers for the same site in the same process). It is sorted in the
  // order of insertion.
  SiteProcessIDPairSet site_process_set_;
};

void CopyFeatureSwitch(const base::CommandLine& src,
                       base::CommandLine* dest,
                       const char* switch_name) {
  std::vector<std::string> features = FeaturesFromSwitch(src, switch_name);
  if (!features.empty())
    dest->AppendSwitchASCII(switch_name, base::JoinString(features, ","));
}

}  // namespace

RendererMainThreadFactoryFunction g_renderer_main_thread_factory = nullptr;
RenderProcessHostImpl::CreateStoragePartitionServiceFunction
    g_create_storage_partition = nullptr;

base::MessageLoop* g_in_process_thread;

// Stores the maximum number of renderer processes the content module can
// create.
static size_t g_max_renderer_count_override = 0;

// static
bool g_run_renderer_in_process_ = false;

// Held by the RPH and used to control an (unowned) ConnectionFilterImpl from
// any thread.
class RenderProcessHostImpl::ConnectionFilterController
    : public base::RefCountedThreadSafe<ConnectionFilterController> {
 public:
  // |filter| is not owned by this object.
  explicit ConnectionFilterController(ConnectionFilterImpl* filter)
      : filter_(filter) {}

  void DisableFilter();

 private:
  friend class base::RefCountedThreadSafe<ConnectionFilterController>;
  friend class ConnectionFilterImpl;

  ~ConnectionFilterController() {}

  void Detach() {
    base::AutoLock lock(lock_);
    filter_ = nullptr;
  }

  base::Lock lock_;
  ConnectionFilterImpl* filter_;
};

// Held by the RPH's BrowserContext's ServiceManagerConnection, ownership
// transferred back to RPH upon RPH destruction.
class RenderProcessHostImpl::ConnectionFilterImpl : public ConnectionFilter {
 public:
  ConnectionFilterImpl(
      const service_manager::Identity& child_identity,
      std::unique_ptr<service_manager::BinderRegistry> registry)
      : child_identity_(child_identity),
        registry_(std::move(registry)),
        controller_(new ConnectionFilterController(this)),
        weak_factory_(this) {
    // Registration of this filter may race with browser shutdown, in which case
    // it's possible for this filter to be destroyed on the main thread. This
    // is fine as long as the filter hasn't been used on the IO thread yet. We
    // detach the ThreadChecker initially and the first use of the filter will
    // bind it.
    thread_checker_.DetachFromThread();
  }

  ~ConnectionFilterImpl() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    controller_->Detach();
  }

  scoped_refptr<ConnectionFilterController> controller() { return controller_; }

  void Disable() {
    base::AutoLock lock(enabled_lock_);
    enabled_ = false;
  }

 private:
  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // We only fulfill connections from the renderer we host.
    if (child_identity_.name() != source_info.identity.name() ||
        child_identity_.instance() != source_info.identity.instance()) {
      return;
    }

    base::AutoLock lock(enabled_lock_);
    if (!enabled_)
      return;

    registry_->TryBindInterface(interface_name, interface_pipe);
  }

  base::ThreadChecker thread_checker_;
  service_manager::Identity child_identity_;
  std::unique_ptr<service_manager::BinderRegistry> registry_;
  scoped_refptr<ConnectionFilterController> controller_;

  // Guards |enabled_|.
  base::Lock enabled_lock_;
  bool enabled_ = true;

  base::WeakPtrFactory<ConnectionFilterImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionFilterImpl);
};

void RenderProcessHostImpl::ConnectionFilterController::DisableFilter() {
  base::AutoLock lock(lock_);
  if (filter_)
    filter_->Disable();
}

base::MessageLoop*
RenderProcessHostImpl::GetInProcessRendererThreadForTesting() {
  return g_in_process_thread;
}

// static
size_t RenderProcessHost::GetMaxRendererProcessCount() {
  if (g_max_renderer_count_override)
    return g_max_renderer_count_override;

#if defined(OS_ANDROID)
  // On Android we don't maintain a limit of renderer process hosts - we are
  // happy with keeping a lot of these, as long as the number of live renderer
  // processes remains reasonable, and on Android the OS takes care of that.
  // TODO(boliu): This is a short term workaround before ChildProcessLauncher
  // can actively kill child processes in LRU order. Bug and process is tracked
  // in crbug.com/693484. Note this workaround is not perfect and still has
  // corner case problems.
  static const size_t kNumRendererSlots =
      ChildProcessLauncher::GetNumberOfRendererSlots();
  return kNumRendererSlots;
#endif

  // On other platforms, we calculate the maximum number of renderer process
  // hosts according to the amount of installed memory as reported by the OS.
  // The calculation assumes that you want the renderers to use half of the
  // installed RAM and assuming that each WebContents uses ~40MB.  If you modify
  // this assumption, you need to adjust the ThirtyFourTabs test to match the
  // expected number of processes.
  //
  // With the given amounts of installed memory below on a 32-bit CPU, the
  // maximum renderer count will roughly be as follows:
  //
  //   128 MB -> 3
  //   512 MB -> 6
  //  1024 MB -> 12
  //  4096 MB -> 51
  // 16384 MB -> 82 (kMaxRendererProcessCount)

  static size_t max_count = 0;
  if (!max_count) {
    const size_t kEstimatedWebContentsMemoryUsage =
#if defined(ARCH_CPU_64_BITS)
        60;  // In MB
#else
        40;  // In MB
#endif
    max_count = base::SysInfo::AmountOfPhysicalMemoryMB() / 2;
    max_count /= kEstimatedWebContentsMemoryUsage;

    const size_t kMinRendererProcessCount = 3;
    max_count = std::max(max_count, kMinRendererProcessCount);
    max_count = std::min(max_count, kMaxRendererProcessCount);
  }
  return max_count;
}

// static
void RenderProcessHost::SetMaxRendererProcessCount(size_t count) {
  g_max_renderer_count_override = count;
}

// static
RenderProcessHost* RenderProcessHostImpl::CreateRenderProcessHost(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition_impl,
    SiteInstance* site_instance,
    bool is_for_guests_only) {
  if (g_render_process_host_factory_) {
    return g_render_process_host_factory_->CreateRenderProcessHost(
        browser_context);
  }

  if (!storage_partition_impl) {
    storage_partition_impl = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetStoragePartition(browser_context, site_instance));
  }

  return new RenderProcessHostImpl(browser_context, storage_partition_impl,
                                   is_for_guests_only);
}

// static
RenderProcessHost* RenderProcessHostImpl::CreateOrUseSpareRenderProcessHost(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition_impl,
    SiteInstance* site_instance,
    bool is_for_guests_only) {
  RenderProcessHost* render_process_host =
      g_spare_render_process_host_manager.Get().MaybeTakeSpareRenderProcessHost(
          browser_context, storage_partition_impl, site_instance,
          is_for_guests_only);

  if (!render_process_host) {
    render_process_host =
        CreateRenderProcessHost(browser_context, storage_partition_impl,
                                site_instance, is_for_guests_only);
  }

  DCHECK(render_process_host);
  return render_process_host;
}

RenderProcessHostImpl::RenderProcessHostImpl(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition_impl,
    bool is_for_guests_only)
    : fast_shutdown_started_(false),
      deleting_soon_(false),
#ifndef NDEBUG
      is_self_deleted_(false),
#endif
      pending_views_(0),
      keep_alive_ref_count_(0),
      is_keep_alive_ref_count_disabled_(false),
      route_provider_binding_(this),
      visible_widgets_(0),
      priority_({
            kLaunchingProcessIsBackgrounded,
            kLaunchingProcessIsBoostedForPendingView,
#if defined(OS_ANDROID)
            ChildProcessImportance::NORMAL,
#endif
      }),
      id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      browser_context_(browser_context),
      storage_partition_impl_(storage_partition_impl),
      sudden_termination_allowed_(true),
      ignore_input_events_(false),
      is_for_guests_only_(is_for_guests_only),
      is_unused_(true),
      gpu_observer_registered_(false),
      delayed_cleanup_needed_(false),
      within_process_died_observer_(false),
#if BUILDFLAG(ENABLE_WEBRTC)
      webrtc_eventlog_host_(id_),
#endif
      permission_service_context_(new PermissionServiceContext(this)),
      indexed_db_factory_(new IndexedDBDispatcherHost(
          id_,
          storage_partition_impl_->GetURLRequestContext(),
          storage_partition_impl_->GetIndexedDBContext(),
          ChromeBlobStorageContext::GetFor(browser_context_))),
      channel_connected_(false),
      sent_render_process_ready_(false),
#if defined(OS_ANDROID)
      never_signaled_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
#endif
      renderer_host_binding_(this),
      instance_weak_factory_(
          new base::WeakPtrFactory<RenderProcessHostImpl>(this)),
      frame_sink_provider_(id_),
      shared_bitmap_allocation_notifier_impl_(
          viz::ServerSharedBitmapManager::current()),
      weak_factory_(this) {
  widget_helper_ = new RenderWidgetHelper();

  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID());

  CHECK(!BrowserMainRunner::ExitedMainMessageLoop());
  RegisterHost(GetID(), this);
  g_all_hosts.Get().set_check_on_null_data(true);
  // Initialize |child_process_activity_time_| to a reasonable value.
  mark_child_process_activity_time();

  if (!GetBrowserContext()->IsOffTheRecord() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::BindOnce(&CacheShaderInfo, GetID(),
                                           storage_partition_impl_->GetPath()));
  }

  push_messaging_manager_.reset(new PushMessagingManager(
      GetID(), storage_partition_impl_->GetServiceWorkerContext()));

  AddObserver(indexed_db_factory_.get());
#if defined(OS_MACOSX)
  if (BootstrapSandboxManager::ShouldEnable())
    AddObserver(BootstrapSandboxManager::GetInstance());
#endif

  InitializeChannelProxy();
}

// static
void RenderProcessHostImpl::ShutDownInProcessRenderer() {
  DCHECK(g_run_renderer_in_process_);

  switch (g_all_hosts.Pointer()->size()) {
    case 0:
      return;
    case 1: {
      RenderProcessHostImpl* host = static_cast<RenderProcessHostImpl*>(
          AllHostsIterator().GetCurrentValue());
      for (auto& observer : host->observers_)
        observer.RenderProcessHostDestroyed(host);
#ifndef NDEBUG
      host->is_self_deleted_ = true;
#endif
      delete host;
      return;
    }
    default:
      NOTREACHED() << "There should be only one RenderProcessHost when running "
                   << "in-process.";
  }
}

void RenderProcessHostImpl::RegisterRendererMainThreadFactory(
    RendererMainThreadFactoryFunction create) {
  g_renderer_main_thread_factory = create;
}

void RenderProcessHostImpl::SetCreateStoragePartitionServiceFunction(
    CreateStoragePartitionServiceFunction function) {
  g_create_storage_partition = function;
}

RenderProcessHostImpl::~RenderProcessHostImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#ifndef NDEBUG
  DCHECK(is_self_deleted_)
      << "RenderProcessHostImpl is destroyed by something other than itself";
#endif

  // Make sure to clean up the in-process renderer before the channel, otherwise
  // it may still run and have its IPCs fail, causing asserts.
  in_process_renderer_.reset();

  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(GetID());

  if (gpu_observer_registered_) {
    ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
    gpu_observer_registered_ = false;
  }

  is_dead_ = true;

  UnregisterHost(GetID());

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::BindOnce(&RemoveShaderInfo, GetID()));
  }
}

bool RenderProcessHostImpl::Init() {
  // calling Init() more than once does nothing, this makes it more convenient
  // for the view host which may not be sure in some cases
  if (HasConnection())
    return true;

  is_dead_ = false;

  base::CommandLine::StringType renderer_prefix;
  // A command prefix is something prepended to the command line of the spawned
  // process.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  renderer_prefix =
      browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);

#if defined(OS_LINUX)
  int flags = renderer_prefix.empty() ? ChildProcessHost::CHILD_ALLOW_SELF
                                      : ChildProcessHost::CHILD_NORMAL;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  // Find the renderer before creating the channel so if this fails early we
  // return without creating the channel.
  base::FilePath renderer_path = ChildProcessHost::GetChildPath(flags);
  if (renderer_path.empty())
    return false;

  sent_render_process_ready_ = false;

  // We may reach Init() during process death notification (e.g.
  // RenderProcessExited on some observer). In this case the Channel may be
  // null, so we re-initialize it here.
  if (!channel_)
    InitializeChannelProxy();
  DCHECK(broker_client_invitation_);

  // Unpause the Channel briefly. This will be paused again below if we launch a
  // real child process. Note that messages may be sent in the short window
  // between now and then (e.g. in response to RenderProcessWillLaunch) and we
  // depend on those messages being sent right away.
  //
  // |channel_| must always be non-null here: either it was initialized in
  // the constructor, or in the most recent call to ProcessDied().
  channel_->Unpause(false /* flush */);

  // Call the embedder first so that their IPC filters have priority.
  GetContentClient()->browser()->RenderProcessWillLaunch(this);

#if !defined(OS_MACOSX)
  // Intentionally delay the hang monitor creation after the first renderer
  // is created. On Mac audio thread is the UI thread, a hang monitor is not
  // necessary or recommended.
  media::AudioManager::StartHangMonitorIfNeeded(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
#endif  // !defined(OS_MACOSX)

#if defined(OS_ANDROID)
  // Initialize the java audio manager so that media session tests will pass.
  // See internal b/29872494.
  static_cast<media::AudioManagerAndroid*>(media::AudioManager::Get())->
      InitializeIfNeeded();
#endif  // defined(OS_ANDROID)

  CreateMessageFilters();
  RegisterMojoInterfaces();

  if (run_renderer_in_process()) {
    DCHECK(g_renderer_main_thread_factory);
    // Crank up a thread and run the initialization there.  With the way that
    // messages flow between the browser and renderer, this thread is required
    // to prevent a deadlock in single-process mode.  Since the primordial
    // thread in the renderer process runs the WebKit code and can sometimes
    // make blocking calls to the UI thread (i.e. this thread), they need to run
    // on separate threads.
    in_process_renderer_.reset(
        g_renderer_main_thread_factory(InProcessChildThreadParams(
            BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
            broker_client_invitation_.get(),
            child_connection_->service_token())));

    base::Thread::Options options;
#if defined(OS_WIN) && !defined(OS_MACOSX)
    // In-process plugins require this to be a UI message loop.
    options.message_loop_type = base::MessageLoop::TYPE_UI;
#else
    // We can't have multiple UI loops on Linux and Android, so we don't support
    // in-process plugins.
    options.message_loop_type = base::MessageLoop::TYPE_DEFAULT;
#endif
    // As for execution sequence, this callback should have no any dependency
    // on starting in-process-render-thread.
    // So put it here to trigger ChannelMojo initialization earlier to enable
    // in-process-render-thread using ChannelMojo there.
    OnProcessLaunched();  // Fake a callback that the process is ready.

    in_process_renderer_->StartWithOptions(options);

    g_in_process_thread = in_process_renderer_->message_loop();

    // Make sure any queued messages on the channel are flushed in the case
    // where we aren't launching a child process.
    channel_->Flush();
  } else {
    // Build command line for renderer.  We call AppendRendererCommandLine()
    // first so the process type argument will appear first.
    std::unique_ptr<base::CommandLine> cmd_line =
        base::MakeUnique<base::CommandLine>(renderer_path);
    if (!renderer_prefix.empty())
      cmd_line->PrependWrapper(renderer_prefix);
    AppendRendererCommandLine(cmd_line.get());

    // Spawn the child process asynchronously to avoid blocking the UI thread.
    // As long as there's no renderer prefix, we can use the zygote process
    // at this stage.
    child_process_launcher_.reset(new ChildProcessLauncher(
        base::MakeUnique<RendererSandboxedProcessLauncherDelegate>(),
        std::move(cmd_line), GetID(), this,
        std::move(broker_client_invitation_),
        base::Bind(&RenderProcessHostImpl::OnMojoError, id_)));
    channel_->Pause();

    fast_shutdown_started_ = false;
  }

  if (!gpu_observer_registered_) {
    gpu_observer_registered_ = true;
    ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
  }

  is_initialized_ = true;
  init_time_ = base::TimeTicks::Now();
  return true;
}

void RenderProcessHostImpl::EnableSendQueue() {
  if (!channel_)
    InitializeChannelProxy();
}

void RenderProcessHostImpl::InitializeChannelProxy() {
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);

  // Acquire a Connector which will route connections to a new instance of the
  // renderer service.
  service_manager::Connector* connector =
      BrowserContext::GetConnectorFor(browser_context_);
  if (!connector) {
    // Note that some embedders (e.g. Android WebView) may not initialize a
    // Connector per BrowserContext. In those cases we fall back to the
    // browser-wide Connector.
    if (!ServiceManagerConnection::GetForProcess()) {
      // Additionally, some test code may not initialize the process-wide
      // ServiceManagerConnection prior to this point. This class of test code
      // doesn't care about render processes, so we can initialize a dummy
      // connection.
      ServiceManagerConnection::SetForProcess(ServiceManagerConnection::Create(
          mojo::MakeRequest(&test_service_), io_task_runner));
    }
    connector = ServiceManagerConnection::GetForProcess()->GetConnector();
  }

  // Establish a ServiceManager connection for the new render service instance.
  broker_client_invitation_ =
      base::MakeUnique<mojo::edk::OutgoingBrokerClientInvitation>();
  service_manager::Identity child_identity(
      mojom::kRendererServiceName,
      BrowserContext::GetServiceUserIdFor(GetBrowserContext()),
      base::StringPrintf("%d_%d", id_, instance_id_++));
  child_connection_.reset(new ChildConnection(child_identity,
                                              broker_client_invitation_.get(),
                                              connector, io_task_runner));

  // Send an interface request to bootstrap the IPC::Channel. Note that this
  // request will happily sit on the pipe until the process is launched and
  // connected to the ServiceManager. We take the other end immediately and
  // plug it into a new ChannelProxy.
  mojo::MessagePipe pipe;
  BindInterface(IPC::mojom::ChannelBootstrap::Name_, std::move(pipe.handle1));
  std::unique_ptr<IPC::ChannelFactory> channel_factory =
      IPC::ChannelMojo::CreateServerFactory(std::move(pipe.handle0),
                                            io_task_runner);

  ResetChannelProxy();

  // Do NOT expand ifdef or run time condition checks here! Synchronous
  // IPCs from browser process are banned. It is only narrowly allowed
  // for Android WebView to maintain backward compatibility.
  // See crbug.com/526842 for details.
#if defined(OS_ANDROID)
  if (GetContentClient()->UsingSynchronousCompositing()) {
    channel_ = IPC::SyncChannel::Create(
        this, io_task_runner.get(), &never_signaled_);
  }
#endif  // OS_ANDROID
  if (!channel_)
    channel_.reset(new IPC::ChannelProxy(this, io_task_runner.get()));
  channel_->Init(std::move(channel_factory), true /* create_pipe_now */);

  // Note that Channel send is effectively paused and unpaused at various points
  // during startup, and existing code relies on a fragile relative message
  // ordering resulting from some early messages being queued until process
  // launch while others are sent immediately. See https://goo.gl/REW75h for
  // details.
  //
  // We acquire a few associated interface proxies here -- before the channel is
  // paused -- to ensure that subsequent initialization messages on those
  // interfaces behave properly. Specifically, this avoids the risk of an
  // interface being requested while the Channel is paused, which could
  // effectively and undesirably block the transmission of a subsequent message
  // on that interface while the Channel is unpaused.
  //
  // See OnProcessLaunched() for some additional details of this somewhat
  // surprising behavior.
  channel_->GetRemoteAssociatedInterface(&remote_route_provider_);
  channel_->GetRemoteAssociatedInterface(&renderer_interface_);

  // We start the Channel in a paused state. It will be briefly unpaused again
  // in Init() if applicable, before process launch is initiated.
  channel_->Pause();
}

void RenderProcessHostImpl::ResetChannelProxy() {
  if (!channel_)
    return;

  channel_.reset();
  channel_connected_ = false;
}

void RenderProcessHostImpl::CreateMessageFilters() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AddFilter(new ResourceSchedulerFilter(GetID()));
  MediaInternals* media_internals = MediaInternals::GetInstance();
  // Add BrowserPluginMessageFilter to ensure it gets the first stab at messages
  // from guests.
  scoped_refptr<BrowserPluginMessageFilter> bp_message_filter(
      new BrowserPluginMessageFilter(GetID()));
  AddFilter(bp_message_filter.get());

  scoped_refptr<net::URLRequestContextGetter> request_context(
      storage_partition_impl_->GetURLRequestContext());
  scoped_refptr<RenderMessageFilter> render_message_filter(
      new RenderMessageFilter(
          GetID(), GetBrowserContext(), request_context.get(),
          widget_helper_.get(), media_internals,
          storage_partition_impl_->GetDOMStorageContext(),
          storage_partition_impl_->GetCacheStorageContext()));
  AddFilter(render_message_filter.get());

  render_frame_message_filter_ = new RenderFrameMessageFilter(
      GetID(),
#if BUILDFLAG(ENABLE_PLUGINS)
      PluginServiceImpl::GetInstance(),
#else
      nullptr,
#endif
      GetBrowserContext(),
      request_context.get(),
      widget_helper_.get());
  AddFilter(render_frame_message_filter_.get());

  BrowserContext* browser_context = GetBrowserContext();
  ResourceContext* resource_context = browser_context->GetResourceContext();

  scoped_refptr<net::URLRequestContextGetter> media_request_context(
      GetStoragePartition()->GetMediaURLRequestContext());

  ResourceMessageFilter::GetContextsCallback get_contexts_callback(base::Bind(
      &GetContexts, resource_context, request_context, media_request_context));

  // Several filters need the Blob storage context, so fetch it in advance.
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context =
      ChromeBlobStorageContext::GetFor(browser_context);

  resource_message_filter_ = new ResourceMessageFilter(
      GetID(), storage_partition_impl_->GetAppCacheService(),
      blob_storage_context.get(),
      storage_partition_impl_->GetFileSystemContext(),
      storage_partition_impl_->GetServiceWorkerContext(), get_contexts_callback,
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));

  AddFilter(resource_message_filter_.get());

  media::AudioManager* audio_manager =
      BrowserMainLoop::GetInstance()->audio_manager();
  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();
  // The AudioInputRendererHost needs to be available for
  // lookup, so it's stashed in a member variable.
  audio_input_renderer_host_ = new AudioInputRendererHost(
      GetID(), audio_manager, media_stream_manager,
      AudioMirroringManager::GetInstance(),
      BrowserMainLoop::GetInstance()->user_input_monitor());
  AddFilter(audio_input_renderer_host_.get());
  if (!RendererAudioOutputStreamFactoryContextImpl::UseMojoFactories()) {
    AddFilter(base::MakeRefCounted<AudioRendererHost>(
                  GetID(), audio_manager,
                  BrowserMainLoop::GetInstance()->audio_system(),
                  AudioMirroringManager::GetInstance(), media_stream_manager,
                  browser_context->GetMediaDeviceIDSalt())
                  .get());
  }
  AddFilter(
      new MidiHost(GetID(), BrowserMainLoop::GetInstance()->midi_service()));
  AddFilter(new AppCacheDispatcherHost(
      storage_partition_impl_->GetAppCacheService(), GetID()));
  AddFilter(new ClipboardMessageFilter(blob_storage_context));
  AddFilter(new DOMStorageMessageFilter(
      storage_partition_impl_->GetDOMStorageContext()));

#if BUILDFLAG(ENABLE_WEBRTC)
  peer_connection_tracker_host_ = new PeerConnectionTrackerHost(
      GetID(), webrtc_eventlog_host_.GetWeakPtr());
  AddFilter(peer_connection_tracker_host_.get());
  AddFilter(new MediaStreamTrackMetricsHost());
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
  AddFilter(new PepperRendererConnection(GetID()));
#endif
  AddFilter(new SpeechRecognitionDispatcherHost(
      GetID(), storage_partition_impl_->GetURLRequestContext()));
  AddFilter(new FileAPIMessageFilter(
      GetID(), storage_partition_impl_->GetURLRequestContext(),
      storage_partition_impl_->GetFileSystemContext(),
      blob_storage_context.get()));
  AddFilter(new BlobDispatcherHost(
      GetID(), blob_storage_context,
      make_scoped_refptr(storage_partition_impl_->GetFileSystemContext())));
  AddFilter(new FileUtilitiesMessageFilter(GetID()));
  AddFilter(
      new DatabaseMessageFilter(storage_partition_impl_->GetDatabaseTracker()));
#if defined(OS_MACOSX)
  AddFilter(new TextInputClientMessageFilter());
#elif defined(OS_WIN)
  AddFilter(new DWriteFontProxyMessageFilter());

  // The FontCacheDispatcher is required only when we're using GDI rendering.
  // TODO(scottmg): pdf/ppapi still require the renderer to be able to precache
  // GDI fonts (http://crbug.com/383227), even when using DirectWrite. This
  // should eventually be if (!ShouldUseDirectWrite()) guarded.
  channel_->AddFilter(new FontCacheDispatcher());
#endif

  scoped_refptr<CacheStorageDispatcherHost> cache_storage_filter =
      new CacheStorageDispatcherHost();
  cache_storage_filter->Init(storage_partition_impl_->GetCacheStorageContext());
  AddFilter(cache_storage_filter.get());

  scoped_refptr<ServiceWorkerDispatcherHost> service_worker_filter =
      new ServiceWorkerDispatcherHost(GetID(), resource_context);
  service_worker_filter->Init(
      storage_partition_impl_->GetServiceWorkerContext());
  AddFilter(service_worker_filter.get());

  AddFilter(new SharedWorkerMessageFilter(
      GetID(), resource_context,
      WorkerStoragePartition(
          storage_partition_impl_->GetURLRequestContext(),
          storage_partition_impl_->GetMediaURLRequestContext(),
          storage_partition_impl_->GetAppCacheService(),
          storage_partition_impl_->GetQuotaManager(),
          storage_partition_impl_->GetFileSystemContext(),
          storage_partition_impl_->GetDatabaseTracker(),
          storage_partition_impl_->GetIndexedDBContext(),
          storage_partition_impl_->GetServiceWorkerContext()),
      base::Bind(&RenderWidgetHelper::GetNextRoutingID,
                 base::Unretained(widget_helper_.get()))));

#if BUILDFLAG(ENABLE_WEBRTC)
  p2p_socket_dispatcher_host_ = new P2PSocketDispatcherHost(
      resource_context, request_context.get());
  AddFilter(p2p_socket_dispatcher_host_.get());
#endif

  AddFilter(new TraceMessageFilter(GetID()));
  AddFilter(new ResolveProxyMsgHelper(request_context.get()));
  AddFilter(new QuotaDispatcherHost(
      GetID(), storage_partition_impl_->GetQuotaManager(),
      GetContentClient()->browser()->CreateQuotaPermissionContext()));

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context(
      static_cast<ServiceWorkerContextWrapper*>(
          storage_partition_impl_->GetServiceWorkerContext()));
  notification_message_filter_ = new NotificationMessageFilter(
      GetID(), storage_partition_impl_->GetPlatformNotificationContext(),
      resource_context, service_worker_context, browser_context);
  AddFilter(notification_message_filter_.get());

  AddFilter(new ProfilerMessageFilter(PROCESS_TYPE_RENDERER));
  AddFilter(new HistogramMessageFilter());
#if defined(OS_ANDROID)
  synchronous_compositor_filter_ =
      new SynchronousCompositorBrowserFilter(GetID());
  AddFilter(synchronous_compositor_filter_.get());
#endif
}

void RenderProcessHostImpl::RegisterMojoInterfaces() {
  auto registry = base::MakeUnique<service_manager::BinderRegistry>();

  channel_->AddAssociatedInterfaceForIOThread(
      base::Bind(&IndexedDBDispatcherHost::AddBinding,
                 base::Unretained(indexed_db_factory_.get())));

  AddUIThreadInterface(
      registry.get(),
      base::Bind(&ForwardShapeDetectionRequest<
                 shape_detection::mojom::BarcodeDetectionRequest>));
  AddUIThreadInterface(
      registry.get(),
      base::Bind(&ForwardShapeDetectionRequest<
                 shape_detection::mojom::FaceDetectionProviderRequest>));
  AddUIThreadInterface(
      registry.get(),
      base::Bind(&ForwardShapeDetectionRequest<
                 shape_detection::mojom::TextDetectionRequest>));

  AddUIThreadInterface(
      registry.get(),
      base::Bind(&PermissionServiceContext::CreateService,
                 base::Unretained(permission_service_context_.get())));

  AddUIThreadInterface(
      registry.get(),
      base::Bind(
          &PaymentAppContextImpl::CreatePaymentManager,
          base::Unretained(storage_partition_impl_->GetPaymentAppContext())));

  AddUIThreadInterface(
      registry.get(),
      base::Bind(&RenderProcessHostImpl::CreateOffscreenCanvasProvider,
                 base::Unretained(this)));

  AddUIThreadInterface(registry.get(),
                       base::Bind(&RenderProcessHostImpl::BindFrameSinkProvider,
                                  base::Unretained(this)));

  AddUIThreadInterface(
      registry.get(),
      base::Bind(&RenderProcessHostImpl::BindSharedBitmapAllocationNotifier,
                 base::Unretained(this)));

  AddUIThreadInterface(
      registry.get(),
      base::Bind(&BackgroundSyncContext::CreateService,
                 base::Unretained(
                     storage_partition_impl_->GetBackgroundSyncContext())));
  AddUIThreadInterface(
      registry.get(),
      base::Bind(&PlatformNotificationContextImpl::CreateService,
                 base::Unretained(
                     storage_partition_impl_->GetPlatformNotificationContext()),
                 GetID()));
  AddUIThreadInterface(
      registry.get(),
      base::Bind(&RenderProcessHostImpl::CreateStoragePartitionService,
                 base::Unretained(this)));
  AddUIThreadInterface(
      registry.get(),
      base::Bind(&BroadcastChannelProvider::Connect,
                 base::Unretained(
                     storage_partition_impl_->GetBroadcastChannelProvider())));
  if (base::FeatureList::IsEnabled(features::kMemoryCoordinator)) {
    AddUIThreadInterface(
        registry.get(), base::Bind(&CreateMemoryCoordinatorHandle, GetID()));
  }
  if (resource_coordinator::IsResourceCoordinatorEnabled()) {
    AddUIThreadInterface(registry.get(),
                         base::Bind(&CreateResourceCoordinatorProcessInterface,
                                    base::Unretained(this)));
  }

  registry->AddInterface(
      base::Bind(&MimeRegistryImpl::Create),
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_BLOCKING}));
#if BUILDFLAG(USE_MINIKIN_HYPHENATION)
  registry->AddInterface(base::Bind(&hyphenation::HyphenationImpl::Create),
                         hyphenation::HyphenationImpl::GetTaskRunner());
#endif

  registry->AddInterface(base::Bind(&device::GamepadMonitor::Create));

  registry->AddInterface(
      base::Bind(&PushMessagingManager::BindRequest,
                 base::Unretained(push_messaging_manager_.get())));

  registry->AddInterface(
      base::Bind(&BackgroundFetchServiceImpl::Create, GetID(),
                 make_scoped_refptr(
                     storage_partition_impl_->GetBackgroundFetchContext())));

  registry->AddInterface(base::Bind(&RenderProcessHostImpl::CreateMusGpuRequest,
                                    base::Unretained(this)));

  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();

  registry->AddInterface(
      base::Bind(&VideoCaptureHost::Create, GetID(), media_stream_manager));

#if BUILDFLAG(ENABLE_WEBRTC)
  registry->AddInterface(base::Bind(
      &RenderProcessHostImpl::CreateMediaStreamDispatcherHost,
      base::Unretained(this), GetBrowserContext()->GetMediaDeviceIDSalt(),
      media_stream_manager));
#endif

  registry->AddInterface(
      base::Bind(&metrics::CreateSingleSampleMetricsProvider));

  registry->AddInterface(
      base::Bind(&CreateReportingServiceProxy, storage_partition_impl_));

  // This is to support usage of WebSockets in cases in which there is no
  // associated RenderFrame (e.g., Shared Workers).
  AddUIThreadInterface(registry.get(),
                       base::Bind(&WebSocketManager::CreateWebSocket, GetID(),
                                  MSG_ROUTING_NONE));

  AddUIThreadInterface(registry.get(), base::Bind(&FieldTrialRecorder::Create));

  associated_interfaces_.reset(new AssociatedInterfaceRegistryImpl());
  GetContentClient()->browser()->ExposeInterfacesToRenderer(
      registry.get(), associated_interfaces_.get(), this);
  static_cast<AssociatedInterfaceRegistry*>(associated_interfaces_.get())
      ->AddInterface(base::Bind(&RenderProcessHostImpl::BindRouteProvider,
                                base::Unretained(this)));

  AddUIThreadInterface(registry.get(),
                       base::Bind(&RenderProcessHostImpl::CreateRendererHost,
                                  base::Unretained(this)));

  if (base::FeatureList::IsEnabled(features::kNetworkService)) {
    AddUIThreadInterface(
        registry.get(),
        base::Bind(&RenderProcessHostImpl::CreateURLLoaderFactory,
                   base::Unretained(this)));
  }

  if (base::FeatureList::IsEnabled(features::kMojoBlobs)) {
    registry->AddInterface(
        base::Bind(&BlobRegistryWrapper::Bind,
                   storage_partition_impl_->GetBlobRegistry(), GetID()));
  }

  ServiceManagerConnection* service_manager_connection =
      BrowserContext::GetServiceManagerConnectionFor(browser_context_);
  std::unique_ptr<ConnectionFilterImpl> connection_filter(
      new ConnectionFilterImpl(child_connection_->child_identity(),
                               std::move(registry)));
  connection_filter_controller_ = connection_filter->controller();
  connection_filter_id_ = service_manager_connection->AddConnectionFilter(
      std::move(connection_filter));
}

void RenderProcessHostImpl::BindRouteProvider(
    mojom::RouteProviderAssociatedRequest request) {
  if (route_provider_binding_.is_bound())
    return;
  route_provider_binding_.Bind(std::move(request));
}

void RenderProcessHostImpl::GetRoute(
    int32_t routing_id,
    mojom::AssociatedInterfaceProviderAssociatedRequest request) {
  DCHECK(request.is_pending());
  associated_interface_provider_bindings_.AddBinding(
      this, std::move(request), routing_id);
}

void RenderProcessHostImpl::GetAssociatedInterface(
    const std::string& name,
    mojom::AssociatedInterfaceAssociatedRequest request) {
  int32_t routing_id =
      associated_interface_provider_bindings_.dispatch_context();
  IPC::Listener* listener = listeners_.Lookup(routing_id);
  if (listener)
    listener->OnAssociatedInterfaceRequest(name, request.PassHandle());
}

void RenderProcessHostImpl::GetBlobURLLoaderFactory(
    mojom::URLLoaderFactoryRequest request) {
  if (!base::FeatureList::IsEnabled(features::kNetworkService)) {
    NOTREACHED();
    return;
  }
  storage_partition_impl_->GetBlobURLLoaderFactory()->HandleRequest(
      std::move(request));
}

void RenderProcessHostImpl::CreateMusGpuRequest(ui::mojom::GpuRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!gpu_client_)
    gpu_client_.reset(new GpuClient(GetID()));
  gpu_client_->Add(std::move(request));
}

void RenderProcessHostImpl::CreateOffscreenCanvasProvider(
    blink::mojom::OffscreenCanvasProviderRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!offscreen_canvas_provider_) {
    // The client id gets converted to a uint32_t in FrameSinkId.
    uint32_t renderer_client_id = base::checked_cast<uint32_t>(id_);
    offscreen_canvas_provider_ = base::MakeUnique<OffscreenCanvasProviderImpl>(
        GetHostFrameSinkManager(), renderer_client_id);
  }
  offscreen_canvas_provider_->Add(std::move(request));
}

void RenderProcessHostImpl::BindFrameSinkProvider(
    mojom::FrameSinkProviderRequest request) {
  frame_sink_provider_.Bind(std::move(request));
}

void RenderProcessHostImpl::BindSharedBitmapAllocationNotifier(
    viz::mojom::SharedBitmapAllocationNotifierRequest request) {
  shared_bitmap_allocation_notifier_impl_.Bind(std::move(request));
}

void RenderProcessHostImpl::CreateStoragePartitionService(
    mojom::StoragePartitionServiceRequest request) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMojoLocalStorage)) {
    if (g_create_storage_partition) {
      g_create_storage_partition(this, std::move(request));
      return;
    }

    storage_partition_impl_->Bind(id_, std::move(request));
  }
}

void RenderProcessHostImpl::CreateRendererHost(
    mojom::RendererHostRequest request) {
  renderer_host_binding_.Bind(std::move(request));
}

void RenderProcessHostImpl::CreateURLLoaderFactory(
    mojom::URLLoaderFactoryRequest request) {
  if (!base::FeatureList::IsEnabled(features::kNetworkService)) {
    NOTREACHED();
    return;
  }
  storage_partition_impl_->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(request), id_);
}

int RenderProcessHostImpl::GetNextRoutingID() {
  return widget_helper_->GetNextRoutingID();
}

void RenderProcessHostImpl::ResumeDeferredNavigation(
    const GlobalRequestID& request_id) {
  widget_helper_->ResumeDeferredNavigation(request_id);
}

void RenderProcessHostImpl::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  child_connection_->BindInterface(interface_name, std::move(interface_pipe));
}

const service_manager::Identity& RenderProcessHostImpl::GetChildIdentity()
    const {
  return child_connection_->child_identity();
}

std::unique_ptr<base::SharedPersistentMemoryAllocator>
RenderProcessHostImpl::TakeMetricsAllocator() {
  return std::move(metrics_allocator_);
}

const base::TimeTicks& RenderProcessHostImpl::GetInitTimeForNavigationMetrics()
    const {
  return init_time_;
}

bool RenderProcessHostImpl::IsProcessBackgrounded() const {
  return priority_.background;
}

void RenderProcessHostImpl::IncrementKeepAliveRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_keep_alive_ref_count_disabled_);
  ++keep_alive_ref_count_;
}

void RenderProcessHostImpl::DecrementKeepAliveRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_keep_alive_ref_count_disabled_);
  DCHECK_GT(keep_alive_ref_count_, 0U);
  --keep_alive_ref_count_;
  if (keep_alive_ref_count_ == 0)
    Cleanup();
}

void RenderProcessHostImpl::DisableKeepAliveRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_keep_alive_ref_count_disabled_);
  is_keep_alive_ref_count_disabled_ = true;
  if (!keep_alive_ref_count_)
    return;
  keep_alive_ref_count_ = 0;
  // Cleaning up will also remove this from the SpareRenderProcessHostManager.
  Cleanup();
}

bool RenderProcessHostImpl::IsKeepAliveRefCountDisabled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_keep_alive_ref_count_disabled_;
}

void RenderProcessHostImpl::PurgeAndSuspend() {
  Send(new ChildProcessMsg_PurgeAndSuspend());
}

void RenderProcessHostImpl::Resume() {
  Send(new ChildProcessMsg_Resume());
}

mojom::Renderer* RenderProcessHostImpl::GetRendererInterface() {
  return renderer_interface_.get();
}

resource_coordinator::ResourceCoordinatorInterface*
RenderProcessHostImpl::GetProcessResourceCoordinator() {
  if (process_resource_coordinator_)
    return process_resource_coordinator_.get();

  if (!resource_coordinator::IsResourceCoordinatorEnabled()) {
    process_resource_coordinator_ =
        base::MakeUnique<resource_coordinator::ResourceCoordinatorInterface>(
            nullptr, resource_coordinator::CoordinationUnitType::kProcess);
  } else {
    auto* connection = ServiceManagerConnection::GetForProcess();
    process_resource_coordinator_ =
        base::MakeUnique<resource_coordinator::ResourceCoordinatorInterface>(
            connection ? connection->GetConnector() : nullptr,
            resource_coordinator::CoordinationUnitType::kProcess);
  }
  return process_resource_coordinator_.get();
}

void RenderProcessHostImpl::SetIsNeverSuitableForReuse() {
  is_never_suitable_for_reuse_ = true;
}

bool RenderProcessHostImpl::MayReuseHost() {
  if (is_never_suitable_for_reuse_)
    return false;

  return GetContentClient()->browser()->MayReuseHost(this);
}

bool RenderProcessHostImpl::IsUnused() {
  return is_unused_;
}

void RenderProcessHostImpl::SetIsUsed() {
  is_unused_ = false;
}

mojom::RouteProvider* RenderProcessHostImpl::GetRemoteRouteProvider() {
  return remote_route_provider_.get();
}

void RenderProcessHostImpl::AddRoute(int32_t routing_id,
                                     IPC::Listener* listener) {
  CHECK(!listeners_.Lookup(routing_id)) << "Found Routing ID Conflict: "
                                        << routing_id;
  listeners_.AddWithID(listener, routing_id);
}

void RenderProcessHostImpl::RemoveRoute(int32_t routing_id) {
  DCHECK(listeners_.Lookup(routing_id) != nullptr);
  listeners_.Remove(routing_id);
  Cleanup();
}

void RenderProcessHostImpl::AddObserver(RenderProcessHostObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderProcessHostImpl::RemoveObserver(
    RenderProcessHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RenderProcessHostImpl::ShutdownForBadMessage(
    CrashReportMode crash_report_mode) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableKillAfterBadIPC))
    return;

  if (run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just
    // crash.
    CHECK(false);
  }

  // We kill the renderer but don't include a NOTREACHED, because we want the
  // browser to try to survive when it gets illegal messages from the renderer.
  Shutdown(RESULT_CODE_KILLED_BAD_MESSAGE, false);

  if (crash_report_mode == CrashReportMode::GENERATE_CRASH_DUMP) {
    // Report a crash, since none will be generated by the killed renderer.
    base::debug::DumpWithoutCrashing();
  }

  // Log the renderer kill to the histogram tracking all kills.
  BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
      PROCESS_TYPE_RENDERER);
}

void RenderProcessHostImpl::WidgetRestored() {
  visible_widgets_++;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::WidgetHidden() {
  // On startup, the browser will call Hide. We ignore this call.
  if (visible_widgets_ == 0)
    return;

  --visible_widgets_;
  if (visible_widgets_ == 0) {
    UpdateProcessPriority();
  }
}

int RenderProcessHostImpl::VisibleWidgetCount() const {
  return visible_widgets_;
}

#if defined(OS_ANDROID)
void RenderProcessHostImpl::UpdateWidgetImportance(
    ChildProcessImportance old_value,
    ChildProcessImportance new_value) {
  DCHECK_NE(old_value, new_value);
  DCHECK(widget_importance_counts_[static_cast<size_t>(old_value)]);
  widget_importance_counts_[static_cast<size_t>(old_value)]--;
  widget_importance_counts_[static_cast<size_t>(new_value)]++;
  UpdateProcessPriority();
}

ChildProcessImportance RenderProcessHostImpl::ComputeEffectiveImportance() {
  ChildProcessImportance importance = ChildProcessImportance::NORMAL;
  for (size_t i = 0u; i < arraysize(widget_importance_counts_); ++i) {
    DCHECK_GE(widget_importance_counts_[i], 0);
    if (widget_importance_counts_[i]) {
      // No early out. Highest importance wins.
      importance = static_cast<ChildProcessImportance>(i);
    }
  }
  return importance;
}
#endif

RendererAudioOutputStreamFactoryContext*
RenderProcessHostImpl::GetRendererAudioOutputStreamFactoryContext() {
  if (!audio_output_stream_factory_context_) {
    media::AudioManager* audio_manager =
        BrowserMainLoop::GetInstance()->audio_manager();
    MediaStreamManager* media_stream_manager =
        BrowserMainLoop::GetInstance()->media_stream_manager();
    media::AudioSystem* audio_system =
        BrowserMainLoop::GetInstance()->audio_system();
    std::string salt = GetBrowserContext()->GetMediaDeviceIDSalt();
    audio_output_stream_factory_context_.reset(
        new RendererAudioOutputStreamFactoryContextImpl(
            GetID(), audio_system, audio_manager, media_stream_manager, salt));
  }
  return audio_output_stream_factory_context_.get();
}

void RenderProcessHostImpl::OnMediaStreamAdded() {
  ++media_stream_count_;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::OnMediaStreamRemoved() {
  DCHECK_GT(media_stream_count_, 0);
  --media_stream_count_;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::set_render_process_host_factory(
    const RenderProcessHostFactory* rph_factory) {
  g_render_process_host_factory_ = rph_factory;
}

// static
void RenderProcessHostImpl::AddFrameWithSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const GURL& site_url) {
  if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                 site_url))
    return;

  SiteProcessCountTracker* tracker = static_cast<SiteProcessCountTracker*>(
      browser_context->GetUserData(kCommittedSiteProcessCountTrackerKey));
  if (!tracker) {
    tracker = new SiteProcessCountTracker();
    browser_context->SetUserData(kCommittedSiteProcessCountTrackerKey,
                                 base::WrapUnique(tracker));
  }
  tracker->IncrementSiteProcessCount(site_url, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::RemoveFrameWithSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const GURL& site_url) {
  if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                 site_url))
    return;

  SiteProcessCountTracker* tracker = static_cast<SiteProcessCountTracker*>(
      browser_context->GetUserData(kCommittedSiteProcessCountTrackerKey));
  if (!tracker) {
    tracker = new SiteProcessCountTracker();
    browser_context->SetUserData(kCommittedSiteProcessCountTrackerKey,
                                 base::WrapUnique(tracker));
  }
  tracker->DecrementSiteProcessCount(site_url, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::AddExpectedNavigationToSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const GURL& site_url) {
  if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                 site_url))
    return;

  SiteProcessCountTracker* tracker = static_cast<SiteProcessCountTracker*>(
      browser_context->GetUserData(kPendingSiteProcessCountTrackerKey));
  if (!tracker) {
    tracker = new SiteProcessCountTracker();
    browser_context->SetUserData(kPendingSiteProcessCountTrackerKey,
                                 base::WrapUnique(tracker));
  }
  tracker->IncrementSiteProcessCount(site_url, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::RemoveExpectedNavigationToSite(
    BrowserContext* browser_context,
    RenderProcessHost* render_process_host,
    const GURL& site_url) {
  if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                 site_url))
    return;

  SiteProcessCountTracker* tracker = static_cast<SiteProcessCountTracker*>(
      browser_context->GetUserData(kPendingSiteProcessCountTrackerKey));
  if (!tracker) {
    tracker = new SiteProcessCountTracker();
    browser_context->SetUserData(kPendingSiteProcessCountTrackerKey,
                                 base::WrapUnique(tracker));
  }
  tracker->DecrementSiteProcessCount(site_url, render_process_host->GetID());
}

// static
void RenderProcessHostImpl::CleanupSpareRenderProcessHost() {
  g_spare_render_process_host_manager.Get().CleanupSpareRenderProcessHost();
}

// static
RenderProcessHost*
RenderProcessHostImpl::GetSpareRenderProcessHostForTesting() {
  return g_spare_render_process_host_manager.Get().spare_render_process_host();
}

bool RenderProcessHostImpl::HostHasNotBeenUsed() {
  return IsUnused() && listeners_.IsEmpty() && keep_alive_ref_count_ == 0 &&
         pending_views_ == 0;
}

bool RenderProcessHostImpl::IsForGuestsOnly() const {
  return is_for_guests_only_;
}

StoragePartition* RenderProcessHostImpl::GetStoragePartition() const {
  return storage_partition_impl_;
}

static void AppendCompositorCommandLineFlags(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      switches::kNumRasterThreads,
      base::IntToString(NumberOfRendererRasterThreads()));

  int msaa_sample_count = GpuRasterizationMSAASampleCount();
  if (msaa_sample_count >= 0) {
    command_line->AppendSwitchASCII(switches::kGpuRasterizationMSAASampleCount,
                                    base::IntToString(msaa_sample_count));
  }

  if (IsZeroCopyUploadEnabled())
    command_line->AppendSwitch(switches::kEnableZeroCopy);
  if (!IsPartialRasterEnabled())
    command_line->AppendSwitch(switches::kDisablePartialRaster);

  if (IsGpuMemoryBufferCompositorResourcesEnabled()) {
    command_line->AppendSwitch(
        switches::kEnableGpuMemoryBufferCompositorResources);
  }

  if (IsMainFrameBeforeActivationEnabled())
    command_line->AppendSwitch(cc::switches::kEnableMainFrameBeforeActivation);

  if (IsCheckerImagingEnabled())
    command_line->AppendSwitch(cc::switches::kEnableCheckerImaging);

  command_line->AppendSwitchASCII(
      switches::kContentImageTextureTarget,
      viz::BufferToTextureTargetMapToString(CreateBufferToTextureTargetMap()));

  // Appending disable-gpu-feature switches due to software rendering list.
  GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  DCHECK(gpu_data_manager);
  gpu_data_manager->AppendRendererCommandLine(command_line);

  // Slimming Paint v2 implies layer lists in the renderer.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSlimmingPaintV2)) {
    command_line->AppendSwitch(cc::switches::kEnableLayerLists);
  }
}

void RenderProcessHostImpl::AppendRendererCommandLine(
    base::CommandLine* command_line) {
  // Pass the process type first, so it shows first in process listings.
  command_line->AppendSwitchASCII(switches::kProcessType,
                                  switches::kRendererProcess);

#if defined(OS_WIN)
  command_line->AppendArg(switches::kPrefetchArgumentRenderer);
#endif  // defined(OS_WIN)

  // Now send any options from our own command line we want to propagate.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  PropagateBrowserCommandLineToRenderer(browser_command_line, command_line);

  // Pass on the browser locale.
  const std::string locale =
      GetContentClient()->browser()->GetApplicationLocale();
  command_line->AppendSwitchASCII(switches::kLang, locale);

  // A non-empty RendererCmdPrefix implies that Zygote is disabled.
  if (!base::CommandLine::ForCurrentProcess()
           ->GetSwitchValueNative(switches::kRendererCmdPrefix)
           .empty()) {
    command_line->AppendSwitch(switches::kNoZygote);
  }

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(command_line,
                                                                GetID());

  if (IsPinchToZoomEnabled())
    command_line->AppendSwitch(switches::kEnablePinch);

#if defined(OS_WIN)
  command_line->AppendSwitchASCII(
      switches::kDeviceScaleFactor,
      base::DoubleToString(display::win::GetDPIScale()));
#endif

  AppendCompositorCommandLineFlags(command_line);

  command_line->AppendSwitchASCII(switches::kServiceRequestChannelToken,
                                  child_connection_->service_token());
  command_line->AppendSwitchASCII(switches::kRendererClientId,
                                  std::to_string(GetID()));
}

void RenderProcessHostImpl::PropagateBrowserCommandLineToRenderer(
    const base::CommandLine& browser_cmd,
    base::CommandLine* renderer_cmd) {
  // Propagate the following switches to the renderer command line (along
  // with any associated values) if present in the browser command line.
  static const char* const kSwitchNames[] = {
    service_manager::switches::kDisableInProcessStackTraces,
    switches::kAgcStartupMinVolume,
    switches::kAecRefinedAdaptiveFilter,
    switches::kAllowLoopbackInPeerConnection,
    switches::kAndroidFontsPath,
    switches::kAudioBufferSize,
    switches::kAutoplayPolicy,
    switches::kBlinkSettings,
    switches::kDefaultTileWidth,
    switches::kDefaultTileHeight,
    switches::kDisable2dCanvasImageChromium,
    switches::kDisable3DAPIs,
    switches::kDisableAcceleratedJpegDecoding,
    switches::kDisableAcceleratedVideoDecode,
    switches::kDisableBackgroundTimerThrottling,
    switches::kDisableBreakpad,
    switches::kDisablePreferCompositingToLCDText,
    switches::kDisableDatabases,
    switches::kDisableDisplayList2dCanvas,
    switches::kDisableDistanceFieldText,
    switches::kDisableFileSystem,
    switches::kDisableGestureRequirementForPresentation,
    switches::kDisableGpuCompositing,
    switches::kDisableGpuMemoryBufferVideoFrames,
    switches::kDisableGpuVsync,
    switches::kDisableLowResTiling,
    switches::kDisableHistogramCustomizer,
    switches::kDisableLCDText,
    switches::kDisableLocalStorage,
    switches::kDisableLogging,
    switches::kDisableMediaSuspend,
    switches::kDisableNotifications,
    switches::kDisableOriginTrialControlledBlinkFeatures,
    switches::kDisablePepper3DImageChromium,
    switches::kDisablePermissionsAPI,
    switches::kDisablePresentationAPI,
    switches::kDisablePinch,
    switches::kDisableRGBA4444Textures,
    switches::kDisableRTCSmoothnessAlgorithm,
    switches::kDisableSeccompFilterSandbox,
    switches::kDisableSharedWorkers,
    switches::kDisableSkiaRuntimeOpts,
    switches::kDisableSlimmingPaintInvalidation,
    switches::kDisableSmoothScrolling,
    switches::kDisableSpeechAPI,
    switches::kDisableThreadedCompositing,
    switches::kDisableThreadedScrolling,
    switches::kDisableTouchAdjustment,
    switches::kDisableTouchDragDrop,
    switches::kDisableV8IdleTasks,
    switches::kDisableWebGLImageChromium,
    switches::kDomAutomationController,
    switches::kEnableBrowserSideNavigation,
    switches::kEnableDisplayList2dCanvas,
    switches::kEnableDistanceFieldText,
    switches::kEnableExperimentalCanvasFeatures,
    switches::kEnableExperimentalWebPlatformFeatures,
    switches::kEnableHeapProfiling,
    switches::kEnableGPUClientLogging,
    switches::kEnableGpuClientTracing,
    switches::kEnableGpuMemoryBufferVideoFrames,
    switches::kEnableGPUServiceLogging,
    switches::kEnableLowResTiling,
    switches::kEnableMediaSuspend,
    switches::kEnableInbandTextTracks,
    switches::kEnableLCDText,
    switches::kEnableLogging,
    switches::kEnableNetworkInformationDownlinkMax,
    switches::kEnableOOPRasterization,
    switches::kEnablePinch,
    switches::kEnablePluginPlaceholderTesting,
    switches::kEnablePreciseMemoryInfo,
    switches::kEnablePrintBrowser,
    switches::kEnablePreferCompositingToLCDText,
    switches::kEnableRGBA4444Textures,
    switches::kEnableSkiaBenchmarking,
    switches::kEnableSlimmingPaintV2,
    switches::kEnableSlimmingPaintInvalidation,
    switches::kEnableSmoothScrolling,
    switches::kEnableStatsTable,
    switches::kEnableThreadedCompositing,
    switches::kEnableTouchDragDrop,
    switches::kEnableUseZoomForDSF,
    switches::kEnableViewport,
    switches::kEnableVtune,
    switches::kEnableWebFontsInterventionTrigger,
    switches::kEnableWebFontsInterventionV2,
    switches::kEnableWebGLDraftExtensions,
    switches::kEnableWebGLImageChromium,
    switches::kEnableWebVR,
    switches::kExplicitlyAllowedPorts,
    switches::kForceColorProfile,
    switches::kForceDeviceScaleFactor,
    switches::kForceDisplayList2dCanvas,
    switches::kForceGpuMemAvailableMb,
    switches::kForceGpuRasterization,
    switches::kForceOverlayFullscreenVideo,
    switches::kForceVideoOverlays,
    switches::kFullMemoryCrashReport,
    switches::kIgnoreAutoplayRestrictionsForTests,
    switches::kIPCConnectionTimeout,
    switches::kIsolateOrigins,
    switches::kIsRunningInMash,
    switches::kJavaScriptFlags,
    switches::kLoggingLevel,
    switches::kMainFrameResizesAreOrientationChanges,
    switches::kMaxUntiledLayerWidth,
    switches::kMaxUntiledLayerHeight,
    switches::kDisableMojoLocalStorage,
    switches::kMSEAudioBufferSizeLimit,
    switches::kMSEVideoBufferSizeLimit,
    switches::kNoReferrers,
    switches::kNoSandbox,
    switches::kNoZygote,
    switches::kOverridePluginPowerSaverForTesting,
    switches::kPassiveListenersDefault,
    switches::kPpapiInProcess,
    switches::kProfilerTiming,
    switches::kReducedReferrerGranularity,
    switches::kReduceSecurityForTesting,
    switches::kRegisterPepperPlugins,
    switches::kRendererStartupDialog,
    switches::kRootLayerScrolls,
    switches::kShowPaintRects,
    switches::kSitePerProcess,
    switches::kStatsCollectionController,
    switches::kTestType,
    switches::kTouchEventFeatureDetection,
    switches::kTouchTextSelectionStrategy,
    switches::kTraceConfigFile,
    switches::kTraceToConsole,
    switches::kUseFakeUIForMediaStream,
    // This flag needs to be propagated to the renderer process for
    // --in-process-webgl.
    switches::kUseGL,
    switches::kUseGpuInTests,
    switches::kUseMobileUserAgent,
    switches::kV,
    switches::kV8CacheStrategiesForCacheStorage,
    switches::kVideoThreads,
    switches::kVideoUnderflowThresholdMs,
    switches::kVModule,
    // Please keep these in alphabetical order. Compositor switches here should
    // also be added to chrome/browser/chromeos/login/chrome_restart_request.cc.
    cc::switches::kDisableCompositedAntialiasing,
    cc::switches::kDisableThreadedAnimation,
    cc::switches::kEnableGpuBenchmarking,
    cc::switches::kEnableLayerLists,
    cc::switches::kEnableTileCompression,
    cc::switches::kShowCompositedLayerBorders,
    cc::switches::kShowFPSCounter,
    cc::switches::kShowLayerAnimationBounds,
    cc::switches::kShowPropertyChangedRects,
    cc::switches::kShowScreenSpaceRects,
    cc::switches::kShowSurfaceDamageRects,
    cc::switches::kSlowDownRasterScaleFactor,
    cc::switches::kBrowserControlsHideThreshold,
    cc::switches::kBrowserControlsShowThreshold,
    cc::switches::kRunAllCompositorStagesBeforeDraw,
    switches::kDisableSurfaceReferences,
    switches::kEnableSurfaceSynchronization,

#if BUILDFLAG(ENABLE_PLUGINS)
    switches::kEnablePepperTesting,
#endif
#if BUILDFLAG(ENABLE_WEBRTC)
    switches::kDisableWebRtcHWDecoding,
    switches::kDisableWebRtcHWEncoding,
    switches::kEnableWebRtcSrtpAesGcm,
    switches::kEnableWebRtcSrtpEncryptedHeaders,
    switches::kEnableWebRtcStunOrigin,
    switches::kEnforceWebRtcIPPermissionCheck,
    switches::kForceWebRtcIPHandlingPolicy,
    switches::kWebRtcMaxCaptureFramerate,
#endif
    switches::kEnableLowEndDeviceMode,
    switches::kDisableLowEndDeviceMode,
    switches::kDisallowNonExactResourceReuse,
#if defined(OS_ANDROID)
    switches::kDisableMediaSessionAPI,
    switches::kRendererWaitForJavaDebugger,
#endif
#if defined(OS_MACOSX)
    // Allow this to be set when invoking the browser and relayed along.
    switches::kEnableSandboxLogging,
#endif
#if defined(OS_WIN)
    switches::kDisableWin32kLockDown,
    switches::kEnableWin7WebRtcHWH264Decoding,
    switches::kTrySupportedChannelLayouts,
    switches::kTraceExportEventsToETW,
#endif
#if defined(USE_OZONE)
    switches::kOzonePlatform,
#endif
#if defined(OS_CHROMEOS)
    switches::kDisableVaapiAcceleratedVideoEncode,
#endif
#if defined(ENABLE_IPC_FUZZER)
    switches::kIpcDumpDirectory,
    switches::kIpcFuzzerTestcase,
#endif
  };
  renderer_cmd->CopySwitchesFrom(browser_cmd, kSwitchNames,
                                 arraysize(kSwitchNames));

  BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(renderer_cmd);

  if (browser_cmd.HasSwitch(switches::kTraceStartup) &&
      BrowserMainLoop::GetInstance()->is_tracing_startup_for_duration()) {
    // Pass kTraceStartup switch to renderer only if startup tracing has not
    // finished.
    renderer_cmd->AppendSwitchASCII(
        switches::kTraceStartup,
        browser_cmd.GetSwitchValueASCII(switches::kTraceStartup));
  }

#if BUILDFLAG(ENABLE_WEBRTC)
  // Only run the Stun trials in the first renderer.
  if (!has_done_stun_trials &&
      browser_cmd.HasSwitch(switches::kWebRtcStunProbeTrialParameter)) {
    has_done_stun_trials = true;
    renderer_cmd->AppendSwitchASCII(
        switches::kWebRtcStunProbeTrialParameter,
        browser_cmd.GetSwitchValueASCII(
            switches::kWebRtcStunProbeTrialParameter));
  }
#endif

  // Disable databases in incognito mode.
  if (GetBrowserContext()->IsOffTheRecord() &&
      !browser_cmd.HasSwitch(switches::kDisableDatabases)) {
    renderer_cmd->AppendSwitch(switches::kDisableDatabases);
  }

  // Add kWaitForDebugger to let renderer process wait for a debugger.
  if (browser_cmd.HasSwitch(switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::string value =
        browser_cmd.GetSwitchValueASCII(switches::kWaitForDebuggerChildren);
    if (value.empty() || value == switches::kRendererProcess) {
      renderer_cmd->AppendSwitch(switches::kWaitForDebugger);
    }
  }

  DCHECK(child_connection_);
  renderer_cmd->AppendSwitchASCII(service_manager::switches::kServicePipeToken,
                                  child_connection_->service_token());

#if defined(OS_WIN) && !defined(OFFICIAL_BUILD)
  // Needed because we can't show the dialog from the sandbox. Don't pass
  // --no-sandbox in official builds because that would bypass the bad_flgs
  // prompt.
  if (renderer_cmd->HasSwitch(switches::kRendererStartupDialog) &&
      !renderer_cmd->HasSwitch(switches::kNoSandbox)) {
    renderer_cmd->AppendSwitch(switches::kNoSandbox);
  }
#endif

  CopyFeatureSwitch(browser_cmd, renderer_cmd, switches::kEnableBlinkFeatures);
  CopyFeatureSwitch(browser_cmd, renderer_cmd, switches::kDisableBlinkFeatures);
}

base::ProcessHandle RenderProcessHostImpl::GetHandle() const {
  if (run_renderer_in_process())
    return base::GetCurrentProcessHandle();

  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting())
    return base::kNullProcessHandle;

  return child_process_launcher_->GetProcess().Handle();
}

bool RenderProcessHostImpl::IsReady() const {
  // The process launch result (that sets GetHandle()) and the channel
  // connection (that sets channel_connected_) can happen in either order.
  return GetHandle() && channel_connected_;
}

bool RenderProcessHostImpl::Shutdown(int exit_code, bool wait) {
  if (run_renderer_in_process())
    return false;  // Single process mode never shuts down the renderer.

  if (!child_process_launcher_.get())
    return false;

  return child_process_launcher_->Terminate(exit_code, wait);
}

bool RenderProcessHostImpl::FastShutdownIfPossible(size_t page_count,
                                                   bool skip_unload_handlers) {
  if (page_count && GetActiveViewCount() != page_count)
    return false;

  if (run_renderer_in_process())
    return false;  // Single process mode never shuts down the renderer.

  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting() ||
      !GetHandle())
    return false;  // Render process hasn't started or is probably crashed.

  // Test if there's an unload listener.
  // NOTE: It's possible that an onunload listener may be installed
  // while we're shutting down, so there's a small race here.  Given that
  // the window is small, it's unlikely that the web page has much
  // state that will be lost by not calling its unload handlers properly.
  if (!skip_unload_handlers && !SuddenTerminationAllowed())
    return false;

  if (keep_alive_ref_count_ != 0) {
    if (keep_alive_start_time_.is_null())
      keep_alive_start_time_ = base::TimeTicks::Now();
    return false;
  }

  // Set this before ProcessDied() so observers can tell if the render process
  // died due to fast shutdown versus another cause.
  fast_shutdown_started_ = true;

  ProcessDied(false /* already_dead */, nullptr);
  return true;
}

bool RenderProcessHostImpl::Send(IPC::Message* msg) {
  TRACE_EVENT0("renderer_host", "RenderProcessHostImpl::Send");

  std::unique_ptr<IPC::Message> message(msg);

  // |channel_| is only null after Cleanup(), at which point we don't care about
  // delivering any messages.
  if (!channel_)
    return false;

#if !defined(OS_ANDROID)
  DCHECK(!message->is_sync());
#else
  if (message->is_sync()) {
    // If Init() hasn't been called yet since construction or the last
    // ProcessDied() we avoid blocking on sync IPC.
    if (!HasConnection())
      return false;

    // Likewise if we've done Init(), but process launch has not yet completed,
    // we avoid blocking on sync IPC.
    if (child_process_launcher_.get() && child_process_launcher_->IsStarting())
      return false;
  }
#endif

  return channel_->Send(message.release());
}

bool RenderProcessHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // If we're about to be deleted, or have initiated the fast shutdown sequence,
  // we ignore incoming messages.

  if (deleting_soon_ || fast_shutdown_started_)
    return false;

  mark_child_process_activity_time();
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // Dispatch control messages.
    IPC_BEGIN_MESSAGE_MAP(RenderProcessHostImpl, msg)
      IPC_MESSAGE_HANDLER(ChildProcessHostMsg_ShutdownRequest,
                          OnShutdownRequest)
      IPC_MESSAGE_HANDLER(RenderProcessHostMsg_SuddenTerminationChanged,
                          SuddenTerminationChanged)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UserMetricsRecordAction,
                          OnUserMetricsRecordAction)
      IPC_MESSAGE_HANDLER(ViewHostMsg_Close_ACK, OnCloseACK)
#if BUILDFLAG(ENABLE_WEBRTC)
      IPC_MESSAGE_HANDLER(AecDumpMsg_RegisterAecDumpConsumer,
                          OnRegisterAecDumpConsumer)
      IPC_MESSAGE_HANDLER(AecDumpMsg_UnregisterAecDumpConsumer,
                          OnUnregisterAecDumpConsumer)
#endif
    // Adding single handlers for your service here is fine, but once your
    // service needs more than one handler, please extract them into a new
    // message filter and add that filter to CreateMessageFilters().
    IPC_END_MESSAGE_MAP()

    return true;
  }

  // Dispatch incoming messages to the appropriate IPC::Listener.
  IPC::Listener* listener = listeners_.Lookup(msg.routing_id());
  if (!listener) {
    if (msg.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }
    return true;
  }
  return listener->OnMessageReceived(msg);
}

void RenderProcessHostImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (associated_interfaces_ &&
      associated_interfaces_->CanBindRequest(interface_name)) {
    associated_interfaces_->BindRequest(interface_name, std::move(handle));
  } else {
    LOG(ERROR) << "Request for unknown Channel-associated interface: "
               << interface_name;
  }
}

void RenderProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  channel_connected_ = true;
  if (IsReady()) {
    DCHECK(!sent_render_process_ready_);
    sent_render_process_ready_ = true;
    // Send RenderProcessReady only if we already received the process handle.
    for (auto& observer : observers_)
      observer.RenderProcessReady(this);
  }

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  Send(new ChildProcessMsg_SetIPCLoggingEnabled(
      IPC::Logging::GetInstance()->Enabled()));
#endif

  tracked_objects::ThreadData::Status status =
      tracked_objects::ThreadData::status();
  Send(new ChildProcessMsg_SetProfilerStatus(status));

  // Inform AudioInputRendererHost about the new render process PID.
  // AudioInputRendererHost is reference counted, so its lifetime is
  // guaranteed during the lifetime of the closure.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&AudioInputRendererHost::set_renderer_pid,
                     audio_input_renderer_host_, peer_pid));
}

void RenderProcessHostImpl::OnChannelError() {
  ProcessDied(true /* already_dead */, nullptr);
}

void RenderProcessHostImpl::OnBadMessageReceived(const IPC::Message& message) {
  // Message de-serialization failed. We consider this a capital crime. Kill the
  // renderer if we have one.
  auto type = message.type();
  LOG(ERROR) << "bad message " << type << " terminating renderer.";

  // The ReceivedBadMessage call below will trigger a DumpWithoutCrashing. Alias
  // enough information here so that we can determine what the bad message was.
  base::debug::Alias(&type);

  bad_message::ReceivedBadMessage(this,
                                  bad_message::RPH_DESERIALIZATION_FAILED);
}

BrowserContext* RenderProcessHostImpl::GetBrowserContext() const {
  return browser_context_;
}

bool RenderProcessHostImpl::InSameStoragePartition(
    StoragePartition* partition) const {
  return storage_partition_impl_ == partition;
}

int RenderProcessHostImpl::GetID() const {
  return id_;
}

bool RenderProcessHostImpl::HasConnection() const {
  return is_initialized_ && !is_dead_;
}

void RenderProcessHostImpl::SetIgnoreInputEvents(bool ignore_input_events) {
  if (ignore_input_events == ignore_input_events_)
    return;

  ignore_input_events_ = ignore_input_events;
  for (auto* widget : widgets_) {
    widget->ProcessIgnoreInputEventsChanged(ignore_input_events);
  }
}

bool RenderProcessHostImpl::IgnoreInputEvents() const {
  return ignore_input_events_;
}

void RenderProcessHostImpl::Cleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Keep the one renderer thread around forever in single process mode.
  if (run_renderer_in_process())
    return;

  // If within_process_died_observer_ is true, one of our observers performed an
  // action that caused us to die (e.g. http://crbug.com/339504). Therefore,
  // delay the destruction until all of the observer callbacks have been made,
  // and guarantee that the RenderProcessHostDestroyed observer callback is
  // always the last callback fired.
  if (within_process_died_observer_) {
    delayed_cleanup_needed_ = true;
    return;
  }
  delayed_cleanup_needed_ = false;

  // Records the time when the process starts kept alive by the ref count for
  // UMA.
  if (listeners_.IsEmpty() && keep_alive_ref_count_ > 0 &&
      keep_alive_start_time_.is_null()) {
    keep_alive_start_time_ = base::TimeTicks::Now();
  }

  // Until there are no other owners of this object, we can't delete ourselves.
  if (!listeners_.IsEmpty() || keep_alive_ref_count_ != 0)
    return;

#if BUILDFLAG(ENABLE_WEBRTC)
  if (is_initialized_)
    ClearWebRtcLogMessageCallback();
#endif

  if (!keep_alive_start_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("BrowserRenderProcessHost.KeepAliveDuration",
                             base::TimeTicks::Now() - keep_alive_start_time_);
  }

  // We cannot clean up twice; if this fails, there is an issue with our
  // control flow.
  DCHECK(!deleting_soon_);

  DCHECK_EQ(0, pending_views_);

  // If the process associated with this RenderProcessHost is still alive,
  // notify all observers that the process has exited cleanly, even though it
  // will be destroyed a bit later. Observers shouldn't rely on this process
  // anymore.
  if (HasConnection()) {
    for (auto& observer : observers_) {
      observer.RenderProcessExited(
          this, base::TERMINATION_STATUS_NORMAL_TERMINATION, 0);
    }
  }
  for (auto& observer : observers_)
    observer.RenderProcessHostDestroyed(this);
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_TERMINATED,
      Source<RenderProcessHost>(this), NotificationService::NoDetails());

  if (connection_filter_id_ !=
        ServiceManagerConnection::kInvalidConnectionFilterId) {
    ServiceManagerConnection* service_manager_connection =
        BrowserContext::GetServiceManagerConnectionFor(browser_context_);
    connection_filter_controller_->DisableFilter();
    service_manager_connection->RemoveConnectionFilter(connection_filter_id_);
    connection_filter_id_ =
        ServiceManagerConnection::kInvalidConnectionFilterId;
  }

#ifndef NDEBUG
  is_self_deleted_ = true;
#endif
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  deleting_soon_ = true;

  // It's important not to wait for the DeleteTask to delete the channel
  // proxy. Kill it off now. That way, in case the profile is going away, the
  // rest of the objects attached to this RenderProcessHost start going
  // away first, since deleting the channel proxy will post a
  // OnChannelClosed() to IPC::ChannelProxy::Context on the IO thread.
  ResetChannelProxy();

  // Its important to remove the kSessionStorageHolder after the channel
  // has been reset to avoid deleting the underlying namespaces prior
  // to processing ipcs referring to them.
  DCHECK(!channel_);
  RemoveUserData(kSessionStorageHolderKey);

  // Remove ourself from the list of renderer processes so that we can't be
  // reused in between now and when the Delete task runs.
  UnregisterHost(GetID());

  instance_weak_factory_.reset(
      new base::WeakPtrFactory<RenderProcessHostImpl>(this));
}

void RenderProcessHostImpl::AddPendingView() {
  pending_views_++;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::RemovePendingView() {
  DCHECK(pending_views_);
  pending_views_--;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::AddWidget(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* widget_impl =
      static_cast<RenderWidgetHostImpl*>(widget);
  widgets_.insert(widget_impl);
#if defined(OS_ANDROID)
  widget_importance_counts_[static_cast<size_t>(widget_impl->importance())]++;
  UpdateProcessPriority();
#endif
}

void RenderProcessHostImpl::RemoveWidget(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* widget_impl =
      static_cast<RenderWidgetHostImpl*>(widget);
  widgets_.erase(widget_impl);

#if defined(OS_ANDROID)
  ChildProcessImportance importance = widget_impl->importance();
  DCHECK(widget_importance_counts_[static_cast<size_t>(importance)]);
  widget_importance_counts_[static_cast<size_t>(importance)]--;
  UpdateProcessPriority();
#endif
}

void RenderProcessHostImpl::SetSuddenTerminationAllowed(bool enabled) {
  sudden_termination_allowed_ = enabled;
}

bool RenderProcessHostImpl::SuddenTerminationAllowed() const {
  return sudden_termination_allowed_;
}

base::TimeDelta RenderProcessHostImpl::GetChildProcessIdleTime() const {
  return base::TimeTicks::Now() - child_process_activity_time_;
}

void RenderProcessHostImpl::FilterURL(bool empty_allowed, GURL* url) {
  FilterURL(this, empty_allowed, url);
}

#if BUILDFLAG(ENABLE_WEBRTC)
void RenderProcessHostImpl::EnableAudioDebugRecordings(
    const base::FilePath& file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Enable AEC dump for each registered consumer.
  base::FilePath file_with_extensions = GetAecDumpFilePathWithExtensions(file);
  for (std::vector<int>::iterator it = aec_dump_consumers_.begin();
       it != aec_dump_consumers_.end(); ++it) {
    EnableAecDumpForId(file_with_extensions, *it);
  }

  // Enable mic input recording. AudioInputRendererHost is reference counted, so
  // its lifetime is guaranteed during the lifetime of the closure.
  if (audio_input_renderer_host_) {
    // Not null if RenderProcessHostImpl::Init has already been called.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&AudioInputRendererHost::EnableDebugRecording,
                       audio_input_renderer_host_, file));
  }
}

void RenderProcessHostImpl::DisableAudioDebugRecordings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Posting on the sequence and then replying back on the UI thread is only
  // for avoiding races between enable and disable. Nothing is done on the
  // sequence.
  GetAecDumpFileTaskRunner().PostTaskAndReply(
      FROM_HERE, base::BindOnce(&base::DoNothing),
      base::BindOnce(&RenderProcessHostImpl::SendDisableAecDumpToRenderer,
                     weak_factory_.GetWeakPtr()));

  // AudioInputRendererHost is reference counted, so it's lifetime is
  // guaranteed during the lifetime of the closure.
  if (audio_input_renderer_host_) {
    // Not null if RenderProcessHostImpl::Init has already been called.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&AudioInputRendererHost::DisableDebugRecording,
                       audio_input_renderer_host_));
  }
}

bool RenderProcessHostImpl::StartWebRTCEventLog(
    const base::FilePath& file_path) {
  return webrtc_eventlog_host_.StartWebRTCEventLog(file_path);
}

bool RenderProcessHostImpl::StopWebRTCEventLog() {
  return webrtc_eventlog_host_.StopWebRTCEventLog();
}

void RenderProcessHostImpl::SetEchoCanceller3(bool enable) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(hlundin) Implement a test to verify that the setting works both with
  // aec_dump_consumers already registered, and with those registered in the
  // future. crbug.com/740104
  override_aec3_ = enable;

  // Piggybacking on AEC dumps.
  // TODO(hlundin): Change name for aec_dump_consumers_;
  // http://crbug.com/709919.
  for (std::vector<int>::iterator it = aec_dump_consumers_.begin();
       it != aec_dump_consumers_.end(); ++it) {
    Send(new AudioProcessingMsg_EnableAec3(*it, enable));
  }
}

void RenderProcessHostImpl::SetWebRtcLogMessageCallback(
    base::Callback<void(const std::string&)> callback) {
#if BUILDFLAG(ENABLE_WEBRTC)
  BrowserMainLoop::GetInstance()->media_stream_manager()->
      RegisterNativeLogCallback(GetID(), callback);
#endif
}

void RenderProcessHostImpl::ClearWebRtcLogMessageCallback() {
#if BUILDFLAG(ENABLE_WEBRTC)
  BrowserMainLoop::GetInstance()
      ->media_stream_manager()
      ->UnregisterNativeLogCallback(GetID());
#endif
}

RenderProcessHostImpl::WebRtcStopRtpDumpCallback
RenderProcessHostImpl::StartRtpDump(
    bool incoming,
    bool outgoing,
    const WebRtcRtpPacketCallback& packet_callback) {
  if (!p2p_socket_dispatcher_host_.get())
    return WebRtcStopRtpDumpCallback();

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(&P2PSocketDispatcherHost::StartRtpDump,
                                         p2p_socket_dispatcher_host_, incoming,
                                         outgoing, packet_callback));

  if (stop_rtp_dump_callback_.is_null()) {
    stop_rtp_dump_callback_ =
        base::Bind(&P2PSocketDispatcherHost::StopRtpDumpOnUIThread,
                   p2p_socket_dispatcher_host_);
  }
  return stop_rtp_dump_callback_;
}
#endif

IPC::ChannelProxy* RenderProcessHostImpl::GetChannel() {
  return channel_.get();
}

void RenderProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  filter->RegisterAssociatedInterfaces(channel_.get());
  channel_->AddFilter(filter->GetFilter());
}

bool RenderProcessHostImpl::FastShutdownStarted() const {
  return fast_shutdown_started_;
}

// static
void RenderProcessHostImpl::RegisterHost(int host_id, RenderProcessHost* host) {
  g_all_hosts.Get().AddWithID(host, host_id);
}

// static
void RenderProcessHostImpl::UnregisterHost(int host_id) {
  RenderProcessHost* host = g_all_hosts.Get().Lookup(host_id);
  if (!host)
    return;

  g_all_hosts.Get().Remove(host_id);

  // Look up the map of site to process for the given browser_context,
  // in case we need to remove this process from it.  It will be registered
  // under any sites it rendered that use process-per-site mode.
  SiteProcessMap* map =
      GetSiteProcessMapForBrowserContext(host->GetBrowserContext());
  map->RemoveProcess(host);
}

// static
void RenderProcessHostImpl::FilterURL(RenderProcessHost* rph,
                                      bool empty_allowed,
                                      GURL* url) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  if (empty_allowed && url->is_empty())
    return;

  if (!url->is_valid()) {
    // Have to use about:blank for the denied case, instead of an empty GURL.
    // This is because the browser treats navigation to an empty GURL as a
    // navigation to the home page. This is often a privileged page
    // (chrome://newtab/) which is exactly what we don't want.
    *url = GURL(url::kAboutBlankURL);
    return;
  }

  if (!policy->CanRequestURL(rph->GetID(), *url)) {
    // If this renderer is not permitted to request this URL, we invalidate the
    // URL.  This prevents us from storing the blocked URL and becoming confused
    // later.
    VLOG(1) << "Blocked URL " << url->spec();
    *url = GURL(url::kAboutBlankURL);
  }
}

// static
bool RenderProcessHostImpl::IsSuitableHost(RenderProcessHost* host,
                                           BrowserContext* browser_context,
                                           const GURL& site_url) {
  if (run_renderer_in_process()) {
    DCHECK_EQ(host->GetBrowserContext(), browser_context)
        << " Single-process mode does not support multiple browser contexts.";
    return true;
  }

  if (host->GetBrowserContext() != browser_context)
    return false;

  // Do not allow sharing of guest hosts. This is to prevent bugs where guest
  // and non-guest storage gets mixed. In the future, we might consider enabling
  // the sharing of guests, in this case this check should be removed and
  // InSameStoragePartition should handle the possible sharing.
  if (host->IsForGuestsOnly())
    return false;

  // Check whether the given host and the intended site_url will be using the
  // same StoragePartition, since a RenderProcessHost can only support a single
  // StoragePartition.  This is relevant for packaged apps.
  StoragePartition* dest_partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, site_url);
  if (!host->InSameStoragePartition(dest_partition))
    return false;

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->HasWebUIBindings(host->GetID()) !=
      WebUIControllerFactoryRegistry::GetInstance()->UseWebUIBindingsForURL(
          browser_context, site_url)) {
    return false;
  }

  // Sites requiring dedicated processes can only reuse a compatible process.
  auto lock_state = policy->CheckOriginLock(host->GetID(), site_url);
  if (lock_state !=
      ChildProcessSecurityPolicyImpl::CheckOriginLockResult::NO_LOCK) {
    // If the process is already dedicated to a site, only allow the destination
    // URL to reuse this process if the URL has the same site.
    return lock_state == ChildProcessSecurityPolicyImpl::CheckOriginLockResult::
                             HAS_EQUAL_LOCK;
  } else if (!host->IsUnused() && SiteInstanceImpl::ShouldLockToOrigin(
                                      browser_context, host, site_url)) {
    // Otherwise, if this process has been used to host any other content, it
    // cannot be reused if the destination site indeed requires a dedicated
    // process and can be locked to just that site.
    return false;
  }

  return GetContentClient()->browser()->IsSuitableHost(host, site_url);
}

// static
void RenderProcessHost::WarmupSpareRenderProcessHost(
    content::BrowserContext* browser_context) {
  g_spare_render_process_host_manager.Get().WarmupSpareRenderProcessHost(
      browser_context);
}

// static
bool RenderProcessHost::run_renderer_in_process() {
  return g_run_renderer_in_process_;
}

// static
void RenderProcessHost::SetRunRendererInProcess(bool value) {
  g_run_renderer_in_process_ = value;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (value) {
    if (!command_line->HasSwitch(switches::kLang)) {
      // Modify the current process' command line to include the browser locale,
      // as the renderer expects this flag to be set.
      const std::string locale =
          GetContentClient()->browser()->GetApplicationLocale();
      command_line->AppendSwitchASCII(switches::kLang, locale);
    }
    // TODO(piman): we should really send configuration through bools rather
    // than by parsing strings, i.e. sending an IPC rather than command line
    // args. crbug.com/314909
    AppendCompositorCommandLineFlags(command_line);
  }
}

// static
RenderProcessHost::iterator RenderProcessHost::AllHostsIterator() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return iterator(g_all_hosts.Pointer());
}

// static
RenderProcessHost* RenderProcessHost::FromID(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_all_hosts.Get().Lookup(render_process_id);
}

// static
bool RenderProcessHost::ShouldTryToUseExistingProcessHost(
    BrowserContext* browser_context,
    const GURL& url) {
  if (run_renderer_in_process())
    return true;

  // NOTE: Sometimes it's necessary to create more render processes than
  //       GetMaxRendererProcessCount(), for instance when we want to create
  //       a renderer process for a browser context that has no existing
  //       renderers. This is OK in moderation, since the
  //       GetMaxRendererProcessCount() is conservative.
  if (g_all_hosts.Get().size() >= GetMaxRendererProcessCount())
    return true;

  return GetContentClient()->browser()->ShouldTryToUseExistingProcessHost(
      browser_context, url);
}

// static
RenderProcessHost* RenderProcessHost::GetExistingProcessHost(
    BrowserContext* browser_context,
    const GURL& site_url) {
  // First figure out which existing renderers we can use.
  std::vector<RenderProcessHost*> suitable_renderers;
  suitable_renderers.reserve(g_all_hosts.Get().size());

  iterator iter(AllHostsIterator());
  while (!iter.IsAtEnd()) {
    if (iter.GetCurrentValue()->MayReuseHost() &&
        RenderProcessHostImpl::IsSuitableHost(iter.GetCurrentValue(),
                                              browser_context, site_url)) {
      suitable_renderers.push_back(iter.GetCurrentValue());
    }
    iter.Advance();
  }

  // Now pick a random suitable renderer, if we have any.
  if (!suitable_renderers.empty()) {
    int suitable_count = static_cast<int>(suitable_renderers.size());
    int random_index = base::RandInt(0, suitable_count - 1);
    // If the process chosen was the spare RenderProcessHost, ensure it won't be
    // used as a spare in the future, or drop the spare if it wasn't used.
    g_spare_render_process_host_manager.Get().DropSpareOnProcessCreation(
        suitable_renderers[random_index]);
    return suitable_renderers[random_index];
  }

  return NULL;
}

// static
bool RenderProcessHost::ShouldUseProcessPerSite(BrowserContext* browser_context,
                                                const GURL& url) {
  // Returns true if we should use the process-per-site model.  This will be
  // the case if the --process-per-site switch is specified, or in
  // process-per-site-instance for particular sites (e.g., WebUI).
  // Note that --single-process is handled in ShouldTryToUseExistingProcessHost.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kProcessPerSite))
    return true;

  // We want to consolidate particular sites like WebUI even when we are using
  // the process-per-tab or process-per-site-instance models.
  // Note: DevTools pages have WebUI type but should not reuse the same host.
  if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
          browser_context, url) &&
      !url.SchemeIs(kChromeDevToolsScheme)) {
    return true;
  }

  // Otherwise let the content client decide, defaulting to false.
  return GetContentClient()->browser()->ShouldUseProcessPerSite(browser_context,
                                                                url);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetProcessHostForSite(
    BrowserContext* browser_context,
    const GURL& url) {
  // Look up the map of site to process for the given browser_context.
  SiteProcessMap* map = GetSiteProcessMapForBrowserContext(browser_context);

  // See if we have an existing process with appropriate bindings for this site.
  // If not, the caller should create a new process and register it.  Note that
  // IsSuitableHost expects a site URL rather than the full |url|.
  GURL site_url = SiteInstance::GetSiteForURL(browser_context, url);
  RenderProcessHost* host = map->FindProcess(site_url.possibly_invalid_spec());
  if (host && (!host->MayReuseHost() ||
               !IsSuitableHost(host, browser_context, site_url))) {
    // The registered process does not have an appropriate set of bindings for
    // the url.  Remove it from the map so we can register a better one.
    RecordAction(
        base::UserMetricsAction("BindingsMismatch_GetProcessHostPerSite"));
    map->RemoveProcess(host);
    host = NULL;
  }

  return host;
}

void RenderProcessHostImpl::RegisterProcessHostForSite(
    BrowserContext* browser_context,
    RenderProcessHost* process,
    const GURL& url) {
  // Look up the map of site to process for the given browser_context.
  SiteProcessMap* map = GetSiteProcessMapForBrowserContext(browser_context);

  // Only register valid, non-empty sites.  Empty or invalid sites will not
  // use process-per-site mode.  We cannot check whether the process has
  // appropriate bindings here, because the bindings have not yet been granted.
  std::string site =
      SiteInstance::GetSiteForURL(browser_context, url).possibly_invalid_spec();
  if (!site.empty())
    map->RegisterProcess(site, process);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetProcessHostForSiteInstance(
    BrowserContext* browser_context,
    SiteInstanceImpl* site_instance) {
  const GURL site_url = site_instance->GetSiteURL();
  SiteInstanceImpl::ProcessReusePolicy process_reuse_policy =
      site_instance->process_reuse_policy();
  bool is_for_guests_only = site_url.SchemeIs(kGuestScheme);
  RenderProcessHost* render_process_host = nullptr;

  bool is_unmatched_service_worker = site_instance->is_for_service_worker();

  // First, attempt to reuse an existing RenderProcessHost if necessary.
  switch (process_reuse_policy) {
    case SiteInstanceImpl::ProcessReusePolicy::PROCESS_PER_SITE:
      render_process_host = GetProcessHostForSite(browser_context, site_url);
      break;
    case SiteInstanceImpl::ProcessReusePolicy::USE_DEFAULT_SUBFRAME_PROCESS:
      DCHECK(SiteIsolationPolicy::IsTopDocumentIsolationEnabled());
      DCHECK(!site_instance->is_for_service_worker());
      render_process_host = GetDefaultSubframeProcessHost(
          browser_context, site_instance, is_for_guests_only);
      break;
    case SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE:
      render_process_host =
          FindReusableProcessHostForSite(browser_context, site_url);
      UMA_HISTOGRAM_BOOLEAN(
          "SiteIsolation.ReusePendingOrCommittedSite.CouldReuse",
          render_process_host != nullptr);
      if (render_process_host)
        is_unmatched_service_worker = false;
      break;
    default:
      break;
  }

  // If not, attempt to reuse an existing process with an unmatched service
  // worker for this site. Exclude cases where the policy is DEFAULT and the
  // site instance is for a service worker. We use DEFAULT when we have failed
  // to start the service worker before and want to use a new process.
  if (!render_process_host &&
      !(process_reuse_policy == SiteInstanceImpl::ProcessReusePolicy::DEFAULT &&
        site_instance->is_for_service_worker())) {
    render_process_host = UnmatchedServiceWorkerProcessTracker::MatchWithSite(
        browser_context, site_url);
  }

  // If not (or if none found), see if we should reuse an existing process.
  if (!render_process_host &&
      ShouldTryToUseExistingProcessHost(browser_context, site_url)) {
    render_process_host = GetExistingProcessHost(browser_context, site_url);
  }

  // Otherwise, use the spare RenderProcessHost or create a new one.
  if (!render_process_host) {
    // Pass a null StoragePartition. Tests with TestBrowserContext using a
    // RenderProcessHostFactory may not instantiate a StoragePartition, and
    // creating one here with GetStoragePartition() can run into cross-thread
    // issues as TestBrowserContext initialization is done on the main thread.
    render_process_host = CreateOrUseSpareRenderProcessHost(
        browser_context, nullptr, site_instance, is_for_guests_only);
  }

  if (is_unmatched_service_worker) {
    UnmatchedServiceWorkerProcessTracker::Register(
        browser_context, render_process_host, site_url);
  }

  // Make sure the chosen process is in the correct StoragePartition for the
  // SiteInstance.
  CHECK(render_process_host->InSameStoragePartition(
      BrowserContext::GetStoragePartition(browser_context, site_instance)));

  return render_process_host;
}

void RenderProcessHostImpl::CreateSharedRendererHistogramAllocator() {
  // Create a persistent memory segment for renderer histograms only if
  // they're active in the browser.
  if (!base::GlobalHistogramAllocator::Get())
    return;

  // Get handle to the renderer process. Stop if there is none.
  base::ProcessHandle destination = GetHandle();
  if (destination == base::kNullProcessHandle)
    return;

  // If a renderer crashes before completing startup and gets restarted, this
  // method will get called a second time meaning that a metrics-allocator
  // already exists. Don't recreate it.
  if (!metrics_allocator_) {
    // Create persistent/shared memory and allow histograms to be stored in
    // it. Memory that is not actualy used won't be physically mapped by the
    // system. RendererMetrics usage, as reported in UMA, peaked around 0.7MiB
    // as of 2016-12-20.
    std::unique_ptr<base::SharedMemory> shm(new base::SharedMemory());
    if (!shm->CreateAndMapAnonymous(2 << 20))  // 2 MiB
      return;
    metrics_allocator_.reset(new base::SharedPersistentMemoryAllocator(
        std::move(shm), GetID(), "RendererMetrics", /*readonly=*/false));
  }

  base::SharedMemoryHandle shm_handle =
      metrics_allocator_->shared_memory()->handle().Duplicate();
  Send(new ChildProcessMsg_SetHistogramMemory(
      shm_handle, metrics_allocator_->shared_memory()->mapped_size()));
}

void RenderProcessHostImpl::ProcessDied(bool already_dead,
                                        RendererClosedDetails* known_details) {
  // Our child process has died.  If we didn't expect it, it's a crash.
  // In any case, we need to let everyone know it's gone.
  // The OnChannelError notification can fire multiple times due to nested sync
  // calls to a renderer. If we don't have a valid channel here it means we
  // already handled the error.

  // It should not be possible for us to be called re-entrantly.
  DCHECK(!within_process_died_observer_);

  // It should not be possible for a process death notification to come in while
  // we are dying.
  DCHECK(!deleting_soon_);

  // child_process_launcher_ can be NULL in single process mode or if fast
  // termination happened.
  base::TerminationStatus status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
  int exit_code = 0;
  if (known_details) {
    status = known_details->status;
    exit_code = known_details->exit_code;
  } else if (child_process_launcher_.get()) {
    status = child_process_launcher_->GetChildTerminationStatus(already_dead,
                                                                &exit_code);
    if (already_dead && status == base::TERMINATION_STATUS_STILL_RUNNING) {
      // May be in case of IPC error, if it takes long time for renderer
      // to exit. Child process will be killed in any case during
      // child_process_launcher_.reset(). Make sure we will not broadcast
      // FrameHostMsg_RenderProcessGone with status
      // TERMINATION_STATUS_STILL_RUNNING, since this will break WebContentsImpl
      // logic.
      status = base::TERMINATION_STATUS_PROCESS_CRASHED;
    }
  }

  RendererClosedDetails details(status, exit_code);

  child_process_launcher_.reset();
  is_dead_ = true;
  if (route_provider_binding_.is_bound())
    route_provider_binding_.Close();
  associated_interfaces_.reset();
  process_resource_coordinator_.reset();
  ResetChannelProxy();

  UpdateProcessPriority();

  within_process_died_observer_ = true;
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_CLOSED, Source<RenderProcessHost>(this),
      Details<RendererClosedDetails>(&details));
  for (auto& observer : observers_)
    observer.RenderProcessExited(this, status, exit_code);
  within_process_died_observer_ = false;

  RemoveUserData(kSessionStorageHolderKey);

  base::IDMap<IPC::Listener*>::iterator iter(&listeners_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->OnMessageReceived(FrameHostMsg_RenderProcessGone(
        iter.GetCurrentKey(), static_cast<int>(status), exit_code));
    iter.Advance();
  }

  // Initialize a new ChannelProxy in case this host is re-used for a new
  // process. This ensures that new messages can be sent on the host ASAP (even
  // before Init()) and they'll eventually reach the new process.
  //
  // Note that this may have already been called by one of the above observers
  EnableSendQueue();

  // It's possible that one of the calls out to the observers might have caused
  // this object to be no longer needed.
  if (delayed_cleanup_needed_)
    Cleanup();

  // If RenderProcessHostImpl is reused, the next renderer will send a new
  // request for FrameSinkProvider so make sure frame_sink_provider_ is ready
  // for that.
  frame_sink_provider_.Unbind();
  if (renderer_host_binding_.is_bound())
    renderer_host_binding_.Unbind();

  shared_bitmap_allocation_notifier_impl_.ChildDied();

  // This object is not deleted at this point and might be reused later.
  // TODO(darin): clean this up
}

size_t RenderProcessHost::GetActiveViewCount() {
  size_t num_active_views = 0;
  std::unique_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Count only RenderWidgetHosts in this process.
    if (widget->GetProcess()->GetID() == GetID())
      num_active_views++;
  }
  return num_active_views;
}

void RenderProcessHost::PostTaskWhenProcessIsReady(base::OnceClosure task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!task.is_null());
  new RenderProcessHostIsReadyObserver(this, std::move(task));
}

void RenderProcessHostImpl::ReleaseOnCloseACK(
    RenderProcessHost* host,
    const SessionStorageNamespaceMap& sessions,
    int view_route_id) {
  DCHECK(host);
  if (sessions.empty())
    return;
  SessionStorageHolder* holder = static_cast<SessionStorageHolder*>(
      host->GetUserData(kSessionStorageHolderKey));
  if (!holder) {
    holder = new SessionStorageHolder();
    host->SetUserData(kSessionStorageHolderKey, base::WrapUnique(holder));
  }
  holder->Hold(sessions, view_route_id);
}

void RenderProcessHostImpl::OnShutdownRequest() {
  // Don't shut down if there are active RenderViews, or if there are pending
  // RenderViews being swapped back in.
  // In single process mode, we never shutdown the renderer.
  if (pending_views_ || run_renderer_in_process() || GetActiveViewCount() > 0)
    return;

  // Notify any contents that might have swapped out renderers from this
  // process. They should not attempt to swap them back in.
  for (auto& observer : observers_)
    observer.RenderProcessWillExit(this);

  Send(new ChildProcessMsg_Shutdown());
}

void RenderProcessHostImpl::SuddenTerminationChanged(bool enabled) {
  SetSuddenTerminationAllowed(enabled);
}

void RenderProcessHostImpl::UpdateProcessPriority() {
  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting()) {
    priority_.background = kLaunchingProcessIsBackgrounded;
    priority_.boost_for_pending_views =
        kLaunchingProcessIsBoostedForPendingView;
    return;
  }

  const ChildProcessLauncherPriority priority = {
    // We background a process as soon as it hosts no active audio/video streams
    // and no visible widgets -- the callers must call this function whenever we
    // transition in/out of those states.
    visible_widgets_ == 0 && media_stream_count_ == 0 &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableRendererBackgrounding),
    // boost_for_pending_views
    !!pending_views_,
#if defined(OS_ANDROID)
    ComputeEffectiveImportance(),
#endif
  };

  const bool should_background_changed =
      priority_.background != priority.background;
  if (priority_ == priority)
    return;

  TRACE_EVENT2("renderer_host", "RenderProcessHostImpl::UpdateProcessPriority",
               "should_background", priority.background, "has_pending_views",
               priority.boost_for_pending_views);
  priority_ = priority;

#if defined(OS_WIN)
  // The cbstext.dll loads as a global GetMessage hook in the browser process
  // and intercepts/unintercepts the kernel32 API SetPriorityClass in a
  // background thread. If the UI thread invokes this API just when it is
  // intercepted the stack is messed up on return from the interceptor
  // which causes random crashes in the browser process. Our hack for now
  // is to not invoke the SetPriorityClass API if the dll is loaded.
  if (GetModuleHandle(L"cbstext.dll"))
    return;
#endif  // OS_WIN

  // Control the background state from the browser process, otherwise the task
  // telling the renderer to "unbackground" itself may be preempted by other
  // tasks executing at lowered priority ahead of it or simply by not being
  // swiftly scheduled by the OS per the low process priority
  // (http://crbug.com/398103).
  child_process_launcher_->SetProcessPriority(priority_);

  // Notify the child process of background state. Note
  // |priority_.boost_for_pending_views| state is not sent to renderer simply
  // due to lack of need.
  if (should_background_changed)
    Send(new ChildProcessMsg_SetProcessBackgrounded(priority.background));
}

void RenderProcessHostImpl::OnProcessLaunched() {
  // No point doing anything, since this object will be destructed soon.  We
  // especially don't want to send the RENDERER_PROCESS_CREATED notification,
  // since some clients might expect a RENDERER_PROCESS_TERMINATED afterwards to
  // properly cleanup.
  if (deleting_soon_)
    return;

  if (child_process_launcher_) {
    DCHECK(child_process_launcher_->GetProcess().IsValid());
    DCHECK_EQ(kLaunchingProcessIsBackgrounded, priority_.background);

    // Unpause the channel now that the process is launched. We don't flush it
    // yet to ensure that any initialization messages sent here (e.g., things
    // done in response to NOTIFICATION_RENDER_PROCESS_CREATED; see below)
    // preempt already queued messages.
    channel_->Unpause(false /* flush */);

    if (child_connection_) {
      child_connection_->SetProcessHandle(
          child_process_launcher_->GetProcess().Handle());
    }

    // Not all platforms launch processes in the same backgrounded state. Make
    // sure |priority_.background| reflects this platform's initial process
    // state.
#if defined(OS_MACOSX)
    priority_.background =
        child_process_launcher_->GetProcess().IsProcessBackgrounded(
            MachBroker::GetInstance());
#elif defined(OS_ANDROID)
    // Android child process priority works differently and cannot be queried
    // directly from base::Process.
    DCHECK_EQ(kLaunchingProcessIsBackgrounded, priority_.background);
#else
    priority_.background =
        child_process_launcher_->GetProcess().IsProcessBackgrounded();
#endif  // defined(OS_MACOSX)

    // Disable updating process priority on startup on desktop platforms for now
    // as it incorrectly results in backgrounding foreground navigations until
    // their first commit is made. A better long term solution would be to be
    // aware of the tab's visibility at this point. https://crbug.com/560446.
    // This is still needed on Android which uses
    // |priority_.boost_for_pending_views| and requires RenderProcessHostImpl to
    // propagate priority changes immediately to ChildProcessLauncher.
#if defined(OS_ANDROID)
    UpdateProcessPriority();
#endif

    // Share histograms between the renderer and this process.
    CreateSharedRendererHistogramAllocator();
  }

  // NOTE: This needs to be before flushing queued messages, because
  // ExtensionService uses this notification to initialize the renderer process
  // with state that must be there before any JavaScript executes.
  //
  // The queued messages contain such things as "navigate". If this notification
  // was after, we can end up executing JavaScript before the initialization
  // happens.
  NotificationService::current()->Notify(NOTIFICATION_RENDERER_PROCESS_CREATED,
                                         Source<RenderProcessHost>(this),
                                         NotificationService::NoDetails());

  if (child_process_launcher_)
    channel_->Flush();

  if (IsReady()) {
    DCHECK(!sent_render_process_ready_);
    sent_render_process_ready_ = true;
    // Send RenderProcessReady only if the channel is already connected.
    for (auto& observer : observers_)
      observer.RenderProcessReady(this);
  }

  GetProcessResourceCoordinator()->SetProperty(
      resource_coordinator::mojom::PropertyType::kPID,
      base::GetProcId(GetHandle()));

#if BUILDFLAG(ENABLE_WEBRTC)
  if (WebRTCInternals::GetInstance()->IsAudioDebugRecordingsEnabled()) {
    EnableAudioDebugRecordings(
        WebRTCInternals::GetInstance()->GetAudioDebugRecordingsFilePath());
  }
#endif
}

void RenderProcessHostImpl::OnProcessLaunchFailed(int error_code) {
  // If this object will be destructed soon, then observers have already been
  // sent a RenderProcessHostDestroyed notification, and we must observe our
  // contract that says that will be the last call.
  if (deleting_soon_)
    return;

  RendererClosedDetails details{base::TERMINATION_STATUS_LAUNCH_FAILED,
                                error_code};
  ProcessDied(true, &details);
}

void RenderProcessHostImpl::OnUserMetricsRecordAction(
    const std::string& action) {
  base::RecordComputedAction(action);
}

void RenderProcessHostImpl::OnCloseACK(int old_route_id) {
  SessionStorageHolder* holder =
      static_cast<SessionStorageHolder*>(GetUserData(kSessionStorageHolderKey));
  if (!holder)
    return;
  holder->Release(old_route_id);
}

void RenderProcessHostImpl::OnGpuSwitched() {
  RecomputeAndUpdateWebKitPreferences();
}

// static
RenderProcessHost* RenderProcessHostImpl::GetDefaultSubframeProcessHost(
    BrowserContext* browser_context,
    SiteInstanceImpl* site_instance,
    bool is_for_guests_only) {
  DefaultSubframeProcessHostHolder* holder =
      static_cast<DefaultSubframeProcessHostHolder*>(
          browser_context->GetUserData(&kDefaultSubframeProcessHostHolderKey));
  if (!holder) {
    holder = new DefaultSubframeProcessHostHolder(browser_context);
    browser_context->SetUserData(kDefaultSubframeProcessHostHolderKey,
                                 base::WrapUnique(holder));
  }

  return holder->GetProcessHost(site_instance, is_for_guests_only);
}

// static
RenderProcessHost* RenderProcessHostImpl::FindReusableProcessHostForSite(
    BrowserContext* browser_context,
    const GURL& site_url) {
  if (!ShouldFindReusableProcessHostForSite(browser_context, site_url))
    return nullptr;

  std::set<RenderProcessHost*> eligible_foreground_hosts;
  std::set<RenderProcessHost*> eligible_background_hosts;

  // First, add the RenderProcessHosts expecting a navigation to |site_url| to
  // the list of eligible RenderProcessHosts.
  SiteProcessCountTracker* pending_tracker =
      static_cast<SiteProcessCountTracker*>(
          browser_context->GetUserData(kPendingSiteProcessCountTrackerKey));
  if (pending_tracker) {
    pending_tracker->FindRenderProcessesForSite(
        site_url, &eligible_foreground_hosts, &eligible_background_hosts);
  }

  if (eligible_foreground_hosts.empty()) {
    // If needed, add the RenderProcessHosts hosting a frame for |site_url| to
    // the list of eligible RenderProcessHosts.
    SiteProcessCountTracker* committed_tracker =
        static_cast<SiteProcessCountTracker*>(
            browser_context->GetUserData(kCommittedSiteProcessCountTrackerKey));
    if (committed_tracker) {
      committed_tracker->FindRenderProcessesForSite(
          site_url, &eligible_foreground_hosts, &eligible_background_hosts);
    }
  }

  if (!eligible_foreground_hosts.empty()) {
    int index = base::RandInt(0, eligible_foreground_hosts.size() - 1);
    auto iterator = eligible_foreground_hosts.begin();
    for (int i = 0; i < index; ++i)
      ++iterator;
    return (*iterator);
  }

  if (!eligible_background_hosts.empty()) {
    int index = base::RandInt(0, eligible_background_hosts.size() - 1);
    auto iterator = eligible_background_hosts.begin();
    for (int i = 0; i < index; ++i)
      ++iterator;
    return (*iterator);
  }

  return nullptr;
}

#if BUILDFLAG(ENABLE_WEBRTC)
void RenderProcessHostImpl::CreateMediaStreamDispatcherHost(
    const std::string& salt,
    MediaStreamManager* media_stream_manager,
    mojom::MediaStreamDispatcherHostRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!media_stream_dispatcher_host_) {
    media_stream_dispatcher_host_.reset(
        new MediaStreamDispatcherHost(GetID(), salt, media_stream_manager));
  }
  media_stream_dispatcher_host_->BindRequest(std::move(request));
}

void RenderProcessHostImpl::OnRegisterAecDumpConsumer(int id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&RenderProcessHostImpl::RegisterAecDumpConsumerOnUIThread,
                     weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::OnUnregisterAecDumpConsumer(int id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &RenderProcessHostImpl::UnregisterAecDumpConsumerOnUIThread,
          weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::RegisterAecDumpConsumerOnUIThread(int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  aec_dump_consumers_.push_back(id);

  if (WebRTCInternals::GetInstance()->IsAudioDebugRecordingsEnabled()) {
    base::FilePath file_with_extensions = GetAecDumpFilePathWithExtensions(
        WebRTCInternals::GetInstance()->GetAudioDebugRecordingsFilePath());
    EnableAecDumpForId(file_with_extensions, id);
  }
  if (override_aec3_) {
    Send(new AudioProcessingMsg_EnableAec3(id, *override_aec3_));
  }
}

void RenderProcessHostImpl::UnregisterAecDumpConsumerOnUIThread(int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (std::vector<int>::iterator it = aec_dump_consumers_.begin();
       it != aec_dump_consumers_.end(); ++it) {
    if (*it == id) {
      aec_dump_consumers_.erase(it);
      break;
    }
  }
}

void RenderProcessHostImpl::EnableAecDumpForId(const base::FilePath& file,
                                               int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskAndReplyWithResult(
      &GetAecDumpFileTaskRunner(), FROM_HERE,
      base::Bind(&CreateFileForProcess, file.AddExtension(IntToStringType(id))),
      base::Bind(&RenderProcessHostImpl::SendAecDumpFileToRenderer,
                 weak_factory_.GetWeakPtr(), id));
}

void RenderProcessHostImpl::SendAecDumpFileToRenderer(
    int id,
    IPC::PlatformFileForTransit file_for_transit) {
  if (file_for_transit == IPC::InvalidPlatformFileForTransit())
    return;
  Send(new AecDumpMsg_EnableAecDump(id, file_for_transit));
}

void RenderProcessHostImpl::SendDisableAecDumpToRenderer() {
  Send(new AecDumpMsg_DisableAecDump());
}

base::FilePath RenderProcessHostImpl::GetAecDumpFilePathWithExtensions(
    const base::FilePath& file) {
  return file.AddExtension(IntToStringType(base::GetProcId(GetHandle())))
      .AddExtension(kAecDumpFileNameAddition);
}

base::SequencedTaskRunner& RenderProcessHostImpl::GetAecDumpFileTaskRunner() {
  if (!audio_debug_recordings_file_task_runner_) {
    audio_debug_recordings_file_task_runner_ =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
             base::TaskPriority::USER_BLOCKING});
  }
  return *audio_debug_recordings_file_task_runner_;
}
#endif  // BUILDFLAG(ENABLE_WEBRTC)

void RenderProcessHostImpl::RecomputeAndUpdateWebKitPreferences() {
  // We are updating all widgets including swapped out ones.
  for (auto* widget : widgets_) {
    RenderViewHost* rvh = RenderViewHost::From(widget);
    if (!rvh)
      continue;

    rvh->OnWebkitPreferencesChanged();
  }
}

// static
void RenderProcessHostImpl::OnMojoError(int render_process_id,
                                        const std::string& error) {
  LOG(ERROR) << "Terminating render process for bad Mojo message: " << error;

  // The ReceivedBadMessage call below will trigger a DumpWithoutCrashing.
  // Capture the error message in a crash key value.
  base::debug::ScopedCrashKey error_key_value("mojo-message-error", error);
  bad_message::ReceivedBadMessage(render_process_id,
                                  bad_message::RPH_MOJO_PROCESS_ERROR);
}

viz::SharedBitmapAllocationNotifierImpl*
RenderProcessHostImpl::GetSharedBitmapAllocationNotifier() {
  return &shared_bitmap_allocation_notifier_impl_;
}

}  // namespace content
