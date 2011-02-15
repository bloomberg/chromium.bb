// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/ppapi_plugin/ppapi_thread.h"

#include <limits>

#include "base/process_util.h"
#include "base/rand_util.h"
#include "chrome/common/child_process.h"
#include "chrome/ppapi_plugin/plugin_process_dispatcher.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/ppapi_messages.h"

PpapiThread::PpapiThread()
    : get_plugin_interface_(NULL),
      local_pp_module_(
          base::RandInt(0, std::numeric_limits<PP_Module>::max())) {
}

PpapiThread::~PpapiThread() {
  if (library_.is_valid()) {
    // The ShutdownModule function is optional.
    pp::proxy::Dispatcher::ShutdownModuleFunc shutdown_module =
        reinterpret_cast<pp::proxy::Dispatcher::ShutdownModuleFunc>(
            library_.GetFunctionPointer("PPP_ShutdownModule"));
    if (shutdown_module)
      shutdown_module();
  }
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
    IPC_MESSAGE_HANDLER(PpapiMsg_CreateChannel, OnMsgCreateChannel)
  IPC_END_MESSAGE_MAP()
  return true;
}

void PpapiThread::OnMsgLoadPlugin(const FilePath& path) {
  base::ScopedNativeLibrary library(base::LoadNativeLibrary(path));
  if (!library.is_valid())
    return;

  // Get the GetInterface function (required).
  get_plugin_interface_ =
      reinterpret_cast<pp::proxy::Dispatcher::GetInterfaceFunc>(
          library.GetFunctionPointer("PPP_GetInterface"));
  if (!get_plugin_interface_) {
    LOG(WARNING) << "No PPP_GetInterface in plugin library";
    return;
  }

  // Get the InitializeModule function (required).
  pp::proxy::Dispatcher::InitModuleFunc init_module =
      reinterpret_cast<pp::proxy::Dispatcher::InitModuleFunc>(
          library.GetFunctionPointer("PPP_InitializeModule"));
  if (!init_module) {
    LOG(WARNING) << "No PPP_InitializeModule in plugin library";
    return;
  }
  int32_t init_error = init_module(
      local_pp_module_,
      &pp::proxy::PluginDispatcher::GetInterfaceFromDispatcher);
  if (init_error != PP_OK) {
    LOG(WARNING) << "InitModule failed with error " << init_error;
    return;
  }

  library_.Reset(library.Release());
}

void PpapiThread::OnMsgCreateChannel(base::ProcessHandle host_process_handle,
                                     int renderer_id) {
  IPC::ChannelHandle channel_handle;
  if (!library_.is_valid() ||  // Plugin couldn't be loaded.
      !SetupRendererChannel(host_process_handle, renderer_id,
                            &channel_handle)) {
    Send(new PpapiHostMsg_ChannelCreated(IPC::ChannelHandle()));
    return;
  }

  Send(new PpapiHostMsg_ChannelCreated(channel_handle));
}

bool PpapiThread::SetupRendererChannel(base::ProcessHandle host_process_handle,
                                       int renderer_id,
                                       IPC::ChannelHandle* handle) {
  PluginProcessDispatcher* dispatcher = new PluginProcessDispatcher(
      host_process_handle, get_plugin_interface_);

  IPC::ChannelHandle plugin_handle;
  plugin_handle.name = StringPrintf("%d.r%d", base::GetCurrentProcId(),
                                    renderer_id);
  if (!dispatcher->InitWithChannel(
          ChildProcess::current()->io_message_loop(),
          plugin_handle, false,
          ChildProcess::current()->GetShutDownEvent()))
    return false;

  handle->name = plugin_handle.name;
#if defined(OS_POSIX)
  // On POSIX, pass the renderer-side FD.
  handle->socket = base::FileDescriptor(dispatcher->GetRendererFD(), false);
#endif

  return true;
}

