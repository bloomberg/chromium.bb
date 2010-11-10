// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/ppapi_plugin/ppapi_thread.h"

#include "base/process_util.h"
#include "chrome/common/child_process.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"

#if defined(OS_POSIX)
#include "base/eintr_wrapper.h"
#include "ipc/ipc_channel_posix.h"
#endif

PpapiThread::PpapiThread()
#if defined(OS_POSIX)
    : renderer_fd_(-1)
#endif
    {
}

PpapiThread::~PpapiThread() {
  pp::proxy::PluginDispatcher::SetGlobal(NULL);
}

// The "regular" ChildThread implements this function and does some standard
// dispatching, then uses the message router. We don't actually need any of
// this so this function just overrides that one.
//
// Note that this function is called only for messages from the channel to the
// browser process. Messages from the renderer process are sent via a different
// channel that ends up at Dispatcher::OnMessageReceived.
void PpapiThread::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PpapiThread, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_LoadPlugin, OnLoadPlugin)

    // The rest of the messages go to the dispatcher.
    /*IPC_MESSAGE_UNHANDLED(
      if (dispatcher_.get())
        dispatcher_->OnMessageReceived(msg)
    )*/
  IPC_END_MESSAGE_MAP()
}

void PpapiThread::OnLoadPlugin(const FilePath& path, int renderer_id) {
  IPC::ChannelHandle channel_handle;
  if (!LoadPluginLib(path) ||
      !SetupRendererChannel(renderer_id, &channel_handle)) {
    // An empty channel handle indicates error.
    Send(new PpapiHostMsg_PluginLoaded(IPC::ChannelHandle()));
    return;
  }

  Send(new PpapiHostMsg_PluginLoaded(channel_handle));
}

bool PpapiThread::LoadPluginLib(const FilePath& path) {
  base::ScopedNativeLibrary library(base::LoadNativeLibrary(path));
  if (!library.is_valid())
    return false;

  // Get the GetInterface function (required).
  pp::proxy::Dispatcher::GetInterfaceFunc get_interface =
      reinterpret_cast<pp::proxy::Dispatcher::GetInterfaceFunc>(
          library.GetFunctionPointer("PPP_GetInterface"));
  if (!get_interface) {
    LOG(WARNING) << "No PPP_GetInterface in plugin library";
    return false;
  }

  // Get the InitializeModule function (required).
  pp::proxy::Dispatcher::InitModuleFunc init_module =
      reinterpret_cast<pp::proxy::Dispatcher::InitModuleFunc>(
          library.GetFunctionPointer("PPP_InitializeModule"));
  if (!init_module) {
    LOG(WARNING) << "No PPP_InitializeModule in plugin library";
    return false;
  }

  // Get the ShutdownModule function (optional).
  pp::proxy::Dispatcher::ShutdownModuleFunc shutdown_module =
      reinterpret_cast<pp::proxy::Dispatcher::ShutdownModuleFunc>(
          library.GetFunctionPointer("PPP_ShutdownModule"));

  library_.Reset(library.Release());
  dispatcher_.reset(new pp::proxy::PluginDispatcher(get_interface, init_module,
                                                    shutdown_module));
  pp::proxy::PluginDispatcher::SetGlobal(dispatcher_.get());
  return true;
}

bool PpapiThread::SetupRendererChannel(int renderer_id,
                                       IPC::ChannelHandle* handle) {
  std::string channel_key = StringPrintf(
      "%d.r%d", base::GetCurrentProcId(), renderer_id);

#if defined(OS_POSIX)
  // This gets called when the PluginChannel is initially created. At this
  // point, create the socketpair and assign the plugin side FD to the channel
  // name. Keep the renderer side FD as a member variable in the PluginChannel
  // to be able to transmit it through IPC.
  int plugin_fd;
  if (!IPC::SocketPair(&plugin_fd, &renderer_fd_))
    return false;
  IPC::AddChannelSocket(channel_key, plugin_fd);
#endif

  if (!dispatcher_->InitWithChannel(
          ChildProcess::current()->io_message_loop(),
          channel_key, false,
          ChildProcess::current()->GetShutDownEvent()))
    return false;

  handle->name = channel_key;
#if defined(OS_POSIX)
  // On POSIX, pass the renderer-side FD.
  handle->socket = base::FileDescriptor(renderer_fd_, false);
#endif
  return true;
}

#if defined(OS_POSIX)
void PpapiThread::CloseRendererFD() {
  if (renderer_fd_ != -1) {
    if (HANDLE_EINTR(close(renderer_fd_)) < 0)
      PLOG(ERROR) << "close";
    renderer_fd_ = -1;
  }
}
#endif
