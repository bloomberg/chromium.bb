// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_tcp_server_socket_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/socket_permission_request.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/error_conversion.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"

using ppapi::NetAddressPrivateImpl;
using ppapi::host::NetErrorToPepperError;

namespace {

size_t g_num_instances = 0;

}  // namespace

namespace content {

PepperTCPServerSocketMessageFilter::PepperTCPServerSocketMessageFilter(
    BrowserPpapiHostImpl* host,
    PP_Instance instance,
    bool private_api,
    const scoped_refptr<PepperMessageFilter>& pepper_message_filter)
    : state_(STATE_BEFORE_LISTENING),
      pepper_message_filter_(pepper_message_filter),
      external_plugin_(host->external_plugin()),
      private_api_(private_api),
      render_process_id_(0),
      render_view_id_(0) {
  ++g_num_instances;
  DCHECK(host);
  if (!host->GetRenderViewIDsForInstance(instance,
                                         &render_process_id_,
                                         &render_view_id_)) {
    NOTREACHED();
  }
}

PepperTCPServerSocketMessageFilter::~PepperTCPServerSocketMessageFilter() {
  --g_num_instances;
}

// static
size_t PepperTCPServerSocketMessageFilter::GetNumInstances() {
  return g_num_instances;
}

scoped_refptr<base::TaskRunner>
PepperTCPServerSocketMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case PpapiHostMsg_TCPServerSocket_Listen::ID:
      return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
    case PpapiHostMsg_TCPServerSocket_Accept::ID:
    case PpapiHostMsg_TCPServerSocket_StopListening::ID:
      return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  }
  return NULL;
}

int32_t PepperTCPServerSocketMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperTCPServerSocketMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_TCPServerSocket_Listen, OnMsgListen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_TCPServerSocket_Accept, OnMsgAccept)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_TCPServerSocket_StopListening, OnMsgStopListening)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperTCPServerSocketMessageFilter::OnMsgListen(
    const ppapi::host::HostMessageContext* context,
    const PP_NetAddress_Private& addr,
    int32_t backlog) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(context);

  SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          content::SocketPermissionRequest::TCP_LISTEN, addr);
  if (!pepper_socket_utils::CanUseSocketAPIs(external_plugin_,
                                             private_api_,
                                             request,
                                             render_process_id_,
                                             render_view_id_)) {
    return PP_ERROR_NOACCESS;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperTCPServerSocketMessageFilter::DoListen, this,
                 context->MakeReplyMessageContext(), addr, backlog));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPServerSocketMessageFilter::OnMsgAccept(
    const ppapi::host::HostMessageContext* context,
    uint32 plugin_dispatcher_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(context);

  if (state_ != STATE_LISTENING)
    return PP_ERROR_FAILED;

  state_ = STATE_ACCEPT_IN_PROGRESS;
  ppapi::host::ReplyMessageContext reply_context(
      context->MakeReplyMessageContext());
  int net_result = socket_->Accept(
      &socket_buffer_,
      base::Bind(&PepperTCPServerSocketMessageFilter::OnAcceptCompleted,
                 base::Unretained(this),
                 reply_context, plugin_dispatcher_id));
  if (net_result != net::ERR_IO_PENDING)
    OnAcceptCompleted(reply_context, plugin_dispatcher_id, net_result);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPServerSocketMessageFilter::OnMsgStopListening(
    const ppapi::host::HostMessageContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(context);

  state_ = STATE_CLOSED;
  socket_.reset();
  return PP_OK;
}

void PepperTCPServerSocketMessageFilter::DoListen(
    const ppapi::host::ReplyMessageContext& context,
    const PP_NetAddress_Private& addr,
    int32_t backlog) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::IPAddressNumber address;
  int port;
  if (state_ != STATE_BEFORE_LISTENING ||
      !NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    SendListenError(context, PP_ERROR_FAILED);
    state_ = STATE_CLOSED;
    return;
  }

  state_ = STATE_LISTEN_IN_PROGRESS;

  socket_.reset(new net::TCPServerSocket(NULL, net::NetLog::Source()));
  int net_result = socket_->Listen(net::IPEndPoint(address, port), backlog);
  if (net_result != net::ERR_IO_PENDING)
    OnListenCompleted(context, net_result);
}

void PepperTCPServerSocketMessageFilter::OnListenCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result) {
  if (state_ != STATE_LISTEN_IN_PROGRESS) {
    SendListenError(context, PP_ERROR_FAILED);
    state_ = STATE_CLOSED;
    return;
  }
  if (net_result != net::OK) {
    SendListenError(context, NetErrorToPepperError(net_result));
    state_ = STATE_BEFORE_LISTENING;
    return;
  }

  DCHECK(socket_.get());

  net::IPEndPoint end_point;
  PP_NetAddress_Private addr;

  int32_t pp_result =
      NetErrorToPepperError(socket_->GetLocalAddress(&end_point));
  if (pp_result != PP_OK) {
    SendListenError(context, pp_result);
    state_ = STATE_BEFORE_LISTENING;
    return;
  }
  if (!NetAddressPrivateImpl::IPEndPointToNetAddress(end_point.address(),
                                                     end_point.port(),
                                                     &addr)) {
    SendListenError(context, PP_ERROR_FAILED);
    state_ = STATE_BEFORE_LISTENING;
    return;
  }

  SendListenReply(context, PP_OK, addr);
  state_ = STATE_LISTENING;
}

void PepperTCPServerSocketMessageFilter::OnAcceptCompleted(
    const ppapi::host::ReplyMessageContext& context,
    uint32 plugin_dispatcher_id,
    int net_result) {
  if (state_ != STATE_ACCEPT_IN_PROGRESS) {
    SendAcceptError(context, PP_ERROR_FAILED);
    state_ = STATE_CLOSED;
    return;
  }

  state_ = STATE_LISTENING;

  if (net_result != net::OK) {
    SendAcceptError(context, NetErrorToPepperError(net_result));
    return;
  }

  DCHECK(socket_buffer_.get());

  scoped_ptr<net::StreamSocket> socket(socket_buffer_.release());
  net::IPEndPoint ip_end_point_local;
  net::IPEndPoint ip_end_point_remote;
  PP_NetAddress_Private local_addr = NetAddressPrivateImpl::kInvalidNetAddress;
  PP_NetAddress_Private remote_addr = NetAddressPrivateImpl::kInvalidNetAddress;

  int32_t pp_result =
      NetErrorToPepperError(socket->GetLocalAddress(&ip_end_point_local));
  if (pp_result != PP_OK) {
    SendAcceptError(context, pp_result);
    return;
  }
  if (!NetAddressPrivateImpl::IPEndPointToNetAddress(
          ip_end_point_local.address(),
          ip_end_point_local.port(),
          &local_addr)) {
    SendAcceptError(context, PP_ERROR_FAILED);
    return;
  }
  pp_result =
      NetErrorToPepperError(socket->GetPeerAddress(&ip_end_point_remote));
  if (pp_result != PP_OK) {
    SendAcceptError(context, pp_result);
    return;
  }
  if (!NetAddressPrivateImpl::IPEndPointToNetAddress(
          ip_end_point_remote.address(),
          ip_end_point_remote.port(),
          &remote_addr)) {
    SendAcceptError(context, PP_ERROR_FAILED);
    return;
  }
  if (!pepper_message_filter_.get() || plugin_dispatcher_id == 0) {
    SendAcceptError(context, PP_ERROR_FAILED);
    return;
  }
  uint32 accepted_socket_id = pepper_message_filter_->AddAcceptedTCPSocket(
      ppapi::API_ID_PPB_TCPSOCKET_PRIVATE,
      plugin_dispatcher_id,
      socket.release());
  if (accepted_socket_id != 0) {
    SendAcceptReply(context, PP_OK, accepted_socket_id, local_addr,
                    remote_addr);
  } else {
    SendAcceptError(context, PP_ERROR_NOSPACE);
  }
}

void PepperTCPServerSocketMessageFilter::SendListenReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result,
    const PP_NetAddress_Private& local_addr) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  SendReply(reply_context,
            PpapiPluginMsg_TCPServerSocket_ListenReply(local_addr));
}

void PepperTCPServerSocketMessageFilter::SendListenError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result) {
  SendListenReply(context, pp_result,
                  NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperTCPServerSocketMessageFilter::SendAcceptReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result,
    uint32 accepted_socket_id,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  SendReply(reply_context, PpapiPluginMsg_TCPServerSocket_AcceptReply(
      accepted_socket_id, local_addr, remote_addr));
}

void PepperTCPServerSocketMessageFilter::SendAcceptError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result) {
  SendAcceptReply(context,
                  pp_result,
                  0,
                  NetAddressPrivateImpl::kInvalidNetAddress,
                  NetAddressPrivateImpl::kInvalidNetAddress);
}

}  // namespace content
