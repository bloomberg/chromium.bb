// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_child_thread.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/gpu_host_messages.h"
#include "content/gpu/gpu_service_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/gpu/content_gpu_client.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/gpu/ipc/service/gpu_video_encode_accelerator.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/ui/gpu/interfaces/gpu_service.mojom.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"
#include "url/gurl.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_ANDROID)
#include "media/base/android/media_drm_bridge_client.h"
#endif

namespace content {
namespace {

static base::LazyInstance<scoped_refptr<ThreadSafeSender>>::DestructorAtExit
    g_thread_safe_sender = LAZY_INSTANCE_INITIALIZER;

bool GpuProcessLogMessageHandler(int severity,
                                 const char* file, int line,
                                 size_t message_start,
                                 const std::string& str) {
  std::string header = str.substr(0, message_start);
  std::string message = str.substr(message_start);

  g_thread_safe_sender.Get()->Send(
      new GpuHostMsg_OnLogMessage(severity, header, message));

  return false;
}

// Message filter used to to handle GpuMsg_CreateGpuMemoryBuffer messages
// on the IO thread. This allows the UI thread in the browser process to remain
// fast at all times.
class GpuMemoryBufferMessageFilter : public IPC::MessageFilter {
 public:
  explicit GpuMemoryBufferMessageFilter(
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory)
      : gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
        sender_(nullptr) {}

  // Overridden from IPC::MessageFilter:
  void OnFilterAdded(IPC::Channel* channel) override {
    DCHECK(!sender_);
    sender_ = channel;
  }
  void OnFilterRemoved() override {
    DCHECK(sender_);
    sender_ = nullptr;
  }
  bool OnMessageReceived(const IPC::Message& message) override {
    DCHECK(sender_);
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(GpuMemoryBufferMessageFilter, message)
      IPC_MESSAGE_HANDLER(GpuMsg_CreateGpuMemoryBuffer, OnCreateGpuMemoryBuffer)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

 protected:
  ~GpuMemoryBufferMessageFilter() override {}

  void OnCreateGpuMemoryBuffer(
      const GpuMsg_CreateGpuMemoryBuffer_Params& params) {
    TRACE_EVENT2("gpu", "GpuMemoryBufferMessageFilter::OnCreateGpuMemoryBuffer",
                 "id", params.id.id, "client_id", params.client_id);

    DCHECK(gpu_memory_buffer_factory_);
    sender_->Send(new GpuHostMsg_GpuMemoryBufferCreated(
        gpu_memory_buffer_factory_->CreateGpuMemoryBuffer(
            params.id, params.size, params.format, params.usage,
            params.client_id, params.surface_handle)));
  }

  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
  IPC::Sender* sender_;
};

ChildThreadImpl::Options GetOptions(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory) {
  ChildThreadImpl::Options::Builder builder;

  builder.AddStartupFilter(
      new GpuMemoryBufferMessageFilter(gpu_memory_buffer_factory));

#if defined(USE_OZONE)
  IPC::MessageFilter* message_filter =
      ui::OzonePlatform::GetInstance()->GetGpuMessageFilter();
  if (message_filter)
    builder.AddStartupFilter(message_filter);
#endif

  builder.ConnectToBrowser(true);

  return builder.Build();
}

}  // namespace

GpuChildThread::GpuChildThread(
    std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread,
    bool dead_on_arrival,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const DeferredMessages& deferred_messages,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : ChildThreadImpl(GetOptions(gpu_memory_buffer_factory)),
      dead_on_arrival_(dead_on_arrival),
      gpu_info_(gpu_info),
      deferred_messages_(deferred_messages),
      in_browser_process_(false),
      gpu_service_(new ui::GpuService(gpu_info,
                                      std::move(watchdog_thread),
                                      gpu_memory_buffer_factory,
                                      ChildProcess::current()->io_task_runner(),
                                      gpu_feature_info)),
      gpu_main_binding_(this) {
#if defined(OS_WIN)
  target_services_ = NULL;
#endif
  g_thread_safe_sender.Get() = thread_safe_sender();
}

GpuChildThread::GpuChildThread(
    const InProcessChildThreadParams& params,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : ChildThreadImpl(ChildThreadImpl::Options::Builder()
                          .InBrowserProcess(params)
                          .AddStartupFilter(new GpuMemoryBufferMessageFilter(
                              gpu_memory_buffer_factory))
                          .ConnectToBrowser(true)
                          .Build()),
      dead_on_arrival_(false),
      gpu_info_(gpu_info),
      in_browser_process_(true),
      gpu_service_(new ui::GpuService(gpu_info,
                                      nullptr /* watchdog thread */,
                                      gpu_memory_buffer_factory,
                                      ChildProcess::current()->io_task_runner(),
                                      gpu_feature_info)),
      gpu_main_binding_(this) {
#if defined(OS_WIN)
  target_services_ = NULL;
#endif
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSingleProcess) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kInProcessGPU));

  g_thread_safe_sender.Get() = thread_safe_sender();
}

GpuChildThread::~GpuChildThread() {
}

void GpuChildThread::Shutdown() {
  ChildThreadImpl::Shutdown();
  logging::SetLogMessageHandler(NULL);
}

void GpuChildThread::Init(const base::Time& process_start_time) {
  process_start_time_ = process_start_time;

#if defined(OS_ANDROID)
  // When running in in-process mode, this has been set in the browser at
  // ChromeBrowserMainPartsAndroid::PreMainMessageLoopRun().
  if (!in_browser_process_)
    media::SetMediaDrmBridgeClient(
        GetContentClient()->GetMediaDrmBridgeClient());
#endif
  // We don't want to process any incoming interface requests until
  // OnInitialize() is invoked.
  GetInterfaceRegistry()->PauseBinding();

  if (GetContentClient()->gpu())  // NULL in tests.
    GetContentClient()->gpu()->Initialize(this);
  AssociatedInterfaceRegistry* registry = &associated_interfaces_;
  registry->AddInterface(base::Bind(
      &GpuChildThread::CreateGpuMainService, base::Unretained(this)));
}

void GpuChildThread::OnFieldTrialGroupFinalized(const std::string& trial_name,
                                                const std::string& group_name) {
  Send(new GpuHostMsg_FieldTrialActivated(trial_name));
}

void GpuChildThread::CreateGpuMainService(
    ui::mojom::GpuMainAssociatedRequest request) {
  gpu_main_binding_.Bind(std::move(request));
}

bool GpuChildThread::Send(IPC::Message* msg) {
  // The GPU process must never send a synchronous IPC message to the browser
  // process. This could result in deadlock.
  DCHECK(!msg->is_sync());

  return ChildThreadImpl::Send(msg);
}

bool GpuChildThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChildThread, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_CollectGraphicsInfo, OnCollectGraphicsInfo)
    IPC_MESSAGE_HANDLER(GpuMsg_GpuSwitched, OnGpuSwitched)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool GpuChildThread::OnMessageReceived(const IPC::Message& msg) {
  if (ChildThreadImpl::OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChildThread, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_DestroyGpuMemoryBuffer, OnDestroyGpuMemoryBuffer)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return true;

  return false;
}

void GpuChildThread::OnAssociatedInterfaceRequest(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (associated_interfaces_.CanBindRequest(name))
    associated_interfaces_.BindRequest(name, std::move(handle));
  else
    ChildThreadImpl::OnAssociatedInterfaceRequest(name, std::move(handle));
}

void GpuChildThread::CreateGpuService(
    ui::mojom::GpuServiceRequest request,
    ui::mojom::GpuHostPtr gpu_host,
    const gpu::GpuPreferences& gpu_preferences,
    mojo::ScopedSharedBufferHandle activity_flags) {
  gpu_service_->Bind(std::move(request));

  gpu_info_.video_decode_accelerator_capabilities =
      media::GpuVideoDecodeAccelerator::GetCapabilities(gpu_preferences);
  gpu_info_.video_encode_accelerator_supported_profiles =
      media::GpuVideoEncodeAccelerator::GetSupportedProfiles(gpu_preferences);
  gpu_info_.jpeg_decode_accelerator_supported =
      media::GpuJpegDecodeAcceleratorFactoryProvider::
          IsAcceleratedJpegDecodeSupported();

  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  gpu_info_.initialization_time = base::Time::Now() - process_start_time_;
  Send(new GpuHostMsg_Initialized(!dead_on_arrival_, gpu_info_,
                                  gpu_service_->gpu_feature_info()));
  while (!deferred_messages_.empty()) {
    const LogMessage& log = deferred_messages_.front();
    Send(new GpuHostMsg_OnLogMessage(log.severity, log.header, log.message));
    deferred_messages_.pop();
  }

  if (dead_on_arrival_) {
    LOG(ERROR) << "Exiting GPU process due to errors during initialization";
    base::MessageLoop::current()->QuitWhenIdle();
    return;
  }

  // We don't need to pipe log messages if we are running the GPU thread in
  // the browser process.
  if (!in_browser_process_)
    logging::SetLogMessageHandler(GpuProcessLogMessageHandler);

  gpu::SyncPointManager* sync_point_manager = nullptr;
  // Note SyncPointManager from ContentGpuClient cannot be owned by this.
  if (GetContentClient()->gpu())
    sync_point_manager = GetContentClient()->gpu()->GetSyncPointManager();
  gpu_service_->InitializeWithHost(
      std::move(gpu_host), gpu_preferences,
      gpu::GpuProcessActivityFlags(std::move(activity_flags)),
      sync_point_manager, ChildProcess::current()->GetShutDownEvent());
  CHECK(gpu_service_->media_gpu_channel_manager());

  // Only set once per process instance.
  service_factory_.reset(new GpuServiceFactory(
      gpu_service_->media_gpu_channel_manager()->AsWeakPtr()));

  GetInterfaceRegistry()->AddInterface(base::Bind(
      &GpuChildThread::BindServiceFactoryRequest, base::Unretained(this)));

  if (GetContentClient()->gpu()) {  // NULL in tests.
    GetContentClient()->gpu()->ExposeInterfacesToBrowser(GetInterfaceRegistry(),
                                                         gpu_preferences);
    GetContentClient()->gpu()->ConsumeInterfacesFromBrowser(
        GetRemoteInterfaces());
  }

  GetInterfaceRegistry()->ResumeBinding();
}

void GpuChildThread::CreateDisplayCompositor(
    cc::mojom::DisplayCompositorRequest request,
    cc::mojom::DisplayCompositorClientPtr client) {
  NOTREACHED();
}

void GpuChildThread::OnCollectGraphicsInfo() {
  if (dead_on_arrival_)
    return;

#if defined(OS_MACOSX)
  // gpu::CollectContextGraphicsInfo() is already called during gpu process
  // initialization (see GpuInit::InitializeAndStartSandbox()) on non-mac
  // platforms, and during in-browser gpu thread initialization on all platforms
  // (See InProcessGpuThread::Init()).
  if (!in_browser_process_) {
    DCHECK_EQ(gpu::kCollectInfoNone, gpu_info_.context_info_state);
    gpu::CollectInfoResult result = gpu::CollectContextGraphicsInfo(&gpu_info_);
    switch (result) {
      case gpu::kCollectInfoFatalFailure:
        LOG(ERROR) << "gpu::CollectGraphicsInfo failed (fatal).";
        // TODO(piman): can we signal overall failure?
        break;
      case gpu::kCollectInfoNonFatalFailure:
        DVLOG(1) << "gpu::CollectGraphicsInfo failed (non-fatal).";
        break;
      case gpu::kCollectInfoNone:
        NOTREACHED();
        break;
      case gpu::kCollectInfoSuccess:
        break;
    }
    GetContentClient()->SetGpuInfo(gpu_info_);
  }
#endif

#if defined(OS_WIN)
  // GPU full info collection should only happen on un-sandboxed GPU process
  // or single process/in-process gpu mode on Windows.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line->HasSwitch(switches::kDisableGpuSandbox) ||
         in_browser_process_);

  // This is slow, but it's the only thing the unsandboxed GPU process does,
  // and GpuDataManager prevents us from sending multiple collecting requests,
  // so it's OK to be blocking.
  gpu::GetDxDiagnostics(&gpu_info_.dx_diagnostics);
  gpu_info_.dx_diagnostics_info_state = gpu::kCollectInfoSuccess;
#endif  // OS_WIN

  Send(new GpuHostMsg_GraphicsInfoCollected(gpu_info_));

#if defined(OS_WIN)
  if (!in_browser_process_) {
    // The unsandboxed GPU process fulfilled its duty.  Rest in peace.
    base::MessageLoop::current()->QuitWhenIdle();
  }
#endif  // OS_WIN
}

void GpuChildThread::OnGpuSwitched() {
  DVLOG(1) << "GPU: GPU has switched";
  // Notify observers in the GPU process.
  if (!in_browser_process_)
    ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched();
}

void GpuChildThread::OnDestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id,
    const gpu::SyncToken& sync_token) {
  if (gpu_channel_manager())
    gpu_channel_manager()->DestroyGpuMemoryBuffer(id, client_id, sync_token);
}

void GpuChildThread::BindServiceFactoryRequest(
    service_manager::mojom::ServiceFactoryRequest request) {
  DVLOG(1) << "GPU: Binding service_manager::mojom::ServiceFactoryRequest";
  DCHECK(service_factory_);
  service_factory_bindings_.AddBinding(service_factory_.get(),
                                       std::move(request));
}

}  // namespace content
