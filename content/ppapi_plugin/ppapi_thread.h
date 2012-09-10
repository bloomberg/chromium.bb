// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_
#define CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/scoped_native_library.h"
#include "build/build_config.h"
#include "content/common/child_thread.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/trusted/ppp_broker.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

class CommandLine;
class FilePath;
class PpapiWebKitPlatformSupportImpl;

namespace IPC {
struct ChannelHandle;
}

class PpapiThread : public ChildThread,
                    public ppapi::proxy::PluginDispatcher::PluginDelegate,
                    public ppapi::proxy::PluginProxyDelegate {
 public:
  PpapiThread(const CommandLine& command_line, bool is_broker);
  virtual ~PpapiThread();

 private:
  // ChildThread overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  // PluginDispatcher::PluginDelegate implementation.
  virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() OVERRIDE;
  virtual base::MessageLoopProxy* GetIPCMessageLoop() OVERRIDE;
  virtual base::WaitableEvent* GetShutdownEvent() OVERRIDE;
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      const IPC::SyncChannel& channel,
      bool should_close_source) OVERRIDE;
  virtual uint32 Register(
      ppapi::proxy::PluginDispatcher* plugin_dispatcher) OVERRIDE;
  virtual void Unregister(uint32 plugin_dispatcher_id) OVERRIDE;

  // PluginProxyDelegate.
  // SendToBrowser() is intended to be safe to use on another thread so
  // long as the main PpapiThread outlives it.
  virtual bool SendToBrowser(IPC::Message* msg) OVERRIDE;
  virtual IPC::Sender* GetBrowserSender() OVERRIDE;
  virtual std::string GetUILanguage() OVERRIDE;
  virtual void PreCacheFont(const void* logfontw) OVERRIDE;
  virtual void SetActiveURL(const std::string& url) OVERRIDE;

  // Message handlers.
  void OnMsgLoadPlugin(const FilePath& path);
  void OnMsgCreateChannel(int renderer_id,
                          bool incognito);
  void OnMsgResourceReply(
      const ppapi::proxy::ResourceMessageReplyParams& reply_params,
      const IPC::Message& nested_msg);
  void OnMsgSetNetworkState(bool online);
  void OnPluginDispatcherMessageReceived(const IPC::Message& msg);

  // Sets up the channel to the given renderer. On success, returns true and
  // fills the given ChannelHandle with the information from the new channel.
  bool SetupRendererChannel(int renderer_id,
                            bool incognito,
                            IPC::ChannelHandle* handle);

  // Sets up the name of the plugin for logging using the given path.
  void SavePluginName(const FilePath& path);

  // True if running in a broker process rather than a normal plugin process.
  bool is_broker_;

  base::ScopedNativeLibrary library_;

  // Global state tracking for the proxy.
  ppapi::proxy::PluginGlobals plugin_globals_;

  PP_GetInterface_Func get_plugin_interface_;

  // Callback to call when a new instance connects to the broker.
  // Used only when is_broker_.
  PP_ConnectInstance_Func connect_instance_func_;

  // Local concept of the module ID. Some functions take this. It's necessary
  // for the in-process PPAPI to handle this properly, but for proxied it's
  // unnecessary. The proxy talking to multiple renderers means that each
  // renderer has a different idea of what the module ID is for this plugin.
  // To force people to "do the right thing" we generate a random module ID
  // and pass it around as necessary.
  PP_Module local_pp_module_;

  // See Dispatcher::Delegate::GetGloballySeenInstanceIDSet.
  std::set<PP_Instance> globally_seen_instance_ids_;

  // The PluginDispatcher instances contained in the map are not owned by it.
  std::map<uint32, ppapi::proxy::PluginDispatcher*> plugin_dispatchers_;
  uint32 next_plugin_dispatcher_id_;

  // The WebKitPlatformSupport implementation.
  scoped_ptr<PpapiWebKitPlatformSupportImpl> webkit_platform_support_;

#if defined(OS_WIN)
  // Caches the handle to the peer process if this is a broker.
  base::win::ScopedHandle peer_handle_;
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(PpapiThread);
};

#endif  // CONTENT_PPAPI_PLUGIN_PPAPI_THREAD_H_
