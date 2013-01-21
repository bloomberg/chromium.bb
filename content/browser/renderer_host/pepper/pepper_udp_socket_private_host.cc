// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_udp_socket_private_host.h"

#include <cstring>
#include <limits>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "content/public/common/socket_permission_request.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/udp/udp_server_socket.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/udp_socket_private_resource.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"

using ppapi::NetAddressPrivateImpl;

namespace content {

PepperUDPSocketPrivateHost::PepperUDPSocketPrivateHost(
    BrowserPpapiHostImpl* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      allow_address_reuse_(false),
      allow_broadcast_(false),
      closed_(false),
      host_(host),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(host_);
}

PepperUDPSocketPrivateHost::~PepperUDPSocketPrivateHost() {
  Close();
}

int32_t PepperUDPSocketPrivateHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperUDPSocketPrivateHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_UDPSocketPrivate_SetBoolSocketFeature,
        OnMsgSetBoolSocketFeature)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_UDPSocketPrivate_Bind,
        OnMsgBind)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_UDPSocketPrivate_RecvFrom,
        OnMsgRecvFrom)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_UDPSocketPrivate_SendTo,
        OnMsgSendTo)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_UDPSocketPrivate_Close,
        OnMsgClose)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperUDPSocketPrivateHost::OnMsgSetBoolSocketFeature(
    const ppapi::host::HostMessageContext* context,
    int32_t name,
    bool value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!socket_.get());
  DCHECK(!closed());

  switch(static_cast<PP_UDPSocketFeature_Private>(name)) {
    case PP_UDPSOCKETFEATURE_ADDRESS_REUSE:
      allow_address_reuse_ = value;
      break;
    case PP_UDPSOCKETFEATURE_BROADCAST:
      allow_broadcast_ = value;
      break;
    default:
      NOTREACHED();
      break;
  }
  return PP_OK;
}

int32_t PepperUDPSocketPrivateHost::OnMsgBind(
    const ppapi::host::HostMessageContext* context,
    const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(context);

  if (bind_context_.get())
    return PP_ERROR_INPROGRESS;
  bind_context_.reset(
      new ppapi::host::ReplyMessageContext(context->MakeReplyMessageContext()));

  SocketPermissionRequest params =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::UDP_BIND, addr);
  CheckSocketPermissionsAndReply(params,
                                 base::Bind(&PepperUDPSocketPrivateHost::DoBind,
                                            weak_factory_.GetWeakPtr(),
                                            addr));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketPrivateHost::OnMsgRecvFrom(
    const ppapi::host::HostMessageContext* context,
    int32_t num_bytes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(context);
  DCHECK(socket_.get());
  DCHECK(!closed());

  if (recv_from_context_.get() || recvfrom_buffer_.get())
    return PP_ERROR_INPROGRESS;
  recv_from_context_.reset(
      new ppapi::host::ReplyMessageContext(context->MakeReplyMessageContext()));
  if (num_bytes > ppapi::proxy::UDPSocketPrivateResource::kMaxReadSize) {
    // |num_bytes| value is checked on the plugin side.
    NOTREACHED();
    num_bytes = ppapi::proxy::UDPSocketPrivateResource::kMaxReadSize;
  }
  recvfrom_buffer_ = new net::IOBuffer(num_bytes);
  int result = socket_->RecvFrom(
      recvfrom_buffer_, num_bytes, &recvfrom_address_,
      base::Bind(&PepperUDPSocketPrivateHost::OnRecvFromCompleted,
                 weak_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING)
    OnRecvFromCompleted(result);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketPrivateHost::OnMsgSendTo(
    const ppapi::host::HostMessageContext* context,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(context);

  if (data.empty() ||
      data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return PP_ERROR_BADARGUMENT;
  }
  if (send_to_context_.get() || sendto_buffer_.get())
    return PP_ERROR_INPROGRESS;
  send_to_context_.reset(
      new ppapi::host::ReplyMessageContext(context->MakeReplyMessageContext()));
  int num_bytes = data.size();
  if (num_bytes > ppapi::proxy::UDPSocketPrivateResource::kMaxWriteSize) {
    // Size of |data| is checked on the plugin side.
    NOTREACHED();
    num_bytes = ppapi::proxy::UDPSocketPrivateResource::kMaxWriteSize;
  }
  sendto_buffer_ = new net::IOBufferWithSize(num_bytes);
  memcpy(sendto_buffer_->data(), data.data(), num_bytes);
  SocketPermissionRequest params =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::UDP_SEND_TO, addr);
  CheckSocketPermissionsAndReply(params,
                                 base::Bind(
                                     &PepperUDPSocketPrivateHost::DoSendTo,
                                     weak_factory_.GetWeakPtr(),
                                     addr));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketPrivateHost::OnMsgClose(
    const ppapi::host::HostMessageContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  Close();
  return PP_OK;
}

void PepperUDPSocketPrivateHost::DoBind(const PP_NetAddress_Private& addr,
                                        bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!closed());

  if (!allowed) {
    SendBindError();
    return;
  }

  socket_.reset(new net::UDPServerSocket(NULL, net::NetLog::Source()));

  net::IPAddressNumber address;
  int port;
  if (!socket_.get() ||
      !NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    SendBindError();
    return;
  }

  if (allow_address_reuse_)
    socket_->AllowAddressReuse();
  if (allow_broadcast_)
    socket_->AllowBroadcast();

  int result = socket_->Listen(net::IPEndPoint(address, port));

  if (result == net::OK &&
      socket_->GetLocalAddress(&bound_address_) != net::OK) {
    SendBindError();
    return;
  }

  OnBindCompleted(result);
}

void PepperUDPSocketPrivateHost::DoSendTo(const PP_NetAddress_Private& addr,
                                          bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(socket_.get());
  DCHECK(sendto_buffer_.get());
  DCHECK(!closed());

  if (!allowed) {
    SendSendToError();
    return;
  }

  net::IPAddressNumber address;
  int port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    SendSendToError();
    return;
  }

  int result = socket_->SendTo(
      sendto_buffer_, sendto_buffer_->size(), net::IPEndPoint(address, port),
      base::Bind(&PepperUDPSocketPrivateHost::OnSendToCompleted,
                 weak_factory_.GetWeakPtr()));

  if (result != net::ERR_IO_PENDING)
    OnSendToCompleted(result);
}

void PepperUDPSocketPrivateHost::Close() {
  if (socket_.get() && !closed_)
    socket_->Close();
  closed_ = true;
}

void PepperUDPSocketPrivateHost::OnBindCompleted(int result) {
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;
  if (result < 0 ||
      !NetAddressPrivateImpl::IPEndPointToNetAddress(bound_address_.address(),
                                                     bound_address_.port(),
                                                     &addr)) {
    SendBindError();
  } else {
    SendBindReply(true, addr);
  }
}

void PepperUDPSocketPrivateHost::OnRecvFromCompleted(int result) {
  DCHECK(recvfrom_buffer_.get());

  // Convert IPEndPoint we get back from RecvFrom to a PP_NetAddress_Private,
  // to send back.
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;
  if (result < 0 ||
      !NetAddressPrivateImpl::IPEndPointToNetAddress(
          recvfrom_address_.address(),
          recvfrom_address_.port(),
          &addr)) {
    SendRecvFromError();
  } else {
    SendRecvFromReply(true,
                      std::string(recvfrom_buffer_->data(), result),
                      addr);
  }

  recvfrom_buffer_ = NULL;
}

void PepperUDPSocketPrivateHost::OnSendToCompleted(int result) {
  DCHECK(sendto_buffer_.get());
  if (result < 0)
    SendSendToError();
  else
    SendSendToReply(true, result);
  sendto_buffer_ = NULL;
}

void PepperUDPSocketPrivateHost::SendBindReply(
    bool succeeded,
    const PP_NetAddress_Private& addr) {
  DCHECK(bind_context_.get());

  scoped_ptr<ppapi::host::ReplyMessageContext> context(bind_context_.release());
  host()->SendReply(*context,
                    PpapiPluginMsg_UDPSocketPrivate_BindReply(succeeded, addr));
}

void PepperUDPSocketPrivateHost::SendRecvFromReply(
    bool succeeded,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  DCHECK(recv_from_context_.get());

  scoped_ptr<ppapi::host::ReplyMessageContext> context(
      recv_from_context_.release());
  host()->SendReply(*context,
                    PpapiPluginMsg_UDPSocketPrivate_RecvFromReply(succeeded,
                                                                  data,
                                                                  addr));
}

void PepperUDPSocketPrivateHost::SendSendToReply(bool succeeded,
                                                 int32_t bytes_written) {
  DCHECK(send_to_context_.get());

  scoped_ptr<ppapi::host::ReplyMessageContext> context(
      send_to_context_.release());
  host()->SendReply(*context,
                    PpapiPluginMsg_UDPSocketPrivate_SendToReply(succeeded,
                                                                bytes_written));
}

void PepperUDPSocketPrivateHost::CheckSocketPermissionsAndReply(
    const SocketPermissionRequest& params,
    const RequestCallback& callback) {
  pepper_socket_utils::PostOnUIThreadWithRenderViewHostAndReply(
      host_, FROM_HERE, pp_instance(),
      base::Bind(&pepper_socket_utils::CanUseSocketAPIs,
                 host_->plugin_process_type(), params),
      callback);
}

void PepperUDPSocketPrivateHost::SendBindError() {
  SendBindReply(false, NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperUDPSocketPrivateHost::SendRecvFromError() {
  SendRecvFromReply(false, "", NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperUDPSocketPrivateHost::SendSendToError() {
  SendSendToReply(false, 0);
}

}  // namespace content
