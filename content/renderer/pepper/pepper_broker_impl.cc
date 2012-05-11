// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_broker_impl.h"

#include "build/build_config.h"
#include "content/renderer/pepper/pepper_plugin_delegate_impl.h"
#include "content/renderer/pepper/pepper_proxy_channel_delegate_impl.h"
#include "content/renderer/renderer_restrict_dispatch_group.h"
#include "ipc/ipc_channel_handle.h"
#include "ppapi/proxy/broker_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/platform_file.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppb_broker_impl.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace content {

namespace {

base::SyncSocket::Handle DuplicateHandle(base::SyncSocket::Handle handle) {
  base::SyncSocket::Handle out_handle = base::kInvalidPlatformFileValue;
#if defined(OS_WIN)
  DWORD options = DUPLICATE_SAME_ACCESS;
  if (!::DuplicateHandle(::GetCurrentProcess(),
                         handle,
                         ::GetCurrentProcess(),
                         &out_handle,
                         0,
                         FALSE,
                         options)) {
    out_handle = base::kInvalidPlatformFileValue;
  }
#elif defined(OS_POSIX)
  // If asked to close the source, we can simply re-use the source fd instead of
  // dup()ing and close()ing.
  out_handle = ::dup(handle);
#else
  #error Not implemented.
#endif
  return out_handle;
}

}  // namespace

PepperBrokerDispatcherWrapper::PepperBrokerDispatcherWrapper() {
}

PepperBrokerDispatcherWrapper::~PepperBrokerDispatcherWrapper() {
}

bool PepperBrokerDispatcherWrapper::Init(
    const IPC::ChannelHandle& channel_handle) {
  if (channel_handle.name.empty())
    return false;

#if defined(OS_POSIX)
  DCHECK_NE(-1, channel_handle.socket.fd);
  if (channel_handle.socket.fd == -1)
    return false;
#endif

  dispatcher_delegate_.reset(new PepperProxyChannelDelegateImpl);
  dispatcher_.reset(
      new ppapi::proxy::BrokerHostDispatcher());

  if (!dispatcher_->InitBrokerWithChannel(dispatcher_delegate_.get(),
                                          channel_handle,
                                          true)) {  // Client.
    dispatcher_.reset();
    dispatcher_delegate_.reset();
    return false;
  }
  dispatcher_->channel()->SetRestrictDispatchChannelGroup(
      content::kRendererRestrictDispatchGroup_Pepper);
  return true;
}

// Does not take ownership of the local pipe.
int32_t PepperBrokerDispatcherWrapper::SendHandleToBroker(
    PP_Instance instance,
    base::SyncSocket::Handle handle) {
  IPC::PlatformFileForTransit foreign_socket_handle =
      dispatcher_->ShareHandleWithRemote(handle, false);
  if (foreign_socket_handle == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;

  int32_t result;
  if (!dispatcher_->Send(
      new PpapiMsg_ConnectToPlugin(instance, foreign_socket_handle, &result))) {
    // The plugin did not receive the handle, so it must be closed.
    // The easiest way to clean it up is to just put it in an object
    // and then close it. This failure case is not performance critical.
    // The handle could still leak if Send succeeded but the IPC later failed.
    base::SyncSocket temp_socket(
        IPC::PlatformFileForTransitToPlatformFile(foreign_socket_handle));
    return PP_ERROR_FAILED;
  }

  return result;
}

PepperBrokerImpl::PepperBrokerImpl(webkit::ppapi::PluginModule* plugin_module,
                                   PepperPluginDelegateImpl* delegate)
    : plugin_module_(plugin_module),
      delegate_(delegate->AsWeakPtr()) {
  DCHECK(plugin_module_);
  DCHECK(delegate_);

  plugin_module_->SetBroker(this);
}

PepperBrokerImpl::~PepperBrokerImpl() {
  // Report failure to all clients that had pending operations.
  for (ClientMap::iterator i = pending_connects_.begin();
       i != pending_connects_.end(); ++i) {
    base::WeakPtr<webkit::ppapi::PPB_Broker_Impl>& weak_ptr = i->second;
    if (weak_ptr) {
      weak_ptr->BrokerConnected(
          ppapi::PlatformFileToInt(base::kInvalidPlatformFileValue),
          PP_ERROR_ABORTED);
    }
  }
  pending_connects_.clear();

  plugin_module_->SetBroker(NULL);
  plugin_module_ = NULL;
}

// If the channel is not ready, queue the connection.
void PepperBrokerImpl::Connect(webkit::ppapi::PPB_Broker_Impl* client) {
  DCHECK(pending_connects_.find(client) == pending_connects_.end())
      << "Connect was already called for this client";

  // Ensure this object and the associated broker exist as long as the
  // client exists. There is a corresponding Release() call in Disconnect(),
  // which is called when the PPB_Broker_Impl is destroyed. The only other
  // possible reference is in pending_connect_broker_, which only holds a
  // transient reference. This ensures the broker is available as long as the
  // plugin needs it and allows the plugin to release the broker when it is no
  // longer using it.
  AddRef();

  if (!dispatcher_.get()) {
    pending_connects_[client] = client->AsWeakPtr();
    return;
  }
  DCHECK(pending_connects_.empty());

  ConnectPluginToBroker(client);
}

void PepperBrokerImpl::Disconnect(webkit::ppapi::PPB_Broker_Impl* client) {
  // Remove the pending connect if one exists. This class will not call client's
  // callback.
  pending_connects_.erase(client);

  // TODO(ddorwin): Send message disconnect message using dispatcher_.

  if (pending_connects_.empty()) {
    // There are no more clients of this broker. Ensure it will be deleted even
    // if the IPC response never comes and OnPepperBrokerChannelCreated is not
    // called to remove this object from pending_connect_broker_.
    // Before the broker is connected, clients must either be in
    // pending_connects_ or not yet associated with this object. Thus, if this
    // object is in pending_connect_broker_, there can be no associated clients
    // once pending_connects_ is empty and it is thus safe to remove this from
    // pending_connect_broker_. Doing so will cause this object to be deleted,
    // removing it from the PluginModule. Any new clients will create a new
    // instance of this object.
    // This doesn't solve all potential problems, but it helps with the ones
    // we can influence.
    if (delegate_) {
      bool stopped = delegate_->StopWaitingForBrokerConnection(this);

      // Verify the assumption that there are no references other than the one
      // client holds, which will be released below.
      DCHECK(!stopped || HasOneRef());
    }
  }

  // Release the reference added in Connect().
  // This must be the last statement because it may delete this object.
  Release();
}

void PepperBrokerImpl::OnBrokerChannelConnected(
    const IPC::ChannelHandle& channel_handle) {
  scoped_ptr<PepperBrokerDispatcherWrapper> dispatcher(
      new PepperBrokerDispatcherWrapper);
  if (dispatcher->Init(channel_handle)) {
    dispatcher_.reset(dispatcher.release());

    // Process all pending channel requests from the plugins.
    for (ClientMap::iterator i = pending_connects_.begin();
         i != pending_connects_.end(); ++i) {
      base::WeakPtr<webkit::ppapi::PPB_Broker_Impl>& weak_ptr = i->second;
      if (weak_ptr)
        ConnectPluginToBroker(weak_ptr);
    }
  } else {
    // Report failure to all clients.
    for (ClientMap::iterator i = pending_connects_.begin();
         i != pending_connects_.end(); ++i) {
      base::WeakPtr<webkit::ppapi::PPB_Broker_Impl>& weak_ptr = i->second;
      if (weak_ptr) {
        weak_ptr->BrokerConnected(
            ppapi::PlatformFileToInt(base::kInvalidPlatformFileValue),
            PP_ERROR_FAILED);
      }
    }
  }
  pending_connects_.clear();
}

void PepperBrokerImpl::ConnectPluginToBroker(
    webkit::ppapi::PPB_Broker_Impl* client) {
  base::SyncSocket::Handle plugin_handle = base::kInvalidPlatformFileValue;
  int32_t result = PP_OK;

  // The socket objects will be deleted when this function exits, closing the
  // handles. Any uses of the socket must duplicate them.
  scoped_ptr<base::SyncSocket> broker_socket(new base::SyncSocket());
  scoped_ptr<base::SyncSocket> plugin_socket(new base::SyncSocket());
  if (base::SyncSocket::CreatePair(broker_socket.get(), plugin_socket.get())) {
    result = dispatcher_->SendHandleToBroker(client->pp_instance(),
                                             broker_socket->handle());

    // If the broker has its pipe handle, duplicate the plugin's handle.
    // Otherwise, the plugin's handle will be automatically closed.
    if (result == PP_OK)
      plugin_handle = DuplicateHandle(plugin_socket->handle());
  } else {
    result = PP_ERROR_FAILED;
  }

  // TOOD(ddorwin): Change the IPC to asynchronous: Queue an object containing
  // client and plugin_socket.release(), then return.
  // That message handler will then call client->BrokerConnected() with the
  // saved pipe handle.
  // Temporarily, just call back.
  client->BrokerConnected(ppapi::PlatformFileToInt(plugin_handle), result);
}

}  // namespace content
