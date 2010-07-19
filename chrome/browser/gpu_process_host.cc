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
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_switches.h"

#if defined(OS_LINUX)
#include "gfx/gtk_native_view_id_manager.h"
#endif

namespace {

// Tasks used by this file
class RouteOnUIThreadTask : public Task {
 public:
  explicit RouteOnUIThreadTask(const IPC::Message& msg) {
    msg_ = new IPC::Message(msg);
  }

 private:
  void Run() {
    GpuProcessHostUIShim::Get()->OnMessageReceived(*msg_);
    delete msg_;
    msg_ = NULL;
  }
  IPC::Message* msg_;
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
  std::wstring gpu_launcher =
      browser_command_line.GetSwitchValue(switches::kGpuLauncher);

  FilePath exe_path = ChildProcessHost::GetChildPath(gpu_launcher.empty());
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchWithValue(switches::kProcessType,
                                  switches::kGpuProcess);
  cmd_line->AppendSwitchWithValue(switches::kProcessChannelID,
                                  ASCIIToWide(channel_id()));

  // Propagate relevant command line switches.
  static const char* const switch_names[] = {
    switches::kUseGL,
  };

  for (size_t i = 0; i < arraysize(switch_names); ++i) {
    if (browser_command_line.HasSwitch(switch_names[i])) {
      cmd_line->AppendSwitchWithValue(switch_names[i],
          browser_command_line.GetSwitchValueASCII(switch_names[i]));
    }
  }

  const CommandLine& browser_cmd_line = *CommandLine::ForCurrentProcess();
  PropagateBrowserCommandLineToGpu(browser_cmd_line, cmd_line);

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
    ReplyToRenderer(IPC::ChannelHandle(), filter);
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
#if defined(OS_LINUX)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GetViewXID, OnGetViewXID)
#endif
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void GpuProcessHost::OnChannelEstablished(
    const IPC::ChannelHandle& channel_handle,
    const GPUInfo& gpu_info) {
  const ChannelRequest& request = sent_requests_.front();
  ReplyToRenderer(channel_handle, request.filter);
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

#if defined(OS_LINUX)
void GpuProcessHost::OnGetViewXID(gfx::NativeViewId id, unsigned long* xid) {
  GtkNativeViewManager* manager = Singleton<GtkNativeViewManager>::get();
  if (!manager->GetXIDForId(xid, id)) {
    DLOG(ERROR) << "Can't find XID for view id " << id;
    *xid = 0;
  }
}
#endif

void GpuProcessHost::ReplyToRenderer(
    const IPC::ChannelHandle& channel,
    ResourceMessageFilter* filter) {
  ViewMsg_GpuChannelEstablished* message =
      new ViewMsg_GpuChannelEstablished(channel);
  // If the renderer process is performing synchronous initialization,
  // it needs to handle this message before receiving the reply for
  // the synchronous ViewHostMsg_SynchronizeGpu message.
  message->set_unblock(true);
  filter->Send(message);
}

void GpuProcessHost::PropagateBrowserCommandLineToGpu(
    const CommandLine& browser_cmd,
    CommandLine* gpu_cmd) const {
  // Propagate the following switches to the GPU process command line (along
  // with any associated values) if present in the browser command line.
  static const char* const switch_names[] = {
    switches::kDisableLogging,
    switches::kEnableLogging,
    switches::kGpuStartupDialog,
    switches::kLoggingLevel,
  };

  for (size_t i = 0; i < arraysize(switch_names); ++i) {
    if (browser_cmd.HasSwitch(switch_names[i])) {
      gpu_cmd->AppendSwitchWithValue(switch_names[i],
          browser_cmd.GetSwitchValueASCII(switch_names[i]));
    }
  }
}

URLRequestContext* GpuProcessHost::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  return NULL;
}

bool GpuProcessHost::CanShutdown() {
  return true;
}
