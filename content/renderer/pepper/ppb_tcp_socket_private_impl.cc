// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_tcp_socket_private_impl.h"

#include "content/common/pepper_messages.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/socket_option_data.h"

namespace content {

PPB_TCPSocket_Private_Impl::PPB_TCPSocket_Private_Impl(
    PP_Instance instance,
    uint32 socket_id,
    int routing_id)
    : ppapi::TCPSocketPrivateImpl(instance, socket_id),
      routing_id_(routing_id) {
  ChildThread::current()->AddRoute(routing_id, this);
}

PPB_TCPSocket_Private_Impl::~PPB_TCPSocket_Private_Impl() {
  ChildThread::current()->RemoveRoute(routing_id_);
  Disconnect();
}

PP_Resource PPB_TCPSocket_Private_Impl::CreateResource(PP_Instance instance) {
  int routing_id = RenderThreadImpl::current()->GenerateRoutingID();
  uint32 socket_id = 0;
  RenderThreadImpl::current()->Send(new PpapiHostMsg_PPBTCPSocket_CreatePrivate(
      routing_id, 0, &socket_id));
  if (!socket_id)
    return 0;

  return (new PPB_TCPSocket_Private_Impl(
      instance, socket_id, routing_id))->GetReference();
}

void PPB_TCPSocket_Private_Impl::SendConnect(const std::string& host,
                                             uint16_t port) {
  RenderThreadImpl::current()->Send(new PpapiHostMsg_PPBTCPSocket_Connect(
      routing_id_, socket_id_, host, port));
}

void PPB_TCPSocket_Private_Impl::SendConnectWithNetAddress(
    const PP_NetAddress_Private& addr) {
  RenderThreadImpl::current()->Send(
      new PpapiHostMsg_PPBTCPSocket_ConnectWithNetAddress(
          routing_id_, socket_id_, addr));
}

void PPB_TCPSocket_Private_Impl::SendSSLHandshake(
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char> >& trusted_certs,
    const std::vector<std::vector<char> >& untrusted_certs) {
  RenderThreadImpl::current()->Send(new PpapiHostMsg_PPBTCPSocket_SSLHandshake(
      socket_id_, server_name, server_port, trusted_certs, untrusted_certs));
}

void PPB_TCPSocket_Private_Impl::SendRead(int32_t bytes_to_read) {
  RenderThreadImpl::current()->Send(new PpapiHostMsg_PPBTCPSocket_Read(
      socket_id_, bytes_to_read));
}


void PPB_TCPSocket_Private_Impl::SendWrite(const std::string& buffer) {
  RenderThreadImpl::current()->Send(
      new PpapiHostMsg_PPBTCPSocket_Write(socket_id_, buffer));
}

void PPB_TCPSocket_Private_Impl::SendDisconnect() {
  RenderThreadImpl::current()->Send(
      new PpapiHostMsg_PPBTCPSocket_Disconnect(socket_id_));
}

void PPB_TCPSocket_Private_Impl::SendSetOption(
    PP_TCPSocket_Option name,
    const ppapi::SocketOptionData& value) {
  RenderThreadImpl::current()->Send(
      new PpapiHostMsg_PPBTCPSocket_SetOption(socket_id_, name, value));
}

bool PPB_TCPSocket_Private_Impl::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_TCPSocket_Private_Impl, message)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_ConnectACK, OnTCPSocketConnectACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_SSLHandshakeACK,
                        OnTCPSocketSSLHandshakeACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_ReadACK, OnTCPSocketReadACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_WriteACK, OnTCPSocketWriteACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTCPSocket_SetOptionACK,
                        OnTCPSocketSetOptionACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_TCPSocket_Private_Impl::OnTCPSocketConnectACK(
    uint32 plugin_dispatcher_id,
    uint32 socket_id,
    int32_t result,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  OnConnectCompleted(result, local_addr, remote_addr);
}

void PPB_TCPSocket_Private_Impl::OnTCPSocketSSLHandshakeACK(
    uint32 plugin_dispatcher_id,
    uint32 socket_id,
    bool succeeded,
    const ppapi::PPB_X509Certificate_Fields& certificate_fields) {
  OnSSLHandshakeCompleted(succeeded, certificate_fields);
}

void PPB_TCPSocket_Private_Impl::OnTCPSocketReadACK(uint32 plugin_dispatcher_id,
                                                    uint32 socket_id,
                                                    int32_t result,
                                                    const std::string& data) {
  OnReadCompleted(result, data);
}

void PPB_TCPSocket_Private_Impl::OnTCPSocketWriteACK(
    uint32 plugin_dispatcher_id,
    uint32 socket_id,
    int32_t result) {
  OnWriteCompleted(result);
}

void PPB_TCPSocket_Private_Impl::OnTCPSocketSetOptionACK(
    uint32 plugin_dispatcher_id,
    uint32 socket_id,
    int32_t result) {
  OnSetOptionCompleted(result);
}

}  // namespace content
