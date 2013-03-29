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

PepperUDPSocketPrivateMessageFilter::IORequest::IORequest(
    const scoped_refptr<net::IOBuffer>& buffer,
    int32_t buffer_size,
    const linked_ptr<net::IPEndPoint>& end_point,
    const ppapi::host::ReplyMessageContext& context)
    : buffer(buffer),
      buffer_size(buffer_size),
      end_point(end_point),
      context(context) {
}

PepperUDPSocketPrivateMessageFilter::IORequest::~IORequest() {
}

PepperUDPSocketPrivateMessageFilter::PepperUDPSocketPrivateMessageFilter(
    BrowserPpapiHostImpl* host,
    PP_Instance instance)
    : allow_address_reuse_(false),
      allow_broadcast_(false),
      custom_send_buffer_size_(false),
      send_buffer_size_(0),
      custom_recv_buffer_size_(false),
      recv_buffer_size_(0),
      closed_(false),
      recvfrom_state_(IO_STATE_IDLE),
      sendto_state_(IO_STATE_IDLE),
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
    case PpapiHostMsg_UDPSocketPrivate_SetInt32SocketFeature::ID:
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
        PpapiHostMsg_UDPSocketPrivate_SetInt32SocketFeature,
        OnMsgSetInt32SocketFeature)
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

  if (closed_)
    return PP_ERROR_FAILED;

  switch(static_cast<PP_UDPSocketFeature_Private>(name)) {
    case PP_UDPSOCKETFEATURE_ADDRESS_REUSE:
      DCHECK(!socket_.get());
      allow_address_reuse_ = value;
      break;
    case PP_UDPSOCKETFEATURE_BROADCAST:
      DCHECK(!socket_.get());
      allow_broadcast_ = value;
      break;
    default:
      NOTREACHED();
      break;
  }
  return PP_OK;
}

int32_t PepperUDPSocketPrivateMessageFilter::OnMsgSetInt32SocketFeature(
    const ppapi::host::HostMessageContext* context,
    int32_t name,
    int32_t value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (closed_)
    return PP_ERROR_FAILED;

  switch(static_cast<PP_UDPSocketFeature_Private>(name)) {
    case PP_UDPSOCKETFEATURE_SEND_BUFFER_SIZE:
      custom_send_buffer_size_ = true;
      send_buffer_size_ = value;
      if (socket_.get())
        socket_->SetSendBufferSize(send_buffer_size_);
      break;
    case PP_UDPSOCKETFEATURE_RECV_BUFFER_SIZE:
      custom_recv_buffer_size_ = true;
      recv_buffer_size_ = value;
      if (socket_.get())
        socket_->SetReceiveBufferSize(recv_buffer_size_);
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

  if (num_bytes > ppapi::proxy::UDPSocketPrivateResource::kMaxReadSize) {
    // |num_bytes| value is checked on the plugin side.
    NOTREACHED();
    num_bytes = ppapi::proxy::UDPSocketPrivateResource::kMaxReadSize;
  }
  scoped_refptr<net::IOBuffer> recvfrom_buffer(new net::IOBuffer(num_bytes));
  linked_ptr<net::IPEndPoint> end_point(new net::IPEndPoint());
  recvfrom_requests_.push(IORequest(recvfrom_buffer,
                                    num_bytes,
                                    end_point,
                                    context->MakeReplyMessageContext()));
  RecvFromInternal();
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

void PepperUDPSocketPrivateMessageFilter::RecvFromInternal() {
  if (recvfrom_requests_.empty() || recvfrom_state_ == IO_STATE_IN_PROCESS)
    return;
  DCHECK(recvfrom_state_ == IO_STATE_IDLE);
  recvfrom_state_ = IO_STATE_IN_PROCESS;
  const IORequest& request = recvfrom_requests_.front();
  int result = socket_->RecvFrom(
      request.buffer.get(),
      static_cast<int>(request.buffer_size),
      request.end_point.get(),
      base::Bind(&PepperUDPSocketPrivateMessageFilter::OnRecvFromCompleted,
                 this, request));
  if (result != net::ERR_IO_PENDING)
    OnRecvFromCompleted(request, result);
}

void PepperUDPSocketPrivateMessageFilter::SendToInternal() {
  if (sendto_requests_.empty() || sendto_state_ == IO_STATE_IN_PROCESS)
    return;
  DCHECK(sendto_state_ == IO_STATE_IDLE);
  sendto_state_ = IO_STATE_IN_PROCESS;
  const IORequest& request = sendto_requests_.front();
  int result = socket_->SendTo(
      request.buffer.get(),
      static_cast<int>(request.buffer_size),
      *request.end_point.get(),
      base::Bind(&PepperUDPSocketPrivateMessageFilter::OnSendToCompleted,
                 this, request));
  if (result != net::ERR_IO_PENDING)
    OnSendToCompleted(request, result);
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
    return;
  }
  if (custom_send_buffer_size_ &&
      !socket_->SetSendBufferSize(send_buffer_size_)) {
    SendBindError(context, PP_ERROR_FAILED);
    return;
  }
  if (custom_recv_buffer_size_ &&
      !socket_->SetReceiveBufferSize(recv_buffer_size_)) {
    SendBindError(context, PP_ERROR_FAILED);
    return;
  }

  SendBindReply(context, PP_OK, net_address);
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

  scoped_refptr<net::IOBuffer> sendto_buffer(
      new net::IOBufferWithSize(num_bytes)) ;
  memcpy(sendto_buffer->data(), data.data(), num_bytes);

  net::IPAddressNumber address;
  int port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    SendSendToError(context, PP_ERROR_FAILED);
    return;
  }
  linked_ptr<net::IPEndPoint> end_point(new net::IPEndPoint(address, port));

  sendto_requests_.push(IORequest(sendto_buffer,
                                  static_cast<int32_t>(num_bytes),
                                  end_point,
                                  context));
  SendToInternal();
}

void PepperUDPSocketPrivateMessageFilter::Close() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (socket_.get() && !closed_)
    socket_->Close();
  closed_ = true;
}

void PepperUDPSocketPrivateMessageFilter::OnRecvFromCompleted(
    const IORequest& request,
    int32_t result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(request.buffer.get());
  DCHECK(request.end_point.get());

  // Convert IPEndPoint we get back from RecvFrom to a PP_NetAddress_Private,
  // to send back.
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;
  if (result < 0 ||
      !NetAddressPrivateImpl::IPEndPointToNetAddress(
          request.end_point->address(),
          request.end_point->port(),
          &addr)) {
    SendRecvFromError(request.context, PP_ERROR_FAILED);
  } else {
    SendRecvFromReply(request.context, PP_OK,
                      std::string(request.buffer->data(), result), addr);
  }
  // Take out |request| from |recvfrom_requests_| queue.
  recvfrom_requests_.pop();
  recvfrom_state_ = IO_STATE_IDLE;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperUDPSocketPrivateMessageFilter::RecvFromInternal, this));
}

void PepperUDPSocketPrivateMessageFilter::OnSendToCompleted(
    const IORequest& request,
    int32_t result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(request.buffer.get());
  if (result < 0)
    SendSendToError(request.context, PP_ERROR_FAILED);
  else
    SendSendToReply(request.context, PP_OK, result);
  // Take out |request| from |sendto_requests_| queue.
  sendto_requests_.pop();
  sendto_state_ = IO_STATE_IDLE;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperUDPSocketPrivateMessageFilter::SendToInternal, this));
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
