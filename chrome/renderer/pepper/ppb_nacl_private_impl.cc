// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/ppb_nacl_private_impl.h"

#ifndef DISABLE_NACL

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_entry_points.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/proxy_channel.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using content::RenderThread;
using content::RenderView;
using webkit::ppapi::HostGlobals;
using webkit::ppapi::PluginInstance;
using webkit::ppapi::PluginDelegate;
using WebKit::WebView;

namespace {

base::LazyInstance<scoped_refptr<IPC::SyncMessageFilter> >
    g_background_thread_sender = LAZY_INSTANCE_INITIALIZER;

// Launch NaCl's sel_ldr process.
PP_Bool LaunchSelLdr(PP_Instance instance,
                     const char* alleged_url,
                     int socket_count,
                     void* imc_handles,
                     void** ipc_channel_handle) {
  std::vector<nacl::FileDescriptor> sockets;
  IPC::Sender* sender = content::RenderThread::Get();
  if (sender == NULL)
    sender = g_background_thread_sender.Pointer()->get();

  scoped_ptr<IPC::ChannelHandle> channel_handle(new IPC::ChannelHandle);
  if (!sender->Send(new ChromeViewHostMsg_LaunchNaCl(
          GURL(alleged_url), socket_count, &sockets,
          channel_handle.get()))) {
    *ipc_channel_handle = NULL;
    return PP_FALSE;
  }

  *ipc_channel_handle = channel_handle.release();

  CHECK(static_cast<int>(sockets.size()) == socket_count);
  for (int i = 0; i < socket_count; i++) {
    static_cast<nacl::Handle*>(imc_handles)[i] =
        nacl::ToNativeHandle(sockets[i]);
  }

  return PP_TRUE;
}

class ProxyChannelDelegate
    : public ppapi::proxy::ProxyChannel::Delegate {
 public:
  ProxyChannelDelegate();
  virtual ~ProxyChannelDelegate();

  // ProxyChannel::Delegate implementation.
  virtual base::MessageLoopProxy* GetIPCMessageLoop() OVERRIDE;
  virtual base::WaitableEvent* GetShutdownEvent() OVERRIDE;
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      const IPC::SyncChannel& channel,
      bool should_close_source) OVERRIDE;
 private:
  // TODO(bbudge) Modify the content public API so we can get
  // the renderer process's shutdown event.
  base::WaitableEvent shutdown_event_;
};

ProxyChannelDelegate::ProxyChannelDelegate()
    : shutdown_event_(true, false) {
}

ProxyChannelDelegate::~ProxyChannelDelegate() {
}

base::MessageLoopProxy* ProxyChannelDelegate::GetIPCMessageLoop() {
  return RenderThread::Get()->GetIOMessageLoopProxy().get();
}

base::WaitableEvent* ProxyChannelDelegate::GetShutdownEvent() {
  return &shutdown_event_;
}

IPC::PlatformFileForTransit ProxyChannelDelegate::ShareHandleWithRemote(
    base::PlatformFile handle,
    const IPC::SyncChannel& channel,
    bool should_close_source) {
  return content::BrokerGetFileHandleForProcess(handle, channel.peer_pid(),
                                                should_close_source);
}

// Stubbed out SyncMessageStatusReceiver, required by HostDispatcher.
// TODO(bbudge) Use a content::PepperHungPluginFilter instead.
class SyncMessageStatusReceiver
    : public ppapi::proxy::HostDispatcher::SyncMessageStatusReceiver {
 public:
  SyncMessageStatusReceiver() {}

  // SyncMessageStatusReceiver implementation.
  virtual void BeginBlockOnSyncMessage() OVERRIDE {}
  virtual void EndBlockOnSyncMessage() OVERRIDE {}

 private:
  virtual ~SyncMessageStatusReceiver() {}
};

class OutOfProcessProxy : public PluginDelegate::OutOfProcessProxy {
 public:
  OutOfProcessProxy() {}
  virtual ~OutOfProcessProxy() {}

  bool Init(const IPC::ChannelHandle& channel_handle,
            PP_Module pp_module,
            PP_GetInterface_Func local_get_interface,
            const ppapi::Preferences& preferences,
            SyncMessageStatusReceiver* status_receiver) {
    if (channel_handle.name.empty())
      return false;

#if defined(OS_POSIX)
    DCHECK_NE(-1, channel_handle.socket.fd);
    if (channel_handle.socket.fd == -1)
      return false;
#endif

    dispatcher_delegate_.reset(new ProxyChannelDelegate);
    dispatcher_.reset(new ppapi::proxy::HostDispatcher(
        pp_module, local_get_interface, status_receiver));

    if (!dispatcher_->InitHostWithChannel(dispatcher_delegate_.get(),
                                          channel_handle,
                                          true,  // Client.
                                          preferences)) {
      dispatcher_.reset();
      dispatcher_delegate_.reset();
      return false;
    }

    return true;
  }

  // OutOfProcessProxy implementation.
  virtual const void* GetProxiedInterface(const char* name) OVERRIDE {
    return dispatcher_->GetProxiedInterface(name);
  }
  virtual void AddInstance(PP_Instance instance) OVERRIDE {
    ppapi::proxy::HostDispatcher::SetForInstance(instance, dispatcher_.get());
  }
  virtual void RemoveInstance(PP_Instance instance) OVERRIDE {
    ppapi::proxy::HostDispatcher::RemoveForInstance(instance);
  }

 private:
  scoped_ptr<ppapi::proxy::HostDispatcher> dispatcher_;
  scoped_ptr<ppapi::proxy::ProxyChannel::Delegate> dispatcher_delegate_;
};

PP_Bool StartPpapiProxy(PP_Instance instance,
                        void* ipc_channel_handle) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNaClIPCProxy)) {
    scoped_ptr<IPC::ChannelHandle> channel_handle(
        static_cast<IPC::ChannelHandle*>(ipc_channel_handle));
    if (channel_handle->name.empty())
      return PP_FALSE;

    webkit::ppapi::PluginInstance* plugin_instance =
        content::GetHostGlobals()->GetInstance(instance);
    if (!plugin_instance)
      return PP_FALSE;

    WebView* web_view =
        plugin_instance->container()->element().document().frame()->view();
    RenderView* render_view = content::RenderView::FromWebView(web_view);

    webkit::ppapi::PluginModule* plugin_module = plugin_instance->module();

    scoped_refptr<SyncMessageStatusReceiver>
        status_receiver(new SyncMessageStatusReceiver());
    scoped_ptr<OutOfProcessProxy> out_of_process_proxy(new OutOfProcessProxy);
    if (out_of_process_proxy->Init(
            *channel_handle,
            plugin_module->pp_module(),
            webkit::ppapi::PluginModule::GetLocalGetInterfaceFunc(),
            ppapi::Preferences(render_view->GetWebkitPreferences()),
            status_receiver.get())) {
      plugin_module->InitAsProxiedNaCl(
          out_of_process_proxy.PassAs<PluginDelegate::OutOfProcessProxy>(),
          instance);
      return PP_TRUE;
    }
  }

  return PP_FALSE;
}

int UrandomFD(void) {
#if defined(OS_POSIX)
  return base::GetUrandomFD();
#else
  return -1;
#endif
}

bool Are3DInterfacesDisabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisable3DAPIs);
}

void EnableBackgroundSelLdrLaunch() {
  g_background_thread_sender.Get() =
      content::RenderThread::Get()->GetSyncMessageFilter();
}

int BrokerDuplicateHandle(void* source_handle,
                          unsigned int process_id,
                          void** target_handle,
                          unsigned int desired_access,
                          unsigned int options) {
#if defined(OS_WIN)
  return content::BrokerDuplicateHandle(source_handle, process_id,
                                        target_handle, desired_access,
                                        options);
#else
  return 0;
#endif
}

const PPB_NaCl_Private nacl_interface = {
  &LaunchSelLdr,
  &StartPpapiProxy,
  &UrandomFD,
  &Are3DInterfacesDisabled,
  &EnableBackgroundSelLdrLaunch,
  &BrokerDuplicateHandle,
};

}  // namespace

const PPB_NaCl_Private* PPB_NaCl_Private_Impl::GetInterface() {
  return &nacl_interface;
}

#endif  // DISABLE_NACL
