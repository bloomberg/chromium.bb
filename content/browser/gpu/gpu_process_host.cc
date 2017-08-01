// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <utility>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/sha1.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/field_trial_recorder.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_main_thread_factory.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/service_manager/child_connection.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_type.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/ipc/host/shader_disk_cache.h"
#include "gpu/ipc/service/switches.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/message_filter.h"
#include "media/base/media_switches.h"
#include "media/media_features.h"
#include "mojo/edk/embedder/embedder.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/runner/common/client_util.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/color_space_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/latency/latency_info.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "content/public/browser/android/java_interfaces.h"
#include "media/mojo/interfaces/android_overlay.mojom.h"
#endif

#if defined(OS_WIN)
#include "content/common/sandbox_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/win/rendering_window_manager.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#endif

#if defined(USE_X11) && !defined(OS_CHROMEOS)
#include "ui/gfx/x/x11_switches.h"  // nogncheck
#endif

#if defined(OS_MACOSX) || defined(OS_ANDROID)
#include "gpu/ipc/common/gpu_surface_tracker.h"
#endif

#if defined(OS_MACOSX)
#include "ui/base/ui_base_switches.h"
#endif

namespace content {

bool GpuProcessHost::gpu_enabled_ = true;
bool GpuProcessHost::hardware_gpu_enabled_ = true;
int GpuProcessHost::gpu_crash_count_ = 0;
int GpuProcessHost::gpu_recent_crash_count_ = 0;
bool GpuProcessHost::crashed_before_ = false;
int GpuProcessHost::swiftshader_crash_count_ = 0;

namespace {

// Command-line switches to propagate to the GPU process.
static const char* const kSwitchNames[] = {
    switches::kCreateDefaultGLContext,
    switches::kDisableAcceleratedVideoDecode,
    switches::kDisableBreakpad,
    switches::kDisableES3GLContext,
    switches::kDisableGpuRasterization,
    switches::kDisableGpuSandbox,
    switches::kDisableGpuWatchdog,
    switches::kDisableGLExtensions,
    switches::kDisableLogging,
    switches::kDisableSeccompFilterSandbox,
    switches::kDisableShaderNameHashing,
#if BUILDFLAG(ENABLE_WEBRTC)
    switches::kDisableWebRtcHWEncoding,
#endif
#if defined(OS_WIN)
    switches::kEnableAcceleratedVpxDecode,
#endif
    switches::kEnableGpuRasterization,
    switches::kEnableHeapProfiling,
    switches::kEnableLogging,
#if defined(OS_CHROMEOS)
    switches::kDisableVaapiAcceleratedVideoEncode,
#endif
    switches::kGpuDriverBugWorkarounds,
    switches::kGpuStartupDialog,
    switches::kGpuSandboxAllowSysVShm,
    switches::kGpuSandboxFailuresFatal,
    switches::kGpuSandboxStartEarly,
    switches::kHeadless,
    switches::kLoggingLevel,
    switches::kEnableLowEndDeviceMode,
    switches::kDisableLowEndDeviceMode,
    switches::kNoSandbox,
    switches::kProfilerTiming,
    switches::kTestGLLib,
    switches::kTraceConfigFile,
    switches::kTraceStartup,
    switches::kTraceToConsole,
    switches::kUseFakeJpegDecodeAccelerator,
    switches::kUseGpuInTests,
    switches::kV,
    switches::kVModule,
#if defined(OS_MACOSX)
    switches::kDisableAVFoundationOverlays,
    switches::kDisableRemoteCoreAnimation,
    switches::kEnableSandboxLogging,
    switches::kShowMacOverlayBorders,
#endif
#if defined(USE_OZONE)
    switches::kOzonePlatform,
#endif
#if defined(USE_X11) && !defined(OS_CHROMEOS)
    switches::kX11Display,
#endif
    switches::kGpuTestingGLVendor,
    switches::kGpuTestingGLRenderer,
    switches::kGpuTestingGLVersion,
    switches::kDisableGpuDriverBugWorkarounds,
    switches::kUsePassthroughCmdDecoder,
    switches::kIgnoreGpuBlacklist};

enum GPUProcessLifetimeEvent {
  LAUNCHED,
  DIED_FIRST_TIME,
  DIED_SECOND_TIME,
  DIED_THIRD_TIME,
  DIED_FOURTH_TIME,
  GPU_PROCESS_LIFETIME_EVENT_MAX = 100
};

// Indexed by GpuProcessKind. There is one of each kind maximum. This array may
// only be accessed from the IO thread.
GpuProcessHost* g_gpu_process_hosts[GpuProcessHost::GPU_PROCESS_KIND_COUNT];

void RunCallbackOnIO(GpuProcessHost::GpuProcessKind kind,
                     bool force_create,
                     const base::Callback<void(GpuProcessHost*)>& callback) {
  GpuProcessHost* host = GpuProcessHost::Get(kind, force_create);
  callback.Run(host);
}

#if defined(USE_OZONE)
// The ozone platform use this callback to send IPC messages to the gpu process.
void SendGpuProcessMessage(base::WeakPtr<GpuProcessHost> host,
                           IPC::Message* message) {
  if (host)
    host->Send(message);
  else
    delete message;
}

void RouteMessageToOzoneOnUI(const IPC::Message& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnMessageReceived(message);
}

#endif  // defined(USE_OZONE)

void OnGpuProcessHostDestroyedOnUI(int host_id, const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GpuDataManagerImpl::GetInstance()->AddLogMessage(
      logging::LOG_ERROR, "GpuProcessHostUIShim", message);
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnChannelDestroyed(host_id);
#endif
}

// NOTE: changes to this class need to be reviewed by the security team.
class GpuSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  explicit GpuSandboxedProcessLauncherDelegate(
      const base::CommandLine& cmd_line)
#if defined(OS_WIN)
      : cmd_line_(cmd_line)
#endif
  {
  }

  ~GpuSandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool DisableDefaultPolicy() override {
    return true;
  }

  // For the GPU process we gotten as far as USER_LIMITED. The next level
  // which is USER_RESTRICTED breaks both the DirectX backend and the OpenGL
  // backend. Note that the GPU process is connected to the interactive
  // desktop.
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    if (cmd_line_.GetSwitchValueASCII(switches::kUseGL) ==
        gl::kGLImplementationDesktopName) {
      // Open GL path.
      policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                            sandbox::USER_LIMITED);
      SetJobLevel(cmd_line_, sandbox::JOB_UNPROTECTED, 0, policy);
      policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    } else {
      policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                            sandbox::USER_LIMITED);

      // UI restrictions break when we access Windows from outside our job.
      // However, we don't want a proxy window in this process because it can
      // introduce deadlocks where the renderer blocks on the gpu, which in
      // turn blocks on the browser UI thread. So, instead we forgo a window
      // message pump entirely and just add job restrictions to prevent child
      // processes.
      SetJobLevel(cmd_line_, sandbox::JOB_LIMITED_USER,
                  JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
                      JOB_OBJECT_UILIMIT_DESKTOP |
                      JOB_OBJECT_UILIMIT_EXITWINDOWS |
                      JOB_OBJECT_UILIMIT_DISPLAYSETTINGS,
                  policy);

      policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    }

    // Allow the server side of GPU sockets, which are pipes that have
    // the "chrome.gpu" namespace and an arbitrary suffix.
    sandbox::ResultCode result = policy->AddRule(
        sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
        sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
        L"\\\\.\\pipe\\chrome.gpu.*");
    if (result != sandbox::SBOX_ALL_OK)
      return false;

    // Block this DLL even if it is not loaded by the browser process.
    policy->AddDllToUnload(L"cmsetac.dll");

    if (cmd_line_.HasSwitch(switches::kEnableLogging)) {
      base::string16 log_file_path = logging::GetLogFileFullPath();
      if (!log_file_path.empty()) {
        result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                                 sandbox::TargetPolicy::FILES_ALLOW_ANY,
                                 log_file_path.c_str());
        if (result != sandbox::SBOX_ALL_OK)
          return false;
      }
    }

    return true;
  }
#endif  // OS_WIN

  SandboxType GetSandboxType() override {
#if defined(OS_WIN)
    if (cmd_line_.HasSwitch(switches::kDisableGpuSandbox)) {
      DVLOG(1) << "GPU sandbox is disabled";
      return SANDBOX_TYPE_NO_SANDBOX;
    }
#endif
    return SANDBOX_TYPE_GPU;
  }

 private:
#if defined(OS_WIN)
  base::CommandLine cmd_line_;
#endif  // OS_WIN
};

#if defined(OS_ANDROID)
template <typename Interface>
void BindJavaInterface(mojo::InterfaceRequest<Interface> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetGlobalJavaInterfaces()->GetInterface(std::move(request));
}
#endif  // defined(OS_ANDROID)

}  // anonymous namespace

class GpuProcessHost::ConnectionFilterImpl : public ConnectionFilter {
 public:
  ConnectionFilterImpl() {
    auto task_runner = BrowserThread::GetTaskRunnerForThread(BrowserThread::UI);
    registry_.AddInterface(base::Bind(&FieldTrialRecorder::Create),
                           task_runner);
#if defined(OS_ANDROID)
    registry_.AddInterface(
        base::Bind(&BindJavaInterface<media::mojom::AndroidOverlayProvider>),
        task_runner);
#endif
  }

 private:
  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    if (!registry_.TryBindInterface(interface_name, interface_pipe)) {
      GetContentClient()->browser()->BindInterfaceRequest(
          source_info, interface_name, interface_pipe);
    }
  }

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionFilterImpl);
};

// static
bool GpuProcessHost::ValidateHost(GpuProcessHost* host) {
  // The Gpu process is invalid if it's not using SwiftShader, the card is
  // blacklisted, and we can kill it and start over.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU) ||
      (host->valid_ &&
       (host->swiftshader_rendering_ ||
        !GpuDataManagerImpl::GetInstance()->ShouldUseSwiftShader()))) {
    return true;
  }

  host->ForceShutdown();
  return false;
}

// static
GpuProcessHost* GpuProcessHost::Get(GpuProcessKind kind, bool force_create) {
  DCHECK(!service_manager::ServiceManagerIsRemote());
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Don't grant further access to GPU if it is not allowed.
  GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  DCHECK(gpu_data_manager);
  if (!gpu_data_manager->GpuAccessAllowed(NULL))
    return NULL;

  if (g_gpu_process_hosts[kind] && ValidateHost(g_gpu_process_hosts[kind]))
    return g_gpu_process_hosts[kind];

  if (!force_create)
    return nullptr;

  // Do not create a new process if browser is shutting down.
  if (BrowserMainRunner::ExitedMainMessageLoop())
    return nullptr;

  static int last_host_id = 0;
  int host_id;
  host_id = ++last_host_id;

  GpuProcessHost* host = new GpuProcessHost(host_id, kind);
  if (host->Init())
    return host;

  // TODO(sievers): Revisit this behavior. It's not really a crash, but we also
  // want the fallback-to-sw behavior if we cannot initialize the GPU.
  host->RecordProcessCrash();

  delete host;
  return NULL;
}

// static
void GpuProcessHost::GetHasGpuProcess(
    const base::Callback<void(bool)>& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&GpuProcessHost::GetHasGpuProcess, callback));
    return;
  }
  bool has_gpu = false;
  for (size_t i = 0; i < arraysize(g_gpu_process_hosts); ++i) {
    GpuProcessHost* host = g_gpu_process_hosts[i];
    if (host && ValidateHost(host)) {
      has_gpu = true;
      break;
    }
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, has_gpu));
}

// static
void GpuProcessHost::CallOnIO(
    GpuProcessKind kind,
    bool force_create,
    const base::Callback<void(GpuProcessHost*)>& callback) {
#if !defined(OS_WIN)
  DCHECK_NE(kind, GpuProcessHost::GPU_PROCESS_KIND_UNSANDBOXED);
#endif
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RunCallbackOnIO, kind, force_create, callback));
}

void GpuProcessHost::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  process_->child_connection()->BindInterface(interface_name,
                                              std::move(interface_pipe));
}

// static
GpuProcessHost* GpuProcessHost::FromID(int host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (int i = 0; i < GPU_PROCESS_KIND_COUNT; ++i) {
    GpuProcessHost* host = g_gpu_process_hosts[i];
    if (host && host->host_id_ == host_id && ValidateHost(host))
      return host;
  }

  return NULL;
}

GpuProcessHost::GpuProcessHost(int host_id, GpuProcessKind kind)
    : host_id_(host_id),
      valid_(true),
      in_process_(false),
      swiftshader_rendering_(false),
      kind_(kind),
      process_launched_(false),
      status_(UNKNOWN),
      gpu_host_binding_(this),
      weak_ptr_factory_(this) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU)) {
    in_process_ = true;
  }

  // If the 'single GPU process' policy ever changes, we still want to maintain
  // it for 'gpu thread' mode and only create one instance of host and thread.
  DCHECK(!in_process_ || g_gpu_process_hosts[kind] == NULL);

  g_gpu_process_hosts[kind] = this;

  process_.reset(new BrowserChildProcessHostImpl(
      PROCESS_TYPE_GPU, this, mojom::kGpuServiceName));
}

GpuProcessHost::~GpuProcessHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SendOutstandingReplies();

  if (status_ == UNKNOWN) {
    RunRequestGPUInfoCallbacks(gpu::GPUInfo());
  } else {
    DCHECK(request_gpu_info_callbacks_.empty());
  }

  // In case we never started, clean up.
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  // This is only called on the IO thread so no race against the constructor
  // for another GpuProcessHost.
  if (g_gpu_process_hosts[kind_] == this)
    g_gpu_process_hosts[kind_] = NULL;

#if defined(OS_MACOSX) || defined(OS_ANDROID)
  UMA_HISTOGRAM_COUNTS_100("GPU.AtExitSurfaceCount",
                           gpu::GpuSurfaceTracker::Get()->GetSurfaceCount());
#endif

  std::string message;
  bool block_offscreen_contexts = true;
  if (!in_process_) {
    int exit_code;
    base::TerminationStatus status = process_->GetTerminationStatus(
        false /* known_dead */, &exit_code);
    UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessTerminationStatus",
                              status,
                              base::TERMINATION_STATUS_MAX_ENUM);

    if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
        status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
        status == base::TERMINATION_STATUS_PROCESS_CRASHED) {
      // Windows always returns PROCESS_CRASHED on abnormal termination, as it
      // doesn't have a way to distinguish the two.
      UMA_HISTOGRAM_SPARSE_SLOWLY("GPU.GPUProcessExitCode",
                                  std::max(0, std::min(100, exit_code)));
    }

    switch (status) {
      case base::TERMINATION_STATUS_NORMAL_TERMINATION:
        // Don't block offscreen contexts (and force page reload for webgl)
        // if this was an intentional shutdown or the OOM killer on Android
        // killed us while Chrome was in the background.
// TODO(crbug.com/598400): Restrict this to Android for now, since other
// platforms might fall through here for the 'exit_on_context_lost' workaround.
#if defined(OS_ANDROID)
        block_offscreen_contexts = false;
#endif
        message = "The GPU process exited normally. Everything is okay.";
        break;
      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
        message = base::StringPrintf(
            "The GPU process exited with code %d.",
            exit_code);
        break;
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
        message = "You killed the GPU process! Why?";
        break;
#if defined(OS_CHROMEOS)
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
        message = "The GUP process was killed due to out of memory.";
        break;
#endif
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
        message = "The GPU process crashed!";
        break;
      case base::TERMINATION_STATUS_LAUNCH_FAILED:
        message = "The GPU process failed to start!";
        break;
      default:
        break;
    }
  }

  // If there are any remaining offscreen contexts at the point the
  // GPU process exits, assume something went wrong, and block their
  // URLs from accessing client 3D APIs without prompting.
  if (block_offscreen_contexts)
    BlockLiveOffscreenContexts();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnGpuProcessHostDestroyedOnUI, host_id_, message));
}

bool GpuProcessHost::Init() {
  init_start_time_ = base::TimeTicks::Now();

  TRACE_EVENT_INSTANT0("gpu", "LaunchGpuProcess", TRACE_EVENT_SCOPE_THREAD);

  // May be null during test execution.
  if (ServiceManagerConnection::GetForProcess()) {
    ServiceManagerConnection::GetForProcess()->AddConnectionFilter(
        base::MakeUnique<ConnectionFilterImpl>());
  }

  process_->GetHost()->CreateChannelMojo();

  gpu::GpuPreferences gpu_preferences = GetGpuPreferencesFromCommandLine();
  GpuDataManagerImpl::GetInstance()->UpdateGpuPreferences(&gpu_preferences);
  if (in_process_) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(GetGpuMainThreadFactory());
    in_process_gpu_thread_.reset(
        GetGpuMainThreadFactory()(InProcessChildThreadParams(
            base::ThreadTaskRunnerHandle::Get(),
            process_->GetInProcessBrokerClientInvitation(),
            process_->child_connection()->service_token())));
    base::Thread::Options options;
#if defined(OS_WIN)
    // WGL needs to create its own window and pump messages on it.
    options.message_loop_type = base::MessageLoop::TYPE_UI;
#endif
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    options.priority = base::ThreadPriority::DISPLAY;
#endif
    in_process_gpu_thread_->StartWithOptions(options);

    OnProcessLaunched();  // Fake a callback that the process is ready.
  } else if (!LaunchGpuProcess()) {
    return false;
  }

  process_->child_channel()
      ->GetAssociatedInterfaceSupport()
      ->GetRemoteAssociatedInterface(&gpu_main_ptr_);
  ui::mojom::GpuHostPtr host_proxy;
  gpu_host_binding_.Bind(mojo::MakeRequest(&host_proxy));
  gpu_main_ptr_->CreateGpuService(mojo::MakeRequest(&gpu_service_ptr_),
                                  std::move(host_proxy), gpu_preferences,
                                  activity_flags_.CloneHandle());

#if defined(USE_OZONE)
  // Ozone needs to send the primary DRM device to GPU process as early as
  // possible to ensure the latter always has a valid device. crbug.com/608839
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnGpuProcessLaunched(
          host_id_, BrowserThread::GetTaskRunnerForThread(BrowserThread::UI),
          base::ThreadTaskRunnerHandle::Get(),
          base::Bind(&SendGpuProcessMessage, weak_ptr_factory_.GetWeakPtr()));
#endif

  return true;
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_->GetHost()->IsChannelOpening()) {
    queued_messages_.push(msg);
    return true;
  }

  bool result = process_->Send(msg);
  if (!result) {
    // Channel is hosed, but we may not get destroyed for a while. Send
    // outstanding channel creation failures now so that the caller can restart
    // with a new process/channel without waiting.
    SendOutstandingReplies();
  }
  return result;
}

bool GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(USE_OZONE)
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&RouteMessageToOzoneOnUI, message));
#endif
  return true;
}

void GpuProcessHost::OnChannelConnected(int32_t peer_pid) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnChannelConnected");

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void GpuProcessHost::EstablishGpuChannel(
    int client_id,
    uint64_t client_tracing_id,
    bool preempts,
    bool allow_view_command_buffers,
    bool allow_real_time_streams,
    const EstablishChannelCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuProcessHost::EstablishGpuChannel");

  // If GPU features are already blacklisted, no need to establish the channel.
  if (!GpuDataManagerImpl::GetInstance()->GpuAccessAllowed(NULL)) {
    DVLOG(1) << "GPU blacklisted, refusing to open a GPU channel.";
    callback.Run(IPC::ChannelHandle(), gpu::GPUInfo(),
                 EstablishChannelStatus::GPU_ACCESS_DENIED);
    return;
  }

  DCHECK_EQ(preempts, allow_view_command_buffers);
  DCHECK_EQ(preempts, allow_real_time_streams);
  bool is_gpu_host = preempts;

  channel_requests_.push(callback);
  gpu_service_ptr_->EstablishGpuChannel(
      client_id, client_tracing_id, is_gpu_host,
      base::Bind(&GpuProcessHost::OnChannelEstablished,
                 weak_ptr_factory_.GetWeakPtr(), client_id, callback));

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuShaderDiskCache)) {
    CreateChannelCache(client_id);
  }
}

void GpuProcessHost::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gpu::SurfaceHandle surface_handle,
    const CreateGpuMemoryBufferCallback& callback) {
  TRACE_EVENT0("gpu", "GpuProcessHost::CreateGpuMemoryBuffer");

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  create_gpu_memory_buffer_requests_.push(callback);
  gpu_service_ptr_->CreateGpuMemoryBuffer(
      id, size, format, usage, client_id, surface_handle,
      base::Bind(&GpuProcessHost::OnGpuMemoryBufferCreated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void GpuProcessHost::DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                            int client_id,
                                            const gpu::SyncToken& sync_token) {
  TRACE_EVENT0("gpu", "GpuProcessHost::DestroyGpuMemoryBuffer");
  gpu_service_ptr_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

void GpuProcessHost::RequestGPUInfo(RequestGPUInfoCallback request_cb) {
  if (status_ == SUCCESS || status_ == FAILURE) {
    std::move(request_cb).Run(GpuDataManagerImpl::GetInstance()->GetGPUInfo());
    return;
  }

  request_gpu_info_callbacks_.push_back(std::move(request_cb));
}

#if defined(OS_ANDROID)
void GpuProcessHost::SendDestroyingVideoSurface(int surface_id,
                                                const base::Closure& done_cb) {
  TRACE_EVENT0("gpu", "GpuProcessHost::SendDestroyingVideoSurface");
  DCHECK(send_destroying_video_surface_done_cb_.is_null());
  DCHECK(!done_cb.is_null());
  send_destroying_video_surface_done_cb_ = done_cb;
  gpu_service_ptr_->DestroyingVideoSurface(
      surface_id, base::Bind(&GpuProcessHost::OnDestroyingVideoSurfaceAck,
                             weak_ptr_factory_.GetWeakPtr()));
}
#endif

void GpuProcessHost::OnChannelEstablished(
    int client_id,
    const EstablishChannelCallback& callback,
    mojo::ScopedMessagePipeHandle channel_handle) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnChannelEstablished");
  DCHECK(!channel_requests_.empty());
  DCHECK(channel_requests_.front().Equals(callback));
  channel_requests_.pop();

  auto* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  // Currently if any of the GPU features are blacklisted, we don't establish a
  // GPU channel.
  if (channel_handle.is_valid() &&
      !gpu_data_manager->GpuAccessAllowed(nullptr)) {
    gpu_service_ptr_->CloseChannel(client_id);
    callback.Run(IPC::ChannelHandle(), gpu::GPUInfo(),
                 EstablishChannelStatus::GPU_ACCESS_DENIED);
    RecordLogMessage(logging::LOG_WARNING, "WARNING",
                     "Hardware acceleration is unavailable.");
    return;
  }

  callback.Run(IPC::ChannelHandle(channel_handle.release()),
               gpu_data_manager->GetGPUInfo(), EstablishChannelStatus::SUCCESS);
}

void GpuProcessHost::OnGpuMemoryBufferCreated(
    const gfx::GpuMemoryBufferHandle& handle) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnGpuMemoryBufferCreated");

  DCHECK(!create_gpu_memory_buffer_requests_.empty());
  auto callback = create_gpu_memory_buffer_requests_.front();
  create_gpu_memory_buffer_requests_.pop();
  callback.Run(handle, BufferCreationStatus::SUCCESS);
}

#if defined(OS_ANDROID)
void GpuProcessHost::OnDestroyingVideoSurfaceAck() {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnDestroyingVideoSurfaceAck");
  if (!send_destroying_video_surface_done_cb_.is_null())
    base::ResetAndReturn(&send_destroying_video_surface_done_cb_).Run();
}
#endif

void GpuProcessHost::OnProcessLaunched() {
  UMA_HISTOGRAM_TIMES("GPU.GPUProcessLaunchTime",
                      base::TimeTicks::Now() - init_start_time_);
}

void GpuProcessHost::OnProcessLaunchFailed(int error_code) {
  // TODO(wfh): do something more useful with this error code.
  RecordProcessCrash();
}

void GpuProcessHost::OnProcessCrashed(int exit_code) {
  // If the GPU process crashed while compiling a shader, we may have invalid
  // cached binaries. Completely clear the shader cache to force shader binaries
  // to be re-created.
  if (activity_flags_.IsFlagSet(
          gpu::ActivityFlagsBase::FLAG_LOADING_PROGRAM_BINARY)) {
    for (auto cache_key : client_id_to_shader_cache_) {
      // This call will temporarily extend the lifetime of the cache (kept
      // alive in the factory), and may drop loads of cached shader binaries if
      // it takes a while to complete. As we are intentionally dropping all
      // binaries, this behavior is fine.
      GetShaderCacheFactorySingleton()->ClearByClientId(
          cache_key.first, base::Time(), base::Time::Max(), base::Bind([] {}));
    }
  }
  SendOutstandingReplies();
  RecordProcessCrash();
  GpuDataManagerImpl::GetInstance()->ProcessCrashed(
      process_->GetTerminationStatus(true /* known_dead */, NULL));
}

void GpuProcessHost::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessInitialized", true);
  status_ = SUCCESS;
  GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  if (!gpu_data_manager->ShouldUseSwiftShader()) {
    gpu_data_manager->UpdateGpuInfo(gpu_info);
    gpu_data_manager->UpdateGpuFeatureInfo(gpu_feature_info);
  }
  RunRequestGPUInfoCallbacks(gpu_data_manager->GetGPUInfo());
}

void GpuProcessHost::DidFailInitialize() {
  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessInitialized", false);
  status_ = FAILURE;
  GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  gpu_data_manager->OnGpuProcessInitFailure();
  RunRequestGPUInfoCallbacks(gpu_data_manager->GetGPUInfo());
}

void GpuProcessHost::DidCreateOffscreenContext(const GURL& url) {
  urls_with_live_offscreen_contexts_.insert(url);
}

void GpuProcessHost::DidDestroyOffscreenContext(const GURL& url) {
  urls_with_live_offscreen_contexts_.erase(url);
}

void GpuProcessHost::DidDestroyChannel(int32_t client_id) {
  TRACE_EVENT0("gpu", "GpuProcessHost::DidDestroyChannel");
  client_id_to_shader_cache_.erase(client_id);
}

void GpuProcessHost::DidLoseContext(bool offscreen,
                                    gpu::error::ContextLostReason reason,
                                    const GURL& active_url) {
  // TODO(kbr): would be nice to see the "offscreen" flag too.
  TRACE_EVENT2("gpu", "GpuProcessHost::DidLoseContext", "reason", reason, "url",
               active_url.possibly_invalid_spec());

  if (!offscreen || active_url.is_empty()) {
    // Assume that the loss of the compositor's or accelerated canvas'
    // context is a serious event and blame the loss on all live
    // offscreen contexts. This more robustly handles situations where
    // the GPU process may not actually detect the context loss in the
    // offscreen context. However, situations have been seen where the
    // compositor's context can be lost due to driver bugs (as of this
    // writing, on Android), so allow that possibility.
    if (!GpuDataManagerImpl::GetInstance()->IsDriverBugWorkaroundActive(
            gpu::DONT_DISABLE_WEBGL_WHEN_COMPOSITOR_CONTEXT_LOST)) {
      BlockLiveOffscreenContexts();
    }
    return;
  }

  GpuDataManagerImpl::DomainGuilt guilt =
      GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN;
  switch (reason) {
    case gpu::error::kGuilty:
      guilt = GpuDataManagerImpl::DOMAIN_GUILT_KNOWN;
      break;
    // Treat most other error codes as though they had unknown provenance.
    // In practice this doesn't affect the user experience. A lost context
    // of either known or unknown guilt still causes user-level 3D APIs
    // (e.g. WebGL) to be blocked on that domain until the user manually
    // reenables them.
    case gpu::error::kUnknown:
    case gpu::error::kOutOfMemory:
    case gpu::error::kMakeCurrentFailed:
    case gpu::error::kGpuChannelLost:
    case gpu::error::kInvalidGpuMessage:
      break;
    case gpu::error::kInnocent:
      return;
  }

  GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(active_url, guilt);
}

void GpuProcessHost::SetChildSurface(gpu::SurfaceHandle parent_handle,
                                     gpu::SurfaceHandle window_handle) {
#if defined(OS_WIN)
  constexpr char kBadMessageError[] = "Bad parenting request from gpu process.";
  if (!in_process_) {
    DCHECK(process_);
    {
      DWORD process_id = 0;
      DWORD thread_id = GetWindowThreadProcessId(parent_handle, &process_id);

      if (!thread_id || process_id != ::GetCurrentProcessId()) {
        process_->TerminateOnBadMessageReceived(kBadMessageError);
        return;
      }
    }

    {
      DWORD process_id = 0;
      DWORD thread_id = GetWindowThreadProcessId(window_handle, &process_id);

      if (!thread_id || process_id != process_->GetProcess().Pid()) {
        process_->TerminateOnBadMessageReceived(kBadMessageError);
        return;
      }
    }
  }

  if (!gfx::RenderingWindowManager::GetInstance()->RegisterChild(
          parent_handle, window_handle)) {
    process_->TerminateOnBadMessageReceived(kBadMessageError);
  }
#endif
}

void GpuProcessHost::StoreShaderToDisk(int32_t client_id,
                                       const std::string& key,
                                       const std::string& shader) {
  TRACE_EVENT0("gpu", "GpuProcessHost::StoreShaderToDisk");
  ClientIdToShaderCacheMap::iterator iter =
      client_id_to_shader_cache_.find(client_id);
  // If the cache doesn't exist then this is an off the record profile.
  if (iter == client_id_to_shader_cache_.end())
    return;
  iter->second->Cache(GetShaderPrefixKey(shader) + ":" + key, shader);
}

void GpuProcessHost::RecordLogMessage(int32_t severity,
                                      const std::string& header,
                                      const std::string& message) {
  GpuDataManagerImpl::GetInstance()->AddLogMessage(severity, header, message);
}

GpuProcessHost::GpuProcessKind GpuProcessHost::kind() {
  return kind_;
}

void GpuProcessHost::ForceShutdown() {
  // This is only called on the IO thread so no race against the constructor
  // for another GpuProcessHost.
  if (g_gpu_process_hosts[kind_] == this)
    g_gpu_process_hosts[kind_] = NULL;

  process_->ForceShutdown();
}

bool GpuProcessHost::LaunchGpuProcess() {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();

  base::CommandLine::StringType gpu_launcher =
      browser_command_line.GetSwitchValueNative(switches::kGpuLauncher);

#if defined(OS_ANDROID)
  // crbug.com/447735. readlink("self/proc/exe") sometimes fails on Android
  // at startup with EACCES. As a workaround ignore this here, since the
  // executable name is actually not used or useful anyways.
  std::unique_ptr<base::CommandLine> cmd_line =
      base::MakeUnique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
#else
#if defined(OS_LINUX)
  int child_flags = gpu_launcher.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                           ChildProcessHost::CHILD_NORMAL;
#else
  int child_flags = ChildProcessHost::CHILD_NORMAL;
#endif

  base::FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
  if (exe_path.empty())
    return false;

  std::unique_ptr<base::CommandLine> cmd_line =
      base::MakeUnique<base::CommandLine>(exe_path);
#endif

  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);

  BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(cmd_line.get());

#if defined(OS_WIN)
  cmd_line->AppendArg(switches::kPrefetchArgumentGpu);
#endif  // defined(OS_WIN)

  if (kind_ == GPU_PROCESS_KIND_UNSANDBOXED)
    cmd_line->AppendSwitch(switches::kDisableGpuSandbox);

  // TODO(penghuang): Replace all GPU related switches with GpuPreferences.
  // https://crbug.com/590825
  // If you want a browser command-line switch passed to the GPU process
  // you need to add it to |kSwitchNames| at the beginning of this file.
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));
  cmd_line->CopySwitchesFrom(
      browser_command_line, switches::kGLSwitchesCopiedFromGpuProcessHost,
      switches::kGLSwitchesCopiedFromGpuProcessHostNumSwitches);

  std::vector<const char*> gpu_workarounds;
  gpu::GpuDriverBugList::AppendAllWorkarounds(&gpu_workarounds);
  cmd_line->CopySwitchesFrom(browser_command_line, gpu_workarounds.data(),
                             gpu_workarounds.size());

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      cmd_line.get(), process_->GetData().id);

  GpuDataManagerImpl::GetInstance()->AppendGpuCommandLine(cmd_line.get());
  if (cmd_line->HasSwitch(switches::kUseGL)) {
    swiftshader_rendering_ = (cmd_line->GetSwitchValueASCII(switches::kUseGL) ==
                              gl::kGLImplementationSwiftShaderForWebGLName);
  }

  bool current_gpu_type_enabled =
      swiftshader_rendering_ ? gpu_enabled_ : hardware_gpu_enabled_;
  if (!current_gpu_type_enabled) {
    SendOutstandingReplies();
    return false;
  }

  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessSoftwareRendering",
                        swiftshader_rendering_);

  // If specified, prepend a launcher program to the command line.
  if (!gpu_launcher.empty())
    cmd_line->PrependWrapper(gpu_launcher);

  std::unique_ptr<GpuSandboxedProcessLauncherDelegate> delegate =
      base::MakeUnique<GpuSandboxedProcessLauncherDelegate>(*cmd_line);
  process_->Launch(std::move(delegate), std::move(cmd_line), true);
  process_launched_ = true;

  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                            LAUNCHED, GPU_PROCESS_LIFETIME_EVENT_MAX);
  return true;
}

void GpuProcessHost::SendOutstandingReplies() {
  valid_ = false;

  // First send empty channel handles for all EstablishChannel requests.
  while (!channel_requests_.empty()) {
    auto callback = channel_requests_.front();
    channel_requests_.pop();
    callback.Run(IPC::ChannelHandle(), gpu::GPUInfo(),
                 EstablishChannelStatus::GPU_HOST_INVALID);
  }

  while (!create_gpu_memory_buffer_requests_.empty()) {
    auto callback = create_gpu_memory_buffer_requests_.front();
    create_gpu_memory_buffer_requests_.pop();
    callback.Run(gfx::GpuMemoryBufferHandle(),
                 BufferCreationStatus::GPU_HOST_INVALID);
  }

  if (!send_destroying_video_surface_done_cb_.is_null())
    base::ResetAndReturn(&send_destroying_video_surface_done_cb_).Run();
}

void GpuProcessHost::RunRequestGPUInfoCallbacks(const gpu::GPUInfo& gpu_info) {
  for (auto& callback : request_gpu_info_callbacks_)
    std::move(callback).Run(gpu_info);

  request_gpu_info_callbacks_.clear();
}

void GpuProcessHost::BlockLiveOffscreenContexts() {
  for (std::multiset<GURL>::iterator iter =
           urls_with_live_offscreen_contexts_.begin();
       iter != urls_with_live_offscreen_contexts_.end(); ++iter) {
    GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(
        *iter, GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN);
  }
}

void GpuProcessHost::RecordProcessCrash() {
  // Maximum number of times the GPU process is allowed to crash in a session.
  // Once this limit is reached, any request to launch the GPU process will
  // fail.
  const int kGpuMaxCrashCount = 3;

  // Last time the GPU process crashed.
  static base::Time last_gpu_crash_time;

  bool disable_crash_limit = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuProcessCrashLimit);

  // Ending only acts as a failure if the GPU process was actually started and
  // was intended for actual rendering (and not just checking caps or other
  // options).
  if (process_launched_ && kind_ == GPU_PROCESS_KIND_SANDBOXED) {
    if (swiftshader_rendering_) {
      UMA_HISTOGRAM_EXACT_LINEAR(
          "GPU.SwiftShaderLifetimeEvents",
          DIED_FIRST_TIME + swiftshader_crash_count_,
          static_cast<int>(GPU_PROCESS_LIFETIME_EVENT_MAX));

      if (++swiftshader_crash_count_ >= kGpuMaxCrashCount &&
          !disable_crash_limit) {
        // SwiftShader is too unstable to use. Disable it for current session.
        gpu_enabled_ = false;
      }
    } else {
      ++gpu_crash_count_;
      UMA_HISTOGRAM_EXACT_LINEAR(
          "GPU.GPUProcessLifetimeEvents",
          std::min(DIED_FIRST_TIME + gpu_crash_count_,
                   GPU_PROCESS_LIFETIME_EVENT_MAX - 1),
          static_cast<int>(GPU_PROCESS_LIFETIME_EVENT_MAX));

      // Allow about 1 GPU crash per hour to be removed from the crash count,
      // so very occasional crashes won't eventually add up and prevent the
      // GPU process from launching.
      ++gpu_recent_crash_count_;
      base::Time current_time = base::Time::Now();
      if (crashed_before_) {
        int hours_different = (current_time - last_gpu_crash_time).InHours();
        gpu_recent_crash_count_ =
            std::max(0, gpu_recent_crash_count_ - hours_different);
      }

      crashed_before_ = true;
      last_gpu_crash_time = current_time;

      if ((gpu_recent_crash_count_ >= kGpuMaxCrashCount ||
           status_ == FAILURE) &&
          !disable_crash_limit) {
#if !defined(OS_CHROMEOS)
        // The GPU process is too unstable to use. Disable it for current
        // session.
        hardware_gpu_enabled_ = false;
        GpuDataManagerImpl::GetInstance()->DisableHardwareAcceleration();
#endif
      }
    }
  }
}

std::string GpuProcessHost::GetShaderPrefixKey(const std::string& shader) {
  if (shader_prefix_key_info_.empty()) {
    gpu::GPUInfo info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();

    shader_prefix_key_info_ =
        GetContentClient()->GetProduct() + "-" +
        info.gl_vendor + "-" + info.gl_renderer + "-" + info.driver_version +
        "-" + info.driver_vendor;

#if defined(OS_ANDROID)
    std::string build_fp =
        base::android::BuildInfo::GetInstance()->android_build_fp();
    // TODO(ericrk): Remove this after it's up for a few days. crbug.com/699122
    CHECK(!build_fp.empty());
    shader_prefix_key_info_ += "-" + build_fp;
#endif
  }

  // The shader prefix key is a SHA1 hash of a set of per-machine info, such as
  // driver version and os version, as well as the shader data being cached.
  // This ensures both that the shader was not corrupted on disk, as well as
  // that the shader is correctly configured for the current hardware.
  std::string prefix;
  base::Base64Encode(base::SHA1HashString(shader_prefix_key_info_ + shader),
                     &prefix);
  return prefix;
}

void GpuProcessHost::LoadedShader(const std::string& key,
                                  const std::string& data) {
  std::string prefix = GetShaderPrefixKey(data);
  bool prefix_ok = !key.compare(0, prefix.length(), prefix);
  UMA_HISTOGRAM_BOOLEAN("GPU.ShaderLoadPrefixOK", prefix_ok);
  if (prefix_ok) {
    // Remove the prefix from the key before load.
    std::string key_no_prefix = key.substr(prefix.length() + 1);
    gpu_service_ptr_->LoadedShader(key_no_prefix, data);
  }
}

viz::mojom::GpuService* GpuProcessHost::gpu_service() {
  DCHECK(gpu_service_ptr_.is_bound());
  return gpu_service_ptr_.get();
}

void GpuProcessHost::CreateChannelCache(int32_t client_id) {
  TRACE_EVENT0("gpu", "GpuProcessHost::CreateChannelCache");

  scoped_refptr<gpu::ShaderDiskCache> cache =
      GetShaderCacheFactorySingleton()->Get(client_id);
  if (!cache.get())
    return;

  cache->set_shader_loaded_callback(base::Bind(&GpuProcessHost::LoadedShader,
                                               weak_ptr_factory_.GetWeakPtr()));

  client_id_to_shader_cache_[client_id] = cache;
}

}  // namespace content
