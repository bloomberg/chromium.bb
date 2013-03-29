// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_udp_socket_private_message_filter.h"

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

namespace {

bool CanUseSocketAPIs(const SocketPermissionRequest& request,
                      bool external_plugin,
                      int render_process_id,
                      int render_view_id) {
  RenderViewHost* render_view_host = RenderViewHost::FromID(render_process_id,
                                                            render_view_id);
  return render_view_host &&
      pepper_socket_utils::CanUseSocketAPIs(external_plugin,
                                            request,
                                            render_view_host);
}

}  // namespace

PepperUDPSocketPrivateMessageFilter::PepperUDPSocketPrivateMessageFilter(
    BrowserPpapiHostImpl* host,
    PP_Instance instance)
    : allow_address_reuse_(false),
      allow_broadcast_(false),
      closed_(false),
      external_plugin_(host->external_plugin()),
      render_process_id_(0),
      render_view_id_(0) {
  DCHECK(host);

  if (!host->GetRenderViewIDsForInstance(
          instance,
          &render_process_id_,
          &render_view_id_)) {
    NOTREACHED();
  }
}

PepperUDPSocketPrivateMessageFilter::~PepperUDPSocketPrivateMessageFilter() {
  Close();
}

scoped_refptr<base::TaskRunner>
PepperUDPSocketPrivateMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case PpapiHostMsg_UDPSocketPrivate_SetBoolSocketFeature::ID:
    case PpapiHostMsg_UDPSocketPrivate_RecvFrom::ID:
    case PpapiHostMsg_UDPSocketPrivate_Close::ID:
      return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
    case PpapiHostMsg_UDPSocketPrivate_Bind::ID:
    case PpapiHostMsg_UDPSocketPrivate_SendTo::ID:
      return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  }
  return NULL;
}

int32_t PepperUDPSocketPrivateMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperUDPSocketPrivateMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_UDPSocketPrivate_SetBoolSocketFeature,
        OnMsgSetBoolSocketFeature)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_UDPSocketPrivate_Bind,
        OnMsgBind)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_UDPSocketPrivate_RecvFrom,
        OnMsgRecvFrom)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_UDPSocketPrivate_SendTo,
        OnMsgSendTo)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_UDPSocketPrivate_Close,
        OnMsgClose)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperUDPSocketPrivateMessageFilter::OnMsgSetBoolSocketFeature(
    const ppapi::host::HostMessageContext* context,
    int32_t name,
    bool value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!socket_.get());

  if (closed_)
    return PP_ERROR_FAILED;

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

int32_t PepperUDPSocketPrivateMessageFilter::OnMsgBind(
    const ppapi::host::HostMessageContext* context,
    const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(context);

  SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::UDP_BIND, addr);
  if (!CanUseSocketAPIs(request, external_plugin_,
                        render_process_id_, render_view_id_)) {
    return PP_ERROR_FAILED;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperUDPSocketPrivateMessageFilter::DoBind, this,
                 context->MakeReplyMessageContext(),
                 addr));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketPrivateMessageFilter::OnMsgRecvFrom(
    const ppapi::host::HostMessageContext* context,
    int32_t num_bytes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(context);
  DCHECK(socket_.get());

  if (closed_)
    return PP_ERROR_FAILED;

  if (recvfrom_buffer_.get())
    return PP_ERROR_INPROGRESS;
  if (num_bytes > ppapi::proxy::UDPSocketPrivateResource::kMaxReadSize) {
    // |num_bytes| value is checked on the plugin side.
    NOTREACHED();
    num_bytes = ppapi::proxy::UDPSocketPrivateResource::kMaxReadSize;
  }
  recvfrom_buffer_ = new net::IOBuffer(num_bytes);
  int result = socket_->RecvFrom(
      recvfrom_buffer_, num_bytes, &recvfrom_address_,
      base::Bind(&PepperUDPSocketPrivateMessageFilter::OnRecvFromCompleted,
                 this, context->MakeReplyMessageContext()));
  if (result != net::ERR_IO_PENDING)
    OnRecvFromCompleted(context->MakeReplyMessageContext(), result);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketPrivateMessageFilter::OnMsgSendTo(
    const ppapi::host::HostMessageContext* context,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(context);

  SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          SocketPermissionRequest::UDP_SEND_TO, addr);
  if (!CanUseSocketAPIs(request, external_plugin_,
                        render_process_id_, render_view_id_)) {
    return PP_ERROR_FAILED;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperUDPSocketPrivateMessageFilter::DoSendTo, this,
                 context->MakeReplyMessageContext(), data, addr));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperUDPSocketPrivateMessageFilter::OnMsgClose(
    const ppapi::host::HostMessageContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  Close();
  return PP_OK;
}

void PepperUDPSocketPrivateMessageFilter::DoBind(
    const ppapi::host::ReplyMessageContext& context,
    const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (closed_) {
    SendBindError(context, PP_ERROR_FAILED);
    return;
  }

  socket_.reset(new net::UDPServerSocket(NULL, net::NetLog::Source()));

  net::IPAddressNumber address;
  int port;
  if (!socket_.get() ||
      !NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    SendBindError(context, PP_ERROR_FAILED);
    return;
  }

  if (allow_address_reuse_)
    socket_->AllowAddressReuse();
  if (allow_broadcast_)
    socket_->AllowBroadcast();

  int result = socket_->Listen(net::IPEndPoint(address, port));

  net::IPEndPoint bound_address;
  PP_NetAddress_Private net_address = NetAddressPrivateImpl::kInvalidNetAddress;
  if (result != net::OK ||
      socket_->GetLocalAddress(&bound_address) != net::OK ||
      !NetAddressPrivateImpl::IPEndPointToNetAddress(bound_address.address(),
                                                     bound_address.port(),
                                                     &net_address)) {
    SendBindError(context, PP_ERROR_FAILED);
  } else {
    SendBindReply(context, PP_OK, net_address);
  }
}

void PepperUDPSocketPrivateMessageFilter::DoSendTo(
    const ppapi::host::ReplyMessageContext& context,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(socket_.get());

  if (closed_) {
    SendSendToError(context, PP_ERROR_FAILED);
    return;
  }

  if (sendto_buffer_.get()) {
    SendSendToError(context, PP_ERROR_INPROGRESS);
    return;
  }

  if (data.empty() ||
      data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    SendSendToError(context, PP_ERROR_BADARGUMENT);
    return;
  }
  size_t num_bytes = data.size();
  if (num_bytes > static_cast<size_t>(
          ppapi::proxy::UDPSocketPrivateResource::kMaxWriteSize)) {
    // Size of |data| is checked on the plugin side.
    NOTREACHED();
    num_bytes = static_cast<size_t>(
        ppapi::proxy::UDPSocketPrivateResource::kMaxWriteSize);
  }
  sendto_buffer_ = new net::IOBufferWithSize(num_bytes);
  memcpy(sendto_buffer_->data(), data.data(), num_bytes);

  net::IPAddressNumber address;
  int port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    SendSendToError(context, PP_ERROR_FAILED);
    return;
  }

  int result = socket_->SendTo(
      sendto_buffer_, sendto_buffer_->size(), net::IPEndPoint(address, port),
      base::Bind(&PepperUDPSocketPrivateMessageFilter::OnSendToCompleted, this,
                 context));
  if (result != net::ERR_IO_PENDING)
    OnSendToCompleted(context, result);
}

void PepperUDPSocketPrivateMessageFilter::Close() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (socket_.get() && !closed_)
    socket_->Close();
  closed_ = true;
}

void PepperUDPSocketPrivateMessageFilter::OnRecvFromCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(recvfrom_buffer_.get());

  // Convert IPEndPoint we get back from RecvFrom to a PP_NetAddress_Private,
  // to send back.
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;
  if (result < 0 ||
      !NetAddressPrivateImpl::IPEndPointToNetAddress(
          recvfrom_address_.address(),
          recvfrom_address_.port(),
          &addr)) {
    SendRecvFromError(context, PP_ERROR_FAILED);
  } else {
    SendRecvFromReply(context, PP_OK,
                      std::string(recvfrom_buffer_->data(), result), addr);
  }

  recvfrom_buffer_ = NULL;
}

void PepperUDPSocketPrivateMessageFilter::OnSendToCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(sendto_buffer_.get());
  if (result < 0)
    SendSendToError(context, PP_ERROR_FAILED);
  else
    SendSendToReply(context, PP_OK, result);
  sendto_buffer_ = NULL;
}

void PepperUDPSocketPrivateMessageFilter::SendBindReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result,
    const PP_NetAddress_Private& addr) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(result);
  SendReply(reply_context, PpapiPluginMsg_UDPSocketPrivate_BindReply(addr));
}

void PepperUDPSocketPrivateMessageFilter::SendRecvFromReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(result);
  SendReply(reply_context,
            PpapiPluginMsg_UDPSocketPrivate_RecvFromReply(data, addr));
}

void PepperUDPSocketPrivateMessageFilter::SendSendToReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result,
    int32_t bytes_written) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(result);
  SendReply(reply_context,
            PpapiPluginMsg_UDPSocketPrivate_SendToReply(bytes_written));
}

void PepperUDPSocketPrivateMessageFilter::SendBindError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result) {
  SendBindReply(context, result, NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperUDPSocketPrivateMessageFilter::SendRecvFromError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result) {
  SendRecvFromReply(context, result, "",
                    NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperUDPSocketPrivateMessageFilter::SendSendToError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t result) {
  SendSendToReply(context, result, 0);
}

}  // namespace content
