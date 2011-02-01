// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_process_host.h"

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/string_piece.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/gpu_blacklist.h"
#include "chrome/browser/gpu_process_host_ui_shim.h"
#include "chrome/browser/renderer_host/gpu_message_filter.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gpu_feature_flags.h"
#include "chrome/common/gpu_info.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/gpu/gpu_thread.h"
#include "grit/browser_resources.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"
#include "media/base/media_switches.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_LINUX)
#include "gfx/gtk_native_view_id_manager.h"
#endif  // defined(OS_LINUX)

namespace {

enum GPUProcessLifetimeEvent {
  LAUNCHED,
  DIED_FIRST_TIME,
  DIED_SECOND_TIME,
  DIED_THIRD_TIME,
  DIED_FOURTH_TIME,
  GPU_PROCESS_LIFETIME_EVENT_MAX
  };

// Tasks used by this file
class RouteOnUIThreadTask : public Task {
 public:
  explicit RouteOnUIThreadTask(const IPC::Message& msg) : msg_(msg) {
  }

 private:
  void Run() {
    GpuProcessHostUIShim::GetInstance()->OnMessageReceived(msg_);
  }
  IPC::Message msg_;
};

// Global GpuProcessHost instance.
// We can not use Singleton<GpuProcessHost> because that gets
// terminated on the wrong thread (main thread). We need the
// GpuProcessHost to be terminated on the same thread on which it is
// initialized, the IO thread.
static GpuProcessHost* sole_instance_ = NULL;

// Number of times the gpu process has crashed in the current browser session.
static int g_gpu_crash_count = 0;
// Maximum number of times the gpu process is allowed to crash in a session.
// Once this limit is reached, any request to launch the gpu process will fail.
static const int kGpuMaxCrashCount = 3;

void RouteOnUIThread(const IPC::Message& message) {
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          new RouteOnUIThreadTask(message));
}

bool SendDelayedMsg(IPC::Message* reply_msg) {
  return GpuProcessHost::Get()->Send(reply_msg);
}

bool SendDelayedMsg(IPC::Message* reply, GpuMessageFilter* filter) {
  return filter->Send(reply);
}

}  // anonymous namespace

class GpuMainThread : public base::Thread {
 public:
  explicit GpuMainThread(const std::string& channel_id)
      : base::Thread("CrGpuMain"),
        channel_id_(channel_id) {
  }

  ~GpuMainThread() {
    Stop();
  }

 protected:
  virtual void Init() {
    // Must be created on GPU thread.
    gpu_thread_.reset(new GpuThread(channel_id_));
    gpu_thread_->Init(base::Time::Now());
  }

  virtual void CleanUp() {
    // Must be destroyed on GPU thread.
    gpu_thread_.reset();
  }

 private:
  scoped_ptr<GpuThread> gpu_thread_;
  std::string channel_id_;
  DISALLOW_COPY_AND_ASSIGN(GpuMainThread);
};

GpuProcessHost::GpuProcessHost()
    : BrowserChildProcessHost(GPU_PROCESS, NULL),
      initialized_(false),
      initialized_successfully_(false),
      gpu_feature_flags_set_(false) {
  DCHECK_EQ(sole_instance_, static_cast<GpuProcessHost*>(NULL));
}

GpuProcessHost::~GpuProcessHost() {
  while (!queued_synchronization_replies_.empty()) {
    delete queued_synchronization_replies_.front().reply;
    queued_synchronization_replies_.pop();
  }
  DCHECK_EQ(sole_instance_, this);
  sole_instance_ = NULL;
}

bool GpuProcessHost::EnsureInitialized() {
  if (!initialized_) {
    initialized_ = true;
    initialized_successfully_ = Init();
    if (initialized_successfully_) {
      Send(new GpuMsg_Initialize());
    }
  }
  return initialized_successfully_;
}

bool GpuProcessHost::Init() {
  if (!LoadGpuBlacklist())
    return false;

  if (!CreateChannel())
    return false;

  if (!CanLaunchGpuProcess())
    return false;

  return LaunchGpuProcess();
}

// static
GpuProcessHost* GpuProcessHost::Get() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (sole_instance_ == NULL)
    sole_instance_ = new GpuProcessHost();
  return sole_instance_;
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());

  if (!EnsureInitialized())
    return false;

  return BrowserChildProcessHost::Send(msg);
}

bool GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  if (message.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(message);

  RouteOnUIThread(message);
  return true;
}

void GpuProcessHost::EstablishGpuChannel(int renderer_id,
                                         GpuMessageFilter* filter) {
  DCHECK(CalledOnValidThread());

  if (Send(new GpuMsg_EstablishChannel(renderer_id))) {
    sent_requests_.push(ChannelRequest(filter));
  } else {
    SendEstablishChannelReply(IPC::ChannelHandle(), GPUInfo(), filter);
  }
}

void GpuProcessHost::Synchronize(IPC::Message* reply,
                                 GpuMessageFilter* filter) {
  DCHECK(CalledOnValidThread());

  if (Send(new GpuMsg_Synchronize())) {
    queued_synchronization_replies_.push(DelayedReply(reply, filter));
  } else {
    SendDelayedMsg(reply, filter);
  }
}

class CVCBThreadHopping {
 public:
  // Send the request for a command buffer from the IO thread and
  // queue that we are expecting a response.
  static void DispatchIPCAndQueueReply(
      gfx::PluginWindowHandle view,
      int32 render_view_id,
      int32 renderer_id,
      const GPUCreateCommandBufferConfig& init_params,
      IPC::Message* reply,
      scoped_refptr<GpuMessageFilter> filter);

  // Get a window for the command buffer that we're creating.
  static void GetViewWindow(
      int32 render_view_id,
      int32 renderer_id,
      const GPUCreateCommandBufferConfig& init_params,
      IPC::Message* reply,
      scoped_refptr<GpuMessageFilter> filter);
};

void CVCBThreadHopping::DispatchIPCAndQueueReply(
    gfx::PluginWindowHandle view,
    int32 render_view_id,
    int32 renderer_id,
    const GPUCreateCommandBufferConfig& init_params,
    IPC::Message* reply,
    scoped_refptr<GpuMessageFilter> filter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GpuProcessHost* host = GpuProcessHost::Get();

  if (view != gfx::kNullPluginWindow &&
      SendDelayedMsg(new GpuMsg_CreateViewCommandBuffer(
          view, render_view_id, renderer_id, init_params))) {
    host->create_command_buffer_replies_.push(
        GpuProcessHost::DelayedReply(reply, filter));
  } else {
    int32 route_id = MSG_ROUTING_NONE;
    GpuHostMsg_CreateViewCommandBuffer::WriteReplyParams(
        reply, route_id);
    SendDelayedMsg(reply, filter);
  }
}

void CVCBThreadHopping::GetViewWindow(
    int32 render_view_id,
    int32 renderer_id,
    const GPUCreateCommandBufferConfig& init_params,
    IPC::Message* reply,
    scoped_refptr<GpuMessageFilter> filter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gfx::PluginWindowHandle window = gfx::kNullPluginWindow;
  RenderProcessHost* process = RenderProcessHost::FromID(renderer_id);
  RenderWidgetHost* host = NULL;
  if (process) {
    host = static_cast<RenderWidgetHost*>(
        process->GetListenerByID(render_view_id));
  }
#if defined(OS_LINUX)
  gfx::NativeViewId view = NULL;
  if (host)
    view = gfx::IdFromNativeView(host->view()->GetNativeView());

  // Lock the window that we will draw into.
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  if (!manager->GetPermanentXIDForId(&window, view)) {
    DLOG(ERROR) << "Can't find XID for view id " << view;
  }
#elif defined(OS_MACOSX)
  // On Mac OS X we currently pass a (fake) PluginWindowHandle for the
  // window that we draw to.
  window = host->view()->AllocateFakePluginWindowHandle(
      /*opaque=*/true, /*root=*/true);
#elif defined(OS_WIN)
  // Create a window that we will overlay.
  window = host->view()->GetCompositorHostWindow();
#endif

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, NewRunnableFunction(
      &CVCBThreadHopping::DispatchIPCAndQueueReply,
      window, render_view_id, renderer_id, init_params, reply, filter));
}

void GpuProcessHost::CreateViewCommandBuffer(
    int32 render_view_id,
    int32 renderer_id,
    const GPUCreateCommandBufferConfig& init_params,
    IPC::Message* reply,
    GpuMessageFilter* filter) {
  DCHECK(CalledOnValidThread());

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableFunction(
      &CVCBThreadHopping::GetViewWindow,
      render_view_id, renderer_id, init_params, reply,
      static_cast<scoped_refptr<GpuMessageFilter> > (filter)));
}


GpuProcessHost::ChannelRequest::ChannelRequest(
    GpuMessageFilter* filter)
    : filter(filter) {
}

GpuProcessHost::ChannelRequest::~ChannelRequest() {}

GpuProcessHost::DelayedReply::DelayedReply(
    IPC::Message* reply,
    GpuMessageFilter* filter)
    : reply(reply),
      filter(filter) {
}

GpuProcessHost::DelayedReply::~DelayedReply() {}

bool GpuProcessHost::OnControlMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  IPC_BEGIN_MESSAGE_MAP(GpuProcessHost, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ChannelEstablished, OnChannelEstablished)
    IPC_MESSAGE_HANDLER(GpuHostMsg_SynchronizeReply, OnSynchronizeReply)
    IPC_MESSAGE_HANDLER(GpuHostMsg_CommandBufferCreated, OnCommandBufferCreated)
    // If the IO thread does not handle the message then automatically route it
    // to the UI thread. The UI thread will report an error if it does not
    // handle it.
    IPC_MESSAGE_UNHANDLED(RouteOnUIThread(message))
  IPC_END_MESSAGE_MAP()

  return true;
}

void GpuProcessHost::OnChannelEstablished(
    const IPC::ChannelHandle& channel_handle,
    const GPUInfo& gpu_info) {
  if (channel_handle.name.size() != 0 && !gpu_feature_flags_set_) {
    gpu_feature_flags_ = gpu_blacklist_->DetermineGpuFeatureFlags(
        GpuBlacklist::kOsAny, NULL, gpu_info);
    gpu_feature_flags_set_ = true;
    uint32 max_entry_id = gpu_blacklist_->max_entry_id();
    if (gpu_feature_flags_.flags() != 0) {
      std::vector<uint32> flag_entries;
      gpu_blacklist_->GetGpuFeatureFlagEntries(GpuFeatureFlags::kGpuFeatureAll,
                                               flag_entries);
      DCHECK_GT(flag_entries.size(), 0u);
      for (size_t i = 0; i < flag_entries.size(); ++i) {
        UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
                                  flag_entries[i], max_entry_id + 1);
      }
    } else {
      // id 0 is never used by any entry, so we use it here to indicate that
      // gpu is allowed.
      UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
                                0, max_entry_id + 1);
    }
  }
  const ChannelRequest& request = sent_requests_.front();
  // Currently if any of the GPU features are blacklised, we don't establish a
  // GPU channel.
  if (gpu_feature_flags_.flags() != 0) {
    Send(new GpuMsg_CloseChannel(channel_handle));
    SendEstablishChannelReply(IPC::ChannelHandle(), gpu_info, request.filter);
  } else {
    SendEstablishChannelReply(channel_handle, gpu_info, request.filter);
  }
  sent_requests_.pop();
}

void GpuProcessHost::OnSynchronizeReply() {
  // Guard against race conditions in abrupt GPU process termination.
  if (queued_synchronization_replies_.size() > 0) {
    const DelayedReply& request =
        queued_synchronization_replies_.front();
    SendDelayedMsg(request.reply, request.filter);
    queued_synchronization_replies_.pop();
  }
}

void GpuProcessHost::OnCommandBufferCreated(const int32 route_id) {
  if (create_command_buffer_replies_.size() > 0) {
    const DelayedReply& request =
        create_command_buffer_replies_.front();
    GpuHostMsg_CreateViewCommandBuffer::WriteReplyParams(
        request.reply, route_id);
    SendDelayedMsg(request.reply, request.filter);
    create_command_buffer_replies_.pop();
  }
}

void GpuProcessHost::SendEstablishChannelReply(
    const IPC::ChannelHandle& channel,
    const GPUInfo& gpu_info,
    GpuMessageFilter* filter) {
  ViewMsg_GpuChannelEstablished* message =
      new ViewMsg_GpuChannelEstablished(channel, gpu_info);
  // If the renderer process is performing synchronous initialization,
  // it needs to handle this message before receiving the reply for
  // the synchronous GpuHostMsg_SynchronizeGpu message.
  message->set_unblock(true);
  filter->Send(message);
}

void GpuProcessHost::SendOutstandingReplies() {
  // First send empty channel handles for all EstablishChannel requests.
  while (!sent_requests_.empty()) {
    const ChannelRequest& request = sent_requests_.front();
    SendEstablishChannelReply(IPC::ChannelHandle(), GPUInfo(), request.filter);
    sent_requests_.pop();
  }

  // Now unblock all renderers waiting for synchronization replies.
  while (!queued_synchronization_replies_.empty()) {
    OnSynchronizeReply();
  }
}

bool GpuProcessHost::CanShutdown() {
  return true;
}

void GpuProcessHost::OnChildDied() {
  SendOutstandingReplies();
  // Located in OnChildDied because OnProcessCrashed suffers from a race
  // condition on Linux.
  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                            DIED_FIRST_TIME + g_gpu_crash_count,
                            GPU_PROCESS_LIFETIME_EVENT_MAX);
  BrowserChildProcessHost::OnChildDied();
}

void GpuProcessHost::OnProcessCrashed(int exit_code) {
  SendOutstandingReplies();
  if (++g_gpu_crash_count >= kGpuMaxCrashCount) {
    // The gpu process is too unstable to use. Disable it for current session.
    RenderViewHostDelegateHelper::set_gpu_enabled(false);
  }
  BrowserChildProcessHost::OnProcessCrashed(exit_code);
}

bool GpuProcessHost::CanLaunchGpuProcess() const {
  return RenderViewHostDelegateHelper::gpu_enabled();
}

bool GpuProcessHost::LaunchGpuProcess() {
  if (g_gpu_crash_count >= kGpuMaxCrashCount)
    return false;

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();

  // If the single-process switch is present, just launch the GPU service in a
  // new thread in the browser process.
  if (browser_command_line.HasSwitch(switches::kSingleProcess)) {
    GpuMainThread* thread = new GpuMainThread(channel_id());

    base::Thread::Options options;
#if defined(OS_LINUX)
    options.message_loop_type = MessageLoop::TYPE_IO;
#else
    options.message_loop_type = MessageLoop::TYPE_UI;
#endif

    if (!thread->StartWithOptions(options))
      return false;

    return true;
  }

  CommandLine::StringType gpu_launcher =
      browser_command_line.GetSwitchValueNative(switches::kGpuLauncher);

  FilePath exe_path = ChildProcessHost::GetChildPath(gpu_launcher.empty());
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id());

  // Propagate relevant command line switches.
  static const char* const kSwitchNames[] = {
    switches::kUseGL,
    switches::kDisableGpuVsync,
    switches::kDisableGpuWatchdog,
    switches::kDisableLogging,
    switches::kEnableAcceleratedDecoding,
    switches::kEnableLogging,
#if defined(OS_MACOSX)
    switches::kEnableSandboxLogging,
#endif
    switches::kGpuStartupDialog,
    switches::kLoggingLevel,
    switches::kNoGpuSandbox,
    switches::kNoSandbox,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));

  // If specified, prepend a launcher program to the command line.
  if (!gpu_launcher.empty())
    cmd_line->PrependWrapper(gpu_launcher);

  Launch(
#if defined(OS_WIN)
      FilePath(),
#elif defined(OS_POSIX)
      false,  // Never use the zygote (GPU plugin can't be sandboxed).
      base::environment_vector(),
#endif
      cmd_line);

  UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessLifetimeEvents",
                            LAUNCHED, GPU_PROCESS_LIFETIME_EVENT_MAX);
  return true;
}

bool GpuProcessHost::LoadGpuBlacklist() {
  if (gpu_blacklist_.get() != NULL)
    return true;
  static const base::StringPiece gpu_blacklist_json(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_GPU_BLACKLIST));
  GpuBlacklist* blacklist = new GpuBlacklist();
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kIgnoreGpuBlacklist) ||
      blacklist->LoadGpuBlacklist(gpu_blacklist_json.as_string(), true)) {
    gpu_blacklist_.reset(blacklist);
    return true;
  }
  delete blacklist;
  return false;
}

