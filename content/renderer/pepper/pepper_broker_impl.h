// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_BROKER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_BROKER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "content/common/content_export.h"
#include "ppapi/proxy/proxy_channel.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppb_broker_impl.h"

namespace IPC {
struct ChannelHandle;
}

namespace ppapi {
namespace proxy {
class BrokerDispatcher;
}
}

namespace webkit {
namespace ppapi {
class PluginModule;
}
}

namespace content {

class PepperPluginDelegateImpl;

// This object is NOT thread-safe.
class CONTENT_EXPORT PepperBrokerDispatcherWrapper {
 public:
  PepperBrokerDispatcherWrapper();
  ~PepperBrokerDispatcherWrapper();

  bool Init(base::ProcessId broker_pid,
            const IPC::ChannelHandle& channel_handle);

  int32_t SendHandleToBroker(PP_Instance instance,
                             base::SyncSocket::Handle handle);

 private:
  scoped_ptr<ppapi::proxy::BrokerDispatcher> dispatcher_;
  scoped_ptr<ppapi::proxy::ProxyChannel::Delegate> dispatcher_delegate_;
};

class PepperBrokerImpl : public webkit::ppapi::PluginDelegate::Broker,
                         public base::RefCountedThreadSafe<PepperBrokerImpl>{
 public:
  PepperBrokerImpl(webkit::ppapi::PluginModule* plugin_module,
                   PepperPluginDelegateImpl* delegate_);

  // webkit::ppapi::PluginDelegate::Broker implementation.
  virtual void Disconnect(webkit::ppapi::PPB_Broker_Impl* client) OVERRIDE;

  // Adds a pending connection to the broker. Balances out Disconnect() calls.
  void AddPendingConnect(webkit::ppapi::PPB_Broker_Impl* client);

  // Called when the channel to the broker has been established.
  void OnBrokerChannelConnected(base::ProcessId broker_pid,
                                const IPC::ChannelHandle& channel_handle);

  // Called when we know whether permission to access the PPAPI broker was
  // granted.
  void OnBrokerPermissionResult(webkit::ppapi::PPB_Broker_Impl* client,
                                bool result);

 private:
  friend class base::RefCountedThreadSafe<PepperBrokerImpl>;

  struct PendingConnection {
    PendingConnection();
    ~PendingConnection();

    bool is_authorized;
    base::WeakPtr<webkit::ppapi::PPB_Broker_Impl> client;
  };

  virtual ~PepperBrokerImpl();

  // Reports failure to all clients that had pending operations.
  void ReportFailureToClients(int error_code);

  // Connects the plugin to the broker via a pipe.
  void ConnectPluginToBroker(webkit::ppapi::PPB_Broker_Impl* client);

  scoped_ptr<PepperBrokerDispatcherWrapper> dispatcher_;

  // A map of pointers to objects that have requested a connection to the weak
  // pointer we can use to reference them. The mapping is needed so we can clean
  // up entries for objects that may have been deleted.
  typedef std::map<webkit::ppapi::PPB_Broker_Impl*, PendingConnection>
      ClientMap;
  ClientMap pending_connects_;

  // Pointer to the associated plugin module.
  // Always set and cleared at the same time as the module's pointer to this.
  webkit::ppapi::PluginModule* plugin_module_;

  base::WeakPtr<PepperPluginDelegateImpl> delegate_;

  DISALLOW_COPY_AND_ASSIGN(PepperBrokerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_BROKER_IMPL_H_
