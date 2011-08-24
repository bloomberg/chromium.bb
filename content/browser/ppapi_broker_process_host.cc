// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ppapi_broker_process_host.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/plugin_service.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/common/content_switches.h"
#include "content/common/pepper_plugin_registry.h"
#include "ipc/ipc_switches.h"
#include "ppapi/proxy/ppapi_messages.h"

PpapiBrokerProcessHost::PpapiBrokerProcessHost()
    : BrowserChildProcessHost(ChildProcessInfo::PPAPI_BROKER_PROCESS) {
}

PpapiBrokerProcessHost::~PpapiBrokerProcessHost() {
  CancelRequests();
}

bool PpapiBrokerProcessHost::Init(const PepperPluginInfo& info) {
  broker_path_ = info.path;
  set_name(UTF8ToUTF16(info.name));
  set_version(UTF8ToUTF16(info.version));

  if (!CreateChannel())
    return false;

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  // Use the same launcher mechanism as ppapi plugins.
  CommandLine::StringType plugin_launcher =
      browser_command_line.GetSwitchValueNative(switches::kPpapiPluginLauncher);

#if defined(OS_LINUX)
  int flags = plugin_launcher.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                        ChildProcessHost::CHILD_NORMAL;
#else
  int flags = ChildProcessHost::CHILD_NORMAL;
#endif

  FilePath exe_path = ChildProcessHost::GetChildPath(flags);
  if (exe_path.empty())
    return false;

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kPpapiBrokerProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id());

  if (!plugin_launcher.empty())
    cmd_line->PrependWrapper(plugin_launcher);

  // On posix, having a plugin launcher means we need to use another process
  // instead of just forking the zygote.
  Launch(
#if defined(OS_WIN)
      FilePath(),
#elif defined(OS_POSIX)
      false,  // Never use the zygote for the broker.
      base::environment_vector(),
#endif
      cmd_line);
  return true;
}

void PpapiBrokerProcessHost::OpenChannelToPpapiBroker(Client* client) {
  if (opening_channel()) {
    // The channel is already in the process of being opened.  Put
    // this "open channel" request into a queue of requests that will
    // be run once the channel is open.
    pending_requests_.push_back(client);
    return;
  }

  // We already have an open channel, send a request right away to broker.
  RequestPpapiBrokerChannel(client);
}

void PpapiBrokerProcessHost::RequestPpapiBrokerChannel(
    Client* client) {
  base::ProcessHandle process_handle;
  int renderer_id;
  client->GetChannelInfo(&process_handle, &renderer_id);

  // We can't send any sync messages from the browser because it might lead to
  // a hang. See the similar code in PluginProcessHost for more description.
  PpapiMsg_CreateChannel* msg = new PpapiMsg_CreateChannel(process_handle,
                                                           renderer_id);
  msg->set_unblock(true);
  if (Send(msg))
    sent_requests_.push(client);
  else
    client->OnChannelOpened(base::kNullProcessHandle, IPC::ChannelHandle());
}

bool PpapiBrokerProcessHost::CanShutdown() {
  return true;
}

void PpapiBrokerProcessHost::OnProcessLaunched() {
}

bool PpapiBrokerProcessHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PpapiBrokerProcessHost, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ChannelCreated,
                        OnRendererPpapiBrokerChannelCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

// Called when the browser <--> ppapi broker channel has been established.
void PpapiBrokerProcessHost::OnChannelConnected(int32 peer_pid) {
  // This will actually load the ppapi broker. Errors will actually not be
  // reported back at this point. Instead, the broker will fail to establish the
  // connections when we request them on behalf of the renderer(s).
  Send(new PpapiMsg_LoadPlugin(broker_path_));

  // Process all pending channel requests from the renderers.
  for (size_t i = 0; i < pending_requests_.size(); i++)
    RequestPpapiBrokerChannel(pending_requests_[i]);
  pending_requests_.clear();
}

// Called when the browser <--> broker channel has an error. This normally
// means the broker has crashed.
void PpapiBrokerProcessHost::OnChannelError() {
  // We don't need to notify the renderers that were communicating with the
  // broker since they have their own channels which will go into the error
  // state at the same time. Instead, we just need to notify any renderers
  // that have requested a connection but have not yet received one.
  CancelRequests();
}

void PpapiBrokerProcessHost::CancelRequests() {
  for (size_t i = 0; i < pending_requests_.size(); i++) {
    pending_requests_[i]->OnChannelOpened(base::kNullProcessHandle,
                                          IPC::ChannelHandle());
  }
  pending_requests_.clear();

  while (!sent_requests_.empty()) {
    sent_requests_.front()->OnChannelOpened(base::kNullProcessHandle,
                                            IPC::ChannelHandle());
    sent_requests_.pop();
  }
}

// Called when a new broker <--> renderer channel has been created.
void PpapiBrokerProcessHost::OnRendererPpapiBrokerChannelCreated(
    const IPC::ChannelHandle& channel_handle) {
  if (sent_requests_.empty())
    return;

  // All requests should be processed FIFO, so the next item in the
  // sent_requests_ queue should be the one that the plugin just created.
  Client* client = sent_requests_.front();
  sent_requests_.pop();

  // Prepare the handle to send to the renderer.
  base::ProcessHandle broker_process = GetChildProcessHandle();
#if defined(OS_WIN)
  base::ProcessHandle renderer_process;
  int renderer_id;
  client->GetChannelInfo(&renderer_process, &renderer_id);

  base::ProcessHandle renderers_broker_handle = NULL;
  ::DuplicateHandle(::GetCurrentProcess(), broker_process,
                    renderer_process, &renderers_broker_handle,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
#elif defined(OS_POSIX)
  // Don't need to duplicate anything on POSIX since it's just a PID.
  base::ProcessHandle renderers_broker_handle = broker_process;
#endif

  client->OnChannelOpened(renderers_broker_handle, channel_handle);
}
