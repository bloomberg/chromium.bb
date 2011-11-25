// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_thread.h"

#include <limits>

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "content/common/child_process.h"
#include "content/ppapi_plugin/broker_process_dispatcher.h"
#include "content/ppapi_plugin/plugin_process_dispatcher.h"
#include "content/ppapi_plugin/ppapi_webkit_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/dev/ppp_network_state_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/interface_list.h"
#include "webkit/plugins/ppapi/webkit_forwarding_impl.h"

#if defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#elif defined(OS_MACOSX)
#include "content/common/sandbox_init_mac.h"
#endif

#if defined(OS_WIN)
extern sandbox::TargetServices* g_target_services;
#else
extern void* g_target_services;
#endif

typedef int32_t (*InitializeBrokerFunc)
    (PP_ConnectInstance_Func* connect_instance_func);

PpapiThread::PpapiThread(bool is_broker)
    : is_broker_(is_broker),
      get_plugin_interface_(NULL),
      connect_instance_func_(NULL),
      local_pp_module_(
          base::RandInt(0, std::numeric_limits<PP_Module>::max())),
      next_plugin_dispatcher_id_(1) {
}

PpapiThread::~PpapiThread() {
  if (!library_.is_valid())
    return;

  // The ShutdownModule/ShutdownBroker function is optional.
  ppapi::proxy::ProxyChannel::ShutdownModuleFunc shutdown_function =
      is_broker_ ?
      reinterpret_cast<ppapi::proxy::ProxyChannel::ShutdownModuleFunc>(
          library_.GetFunctionPointer("PPP_ShutdownBroker")) :
      reinterpret_cast<ppapi::proxy::ProxyChannel::ShutdownModuleFunc>(
          library_.GetFunctionPointer("PPP_ShutdownModule"));
  if (shutdown_function)
    shutdown_function();
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
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBTCPSocket_ConnectACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBTCPSocket_SSLHandshakeACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBTCPSocket_ReadACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBTCPSocket_WriteACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBUDPSocket_RecvFromACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBUDPSocket_SendToACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER_GENERIC(PpapiMsg_PPBUDPSocket_BindACK,
                                OnPluginDispatcherMessageReceived(msg))
    IPC_MESSAGE_HANDLER(PpapiMsg_SetNetworkState, OnMsgSetNetworkState)
  IPC_END_MESSAGE_MAP()
  return true;
}

base::MessageLoopProxy* PpapiThread::GetIPCMessageLoop() {
  return ChildProcess::current()->io_message_loop_proxy();
}

base::WaitableEvent* PpapiThread::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

std::set<PP_Instance>* PpapiThread::GetGloballySeenInstanceIDSet() {
  return &globally_seen_instance_ids_;
}

ppapi::WebKitForwarding* PpapiThread::GetWebKitForwarding() {
  if (!webkit_forwarding_.get())
    webkit_forwarding_.reset(new webkit::ppapi::WebKitForwardingImpl);
  return webkit_forwarding_.get();
}

void PpapiThread::PostToWebKitThread(const tracked_objects::Location& from_here,
                                     const base::Closure& task) {
  if (!webkit_thread_.get())
    webkit_thread_.reset(new PpapiWebKitThread);
  webkit_thread_->PostTask(from_here, task);
}

bool PpapiThread::SendToBrowser(IPC::Message* msg) {
  return Send(msg);
}

uint32 PpapiThread::Register(ppapi::proxy::PluginDispatcher* plugin_dispatcher) {
  if (!plugin_dispatcher ||
      plugin_dispatchers_.size() >= std::numeric_limits<uint32>::max()) {
    return 0;
  }

  uint32 id = 0;
  do {
    // Although it is unlikely, make sure that we won't cause any trouble when
    // the counter overflows.
    id = next_plugin_dispatcher_id_++;
  } while (id == 0 ||
           plugin_dispatchers_.find(id) != plugin_dispatchers_.end());
  plugin_dispatchers_[id] = plugin_dispatcher;
  return id;
}

void PpapiThread::Unregister(uint32 plugin_dispatcher_id) {
  plugin_dispatchers_.erase(plugin_dispatcher_id);
}

void PpapiThread::OnMsgLoadPlugin(const FilePath& path) {
  std::string error;
  base::ScopedNativeLibrary library(base::LoadNativeLibrary(path, &error));

#if defined(OS_WIN)
  // Once we lower the token the sandbox is locked down and no new modules
  // can be loaded. TODO(cpu): consider changing to the loading style of
  // regular plugins.
  if (g_target_services) {
    // Cause advapi32 to load before the sandbox is turned on.
    unsigned int dummy_rand;
    rand_s(&dummy_rand);
    // Warm up language subsystems before the sandbox is turned on.
    ::GetUserDefaultLangID();
    ::GetUserDefaultLCID();

    g_target_services->LowerToken();
  }
#endif

  if (!library.is_valid()) {
    LOG(ERROR) << "Failed to load Pepper module from "
      << path.value() << " (error: " << error << ")";
    return;
  }

  if (is_broker_) {
    // Get the InitializeBroker function (required).
    InitializeBrokerFunc init_broker =
        reinterpret_cast<InitializeBrokerFunc>(
            library.GetFunctionPointer("PPP_InitializeBroker"));
    if (!init_broker) {
      LOG(WARNING) << "No PPP_InitializeBroker in plugin library";
      return;
    }

    int32_t init_error = init_broker(&connect_instance_func_);
    if (init_error != PP_OK) {
      LOG(WARNING) << "InitBroker failed with error " << init_error;
      return;
    }
    if (!connect_instance_func_) {
      LOG(WARNING) << "InitBroker did not provide PP_ConnectInstance_Func";
      return;
    }
  } else {
    // Get the GetInterface function (required).
    get_plugin_interface_ =
        reinterpret_cast<ppapi::proxy::Dispatcher::GetInterfaceFunc>(
            library.GetFunctionPointer("PPP_GetInterface"));
    if (!get_plugin_interface_) {
      LOG(WARNING) << "No PPP_GetInterface in plugin library";
      return;
    }

#if defined(OS_MACOSX)
    // We need to do this after getting |PPP_GetInterface()| (or presumably
    // doing something nontrivial with the library), else the sandbox
    // intercedes.
    if (!content::InitializeSandbox()) {
      LOG(WARNING) << "Failed to initialize sandbox";
    }
#endif

    // Get the InitializeModule function (required).
    ppapi::proxy::Dispatcher::InitModuleFunc init_module =
        reinterpret_cast<ppapi::proxy::Dispatcher::InitModuleFunc>(
            library.GetFunctionPointer("PPP_InitializeModule"));
    if (!init_module) {
      LOG(WARNING) << "No PPP_InitializeModule in plugin library";
      return;
    }
    int32_t init_error = init_module(
        local_pp_module_,
        &ppapi::proxy::PluginDispatcher::GetBrowserInterface);
    if (init_error != PP_OK) {
      LOG(WARNING) << "InitModule failed with error " << init_error;
      return;
    }
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

void PpapiThread::OnMsgSetNetworkState(bool online) {
  if (!get_plugin_interface_)
    return;
  const PPP_NetworkState_Dev* ns = static_cast<const PPP_NetworkState_Dev*>(
      get_plugin_interface_(PPP_NETWORK_STATE_DEV_INTERFACE));
  if (ns)
    ns->SetOnLine(PP_FromBool(online));
}

void PpapiThread::OnPluginDispatcherMessageReceived(const IPC::Message& msg) {
  // The first parameter should be a plugin dispatcher ID.
  void* iter = NULL;
  uint32 id = 0;
  if (!msg.ReadUInt32(&iter, &id)) {
    NOTREACHED();
    return;
  }
  std::map<uint32, ppapi::proxy::PluginDispatcher*>::iterator dispatcher =
      plugin_dispatchers_.find(id);
  if (dispatcher != plugin_dispatchers_.end())
    dispatcher->second->OnMessageReceived(msg);
}

bool PpapiThread::SetupRendererChannel(base::ProcessHandle host_process_handle,
                                       int renderer_id,
                                       IPC::ChannelHandle* handle) {
  DCHECK(is_broker_ == (connect_instance_func_ != NULL));
  DCHECK(is_broker_ == (get_plugin_interface_ == NULL));
  IPC::ChannelHandle plugin_handle;
  plugin_handle.name = StringPrintf("%d.r%d", base::GetCurrentProcId(),
                                    renderer_id);

  ppapi::proxy::ProxyChannel* dispatcher = NULL;
  bool init_result = false;
  if (is_broker_) {
    BrokerProcessDispatcher* broker_dispatcher =
        new BrokerProcessDispatcher(host_process_handle,
                                    connect_instance_func_);
    init_result = broker_dispatcher->InitBrokerWithChannel(this,
                                                           plugin_handle,
                                                           false);
    dispatcher = broker_dispatcher;
  } else {
    PluginProcessDispatcher* plugin_dispatcher =
        new PluginProcessDispatcher(host_process_handle, get_plugin_interface_);
    init_result = plugin_dispatcher->InitPluginWithChannel(this,
                                                           plugin_handle,
                                                           false);
    dispatcher = plugin_dispatcher;
  }

  if (!init_result) {
    delete dispatcher;
    return false;
  }

  handle->name = plugin_handle.name;
#if defined(OS_POSIX)
  // On POSIX, transfer ownership of the renderer-side (client) FD.
  // This ensures this process will be notified when it is closed even if a
  // connection is not established.
  handle->socket = base::FileDescriptor(dispatcher->TakeRendererFD(), true);
  if (handle->socket.fd == -1)
    return false;
#endif

  // From here, the dispatcher will manage its own lifetime according to the
  // lifetime of the attached channel.
  return true;
}
