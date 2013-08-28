// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_tcp_socket_message_filter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_socket_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/socket_permission_request.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/dns/single_request_host_resolver.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/error_conversion.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/tcp_socket_resource_base.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"

using ppapi::NetAddressPrivateImpl;
using ppapi::host::NetErrorToPepperError;
using ppapi::proxy::TCPSocketResourceBase;

namespace {

size_t g_num_instances = 0;

}  // namespace

namespace content {

PepperTCPSocketMessageFilter::PepperTCPSocketMessageFilter(
    BrowserPpapiHostImpl* host,
    PP_Instance instance,
    bool private_api)
    : external_plugin_(host->external_plugin()),
      private_api_(private_api),
      render_process_id_(0),
      render_view_id_(0),
      state_(STATE_BEFORE_CONNECT),
      end_of_file_reached_(false),
      ssl_context_helper_(host->ssl_context_helper()) {
  DCHECK(host);
  ++g_num_instances;
  if (!host->GetRenderViewIDsForInstance(instance,
                                         &render_process_id_,
                                         &render_view_id_)) {
    NOTREACHED();
  }
}

PepperTCPSocketMessageFilter::PepperTCPSocketMessageFilter(
    BrowserPpapiHostImpl* host,
    PP_Instance instance,
    bool private_api,
    net::StreamSocket* socket)
    : external_plugin_(host->external_plugin()),
      private_api_(private_api),
      render_process_id_(0),
      render_view_id_(0),
      state_(STATE_CONNECTED),
      end_of_file_reached_(false),
      socket_(socket),
      ssl_context_helper_(host->ssl_context_helper()) {
  DCHECK(host);
  ++g_num_instances;
  if (!host->GetRenderViewIDsForInstance(instance,
                                         &render_process_id_,
                                         &render_view_id_)) {
    NOTREACHED();
  }
}

PepperTCPSocketMessageFilter::~PepperTCPSocketMessageFilter() {
  if (socket_)
    socket_->Disconnect();
  --g_num_instances;
}

// static
size_t PepperTCPSocketMessageFilter::GetNumInstances() {
  return g_num_instances;
}

scoped_refptr<base::TaskRunner>
PepperTCPSocketMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case PpapiHostMsg_TCPSocket_Connect::ID:
    case PpapiHostMsg_TCPSocket_ConnectWithNetAddress::ID:
      return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
    case PpapiHostMsg_TCPSocket_SSLHandshake::ID:
    case PpapiHostMsg_TCPSocket_Read::ID:
    case PpapiHostMsg_TCPSocket_Write::ID:
    case PpapiHostMsg_TCPSocket_Disconnect::ID:
    case PpapiHostMsg_TCPSocket_SetOption::ID:
      return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  }
  return NULL;
}

int32_t PepperTCPSocketMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperTCPSocketMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_TCPSocket_Connect, OnMsgConnect)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_TCPSocket_ConnectWithNetAddress,
        OnMsgConnectWithNetAddress)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_TCPSocket_SSLHandshake, OnMsgSSLHandshake)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_TCPSocket_Read, OnMsgRead)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_TCPSocket_Write, OnMsgWrite)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_TCPSocket_Disconnect, OnMsgDisconnect)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_TCPSocket_SetOption, OnMsgSetOption)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperTCPSocketMessageFilter::OnMsgConnect(
    const ppapi::host::HostMessageContext* context,
    const std::string& host,
    uint16_t port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This is only supported by PPB_TCPSocket_Private.
  if (!private_api_) {
    NOTREACHED();
    return PP_ERROR_NOACCESS;
  }

  SocketPermissionRequest request(SocketPermissionRequest::TCP_CONNECT,
                                  host,
                                  port);
  if (!pepper_socket_utils::CanUseSocketAPIs(external_plugin_, private_api_,
                                             request, render_process_id_,
                                             render_view_id_)) {
    return PP_ERROR_NOACCESS;
  }

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id_);
  if (!render_process_host)
    return PP_ERROR_FAILED;
  BrowserContext* browser_context = render_process_host->GetBrowserContext();
  if (!browser_context || !browser_context->GetResourceContext())
    return PP_ERROR_FAILED;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperTCPSocketMessageFilter::DoConnect, this,
                 context->MakeReplyMessageContext(),
                 host, port, browser_context->GetResourceContext()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgConnectWithNetAddress(
    const ppapi::host::HostMessageContext* context,
    const PP_NetAddress_Private& net_addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::SocketPermissionRequest request =
      pepper_socket_utils::CreateSocketPermissionRequest(
          content::SocketPermissionRequest::TCP_CONNECT, net_addr);
  if (!pepper_socket_utils::CanUseSocketAPIs(external_plugin_, private_api_,
                                             request, render_process_id_,
                                             render_view_id_)) {
    return PP_ERROR_NOACCESS;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperTCPSocketMessageFilter::DoConnectWithNetAddress, this,
                 context->MakeReplyMessageContext(), net_addr));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgSSLHandshake(
    const ppapi::host::HostMessageContext* context,
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char> >& trusted_certs,
    const std::vector<std::vector<char> >& untrusted_certs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Allow to do SSL handshake only if currently the socket has been connected
  // and there isn't pending read or write.
  // IsConnected() includes the state that SSL handshake has been finished and
  // therefore isn't suitable here.
  if (state_ != STATE_CONNECTED || read_buffer_.get() ||
      write_buffer_base_.get() || write_buffer_.get()) {
    return PP_ERROR_FAILED;
  }

  SetState(STATE_SSL_HANDSHAKE_IN_PROGRESS);
  // TODO(raymes,rsleevi): Use trusted/untrusted certificates when connecting.

  scoped_ptr<net::ClientSocketHandle> handle(new net::ClientSocketHandle());
  handle->SetSocket(socket_.Pass());
  net::ClientSocketFactory* factory =
      net::ClientSocketFactory::GetDefaultFactory();
  net::HostPortPair host_port_pair(server_name, server_port);
  net::SSLClientSocketContext ssl_context;
  ssl_context.cert_verifier = ssl_context_helper_->GetCertVerifier();
  ssl_context.transport_security_state =
      ssl_context_helper_->GetTransportSecurityState();
  socket_ = factory->CreateSSLClientSocket(
      handle.Pass(), host_port_pair, ssl_context_helper_->ssl_config(),
      ssl_context);
  if (!socket_) {
    LOG(WARNING) << "Failed to create an SSL client socket.";
    return PP_ERROR_FAILED;
  }

  const ppapi::host::ReplyMessageContext reply_context(
      context->MakeReplyMessageContext());
  int net_result = socket_->Connect(
      base::Bind(&PepperTCPSocketMessageFilter::OnSSLHandshakeCompleted,
                 base::Unretained(this), reply_context));
  if (net_result != net::ERR_IO_PENDING)
    OnSSLHandshakeCompleted(reply_context, net_result);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgRead(
    const ppapi::host::HostMessageContext* context,
    int32_t bytes_to_read) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!IsConnected() || end_of_file_reached_)
    return PP_ERROR_FAILED;
  if (read_buffer_.get())
    return PP_ERROR_INPROGRESS;
  if (bytes_to_read <= 0 ||
      bytes_to_read > TCPSocketResourceBase::kMaxReadSize) {
    return PP_ERROR_BADARGUMENT;
  }

  ppapi::host::ReplyMessageContext reply_context(
      context->MakeReplyMessageContext());
  read_buffer_ = new net::IOBuffer(bytes_to_read);
  int net_result = socket_->Read(
      read_buffer_.get(),
      bytes_to_read,
      base::Bind(&PepperTCPSocketMessageFilter::OnReadCompleted,
                 base::Unretained(this), reply_context));
  if (net_result != net::ERR_IO_PENDING)
    OnReadCompleted(reply_context, net_result);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgWrite(
    const ppapi::host::HostMessageContext* context,
    const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!IsConnected())
    return PP_ERROR_FAILED;
  if (write_buffer_base_.get() || write_buffer_.get())
    return PP_ERROR_INPROGRESS;

  size_t data_size = data.size();
  if (data_size == 0 ||
      data_size > static_cast<size_t>(TCPSocketResourceBase::kMaxWriteSize)) {
    return PP_ERROR_BADARGUMENT;
  }

  write_buffer_base_ = new net::IOBuffer(data_size);
  memcpy(write_buffer_base_->data(), data.data(), data_size);
  write_buffer_ =
      new net::DrainableIOBuffer(write_buffer_base_.get(), data_size);
  DoWrite(context->MakeReplyMessageContext());
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTCPSocketMessageFilter::OnMsgDisconnect(
    const ppapi::host::HostMessageContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SetState(STATE_CLOSED);
  return PP_OK;
}

int32_t PepperTCPSocketMessageFilter::OnMsgSetOption(
    const ppapi::host::HostMessageContext* context,
    PP_TCPSocket_Option name,
    const ppapi::SocketOptionData& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!IsConnected() || IsSsl())
    return PP_ERROR_FAILED;

  net::TCPClientSocket* tcp_socket =
      static_cast<net::TCPClientSocket*>(socket_.get());
  DCHECK(tcp_socket);

  switch (name) {
    case PP_TCPSOCKET_OPTION_NO_DELAY: {
      bool boolean_value = false;
      if (!value.GetBool(&boolean_value))
        return PP_ERROR_BADARGUMENT;
      return tcp_socket->SetNoDelay(boolean_value) ? PP_OK : PP_ERROR_FAILED;
    }
    case PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE:
    case PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE: {
      int32_t integer_value = 0;
      if (!value.GetInt32(&integer_value) || integer_value <= 0)
        return PP_ERROR_BADARGUMENT;

      bool result = false;
      if (name == PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE) {
        if (integer_value > TCPSocketResourceBase::kMaxSendBufferSize)
          return PP_ERROR_BADARGUMENT;
        result = tcp_socket->SetSendBufferSize(integer_value);
      } else {
        if (integer_value > TCPSocketResourceBase::kMaxReceiveBufferSize)
          return PP_ERROR_BADARGUMENT;
        result = tcp_socket->SetReceiveBufferSize(integer_value);
      }
      return result ? PP_OK : PP_ERROR_FAILED;
    }
    default: {
      NOTREACHED();
      return PP_ERROR_BADARGUMENT;
    }
  }
  return PP_ERROR_FAILED;
}

void PepperTCPSocketMessageFilter::DoConnect(
    const ppapi::host::ReplyMessageContext& context,
    const std::string& host,
    uint16_t port,
    ResourceContext* resource_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (state_ != STATE_BEFORE_CONNECT) {
    SendConnectError(context, PP_ERROR_FAILED);
    return;
  }

  SetState(STATE_CONNECT_IN_PROGRESS);
  net::HostResolver::RequestInfo request_info(net::HostPortPair(host, port));
  resolver_.reset(new net::SingleRequestHostResolver(
      resource_context->GetHostResolver()));
  int net_result = resolver_->Resolve(
      request_info,
      net::DEFAULT_PRIORITY,
      &address_list_,
      base::Bind(&PepperTCPSocketMessageFilter::OnResolveCompleted,
                 base::Unretained(this), context),
      net::BoundNetLog());
  if (net_result != net::ERR_IO_PENDING)
    OnResolveCompleted(context, net_result);
}

void PepperTCPSocketMessageFilter::DoConnectWithNetAddress(
    const ppapi::host::ReplyMessageContext& context,
    const PP_NetAddress_Private& net_addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (state_ != STATE_BEFORE_CONNECT) {
    SendConnectError(context, PP_ERROR_FAILED);
    return;
  }

  net::IPAddressNumber address;
  int port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(net_addr, &address,
                                                     &port)) {
    SendConnectError(context, PP_ERROR_ADDRESS_INVALID);
    return;
  }

  // Copy the single IPEndPoint to address_list_.
  address_list_.clear();
  address_list_.push_back(net::IPEndPoint(address, port));
  SetState(STATE_CONNECT_IN_PROGRESS);
  StartConnect(context);
}

void PepperTCPSocketMessageFilter::DoWrite(
    const ppapi::host::ReplyMessageContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(write_buffer_base_.get());
  DCHECK(write_buffer_.get());
  DCHECK_GT(write_buffer_->BytesRemaining(), 0);

  int net_result = socket_->Write(
      write_buffer_.get(),
      write_buffer_->BytesRemaining(),
      base::Bind(&PepperTCPSocketMessageFilter::OnWriteCompleted,
                 base::Unretained(this), context));
  if (net_result != net::ERR_IO_PENDING)
    OnWriteCompleted(context, net_result);
}

void PepperTCPSocketMessageFilter::OnResolveCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (state_ != STATE_CONNECT_IN_PROGRESS) {
    SendConnectError(context, PP_ERROR_FAILED);
    SetState(STATE_CLOSED);
    return;
  }

  if (net_result != net::OK) {
    SendConnectError(context, NetErrorToPepperError(net_result));
    SetState(STATE_BEFORE_CONNECT);
    return;
  }

  StartConnect(context);
}

void PepperTCPSocketMessageFilter::StartConnect(
    const ppapi::host::ReplyMessageContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (state_ != STATE_CONNECT_IN_PROGRESS) {
    SendConnectError(context, PP_ERROR_FAILED);
    SetState(STATE_CLOSED);
    return;
  }

  socket_.reset(new net::TCPClientSocket(address_list_, NULL,
                                         net::NetLog::Source()));
  int net_result = socket_->Connect(
      base::Bind(&PepperTCPSocketMessageFilter::OnConnectCompleted,
                 base::Unretained(this), context));
  if (net_result != net::ERR_IO_PENDING)
    OnConnectCompleted(context, net_result);
}

void PepperTCPSocketMessageFilter::OnConnectCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(socket_.get());

  if (state_ != STATE_CONNECT_IN_PROGRESS) {
    SendConnectError(context, PP_ERROR_FAILED);
    SetState(STATE_CLOSED);
    return;
  }

  int32_t pp_result = NetErrorToPepperError(net_result);
  do {
    if (pp_result != PP_OK)
      break;

    net::IPEndPoint ip_end_point_local;
    net::IPEndPoint ip_end_point_remote;
    pp_result = NetErrorToPepperError(
        socket_->GetLocalAddress(&ip_end_point_local));
    if (pp_result != PP_OK)
      break;
    pp_result = NetErrorToPepperError(
        socket_->GetPeerAddress(&ip_end_point_remote));
    if (pp_result != PP_OK)
      break;

    PP_NetAddress_Private local_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;
    PP_NetAddress_Private remote_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;
    if (!NetAddressPrivateImpl::IPEndPointToNetAddress(
            ip_end_point_local.address(),
            ip_end_point_local.port(),
            &local_addr) ||
        !NetAddressPrivateImpl::IPEndPointToNetAddress(
            ip_end_point_remote.address(),
            ip_end_point_remote.port(),
            &remote_addr)) {
      pp_result = PP_ERROR_ADDRESS_INVALID;
      break;
    }

    SendConnectReply(context, PP_OK, local_addr, remote_addr);
    SetState(STATE_CONNECTED);
    return;
  } while (false);

  SendConnectError(context, pp_result);
  SetState(STATE_BEFORE_CONNECT);
}

void PepperTCPSocketMessageFilter::OnSSLHandshakeCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (state_ != STATE_SSL_HANDSHAKE_IN_PROGRESS) {
    SendSSLHandshakeReply(context, PP_ERROR_FAILED);
    SetState(STATE_CLOSED);
    return;
  }
  SendSSLHandshakeReply(context, NetErrorToPepperError(net_result));
  SetState(net_result == net::OK ?
           STATE_SSL_CONNECTED :
           STATE_SSL_HANDSHAKE_FAILED);
}

void PepperTCPSocketMessageFilter::OnReadCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(read_buffer_.get());

  if (net_result > 0) {
    SendReadReply(context,
                  PP_OK,
                  std::string(read_buffer_->data(), net_result));
  } else if (net_result == 0) {
    end_of_file_reached_ = true;
    SendReadReply(context, PP_OK, std::string());
  } else {
    SendReadError(context, NetErrorToPepperError(net_result));
  }
  read_buffer_ = NULL;
}

void PepperTCPSocketMessageFilter::OnWriteCompleted(
    const ppapi::host::ReplyMessageContext& context,
    int net_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(write_buffer_base_.get());
  DCHECK(write_buffer_.get());

  // Note: For partial writes of 0 bytes, don't continue writing to avoid a
  // likely infinite loop.
  if (net_result > 0) {
    write_buffer_->DidConsume(net_result);
    if (write_buffer_->BytesRemaining() > 0) {
      DoWrite(context);
      return;
    }
  }

  if (net_result >= 0)
    SendWriteReply(context, write_buffer_->BytesConsumed());
  else
    SendWriteReply(context, NetErrorToPepperError(net_result));

  write_buffer_ = NULL;
  write_buffer_base_ = NULL;
}

void PepperTCPSocketMessageFilter::SendConnectReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  SendReply(reply_context,
            PpapiPluginMsg_TCPSocket_ConnectReply(local_addr, remote_addr));
}

void PepperTCPSocketMessageFilter::SendConnectError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_error) {
  SendConnectReply(context,
                   pp_error,
                   NetAddressPrivateImpl::kInvalidNetAddress,
                   NetAddressPrivateImpl::kInvalidNetAddress);
}

void PepperTCPSocketMessageFilter::SendSSLHandshakeReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  ppapi::PPB_X509Certificate_Fields certificate_fields;
  if (pp_result == PP_OK) {
    // Our socket is guaranteed to be an SSL socket if we get here.
    net::SSLClientSocket* ssl_socket =
        static_cast<net::SSLClientSocket*>(socket_.get());
    net::SSLInfo ssl_info;
    ssl_socket->GetSSLInfo(&ssl_info);
    if (ssl_info.cert.get()) {
      pepper_socket_utils::GetCertificateFields(*ssl_info.cert.get(),
                                                &certificate_fields);
    }
  }
  SendReply(reply_context,
            PpapiPluginMsg_TCPSocket_SSLHandshakeReply(certificate_fields));
}

void PepperTCPSocketMessageFilter::SendReadReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result,
    const std::string& data) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  SendReply(reply_context, PpapiPluginMsg_TCPSocket_ReadReply(data));
}

void PepperTCPSocketMessageFilter::SendReadError(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_error) {
  SendReadReply(context, pp_error, std::string());
}

void PepperTCPSocketMessageFilter::SendWriteReply(
    const ppapi::host::ReplyMessageContext& context,
    int32_t pp_result) {
  ppapi::host::ReplyMessageContext reply_context(context);
  reply_context.params.set_result(pp_result);
  SendReply(reply_context, PpapiPluginMsg_TCPSocket_WriteReply());
}

bool PepperTCPSocketMessageFilter::IsConnected() const {
  return state_ == STATE_CONNECTED || state_ == STATE_SSL_CONNECTED;
}

bool PepperTCPSocketMessageFilter::IsSsl() const {
  return state_ == STATE_SSL_HANDSHAKE_IN_PROGRESS ||
      state_ == STATE_SSL_CONNECTED ||
      state_ == STATE_SSL_HANDSHAKE_FAILED;
}

void PepperTCPSocketMessageFilter::SetState(State state) {
  state_ = state;
  if (state_ == STATE_CLOSED && socket_) {
    // Make sure no further callbacks from socket_.
    socket_->Disconnect();
    socket_.reset();
  }
}

}  // namespace content
