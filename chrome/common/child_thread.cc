// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_thread.h"

#include "base/string_util.h"
#include "base/command_line.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/socket_stream_dispatcher.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_switches.h"
#include "webkit/glue/webkit_glue.h"


ChildThread::ChildThread() {
  channel_name_ = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kProcessChannelID);
  Init();
}

ChildThread::ChildThread(const std::string& channel_name)
    : channel_name_(channel_name) {
  Init();
}

void ChildThread::Init() {
  check_with_browser_before_shutdown_ = false;
  message_loop_ = MessageLoop::current();
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserAgent)) {
    webkit_glue::SetUserAgent(
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kUserAgent));
  }

  channel_.reset(new IPC::SyncChannel(channel_name_,
      IPC::Channel::MODE_CLIENT, this, NULL,
      ChildProcess::current()->io_message_loop(), true,
      ChildProcess::current()->GetShutDownEvent()));
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::current()->SetIPCSender(this);
#endif

  resource_dispatcher_.reset(new ResourceDispatcher(this));
  socket_stream_dispatcher_.reset(new SocketStreamDispatcher());

  // When running in unit tests, there is already a NotificationService object.
  // Since only one can exist at a time per thread, check first.
  if (!NotificationService::current())
    notification_service_.reset(new NotificationService);
}

ChildThread::~ChildThread() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::current()->SetIPCSender(NULL);
#endif

  // The ChannelProxy object caches a pointer to the IPC thread, so need to
  // reset it as it's not guaranteed to outlive this object.
  // NOTE: this also has the side-effect of not closing the main IPC channel to
  // the browser process.  This is needed because this is the signal that the
  // browser uses to know that this process has died, so we need it to be alive
  // until this process is shut down, and the OS closes the handle
  // automatically.  We used to watch the object handle on Windows to do this,
  // but it wasn't possible to do so on POSIX.
  channel_->ClearIPCMessageLoop();
}

void ChildThread::OnChannelError() {
  MessageLoop::current()->Quit();
}

bool ChildThread::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }

  return channel_->Send(msg);
}

void ChildThread::AddRoute(int32 routing_id, IPC::Channel::Listener* listener) {
  DCHECK(MessageLoop::current() == message_loop());

  router_.AddRoute(routing_id, listener);
}

void ChildThread::RemoveRoute(int32 routing_id) {
  DCHECK(MessageLoop::current() == message_loop());

  router_.RemoveRoute(routing_id);
}

webkit_glue::ResourceLoaderBridge* ChildThread::CreateBridge(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info,
    int host_renderer_id,
    int host_render_view_id) {
  return resource_dispatcher()->
      CreateBridge(request_info, host_renderer_id, host_render_view_id);
}


void ChildThread::OnMessageReceived(const IPC::Message& msg) {
  // Resource responses are sent to the resource dispatcher.
  if (resource_dispatcher_->OnMessageReceived(msg))
    return;
  if (socket_stream_dispatcher_->OnMessageReceived(msg))
    return;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChildThread, msg)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_AskBeforeShutdown, OnAskBeforeShutdown)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_Shutdown, OnShutdown)
#if defined(IPC_MESSAGE_LOG_ENABLED)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_SetIPCLoggingEnabled,
                        OnSetIPCLoggingEnabled)
#endif  // IPC_MESSAGE_HANDLER
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (handled)
    return;

  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    OnControlMessageReceived(msg);
  } else {
    router_.OnMessageReceived(msg);
  }
}

void ChildThread::OnAskBeforeShutdown() {
  check_with_browser_before_shutdown_ = true;
}

void ChildThread::OnShutdown() {
  MessageLoop::current()->Quit();
}

#if defined(IPC_MESSAGE_LOG_ENABLED)
void ChildThread::OnSetIPCLoggingEnabled(bool enable) {
  if (enable)
    IPC::Logging::current()->Enable();
  else
    IPC::Logging::current()->Disable();
}
#endif  //  IPC_MESSAGE_LOG_ENABLED

ChildThread* ChildThread::current() {
  return ChildProcess::current()->main_thread();
}

void ChildThread::OnProcessFinalRelease() {
  if (!check_with_browser_before_shutdown_) {
    MessageLoop::current()->Quit();
    return;
  }

  // The child process shutdown sequence is a request response based mechanism,
  // where we send out an initial feeler request to the child process host
  // instance in the browser to verify if it's ok to shutdown the child process.
  // The browser then sends back a response if it's ok to shutdown.
  Send(new PluginProcessHostMsg_ShutdownRequest);
}
