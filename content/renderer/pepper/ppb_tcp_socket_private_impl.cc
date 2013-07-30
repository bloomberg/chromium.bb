// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_tcp_socket_private_impl.h"

#include "content/common/pepper_messages.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_delegate_impl.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/resource_helper.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/socket_option_data.h"

namespace content {

PPB_TCPSocket_Private_Impl::PPB_TCPSocket_Private_Impl(
    PP_Instance instance, uint32 socket_id)
    : ::ppapi::TCPSocketPrivateImpl(instance, socket_id) {
}

PPB_TCPSocket_Private_Impl::~PPB_TCPSocket_Private_Impl() {
  Disconnect();
}

PP_Resource PPB_TCPSocket_Private_Impl::CreateResource(PP_Instance instance) {
  PepperPluginDelegateImpl* plugin_delegate = GetPluginDelegate(instance);
  if (!plugin_delegate)
    return 0;

  uint32 socket_id = 0;
  plugin_delegate->Send(new PpapiHostMsg_PPBTCPSocket_CreatePrivate(
      plugin_delegate->routing_id(), 0, &socket_id));
  if (!socket_id)
    return 0;

  return (new PPB_TCPSocket_Private_Impl(instance, socket_id))->GetReference();
}

PP_Resource PPB_TCPSocket_Private_Impl::CreateConnectedSocket(
    PP_Instance instance,
    uint32 socket_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  PepperPluginDelegateImpl* plugin_delegate = GetPluginDelegate(instance);
  if (!plugin_delegate)
    return 0;

  PPB_TCPSocket_Private_Impl* socket =
      new PPB_TCPSocket_Private_Impl(instance, socket_id);

  socket->connection_state_ = PPB_TCPSocket_Private_Impl::CONNECTED;
  socket->local_addr_ = local_addr;
  socket->remote_addr_ = remote_addr;

  plugin_delegate->RegisterTCPSocket(socket, socket_id);

  return socket->GetReference();
}

void PPB_TCPSocket_Private_Impl::SendConnect(const std::string& host,
                                             uint16_t port) {
  PepperPluginDelegateImpl* plugin_delegate = GetPluginDelegate(pp_instance());
  if (!plugin_delegate)
    return;

  plugin_delegate->RegisterTCPSocket(this, socket_id_);
  plugin_delegate->Send(new PpapiHostMsg_PPBTCPSocket_Connect(
      plugin_delegate->routing_id(), socket_id_, host, port));
}

void PPB_TCPSocket_Private_Impl::SendConnectWithNetAddress(
    const PP_NetAddress_Private& addr) {
  PepperPluginDelegateImpl* plugin_delegate = GetPluginDelegate(pp_instance());
  if (!plugin_delegate)
    return;

  plugin_delegate->RegisterTCPSocket(this, socket_id_);
  plugin_delegate->Send(new PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress(
      plugin_delegate->routing_id(), socket_id_, addr));
}

void PPB_TCPSocket_Private_Impl::SendSSLHandshake(
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char> >& trusted_certs,
    const std::vector<std::vector<char> >& untrusted_certs) {
  PepperPluginDelegateImpl* plugin_delegate = GetPluginDelegate(pp_instance());
  if (!plugin_delegate)
    return;

  plugin_delegate->Send(new PpapiHostMsg_PPBTCPSocket_SSLHandshake(
      socket_id_, server_name, server_port, trusted_certs, untrusted_certs));
}

void PPB_TCPSocket_Private_Impl::SendRead(int32_t bytes_to_read) {
  PepperPluginDelegateImpl* plugin_delegate = GetPluginDelegate(pp_instance());
  if (!plugin_delegate)
    return;

  plugin_delegate->Send(new PpapiHostMsg_PPBTCPSocket_Read(
      socket_id_, bytes_to_read));
}


void PPB_TCPSocket_Private_Impl::SendWrite(const std::string& buffer) {
  PepperPluginDelegateImpl* plugin_delegate = GetPluginDelegate(pp_instance());
  if (!plugin_delegate)
    return;

  plugin_delegate->Send(new PpapiHostMsg_PPBTCPSocket_Write(
      socket_id_, buffer));
}

void PPB_TCPSocket_Private_Impl::SendDisconnect() {
  PepperPluginDelegateImpl* plugin_delegate = GetPluginDelegate(pp_instance());
  if (!plugin_delegate)
    return;

  plugin_delegate->Send(new PpapiHostMsg_PPBTCPSocket_Disconnect(socket_id_));
}

void PPB_TCPSocket_Private_Impl::SendSetOption(
    PP_TCPSocket_Option name,
    const ::ppapi::SocketOptionData& value) {
  PepperPluginDelegateImpl* plugin_delegate = GetPluginDelegate(pp_instance());
  if (!plugin_delegate)
    return;

  plugin_delegate->Send(new PpapiHostMsg_PPBTCPSocket_SetOption(
      socket_id_, name, value));
}

PepperPluginDelegateImpl* PPB_TCPSocket_Private_Impl::GetPluginDelegate(
    PP_Instance instance) {
  PepperPluginInstanceImpl* plugin_instance =
      HostGlobals::Get()->GetInstance(instance);
  if (!plugin_instance)
    return NULL;
  return plugin_instance->delegate();
}

}  // namespace content
