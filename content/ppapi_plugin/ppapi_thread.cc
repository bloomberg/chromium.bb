// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_thread.h"

#include <limits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/common/child_process.h"
#include "content/common/child_process_messages.h"
#include "content/ppapi_plugin/broker_process_dispatcher.h"
#include "content/ppapi_plugin/plugin_process_dispatcher.h"
#include "content/ppapi_plugin/ppapi_webkitplatformsupport_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/plugin/content_plugin_client.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ppapi/c/dev/ppp_network_state_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/interface_list.h"
#include "ppapi/shared_impl/api_id.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "ui/base/ui_base_switches.h"
#include "webkit/plugins/plugin_switches.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "sandbox/win/src/sandbox.h"
#elif defined(OS_MACOSX)
#include "content/common/sandbox_init_mac.h"
#endif

#if defined(OS_WIN)
extern sandbox::TargetServices* g_target_services;
#else
extern void* g_target_services;
#endif

namespace content {

typedef int32_t (*InitializeBrokerFunc)
    (PP_ConnectInstance_Func* connect_instance_func);

PpapiThread::DispatcherMessageListener::DispatcherMessageListener(
    PpapiThread* owner) : owner_(owner) {
}

PpapiThread::DispatcherMessageListener::~DispatcherMessageListener() {
}

bool PpapiThread::DispatcherMessageListener::OnMessageReceived(
    const IPC::Message& msg) {
  // The first parameter should be a plugin dispatcher ID.
  PickleIterator iter(msg);
  uint32 id = 0;
  if (!msg.ReadUInt32(&iter, &id)) {
    NOTREACHED();
    return false;
  }
  std::map<uint32, ppapi::proxy::PluginDispatcher*>::iterator dispatcher =
      owner_->plugin_dispatchers_.find(id);
  if (dispatcher != owner_->plugin_dispatchers_.end())
    return dispatcher->second->OnMessageReceived(msg);

  return false;
}

PpapiThread::PpapiThread(const CommandLine& command_line, bool is_broker)
    : is_broker_(is_broker),
      connect_instance_func_(NULL),
      local_pp_module_(
          base::RandInt(0, std::numeric_limits<PP_Module>::max())),
      next_plugin_dispatcher_id_(1),
      ALLOW_THIS_IN_INITIALIZER_LIST(dispatcher_message_listener_(this)) {
  ppapi::proxy::PluginGlobals* globals = ppapi::proxy::PluginGlobals::Get();
  globals->set_plugin_proxy_delegate(this);
  globals->set_command_line(
      command_line.GetSwitchValueASCII(switches::kPpapiFlashArgs));
  if (command_line.HasSwitch(switches::kDisablePepperThreading)) {
    globals->set_enable_threading(false);
  } else if (command_line.HasSwitch(switches::kEnablePepperThreading)) {
    globals->set_enable_threading(true);
  } else {
    globals->set_enable_threading(false);
  }

  webkit_platform_support_.reset(new PpapiWebKitPlatformSupportImpl);
  WebKit::initialize(webkit_platform_support_.get());

  // Register interfaces that expect messages from the browser process. Please
  // note that only those InterfaceProxy-based ones require registration.
  AddRoute(ppapi::API_ID_PPB_TCPSERVERSOCKET_PRIVATE,
           &dispatcher_message_listener_);
  AddRoute(ppapi::API_ID_PPB_TCPSOCKET_PRIVATE,
           &dispatcher_message_listener_);
  AddRoute(ppapi::API_ID_PPB_UDPSOCKET_PRIVATE,
           &dispatcher_message_listener_);
  AddRoute(ppapi::API_ID_PPB_HOSTRESOLVER_PRIVATE,
           &dispatcher_message_listener_);
  AddRoute(ppapi::API_ID_PPB_NETWORKMANAGER_PRIVATE,
           &dispatcher_message_listener_);
}

PpapiThread::~PpapiThread() {
  ppapi::proxy::PluginGlobals::Get()->set_plugin_proxy_delegate(NULL);
  if (plugin_entry_points_.shutdown_module)
    plugin_entry_points_.shutdown_module();
  WebKit::shutdown();

#if defined(OS_WIN)
  if (permissions_.HasPermission(ppapi::PERMISSION_FLASH))
    base::win::SetShouldCrashOnProcessDetach(false);
#endif
}

bool PpapiThread::Send(IPC::Message* msg) {
  // Allow access from multiple threads.
  if (MessageLoop::current() == message_loop())
    return ChildThread::Send(msg);

  return sync_message_filter()->Send(msg);
}

// Note that this function is called only for messages from the channel to the
// browser process. Messages from the renderer process are sent via a different
// channel that ends up at Dispatcher::OnMessageReceived.
bool PpapiThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PpapiThread, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_LoadPlugin, OnLoadPlugin)
    IPC_MESSAGE_HANDLER(PpapiMsg_CreateChannel, OnCreateChannel)
    IPC_MESSAGE_HANDLER(PpapiMsg_SetNetworkState, OnSetNetworkState)
    IPC_MESSAGE_HANDLER(PpapiPluginMsg_ResourceReply, OnResourceReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PpapiThread::OnChannelConnected(int32 peer_pid) {
  ChildThread::OnChannelConnected(peer_pid);
#if defined(OS_WIN)
  if (is_broker_)
    peer_handle_.Set(::OpenProcess(PROCESS_DUP_HANDLE, FALSE, peer_pid));
#endif
}

base::MessageLoopProxy* PpapiThread::GetIPCMessageLoop() {
  return ChildProcess::current()->io_message_loop_proxy();
}

base::WaitableEvent* PpapiThread::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

IPC::PlatformFileForTransit PpapiThread::ShareHandleWithRemote(
    base::PlatformFile handle,
    base::ProcessId peer_pid,
    bool should_close_source) {
#if defined(OS_WIN)
  if (peer_handle_.IsValid()) {
    DCHECK(is_broker_);
    return IPC::GetFileHandleForProcess(handle, peer_handle_,
                                        should_close_source);
  }
#endif

  DCHECK(peer_pid != base::kNullProcessId);
  return BrokerGetFileHandleForProcess(handle, peer_pid, should_close_source);
}

std::set<PP_Instance>* PpapiThread::GetGloballySeenInstanceIDSet() {
  return &globally_seen_instance_ids_;
}

IPC::Sender* PpapiThread::GetBrowserSender() {
  return this;
}

std::string PpapiThread::GetUILanguage() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->GetSwitchValueASCII(switches::kLang);
}

void PpapiThread::PreCacheFont(const void* logfontw) {
#if defined(OS_WIN)
  Send(new ChildProcessHostMsg_PreCacheFont(
      *static_cast<const LOGFONTW*>(logfontw)));
#endif
}

void PpapiThread::SetActiveURL(const std::string& url) {
  GetContentClient()->SetActiveURL(GURL(url));
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

void PpapiThread::OnLoadPlugin(const FilePath& path,
                               const ppapi::PpapiPermissions& permissions) {
  SavePluginName(path);

  // This must be set before calling into the plugin so it can get the
  // interfaces it has permission for.
  ppapi::proxy::InterfaceList::SetProcessGlobalPermissions(permissions);
  permissions_ = permissions;

  // Trusted Pepper plugins may be "internal", i.e. built-in to the browser
  // binary.  If we're being asked to load such a plugin (e.g. the Chromoting
  // client) then fetch the entry points from the embedder, rather than a DLL.
  std::vector<PepperPluginInfo> plugins;
  GetContentClient()->AddPepperPlugins(&plugins);
  for (size_t i = 0; i < plugins.size(); ++i) {
    if (plugins[i].is_internal && plugins[i].path == path) {
      // An internal plugin is being loaded, so fetch the entry points.
      plugin_entry_points_ = plugins[i].internal_entry_points;
    }
  }

  // If the plugin isn't internal then load it from |path|.
  base::ScopedNativeLibrary library;
  if (plugin_entry_points_.initialize_module == NULL) {
    // Load the plugin from the specified library.
    std::string error;
    library.Reset(base::LoadNativeLibrary(path, &error));
    if (!library.is_valid()) {
      LOG(ERROR) << "Failed to load Pepper module from "
        << path.value() << " (error: " << error << ")";
      return;
    }

    // Get the GetInterface function (required).
    plugin_entry_points_.get_interface =
        reinterpret_cast<PP_GetInterface_Func>(
            library.GetFunctionPointer("PPP_GetInterface"));
    if (!plugin_entry_points_.get_interface) {
      LOG(WARNING) << "No PPP_GetInterface in plugin library";
      return;
    }

    // The ShutdownModule/ShutdownBroker function is optional.
    plugin_entry_points_.shutdown_module =
        is_broker_ ?
        reinterpret_cast<PP_ShutdownModule_Func>(
            library.GetFunctionPointer("PPP_ShutdownBroker")) :
        reinterpret_cast<PP_ShutdownModule_Func>(
            library.GetFunctionPointer("PPP_ShutdownModule"));

    if (!is_broker_) {
      // Get the InitializeModule function (required for non-broker code).
      plugin_entry_points_.initialize_module =
          reinterpret_cast<PP_InitializeModule_Func>(
              library.GetFunctionPointer("PPP_InitializeModule"));
      if (!plugin_entry_points_.initialize_module) {
        LOG(WARNING) << "No PPP_InitializeModule in plugin library";
        return;
      }
    }
  }

#if defined(OS_WIN)
  // If code subsequently tries to exit using abort(), force a crash (since
  // otherwise these would be silent terminations and fly under the radar).
  base::win::SetAbortBehaviorForCrashReporting();
  if (permissions.HasPermission(ppapi::PERMISSION_FLASH)) {
    // Force a crash for exit(), _exit(), or ExitProcess(), but only do that for
    // Pepper Flash.
    base::win::SetShouldCrashOnProcessDetach(true);
  }

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
#if defined(OS_MACOSX)
    // We need to do this after getting |PPP_GetInterface()| (or presumably
    // doing something nontrivial with the library), else the sandbox
    // intercedes.
    CHECK(InitializeSandbox());
#endif

    int32_t init_error = plugin_entry_points_.initialize_module(
        local_pp_module_,
        &ppapi::proxy::PluginDispatcher::GetBrowserInterface);
    if (init_error != PP_OK) {
      LOG(WARNING) << "InitModule failed with error " << init_error;
      return;
    }
  }

  // Initialization succeeded, so keep the plugin DLL loaded.
  library_.Reset(library.Release());
}

void PpapiThread::OnCreateChannel(base::ProcessId renderer_pid,
                                  int renderer_child_id,
                                  bool incognito) {
  IPC::ChannelHandle channel_handle;

  if (!plugin_entry_points_.get_interface ||  // Plugin couldn't be loaded.
      !SetupRendererChannel(renderer_pid, renderer_child_id, incognito,
                            &channel_handle)) {
    Send(new PpapiHostMsg_ChannelCreated(IPC::ChannelHandle()));
    return;
  }

  Send(new PpapiHostMsg_ChannelCreated(channel_handle));
}

void PpapiThread::OnResourceReply(
    const ppapi::proxy::ResourceMessageReplyParams& reply_params,
    const IPC::Message& nested_msg) {
  ppapi::proxy::PluginDispatcher::DispatchResourceReply(reply_params,
                                                        nested_msg);
}

void PpapiThread::OnSetNetworkState(bool online) {
  // Note the browser-process side shouldn't send us these messages in the
  // first unless the plugin has dev permissions, so we don't need to check
  // again here. We don't want random plugins depending on this dev interface.
  if (!plugin_entry_points_.get_interface)
    return;
  const PPP_NetworkState_Dev* ns = static_cast<const PPP_NetworkState_Dev*>(
      plugin_entry_points_.get_interface(PPP_NETWORK_STATE_DEV_INTERFACE));
  if (ns)
    ns->SetOnLine(PP_FromBool(online));
}

bool PpapiThread::SetupRendererChannel(base::ProcessId renderer_pid,
                                       int renderer_child_id,
                                       bool incognito,
                                       IPC::ChannelHandle* handle) {
  DCHECK(is_broker_ == (connect_instance_func_ != NULL));
  IPC::ChannelHandle plugin_handle;
  plugin_handle.name = IPC::Channel::GenerateVerifiedChannelID(
      StringPrintf("%d.r%d", base::GetCurrentProcId(), renderer_child_id));

  ppapi::proxy::ProxyChannel* dispatcher = NULL;
  bool init_result = false;
  if (is_broker_) {
    BrokerProcessDispatcher* broker_dispatcher =
        new BrokerProcessDispatcher(plugin_entry_points_.get_interface,
                                    connect_instance_func_);
    init_result = broker_dispatcher->InitBrokerWithChannel(this,
                                                           renderer_pid,
                                                           plugin_handle,
                                                           false);
    dispatcher = broker_dispatcher;
  } else {
    PluginProcessDispatcher* plugin_dispatcher =
        new PluginProcessDispatcher(plugin_entry_points_.get_interface,
                                    permissions_,
                                    incognito);
    init_result = plugin_dispatcher->InitPluginWithChannel(this,
                                                           renderer_pid,
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

void PpapiThread::SavePluginName(const FilePath& path) {
  ppapi::proxy::PluginGlobals::Get()->set_plugin_name(
      path.BaseName().AsUTF8Unsafe());

  // plugin() is NULL when in-process.  Which is fine, because this is
  // just a hook for setting the process name.
  if (GetContentClient()->plugin()) {
    GetContentClient()->plugin()->PluginProcessStarted(
        path.BaseName().RemoveExtension().LossyDisplayName());
  }
}

}  // namespace content
