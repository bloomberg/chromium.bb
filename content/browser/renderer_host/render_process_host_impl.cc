// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "content/browser/renderer_host/render_process_host_impl.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/token.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/discardable_memory/public/mojom/discardable_shared_memory_manager.mojom.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/metrics/single_sample_metrics.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/gpu_client.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_service_impl.h"
#include "content/browser/background_sync/one_shot_background_sync_service_impl.h"
#include "content/browser/background_sync/periodic_background_sync_service_impl.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/blob_registry_wrapper.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/browser_main.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/field_trial_recorder.h"
#include "content/browser/field_trial_synchronizer.h"
#include "content/browser/file_system/file_system_manager_impl.h"
#include "content/browser/font_unique_name_lookup/font_unique_name_lookup_service.h"
#include "content/browser/frame_host/render_frame_message_filter.h"
#include "content/browser/gpu/browser_gpu_client_delegate.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/browser/histogram_controller.h"
#include "content/browser/locks/lock_manager.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/media/midi_host.h"
#include "content/browser/mime_registry_impl.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/browser/native_io/native_io_context.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/payments/payment_manager.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/permissions/permission_service_impl.h"
#include "content/browser/push_messaging/push_messaging_manager.h"
#include "content/browser/quota/quota_context.h"
#include "content/browser/renderer_host/agent_metrics_collector.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/embedded_frame_sink_provider_impl.h"
#include "content/browser/renderer_host/file_utilities_host_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_track_metrics_host.h"
#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"
#include "content/browser/renderer_host/media/video_capture_host.h"
#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"
#include "content/browser/renderer_host/plugin_registry_impl.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/text_input_client_message_filter.h"
#include "content/browser/renderer_host/web_database_host_impl.h"
#include "content/browser/resolve_proxy_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/theme_helper.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/v8_snapshot_files.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/child_process.mojom.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/resource_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_coordinator_service.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/webrtc_log.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "device/gamepad/gamepad_haptics_manager.h"
#include "google_apis/gaia/gaia_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "ipc/trace_ipc_message.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "media/webrtc/webrtc_switches.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/power_monitor.mojom.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/time_zone_monitor.mojom.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "services/metrics/ukm_recorder_interface.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/sandbox/switches.h"
#include "services/service_manager/zygote/common/zygote_buildflags.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/disk_allocator.mojom.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_features.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/java_interfaces.h"
#include "ipc/ipc_sync_channel.h"
#include "media/audio/android/audio_manager_android.h"
#else
#include "content/browser/gpu/gpu_data_manager_impl.h"
#endif

#if defined(OS_LINUX)
#include <sys/resource.h>
#include <sys/time.h>

#include "components/services/font/public/mojom/font_service.mojom.h"  // nogncheck
#include "content/browser/font_service.h"  // nogncheck
#include "third_party/blink/public/mojom/memory_usage_monitor_linux.mojom.h"  // nogncheck
#endif

#if defined(OS_MACOSX)
#include "content/browser/child_process_task_port_provider_mac.h"
#include "content/browser/sandbox_support_mac_impl.h"
#include "content/common/sandbox_support_mac.mojom.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"
#include "content/public/common/font_cache_dispatcher_win.h"
#include "content/public/common/font_cache_win.mojom.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#include "ui/display/win/dpi.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/browser/media/key_system_support_impl.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#include "ppapi/shared_impl/ppapi_switches.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_REPORTING)
#include "content/browser/net/reporting_service_proxy.h"
#endif

#if BUILDFLAG(USE_MINIKIN_HYPHENATION)
#include "content/browser/hyphenation/hyphenation_impl.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_switches.h"
#endif

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "services/service_manager/zygote/common/zygote_handle.h"  // nogncheck
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "content/public/common/profiling_utils.h"
#endif

namespace content {

namespace {

// Stores the maximum number of renderer processes the content module can
// create. Only applies if it is set to a non-zero value.
size_t g_max_renderer_count_override = 0;

bool g_run_renderer_in_process = false;

RendererMainThreadFactoryFunction g_renderer_main_thread_factory = nullptr;

base::Thread* g_in_process_thread = nullptr;

RenderProcessHostFactory* g_render_process_host_factory_ = nullptr;
const char kSiteProcessMapKeyName[] = "content_site_process_map";

RenderProcessHost::AnalyzeHungRendererFunction g_analyze_hung_renderer =
    nullptr;

void CacheShaderInfo(int32_t id, base::FilePath path) {
  if (GetShaderCacheFactorySingleton())
    GetShaderCacheFactorySingleton()->SetCacheInfo(id, path);
}

void RemoveShaderInfo(int32_t id) {
  if (GetShaderCacheFactorySingleton())
    GetShaderCacheFactorySingleton()->RemoveCacheInfo(id);
}

// Allow us to only run the trial in the first renderer.
bool has_done_stun_trials = false;

// the global list of all renderer processes
base::IDMap<RenderProcessHost*>& GetAllHosts() {
  static base::NoDestructor<base::IDMap<RenderProcessHost*>> s_all_hosts;
  return *s_all_hosts;
}

// Returns the global list of RenderProcessHostCreationObserver objects.
std::vector<RenderProcessHostCreationObserver*>& GetAllCreationObservers() {
  static base::NoDestructor<std::vector<RenderProcessHostCreationObserver*>>
      s_all_creation_observers;
  return *s_all_creation_observers;
}

// Map of site to process, to ensure we only have one RenderProcessHost per
// site in process-per-site mode.  Each map is specific to a BrowserContext.
class SiteProcessMap : public base::SupportsUserData::Data {
 public:
  typedef std::unordered_map<std::string, RenderProcessHost*> SiteToProcessMap;
  SiteProcessMap() {}

  void RegisterProcess(const std::string& site, RenderProcessHost* process) {
    // There could already exist a site to process mapping due to races between
    // two WebContents with blank SiteInstances. If that occurs, keeping the
    // existing entry and not overwriting it is a predictable behavior that is
    // safe.
    auto i = map_.find(site);
    if (i == map_.end())
      map_[site] = process;
  }

  RenderProcessHost* FindProcess(const std::string& site) {
    auto i = map_.find(site);
    if (i != map_.end())
      return i->second;
    return nullptr;
  }

  void RemoveProcess(RenderProcessHost* host) {
    // Find all instances of this process in the map, then separately remove
    // them.
    std::set<std::string> sites;
    for (SiteToProcessMap::const_iterator i = map_.begin(); i != map_.end();
         ++i) {
      if (i->second == host)
        sites.insert(i->first);
    }
    for (auto i = sites.begin(); i != sites.end(); ++i) {
      auto iter = map_.find(*i);
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
  SiteProcessMap* existing_map = static_cast<SiteProcessMap*>(
      context->GetUserData(kSiteProcessMapKeyName));
  if (existing_map)
    return existing_map;

  auto new_map = std::make_unique<SiteProcessMap>();
  auto* new_map_ptr = new_map.get();
  context->SetUserData(kSiteProcessMapKeyName, std::move(new_map));
  return new_map_ptr;
}

// NOTE: changes to this class need to be reviewed by the security team.
class RendererSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  RendererSandboxedProcessLauncherDelegate()
#if defined(OS_WIN)
      : renderer_code_integrity_enabled_(
            GetContentClient()->browser()->IsRendererCodeIntegrityEnabled())
#endif
  {
  }

  ~RendererSandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    service_manager::SandboxWin::AddBaseHandleClosePolicy(policy);

    const base::string16& sid =
        GetContentClient()->browser()->GetAppContainerSidForSandboxType(
            GetSandboxType());
    if (!sid.empty())
      service_manager::SandboxWin::AddAppContainerPolicy(policy, sid.c_str());
    ContentBrowserClient::RendererSpawnFlags flags(
        ContentBrowserClient::RendererSpawnFlags::NONE);
    if (renderer_code_integrity_enabled_)
      flags = ContentBrowserClient::RendererSpawnFlags::RENDERER_CODE_INTEGRITY;
    return GetContentClient()->browser()->PreSpawnRenderer(policy, flags);
  }
#endif  // OS_WIN

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  service_manager::ZygoteHandle GetZygote() override {
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringType renderer_prefix =
        browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);
    if (!renderer_prefix.empty())
      return nullptr;
    return service_manager::GetGenericZygote();
  }
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

  service_manager::SandboxType GetSandboxType() override {
    return service_manager::SandboxType::kRenderer;
  }

#if defined(OS_WIN)
 private:
  const bool renderer_code_integrity_enabled_;
#endif
};

const char kSessionStorageHolderKey[] = "kSessionStorageHolderKey";

class SessionStorageHolder : public base::SupportsUserData::Data {
 public:
  SessionStorageHolder()
      : session_storage_namespaces_awaiting_close_(
            new std::map<int, SessionStorageNamespaceMap>) {}

  ~SessionStorageHolder() override {
    // Its important to delete the map on the IO thread to avoid deleting
    // the underlying namespaces prior to processing ipcs referring to them.
    base::DeleteSoon(FROM_HERE, {BrowserThread::IO},
                     session_storage_namespaces_awaiting_close_.release());
  }

  void Hold(const SessionStorageNamespaceMap& sessions, int widget_route_id) {
    (*session_storage_namespaces_awaiting_close_)[widget_route_id] = sessions;
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
// renderer (SpareRenderProcessHostManager::GetInstance(), below). This class
// encapsulates the implementation of
// RenderProcessHost::WarmupSpareRenderProcessHost()
//
// RenderProcessHostImpl should call
// SpareRenderProcessHostManager::MaybeTakeSpareRenderProcessHost when creating
// a new RPH. In this implementation, the spare renderer is bound to a
// BrowserContext and its default StoragePartition. If
// MaybeTakeSpareRenderProcessHost is called with a BrowserContext that does not
// match, the spare renderer is discarded. Only the default StoragePartition
// will be able to use a spare renderer. The spare renderer will also not be
// used as a guest renderer (is_for_guests_ == true).
//
// It is safe to call WarmupSpareRenderProcessHost multiple times, although if
// called in a context where the spare renderer is not likely to be used
// performance may suffer due to the unnecessary RPH creation.
class SpareRenderProcessHostManager : public RenderProcessHostObserver {
 public:
  SpareRenderProcessHostManager() {}

  static SpareRenderProcessHostManager& GetInstance() {
    static base::NoDestructor<SpareRenderProcessHostManager> s_instance;
    return *s_instance;
  }

  void WarmupSpareRenderProcessHost(BrowserContext* browser_context) {
    if (spare_render_process_host_ &&
        spare_render_process_host_->GetBrowserContext() == browser_context) {
      DCHECK_EQ(BrowserContext::GetDefaultStoragePartition(browser_context),
                spare_render_process_host_->GetStoragePartition());
      return;  // Nothing to warm up.
    }

    CleanupSpareRenderProcessHost();

    // Don't create a spare renderer if we're using --single-process or if we've
    // got too many processes. See also ShouldTryToUseExistingProcessHost in
    // this file.
    if (RenderProcessHost::run_renderer_in_process() ||
        GetAllHosts().size() >=
            RenderProcessHostImpl::GetMaxRendererProcessCount())
      return;

    // Don't create a spare renderer when the system is under load.  This is
    // currently approximated by only looking at the memory pressure.  See also
    // https://crbug.com/852905.
    auto* memory_monitor = base::MemoryPressureMonitor::Get();
    if (memory_monitor &&
        memory_monitor->GetCurrentPressureLevel() >=
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE)
      return;

    spare_render_process_host_ = RenderProcessHostImpl::CreateRenderProcessHost(
        browser_context, nullptr /* storage_partition_impl */,
        nullptr /* site_instance */);
    spare_render_process_host_->AddObserver(this);
    spare_render_process_host_->Init();
  }

  RenderProcessHost* MaybeTakeSpareRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstanceImpl* site_instance) {
    // Give embedder a chance to disable using a spare RenderProcessHost for
    // certain SiteInstances.  Some navigations, such as to NTP or extensions,
    // require passing command-line flags to the renderer process at process
    // launch time, but this cannot be done for spare RenderProcessHosts, which
    // are started before it is known which navigation might use them.  So, a
    // spare RenderProcessHost should not be used in such cases.
    //
    // Note that exempting NTP and extensions from using the spare process might
    // also happen via HasProcess check below (which returns true for
    // process-per-site SiteInstances if the given process-per-site process
    // already exists).  Despite this potential overlap, it is important to do
    // both kinds of checks (to account for other non-ntp/extension
    // process-per-site scenarios + to work correctly even if
    // ShouldUseSpareRenderProcessHost starts covering non-process-per-site
    // scenarios).
    bool embedder_allows_spare_usage =
        GetContentClient()->browser()->ShouldUseSpareRenderProcessHost(
            browser_context, site_instance->GetSiteURL());

    // We shouldn't use the spare if:
    // 1. The SiteInstance has already got an associated process.  This is
    //    important to avoid taking and then immediately discarding the spare
    //    for process-per-site scenarios (which the HasProcess call below
    //    accounts for).  Note that HasProcess will return false and allow using
    //    the spare if the given process-per-site process hasn't been launched.
    // 2. The SiteInstance has opted out of using the spare process.
    bool site_instance_allows_spare_usage =
        !site_instance->HasProcess() &&
        site_instance->CanAssociateWithSpareProcess();

    // Get the StoragePartition for |site_instance|.  Note that this might be
    // different than the default StoragePartition for |browser_context|.
    StoragePartition* site_storage =
        BrowserContext::GetStoragePartition(browser_context, site_instance);

    // Log UMA metrics.
    using SpareProcessMaybeTakeAction =
        RenderProcessHostImpl::SpareProcessMaybeTakeAction;
    SpareProcessMaybeTakeAction action =
        SpareProcessMaybeTakeAction::kNoSparePresent;
    if (!spare_render_process_host_)
      action = SpareProcessMaybeTakeAction::kNoSparePresent;
    else if (browser_context != spare_render_process_host_->GetBrowserContext())
      action = SpareProcessMaybeTakeAction::kMismatchedBrowserContext;
    else if (site_storage != spare_render_process_host_->GetStoragePartition())
      action = SpareProcessMaybeTakeAction::kMismatchedStoragePartition;
    else if (!embedder_allows_spare_usage)
      action = SpareProcessMaybeTakeAction::kRefusedByEmbedder;
    else if (!site_instance_allows_spare_usage)
      action = SpareProcessMaybeTakeAction::kRefusedBySiteInstance;
    else
      action = SpareProcessMaybeTakeAction::kSpareTaken;
    UMA_HISTOGRAM_ENUMERATION(
        "BrowserRenderProcessHost.SpareProcessMaybeTakeAction", action);

    // Decide whether to take or drop the spare process.
    RenderProcessHost* returned_process = nullptr;
    if (spare_render_process_host_ &&
        browser_context == spare_render_process_host_->GetBrowserContext() &&
        site_storage == spare_render_process_host_->GetStoragePartition() &&
        !site_instance->IsGuest() && embedder_allows_spare_usage &&
        site_instance_allows_spare_usage) {
      CHECK(spare_render_process_host_->HostHasNotBeenUsed());

      // If the spare process ends up getting killed, the spare manager should
      // discard the spare RPH, so if one exists, it should always be live here.
      CHECK(spare_render_process_host_->IsInitializedAndNotDead());

      DCHECK_EQ(SpareProcessMaybeTakeAction::kSpareTaken, action);
      returned_process = spare_render_process_host_;
      ReleaseSpareRenderProcessHost(spare_render_process_host_);
    } else if (!RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
      // If the spare shouldn't be kept around, then discard it as soon as we
      // find that the current spare was mismatched.
      CleanupSpareRenderProcessHost();
    } else if (GetAllHosts().size() >=
               RenderProcessHostImpl::GetMaxRendererProcessCount()) {
      // Drop the spare if we are at a process limit and the spare wasn't taken.
      // This helps avoid process reuse.
      CleanupSpareRenderProcessHost();
    }

    return returned_process;
  }

  // Prepares for future requests (with an assumption that a future navigation
  // might require a new process for |browser_context|).
  //
  // Note that depending on the caller PrepareForFutureRequests can be called
  // after the spare_render_process_host_ has either been 1) matched and taken
  // or 2) mismatched and ignored or 3) matched and ignored.
  void PrepareForFutureRequests(BrowserContext* browser_context) {
    if (RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
      // Always keep around a spare process for the most recently requested
      // |browser_context|.
      WarmupSpareRenderProcessHost(browser_context);
    } else {
      // Discard the ignored (probably non-matching) spare so as not to waste
      // resources.
      CleanupSpareRenderProcessHost();
    }
  }

  // Gracefully remove and cleanup a spare RenderProcessHost if it exists.
  void CleanupSpareRenderProcessHost() {
    if (spare_render_process_host_) {
      // Stop observing the process, to avoid getting notifications as a
      // consequence of the Cleanup call below - such notification could call
      // back into CleanupSpareRenderProcessHost leading to stack overflow.
      spare_render_process_host_->RemoveObserver(this);

      // Make sure the RenderProcessHost object gets destroyed.
      if (!spare_render_process_host_->IsKeepAliveRefCountDisabled())
        spare_render_process_host_->Cleanup();

      // Drop reference to the RenderProcessHost object.
      spare_render_process_host_ = nullptr;
    }
  }

  RenderProcessHost* spare_render_process_host() {
    return spare_render_process_host_;
  }

 private:
  // Release ownership of |host| as a possible spare renderer.  Called when
  // |host| has either been 1) claimed to be used in a navigation or 2) shutdown
  // somewhere else.
  void ReleaseSpareRenderProcessHost(RenderProcessHost* host) {
    if (spare_render_process_host_ && spare_render_process_host_ == host) {
      spare_render_process_host_->RemoveObserver(this);
      spare_render_process_host_ = nullptr;
    }
  }

  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    if (host == spare_render_process_host_)
      CleanupSpareRenderProcessHost();
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    ReleaseSpareRenderProcessHost(host);
  }

  // This is a bare pointer, because RenderProcessHost manages the lifetime of
  // all its instances; see GetAllHosts().
  RenderProcessHost* spare_render_process_host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SpareRenderProcessHostManager);
};

class RenderProcessHostIsReadyObserver : public RenderProcessHostObserver {
 public:
  RenderProcessHostIsReadyObserver(RenderProcessHost* render_process_host,
                                   base::OnceClosure task)
      : render_process_host_(render_process_host), task_(std::move(task)) {
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
    base::PostTask(FROM_HERE, {BrowserThread::UI},
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
  base::WeakPtrFactory<RenderProcessHostIsReadyObserver> weak_factory_{this};

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
    DCHECK_GE(counts_per_process[render_process_host_id], 0);

    if (counts_per_process[render_process_host_id] == 0)
      counts_per_process.erase(render_process_host_id);

    if (counts_per_process.empty())
      map_.erase(site_url);
  }

  void FindRenderProcessesForSiteInstance(
      SiteInstanceImpl* site_instance,
      std::set<RenderProcessHost*>* foreground_processes,
      std::set<RenderProcessHost*>* background_processes) {
    auto result = map_.find(site_instance->GetSiteURL());
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

      // It's possible that |host| has become unsuitable for hosting
      // |site_url|, for example if it was reused by a navigation to a
      // different site, and |site_url| requires a dedicated process.  Do not
      // allow such hosts to be reused.  See https://crbug.com/780661.
      if (!host->MayReuseHost() ||
          !RenderProcessHostImpl::IsSuitableHost(
              host, site_instance->GetIsolationContext(),
              site_instance->GetSiteURL(), site_instance->lock_url(),
              site_instance->IsGuest())) {
        continue;
      }

      if (host->VisibleClientCount())
        foreground_processes->insert(host);
      else
        background_processes->insert(host);
    }
  }

  // Check whether |host| is associated with at least one URL for which
  // SiteInstance does not assign site URLs.  This is used to disqualify |host|
  // from being reused if it has pending navigations to such URLs.
  bool ContainsNonReusableSiteForHost(RenderProcessHost* host) {
    for (auto iter : map_) {
      // If SiteInstance doesn't assign a site URL for the current entry (and it
      // isn't about:blank, which is allowed anywhere), check whether |host| is
      // on the list of processes the entry is associated with.
      //
      // TODO(alexmos): ShouldAssignSiteForURL() expects a full URL, whereas we
      // only have a site URL here.  For now, this mismatch is ok since
      // ShouldAssignSiteForURL() only cares about schemes in practice, but
      // this should be cleaned up.
      if (!SiteInstanceImpl::ShouldAssignSiteForURL(iter.first) &&
          !iter.first.IsAboutBlank() &&
          base::Contains(iter.second, host->GetID()))
        return true;
    }
    return false;
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
  if (site_url.is_empty())
    return false;

  return ShouldUseSiteProcessTracking(
      browser_context, render_process_host->GetStoragePartition(), site_url);
}

bool ShouldFindReusableProcessHostForSite(BrowserContext* browser_context,
                                          const GURL& site_url) {
  if (site_url.is_empty())
    return false;

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
  // |site_instance|.
  static void Register(RenderProcessHost* render_process_host,
                       SiteInstanceImpl* site_instance) {
    BrowserContext* browser_context = site_instance->GetBrowserContext();
    DCHECK(!site_instance->GetSiteURL().is_empty());
    if (!ShouldTrackProcessForSite(browser_context, render_process_host,
                                   site_instance->GetSiteURL()))
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
    tracker->RegisterProcessForSite(render_process_host, site_instance);
  }

  // Find a process with an unmatched service worker for |site_instance| and
  // removes the process from the tracker if it exists.
  static RenderProcessHost* MatchWithSite(SiteInstanceImpl* site_instance) {
    BrowserContext* browser_context = site_instance->GetBrowserContext();
    if (!ShouldFindReusableProcessHostForSite(browser_context,
                                              site_instance->GetSiteURL()))
      return nullptr;

    UnmatchedServiceWorkerProcessTracker* tracker =
        static_cast<UnmatchedServiceWorkerProcessTracker*>(
            browser_context->GetUserData(
                kUnmatchedServiceWorkerProcessTrackerKey));
    if (!tracker)
      return nullptr;
    return tracker->TakeFreshestProcessForSite(site_instance);
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
  using ProcessID = int;
  using SiteProcessIDPair = std::pair<GURL, ProcessID>;
  using SiteProcessIDPairSet = std::set<SiteProcessIDPair>;

  void RegisterProcessForSite(RenderProcessHost* host,
                              SiteInstanceImpl* site_instance) {
    if (!HasProcess(host))
      host->AddObserver(this);
    site_process_set_.insert(
        SiteProcessIDPair(site_instance->GetSiteURL(), host->GetID()));
  }

  RenderProcessHost* TakeFreshestProcessForSite(
      SiteInstanceImpl* site_instance) {
    SiteProcessIDPair site_process_pair =
        FindFreshestProcessForSite(site_instance);

    if (site_process_pair.first.is_empty())
      return nullptr;

    RenderProcessHost* host =
        RenderProcessHost::FromID(site_process_pair.second);

    if (!host)
      return nullptr;

    // It's possible that |host| is currently unsuitable for hosting
    // |site_url|, for example if it was used for a ServiceWorker for a
    // nonexistent extension URL.  See https://crbug.com/782349 and
    // https://crbug.com/780661.
    GURL site_url(site_instance->GetSiteURL());
    if (!host->MayReuseHost() ||
        !RenderProcessHostImpl::IsSuitableHost(
            host, site_instance->GetIsolationContext(), site_url,
            site_instance->lock_url(), site_instance->IsGuest()))
      return nullptr;

    site_process_set_.erase(site_process_pair);
    if (!HasProcess(host))
      host->RemoveObserver(this);
    return host;
  }

  SiteProcessIDPair FindFreshestProcessForSite(
      SiteInstanceImpl* site_instance) const {
    const auto reversed_site_process_set = base::Reversed(site_process_set_);
    if (site_instance->IsDefaultSiteInstance()) {
      // See if we can find an entry that maps to a site associated with the
      // default SiteInstance. This allows the default SiteInstance to reuse a
      // service worker process for any site that has been associated with it.
      for (const auto& site_process_pair : reversed_site_process_set) {
        if (site_instance->IsSiteInDefaultSiteInstance(site_process_pair.first))
          return site_process_pair;
      }
    } else {
      const GURL site_url(site_instance->GetSiteURL());
      for (const auto& site_process_pair : reversed_site_process_set) {
        if (site_process_pair.first == site_url)
          return site_process_pair;
      }
    }
    return SiteProcessIDPair();
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

RenderProcessHostImpl::DomStorageBinder& GetDomStorageBinder() {
  static base::NoDestructor<RenderProcessHostImpl::DomStorageBinder> binder;
  return *binder;
}

RenderProcessHostImpl::BroadcastChannelProviderReceiverHandler&
GetBroadcastChannelProviderReceiverHandler() {
  static base::NoDestructor<
      RenderProcessHostImpl::BroadcastChannelProviderReceiverHandler>
      instance;
  return *instance;
}

RenderProcessHostImpl::CodeCacheHostReceiverHandler&
GetCodeCacheHostReceiverHandler() {
  static base::NoDestructor<RenderProcessHostImpl::CodeCacheHostReceiverHandler>
      instance;
  return *instance;
}

// Keep track of plugin process IDs that require exceptions from CORB,
// request_initiator_site_lock checks (for particular origins), or both.
struct PluginExceptionsForNetworkService {
  bool is_corb_disabled = false;
  std::set<url::Origin> allowed_request_initiators;
};
std::map<int, PluginExceptionsForNetworkService>&
GetPluginExceptionsForNetworkService() {
  static base::NoDestructor<std::map<int, PluginExceptionsForNetworkService>>
      s_data;
  return *s_data;
}

void OnNetworkServiceCrashRestorePluginExceptions() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network::mojom::NetworkService* network_service = GetNetworkService();
  for (auto it : GetPluginExceptionsForNetworkService()) {
    const int process_id = it.first;
    const PluginExceptionsForNetworkService& exceptions = it.second;

    if (exceptions.is_corb_disabled)
      network_service->AddCorbExceptionForPlugin(process_id);

    for (const url::Origin& origin : exceptions.allowed_request_initiators)
      network_service->AddAllowedRequestInitiatorForPlugin(process_id, origin);
  }
}

void RemoveNetworkServicePluginExceptions(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetPluginExceptionsForNetworkService().erase(process_id);
  GetNetworkService()->RemoveSecurityExceptionsForPlugin(process_id);
}

bool PrepareToAddNewPluginExceptions(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* process = RenderProcessHostImpl::FromID(process_id);
  if (!process)
    return false;  // failure

  process->CleanupNetworkServicePluginExceptionsUponDestruction();

  static base::NoDestructor<
      std::unique_ptr<base::CallbackList<void()>::Subscription>>
      s_crash_handler_subscription;
  if (!*s_crash_handler_subscription) {
    *s_crash_handler_subscription = RegisterNetworkServiceCrashHandler(
        base::BindRepeating(&OnNetworkServiceCrashRestorePluginExceptions));
  }

  return true;  // success
}

void AddCorbExceptionForPluginOnUIThread(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!PrepareToAddNewPluginExceptions(process_id))
    return;

  GetPluginExceptionsForNetworkService()[process_id].is_corb_disabled = true;
  GetNetworkService()->AddCorbExceptionForPlugin(process_id);
}

void AddAllowedRequestInitiatorForPluginOnUIThread(
    int process_id,
    const url::Origin& allowed_request_initiator) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!PrepareToAddNewPluginExceptions(process_id))
    return;

  GetPluginExceptionsForNetworkService()[process_id]
      .allowed_request_initiators.insert(allowed_request_initiator);
  GetNetworkService()->AddAllowedRequestInitiatorForPlugin(
      process_id, allowed_request_initiator);
}

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
static constexpr size_t kUnknownPlatformProcessLimit = 0;

// Returns the process limit from the system. Use |kUnknownPlatformProcessLimit|
// to indicate failure and std::numeric_limits<size_t>::max() to indicate
// unlimited.
size_t GetPlatformProcessLimit() {
#if defined(OS_LINUX)
  struct rlimit limit;
  if (getrlimit(RLIMIT_NPROC, &limit) != 0)
    return kUnknownPlatformProcessLimit;

  if (limit.rlim_cur == RLIM_INFINITY)
    return std::numeric_limits<size_t>::max();
  return base::saturated_cast<size_t>(limit.rlim_cur);
#else
  // TODO(https://crbug.com/104689): Implement on other platforms.
  return kUnknownPlatformProcessLimit;
#endif  // defined(OS_LINUX)
}
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

RenderProcessHost::BindHostReceiverInterceptor&
GetBindHostReceiverInterceptor() {
  static base::NoDestructor<RenderProcessHost::BindHostReceiverInterceptor>
      interceptor;
  return *interceptor;
}

RenderProcessHostImpl::BatteryMonitorBinder& GetBatteryMonitorBinderOverride() {
  static base::NoDestructor<RenderProcessHostImpl::BatteryMonitorBinder> binder;
  return *binder;
}

void BindBatteryMonitor(
    mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) {
  const auto& binder = GetBatteryMonitorBinderOverride();
  if (binder)
    binder.Run(std::move(receiver));
  else
    GetDeviceService().BindBatteryMonitor(std::move(receiver));
}

RenderProcessHostImpl::CreateNetworkFactoryCallback&
GetCreateNetworkFactoryCallback() {
  static base::NoDestructor<RenderProcessHostImpl::CreateNetworkFactoryCallback>
      s_callback;
  return *s_callback;
}

RenderProcessHostImpl::BadMojoMessageCallbackForTesting&
GetBadMojoMessageCallbackForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<
      RenderProcessHostImpl::BadMojoMessageCallbackForTesting>
      s_callback;
  return *s_callback;
}

void InvokeBadMojoMessageCallbackForTesting(int render_process_id,
                                            const std::string& error) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&InvokeBadMojoMessageCallbackForTesting,
                                  render_process_id, error));
    return;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHostImpl::BadMojoMessageCallbackForTesting& callback =
      GetBadMojoMessageCallbackForTesting();
  if (!callback.is_null())
    callback.Run(render_process_id, error);
}

}  // namespace

// A RenderProcessHostImpl's IO thread implementation of the
// |mojom::ChildProcessHost| interface. This exists to allow the process host
// to bind incoming receivers on the IO-thread without a main-thread hop if
// necessary. Also owns the RPHI's |mojom::ChildProcess| remote.
class RenderProcessHostImpl::IOThreadHostImpl
    : public mojom::ChildProcessHostBootstrap,
      public mojom::ChildProcessHost {
 public:
  IOThreadHostImpl(int render_process_id,
                   base::WeakPtr<RenderProcessHostImpl> weak_host,
                   std::unique_ptr<service_manager::BinderRegistry> binders,
                   mojo::PendingReceiver<mojom::ChildProcessHostBootstrap>
                       bootstrap_receiver)
      : render_process_id_(render_process_id),
        weak_host_(std::move(weak_host)),
        binders_(std::move(binders)),
        bootstrap_receiver_(this, std::move(bootstrap_receiver)) {}
  ~IOThreadHostImpl() override = default;

 private:
  // mojom::ChildProcessHostBootstrap implementation:
  void BindProcessHost(
      mojo::PendingReceiver<mojom::ChildProcessHost> receiver) override {
    receiver_.Bind(std::move(receiver));
  }

  // mojom::ChildProcessHost implementation:
  void BindHostReceiver(mojo::GenericPendingReceiver receiver) override {
    const auto& interceptor = GetBindHostReceiverInterceptor();
    if (interceptor) {
      interceptor.Run(render_process_id_, &receiver);
      if (!receiver)
        return;
    }

#if defined(OS_LINUX)
    if (auto font_receiver = receiver.As<font_service::mojom::FontService>()) {
      ConnectToFontService(std::move(font_receiver));
      return;
    }
#endif

#if defined(OS_WIN)
    if (auto r = receiver.As<mojom::FontCacheWin>()) {
      FontCacheDispatcher::Create(std::move(r));
      return;
    }
#endif

#if defined(OS_MACOSX)
    if (auto r = receiver.As<mojom::SandboxSupportMac>()) {
      static base::NoDestructor<SandboxSupportMacImpl> sandbox_support;
      sandbox_support->BindReceiver(std::move(r));
      return;
    }
#endif

    if (auto r = receiver.As<
                 discardable_memory::mojom::DiscardableSharedMemoryManager>()) {
      discardable_memory::DiscardableSharedMemoryManager::Get()->Bind(
          std::move(r));
      return;
    }

    if (auto r = receiver.As<ukm::mojom::UkmRecorderInterface>()) {
      metrics::UkmRecorderInterface::Create(ukm::UkmRecorder::Get(),
                                            std::move(r));
      return;
    }

    std::string interface_name = *receiver.interface_name();
    mojo::ScopedMessagePipeHandle pipe = receiver.PassPipe();
    if (binders_->TryBindInterface(interface_name, &pipe))
      return;

    receiver = mojo::GenericPendingReceiver(interface_name, std::move(pipe));
    if (!receiver)
      return;

    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&IOThreadHostImpl::BindHostReceiverOnUIThread,
                                  weak_host_, std::move(receiver)));
  }

  static void BindHostReceiverOnUIThread(
      base::WeakPtr<RenderProcessHostImpl> weak_host,
      mojo::GenericPendingReceiver receiver) {
    if (weak_host)
      weak_host->OnBindHostReceiver(std::move(receiver));
  }

  const int render_process_id_;
  const base::WeakPtr<RenderProcessHostImpl> weak_host_;
  std::unique_ptr<service_manager::BinderRegistry> binders_;
  mojo::Receiver<mojom::ChildProcessHostBootstrap> bootstrap_receiver_;
  mojo::Receiver<mojom::ChildProcessHost> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(IOThreadHostImpl);
};

// static
scoped_refptr<base::SingleThreadTaskRunner>
RenderProcessHostImpl::GetInProcessRendererThreadTaskRunnerForTesting() {
  return g_in_process_thread->task_runner();
}

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
// static
size_t RenderProcessHostImpl::GetPlatformMaxRendererProcessCount() {
  // Set the limit to half of the system limit to leave room for other programs.
  size_t limit = GetPlatformProcessLimit() / 2;

  // If the system limit is unavailable, use a fallback value instead.
  if (limit == kUnknownPlatformProcessLimit) {
    static constexpr size_t kMaxRendererProcessCount = 82;
    limit = kMaxRendererProcessCount;
  }
  return limit;
}
#endif

// static
size_t RenderProcessHost::GetMaxRendererProcessCount() {
  if (g_max_renderer_count_override)
    return g_max_renderer_count_override;

#if defined(OS_ANDROID)
  // On Android we don't maintain a limit of renderer process hosts - we are
  // happy with keeping a lot of these, as long as the number of live renderer
  // processes remains reasonable, and on Android the OS takes care of that.
  return std::numeric_limits<size_t>::max();
#elif defined(OS_CHROMEOS)
  // On Chrome OS new renderer processes are very cheap and there's no OS
  // driven constraint on the number of processes, and the effectiveness
  // of the tab discarder is very poor when we have tabs sharing a
  // renderer process.  So, set a high limit, and based on UMA stats
  // for CrOS the 99.9th percentile of Tabs.MaxTabsInADay is around 100.
  return 100;
#else
  // On other platforms, calculate the maximum number of renderer process hosts
  // according to the amount of installed memory as reported by the OS, along
  // with some hard-coded limits. The calculation assumes that the renderers
  // will use up to half of the installed RAM and assumes that each WebContents
  // uses |kEstimatedWebContentsMemoryUsage| MB. If this assumption changes, the
  // ThirtyFourTabs test needs to be adjusted to match the expected number of
  // processes.
  //
  // Using the above assumptions, with the given amounts of installed memory
  // below on a 64-bit CPU, the maximum renderer count based on available RAM
  // alone will be as follows:
  //
  //   128 MB -> 0
  //   512 MB -> 3
  //  1024 MB -> 6
  //  4096 MB -> 24
  // 16384 MB -> 96
  //
  // Then the calculated value will be clamped by |kMinRendererProcessCount| and
  // GetPlatformMaxRendererProcessCount().

  static size_t max_count = 0;
  if (!max_count) {
    static constexpr size_t kEstimatedWebContentsMemoryUsage =
#if defined(ARCH_CPU_64_BITS)
        85;  // In MB
#else
        60;  // In MB
#endif
    max_count = base::SysInfo::AmountOfPhysicalMemoryMB() / 2;
    max_count /= kEstimatedWebContentsMemoryUsage;

    static constexpr size_t kMinRendererProcessCount = 3;
    static const size_t kMaxRendererProcessCount =
        RenderProcessHostImpl::GetPlatformMaxRendererProcessCount();
    DCHECK_LE(kMinRendererProcessCount, kMaxRendererProcessCount);

    max_count = base::ClampToRange(max_count, kMinRendererProcessCount,
                                   kMaxRendererProcessCount);
  }
  return max_count;
#endif
}

// static
void RenderProcessHost::SetMaxRendererProcessCount(size_t count) {
  g_max_renderer_count_override = count;
  if (GetAllHosts().size() > count) {
    SpareRenderProcessHostManager::GetInstance()
        .CleanupSpareRenderProcessHost();
  }
}

// static
int RenderProcessHost::GetCurrentRenderProcessCountForTesting() {
  RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
  int count = 0;
  while (!it.IsAtEnd()) {
    RenderProcessHost* host = it.GetCurrentValue();
    if (host->IsInitializedAndNotDead() &&
        host != RenderProcessHostImpl::GetSpareRenderProcessHostForTesting()) {
      count++;
    }
    it.Advance();
  }
  return count;
}

// static
RenderProcessHost* RenderProcessHostImpl::CreateRenderProcessHost(
    BrowserContext* browser_context,
    StoragePartitionImpl* storage_partition_impl,
    SiteInstance* site_instance) {
  if (g_render_process_host_factory_) {
    return g_render_process_host_factory_->CreateRenderProcessHost(
        browser_context, site_instance);
  }

  if (!storage_partition_impl) {
    storage_partition_impl = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetStoragePartition(browser_context, site_instance));
  }
  // If we've made a StoragePartition for guests (e.g., for the <webview> tag),
  // stash the Site URL on it. This way, when we start a service worker inside
  // this storage partition, we can create the appropriate SiteInstance for
  // finding a process (e.g., we will try to start a worker from
  // "https://example.com/sw.js" but need to use the guest site URL
  // to get a process in the guest's StoragePartition.)
  const bool is_for_guests_only = site_instance && site_instance->IsGuest();
  if (is_for_guests_only &&
      storage_partition_impl->site_for_guest_service_worker().is_empty()) {
    storage_partition_impl->set_site_for_guest_service_worker(
        site_instance->GetSiteURL());
  }

  return new RenderProcessHostImpl(browser_context, storage_partition_impl,
                                   is_for_guests_only);
}

// static
const unsigned int RenderProcessHostImpl::kMaxFrameDepthForPriority =
    std::numeric_limits<unsigned int>::max();

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
      visible_clients_(0),
      priority_(!blink::kLaunchingProcessIsBackgrounded,
                false /* has_media_stream */,
                false /* has_foreground_service_worker */,
                false /* all_low_priority_frames */,
                frame_depth_,
                false /* intersects_viewport */,
                true /* boost_for_pending_views */
#if defined(OS_ANDROID)
                ,
                ChildProcessImportance::NORMAL
#endif
                ),
      clock_(base::DefaultTickClock::GetInstance()),
      id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      browser_context_(browser_context),
      storage_partition_impl_(storage_partition_impl),
      sudden_termination_allowed_(true),
      is_blocked_(false),
      is_for_guests_only_(is_for_guests_only),
      is_unused_(true),
      delayed_cleanup_needed_(false),
      within_process_died_observer_(false),
      channel_connected_(false),
      sent_render_process_ready_(false),
      push_messaging_manager_(
          nullptr,
          base::OnTaskRunnerDeleter(base::CreateSequencedTaskRunner(
              {ServiceWorkerContext::GetCoreThreadId()}))),
      instance_weak_factory_(base::in_place, this),
      frame_sink_provider_(id_),
      shutdown_exit_code_(-1) {
  TRACE_EVENT2("shutdown", "RenderProcessHostImpl", "render_process_host", this,
               "id", GetID());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2("shutdown", "Browser.RenderProcessHostImpl",
                                    this, "render_process_host", this,
                                    "browser_context", browser_context_);
  widget_helper_ = new RenderWidgetHelper();
  resolve_proxy_helper_ = new ResolveProxyHelper(GetID());

  ChildProcessSecurityPolicyImpl::GetInstance()->Add(GetID(), browser_context);

  CHECK(!BrowserMainRunner::ExitedMainMessageLoop());
  RegisterHost(GetID(), this);
  GetAllHosts().set_check_on_null_data(true);
  // Initialize |child_process_activity_time_| to a reasonable value.
  mark_child_process_activity_time();

  if (!GetBrowserContext()->IsOffTheRecord() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&CacheShaderInfo, GetID(),
                                  storage_partition_impl_->GetPath()));
  }

  // This instance of PushMessagingManager is only used from clients
  // bound to service workers (i.e. PushProvider), since frame-bound
  // clients will rely on BrowserInterfaceBroker instead. Therefore,
  // pass an invalid frame ID here.
  //
  // Constructing the manager must occur after RegisterHost(), since
  // PushMessagingManager::Core looks up |this| using the process id.
  push_messaging_manager_.reset(new PushMessagingManager(
      GetID(),
      /* render_frame_id= */ ChildProcessHost::kInvalidUniqueID,
      storage_partition_impl_->GetServiceWorkerContext()));

  InitializeChannelProxy();

  const int id = GetID();
  const uint64_t tracing_id =
      ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(id);
  gpu_client_.reset(new viz::GpuClient(
      std::make_unique<BrowserGpuClientDelegate>(), id, tracing_id,
      base::CreateSingleThreadTaskRunner({BrowserThread::IO})));
}

// static
void RenderProcessHostImpl::ShutDownInProcessRenderer() {
  DCHECK(g_run_renderer_in_process);

  switch (GetAllHosts().size()) {
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

void RenderProcessHostImpl::SetDomStorageBinderForTesting(
    DomStorageBinder binder) {
  GetDomStorageBinder() = std::move(binder);
}

// static
void RenderProcessHostImpl::SetBadMojoMessageCallbackForTesting(
    BadMojoMessageCallbackForTesting callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // No support for setting the global callback twice.
  DCHECK_NE(callback.is_null(),
            GetBadMojoMessageCallbackForTesting().is_null());

  GetBadMojoMessageCallbackForTesting() = callback;
}

void RenderProcessHostImpl::
    SetBroadcastChannelProviderReceiverHandlerForTesting(
        BroadcastChannelProviderReceiverHandler handler) {
  GetBroadcastChannelProviderReceiverHandler() = handler;
}

void RenderProcessHostImpl::SetCodeCacheHostReceiverHandlerForTesting(
    CodeCacheHostReceiverHandler handler) {
  GetCodeCacheHostReceiverHandler() = handler;
}

RenderProcessHostImpl::~RenderProcessHostImpl() {
  TRACE_EVENT2("shutdown", "~RenderProcessHostImpl", "render_process_host",
               this, "id", GetID());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#ifndef NDEBUG
  DCHECK(is_self_deleted_)
      << "RenderProcessHostImpl is destroyed by something other than itself";
#endif

  // Make sure to clean up the in-process renderer before the channel, otherwise
  // it may still run and have its IPCs fail, causing asserts.
  in_process_renderer_.reset();
  g_in_process_thread = nullptr;

  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(GetID());

  is_dead_ = true;

  UnregisterHost(GetID());

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&RemoveShaderInfo, GetID()));
  }

  if (cleanup_network_service_plugin_exceptions_upon_destruction_)
    RemoveNetworkServicePluginExceptions(GetID());

  // Do reporting here for the priority of the frames seen by the host.
  FramePrioritiesSeen report = FramePrioritiesSeen::kNoFramesSeen;
  if (normal_priority_frames_seen_ && low_priority_frames_seen_) {
    report = FramePrioritiesSeen::kMixedPrioritiesSeen;
  } else if (normal_priority_frames_seen_) {
    report = FramePrioritiesSeen::kOnlyNormalPrioritiesSeen;
  } else if (low_priority_frames_seen_) {
    report = FramePrioritiesSeen::kOnlyLowPrioritiesSeen;
  }
  UMA_HISTOGRAM_ENUMERATION("BrowserRenderProcessHost.FramePrioritiesSeen",
                            report);

  // Report the histograms if the time is nonzero.  Note that LONG_TIMES records
  // times in exponential bins up to an hour, so it should sufficiently catch
  // most cases.
  if (!background_status_update_time_.is_null()) {
    base::TimeTicks current_time = clock_->NowTicks();
    base::TimeDelta total_duration = current_time - init_time_;

    // Only record for durations greater than zero.
    if (total_duration.InMicroseconds() > 0) {
      if (is_backgrounded_)
        background_duration_ += current_time - background_status_update_time_;
      UMA_HISTOGRAM_LONG_TIMES("BrowserRenderProcessHost.TotalTime",
                               total_duration);
      UMA_HISTOGRAM_LONG_TIMES("BrowserRenderProcessHost.BackgroundTime",
                               background_duration_);
    }
  }
  TRACE_EVENT_NESTABLE_ASYNC_END2("shutdown", "Cleanup in progress", this,
                                  "render_process_host", this,
                                  "browser_context", browser_context_);
  TRACE_EVENT_NESTABLE_ASYNC_END2("shutdown", "Browser.RenderProcessHostImpl",
                                  this, "render_process_host", this,
                                  "browser_context", browser_context_);
}

bool RenderProcessHostImpl::Init() {
  // calling Init() more than once does nothing, this makes it more convenient
  // for the view host which may not be sure in some cases
  if (IsInitializedAndNotDead())
    return true;

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
#elif defined(OS_MACOSX)
  int flags = ChildProcessHost::CHILD_RENDERER;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  // Find the renderer before creating the channel so if this fails early we
  // return without creating the channel.
  base::FilePath renderer_path = ChildProcessHost::GetChildPath(flags);
  if (renderer_path.empty())
    return false;

  is_initialized_ = true;
  is_dead_ = false;
  sent_render_process_ready_ = false;

  if (gpu_client_)
    gpu_client_->PreEstablishGpuChannel();

  // We may reach Init() during process death notification (e.g.
  // RenderProcessExited on some observer). In this case the Channel may be
  // null, so we re-initialize it here.
  if (!channel_)
    InitializeChannelProxy();

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

  FieldTrialSynchronizer::UpdateRendererVariationsHeader(this);

#if defined(OS_ANDROID)
  // Initialize the java audio manager so that media session tests will pass.
  // See internal b/29872494.
  static_cast<media::AudioManagerAndroid*>(media::AudioManager::Get())
      ->InitializeIfNeeded();
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
    in_process_renderer_.reset(g_renderer_main_thread_factory(
        InProcessChildThreadParams(
            base::CreateSingleThreadTaskRunner({BrowserThread::IO}),
            &mojo_invitation_),
        base::checked_cast<int32_t>(id_)));

    base::Thread::Options options;
#if defined(OS_WIN) && !defined(OS_MACOSX)
    // In-process plugins require this to be a UI message loop.
    options.message_pump_type = base::MessagePumpType::UI;
#else
    // We can't have multiple UI loops on Linux and Android, so we don't support
    // in-process plugins.
    options.message_pump_type = base::MessagePumpType::DEFAULT;
#endif
    // As for execution sequence, this callback should have no any dependency
    // on starting in-process-render-thread.
    // So put it here to trigger ChannelMojo initialization earlier to enable
    // in-process-render-thread using ChannelMojo there.
    OnProcessLaunched();  // Fake a callback that the process is ready.

    in_process_renderer_->StartWithOptions(options);

    g_in_process_thread = in_process_renderer_.get();

    // Make sure any queued messages on the channel are flushed in the case
    // where we aren't launching a child process.
    channel_->Flush();
  } else {
    // Build command line for renderer.  We call AppendRendererCommandLine()
    // first so the process type argument will appear first.
    std::unique_ptr<base::CommandLine> cmd_line =
        std::make_unique<base::CommandLine>(renderer_path);
    if (!renderer_prefix.empty())
      cmd_line->PrependWrapper(renderer_prefix);
    AppendRendererCommandLine(cmd_line.get());

    // Spawn the child process asynchronously to avoid blocking the UI thread.
    // As long as there's no renderer prefix, we can use the zygote process
    // at this stage.
    child_process_launcher_ = std::make_unique<ChildProcessLauncher>(
        std::make_unique<RendererSandboxedProcessLauncherDelegate>(),
        std::move(cmd_line), GetID(), this, std::move(mojo_invitation_),
        base::BindRepeating(&RenderProcessHostImpl::OnMojoError, id_),
        GetV8SnapshotFilesToPreload());
    channel_->Pause();

    // In single process mode, browser-side tracing and memory will cover the
    // whole process including renderers.
    BackgroundTracingManagerImpl::ActivateForProcess(GetID(),
                                                     child_process_.get());

    fast_shutdown_started_ = false;
  }

  init_time_ = clock_->NowTicks();
  background_status_update_time_ = init_time_;
  return true;
}

void RenderProcessHostImpl::EnableSendQueue() {
  if (!channel_)
    InitializeChannelProxy();
}

bool RenderProcessHostImpl::HasOnlyLowPriorityFrames() {
  return (low_priority_frames_ > 0) && (total_frames_ == low_priority_frames_);
}

void RenderProcessHostImpl::InitializeChannelProxy() {
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      base::CreateSingleThreadTaskRunner({BrowserThread::IO});

  // Establish a ChildProcess interface connection to the new renderer. This is
  // connected as the primordial message pipe via a Mojo invitation to the
  // process.
  mojo_invitation_ = {};
  child_process_.reset();
  child_process_.Bind(mojo::PendingRemote<mojom::ChildProcess>(
      mojo_invitation_.AttachMessagePipe(0), /*version=*/0));

  // Bootstrap the IPC Channel.
  mojo::PendingRemote<IPC::mojom::ChannelBootstrap> bootstrap;
  child_process_->BootstrapLegacyIpc(
      bootstrap.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<IPC::ChannelFactory> channel_factory =
      IPC::ChannelMojo::CreateServerFactory(
          bootstrap.PassPipe(), io_task_runner,
          base::ThreadTaskRunnerHandle::Get());

  ResetChannelProxy();

  // Do NOT expand ifdef or run time condition checks here! Synchronous
  // IPCs from browser process are banned. It is only narrowly allowed
  // for Android WebView to maintain backward compatibility.
  // See crbug.com/526842 for details.
#if defined(OS_ANDROID)
  if (GetContentClient()->UsingSynchronousCompositing()) {
    // Android never performs a clean shutdown, so we pass nullptr for
    // shudown_event to indicate that we never intend to signal a shutdown.
    channel_ =
        IPC::SyncChannel::Create(this, io_task_runner.get(),
                                 base::ThreadTaskRunnerHandle::Get(), nullptr);
  }
#endif  // OS_ANDROID
  if (!channel_) {
    channel_ = std::make_unique<IPC::ChannelProxy>(
        this, io_task_runner.get(), base::ThreadTaskRunnerHandle::Get());
  }
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
  remote_route_provider_.reset();
  channel_->GetRemoteAssociatedInterface(&remote_route_provider_);
  renderer_interface_.reset();
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
  MediaInternals* media_internals = MediaInternals::GetInstance();

  scoped_refptr<RenderMessageFilter> render_message_filter =
      base::MakeRefCounted<RenderMessageFilter>(
          GetID(), GetBrowserContext(), widget_helper_.get(), media_internals);
  AddFilter(render_message_filter.get());

  render_frame_message_filter_ = new RenderFrameMessageFilter(
      GetID(),
#if BUILDFLAG(ENABLE_PLUGINS)
      PluginServiceImpl::GetInstance(),
#else
      nullptr,
#endif
      GetBrowserContext(), storage_partition_impl_, widget_helper_.get());
  AddFilter(render_frame_message_filter_.get());

#if BUILDFLAG(ENABLE_PLUGINS)
  AddFilter(new PepperRendererConnection(GetID()));
#endif
#if defined(OS_MACOSX)
  AddFilter(new TextInputClientMessageFilter());
#endif

  p2p_socket_dispatcher_host_ =
      std::make_unique<P2PSocketDispatcherHost>(GetID());

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context(
      static_cast<ServiceWorkerContextWrapper*>(
          storage_partition_impl_->GetServiceWorkerContext()));
}

void RenderProcessHostImpl::BindCacheStorage(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter_remote,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_impl_->GetCacheStorageContext()->AddReceiver(
      cross_origin_embedder_policy, std::move(coep_reporter_remote), origin,
      std::move(receiver));
}

void RenderProcessHostImpl::BindIndexedDB(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (origin.opaque()) {
    // Opaque origins aren't valid for IndexedDB access, so we won't bind
    // |receiver| to |indexed_db_factory_|.  Return early here which
    // will cause |receiver| to be freed.  When |receiver| is
    // freed, we expect the pipe on the client will be closed.
    return;
  }
  storage_partition_impl_->GetIndexedDBControl().BindIndexedDB(
      origin, std::move(receiver));
}

void RenderProcessHostImpl::ForceCrash() {
  child_process_->CrashHungProcess();
}

void RenderProcessHostImpl::BindFileSystemManager(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Note, the base::Unretained() is safe because the target object has an IO
  // thread deleter and the callback is also targeting the IO thread.
  // TODO(https://crbug.com/873661): Pass origin to FileSystemManager.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FileSystemManagerImpl::BindReceiver,
                     base::Unretained(file_system_manager_impl_.get()),
                     std::move(receiver)));
}

void RenderProcessHostImpl::BindNativeFileSystemManager(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This code path is only for workers, hence always pass in
  // MSG_ROUTING_NONE as frame ID. Frames themselves go through
  // RenderFrameHostImpl instead.
  auto* storage_partition =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());
  auto* manager = storage_partition->GetNativeFileSystemManager();
  manager->BindReceiver(
      NativeFileSystemManagerImpl::BindingContext(
          origin,
          // TODO(https://crbug.com/989323): Obtain and use a better
          // URL for workers instead of the origin as source url.
          // This URL will be used for SafeBrowsing checks and for
          // the Quarantine Service.
          origin.GetURL(), GetID(), MSG_ROUTING_NONE),
      std::move(receiver));
}

void RenderProcessHostImpl::BindNativeIOHost(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());
  storage_partition->GetNativeIOContext()->BindReceiver(origin,
                                                        std::move(receiver));
}

void RenderProcessHostImpl::SetClockForTesting(base::TickClock* clock) {
  clock_ = clock;
  init_time_ = clock_->NowTicks();
  background_status_update_time_ = init_time_;
}

void RenderProcessHostImpl::BindRestrictedCookieManagerForServiceWorker(
    const url::Origin& origin,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StoragePartitionImpl* storage_partition =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());
  storage_partition->CreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole::SCRIPT, origin,
      net::SiteForCookies::FromOrigin(origin), origin,
      true /* is_service_worker */, GetID(), MSG_ROUTING_NONE,
      std::move(receiver),
      storage_partition->CreateCookieAccessObserverForServiceWorker());
}

void RenderProcessHostImpl::BindVideoDecodePerfHistory(
    mojo::PendingReceiver<media::mojom::VideoDecodePerfHistory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetBrowserContext()->GetVideoDecodePerfHistory()->BindReceiver(
      std::move(receiver));
}

void RenderProcessHostImpl::BindQuotaManagerHost(
    int render_frame_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::QuotaManagerHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* storage_partition =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());
  storage_partition->GetQuotaContext()->BindQuotaManagerHost(
      GetID(), render_frame_id, origin, std::move(receiver));
}

void RenderProcessHostImpl::CreateLockManager(
    int render_frame_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_impl_->GetLockManager()->BindReceiver(
      GetID(), render_frame_id, origin, std::move(receiver));
}

void RenderProcessHostImpl::CreatePermissionService(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!permission_service_context_)
    permission_service_context_.reset(new PermissionServiceContext(this));

  permission_service_context_->CreateServiceForWorker(origin,
                                                      std::move(receiver));
}

void RenderProcessHostImpl::CreatePaymentManagerForOrigin(
    const url::Origin& origin,
    mojo::PendingReceiver<payments::mojom::PaymentManager> receiver) {
  static_cast<StoragePartitionImpl*>(GetStoragePartition())
      ->GetPaymentAppContext()
      ->CreatePaymentManagerForOrigin(origin, std::move(receiver));
}

void RenderProcessHostImpl::CreateNotificationService(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NotificationService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_impl_->GetPlatformNotificationContext()->CreateService(
      origin, std::move(receiver));
}

void RenderProcessHostImpl::CreateWebSocketConnector(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver) {
  // TODO(jam): is it ok to not send extraHeaders for sockets created from
  // shared and service workers?
  //
  // Shared Workers and service workers are not directly associated with a
  // frame, so the concept of "top-level frame" does not exist. Can use
  // (origin, origin, origin) for the IsolationInfo for requests because these
  // workers can only be created when the site has cookie access.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebSocketConnectorImpl>(
          GetID(), MSG_ROUTING_NONE, origin,
          net::IsolationInfo::Create(
              net::IsolationInfo::RedirectMode::kUpdateNothing, origin, origin,
              net::SiteForCookies::FromOrigin(origin))),
      std::move(receiver));
}

void RenderProcessHostImpl::CancelProcessShutdownDelayForUnload() {
  if (IsKeepAliveRefCountDisabled())
    return;
  DecrementKeepAliveRefCount();
}

void RenderProcessHostImpl::DelayProcessShutdownForUnload(
    const base::TimeDelta& timeout) {
  // No need to delay shutdown if the process is already shutting down.
  if (IsKeepAliveRefCountDisabled() || deleting_soon_ || fast_shutdown_started_)
    return;

  IncrementKeepAliveRefCount();
  base::PostDelayedTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &RenderProcessHostImpl::CancelProcessShutdownDelayForUnload,
          weak_factory_.GetWeakPtr()),
      timeout);
}

// static
void RenderProcessHostImpl::AddCorbExceptionForPlugin(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&AddCorbExceptionForPluginOnUIThread, process_id));
}

// static
void RenderProcessHostImpl::AddAllowedRequestInitiatorForPlugin(
    int process_id,
    const url::Origin& allowed_request_initiator) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&AddAllowedRequestInitiatorForPluginOnUIThread,
                                process_id, allowed_request_initiator));
}

void RenderProcessHostImpl::
    CleanupNetworkServicePluginExceptionsUponDestruction() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cleanup_network_service_plugin_exceptions_upon_destruction_ = true;
}

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
void RenderProcessHostImpl::DumpProfilingData(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetRendererInterface()->WriteClangProfilingProfile(std::move(callback));
}
#endif

PeerConnectionTrackerHost*
RenderProcessHostImpl::GetPeerConnectionTrackerHost() {
  if (!peer_connection_tracker_host_) {
    peer_connection_tracker_host_ =
        std::make_unique<PeerConnectionTrackerHost>(this);
  }
  return peer_connection_tracker_host_.get();
}

// static
void RenderProcessHostImpl::OverrideBatteryMonitorBinderForTesting(
    BatteryMonitorBinder binder) {
  GetBatteryMonitorBinderOverride() = std::move(binder);
}

void RenderProcessHostImpl::RegisterMojoInterfaces() {
  auto registry = std::make_unique<service_manager::BinderRegistry>();

  AddUIThreadInterface(registry.get(),
                       base::BindRepeating(&BindBatteryMonitor));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](mojo::PendingReceiver<device::mojom::TimeZoneMonitor> receiver) {
            GetDeviceService().BindTimeZoneMonitor(std::move(receiver));
          }));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](mojo::PendingReceiver<device::mojom::PowerMonitor> receiver) {
            GetDeviceService().BindPowerMonitor(std::move(receiver));
          }));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](mojo::PendingReceiver<device::mojom::ScreenOrientationListener>
                 receiver) {
            GetDeviceService().BindScreenOrientationListener(
                std::move(receiver));
          }));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          &RenderProcessHostImpl::CreateEmbeddedFrameSinkProvider,
          weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindFrameSinkProvider,
                          weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindCompositingModeReporter,
                          weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::CreateOneShotSyncService,
                          weak_factory_.GetWeakPtr()));
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::CreatePeriodicSyncService,
                          weak_factory_.GetWeakPtr()));
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::CreateDomStorageProvider,
                          weak_factory_.GetWeakPtr()));
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          &RenderProcessHostImpl::CreateBroadcastChannelProvider,
          weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindWebDatabaseHostImpl,
                          weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          [](base::WeakPtr<RenderProcessHostImpl> host,
             mojo::PendingReceiver<
                 memory_instrumentation::mojom::CoordinatorConnector>
                 receiver) {
            if (!host)
              return;
            host->coordinator_connector_receiver_.reset();
            host->coordinator_connector_receiver_.Bind(std::move(receiver));
            if (!host->GetProcess().IsValid()) {
              // We only want to accept messages from this interface once we
              // have a known PID.
              host->coordinator_connector_receiver_.Pause();
            }
          },
          weak_factory_.GetWeakPtr()));

  registry->AddInterface(
      base::BindRepeating(&MimeRegistryImpl::Create),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_BLOCKING}));
#if BUILDFLAG(USE_MINIKIN_HYPHENATION)
  registry->AddInterface(
      base::BindRepeating(&hyphenation::HyphenationImpl::Create),
      hyphenation::HyphenationImpl::GetTaskRunner());
#endif
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kFontSrcLocalMatching)) {
    registry->AddInterface(
        base::BindRepeating(&FontUniqueNameLookupService::Create),
        FontUniqueNameLookupService::GetTaskRunner());
  }
#endif

#if defined(OS_WIN)
  registry->AddInterface(
      base::BindRepeating(&DWriteFontProxyImpl::Create),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING, base::MayBlock()}));
#endif

  registry->AddInterface(
      base::BindRepeating(&device::GamepadHapticsManager::Create));

  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    AddUIThreadInterface(
        registry.get(),
        base::BindRepeating(&RenderProcessHostImpl::BindPushMessagingManager,
                            weak_factory_.GetWeakPtr()));
  } else {
    // Note, the base::Unretained() is safe because the target object has an IO
    // thread deleter and the callback is also targeting the IO thread.  When
    // the RPHI is destroyed it also triggers the destruction of the registry
    // on the IO thread.
    registry->AddInterface(
        base::BindRepeating(&PushMessagingManager::AddPushMessagingReceiver,
                            base::Unretained(push_messaging_manager_.get())));
  }

  file_system_manager_impl_.reset(new FileSystemManagerImpl(
      GetID(), storage_partition_impl_->GetFileSystemContext(),
      ChromeBlobStorageContext::GetFor(GetBrowserContext())));
  // This interface is still exposed by the RenderProcessHost's registry so
  // that it can be accessed by PepperFileSystemHost. Blink accesses this
  // interface through RenderFrameHost/RendererInterfaceBinders.
  //
  // Note, the base::Unretained() is safe because the target object has an IO
  // thread deleter and the callback is also targeting the IO thread.  When
  // the RPHI is destroyed it also triggers the destruction of the registry
  // on the IO thread.
  //
  // TODO(https://crbug.com/873661): Make PepperFileSystemHost access this with
  // the RenderFrameHost's registry, and remove this registration.
  registry->AddInterface(
      base::BindRepeating(&FileSystemManagerImpl::BindReceiver,
                          base::Unretained(file_system_manager_impl_.get())));

  registry->AddInterface(base::BindRepeating(
      &MidiHost::BindReceiver, GetID(),
      base::Unretained(BrowserMainLoop::GetInstance()->midi_service())));

  if (gpu_client_) {
    // |gpu_client_| outlives the registry, because its destruction is posted to
    // IO thread from the destructor of |this|.
    registry->AddInterface(base::BindRepeating(
        &viz::GpuClient::Add, base::Unretained(gpu_client_.get())));
  }

  MediaStreamManager* media_stream_manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();

  registry->AddInterface(base::BindRepeating(&VideoCaptureHost::Create, GetID(),
                                             media_stream_manager));

  registry->AddInterface(
      base::BindRepeating(&FileUtilitiesHostImpl::Create, GetID()),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}));

  // Note, the base::Unretained() is safe because the target object has an IO
  // thread deleter and the callback is also targeting the IO thread.  When
  // the RPHI is destroyed it also triggers the destruction of the registry
  // on the IO thread.
  media_stream_track_metrics_host_.reset(new MediaStreamTrackMetricsHost());
  registry->AddInterface(base::BindRepeating(
      &MediaStreamTrackMetricsHost::BindReceiver,
      base::Unretained(media_stream_track_metrics_host_.get())));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          &RenderProcessHostImpl::CreateAgentMetricsCollectorHost,
          weak_factory_.GetWeakPtr()));

  registry->AddInterface(
      base::BindRepeating(&metrics::CreateSingleSampleMetricsProvider));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindPeerConnectionTrackerHost,
                          weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::CreateCodeCacheHost,
                          weak_factory_.GetWeakPtr()));

#if BUILDFLAG(ENABLE_REPORTING)
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&CreateReportingServiceProxy, GetID()));
#endif  // BUILDFLAG(ENABLE_REPORTING)

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindP2PSocketManager,
                          weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::CreateMediaLogRecordHost,
                          weak_factory_.GetWeakPtr()));

#if BUILDFLAG(ENABLE_MDNS)
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::CreateMdnsResponder,
                          weak_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(ENABLE_MDNS)

  AddUIThreadInterface(registry.get(),
                       base::BindRepeating(&FieldTrialRecorder::Create));

  associated_interfaces_ =
      std::make_unique<blink::AssociatedInterfaceRegistry>();
  blink::AssociatedInterfaceRegistry* associated_registry =
      associated_interfaces_.get();

  // This base::Unretained() usage is safe since the associated_registry is
  // owned by this RPHI.
  associated_registry->AddInterface(base::BindRepeating(
      &RenderProcessHostImpl::BindRouteProvider, base::Unretained(this)));
  associated_registry->AddInterface(base::BindRepeating(
      &RenderProcessHostImpl::CreateRendererHost, base::Unretained(this)));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(
          &RenderProcessHostImpl::CreateURLLoaderFactoryForRendererProcess,
          weak_factory_.GetWeakPtr()));

  registry->AddInterface(
      base::BindRepeating(&BlobRegistryWrapper::Bind,
                          storage_partition_impl_->GetBlobRegistry(), GetID()));

#if BUILDFLAG(ENABLE_PLUGINS)
  // Initialization can happen more than once (in the case of a child process
  // crash), but we don't want to lose the plugin registry in this case.
  if (!plugin_registry_) {
    plugin_registry_ = std::make_unique<PluginRegistryImpl>(GetID());
  }
  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindPluginRegistry,
                          weak_factory_.GetWeakPtr()));
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  registry->AddInterface(base::BindRepeating(&KeySystemSupportImpl::Create));
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindVideoDecoderService,
                          weak_factory_.GetWeakPtr()));

  AddUIThreadInterface(
      registry.get(),
      base::BindRepeating(&RenderProcessHostImpl::BindAecDumpManager,
                          weak_factory_.GetWeakPtr()));

  // ---- Please do not register interfaces below this line ------
  //
  // This call should be done after registering all interfaces above, so that
  // embedder can override any interfaces. The fact that registry calls
  // the last registration for the name allows us to easily override interfaces.
  GetContentClient()->browser()->ExposeInterfacesToRenderer(
      registry.get(), associated_interfaces_.get(), this);

  mojo::PendingRemote<mojom::ChildProcessHostBootstrap> bootstrap_remote;
  io_thread_host_impl_.emplace(
      base::CreateSingleThreadTaskRunner({BrowserThread::IO}), GetID(),
      instance_weak_factory_->GetWeakPtr(), std::move(registry),
      bootstrap_remote.InitWithNewPipeAndPassReceiver());
  child_process_->Initialize(std::move(bootstrap_remote));
}

void RenderProcessHostImpl::BindRouteProvider(
    mojo::PendingAssociatedReceiver<mojom::RouteProvider> receiver) {
  if (route_provider_receiver_.is_bound())
    return;
  route_provider_receiver_.Bind(std::move(receiver));
}

void RenderProcessHostImpl::GetRoute(
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  DCHECK(receiver.is_valid());
  associated_interface_provider_receivers_.Add(this, std::move(receiver),
                                               routing_id);
}

void RenderProcessHostImpl::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  int32_t routing_id =
      associated_interface_provider_receivers_.current_context();
  IPC::Listener* listener = listeners_.Lookup(routing_id);
  if (listener)
    listener->OnAssociatedInterfaceRequest(name, receiver.PassHandle());
}

void RenderProcessHostImpl::CreateEmbeddedFrameSinkProvider(
    mojo::PendingReceiver<blink::mojom::EmbeddedFrameSinkProvider> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!embedded_frame_sink_provider_) {
    // The client id gets converted to a uint32_t in FrameSinkId.
    uint32_t renderer_client_id = base::checked_cast<uint32_t>(id_);
    embedded_frame_sink_provider_ =
        std::make_unique<EmbeddedFrameSinkProviderImpl>(
            GetHostFrameSinkManager(), renderer_client_id);
  }
  embedded_frame_sink_provider_->Add(std::move(receiver));
}

void RenderProcessHostImpl::BindFrameSinkProvider(
    mojo::PendingReceiver<mojom::FrameSinkProvider> receiver) {
  frame_sink_provider_.Bind(std::move(receiver));
}

void RenderProcessHostImpl::BindCompositingModeReporter(
    mojo::PendingReceiver<viz::mojom::CompositingModeReporter> receiver) {
  BrowserMainLoop::GetInstance()->GetCompositingModeReporter(
      std::move(receiver));
}

void RenderProcessHostImpl::CreateDomStorageProvider(
    mojo::PendingReceiver<blink::mojom::DomStorageProvider> receiver) {
  DCHECK(!dom_storage_provider_receiver_.is_bound());
  dom_storage_provider_receiver_.Bind(std::move(receiver));
}

void RenderProcessHostImpl::CreateBroadcastChannelProvider(
    mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver) {
  if (!GetBroadcastChannelProviderReceiverHandler().is_null()) {
    GetBroadcastChannelProviderReceiverHandler().Run(this, std::move(receiver));
    return;
  }

  storage_partition_impl_->GetBroadcastChannelProvider()->Connect(
      ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(id_),
      std::move(receiver));
}

void RenderProcessHostImpl::CreateCodeCacheHost(
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // There should be at most one CodeCacheHostImpl for any given
  // RenderProcessHost.
  DCHECK(!code_cache_host_impl_);

  // Create a new CodeCacheHostImpl and bind it to the given receiver.
  code_cache_host_impl_ = std::make_unique<CodeCacheHostImpl>(
      GetID(), storage_partition_impl_->GetCacheStorageContext(),
      storage_partition_impl_->GetGeneratedCodeCacheContext(),
      std::move(receiver));

  // If there is a callback registered, then invoke it with the newly
  // created CodeCacheHostImpl.
  if (!GetCodeCacheHostReceiverHandler().is_null()) {
    GetCodeCacheHostReceiverHandler().Run(this, code_cache_host_impl_.get());
  }
}

void RenderProcessHostImpl::BindVideoDecoderService(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver) {
  if (!video_decoder_proxy_)
    video_decoder_proxy_.reset(new VideoDecoderProxy());
  video_decoder_proxy_->Add(std::move(receiver));
}

void RenderProcessHostImpl::BindWebDatabaseHostImpl(
    mojo::PendingReceiver<blink::mojom::WebDatabaseHost> receiver) {
  storage::DatabaseTracker* db_tracker =
      storage_partition_impl_->GetDatabaseTracker();
  db_tracker->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebDatabaseHostImpl::Create, GetID(),
                     base::WrapRefCounted(db_tracker), std::move(receiver)));
}
void RenderProcessHostImpl::BindAecDumpManager(
    mojo::PendingReceiver<blink::mojom::AecDumpManager> receiver) {
  aec_dump_manager_.AddReceiver(std::move(receiver));
}

void RenderProcessHostImpl::CreateOneShotSyncService(
    mojo::PendingReceiver<blink::mojom::OneShotBackgroundSyncService>
        receiver) {
  storage_partition_impl_->GetBackgroundSyncContext()->CreateOneShotSyncService(
      std::move(receiver));
}

void RenderProcessHostImpl::CreatePeriodicSyncService(
    mojo::PendingReceiver<blink::mojom::PeriodicBackgroundSyncService>
        receiver) {
  storage_partition_impl_->GetBackgroundSyncContext()
      ->CreatePeriodicSyncService(std::move(receiver));
}

void RenderProcessHostImpl::BindPushMessagingManager(
    mojo::PendingReceiver<blink::mojom::PushMessaging> receiver) {
  push_messaging_manager_->AddPushMessagingReceiver(std::move(receiver));
}

void RenderProcessHostImpl::BindP2PSocketManager(
    mojo::PendingReceiver<network::mojom::P2PSocketManager> receiver) {
  p2p_socket_dispatcher_host_->BindReceiver(std::move(receiver));
}

void RenderProcessHostImpl::CreateMediaLogRecordHost(
    mojo::PendingReceiver<content::mojom::MediaInternalLogRecords> receiver) {
  content::MediaInternals::CreateMediaLogRecords(GetID(), std::move(receiver));
}

#if BUILDFLAG(ENABLE_PLUGINS)
void RenderProcessHostImpl::BindPluginRegistry(
    mojo::PendingReceiver<blink::mojom::PluginRegistry> receiver) {
  plugin_registry_->Bind(std::move(receiver));
}
#endif

void RenderProcessHostImpl::BindDomStorage(
    mojo::PendingReceiver<blink::mojom::DomStorage> receiver,
    mojo::PendingRemote<blink::mojom::DomStorageClient> client) {
  const DomStorageBinder& binder = GetDomStorageBinder();
  if (binder) {
    binder.Run(this, std::move(receiver));
    return;
  }

  dom_storage_receiver_ids_.insert(storage_partition_impl_->BindDomStorage(
      id_, std::move(receiver), std::move(client)));

  // Renderers only use this interface to send a single BindDomStorage message,
  // so we can tear down the receiver now.
  dom_storage_provider_receiver_.reset();
}

void RenderProcessHostImpl::RegisterCoordinatorClient(
    mojo::PendingReceiver<memory_instrumentation::mojom::Coordinator> receiver,
    mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess>
        client_process) {
  if (!GetProcess().IsValid()) {
    // If the process dies before we get this message. we have no valid PID
    // and there's nothing to register.
    return;
  }

  base::trace_event::MemoryDumpManager::GetInstance()
      ->GetDumpThreadTaskRunner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](mojo::PendingReceiver<
                     memory_instrumentation::mojom::Coordinator> receiver,
                 mojo::PendingRemote<
                     memory_instrumentation::mojom::ClientProcess>
                     client_process,
                 base::ProcessId pid) {
                GetMemoryInstrumentationCoordinatorController()
                    ->RegisterClientProcess(
                        std::move(receiver), std::move(client_process),
                        memory_instrumentation::mojom::ProcessType::RENDERER,
                        pid,
                        /*service_name=*/base::nullopt);
              },
              std::move(receiver), std::move(client_process),
              GetProcess().Pid()));

  coordinator_connector_receiver_.reset();
}

void RenderProcessHostImpl::CreateRendererHost(
    mojo::PendingAssociatedReceiver<mojom::RendererHost> receiver) {
  renderer_host_receiver_.Bind(std::move(receiver));
}

int RenderProcessHostImpl::GetNextRoutingID() {
  return widget_helper_->GetNextRoutingID();
}

void RenderProcessHostImpl::BindReceiver(
    mojo::GenericPendingReceiver receiver) {
  child_process_->BindReceiver(std::move(receiver));
}

std::unique_ptr<base::PersistentMemoryAllocator>
RenderProcessHostImpl::TakeMetricsAllocator() {
  return std::move(metrics_allocator_);
}

const base::TimeTicks&
RenderProcessHostImpl::GetInitTimeForNavigationMetrics() {
  return init_time_;
}

bool RenderProcessHostImpl::IsProcessBackgrounded() {
  return priority_.is_background();
}

void RenderProcessHostImpl::IncrementKeepAliveRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_keep_alive_ref_count_disabled_);
  ++keep_alive_ref_count_;
  if (keep_alive_ref_count_ == 1)
    GetRendererInterface()->SetSchedulerKeepActive(true);
}

void RenderProcessHostImpl::DecrementKeepAliveRefCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_keep_alive_ref_count_disabled_);
  DCHECK_GT(keep_alive_ref_count_, 0U);
  --keep_alive_ref_count_;
  if (keep_alive_ref_count_ == 0) {
    Cleanup();
    GetRendererInterface()->SetSchedulerKeepActive(false);
  }
}

void RenderProcessHostImpl::DisableKeepAliveRefCount() {
  TRACE_EVENT2("shutdown", "RenderProcessHostImpl::DisableKeepAliveRefCount",
               "browser_context", browser_context_, "render_process_host",
               this);

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (is_keep_alive_ref_count_disabled_)
    return;
  is_keep_alive_ref_count_disabled_ = true;

  keep_alive_ref_count_ = 0;
  // Cleaning up will also remove this from the SpareRenderProcessHostManager.
  // (in this case |keep_alive_ref_count_| would be 0 even before).
  Cleanup();
}

bool RenderProcessHostImpl::IsKeepAliveRefCountDisabled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_keep_alive_ref_count_disabled_;
}

void RenderProcessHostImpl::Resume() {}

mojom::Renderer* RenderProcessHostImpl::GetRendererInterface() {
  return renderer_interface_.get();
}

// static
void RenderProcessHostImpl::SetNetworkFactoryForTesting(
    const CreateNetworkFactoryCallback& create_network_factory_callback) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(create_network_factory_callback.is_null() ||
         GetCreateNetworkFactoryCallback().is_null())
      << "It is not expected that this is called with non-null callback when "
      << "another overriding callback is already set.";
  GetCreateNetworkFactoryCallback() = create_network_factory_callback;
}

void RenderProcessHostImpl::CreateURLLoaderFactoryForRendererProcess(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CreateURLLoaderFactory(
      std::move(receiver),
      URLLoaderFactoryParamsHelper::CreateForRendererProcess(this));
}

void RenderProcessHostImpl::CreateURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    network::mojom::URLLoaderFactoryParamsPtr params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(params);
  DCHECK_EQ(GetID(), static_cast<int>(params->process_id));

  if (GetCreateNetworkFactoryCallback().is_null()) {
    storage_partition_impl_->GetNetworkContext()->CreateURLLoaderFactory(
        std::move(receiver), std::move(params));
  } else {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory;
    storage_partition_impl_->GetNetworkContext()->CreateURLLoaderFactory(
        original_factory.InitWithNewPipeAndPassReceiver(), std::move(params));
    GetCreateNetworkFactoryCallback().Run(std::move(receiver), GetID(),
                                          std::move(original_factory));
  }
}

bool RenderProcessHostImpl::MayReuseHost() {
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
  TRACE_EVENT2("shutdown", "RenderProcessHostImpl::AddRoute",
               "render_process_host", this, "routing_id", routing_id);
  CHECK(!listeners_.Lookup(routing_id))
      << "Found Routing ID Conflict: " << routing_id;
  listeners_.AddWithID(listener, routing_id);
}

void RenderProcessHostImpl::RemoveRoute(int32_t routing_id) {
  TRACE_EVENT2("shutdown", "RenderProcessHostImpl::RemoveRoute",
               "render_process_host", this, "routing_id", routing_id);
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
  // browser to try to survive when it gets illegal messages from the
  // renderer.
  Shutdown(RESULT_CODE_KILLED_BAD_MESSAGE);

  if (crash_report_mode == CrashReportMode::GENERATE_CRASH_DUMP) {
    // Set crash keys to understand renderer kills related to site isolation.
    ChildProcessSecurityPolicyImpl::GetInstance()->LogKilledProcessOriginLock(
        GetID());

    std::string site_isolation_mode;
    if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
      site_isolation_mode += "spp ";
    if (SiteIsolationPolicy::AreIsolatedOriginsEnabled())
      site_isolation_mode += "io ";
    if (SiteIsolationPolicy::IsStrictOriginIsolationEnabled())
      site_isolation_mode += "soi ";
    if (site_isolation_mode.empty())
      site_isolation_mode = "(none)";

    static auto* isolation_mode_key = base::debug::AllocateCrashKeyString(
        "site_isolation_mode", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(isolation_mode_key, site_isolation_mode);

    // Report a crash, since none will be generated by the killed renderer.
    base::debug::DumpWithoutCrashing();
  }

  // Log the renderer kill to the histogram tracking all kills.
  BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
      PROCESS_TYPE_RENDERER);
}

void RenderProcessHostImpl::UpdateClientPriority(PriorityClient* client) {
  DCHECK(client);
  DCHECK_EQ(1u, priority_clients_.count(client));
  UpdateProcessPriorityInputs();
}

void RenderProcessHostImpl::UpdateFrameWithPriority(
    base::Optional<FramePriority> previous_priority,
    base::Optional<FramePriority> new_priority) {
  // Record the priority of the frames seens so we know for reporting what
  // combination of normal and low priority frames have been seen.
  if (new_priority) {
    if (new_priority == FramePriority::kNormal) {
      normal_priority_frames_seen_ = true;
    } else {
      low_priority_frames_seen_ = true;
    }
  }

  // If we're not using frame priorities, return after recording.
  if (!base::FeatureList::IsEnabled(
          features::kUseFramePriorityInRenderProcessHost)) {
    return;
  }

  const bool previous_all_low_priority_frames = HasOnlyLowPriorityFrames();
  total_frames_ =
      total_frames_ - (previous_priority ? 1 : 0) + (new_priority ? 1 : 0);
  low_priority_frames_ =
      low_priority_frames_ -
      (previous_priority && previous_priority == FramePriority::kLow ? 1 : 0) +
      (new_priority && new_priority == FramePriority::kLow ? 1 : 0);
  if (previous_all_low_priority_frames != HasOnlyLowPriorityFrames())
    UpdateProcessPriority();
}

int RenderProcessHostImpl::VisibleClientCount() {
  return visible_clients_;
}

unsigned int RenderProcessHostImpl::GetFrameDepth() {
  return frame_depth_;
}

bool RenderProcessHostImpl::GetIntersectsViewport() {
  return intersects_viewport_;
}

#if defined(OS_ANDROID)
ChildProcessImportance RenderProcessHostImpl::GetEffectiveImportance() {
  return effective_importance_;
}

void RenderProcessHostImpl::DumpProcessStack() {
  if (child_process_launcher_)
    child_process_launcher_->DumpProcessStack();
}
#endif

void RenderProcessHostImpl::OnMediaStreamAdded() {
  ++media_stream_count_;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::OnMediaStreamRemoved() {
  DCHECK_GT(media_stream_count_, 0);
  --media_stream_count_;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::OnForegroundServiceWorkerAdded() {
  foreground_service_worker_count_ += 1;
  UpdateProcessPriority();
}

void RenderProcessHostImpl::OnForegroundServiceWorkerRemoved() {
  DCHECK_GT(foreground_service_worker_count_, 0);
  foreground_service_worker_count_ -= 1;
  UpdateProcessPriority();
}

// static
void RenderProcessHostImpl::set_render_process_host_factory_for_testing(
    RenderProcessHostFactory* rph_factory) {
  g_render_process_host_factory_ = rph_factory;
}

// static
RenderProcessHostFactory*
RenderProcessHostImpl::get_render_process_host_factory_for_testing() {
  return g_render_process_host_factory_;
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
void RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedBrowserContext(
    BrowserContext* browser_context) {
  SpareRenderProcessHostManager::GetInstance().PrepareForFutureRequests(
      browser_context);
}

// static
RenderProcessHost* RenderProcessHost::GetSpareRenderProcessHostForTesting() {
  return SpareRenderProcessHostManager::GetInstance()
      .spare_render_process_host();
}

// static
void RenderProcessHostImpl::DiscardSpareRenderProcessHostForTesting() {
  SpareRenderProcessHostManager::GetInstance().CleanupSpareRenderProcessHost();
}

// static
bool RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes() {
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return false;

  if (!base::FeatureList::IsEnabled(features::kSpareRendererForSitePerProcess))
    return false;

  // Spare renderer actually hurts performance on low-memory devices.  See
  // https://crbug.com/843775 for more details.
  //
  // The comparison below is using 1077 rather than 1024 because 1) this helps
  // ensure that devices with exactly 1GB of RAM won't get included because of
  // inaccuracies or off-by-one errors and 2) this is the bucket boundary in
  // Memory.Stats.Win.TotalPhys2.
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <= 1077)
    return false;

  return true;
}

bool RenderProcessHostImpl::HostHasNotBeenUsed() {
  return IsUnused() && listeners_.IsEmpty() && keep_alive_ref_count_ == 0 &&
         pending_views_ == 0;
}

void RenderProcessHostImpl::LockToOrigin(
    const IsolationContext& isolation_context,
    const GURL& lock_url) {
  ChildProcessSecurityPolicyImpl::GetInstance()->LockToOrigin(
      isolation_context, GetID(), lock_url);

  // Note that LockToOrigin is only called once per RenderProcessHostImpl
  // (when committing a navigation into an empty renderer).  Therefore, the
  // call to NotifyRendererIfLockedToSite below is insufficient for setting up
  // renderers respawned after crashing - this is handled by another call to
  // NotifyRendererIfLockedToSite from OnProcessLaunched.
  NotifyRendererIfLockedToSite();
}

void RenderProcessHostImpl::NotifyRendererIfLockedToSite() {
  GURL lock_url =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetOriginLock(GetID());
  if (!lock_url.is_valid())
    return;

  if (!SiteInstanceImpl::IsOriginLockASite(lock_url))
    return;

  GetRendererInterface()->SetIsLockedToSite();
}

bool RenderProcessHostImpl::IsForGuestsOnly() {
  return is_for_guests_only_;
}

StoragePartition* RenderProcessHostImpl::GetStoragePartition() {
  return storage_partition_impl_;
}

static void AppendCompositorCommandLineFlags(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(
      switches::kNumRasterThreads,
      base::NumberToString(NumberOfRendererRasterThreads()));

  int msaa_sample_count = GpuRasterizationMSAASampleCount();
  if (msaa_sample_count >= 0) {
    command_line->AppendSwitchASCII(switches::kGpuRasterizationMSAASampleCount,
                                    base::NumberToString(msaa_sample_count));
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

#if defined(OS_WIN)
  command_line->AppendSwitchASCII(
      switches::kDeviceScaleFactor,
      base::NumberToString(display::win::GetDPIScale()));
#endif

  AppendCompositorCommandLineFlags(command_line);

  command_line->AppendSwitchASCII(switches::kRendererClientId,
                                  std::to_string(GetID()));

  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    // Disable V8 code mitigations if renderer processes are site-isolated.
    command_line->AppendSwitch(switches::kNoV8UntrustedCodeMitigations);
  }
}

void RenderProcessHostImpl::PropagateBrowserCommandLineToRenderer(
    const base::CommandLine& browser_cmd,
    base::CommandLine* renderer_cmd) {
  // Propagate the following switches to the renderer command line (along
  // with any associated values) if present in the browser command line.
  static const char* const kSwitchNames[] = {
    network::switches::kExplicitlyAllowedPorts,
    service_manager::switches::kDisableInProcessStackTraces,
    service_manager::switches::kDisableSeccompFilterSandbox,
    service_manager::switches::kNoSandbox,
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    switches::kDisableDevShmUsage,
#endif
#if defined(OS_MACOSX)
    // Allow this to be set when invoking the browser and relayed along.
    service_manager::switches::kEnableSandboxLogging,
#endif
    switches::kAgcStartupMinVolume,
    switches::kAllowPreCommitInput,
    switches::kAllowLoopbackInPeerConnection,
    switches::kAndroidFontsPath,
    switches::kAudioBufferSize,
    switches::kAutoplayPolicy,
    switches::kBlinkSettings,
    switches::kDefaultTileWidth,
    switches::kDefaultTileHeight,
    switches::kMinHeightForGpuRasterTile,
    switches::kDisable2dCanvasImageChromium,
    switches::kDisableYUVImageDecoding,
    switches::kDisableAcceleratedVideoDecode,
    switches::kDisableBackgroundTimerThrottling,
    switches::kDisableBestEffortTasks,
    switches::kDisableBreakpad,
    switches::kDisablePreferCompositingToLCDText,
    switches::kDisableDatabases,
    switches::kDisableFileSystem,
    switches::kDisableFrameRateLimit,
    switches::kDisableGpuMemoryBufferVideoFrames,
    switches::kDisableImageAnimationResync,
    switches::kDisableLowResTiling,
    switches::kDisableHistogramCustomizer,
    switches::kDisableLCDText,
    switches::kDisableLogging,
    switches::kDisableBackgroundMediaSuspend,
    switches::kDisableNotifications,
    switches::kDisableOopRasterization,
    switches::kEnableDeJelly,
    switches::kDisableOriginTrialControlledBlinkFeatures,
    switches::kDisablePepper3DImageChromium,
    switches::kDisablePermissionsAPI,
    switches::kDisablePresentationAPI,
    switches::kDisableRGBA4444Textures,
    switches::kDisableRTCSmoothnessAlgorithm,
    switches::kDisableScrollToTextFragment,
    switches::kDisableSharedWorkers,
    switches::kDisableSkiaRuntimeOpts,
    switches::kDisableSpeechAPI,
    switches::kDisableThreadedCompositing,
    switches::kDisableThreadedScrolling,
    switches::kDisableTouchAdjustment,
    switches::kDisableTouchDragDrop,
    switches::kDisableV8IdleTasks,
    switches::kDisableVideoCaptureUseGpuMemoryBuffer,
    switches::kDisableWebGLImageChromium,
    switches::kDomAutomationController,
    switches::kEnableAccessibilityObjectModel,
    switches::kEnableAutomation,
    switches::kEnableExperimentalAccessibilityLanguageDetection,
    switches::kEnableExperimentalAccessibilityLabelsDebugging,
    switches::kEnableExperimentalWebPlatformFeatures,
    switches::kEnableGPUClientLogging,
    switches::kEnableGpuClientTracing,
    switches::kEnableGpuMemoryBufferVideoFrames,
    switches::kEnableGPUServiceLogging,
    switches::kEnableLowResTiling,
    switches::kEnableLCDText,
    switches::kEnableLogging,
    switches::kEnableNetworkInformationDownlinkMax,
    switches::kEnableOopRasterization,
    switches::kEnablePluginPlaceholderTesting,
    switches::kEnablePreciseMemoryInfo,
    switches::kEnablePreferCompositingToLCDText,
    switches::kEnableRGBA4444Textures,
    switches::kEnableSkiaBenchmarking,
    switches::kEnableThreadedCompositing,
    switches::kEnableTouchDragDrop,
    switches::kEnableUnsafeWebGPU,
    switches::kEnableUseZoomForDSF,
    switches::kEnableViewport,
    switches::kEnableVtune,
    switches::kEnableWebGL2ComputeContext,
    switches::kEnableWebGLDraftExtensions,
    switches::kEnableWebGLImageChromium,
    switches::kFileUrlPathAlias,
    switches::kForceDeviceScaleFactor,
    switches::kForceDisplayColorProfile,
    switches::kForceGpuMemAvailableMb,
    switches::kForceHighContrast,
    switches::kForceOverlayFullscreenVideo,
    switches::kForceVideoOverlays,
    switches::kFullMemoryCrashReport,
    switches::kGaiaUrl,
    switches::kIPCConnectionTimeout,
    switches::kJavaScriptFlags,
    switches::kLogBestEffortTasks,
    switches::kLogFile,
    switches::kLoggingLevel,
    switches::kMaxActiveWebGLContexts,
    switches::kMaxUntiledLayerWidth,
    switches::kMaxUntiledLayerHeight,
    switches::kMSEAudioBufferSizeLimitMb,
    switches::kMSEVideoBufferSizeLimitMb,
    switches::kNetworkQuietTimeout,
    switches::kNoZygote,
    switches::kOverridePluginPowerSaverForTesting,
    switches::kPassiveListenersDefault,
    switches::kPerfettoDisableInterning,
    switches::kPpapiInProcess,
    switches::kProfilingAtStart,
    switches::kProfilingFile,
    switches::kProfilingFlush,
    switches::kRegisterPepperPlugins,
    switches::kRemoteDebuggingPipe,
    switches::kRemoteDebuggingPort,
    switches::kRendererStartupDialog,
    switches::kReportVp9AsAnUnsupportedMimeType,
    switches::kShowLayoutShiftRegions,
    switches::kShowPaintRects,
    switches::kStatsCollectionController,
    switches::kSkiaFontCacheLimitMb,
    switches::kSkiaResourceCacheLimitMb,
    switches::kTestType,
    switches::kTouchEventFeatureDetection,
    switches::kTouchTextSelectionStrategy,
    switches::kTraceToConsole,
    switches::kUseFakeCodecForPeerConnection,
    switches::kUseFakeUIForMediaStream,
    switches::kUseLegacyFormControls,
    switches::kUseMobileUserAgent,
    switches::kV,
    switches::kVideoCaptureUseGpuMemoryBuffer,
    switches::kVideoThreads,
    switches::kVideoUnderflowThresholdMs,
    switches::kVModule,
    switches::kWebglAntialiasingMode,
    switches::kWebglMSAASampleCount,
    // Please keep these in alphabetical order. Compositor switches here
    // should also be added to
    // chrome/browser/chromeos/login/chrome_restart_request.cc.
    cc::switches::kCCScrollAnimationDurationForTesting,
    cc::switches::kCheckDamageEarly,
    cc::switches::kDisableCheckerImaging,
    cc::switches::kDisableCompositedAntialiasing,
    cc::switches::kDisableThreadedAnimation,
    cc::switches::kEnableGpuBenchmarking,
    cc::switches::kHighlightNonLCDTextLayers,
    cc::switches::kShowCompositedLayerBorders,
    cc::switches::kShowFPSCounter,
    cc::switches::kShowLayerAnimationBounds,
    cc::switches::kShowPropertyChangedRects,
    cc::switches::kShowScreenSpaceRects,
    cc::switches::kShowSurfaceDamageRects,
    cc::switches::kSlowDownRasterScaleFactor,
    cc::switches::kBrowserControlsHideThreshold,
    cc::switches::kBrowserControlsShowThreshold,
    switches::kRunAllCompositorStagesBeforeDraw,

#if BUILDFLAG(ENABLE_PLUGINS)
    switches::kEnablePepperTesting,
#endif
    switches::kDisableWebRtcHWDecoding,
    switches::kDisableWebRtcHWEncoding,
    switches::kEnableWebRtcSrtpAesGcm,
    switches::kEnableWebRtcSrtpEncryptedHeaders,
    switches::kEnableWebRtcStunOrigin,
    switches::kEnforceWebRtcIPPermissionCheck,
    switches::kWebRtcMaxCaptureFramerate,
    switches::kEnableLowEndDeviceMode,
    switches::kDisableLowEndDeviceMode,
    switches::kDisallowNonExactResourceReuse,
#if defined(OS_ANDROID)
    switches::kDisableMediaSessionAPI,
    switches::kEnableReachedCodeProfiler,
    switches::kRendererWaitForJavaDebugger,
#endif
#if defined(OS_WIN)
    service_manager::switches::kDisableWin32kLockDown,
    switches::kDisableHighResTimer,
    switches::kEnableWin7WebRtcHWH264Decoding,
    switches::kTrySupportedChannelLayouts,
#endif
#if defined(USE_OZONE)
    switches::kOzonePlatform,
#endif
#if defined(ENABLE_IPC_FUZZER)
    switches::kIpcDumpDirectory,
    switches::kIpcFuzzerTestcase,
#endif
  };
  renderer_cmd->CopySwitchesFrom(browser_cmd, kSwitchNames,
                                 base::size(kSwitchNames));

  BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(renderer_cmd);
  BrowserChildProcessHostImpl::CopyTraceStartupFlags(renderer_cmd);

  // Only run the Stun trials in the first renderer.
  if (!has_done_stun_trials &&
      browser_cmd.HasSwitch(switches::kWebRtcStunProbeTrialParameter)) {
    has_done_stun_trials = true;
    renderer_cmd->AppendSwitchASCII(
        switches::kWebRtcStunProbeTrialParameter,
        browser_cmd.GetSwitchValueASCII(
            switches::kWebRtcStunProbeTrialParameter));
  }

  // Disable databases in incognito mode.
  if (GetBrowserContext()->IsOffTheRecord() &&
      !browser_cmd.HasSwitch(switches::kDisableDatabases)) {
    renderer_cmd->AppendSwitch(switches::kDisableDatabases);
  }

#if defined(OS_ANDROID)
  if (browser_cmd.HasSwitch(switches::kDisableGpuCompositing)) {
    renderer_cmd->AppendSwitch(switches::kDisableGpuCompositing);
  }
#elif !defined(OS_CHROMEOS)
  // If gpu compositing is not being used, tell the renderer at startup. This
  // is inherently racey, as it may change while the renderer is being
  // launched, but the renderer will hear about the correct state eventually.
  // This optimizes the common case to avoid wasted work.
  if (GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled())
    renderer_cmd->AppendSwitch(switches::kDisableGpuCompositing);
#endif  // defined(OS_ANDROID)

  // Add kWaitForDebugger to let renderer process wait for a debugger.
  if (browser_cmd.HasSwitch(switches::kWaitForDebuggerChildren)) {
    // Look to pass-on the kWaitForDebugger flag.
    std::string value =
        browser_cmd.GetSwitchValueASCII(switches::kWaitForDebuggerChildren);
    if (value.empty() || value == switches::kRendererProcess) {
      renderer_cmd->AppendSwitch(switches::kWaitForDebugger);
    }
  }

#if defined(OS_WIN) && !defined(OFFICIAL_BUILD)
  // Needed because we can't show the dialog from the sandbox. Don't pass
  // --no-sandbox in official builds because that would bypass the bad_flgs
  // prompt.
  if (renderer_cmd->HasSwitch(switches::kRendererStartupDialog) &&
      !renderer_cmd->HasSwitch(service_manager::switches::kNoSandbox)) {
    renderer_cmd->AppendSwitch(service_manager::switches::kNoSandbox);
  }
#endif

  CopyFeatureSwitch(browser_cmd, renderer_cmd, switches::kEnableBlinkFeatures);
  CopyFeatureSwitch(browser_cmd, renderer_cmd, switches::kDisableBlinkFeatures);
}

const base::Process& RenderProcessHostImpl::GetProcess() {
  if (run_renderer_in_process()) {
    // This is a sentinel object used for this process in single process mode.
    static const base::NoDestructor<base::Process> self(
        base::Process::Current());
    return *self;
  }

  if (!child_process_launcher_.get() || child_process_launcher_->IsStarting()) {
    // This is a sentinel for "no process".
    static const base::NoDestructor<base::Process> null_process;
    return *null_process;
  }

  return child_process_launcher_->GetProcess();
}

bool RenderProcessHostImpl::IsReady() {
  // The process launch result (that sets GetHandle()) and the channel
  // connection (that sets channel_connected_) can happen in either order.
  return GetProcess().Handle() && channel_connected_;
}

bool RenderProcessHostImpl::Shutdown(int exit_code) {
  if (run_renderer_in_process())
    return false;  // Single process mode never shuts down the renderer.

  if (!child_process_launcher_.get())
    return false;

  shutdown_exit_code_ = exit_code;
  return child_process_launcher_->Terminate(exit_code);
}

bool RenderProcessHostImpl::FastShutdownIfPossible(size_t page_count,
                                                   bool skip_unload_handlers) {
  // Do not shut down the process if there are active or pending views other
  // than the ones we're shutting down.
  if (page_count && page_count != (GetActiveViewCount() + pending_views_))
    return false;

  if (run_renderer_in_process())
    return false;  // Single process mode never shuts down the renderer.

  if (!child_process_launcher_.get())
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
  TRACE_IPC_MESSAGE_SEND("renderer_host", "RenderProcessHostImpl::Send", msg);

  std::unique_ptr<IPC::Message> message(msg);

  // |channel_| is only null after Cleanup(), at which point we don't care
  // about delivering any messages.
  if (!channel_)
    return false;

#if !defined(OS_ANDROID)
  DCHECK(!message->is_sync());
#else
  if (message->is_sync()) {
    // If Init() hasn't been called yet since construction or the last
    // ProcessDied() we avoid blocking on sync IPC.
    if (!IsInitializedAndNotDead())
      return false;

    // Likewise if we've done Init(), but process launch has not yet
    // completed, we avoid blocking on sync IPC.
    if (child_process_launcher_.get() && child_process_launcher_->IsStarting())
      return false;
  }
#endif

  // Allow tests to watch IPCs sent to the renderer.
  if (ipc_send_watcher_for_testing_)
    ipc_send_watcher_for_testing_.Run(*message);

  return channel_->Send(message.release());
}

bool RenderProcessHostImpl::OnMessageReceived(const IPC::Message& msg) {
  // If we're about to be deleted, or have initiated the fast shutdown
  // sequence, we ignore incoming messages.

  if (deleting_soon_ || fast_shutdown_started_)
    return false;

  mark_child_process_activity_time();
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // Dispatch control messages.
    IPC_BEGIN_MESSAGE_MAP(RenderProcessHostImpl, msg)
      IPC_MESSAGE_HANDLER(WidgetHostMsg_Close_ACK, OnCloseACK)
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
      // The listener has gone away, so we must respond or else the caller
      // will hang waiting for a reply.
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
      !associated_interfaces_->TryBindInterface(interface_name, &handle)) {
    LOG(ERROR) << "Request for unknown Channel-associated interface: "
               << interface_name;
  }
}

void RenderProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  channel_connected_ = true;

#if defined(OS_MACOSX)
  ChildProcessTaskPortProvider::GetInstance()->OnChildProcessLaunched(
      peer_pid, child_process_.get());
#endif

  if (IsReady()) {
    DCHECK(!sent_render_process_ready_);
    sent_render_process_ready_ = true;
    // Send RenderProcessReady only if we already received the process handle.
    for (auto& observer : observers_)
      observer.RenderProcessReady(this);

#if defined(OS_LINUX)
    // Provide /proc/{renderer pid}/status and statm files for
    // MemoryUsageMonitor in blink.
    ProvideStatusFileForRenderer();
#endif

    ProvideSwapFileForRenderer();
  }

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  child_process_->SetIPCLoggingEnabled(IPC::Logging::GetInstance()->Enabled());
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  child_process_->SetProfilingFile(OpenProfilingFile());
#endif
}

void RenderProcessHostImpl::OnChannelError() {
  ProcessDied(true /* already_dead */, nullptr);
}

void RenderProcessHostImpl::OnBadMessageReceived(const IPC::Message& message) {
  // Message de-serialization failed. We consider this a capital crime. Kill
  // the renderer if we have one.
  auto type = message.type();
  LOG(ERROR) << "bad message " << type << " terminating renderer.";

  // The ReceivedBadMessage call below will trigger a DumpWithoutCrashing.
  // Alias enough information here so that we can determine what the bad
  // message was.
  base::debug::Alias(&type);

  bad_message::ReceivedBadMessage(this,
                                  bad_message::RPH_DESERIALIZATION_FAILED);
}

BrowserContext* RenderProcessHostImpl::GetBrowserContext() {
  return browser_context_;
}

bool RenderProcessHostImpl::InSameStoragePartition(
    StoragePartition* partition) {
  return storage_partition_impl_ == partition;
}

int RenderProcessHostImpl::GetID() {
  return id_;
}

bool RenderProcessHostImpl::IsInitializedAndNotDead() {
  return is_initialized_ && !is_dead_;
}

void RenderProcessHostImpl::SetBlocked(bool blocked) {
  if (blocked == is_blocked_)
    return;

  is_blocked_ = blocked;
  blocked_state_changed_callback_list_.Notify(blocked);
}

bool RenderProcessHostImpl::IsBlocked() {
  return is_blocked_;
}

std::unique_ptr<base::CallbackList<void(bool)>::Subscription>
RenderProcessHostImpl::RegisterBlockStateChangedCallback(
    const base::RepeatingCallback<void(bool)>& cb) {
  return blocked_state_changed_callback_list_.Add(cb);
}

void RenderProcessHostImpl::Cleanup() {
  TRACE_EVENT1("shutdown", "RenderProcessHostImpl::Cleanup",
               "render_process_host", this);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Keep the one renderer thread around forever in single process mode.
  if (run_renderer_in_process())
    return;

  // If within_process_died_observer_ is true, one of our observers performed
  // an action that caused us to die (e.g. http://crbug.com/339504).
  // Therefore, delay the destruction until all of the observer callbacks have
  // been made, and guarantee that the RenderProcessHostDestroyed observer
  // callback is always the last callback fired.
  if (within_process_died_observer_) {
    TRACE_EVENT1(
        "shutdown",
        "RenderProcessHostImpl::Cleanup : within_process_died_observer",
        "render_process_host", this);
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

  // Until there are no other owners of this object, we can't delete
  // ourselves.
  if (!listeners_.IsEmpty()) {
    TRACE_EVENT2("shutdown", "RenderProcessHostImpl::Cleanup : Has listeners.",
                 "render_process_host", this, "listener_count",
                 listeners_.size());
    return;
  } else if (keep_alive_ref_count_ != 0) {
    TRACE_EVENT2("shutdown",
                 "RenderProcessHostImpl::Cleanup : Have keep_alive_ref.",
                 "render_process_host", this, "keep_alive_ref_count_",
                 keep_alive_ref_count_);
    return;
  }

  TRACE_EVENT1("shutdown", "RenderProcessHostImpl::Cleanup : Starting cleanup.",
               "render_process_host", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2("shutdown", "Cleanup in progress", this,
                                    "render_process_host", this,
                                    "browser_context", browser_context_);

  if (is_initialized_) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&WebRtcLog::ClearLogMessageCallback, GetID()));
  }

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
  if (IsInitializedAndNotDead()) {
    // Populates Android-only fields and closes the underlying base::Process.
    ChildProcessTerminationInfo info =
        child_process_launcher_->GetChildTerminationInfo(
            false /* already_dead */);
    info.status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
    info.exit_code = 0;
    PopulateTerminationInfoRendererFields(&info);
    within_cleanup_process_died_observer_ = true;
    for (auto& observer : observers_) {
      observer.RenderProcessExited(this, info);
    }
    within_cleanup_process_died_observer_ = false;
  }
  for (auto& observer : observers_)
    observer.RenderProcessHostDestroyed(this);
  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_TERMINATED, Source<RenderProcessHost>(this),
      NotificationService::NoDetails());

#ifndef NDEBUG
  is_self_deleted_ = true;
#endif
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  deleting_soon_ = true;

  if (render_frame_message_filter_) {
    // RenderFrameMessageFilter is refcounted and can outlive the
    // ResourceContext. If the BrowserContext is shutting down, after
    // RenderProcessHostImpl cleanup a task will be posted to the IO thread
    // that destroys the ResourceContext. Therefore the ClearResourceContext
    // task must be posted now to ensure it gets ahead of the destruction of
    // the ResourceContext in the IOThread sequence.
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&RenderFrameMessageFilter::ClearResourceContext,
                       render_frame_message_filter_));
  }

  // Destroy all mojo bindings and IPC channels that can cause calls to this
  // object, to avoid method invocations that trigger usages of profile.
  ResetIPC();

  // Its important to remove the kSessionStorageHolder after the channel
  // has been reset to avoid deleting the underlying namespaces prior
  // to processing ipcs referring to them.
  DCHECK(!channel_);
  RemoveUserData(kSessionStorageHolderKey);

  // Remove ourself from the list of renderer processes so that we can't be
  // reused in between now and when the Delete task runs.
  UnregisterHost(GetID());
}

void RenderProcessHostImpl::PopulateTerminationInfoRendererFields(
    ChildProcessTerminationInfo* info) {
  info->renderer_has_visible_clients = VisibleClientCount() > 0;
  info->renderer_was_subframe = GetFrameDepth() > 0;
}

void RenderProcessHostImpl::AddPendingView() {
  const bool had_pending_views = pending_views_++;
  if (!had_pending_views)
    UpdateProcessPriority();
}

void RenderProcessHostImpl::RemovePendingView() {
  DCHECK(pending_views_);
  --pending_views_;
  if (!pending_views_)
    UpdateProcessPriority();
}

void RenderProcessHostImpl::AddPriorityClient(PriorityClient* priority_client) {
  DCHECK(!base::Contains(priority_clients_, priority_client));
  priority_clients_.insert(priority_client);
  UpdateProcessPriorityInputs();
}

void RenderProcessHostImpl::RemovePriorityClient(
    PriorityClient* priority_client) {
  DCHECK(base::Contains(priority_clients_, priority_client));
  priority_clients_.erase(priority_client);
  UpdateProcessPriorityInputs();
}

void RenderProcessHostImpl::SetPriorityOverride(bool foregrounded) {
  priority_override_ = foregrounded;
  UpdateProcessPriority();
}

bool RenderProcessHostImpl::HasPriorityOverride() {
  return priority_override_.has_value();
}

void RenderProcessHostImpl::ClearPriorityOverride() {
  priority_override_.reset();
  UpdateProcessPriority();
}

void RenderProcessHostImpl::SetSuddenTerminationAllowed(bool enabled) {
  sudden_termination_allowed_ = enabled;
}

bool RenderProcessHostImpl::SuddenTerminationAllowed() {
  return sudden_termination_allowed_;
}

base::TimeDelta RenderProcessHostImpl::GetChildProcessIdleTime() {
  return base::TimeTicks::Now() - child_process_activity_time_;
}

void RenderProcessHostImpl::FilterURL(bool empty_allowed, GURL* url) {
  FilterURL(this, empty_allowed, url);
}

void RenderProcessHostImpl::EnableAudioDebugRecordings(
    const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  aec_dump_manager_.Start(file_path);
}

void RenderProcessHostImpl::DisableAudioDebugRecordings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  aec_dump_manager_.Stop();
}

RenderProcessHostImpl::WebRtcStopRtpDumpCallback
RenderProcessHostImpl::StartRtpDump(bool incoming,
                                    bool outgoing,
                                    WebRtcRtpPacketCallback packet_callback) {
  p2p_socket_dispatcher_host_->StartRtpDump(incoming, outgoing,
                                            std::move(packet_callback));

  return base::BindOnce(&P2PSocketDispatcherHost::StopRtpDump,
                        p2p_socket_dispatcher_host_->GetWeakPtr());
}

void RenderProcessHostImpl::EnableWebRtcEventLogOutput(int lid,
                                                       int output_period_ms) {
  GetPeerConnectionTrackerHost()->StartEventLog(lid, output_period_ms);
}

void RenderProcessHostImpl::DisableWebRtcEventLogOutput(int lid) {
  GetPeerConnectionTrackerHost()->StopEventLog(lid);
}

IPC::ChannelProxy* RenderProcessHostImpl::GetChannel() {
  return channel_.get();
}

void RenderProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  filter->RegisterAssociatedInterfaces(channel_.get());
  channel_->AddFilter(filter->GetFilter());
}

bool RenderProcessHostImpl::FastShutdownStarted() {
  return fast_shutdown_started_;
}

// static
void RenderProcessHostImpl::RegisterHost(int host_id, RenderProcessHost* host) {
  TRACE_EVENT2("shutdown", "RenderProcessHostImpl::RegisterHost",
               "render_process_host", host, "host_id", host_id);
  GetAllHosts().AddWithID(host, host_id);
}

// static
void RenderProcessHostImpl::UnregisterHost(int host_id) {
  RenderProcessHost* host = GetAllHosts().Lookup(host_id);
  TRACE_EVENT2("shutdown", "RenderProcessHostImpl::UnregisterHost",
               "render_process_host", host, "host_id", host_id);

  if (!host)
    return;

  GetAllHosts().Remove(host_id);

  // Look up the map of site to process for the given browser_context,
  // in case we need to remove this process from it.  It will be registered
  // under any sites it rendered that use process-per-site mode.
  SiteProcessMap* map =
      GetSiteProcessMapForBrowserContext(host->GetBrowserContext());
  map->RemoveProcess(host);
}

// static
void RenderProcessHostImpl::RegisterCreationObserver(
    RenderProcessHostCreationObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetAllCreationObservers().push_back(observer);
}

// static
void RenderProcessHostImpl::UnregisterCreationObserver(
    RenderProcessHostCreationObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = std::find(GetAllCreationObservers().begin(),
                        GetAllCreationObservers().end(), observer);
  DCHECK(iter != GetAllCreationObservers().end());
  GetAllCreationObservers().erase(iter);
}

// static
void RenderProcessHostImpl::FilterURL(RenderProcessHost* rph,
                                      bool empty_allowed,
                                      GURL* url) {
  if (empty_allowed && url->is_empty())
    return;

  if (!url->is_valid()) {
    // Have to use about:blank for the denied case, instead of an empty GURL.
    // This is because the browser treats navigation to an empty GURL as a
    // navigation to the home page. This is often a privileged page
    // (chrome://newtab/) which is exactly what we don't want.
    TRACE_EVENT1("navigation", "RenderProcessHost::FilterURL - invalid URL",
                 "process_id", rph->GetID());
    *url = GURL(kBlockedURL);
    return;
  }

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanRequestURL(rph->GetID(), *url)) {
    // If this renderer is not permitted to request this URL, we invalidate
    // the URL.  This prevents us from storing the blocked URL and becoming
    // confused later.
    TRACE_EVENT2("navigation",
                 "RenderProcessHost::FilterURL - failed CanRequestURL",
                 "process_id", rph->GetID(), "url", url->spec());
    VLOG(1) << "Blocked URL " << url->spec();
    *url = GURL(kBlockedURL);
  }
}

// static
bool RenderProcessHostImpl::IsSuitableHost(
    RenderProcessHost* host,
    const IsolationContext& isolation_context,
    const GURL& site_url,
    const GURL& lock_url,
    const bool is_guest) {
  BrowserContext* browser_context =
      isolation_context.browser_or_resource_context().ToBrowserContext();
  DCHECK(browser_context);
  if (run_renderer_in_process()) {
    DCHECK_EQ(host->GetBrowserContext(), browser_context)
        << " Single-process mode does not support multiple browser contexts.";
    return true;
  }

  if (host->GetBrowserContext() != browser_context)
    return false;

  // Do not allow sharing of guest hosts. This is to prevent bugs where guest
  // and non-guest storage gets mixed. In the future, we might consider
  // enabling the sharing of guests, in this case this check should be removed
  // and InSameStoragePartition should handle the possible sharing.
  if (host->IsForGuestsOnly())
    return false;

  // Check whether the given host and the intended site_url will be using the
  // same StoragePartition, since a RenderProcessHost can only support a
  // single StoragePartition.  This is relevant for packaged apps.
  StoragePartition* dest_partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, site_url);
  if (!host->InSameStoragePartition(dest_partition))
    return false;

  // Check WebUI bindings and origin locks.  Note that |lock_url| may differ
  // from |site_url| if an effective URL is used.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  bool host_has_web_ui_bindings = policy->HasWebUIBindings(host->GetID());
  GURL process_lock = policy->GetOriginLock(host->GetID());
  if (host->HostHasNotBeenUsed()) {
    // If the host hasn't been used, it won't have the expected WebUI bindings
    // or origin locks just *yet* - skip the checks in this case.  One example
    // where this case can happen is when the spare RenderProcessHost gets
    // used.
    CHECK(!host_has_web_ui_bindings);
    CHECK(process_lock.is_empty());
  } else {
    // WebUI checks.
    bool url_requires_web_ui_bindings =
        WebUIControllerFactoryRegistry::GetInstance()->UseWebUIBindingsForURL(
            browser_context, site_url);
    if (host_has_web_ui_bindings != url_requires_web_ui_bindings)
      return false;

    if (!process_lock.is_empty()) {
      // If this process is locked to a site, it cannot be reused for a
      // destination that doesn't require a dedicated process, even for the
      // same site. This can happen with dynamic isolated origins (see
      // https://crbug.com/950453).
      if (!SiteInstanceImpl::ShouldLockToOrigin(isolation_context, site_url,
                                                is_guest))
        return false;

      // If the destination requires a different process lock, this process
      // cannot be used.
      if (process_lock != lock_url)
        return false;
    } else {
      if (!host->IsUnused() && SiteInstanceImpl::ShouldLockToOrigin(
                                   isolation_context, site_url, is_guest)) {
        // If this process has been used to host any other content, it cannot
        // be reused if the destination site requires a dedicated process and
        // should use a process locked to just that site.
        return false;
      }
    }
  }

  if (!GetContentClient()->browser()->IsSuitableHost(host, site_url))
    return false;

  // If this site_url is going to require a dedicated process, then check
  // whether this process has a pending navigation to a URL for which
  // SiteInstance does not assign site URLs.  If this is the case, it is not
  // safe to reuse this process for a navigation which itself assigns site
  // URLs, since in that case the latter navigation could lock this process
  // before the commit for the siteless URL arrives, resulting in a renderer
  // kill. See https://crbug.com/970046.
  if (SiteInstanceImpl::ShouldAssignSiteForURL(site_url) &&
      SiteInstanceImpl::DoesSiteRequireDedicatedProcess(isolation_context,
                                                        site_url)) {
    SiteProcessCountTracker* pending_tracker =
        static_cast<SiteProcessCountTracker*>(
            browser_context->GetUserData(kPendingSiteProcessCountTrackerKey));
    if (pending_tracker &&
        pending_tracker->ContainsNonReusableSiteForHost(host))
      return false;
  }

  return true;
}

// static
void RenderProcessHost::WarmupSpareRenderProcessHost(
    content::BrowserContext* browser_context) {
  SpareRenderProcessHostManager::GetInstance().WarmupSpareRenderProcessHost(
      browser_context);
}

// static
bool RenderProcessHost::run_renderer_in_process() {
  return g_run_renderer_in_process;
}

// static
void RenderProcessHost::SetRunRendererInProcess(bool value) {
  g_run_renderer_in_process = value;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (value) {
    if (!command_line->HasSwitch(switches::kLang)) {
      // Modify the current process' command line to include the browser
      // locale, as the renderer expects this flag to be set.
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
  return iterator(&GetAllHosts());
}

// static
RenderProcessHost* RenderProcessHost::FromID(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetAllHosts().Lookup(render_process_id);
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
  if (GetAllHosts().size() >= GetMaxRendererProcessCount())
    return true;

  return GetContentClient()->browser()->ShouldTryToUseExistingProcessHost(
      browser_context, url);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetExistingProcessHost(
    SiteInstanceImpl* site_instance) {
  // First figure out which existing renderers we can use.
  std::vector<RenderProcessHost*> suitable_renderers;
  suitable_renderers.reserve(GetAllHosts().size());

  for (iterator iter(AllHostsIterator()); !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrentValue()->MayReuseHost() &&
        RenderProcessHostImpl::IsSuitableHost(
            iter.GetCurrentValue(), site_instance->GetIsolationContext(),
            site_instance->GetSiteURL(), site_instance->lock_url(),
            site_instance->IsGuest())) {
      // The spare is always considered before process reuse.
      DCHECK_NE(iter.GetCurrentValue(),
                SpareRenderProcessHostManager::GetInstance()
                    .spare_render_process_host());

      suitable_renderers.push_back(iter.GetCurrentValue());
    }
  }

  // Now pick a random suitable renderer, if we have any.
  if (!suitable_renderers.empty()) {
    int suitable_count = static_cast<int>(suitable_renderers.size());
    int random_index = base::RandInt(0, suitable_count - 1);
    return suitable_renderers[random_index];
  }

  return nullptr;
}

// static
RenderProcessHost* RenderProcessHostImpl::GetUnusedProcessHostForServiceWorker(
    SiteInstanceImpl* site_instance) {
  DCHECK(site_instance->is_for_service_worker());
  if (site_instance->process_reuse_policy() !=
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE) {
    return nullptr;
  }

  auto& spare_process_manager = SpareRenderProcessHostManager::GetInstance();
  iterator iter(AllHostsIterator());
  while (!iter.IsAtEnd()) {
    auto* host = iter.GetCurrentValue();
    // This function tries to choose the process that will be chosen by a
    // navigation that will use the service worker that is being started.
    // Prefer to not take the spare process host, since if the navigation is
    // out of the New Tab Page on Android, it will be using the existing NTP
    // process instead of the spare process. If this function doesn't find a
    // suitable process, the spare can still be chosen when
    // MaybeTakeSpareRenderProcessHost() is called later.
    bool is_spare = (host == spare_process_manager.spare_render_process_host());

    if (!is_spare && iter.GetCurrentValue()->MayReuseHost() &&
        iter.GetCurrentValue()->IsUnused() &&
        RenderProcessHostImpl::IsSuitableHost(
            iter.GetCurrentValue(), site_instance->GetIsolationContext(),
            site_instance->GetSiteURL(), site_instance->lock_url(),
            site_instance->IsGuest())) {
      return host;
    }
    iter.Advance();
  }
  return nullptr;
}

// static
bool RenderProcessHost::ShouldUseProcessPerSite(BrowserContext* browser_context,
                                                const GURL& site_url) {
  // Returns true if we should use the process-per-site model.  This will be
  // the case if the --process-per-site switch is specified, or in
  // process-per-site-instance for particular sites (e.g., NTP). Note that
  // --single-process is handled in ShouldTryToUseExistingProcessHost.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kProcessPerSite))
    return true;

  // Error pages should use process-per-site model, as it is useful to
  // consolidate them to minimize resource usage and there is no security
  // drawback to combining them all in the same process.
  if (site_url.SchemeIs(kChromeErrorScheme))
    return true;

  // Otherwise let the content client decide, defaulting to false.
  return GetContentClient()->browser()->ShouldUseProcessPerSite(browser_context,
                                                                site_url);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetSoleProcessHostForURL(
    const IsolationContext& isolation_context,
    const GURL& url) {
  GURL site_url = SiteInstanceImpl::GetSiteForURL(isolation_context, url);
  GURL lock_url =
      SiteInstanceImpl::DetermineProcessLockURL(isolation_context, url);
  return GetSoleProcessHostForSite(isolation_context, site_url, lock_url,
                                   /* is_guest */ false);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetSoleProcessHostForSite(
    const IsolationContext& isolation_context,
    const GURL& site_url,
    const GURL& lock_url,
    const bool is_guest) {
  // Look up the map of site to process for the given browser_context.
  SiteProcessMap* map = GetSiteProcessMapForBrowserContext(
      isolation_context.browser_or_resource_context().ToBrowserContext());

  // See if we have an existing process with appropriate bindings for this
  // site. If not, the caller should create a new process and register it.
  // Note that IsSuitableHost expects a site URL rather than the full |url|.
  RenderProcessHost* host = map->FindProcess(site_url.possibly_invalid_spec());
  if (host && (!host->MayReuseHost() ||
               !IsSuitableHost(host, isolation_context, site_url, lock_url,
                               is_guest))) {
    // The registered process does not have an appropriate set of bindings for
    // the url.  Remove it from the map so we can register a better one.
    RecordAction(
        base::UserMetricsAction("BindingsMismatch_GetProcessHostPerSite"));
    map->RemoveProcess(host);
    host = nullptr;
  }

  return host;
}

void RenderProcessHostImpl::RegisterSoleProcessHostForSite(
    RenderProcessHost* process,
    SiteInstanceImpl* site_instance) {
  DCHECK(process);
  DCHECK(site_instance);

  // Look up the map of site to process for site_instance's BrowserContext.
  SiteProcessMap* map =
      GetSiteProcessMapForBrowserContext(site_instance->GetBrowserContext());

  // Only register valid, non-empty sites.  Empty or invalid sites will not
  // use process-per-site mode.  We cannot check whether the process has
  // appropriate bindings here, because the bindings have not yet been
  // granted.
  std::string site = site_instance->GetSiteURL().possibly_invalid_spec();
  if (!site.empty())
    map->RegisterProcess(site, process);
}

// static
RenderProcessHost* RenderProcessHostImpl::GetProcessHostForSiteInstance(
    SiteInstanceImpl* site_instance) {
  const GURL site_url = site_instance->GetSiteURL();
  SiteInstanceImpl::ProcessReusePolicy process_reuse_policy =
      site_instance->process_reuse_policy();
  RenderProcessHost* render_process_host = nullptr;

  bool is_unmatched_service_worker = site_instance->is_for_service_worker();
  BrowserContext* browser_context = site_instance->GetBrowserContext();

  // First, attempt to reuse an existing RenderProcessHost if necessary.
  switch (process_reuse_policy) {
    case SiteInstanceImpl::ProcessReusePolicy::PROCESS_PER_SITE:
      render_process_host = GetSoleProcessHostForSite(
          site_instance->GetIsolationContext(), site_url,
          site_instance->lock_url(), site_instance->IsGuest());
      break;
    case SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE:
      render_process_host =
          FindReusableProcessHostForSiteInstance(site_instance);
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
    render_process_host =
        UnmatchedServiceWorkerProcessTracker::MatchWithSite(site_instance);
  }

  // If a process hasn't been selected yet, check whether a default process
  // has been assigned for this browsing instance and use that. This is used
  // to group all SiteInstance that don't require a dedicated process into a
  // single process. This will only be set if
  // kProcessSharingWithStrictSiteInstances is enabled.
  if (!render_process_host) {
    render_process_host = site_instance->GetDefaultProcessIfUsable();
    DCHECK(!render_process_host ||
           base::FeatureList::IsEnabled(
               features::kProcessSharingWithStrictSiteInstances));
  }

  // If a process hasn't been selected yet, and the site instance is for a
  // service worker, try to use an unused process host. One might have been
  // created for a navigation and this will let the navigation and the service
  // worker share the same process.
  if (base::FeatureList::IsEnabled(
          features::kServiceWorkerPrefersUnusedProcess) &&
      !render_process_host && is_unmatched_service_worker) {
    render_process_host = GetUnusedProcessHostForServiceWorker(site_instance);
  }

  // See if the spare RenderProcessHost can be used.
  auto& spare_process_manager = SpareRenderProcessHostManager::GetInstance();
  bool spare_was_taken = false;
  if (!render_process_host) {
    render_process_host = spare_process_manager.MaybeTakeSpareRenderProcessHost(
        browser_context, site_instance);
    spare_was_taken = (render_process_host != nullptr);
  }

  // If not (or if none found), see if we should reuse an existing process.
  if (!render_process_host &&
      ShouldTryToUseExistingProcessHost(browser_context, site_url)) {
    render_process_host = GetExistingProcessHost(site_instance);
  }

  // If we found a process to reuse, sanity check that it is suitable for
  // hosting |site_url|. For example, if |site_url| requires a dedicated
  // process, we should never pick a process used by, or locked to, a different
  // site.
  if (render_process_host &&
      !RenderProcessHostImpl::IsSuitableHost(
          render_process_host, site_instance->GetIsolationContext(), site_url,
          site_instance->lock_url(), site_instance->IsGuest())) {
    base::debug::SetCrashKeyString(bad_message::GetRequestedSiteURLKey(),
                                   site_url.possibly_invalid_spec());
    ChildProcessSecurityPolicyImpl::GetInstance()->LogKilledProcessOriginLock(
        render_process_host->GetID());
    CHECK(false) << "Unsuitable process reused for site " << site_url;
  }

  // Otherwise, create a new RenderProcessHost.
  if (!render_process_host) {
    // Pass a null StoragePartition. Tests with TestBrowserContext using a
    // RenderProcessHostFactory may not instantiate a StoragePartition, and
    // creating one here with GetStoragePartition() can run into cross-thread
    // issues as TestBrowserContext initialization is done on the main thread.
    render_process_host =
        CreateRenderProcessHost(browser_context, nullptr, site_instance);
  }

  // It is important to call PrepareForFutureRequests *after* potentially
  // creating a process a few statements earlier - doing this avoids violating
  // the process limit.
  //
  // We should not warm-up another spare if the spare was not taken, because
  // in this case we might have created a new process - we want to avoid
  // spawning two processes at the same time.  In this case the call to
  // PrepareForFutureRequests will be postponed until later (e.g. until the
  // navigation commits or a cross-site redirect happens).
  if (spare_was_taken)
    spare_process_manager.PrepareForFutureRequests(browser_context);

  if (is_unmatched_service_worker) {
    UnmatchedServiceWorkerProcessTracker::Register(render_process_host,
                                                   site_instance);
  }

  // Make sure the chosen process is in the correct StoragePartition for the
  // SiteInstance.
  CHECK(render_process_host->InSameStoragePartition(
      BrowserContext::GetStoragePartition(browser_context, site_instance,
                                          false /* can_create */)));

  return render_process_host;
}

void RenderProcessHostImpl::CreateSharedRendererHistogramAllocator() {
  // Create a persistent memory segment for renderer histograms only if
  // they're active in the browser.
  if (!base::GlobalHistogramAllocator::Get()) {
    if (is_initialized_) {
      HistogramController::GetInstance()->SetHistogramMemory<RenderProcessHost>(
          this, base::WritableSharedMemoryRegion());
    }
    return;
  }

  // Get handle to the renderer process. Stop if there is none.
  base::ProcessHandle destination = GetProcess().Handle();
  if (destination == base::kNullProcessHandle)
    return;

  // Create persistent/shared memory and allow histograms to be stored in
  // it. Memory that is not actually used won't be physically mapped by the
  // system. RendererMetrics usage, as reported in UMA, peaked around 0.7MiB
  // as of 2016-12-20.
  base::WritableSharedMemoryRegion shm_region =
      base::WritableSharedMemoryRegion::Create(2 << 20);  // 2 MiB
  base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
  if (!shm_region.IsValid() || !shm_mapping.IsValid())
    return;

  // If a renderer crashes before completing startup and gets restarted, this
  // method will get called a second time meaning that a metrics-allocator
  // already exists. We have to recreate it here because previously used
  // |shm_region| is gone.
  metrics_allocator_ =
      std::make_unique<base::WritableSharedPersistentMemoryAllocator>(
          std::move(shm_mapping), GetID(), "RendererMetrics");

  HistogramController::GetInstance()->SetHistogramMemory<RenderProcessHost>(
      this, std::move(shm_region));
}

void RenderProcessHostImpl::ProcessDied(
    bool already_dead,
    ChildProcessTerminationInfo* known_info) {
  // Our child process has died.  If we didn't expect it, it's a crash.
  // In any case, we need to let everyone know it's gone.
  // The OnChannelError notification can fire multiple times due to nested
  // sync calls to a renderer. If we don't have a valid channel here it means
  // we already handled the error.

  // It should not be possible for us to be called re-entrantly.
  DCHECK(!within_process_died_observer_);

  // It should not be possible for a process death notification to come in
  // while we are dying.
  DCHECK(!deleting_soon_);

  // child_process_launcher_ can be NULL in single process mode or if fast
  // termination happened.
  ChildProcessTerminationInfo info;
  info.exit_code = 0;
  if (known_info) {
    info = *known_info;
  } else if (child_process_launcher_.get()) {
    info = child_process_launcher_->GetChildTerminationInfo(already_dead);
    if (already_dead && info.status == base::TERMINATION_STATUS_STILL_RUNNING) {
      // May be in case of IPC error, if it takes long time for renderer
      // to exit. Child process will be killed in any case during
      // child_process_launcher_.reset(). Make sure we will not broadcast
      // RenderProcessExited with status TERMINATION_STATUS_STILL_RUNNING,
      // since this will break WebContentsImpl logic.
      info.status = base::TERMINATION_STATUS_PROCESS_CRASHED;

      // TODO(siggi): Remove this once https://crbug.com/806661 is resolved.
#if defined(OS_WIN)
      if (info.exit_code == WAIT_TIMEOUT && g_analyze_hung_renderer)
        g_analyze_hung_renderer(child_process_launcher_->GetProcess());
#endif
    }
  }
  PopulateTerminationInfoRendererFields(&info);

  child_process_launcher_.reset();
  is_dead_ = true;
  // Make sure no IPCs or mojo calls from the old process get dispatched after
  // it has died.
  ResetIPC();

  UpdateProcessPriority();

  // RenderProcessExited relies on the exit code set during shutdown.
  if (shutdown_exit_code_ != -1)
    info.exit_code = shutdown_exit_code_;

  within_process_died_observer_ = true;
  for (auto& observer : observers_)
    observer.RenderProcessExited(this, info);

  NotificationService::current()->Notify(
      NOTIFICATION_RENDERER_PROCESS_CLOSED, Source<RenderProcessHost>(this),
      Details<ChildProcessTerminationInfo>(&info));
  within_process_died_observer_ = false;

  RemoveUserData(kSessionStorageHolderKey);

  // Initialize a new ChannelProxy in case this host is re-used for a new
  // process. This ensures that new messages can be sent on the host ASAP
  // (even before Init()) and they'll eventually reach the new process.
  //
  // Note that this may have already been called by one of the above observers
  EnableSendQueue();

  // It's possible that one of the calls out to the observers might have
  // caused this object to be no longer needed.
  if (delayed_cleanup_needed_)
    Cleanup();

  compositing_mode_reporter_.reset();

  HistogramController::GetInstance()->NotifyChildDied<RenderProcessHost>(this);
  // This object is not deleted at this point and might be reused later.
  // TODO(darin): clean this up
}

void RenderProcessHostImpl::ResetIPC() {
  renderer_host_receiver_.reset();
  io_thread_host_impl_.reset();
  route_provider_receiver_.reset();
  associated_interface_provider_receivers_.Clear();
  associated_interfaces_.reset();
  coordinator_connector_receiver_.reset();
  tracing_registration_.reset();

  // Destroy all embedded CompositorFrameSinks.
  embedded_frame_sink_provider_.reset();

  dom_storage_provider_receiver_.reset();
  for (auto receiver_id : dom_storage_receiver_ids_)
    storage_partition_impl_->UnbindDomStorage(receiver_id);

  instance_weak_factory_.emplace(this);

  // If RenderProcessHostImpl is reused, the next renderer will send a new
  // request for FrameSinkProvider so make sure frame_sink_provider_ is ready
  // for that.
  frame_sink_provider_.Unbind();

  // If RenderProcessHostImpl is reused, the next renderer will send a new
  // request for CodeCacheHost.  Make sure that we clear the stale
  // object so that we can clearly create the new CodeCacheHostImpl while
  // asserting we don't have any duplicates.
  code_cache_host_impl_.reset();

  // It's important not to wait for the DeleteTask to delete the channel
  // proxy. Kill it off now. That way, in case the profile is going away, the
  // rest of the objects attached to this RenderProcessHost start going
  // away first, since deleting the channel proxy will post a
  // OnChannelClosed() to IPC::ChannelProxy::Context on the IO thread.
  ResetChannelProxy();

  // The PermissionServiceContext holds PermissionSubscriptions originating from
  // service workers. These subscriptions observe the PermissionControllerImpl
  // that is owned by the Profile corresponding to |this|. At this point, IPC
  // are unbound so no new subscriptions can be made. Existing subscriptions
  // need to be released here, as the Profile, and with it, the
  // PermissionControllerImpl, can be destroyed anytime after
  // RenderProcessHostImpl::Cleanup() returns.
  permission_service_context_.reset();
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

// static
void RenderProcessHost::SetHungRendererAnalysisFunction(
    AnalyzeHungRendererFunction analyze_hung_renderer) {
  g_analyze_hung_renderer = analyze_hung_renderer;
}

void RenderProcessHostImpl::ReleaseOnCloseACK(
    RenderProcessHost* host,
    const SessionStorageNamespaceMap& sessions,
    int widget_route_id) {
  DCHECK(host);
  if (sessions.empty())
    return;
  SessionStorageHolder* holder = static_cast<SessionStorageHolder*>(
      host->GetUserData(kSessionStorageHolderKey));
  if (!holder) {
    holder = new SessionStorageHolder();
    host->SetUserData(kSessionStorageHolderKey, base::WrapUnique(holder));
  }
  holder->Hold(sessions, widget_route_id);
}

void RenderProcessHostImpl::SuddenTerminationChanged(bool enabled) {
  SetSuddenTerminationAllowed(enabled);
}

void RenderProcessHostImpl::RecordUserMetricsAction(const std::string& action) {
  base::RecordComputedAction(action);
}

void RenderProcessHostImpl::ResolveProxy(
    const GURL& url,
    mojom::RendererHost::ResolveProxyCallback callback) {
  resolve_proxy_helper_->ResolveProxy(url, std::move(callback));
}

void RenderProcessHostImpl::UpdateProcessPriorityInputs() {
  int32_t new_visible_widgets_count = 0;
  unsigned int new_frame_depth = kMaxFrameDepthForPriority;
  bool new_intersects_viewport = false;
#if defined(OS_ANDROID)
  ChildProcessImportance new_effective_importance =
      ChildProcessImportance::NORMAL;
#endif
  for (auto* client : priority_clients_) {
    Priority priority = client->GetPriority();

    // Compute the lowest depth of widgets with highest visibility priority.
    // See comment on |frame_depth_| for more details.
    if (priority.is_hidden) {
      if (!new_visible_widgets_count) {
        new_frame_depth = std::min(new_frame_depth, priority.frame_depth);
        new_intersects_viewport =
            new_intersects_viewport || priority.intersects_viewport;
      }
    } else {
      if (new_visible_widgets_count) {
        new_frame_depth = std::min(new_frame_depth, priority.frame_depth);
        new_intersects_viewport =
            new_intersects_viewport || priority.intersects_viewport;
      } else {
        new_frame_depth = priority.frame_depth;
        new_intersects_viewport = priority.intersects_viewport;
      }
      new_visible_widgets_count++;
    }

#if defined(OS_ANDROID)
    new_effective_importance =
        std::max(new_effective_importance, priority.importance);
#endif
  }

  bool inputs_changed = new_visible_widgets_count != visible_clients_ ||
                        frame_depth_ != new_frame_depth ||
                        intersects_viewport_ != new_intersects_viewport;
  visible_clients_ = new_visible_widgets_count;
  frame_depth_ = new_frame_depth;
  intersects_viewport_ = new_intersects_viewport;
#if defined(OS_ANDROID)
  inputs_changed =
      inputs_changed || new_effective_importance != effective_importance_;
  effective_importance_ = new_effective_importance;
#endif
  if (inputs_changed)
    UpdateProcessPriority();
}

void RenderProcessHostImpl::UpdateProcessPriority() {
  if (!run_renderer_in_process() && (!child_process_launcher_.get() ||
                                     child_process_launcher_->IsStarting())) {
    // This path can be hit early (no-op) or on ProcessDied(). Reset
    // |priority_| to defaults in case this RenderProcessHostImpl is re-used.
    priority_.visible = !blink::kLaunchingProcessIsBackgrounded;
    priority_.boost_for_pending_views = true;
    return;
  }

  if (!has_recorded_media_stream_frame_depth_metric_ && !visible_clients_ &&
      media_stream_count_) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "BrowserRenderProcessHost.InvisibleMediaStreamFrameDepth", frame_depth_,
        50);
    has_recorded_media_stream_frame_depth_metric_ = true;
  }

  ChildProcessLauncherPriority priority(
      visible_clients_ > 0 || base::CommandLine::ForCurrentProcess()->HasSwitch(
                                  switches::kDisableRendererBackgrounding),
      media_stream_count_ > 0, foreground_service_worker_count_ > 0,
      HasOnlyLowPriorityFrames(), frame_depth_, intersects_viewport_,
      !!pending_views_ /* boost_for_pending_views */
#if defined(OS_ANDROID)
      ,
      GetEffectiveImportance()
#endif
  );

  // If a priority override has been specified, use it instead.
  // TODO(chrisha): After experimentation, either integrate the experimental
  // logic into this class, or rip out the existing logic entirely.
  if (priority_override_.has_value()) {
    bool foregrounded = priority_override_.value();
    priority = ChildProcessLauncherPriority(
        foregrounded, /* is_visible */
        foregrounded, /* has_media_stream */
        foregrounded, /* has_foreground_service_worker */
        false,        /* has_only_low_priority_frames */
        0,            /* frame_depth */
        foregrounded, /* intersects_viewport */
        false         /* boost_for_pending_views */
#if defined(OS_ANDROID)
        ,
        foregrounded ? ChildProcessImportance::NORMAL
                     : ChildProcessImportance::MODERATE /* importance */
#endif
    );
    DCHECK_EQ(!foregrounded, priority.is_background());
  }

  if (priority_ == priority)
    return;
  const bool background_state_changed =
      priority_.is_background() != priority.is_background();
  const bool visibility_state_changed = priority_.visible != priority.visible;

  TRACE_EVENT2("renderer_host", "RenderProcessHostImpl::UpdateProcessPriority",
               "should_background", priority.is_background(),
               "has_pending_views", priority.boost_for_pending_views);
  priority_ = priority;

  // Control the background state from the browser process, otherwise the task
  // telling the renderer to "unbackground" itself may be preempted by other
  // tasks executing at lowered priority ahead of it or simply by not being
  // swiftly scheduled by the OS per the low process priority
  // (http://crbug.com/398103).
  if (!run_renderer_in_process()) {
    DCHECK(child_process_launcher_.get());
    DCHECK(!child_process_launcher_->IsStarting());
    // Make sure to keep the pid in the trace so we can tell which process is
    // being modified.
    TRACE_EVENT2(
        "renderer_host",
        "RenderProcessHostImpl::UpdateProcessPriority.SetProcessPriority",
        "pid", child_process_launcher_->GetProcess().Pid(),
        "priority_is_background", priority.is_background());
    child_process_launcher_->SetProcessPriority(priority_);
  }

  // When switching in/out of the background, update the time spent in the
  // background so the time spent backgrounded vs overall can be reported.
  if (background_state_changed) {
    is_backgrounded_ = priority_.is_background();
    // Don't update backgrounding metrics until the render process finishes
    // initializing, at which point it will set |background_status_update_time_|
    // to the current time.
    if (!background_status_update_time_.is_null()) {
      base::TimeTicks update_time = clock_->NowTicks();
      base::TimeDelta update_duration =
          update_time - background_status_update_time_;
      background_status_update_time_ = update_time;
      if (!is_backgrounded_)
        background_duration_ += update_duration;
    }
  }

  // Notify the child process of the change in state.
  if ((background_state_changed) || visibility_state_changed) {
    SendProcessStateToRenderer();
  }
}

void RenderProcessHostImpl::SendProcessStateToRenderer() {
  mojom::RenderProcessBackgroundState background_state =
      priority_.is_background()
          ? mojom::RenderProcessBackgroundState::kBackgrounded
          : mojom::RenderProcessBackgroundState::kForegrounded;
  mojom::RenderProcessVisibleState visible_state =
      priority_.visible ? mojom::RenderProcessVisibleState::kVisible
                        : mojom::RenderProcessVisibleState::kHidden;
  GetRendererInterface()->SetProcessState(background_state, visible_state);
}

void RenderProcessHostImpl::OnProcessLaunched() {
  // No point doing anything, since this object will be destructed soon.  We
  // especially don't want to send the RENDERER_PROCESS_CREATED notification,
  // since some clients might expect a RENDERER_PROCESS_TERMINATED afterwards
  // to properly cleanup.
  if (deleting_soon_)
    return;

  if (child_process_launcher_) {
    DCHECK(child_process_launcher_->GetProcess().IsValid());
    // TODO(https://crbug.com/875933): This should be based on
    // |priority_.is_background()|, see similar check below.
    DCHECK_EQ(blink::kLaunchingProcessIsBackgrounded, !priority_.visible);

    // Unpause the channel now that the process is launched. We don't flush it
    // yet to ensure that any initialization messages sent here (e.g., things
    // done in response to NOTIFICATION_RENDER_PROCESS_CREATED; see below)
    // preempt already queued messages.
    channel_->Unpause(false /* flush */);

    if (coordinator_connector_receiver_.is_bound())
      coordinator_connector_receiver_.Resume();

      // Not all platforms launch processes in the same backgrounded state. Make
      // sure |priority_.visible| reflects this platform's initial process
      // state.
#if defined(OS_MACOSX)
    priority_.visible =
        !child_process_launcher_->GetProcess().IsProcessBackgrounded(
            ChildProcessTaskPortProvider::GetInstance());
#elif defined(OS_ANDROID)
    // Android child process priority works differently and cannot be queried
    // directly from base::Process.
    // TODO(https://crbug.com/875933): Fix initial priority on Android to
    // reflect |priority_.is_background()|.
    DCHECK_EQ(blink::kLaunchingProcessIsBackgrounded, !priority_.visible);
#else
    priority_.visible =
        !child_process_launcher_->GetProcess().IsProcessBackgrounded();
#endif  // defined(OS_MACOSX)

    // Only update the priority on startup if boosting is enabled (to avoid
    // reintroducing https://crbug.com/560446#c13 while pending views only
    // experimentally result in a boost).
    if (priority_.boost_for_pending_views)
      UpdateProcessPriority();

    // Share histograms between the renderer and this process.
    CreateSharedRendererHistogramAllocator();
  }

  // Pass bits of global renderer state to the renderer.
  GetRendererInterface()->SetUserAgent(
      GetContentClient()->browser()->GetUserAgent());
  GetRendererInterface()->SetUserAgentMetadata(
      GetContentClient()->browser()->GetUserAgentMetadata());
  NotifyRendererIfLockedToSite();
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites() &&
      base::FeatureList::IsEnabled(features::kV8LowMemoryModeForSubframes)) {
    GetRendererInterface()->EnableV8LowMemoryMode();
  }

  // Send the initial system color info to the renderer.
  ThemeHelper::GetInstance()->SendSystemColorInfo(GetRendererInterface());

  // NOTE: This needs to be before flushing queued messages, because
  // ExtensionService uses this notification to initialize the renderer
  // process with state that must be there before any JavaScript executes.
  //
  // The queued messages contain such things as "navigate". If this
  // notification was after, we can end up executing JavaScript before the
  // initialization happens.
  NotificationService::current()->Notify(NOTIFICATION_RENDERER_PROCESS_CREATED,
                                         Source<RenderProcessHost>(this),
                                         NotificationService::NoDetails());
  for (auto* observer : GetAllCreationObservers())
    observer->OnRenderProcessHostCreated(this);

  if (child_process_launcher_)
    channel_->Flush();

  if (IsReady()) {
    DCHECK(!sent_render_process_ready_);
    sent_render_process_ready_ = true;
    // Send RenderProcessReady only if the channel is already connected.
    for (auto& observer : observers_)
      observer.RenderProcessReady(this);

#if defined(OS_LINUX)
    // Provide /proc/{renderer pid}/status and statm files for
    // MemoryUsageMonitor in blink.
    ProvideStatusFileForRenderer();
#endif
  }

  aec_dump_manager_.set_pid(GetProcess().Pid());
  aec_dump_manager_.AutoStart();

  tracing_registration_ = TracingServiceController::Get().RegisterClient(
      GetProcess().Pid(),
      base::BindRepeating(&RenderProcessHostImpl::BindTracedProcess,
                          instance_weak_factory_->GetWeakPtr()));
}

void RenderProcessHostImpl::OnProcessLaunchFailed(int error_code) {
  // If this object will be destructed soon, then observers have already been
  // sent a RenderProcessHostDestroyed notification, and we must observe our
  // contract that says that will be the last call.
  if (deleting_soon_)
    return;

  ChildProcessTerminationInfo info;
  info.status = base::TERMINATION_STATUS_LAUNCH_FAILED;
  info.exit_code = error_code;
  PopulateTerminationInfoRendererFields(&info);
  ProcessDied(true, &info);
}

void RenderProcessHostImpl::OnCloseACK(int closed_widget_route_id) {
  SessionStorageHolder* holder =
      static_cast<SessionStorageHolder*>(GetUserData(kSessionStorageHolderKey));
  if (!holder)
    return;
  holder->Release(closed_widget_route_id);
}

// static
RenderProcessHost*
RenderProcessHostImpl::FindReusableProcessHostForSiteInstance(
    SiteInstanceImpl* site_instance) {
  BrowserContext* browser_context = site_instance->GetBrowserContext();
  GURL site_url(site_instance->GetSiteURL());
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
    pending_tracker->FindRenderProcessesForSiteInstance(
        site_instance, &eligible_foreground_hosts, &eligible_background_hosts);
  }

  if (eligible_foreground_hosts.empty()) {
    // If needed, add the RenderProcessHosts hosting a frame for |site_url| to
    // the list of eligible RenderProcessHosts.
    SiteProcessCountTracker* committed_tracker =
        static_cast<SiteProcessCountTracker*>(
            browser_context->GetUserData(kCommittedSiteProcessCountTrackerKey));
    if (committed_tracker) {
      committed_tracker->FindRenderProcessesForSiteInstance(
          site_instance, &eligible_foreground_hosts,
          &eligible_background_hosts);
    }
  }

  if (!eligible_foreground_hosts.empty()) {
    int index = base::RandInt(0, eligible_foreground_hosts.size() - 1);
    auto iterator = eligible_foreground_hosts.begin();
    for (int i = 0; i < index; ++i)
      ++iterator;
    return *iterator;
  }

  if (!eligible_background_hosts.empty()) {
    int index = base::RandInt(0, eligible_background_hosts.size() - 1);
    auto iterator = eligible_background_hosts.begin();
    for (int i = 0; i < index; ++i)
      ++iterator;
    return *iterator;
  }

  return nullptr;
}

void RenderProcessHostImpl::CreateAgentMetricsCollectorHost(
    mojo::PendingReceiver<blink::mojom::AgentMetricsCollectorHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!agent_metrics_collector_host_) {
    agent_metrics_collector_host_.reset(
        new AgentMetricsCollectorHost(this->GetID(), std::move(receiver)));
    AddObserver(agent_metrics_collector_host_.get());
  }
}

void RenderProcessHostImpl::BindPeerConnectionTrackerHost(
    mojo::PendingReceiver<blink::mojom::PeerConnectionTrackerHost> receiver) {
  GetPeerConnectionTrackerHost()->BindReceiver(std::move(receiver));
}

#if BUILDFLAG(ENABLE_MDNS)
void RenderProcessHostImpl::CreateMdnsResponder(
    mojo::PendingReceiver<network::mojom::MdnsResponder> receiver) {
  GetStoragePartition()->GetNetworkContext()->CreateMdnsResponder(
      std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_MDNS)

// static
void RenderProcessHostImpl::OnMojoError(int render_process_id,
                                        const std::string& error) {
  LOG(ERROR) << "Terminating render process for bad Mojo message: " << error;

  InvokeBadMojoMessageCallbackForTesting(render_process_id, error);

  // The ReceivedBadMessage call below will trigger a DumpWithoutCrashing.
  // Capture the error message in a crash key value.
  mojo::debug::ScopedMessageErrorCrashKey error_key_value(error);
  bad_message::ReceivedBadMessage(render_process_id,
                                  bad_message::RPH_MOJO_PROCESS_ERROR);
}

void RenderProcessHostImpl::GetBrowserHistogram(
    const std::string& name,
    BrowserHistogramCallback callback) {
  // Security: Only allow access to browser histograms when running in the
  // context of a test.
  bool using_stats_collection_controller =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStatsCollectionController);
  if (!using_stats_collection_controller) {
    std::move(callback).Run(std::string());
    return;
  }
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  std::string histogram_json;
  if (!histogram) {
    histogram_json = "{}";
  } else {
    histogram->WriteJSON(&histogram_json, base::JSON_VERBOSITY_LEVEL_FULL);
  }
  std::move(callback).Run(histogram_json);
}

void RenderProcessHostImpl::BindTracedProcess(
    mojo::PendingReceiver<tracing::mojom::TracedProcess> receiver) {
  BindReceiver(std::move(receiver));
}

void RenderProcessHostImpl::OnBindHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  GetContentClient()->browser()->BindHostReceiverForRenderer(
      this, std::move(receiver));
}

// static
void RenderProcessHost::InterceptBindHostReceiverForTesting(
    BindHostReceiverInterceptor callback) {
  GetBindHostReceiverInterceptor() = std::move(callback);
}

#if defined(OS_LINUX)
void RenderProcessHostImpl::ProvideStatusFileForRenderer() {
  // We use ScopedAllowBlocking, because opening /proc/{pid}/status and
  // /proc/{pid}/statm is not blocking call.
  base::ScopedAllowBlocking allow_blocking;
  base::FilePath proc_pid_dir =
      base::FilePath("/proc").Append(base::NumberToString(GetProcess().Pid()));

  base::File status_file(
      proc_pid_dir.Append("status"),
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  base::File statm_file(
      proc_pid_dir.Append("statm"),
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  if (!status_file.IsValid() || !statm_file.IsValid())
    return;

  mojo::Remote<blink::mojom::MemoryUsageMonitorLinux> monitor;
  BindReceiver(monitor.BindNewPipeAndPassReceiver());
  monitor->SetProcFiles(statm_file.Duplicate(), status_file.Duplicate());
}
#endif

void RenderProcessHostImpl::ProvideSwapFileForRenderer() {
  if (!base::FeatureList::IsEnabled(features::kParkableStringsToDisk))
    return;
  mojo::Remote<blink::mojom::DiskAllocator> allocator;
  BindReceiver(allocator.BindNewPipeAndPassReceiver());

  // In Incognito, nothing should be written to disk. Providing an invalid file
  // prevents the renderer from doing so.
  if (GetBrowserContext()->IsOffTheRecord()) {
    allocator->ProvideTemporaryFile(base::File());
    return;
  }

  // File creation done on a background thread. The renderer side will behave
  // correctly even if the file is provided later or never.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce([]() {
        base::FilePath path;
        CHECK(base::CreateTemporaryFile(&path));

        int flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                    base::File::FLAG_WRITE | base::File::FLAG_DELETE_ON_CLOSE;
        auto file = base::File(base::FilePath(path), flags);
        CHECK(file.IsValid());
        return file;
      }),
      base::BindOnce(
          [](mojo::Remote<blink::mojom::DiskAllocator> allocator,
             base::File file) {
            allocator->ProvideTemporaryFile(std::move(file));
          },
          std::move(allocator)));
}

}  // namespace content
