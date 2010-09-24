// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_process_host.h"

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/gpu_process_host_ui_shim.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gpu_info.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_switches.h"

#if defined(OS_LINUX)
#include "gfx/gtk_native_view_id_manager.h"
#endif

namespace {

// Tasks used by this file
class RouteOnUIThreadTask : public Task {
 public:
  explicit RouteOnUIThreadTask(const IPC::Message& msg) : msg_(msg) {
  }

 private:
  void Run() {
    GpuProcessHostUIShim::Get()->OnMessageReceived(msg_);
  }
  IPC::Message msg_;
};

// Global GpuProcessHost instance.
// We can not use Singleton<GpuProcessHost> because that gets
// terminated on the wrong thread (main thread). We need the
// GpuProcessHost to be terminated on the same thread on which it is
// initialized, the IO thread.
static GpuProcessHost* sole_instance_ = NULL;

}  // anonymous namespace

GpuProcessHost::GpuProcessHost()
    : BrowserChildProcessHost(GPU_PROCESS, NULL),
      initialized_(false),
      initialized_successfully_(false) {
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
  }
  return initialized_successfully_;
}

bool GpuProcessHost::Init() {
  if (!CreateChannel())
    return false;

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
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
    switches::kDisableLogging,
    switches::kEnableLogging,
    switches::kGpuStartupDialog,
    switches::kLoggingLevel,
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

  return true;
}

// static
GpuProcessHost* GpuProcessHost::Get() {
  if (sole_instance_ == NULL)
    sole_instance_ = new GpuProcessHost();
  return sole_instance_;
}

// static
void GpuProcessHost::SendAboutGpuCrash() {
  Get()->Send(new GpuMsg_Crash());
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  if (!EnsureInitialized())
    return false;

  return BrowserChildProcessHost::Send(msg);
}

void GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  if (message.routing_id() == MSG_ROUTING_CONTROL) {
    OnControlMessageReceived(message);
  } else {
    // Need to transfer this message to the UI thread and the
    // GpuProcessHostUIShim for dispatching via its message router.
    ChromeThread::PostTask(ChromeThread::UI,
                           FROM_HERE,
                           new RouteOnUIThreadTask(message));
  }
}

void GpuProcessHost::EstablishGpuChannel(int renderer_id,
                                         ResourceMessageFilter* filter) {
  if (Send(new GpuMsg_EstablishChannel(renderer_id))) {
    sent_requests_.push(ChannelRequest(filter));
  } else {
    ReplyToRenderer(IPC::ChannelHandle(), GPUInfo(), filter);
  }
}

void GpuProcessHost::Synchronize(IPC::Message* reply,
                                 ResourceMessageFilter* filter) {
  queued_synchronization_replies_.push(SynchronizationRequest(reply, filter));
  Send(new GpuMsg_Synchronize());
}

GPUInfo GpuProcessHost::gpu_info() const {
  return gpu_info_;
}

void GpuProcessHost::OnControlMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(GpuProcessHost, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_ChannelEstablished, OnChannelEstablished)
    IPC_MESSAGE_HANDLER(GpuHostMsg_SynchronizeReply, OnSynchronizeReply)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GraphicsInfoCollected,
                        OnGraphicsInfoCollected)
#if defined(OS_LINUX)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GetViewXID, OnGetViewXID)
#elif defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceSetIOSurface,
                        OnAcceleratedSurfaceSetIOSurface)
    IPC_MESSAGE_HANDLER(GpuHostMsg_AcceleratedSurfaceBuffersSwapped,
                        OnAcceleratedSurfaceBuffersSwapped)
#endif
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void GpuProcessHost::OnChannelEstablished(
    const IPC::ChannelHandle& channel_handle,
    const GPUInfo& gpu_info) {
  const ChannelRequest& request = sent_requests_.front();
  ReplyToRenderer(channel_handle, gpu_info, request.filter);
  sent_requests_.pop();
  gpu_info_ = gpu_info;
  child_process_logging::SetGpuInfo(gpu_info);
}

void GpuProcessHost::OnSynchronizeReply() {
  const SynchronizationRequest& request =
      queued_synchronization_replies_.front();
  request.filter->Send(request.reply);
  queued_synchronization_replies_.pop();
}

void GpuProcessHost::OnGraphicsInfoCollected(const GPUInfo& gpu_info) {
  gpu_info_ = gpu_info;
}

#if defined(OS_LINUX)
void GpuProcessHost::OnGetViewXID(gfx::NativeViewId id, unsigned long* xid) {
  GtkNativeViewManager* manager = Singleton<GtkNativeViewManager>::get();
  if (!manager->GetXIDForId(xid, id)) {
    DLOG(ERROR) << "Can't find XID for view id " << id;
    *xid = 0;
  }
}

#elif defined(OS_MACOSX)

namespace {

class SetIOSurfaceDispatcher : public Task {
 public:
  SetIOSurfaceDispatcher(
      const GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params& params)
      : params_(params) {
  }

  void Run() {
    RenderViewHost* host = RenderViewHost::FromID(params_.renderer_id,
                                                  params_.render_view_id);
    if (!host)
      return;
    RenderWidgetHostView* view = host->view();
    if (!view)
      return;
    view->AcceleratedSurfaceSetIOSurface(params_.window,
                                         params_.width,
                                         params_.height,
                                         params_.identifier);
  }

 private:
  GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params params_;

  DISALLOW_COPY_AND_ASSIGN(SetIOSurfaceDispatcher);
};

}  // namespace

void GpuProcessHost::OnAcceleratedSurfaceSetIOSurface(
    const GpuHostMsg_AcceleratedSurfaceSetIOSurface_Params& params) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      new SetIOSurfaceDispatcher(params));
}

namespace {

class BuffersSwappedDispatcher : public Task {
 public:
  BuffersSwappedDispatcher(
      int32 renderer_id, int32 render_view_id, gfx::PluginWindowHandle window)
      : renderer_id_(renderer_id),
        render_view_id_(render_view_id),
        window_(window) {
  }

  void Run() {
    RenderViewHost* host = RenderViewHost::FromID(renderer_id_,
                                                  render_view_id_);
    if (!host)
      return;
    RenderWidgetHostView* view = host->view();
    if (!view)
      return;
    view->AcceleratedSurfaceBuffersSwapped(window_);
  }

 private:
  int32 renderer_id_;
  int32 render_view_id_;
  gfx::PluginWindowHandle window_;

  DISALLOW_COPY_AND_ASSIGN(BuffersSwappedDispatcher);
};

}  // namespace

void GpuProcessHost::OnAcceleratedSurfaceBuffersSwapped(
    int32 renderer_id,
    int32 render_view_id,
    gfx::PluginWindowHandle window) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      new BuffersSwappedDispatcher(renderer_id, render_view_id, window));
}
#endif

void GpuProcessHost::ReplyToRenderer(
    const IPC::ChannelHandle& channel,
    const GPUInfo& gpu_info,
    ResourceMessageFilter* filter) {
  ViewMsg_GpuChannelEstablished* message =
      new ViewMsg_GpuChannelEstablished(channel, gpu_info);
  // If the renderer process is performing synchronous initialization,
  // it needs to handle this message before receiving the reply for
  // the synchronous ViewHostMsg_SynchronizeGpu message.
  message->set_unblock(true);
  filter->Send(message);
}

URLRequestContext* GpuProcessHost::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  return NULL;
}

bool GpuProcessHost::CanShutdown() {
  return true;
}
