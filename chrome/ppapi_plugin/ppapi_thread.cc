// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
}

// The "regular" ChildThread implements this function and does some standard
// dispatching, then uses the message router. We don't actually need any of
// this so this function just overrides that one.
//
// Note that this function is called only for messages from the channel to the
// browser process. Messages from the renderer process are sent via a different
// channel that ends up at Dispatcher::OnMessageReceived.
bool PpapiThread::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PpapiThread, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_LoadPlugin, OnMsgLoadPlugin)
  IPC_END_MESSAGE_MAP()
  return true;
}

void PpapiThread::OnMsgLoadPlugin(base::ProcessHandle host_process_handle,
                                  const FilePath& path,
                                  int renderer_id) {
  IPC::ChannelHandle channel_handle;
  if (!LoadPluginLib(host_process_handle, path) ||
      !SetupRendererChannel(renderer_id, &channel_handle)) {
    // An empty channel handle indicates error.
    Send(new PpapiHostMsg_PluginLoaded(IPC::ChannelHandle()));
    return;
  }

  Send(new PpapiHostMsg_PluginLoaded(channel_handle));
}

bool PpapiThread::LoadPluginLib(base::ProcessHandle host_process_handle,
                                const FilePath& path) {
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
  dispatcher_.reset(new pp::proxy::PluginDispatcher(
      host_process_handle, get_interface, init_module, shutdown_module));
  return true;
}

bool PpapiThread::SetupRendererChannel(int renderer_id,
                                       IPC::ChannelHandle* handle) {
  IPC::ChannelHandle plugin_handle;
  plugin_handle.name = StringPrintf("%d.r%d", base::GetCurrentProcId(),
                                    renderer_id);
  if (!dispatcher_->InitWithChannel(
          ChildProcess::current()->io_message_loop(),
          plugin_handle, false,
          ChildProcess::current()->GetShutDownEvent()))
    return false;

  handle->name = plugin_handle.name;
#if defined(OS_POSIX)
  // On POSIX, pass the renderer-side FD.
  renderer_fd_ = dispatcher_->channel()->GetClientFileDescriptor();
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
