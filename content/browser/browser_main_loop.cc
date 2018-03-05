// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/memory_coordinator_proxy.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/pending_task.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "base/system_monitor/system_monitor.h"
#include "base/task_scheduler/initialization_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/single_thread_task_runner_thread_mode.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/hi_res_timer_manager.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/tracing/common/trace_config_file.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/forwarding_compositing_mode_reporter_impl.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display_embedder/compositing_mode_reporter_impl.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/compositor/gpu_process_transport_factory.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/compositor/viz_process_transport_factory.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/browser/histogram_synchronizer.h"
#include "content/browser/leveldb_wrapper_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader_delegate_impl.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/memory/memory_coordinator_impl.h"
#include "content/browser/memory/swap_metrics_delegate_uma.h"
#include "content/browser/net/browser_online_state_observer.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/browser/site_isolation_policy.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/startup_task_runner.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/browser/utility_process_host_impl.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/common/content_switches_internal.h"
#include "content/common/service_manager/service_manager_connection_impl.h"
#include "content/common/task_scheduler.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/swap_metrics_driver.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/zygote_features.h"
#include "device/gamepad/gamepad_service.h"
#include "gpu/vulkan/features.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_system.h"
#include "media/audio/audio_thread_impl.h"
#include "media/base/media.h"
#include "media/base/user_input_monitor.h"
#include "media/media_features.h"
#include "media/midi/midi_service.h"
#include "media/mojo/features.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/client_socket_factory.h"
#include "net/ssl/ssl_config_service.h"
#include "ppapi/features/features.h"
#include "services/audio/public/cpp/audio_system_factory.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/runner/common/client_util.h"
#include "skia/ext/event_tracer_impl.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "sql/sql_memory_dump_provider.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/switches.h"

#if defined(USE_AURA) || defined(OS_MACOSX)
#include "content/browser/compositor/image_transport_factory.h"
#endif

#if defined(USE_AURA)
#include "content/public/browser/context_factory.h"
#include "ui/aura/env.h"
#endif

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "content/public/common/common_sandbox_support_linux.h"
#include "content/public/common/zygote_handle.h"
#include "media/base/media_switches.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "components/tracing/common/graphics_memory_dump_provider_android.h"
#include "content/browser/android/browser_startup_controller.h"
#include "content/browser/android/launcher_thread.h"
#include "content/browser/android/scoped_surface_request_manager.h"
#include "content/browser/android/tracing_controller_android.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/screen_orientation/screen_orientation_delegate_android.h"
#include "media/base/android/media_drm_bridge_client.h"
#include "ui/android/screen_android.h"
#include "ui/display/screen.h"
#include "ui/gl/gl_surface.h"
#endif

#if defined(OS_MACOSX)
#include "base/allocator/allocator_interception_mac.h"
#include "base/memory/memory_pressure_monitor_mac.h"
#include "content/browser/cocoa/system_hotkey_helper_mac.h"
#include "content/browser/mach_broker_mac.h"
#include "content/browser/renderer_host/browser_compositor_view_mac.h"
#include "content/browser/theme_helper_mac.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "base/memory/memory_pressure_monitor_win.h"
#include "net/base/winsock_init.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/display/win/screen_win.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/memory/memory_pressure_monitor_chromeos.h"
#include "chromeos/chromeos_switches.h"
#endif

#if defined(USE_GLIB)
#include <glib-object.h>
#endif

#if defined(OS_WIN)
#include "media/device_monitors/system_message_window_win.h"
#elif defined(OS_LINUX) && defined(USE_UDEV)
#include "media/device_monitors/device_monitor_udev.h"
#elif defined(OS_MACOSX)
#include "media/device_monitors/device_monitor_mac.h"
#endif

#if defined(OS_FUCHSIA)
#include <zircon/process.h>
#include <zircon/syscalls.h>

#include "base/fuchsia/default_job.h"
#endif  // defined(OS_FUCHSIA)

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "content/browser/sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"

#if !defined(OS_ANDROID)
#include "content/browser/zygote_host/zygote_communication_linux.h"
#endif  // !defined(OS_ANDROID)
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)


#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/browser/media/cdm_registry_impl.h"
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
#include "content/browser/webrtc/webrtc_event_log_manager.h"
#include "content/browser/webrtc/webrtc_internals.h"
#endif

#if defined(USE_X11)
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "ui/base/x/x11_util_internal.h"  // nogncheck
#include "ui/gfx/x/x11_connection.h"  // nogncheck
#include "ui/gfx/x/x11_types.h"  // nogncheck
#endif

#if defined(USE_NSS_CERTS)
#include "crypto/nss_util.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_implementation.h"
#endif

#if BUILDFLAG(ENABLE_MUS)
#include "services/ui/common/image_cursors_set.h"
#endif

// One of the linux specific headers defines this as a macro.
#ifdef DestroyAll
#undef DestroyAll
#endif

namespace content {
namespace {

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
pid_t LaunchZygoteHelper(base::CommandLine* cmd_line,
                         base::ScopedFD* control_fd) {
  // Append any switches from the browser process that need to be forwarded on
  // to the zygote/renderers.
  static const char* const kForwardSwitches[] = {
      switches::kAndroidFontsPath, switches::kClearKeyCdmPathForTesting,
      switches::kEnableHeapProfiling,
      switches::kEnableLogging,  // Support, e.g., --enable-logging=stderr.
      // Need to tell the zygote that it is headless so that we don't try to use
      // the wrong type of main delegate.
      switches::kHeadless,
      // Zygote process needs to know what resources to have loaded when it
      // becomes a renderer process.
      switches::kForceDeviceScaleFactor, switches::kLoggingLevel,
      switches::kPpapiInProcess, switches::kRegisterPepperPlugins, switches::kV,
      switches::kVModule,
  };
  cmd_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                             kForwardSwitches, arraysize(kForwardSwitches));

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(cmd_line, -1);

  // Start up the sandbox host process and get the file descriptor for the
  // sandboxed processes to talk to it.
  base::FileHandleMappingVector additional_remapped_fds;
  additional_remapped_fds.emplace_back(
      SandboxHostLinux::GetInstance()->GetChildSocket(), GetSandboxFD());

  return ZygoteHostImpl::GetInstance()->LaunchZygote(
      cmd_line, control_fd, std::move(additional_remapped_fds));
}

void SetupSandbox(const base::CommandLine& parsed_command_line) {
  TRACE_EVENT0("startup", "SetupSandbox");
  // SandboxHostLinux needs to be initialized even if the sandbox and
  // zygote are both disabled. It initializes the sandboxed process socket.
  SandboxHostLinux::GetInstance()->Init();

  if (parsed_command_line.HasSwitch(switches::kNoZygote) &&
      !parsed_command_line.HasSwitch(switches::kNoSandbox)) {
    LOG(ERROR) << "--no-sandbox should be used together with --no--zygote";
    exit(EXIT_FAILURE);
  }

  // Tickle the zygote host so it forks now.
  ZygoteHostImpl::GetInstance()->Init(parsed_command_line);
  ZygoteHandle generic_zygote =
      CreateGenericZygote(base::BindOnce(LaunchZygoteHelper));

  // TODO(kerrnel): Investigate doing this without the ZygoteHostImpl as a
  // proxy. It is currently done this way due to concerns about race
  // conditions.
  ZygoteHostImpl::GetInstance()->SetRendererSandboxStatus(
      generic_zygote->GetSandboxStatus());
}
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(USE_GLIB)
static void GLibLogHandler(const gchar* log_domain,
                           GLogLevelFlags log_level,
                           const gchar* message,
                           gpointer userdata) {
  if (!log_domain)
    log_domain = "<unknown>";
  if (!message)
    message = "<no message>";

  GLogLevelFlags always_fatal_flags = g_log_set_always_fatal(G_LOG_LEVEL_MASK);
  g_log_set_always_fatal(always_fatal_flags);
  GLogLevelFlags fatal_flags =
      g_log_set_fatal_mask(log_domain, G_LOG_LEVEL_MASK);
  g_log_set_fatal_mask(log_domain, fatal_flags);
  if ((always_fatal_flags | fatal_flags) & log_level) {
    LOG(DFATAL) << log_domain << ": " << message;
  } else if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL)) {
    LOG(ERROR) << log_domain << ": " << message;
  } else if (log_level & (G_LOG_LEVEL_WARNING)) {
    LOG(WARNING) << log_domain << ": " << message;
  } else if (log_level &
             (G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG)) {
    LOG(INFO) << log_domain << ": " << message;
  } else {
    NOTREACHED();
    LOG(DFATAL) << log_domain << ": " << message;
  }
}

static void SetUpGLibLogHandler() {
  // Register GLib-handled assertions to go through our logging system.
  const char* const kLogDomains[] =
      { nullptr, "Gtk", "Gdk", "GLib", "GLib-GObject" };
  for (size_t i = 0; i < arraysize(kLogDomains); i++) {
    g_log_set_handler(
        kLogDomains[i],
        static_cast<GLogLevelFlags>(G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL |
                                    G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
                                    G_LOG_LEVEL_WARNING),
        GLibLogHandler, nullptr);
  }
}
#endif  // defined(USE_GLIB)

void OnStoppedStartupTracing(const base::FilePath& trace_file) {
  VLOG(0) << "Completed startup tracing to " << trace_file.value();
}

// Disable optimizations for this block of functions so the compiler doesn't
// merge them all together. This makes it possible to tell what thread was
// unresponsive by inspecting the callstack.
MSVC_DISABLE_OPTIMIZE()
MSVC_PUSH_DISABLE_WARNING(4748)

#if defined(OS_ANDROID)
NOINLINE void ResetThread_PROCESS_LAUNCHER(
    std::unique_ptr<BrowserProcessSubThread> thread) {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  thread.reset();
}
#else   // defined(OS_ANDROID)
NOINLINE void ResetThread_PROCESS_LAUNCHER() {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  BrowserThreadImpl::StopRedirectionOfThreadID(BrowserThread::PROCESS_LAUNCHER);
}
#endif  // defined(OS_ANDROID)

NOINLINE void ResetThread_IO(std::unique_ptr<BrowserProcessSubThread> thread) {
  volatile int inhibit_comdat = __LINE__;
  ALLOW_UNUSED_LOCAL(inhibit_comdat);
  thread.reset();
}

MSVC_POP_WARNING()
MSVC_ENABLE_OPTIMIZE();

#if defined(OS_WIN)
// Creates a memory pressure monitor using automatic thresholds, or those
// specified on the command-line. Ownership is passed to the caller.
std::unique_ptr<base::win::MemoryPressureMonitor>
CreateWinMemoryPressureMonitor(const base::CommandLine& parsed_command_line) {
  std::vector<std::string> thresholds =
      base::SplitString(parsed_command_line.GetSwitchValueASCII(
                            switches::kMemoryPressureThresholdsMb),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  int moderate_threshold_mb = 0;
  int critical_threshold_mb = 0;
  if (thresholds.size() == 2 &&
      base::StringToInt(thresholds[0], &moderate_threshold_mb) &&
      base::StringToInt(thresholds[1], &critical_threshold_mb) &&
      moderate_threshold_mb >= critical_threshold_mb &&
      critical_threshold_mb >= 0) {
    return std::make_unique<base::win::MemoryPressureMonitor>(
        moderate_threshold_mb, critical_threshold_mb);
  }

  // In absence of valid switches use the automatic defaults.
  return std::make_unique<base::win::MemoryPressureMonitor>();
}
#endif  // defined(OS_WIN)

enum WorkerPoolType : size_t {
  BACKGROUND = 0,
  BACKGROUND_BLOCKING,
  FOREGROUND,
  FOREGROUND_BLOCKING,
  WORKER_POOL_COUNT  // Always last.
};

std::unique_ptr<base::TaskScheduler::InitParams>
GetDefaultTaskSchedulerInitParams() {
#if defined(OS_ANDROID)
  // Mobile config, for iOS see ios/web/app/web_main_loop.cc.
  return std::make_unique<base::TaskScheduler::InitParams>(
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(2, 8, 0.1, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(2, 8, 0.1, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.3, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.3, 0),
          base::TimeDelta::FromSeconds(60)));
#else
  // Desktop config.
  return std::make_unique<base::TaskScheduler::InitParams>(
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0),
          base::TimeDelta::FromSeconds(40)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0),
          base::TimeDelta::FromSeconds(60))
#if defined(OS_WIN)
          ,
      base::TaskScheduler::InitParams::SharedWorkerPoolEnvironment::COM_MTA
#endif  // defined(OS_WIN)
      );
#endif
}

#if !defined(OS_FUCHSIA)
// Time between updating and recording swap rates.
constexpr base::TimeDelta kSwapMetricsInterval =
    base::TimeDelta::FromSeconds(60);
#endif  // !defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
// Create and register the job which will contain all child processes
// of the browser process as well as their descendents.
void InitDefaultJob() {
  base::ScopedZxHandle handle;
  zx_status_t result = zx_job_create(zx_job_default(), 0, handle.receive());
  CHECK_EQ(ZX_OK, result) << "zx_job_create(job): "
                          << zx_status_get_string(result);
  base::SetDefaultJob(std::move(handle));
}
#endif  // defined(OS_FUCHSIA)

}  // namespace

#if defined(USE_X11)
namespace internal {

// Forwards GPUInfo updates to ui::XVisualManager
class GpuDataManagerVisualProxy : public GpuDataManagerObserver {
 public:
  explicit GpuDataManagerVisualProxy(GpuDataManagerImpl* gpu_data_manager)
      : gpu_data_manager_(gpu_data_manager) {
    gpu_data_manager_->AddObserver(this);
  }

  ~GpuDataManagerVisualProxy() override {
    gpu_data_manager_->RemoveObserver(this);
  }

  void OnGpuInfoUpdate() override {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless))
      return;
    gpu::GPUInfo gpu_info = gpu_data_manager_->GetGPUInfo();
    if (!ui::XVisualManager::GetInstance()->OnGPUInfoChanged(
            gpu_info.software_rendering ||
                !gpu_data_manager_->GpuAccessAllowed(nullptr),
            gpu_info.system_visual, gpu_info.rgba_visual)) {
      // The GPU process sent back bad visuals, which should never happen.
      auto* gpu_process_host = GpuProcessHost::Get(
          GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED, false);
      if (gpu_process_host)
        gpu_process_host->ForceShutdown();
    }
  }

 private:
  GpuDataManagerImpl* gpu_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManagerVisualProxy);
};

}  // namespace internal
#endif

#if defined(OS_WIN)
namespace {

// Provides a bridge whereby display::win::ScreenWin can ask the GPU process
// about the HDR status of the system.
class HDRProxy {
 public:
  static void Initialize() {
    display::win::ScreenWin::SetRequestHDRStatusCallback(
        base::Bind(&HDRProxy::RequestHDRStatus));
  }

  static void RequestHDRStatus() {
    // The request must be sent to the GPU process from the IO thread.
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&HDRProxy::RequestOnIOThread));
  }

 private:
  static void RequestOnIOThread() {
    auto* gpu_process_host =
        GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED, false);
    if (gpu_process_host) {
      gpu_process_host->RequestHDRStatus(
          base::Bind(&HDRProxy::GotResultOnIOThread));
    } else {
      bool hdr_enabled = false;
      GotResultOnIOThread(hdr_enabled);
    }
  }
  static void GotResultOnIOThread(bool hdr_enabled) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&HDRProxy::GotResult, hdr_enabled));
  }
  static void GotResult(bool hdr_enabled) {
    display::win::ScreenWin::SetHDREnabled(hdr_enabled);
  }
};

}  // namespace
#endif

// The currently-running BrowserMainLoop.  There can be one or zero.
BrowserMainLoop* g_current_browser_main_loop = nullptr;

#if defined(OS_ANDROID)
bool g_browser_main_loop_shutting_down = false;
#endif

// BrowserMainLoop construction / destruction =============================

BrowserMainLoop* BrowserMainLoop::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_current_browser_main_loop;
}

BrowserMainLoop::BrowserMainLoop(const MainFunctionParams& parameters)
    : parameters_(parameters),
      parsed_command_line_(parameters.command_line),
      result_code_(RESULT_CODE_NORMAL_EXIT),
      created_threads_(false),
      // ContentMainRunner should have enabled tracing of the browser process
      // when kTraceStartup or kTraceConfigFile is in the command line.
      is_tracing_startup_for_duration_(
          parameters.command_line.HasSwitch(switches::kTraceStartup) ||
          (tracing::TraceConfigFile::GetInstance()->IsEnabled() &&
           tracing::TraceConfigFile::GetInstance()->GetStartupDuration() > 0)) {
  DCHECK(!g_current_browser_main_loop);
  g_current_browser_main_loop = this;

  if (GetContentClient()->browser()->ShouldCreateTaskScheduler()) {
    // Use an empty string as TaskScheduler name to match the suffix of browser
    // process TaskScheduler histograms.
    base::TaskScheduler::Create("Browser");
  }
}

BrowserMainLoop::~BrowserMainLoop() {
  DCHECK_EQ(this, g_current_browser_main_loop);
  ui::Clipboard::DestroyClipboardForCurrentThread();
  g_current_browser_main_loop = nullptr;
}

void BrowserMainLoop::Init() {
  TRACE_EVENT0("startup", "BrowserMainLoop::Init");

  parts_.reset(
      GetContentClient()->browser()->CreateBrowserMainParts(parameters_));
}

// BrowserMainLoop stages ==================================================

int BrowserMainLoop::EarlyInitialization() {
  TRACE_EVENT0("startup", "BrowserMainLoop::EarlyInitialization");

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  // No thread should be created before this call, as SetupSandbox()
  // will end-up using fork().
  SetupSandbox(parsed_command_line_);
#endif

#if defined(USE_X11)
  if (UsingInProcessGpu()) {
    if (!gfx::InitializeThreadedX11()) {
      LOG(ERROR) << "Failed to put Xlib into threaded mode.";
    }
  }
#endif

  // GLib's spawning of new processes is buggy, so it's important that at this
  // point GLib does not need to start DBUS. Chrome should always start with
  // DBUS_SESSION_BUS_ADDRESS properly set. See crbug.com/309093.
#if defined(USE_GLIB)
  // g_type_init will be deprecated in 2.36. 2.35 is the development
  // version for 2.36, hence do not call g_type_init starting 2.35.
  // http://developer.gnome.org/gobject/unstable/gobject-Type-Information.html#g-type-init
#if !GLIB_CHECK_VERSION(2, 35, 0)
  // GLib type system initialization. It's unclear if it's still required for
  // any remaining code. Most likely this is superfluous as gtk_init() ought
  // to do this. It's definitely harmless, so it's retained here.
  g_type_init();
#endif  // !GLIB_CHECK_VERSION(2, 35, 0)

  SetUpGLibLogHandler();
#endif  // defined(USE_GLIB)

  if (parts_) {
    const int pre_early_init_error_code = parts_->PreEarlyInitialization();
    if (pre_early_init_error_code != content::RESULT_CODE_NORMAL_EXIT)
      return pre_early_init_error_code;
  }

  if (!parts_ || parts_->ShouldContentCreateFeatureList()) {
    // Note that we do not initialize a new FeatureList when calling this for
    // the second time.
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    base::FeatureList::InitializeInstance(
        command_line->GetSwitchValueASCII(switches::kEnableFeatures),
        command_line->GetSwitchValueASCII(switches::kDisableFeatures));
  }

#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_CHROMEOS)
  // We use quite a few file descriptors for our IPC as well as disk the disk
  // cache,and the default limit on the Mac is low (256), so bump it up.

  // Same for Linux. The default various per distro, but it is 1024 on Fedora.
  // Low soft limits combined with liberal use of file descriptors means power
  // users can easily hit this limit with many open tabs. Bump up the limit to
  // an arbitrarily high number. See https://crbug.com/539567
  base::SetFdLimit(8192);
#endif  // defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif

#if defined(USE_NSS_CERTS)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif

#if defined(OS_FUCHSIA)
  InitDefaultJob();
#endif

  if (parsed_command_line_.HasSwitch(switches::kRendererProcessLimit)) {
    std::string limit_string = parsed_command_line_.GetSwitchValueASCII(
        switches::kRendererProcessLimit);
    size_t process_limit;
    if (base::StringToSizeT(limit_string, &process_limit)) {
      RenderProcessHost::SetMaxRendererProcessCount(process_limit);
    }
  }

  if (parts_)
    parts_->PostEarlyInitialization();

  return content::RESULT_CODE_NORMAL_EXIT;
}

void BrowserMainLoop::PreMainMessageLoopStart() {
  if (parts_) {
    TRACE_EVENT0("startup",
        "BrowserMainLoop::MainMessageLoopStart:PreMainMessageLoopStart");
    parts_->PreMainMessageLoopStart();
  }

#if defined(OS_WIN)
  // If we're running tests (ui_task is non-null), then the ResourceBundle
  // has already been initialized.
  if (!parameters_.ui_task) {
    // Override the configured locale with the user's preferred UI language.
    l10n_util::OverrideLocaleWithUILanguageList();
  }
#endif
}

void BrowserMainLoop::MainMessageLoopStart() {
  // DO NOT add more code here. Use PreMainMessageLoopStart() above or
  // PostMainMessageLoopStart() below.

  TRACE_EVENT0("startup", "BrowserMainLoop::MainMessageLoopStart");

  // Create a MessageLoop if one does not already exist for the current thread.
  if (!base::MessageLoop::current())
    main_message_loop_.reset(new base::MessageLoopForUI);

  InitializeMainThread();
}

void BrowserMainLoop::PostMainMessageLoopStart() {
  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:CreateBrowserThread::IO");
    InitializeIOThread();
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:SystemMonitor");
    system_monitor_.reset(new base::SystemMonitor);
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:PowerMonitor");
    std::unique_ptr<base::PowerMonitorSource> power_monitor_source(
        new base::PowerMonitorDeviceSource());
    power_monitor_.reset(
        new base::PowerMonitor(std::move(power_monitor_source)));
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:HighResTimerManager");
    hi_res_timer_manager_.reset(new base::HighResolutionTimerManager);
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:NetworkChangeNotifier");
    network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:MediaFeatures");
    media::InitializeMediaLibrary();
  }
  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:ContentWebUIController");
    WebUIControllerFactory::RegisterFactory(
        ContentWebUIControllerFactory::GetInstance());
  }

  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:OnlineStateObserver");
    online_state_observer_.reset(new BrowserOnlineStateObserver);
  }

  {
    system_stats_monitor_.reset(
        new base::trace_event::TraceEventSystemStatsMonitor(
            base::ThreadTaskRunnerHandle::Get()));
  }

  {
    base::SetRecordActionTaskRunner(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::UI));
  }

  if (!base::FeatureList::IsEnabled(::features::kMash)) {
    discardable_shared_memory_manager_ =
        std::make_unique<discardable_memory::DiscardableSharedMemoryManager>();
    // TODO(boliu): kSingleProcess check is a temporary workaround for
    // in-process Android WebView. crbug.com/503724 tracks proper fix.
    if (!parsed_command_line_.HasSwitch(switches::kSingleProcess)) {
      base::DiscardableMemoryAllocator::SetInstance(
          discardable_shared_memory_manager_.get());
    }
  }

  if (parts_)
    parts_->PostMainMessageLoopStart();

#if defined(OS_ANDROID)
  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:BrowserMediaPlayerManager");
    if (UsingInProcessGpu()) {
      gpu::ScopedSurfaceRequestConduit::SetInstance(
          ScopedSurfaceRequestManager::GetInstance());
    }
  }

  if (!parsed_command_line_.HasSwitch(
      switches::kDisableScreenOrientationLock)) {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:ScreenOrientationProvider");
    screen_orientation_delegate_.reset(
        new ScreenOrientationDelegateAndroid());
  }
#endif

  if (parsed_command_line_.HasSwitch(
          switches::kEnableAggressiveDOMStorageFlushing)) {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:EnableAggressiveCommitDelay");
    DOMStorageArea::EnableAggressiveCommitDelay();
    LevelDBWrapperImpl::EnableAggressiveCommitDelay();
  }

  // Enable memory-infra dump providers.
  InitSkiaEventTracer();
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      viz::ServerSharedBitmapManager::current(),
      "viz::ServerSharedBitmapManager", nullptr);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      skia::SkiaMemoryDumpProvider::GetInstance(), "Skia", nullptr);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      sql::SqlMemoryDumpProvider::GetInstance(), "Sql", nullptr);
}

int BrowserMainLoop::PreCreateThreads() {
  if (parts_) {
    TRACE_EVENT0("startup",
        "BrowserMainLoop::CreateThreads:PreCreateThreads");

    result_code_ = parts_->PreCreateThreads();
  }

  InitializeMemoryManagementComponent();

#if defined(OS_MACOSX)
  if (base::CommandLine::InitializedForCurrentProcess() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableHeapProfiling)) {
    base::allocator::PeriodicallyShimNewMallocZones();
  }
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  // Prior to any processing happening on the IO thread, we create the
  // plugin service as it is predominantly used from the IO thread,
  // but must be created on the main thread. The service ctor is
  // inexpensive and does not invoke the io_thread() accessor.
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::CreateThreads:PluginService");
    PluginService::GetInstance()->Init();
  }
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Prior to any processing happening on the IO thread, we create the
  // CDM service as it is predominantly used from the IO thread. This must
  // be called on the main thread since it involves file path checks.
  CdmRegistry::GetInstance()->Init();
#endif

#if defined(OS_MACOSX)
  // The WindowResizeHelper allows the UI thread to wait on specific renderer
  // and GPU messages from the IO thread. Initializing it before the IO thread
  // starts ensures the affected IO thread messages always have somewhere to go.
  ui::WindowResizeHelperMac::Get()->Init(base::ThreadTaskRunnerHandle::Get());
#endif

  // 1) Need to initialize in-process GpuDataManager before creating threads.
  // It's unsafe to append the gpu command line switches to the global
  // CommandLine::ForCurrentProcess object after threads are created.
  // 2) Must be after parts_->PreCreateThreads to pick up chrome://flags.
  GpuDataManagerImpl::GetInstance();

#if defined(USE_X11)
  gpu_data_manager_visual_proxy_.reset(new internal::GpuDataManagerVisualProxy(
      GpuDataManagerImpl::GetInstance()));
#endif

#if !defined(GOOGLE_CHROME_BUILD) || defined(OS_ANDROID)
  // Single-process is an unsupported and not fully tested mode, so
  // don't enable it for official Chrome builds (except on Android).
  if (parsed_command_line_.HasSwitch(switches::kSingleProcess))
    RenderProcessHost::SetRunRendererInProcess(true);
#endif

  // Initialize origins that are whitelisted for process isolation.  Must be
  // done after base::FeatureList is initialized, but before any navigations
  // can happen.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins(SiteIsolationPolicy::GetIsolatedOrigins());

  // Record metrics about which site isolation flags have been turned on.
  SiteIsolationPolicy::StartRecordingSiteIsolationFlagUsage();

  return result_code_;
}

void BrowserMainLoop::PreShutdown() {
  parts_->PreShutdown();

  ui::Clipboard::OnPreShutdownForCurrentThread();
}

void BrowserMainLoop::CreateStartupTasks() {
  TRACE_EVENT0("startup", "BrowserMainLoop::CreateStartupTasks");

  DCHECK(!startup_task_runner_);
#if defined(OS_ANDROID)
  startup_task_runner_ = std::make_unique<StartupTaskRunner>(
      base::Bind(&BrowserStartupComplete), base::ThreadTaskRunnerHandle::Get());
#else
  startup_task_runner_ = std::make_unique<StartupTaskRunner>(
      base::Callback<void(int)>(), base::ThreadTaskRunnerHandle::Get());
#endif
  StartupTask pre_create_threads =
      base::Bind(&BrowserMainLoop::PreCreateThreads, base::Unretained(this));
  startup_task_runner_->AddTask(pre_create_threads);

  StartupTask create_threads =
      base::Bind(&BrowserMainLoop::CreateThreads, base::Unretained(this));
  startup_task_runner_->AddTask(create_threads);

  StartupTask post_create_threads =
      base::Bind(&BrowserMainLoop::PostCreateThreads, base::Unretained(this));
  startup_task_runner_->AddTask(post_create_threads);

  StartupTask browser_thread_started = base::Bind(
      &BrowserMainLoop::BrowserThreadsStarted, base::Unretained(this));
  startup_task_runner_->AddTask(browser_thread_started);

  StartupTask pre_main_message_loop_run = base::Bind(
      &BrowserMainLoop::PreMainMessageLoopRun, base::Unretained(this));
  startup_task_runner_->AddTask(pre_main_message_loop_run);

#if defined(OS_ANDROID)
  if (parameters_.ui_task) {
    // Running inside browser tests, which relies on synchronous start.
    startup_task_runner_->RunAllTasksNow();
  } else {
    startup_task_runner_->StartRunningTasksAsync();
  }
#else
  startup_task_runner_->RunAllTasksNow();
#endif
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserMainLoop::GetResizeTaskRunner() {
#if defined(OS_MACOSX)
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ui::WindowResizeHelperMac::Get()->task_runner();
  // In tests, WindowResizeHelperMac task runner might not be initialized.
  return task_runner ? task_runner : base::ThreadTaskRunnerHandle::Get();
#else
  return base::ThreadTaskRunnerHandle::Get();
#endif
}

gpu::GpuChannelEstablishFactory*
BrowserMainLoop::gpu_channel_establish_factory() const {
  return BrowserGpuChannelHostFactory::instance();
}

#if defined(OS_ANDROID)
void BrowserMainLoop::SynchronouslyFlushStartupTasks() {
  startup_task_runner_->RunAllTasksNow();
}
#endif  // OS_ANDROID

int BrowserMainLoop::CreateThreads() {
  TRACE_EVENT0("startup,rail", "BrowserMainLoop::CreateThreads");

  {
    auto task_scheduler_init_params =
        GetContentClient()->browser()->GetTaskSchedulerInitParams();
    if (!task_scheduler_init_params)
      task_scheduler_init_params = GetDefaultTaskSchedulerInitParams();
    DCHECK(task_scheduler_init_params);

    // If a renderer lives in the browser process, adjust the number of threads
    // in the foreground pool.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSingleProcess)) {
      const base::SchedulerWorkerPoolParams&
          current_foreground_worker_pool_params(
              task_scheduler_init_params->foreground_worker_pool_params);
      task_scheduler_init_params->foreground_worker_pool_params =
          base::SchedulerWorkerPoolParams(
              std::max(GetMinThreadsInRendererTaskSchedulerForegroundPool(),
                       current_foreground_worker_pool_params.max_threads()),
              current_foreground_worker_pool_params.suggested_reclaim_time(),
              current_foreground_worker_pool_params.backward_compatibility());
    }

    base::TaskScheduler::GetInstance()->Start(
        *task_scheduler_init_params.get());
  }

  TRACE_EVENT_BEGIN1("startup", "BrowserMainLoop::CreateThreads:start",
                     "Thread", "BrowserThread::PROCESS_LAUNCHER");

#if defined(OS_ANDROID)
  // Android specializes Launcher thread so it is accessible in java.
  // Note Android never does clean shutdown, so shutdown use-after-free
  // concerns are not a problem in practice.
  base::MessageLoop* message_loop = android::LauncherThread::GetMessageLoop();
  DCHECK(message_loop);
  // This BrowserThread will use this message loop instead of creating a new
  // thread. Note that means this/ thread will not be joined on shutdown, and
  // may cause use-after-free if anything tries to access objects deleted by
  // AtExitManager, such as non-leaky LazyInstance.
  process_launcher_thread_.reset(new BrowserProcessSubThread(
      BrowserThread::PROCESS_LAUNCHER, message_loop));
#else   // defined(OS_ANDROID)
  // This thread ID will be backed by a SingleThreadTaskRunner using
  // |task_traits|.
  // TODO(gab): WithBaseSyncPrimitives() is likely not required here.
  base::TaskTraits task_traits = {base::MayBlock(),
                                  base::WithBaseSyncPrimitives(),
                                  base::TaskPriority::USER_BLOCKING,
                                  base::TaskShutdownBehavior::BLOCK_SHUTDOWN};
  scoped_refptr<base::SingleThreadTaskRunner> redirection_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits(
          task_traits, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  DCHECK(redirection_task_runner);
  BrowserThreadImpl::RedirectThreadIDToTaskRunner(
      BrowserThread::PROCESS_LAUNCHER, std::move(redirection_task_runner));
#endif  // defined(OS_ANDROID)

  // |io_thread_| is created by |PostMainMessageLoopStart()|, but its
  // full initialization is deferred until this point because it requires
  // several dependencies we don't want to depend on so early in startup.
  DCHECK(io_thread_);
  io_thread_->InitIOThreadDelegate();

  TRACE_EVENT_END0("startup", "BrowserMainLoop::CreateThreads:start");
  created_threads_ = true;
  return result_code_;
}

int BrowserMainLoop::PostCreateThreads() {
  if (parts_) {
    TRACE_EVENT0("startup", "BrowserMainLoop::PostCreateThreads");
    parts_->PostCreateThreads();
  }

  return result_code_;
}

int BrowserMainLoop::PreMainMessageLoopRun() {
#if defined(OS_ANDROID)
  // Let screen instance be overridable by parts.
  ui::SetScreenAndroid();
#endif

  if (parts_) {
    TRACE_EVENT0("startup",
        "BrowserMainLoop::CreateThreads:PreMainMessageLoopRun");

    parts_->PreMainMessageLoopRun();
  }

  // If the UI thread blocks, the whole UI is unresponsive.
  // Do not allow disk IO from the UI thread.
  base::ThreadRestrictions::SetIOAllowed(false);
  base::ThreadRestrictions::DisallowWaiting();
  return result_code_;
}

void BrowserMainLoop::RunMainMessageLoopParts() {
  // Don't use the TRACE_EVENT0 macro because the tracing infrastructure doesn't
  // expect synchronous events around the main loop of a thread.
  TRACE_EVENT_ASYNC_BEGIN0("toplevel", "BrowserMain:MESSAGE_LOOP", this);

  bool ran_main_loop = false;
  if (parts_)
    ran_main_loop = parts_->MainMessageLoopRun(&result_code_);

  if (!ran_main_loop)
    MainMessageLoopRun();

  TRACE_EVENT_ASYNC_END0("toplevel", "BrowserMain:MESSAGE_LOOP", this);
}

void BrowserMainLoop::ShutdownThreadsAndCleanUp() {
  if (!created_threads_) {
    // Called early, nothing to do
    return;
  }
  TRACE_EVENT0("shutdown", "BrowserMainLoop::ShutdownThreadsAndCleanUp");

  // Teardown may start in PostMainMessageLoopRun, and during teardown we
  // need to be able to perform IO.
  base::ThreadRestrictions::SetIOAllowed(true);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&base::ThreadRestrictions::SetIOAllowed), true));

#if defined(OS_ANDROID)
  g_browser_main_loop_shutting_down = true;
#endif

  if (RenderProcessHost::run_renderer_in_process())
    RenderProcessHostImpl::ShutDownInProcessRenderer();

#if BUILDFLAG(ENABLE_MUS)
  // NOTE: because of dependencies this has to happen before
  // PostMainMessageLoopRun().
  image_cursors_set_.reset();
#endif

  if (parts_) {
    TRACE_EVENT0("shutdown",
                 "BrowserMainLoop::Subsystem:PostMainMessageLoopRun");
    parts_->PostMainMessageLoopRun();
  }

  system_stats_monitor_.reset();

  // Cancel pending requests and prevent new requests.
  if (resource_dispatcher_host_) {
    TRACE_EVENT0("shutdown",
                 "BrowserMainLoop::Subsystem:ResourceDispatcherHost");
    resource_dispatcher_host_->Shutdown();
  }
  // Request shutdown to clean up allocated resources on the IO thread.
  if (midi_service_) {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:MidiService");
    midi_service_->Shutdown();
  }

  TRACE_EVENT0("shutdown",
               "BrowserMainLoop::Subsystem:SpeechRecognitionManager");
  io_thread_->task_runner()->DeleteSoon(FROM_HERE,
                                        speech_recognition_manager_.release());

  memory_pressure_monitor_.reset();

#if defined(OS_MACOSX)
  BrowserCompositorMac::DisableRecyclingForShutdown();
#endif

#if defined(USE_AURA) || defined(OS_MACOSX)
  {
    TRACE_EVENT0("shutdown",
                 "BrowserMainLoop::Subsystem:ImageTransportFactory");
    ImageTransportFactory::Terminate();
  }
#endif

#if !defined(OS_ANDROID)
  host_frame_sink_manager_.reset();
  frame_sink_manager_impl_.reset();
  compositing_mode_reporter_impl_.reset();
  forwarding_compositing_mode_reporter_impl_.reset();
#endif

// The device monitors are using |system_monitor_| as dependency, so delete
// them before |system_monitor_| goes away.
// On Mac and windows, the monitor needs to be destroyed on the same thread
// as they were created. On Linux, the monitor will be deleted when IO thread
// goes away.
#if defined(OS_WIN)
  system_message_window_.reset();
#elif defined(OS_MACOSX)
  device_monitor_mac_.reset();
#endif

  if (BrowserGpuChannelHostFactory::instance()) {
    BrowserGpuChannelHostFactory::instance()->CloseChannel();
  }

  // Shutdown the Service Manager and IPC.
  service_manager_context_.reset();
  mojo_ipc_support_.reset();

  if (save_file_manager_)
    save_file_manager_->Shutdown();

  {
    base::ThreadRestrictions::ScopedAllowWait allow_wait_for_join;

    // Must be size_t so we can subtract from it.
    for (size_t thread_id = BrowserThread::ID_COUNT - 1;
         thread_id >= (BrowserThread::UI + 1); --thread_id) {
      // Find the thread object we want to stop. Looping over all valid
      // BrowserThread IDs and DCHECKing on a missing case in the switch
      // statement helps avoid a mismatch between this code and the
      // BrowserThread::ID enumeration.
      //
      // The destruction order is the reverse order of occurrence in the
      // BrowserThread::ID list. The rationale for the order is that he
      // PROCESS_LAUNCHER thread must be stopped after IO in case the IO thread
      // posted a task to terminate a process on the process launcher thread.
      switch (thread_id) {
        case BrowserThread::PROCESS_LAUNCHER: {
          TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:LauncherThread");
#if defined(OS_ANDROID)
          ResetThread_PROCESS_LAUNCHER(std::move(process_launcher_thread_));
#else   // defined(OS_ANDROID)
          ResetThread_PROCESS_LAUNCHER();
#endif  // defined(OS_ANDROID)
          break;
        }
        case BrowserThread::IO: {
          TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:IOThread");
          ResetThread_IO(std::move(io_thread_));
          break;
        }
        case BrowserThread::UI:
        case BrowserThread::ID_COUNT:
          NOTREACHED();
          break;
      }
    }

    {
      TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:TaskScheduler");
      base::TaskScheduler::GetInstance()->Shutdown();
    }
  }

  // Must happen after the IO thread is shutdown since this may be accessed from
  // it.
  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:GPUChannelFactory");
    if (BrowserGpuChannelHostFactory::instance()) {
      BrowserGpuChannelHostFactory::Terminate();
    }
  }

  // Must happen after the I/O thread is shutdown since this class lives on the
  // I/O thread and isn't threadsafe.
  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:GamepadService");
    device::GamepadService::GetInstance()->Terminate();
  }
  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:DeleteDataSources");
    URLDataManager::DeleteDataSources();
  }
  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:AudioMan");
    if (audio_manager_ && !audio_manager_->Shutdown()) {
      // Intentionally leak AudioManager if shutdown failed.
      // We might run into various CHECK(s) in AudioManager destructor.
      ignore_result(audio_manager_.release());
      // |user_input_monitor_| may be in use by stray streams in case
      // AudioManager shutdown failed.
      ignore_result(user_input_monitor_.release());
    }

    // Leaking AudioSystem: we cannot correctly destroy it since Audio service
    // connection in there is bound to IO thread.
    ignore_result(audio_system_.release());
  }

  if (parts_) {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:PostDestroyThreads");
    parts_->PostDestroyThreads();
  }
}

base::SequencedTaskRunner* BrowserMainLoop::audio_service_runner() {
  return audio_service_runner_.get();
}

void BrowserMainLoop::InitializeIOThreadForTesting() {
  InitializeIOThread();
}

#if !defined(OS_ANDROID)
viz::FrameSinkManagerImpl* BrowserMainLoop::GetFrameSinkManager() const {
  return frame_sink_manager_impl_.get();
}
#endif

void BrowserMainLoop::GetCompositingModeReporter(
    viz::mojom::CompositingModeReporterRequest request) {
#if defined(OS_ANDROID)
  // Android doesn't support non-gpu compositing modes, and doesn't make a
  // CompositingModeReporter.
  return;
#else
  if (features::IsMusEnabled()) {
    // Mus == ChromeOS, which doesn't support software compositing, so no need
    // to report compositing mode.
    return;
  }

  if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor))
    forwarding_compositing_mode_reporter_impl_->BindRequest(std::move(request));
  else
    compositing_mode_reporter_impl_->BindRequest(std::move(request));
#endif
}

void BrowserMainLoop::StopStartupTracingTimer() {
  startup_trace_timer_.Stop();
}

void BrowserMainLoop::InitializeMainThread() {
  TRACE_EVENT0("startup", "BrowserMainLoop::InitializeMainThread");
  base::PlatformThread::SetName("CrBrowserMain");

  // Register the main thread by instantiating it, but don't call any methods.
  main_thread_.reset(
      new BrowserThreadImpl(BrowserThread::UI, base::MessageLoop::current()));
}

int BrowserMainLoop::BrowserThreadsStarted() {
  TRACE_EVENT0("startup", "BrowserMainLoop::BrowserThreadsStarted");

  audio_service_runner_ =
      base::MakeRefCounted<base::DeferredSequencedTaskRunner>();

  // Bring up Mojo IPC and the embedded Service Manager as early as possible.
  // Initializaing mojo requires the IO thread to have been initialized first,
  // so this cannot happen any earlier than now.
  InitializeMojo();

#if BUILDFLAG(ENABLE_MUS)
  if (features::IsMusEnabled()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableSurfaceSynchronization);
  }
#endif

  HistogramSynchronizer::GetInstance();
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // Up the priority of the UI thread.
  base::PlatformThread::SetCurrentThreadPriority(base::ThreadPriority::DISPLAY);
#endif

#if BUILDFLAG(ENABLE_VULKAN)
  if (parsed_command_line_.HasSwitch(switches::kEnableVulkan))
    gpu::InitializeVulkan();
#endif

  // Initialize the GPU shader cache. This needs to be initialized before
  // BrowserGpuChannelHostFactory below, since that depends on an initialized
  // ShaderCacheFactory.
  InitShaderCacheFactorySingleton(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));

  // If mus is not hosting viz, then the browser must.
  bool browser_is_viz_host = !base::FeatureList::IsEnabled(::features::kMash);

  bool always_uses_gpu = true;
  bool established_gpu_channel = false;
#if defined(OS_ANDROID)
  // TODO(crbug.com/439322): This should be set to |true|.
  established_gpu_channel = false;
  always_uses_gpu = ShouldStartGpuProcessOnBrowserStartup();
  BrowserGpuChannelHostFactory::Initialize(established_gpu_channel);
#else
  established_gpu_channel = true;
  if (parsed_command_line_.HasSwitch(switches::kDisableGpu) ||
      parsed_command_line_.HasSwitch(switches::kDisableGpuCompositing) ||
      parsed_command_line_.HasSwitch(switches::kDisableGpuEarlyInit) ||
      !browser_is_viz_host) {
    established_gpu_channel = always_uses_gpu = false;
  }

  if (browser_is_viz_host) {
    host_frame_sink_manager_ = std::make_unique<viz::HostFrameSinkManager>();
    BrowserGpuChannelHostFactory::Initialize(established_gpu_channel);
    if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor)) {
      forwarding_compositing_mode_reporter_impl_ =
          std::make_unique<viz::ForwardingCompositingModeReporterImpl>();

      auto transport_factory = std::make_unique<VizProcessTransportFactory>(
          BrowserGpuChannelHostFactory::instance(), GetResizeTaskRunner(),
          forwarding_compositing_mode_reporter_impl_.get());
      transport_factory->ConnectHostFrameSinkManager();
      ImageTransportFactory::SetFactory(std::move(transport_factory));
    } else {
      frame_sink_manager_impl_ = std::make_unique<viz::FrameSinkManagerImpl>(
          switches::GetDeadlineToSynchronizeSurfaces());

      surface_utils::ConnectWithLocalFrameSinkManager(
          host_frame_sink_manager_.get(), frame_sink_manager_impl_.get());

      compositing_mode_reporter_impl_ =
          std::make_unique<viz::CompositingModeReporterImpl>();

      ImageTransportFactory::SetFactory(
          std::make_unique<GpuProcessTransportFactory>(
              BrowserGpuChannelHostFactory::instance(),
              compositing_mode_reporter_impl_.get(), GetResizeTaskRunner()));
    }
  }

#if defined(USE_AURA)
  if (browser_is_viz_host) {
    env_->set_context_factory(GetContextFactory());
    env_->set_context_factory_private(GetContextFactoryPrivate());
  }
#endif  // defined(USE_AURA)
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      tracing::GraphicsMemoryDumpProvider::GetInstance(), "AndroidGraphics",
      nullptr);
#endif

  {
    TRACE_EVENT0("startup", "BrowserThreadsStarted::Subsystem:AudioMan");
    CreateAudioManager();
  }

  {
    TRACE_EVENT0("startup", "BrowserThreadsStarted::Subsystem:MidiService");
    midi_service_.reset(new midi::MidiService);
  }

#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(features::kHighDynamicRange))
    HDRProxy::Initialize();
  system_message_window_.reset(new media::SystemMessageWindowWin);
#elif defined(OS_LINUX) && defined(USE_UDEV)
  device_monitor_linux_.reset(
      new media::DeviceMonitorLinux(io_thread_->task_runner()));
#elif defined(OS_MACOSX)
  device_monitor_mac_.reset(
      new media::DeviceMonitorMac(audio_manager_->GetTaskRunner()));
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
  webrtc_event_log_manager_.reset(
      WebRtcEventLogManager::CreateSingletonInstance());
  webrtc_internals_.reset(WebRTCInternals::CreateSingletonInstance());
#endif

  // RDH needs the IO thread to be created
  {
    TRACE_EVENT0("startup",
      "BrowserMainLoop::BrowserThreadsStarted:InitResourceDispatcherHost");
    // TODO(ananta)
    // We register an interceptor on the ResourceDispatcherHostImpl instance to
    // intercept requests to create handlers for download requests. We need to
    // find a better way to achieve this. Ideally we don't want knowledge of
    // downloads in ResourceDispatcherHostImpl.
    // We pass the task runners for the UI and IO threads as a stopgap approach
    // for now. Eventually variants of these runners would be available in the
    // network service.
    resource_dispatcher_host_.reset(new ResourceDispatcherHostImpl(
        base::Bind(&DownloadResourceHandler::Create),
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
        !parsed_command_line_.HasSwitch(switches::kDisableResourceScheduler)));
    GetContentClient()->browser()->ResourceDispatcherHostCreated();

    loader_delegate_.reset(new LoaderDelegateImpl());
    resource_dispatcher_host_->SetLoaderDelegate(loader_delegate_.get());
  }

  // MediaStreamManager needs the IO thread to be created.
  {
    TRACE_EVENT0("startup",
      "BrowserMainLoop::BrowserThreadsStarted:InitMediaStreamManager");
    media_stream_manager_.reset(new MediaStreamManager(
        audio_system_.get(), audio_manager_->GetTaskRunner()));
  }

  {
    TRACE_EVENT0("startup",
      "BrowserMainLoop::BrowserThreadsStarted:InitSpeechRecognition");
    speech_recognition_manager_.reset(new SpeechRecognitionManagerImpl(
        audio_system_.get(), audio_manager_.get(),
        media_stream_manager_.get()));
  }

  {
    TRACE_EVENT0(
        "startup",
        "BrowserMainLoop::BrowserThreadsStarted::InitUserInputMonitor");
    user_input_monitor_ = media::UserInputMonitor::Create(
        io_thread_->task_runner(), main_thread_->task_runner());
  }

  {
    TRACE_EVENT0("startup",
      "BrowserMainLoop::BrowserThreadsStarted::SaveFileManager");
    save_file_manager_ = new SaveFileManager();
  }

  // Alert the clipboard class to which threads are allowed to access the
  // clipboard:
  std::vector<base::PlatformThreadId> allowed_clipboard_threads;
  // The current thread is the UI thread.
  allowed_clipboard_threads.push_back(base::PlatformThread::CurrentId());
#if defined(OS_WIN)
  // On Windows, clipboard is also used on the IO thread.
  allowed_clipboard_threads.push_back(io_thread_->GetThreadId());
#endif
  ui::Clipboard::SetAllowedThreads(allowed_clipboard_threads);

  if (GpuDataManagerImpl::GetInstance()->GpuAccessAllowed(nullptr) &&
      !established_gpu_channel && always_uses_gpu && browser_is_viz_host) {
    TRACE_EVENT_INSTANT0("gpu", "Post task to launch GPU process",
                         TRACE_EVENT_SCOPE_THREAD);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(base::IgnoreResult(&GpuProcessHost::Get),
                       GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                       true /* force_create */));
  }

#if defined(OS_MACOSX)
  ThemeHelperMac::GetInstance();
  SystemHotkeyHelperMac::GetInstance()->DeferredLoadSystemHotkeys();
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID)
  media::SetMediaDrmBridgeClient(GetContentClient()->GetMediaDrmBridgeClient());
#endif

  return result_code_;
}

bool BrowserMainLoop::UsingInProcessGpu() const {
  return parsed_command_line_.HasSwitch(switches::kSingleProcess) ||
         parsed_command_line_.HasSwitch(switches::kInProcessGPU);
}

void BrowserMainLoop::InitializeMemoryManagementComponent() {
  // TODO(chrisha): Abstract away this construction mess to a helper function,
  // once MemoryPressureMonitor is made a concrete class.
#if defined(OS_CHROMEOS)
  if (chromeos::switches::MemoryPressureHandlingEnabled()) {
    memory_pressure_monitor_ =
        std::make_unique<base::chromeos::MemoryPressureMonitor>(
            chromeos::switches::GetMemoryPressureThresholds());
  }
#elif defined(OS_MACOSX)
  memory_pressure_monitor_ =
      std::make_unique<base::mac::MemoryPressureMonitor>();
#elif defined(OS_WIN)
  memory_pressure_monitor_ =
      CreateWinMemoryPressureMonitor(parsed_command_line_);
#endif

  if (base::FeatureList::IsEnabled(features::kMemoryCoordinator))
    MemoryCoordinatorImpl::GetInstance()->Start();

  std::unique_ptr<SwapMetricsDriver::Delegate> delegate(
      base::WrapUnique<SwapMetricsDriver::Delegate>(
          new SwapMetricsDelegateUma()));

#if !defined(OS_FUCHSIA)
  swap_metrics_driver_ =
      SwapMetricsDriver::Create(std::move(delegate), kSwapMetricsInterval);
  if (swap_metrics_driver_)
    swap_metrics_driver_->Start();
#endif  // !defined(OS_FUCHSIA)
}

bool BrowserMainLoop::InitializeToolkit() {
  TRACE_EVENT0("startup", "BrowserMainLoop::InitializeToolkit");

  // TODO(evan): this function is rather subtle, due to the variety
  // of intersecting ifdefs we have.  To keep it easy to follow, there
  // are no #else branches on any #ifs.
  // TODO(stevenjb): Move platform specific code into platform specific Parts
  // (Need to add InitializeToolkit stage to BrowserParts).
  // See also GTK setup in EarlyInitialization, above, and associated comments.

#if defined(OS_WIN)
  INITCOMMONCONTROLSEX config;
  config.dwSize = sizeof(config);
  config.dwICC = ICC_WIN95_CLASSES;
  if (!InitCommonControlsEx(&config))
    PLOG(FATAL);
#endif

#if defined(USE_AURA)

#if defined(USE_X11)
  if (!parsed_command_line_.HasSwitch(switches::kHeadless) &&
      !gfx::GetXDisplay()) {
    LOG(ERROR) << "Unable to open X display.";
    return false;
  }
#endif

  // Env creates the compositor. Aura widgets need the compositor to be created
  // before they can be initialized by the browser.
  env_ = aura::Env::CreateInstance(
      features::IsMusEnabled() ? aura::Env::Mode::MUS : aura::Env::Mode::LOCAL);
#endif  // defined(USE_AURA)

#if BUILDFLAG(ENABLE_MUS)
  if (features::IsMusEnabled())
    image_cursors_set_ = std::make_unique<ui::ImageCursorsSet>();
#endif

  if (parts_)
    parts_->ToolkitInitialized();

  return true;
}

void BrowserMainLoop::MainMessageLoopRun() {
#if defined(OS_ANDROID)
  // Android's main message loop is the Java message loop.
  NOTREACHED();
#else
  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (parameters_.ui_task) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  *parameters_.ui_task);
  }

  base::RunLoop run_loop;
  run_loop.Run();
#endif
}

void BrowserMainLoop::InitializeIOThread() {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // Up the priority of the |io_thread_| as some of its IPCs relate to
  // display tasks.
  options.priority = base::ThreadPriority::DISPLAY;
#endif

  io_thread_.reset(new BrowserProcessSubThread(BrowserThread::IO));
  if (!io_thread_->StartWithOptions(options))
    LOG(FATAL) << "Failed to start the browser thread: IO";
}

void BrowserMainLoop::InitializeMojo() {
  if (!parsed_command_line_.HasSwitch(switches::kSingleProcess)) {
    // Disallow mojo sync calls in the browser process. Note that we allow sync
    // calls in single-process mode since renderer IPCs are made from a browser
    // thread.
    bool sync_call_allowed = false;
    MojoResult result = mojo::edk::SetProperty(
        MOJO_PROPERTY_TYPE_SYNC_CALL_ALLOWED, &sync_call_allowed);
    DCHECK_EQ(MOJO_RESULT_OK, result);
  }

  mojo_ipc_support_.reset(new mojo::edk::ScopedIPCSupport(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::FAST));

  service_manager_context_.reset(new ServiceManagerContext);
#if defined(OS_MACOSX)
  mojo::edk::SetMachPortProvider(MachBroker::GetInstance());
#endif  // defined(OS_MACOSX)
  GetContentClient()->OnServiceManagerConnected(
      ServiceManagerConnection::GetForProcess());

  tracing_controller_ = std::make_unique<content::TracingControllerImpl>();
  content::BackgroundTracingManagerImpl::GetInstance()
      ->AddMetadataGeneratorFunction();

  // Registers the browser process as a memory-instrumentation client, so
  // that data for the browser process will be available in memory dumps.
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  memory_instrumentation::ClientProcessImpl::Config config(
      connector, resource_coordinator::mojom::kServiceName,
      memory_instrumentation::mojom::ProcessType::BROWSER);
  memory_instrumentation::ClientProcessImpl::CreateInstance(config);

  // Start startup tracing through TracingController's interface. TraceLog has
  // been enabled in content_main_runner where threads are not available. Now We
  // need to start tracing for all other tracing agents, which require threads.
  if (parsed_command_line_.HasSwitch(switches::kTraceStartup)) {
    base::trace_event::TraceConfig trace_config(
        parsed_command_line_.GetSwitchValueASCII(switches::kTraceStartup),
        base::trace_event::RECORD_UNTIL_FULL);
    TracingController::GetInstance()->StartTracing(
        trace_config, TracingController::StartTracingDoneCallback());
  } else if (parsed_command_line_.HasSwitch(switches::kTraceToConsole)) {
    TracingController::GetInstance()->StartTracing(
        tracing::GetConfigForTraceToConsole(),
        TracingController::StartTracingDoneCallback());
  } else if (tracing::TraceConfigFile::GetInstance()->IsEnabled()) {
    // This checks kTraceConfigFile switch.
    TracingController::GetInstance()->StartTracing(
        tracing::TraceConfigFile::GetInstance()->GetTraceConfig(),
        TracingController::StartTracingDoneCallback());
  }
  // Start tracing to a file for certain duration if needed. Only do this after
  // starting the main message loop to avoid calling
  // MessagePumpForUI::ScheduleWork() before MessagePumpForUI::Start() as it
  // will crash the browser.
  if (is_tracing_startup_for_duration_) {
    TRACE_EVENT0("startup", "BrowserMainLoop::InitStartupTracingForDuration");
    InitStartupTracingForDuration(parsed_command_line_);
  }

  if (parts_) {
    parts_->ServiceManagerConnectionStarted(
        ServiceManagerConnection::GetForProcess());
  }
}

base::FilePath BrowserMainLoop::GetStartupTraceFileName(
    const base::CommandLine& command_line) const {
  base::FilePath trace_file;
  if (command_line.HasSwitch(switches::kTraceStartup)) {
    trace_file = command_line.GetSwitchValuePath(
        switches::kTraceStartupFile);
    // trace_file = "none" means that startup events will show up for the next
    // begin/end tracing (via about:tracing or AutomationProxy::BeginTracing/
    // EndTracing, for example).
    if (trace_file == base::FilePath().AppendASCII("none"))
      return trace_file;

    if (trace_file.empty()) {
#if defined(OS_ANDROID)
      TracingControllerAndroid::GenerateTracingFilePath(&trace_file);
#else
      // Default to saving the startup trace into the current dir.
      trace_file = base::FilePath().AppendASCII("chrometrace.log");
#endif
    }
  } else {
#if defined(OS_ANDROID)
    TracingControllerAndroid::GenerateTracingFilePath(&trace_file);
#else
    trace_file = tracing::TraceConfigFile::GetInstance()->GetResultFile();
#endif
  }

  return trace_file;
}

void BrowserMainLoop::InitStartupTracingForDuration(
    const base::CommandLine& command_line) {
  DCHECK(is_tracing_startup_for_duration_);

  startup_trace_file_ = GetStartupTraceFileName(parsed_command_line_);

  int delay_secs = 5;
  if (command_line.HasSwitch(switches::kTraceStartup)) {
    std::string delay_str = command_line.GetSwitchValueASCII(
        switches::kTraceStartupDuration);
    if (!delay_str.empty() && !base::StringToInt(delay_str, &delay_secs)) {
      DLOG(WARNING) << "Could not parse --" << switches::kTraceStartupDuration
          << "=" << delay_str << " defaulting to 5 (secs)";
      delay_secs = 5;
    }
  } else {
    delay_secs = tracing::TraceConfigFile::GetInstance()->GetStartupDuration();
  }

  startup_trace_timer_.Start(FROM_HERE,
                             base::TimeDelta::FromSeconds(delay_secs),
                             this,
                             &BrowserMainLoop::EndStartupTracing);
}

void BrowserMainLoop::EndStartupTracing() {
  DCHECK(is_tracing_startup_for_duration_);

  is_tracing_startup_for_duration_ = false;
  TracingController::GetInstance()->StopTracing(
      TracingController::CreateFileEndpoint(
          startup_trace_file_,
          base::Bind(OnStoppedStartupTracing, startup_trace_file_)));
}

void BrowserMainLoop::CreateAudioManager() {
  DCHECK(!audio_manager_);

  audio_manager_ = GetContentClient()->browser()->CreateAudioManager(
      MediaInternals::GetInstance());
  if (!audio_manager_) {
    audio_manager_ =
        media::AudioManager::Create(std::make_unique<media::AudioThreadImpl>(),
                                    MediaInternals::GetInstance());
  }
  CHECK(audio_manager_);

  TRACE_EVENT_INSTANT0("startup", "Starting Audio service task runner",
                       TRACE_EVENT_SCOPE_THREAD);
  audio_service_runner_->StartWithTaskRunner(audio_manager_->GetTaskRunner());

  audio_system_ = audio::CreateAudioSystem(
      content::ServiceManagerConnection::GetForProcess()
          ->GetConnector()
          ->Clone());
  CHECK(audio_system_);
}

}  // namespace content
