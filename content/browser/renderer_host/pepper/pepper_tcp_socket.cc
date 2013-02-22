// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_tcp_socket.h"

#include <string.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/single_request_host_resolver.h"
#include "net/base/x509_certificate.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"
#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"

using ppapi::NetAddressPrivateImpl;

namespace content {

PepperTCPSocket::PepperTCPSocket(
    PepperMessageFilter* manager,
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32 socket_id)
    : manager_(manager),
      routing_id_(routing_id),
      plugin_dispatcher_id_(plugin_dispatcher_id),
      socket_id_(socket_id),
      connection_state_(BEFORE_CONNECT),
      end_of_file_reached_(false) {
  DCHECK(manager);
}

PepperTCPSocket::PepperTCPSocket(
    PepperMessageFilter* manager,
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32 socket_id,
    net::StreamSocket* socket)
    : manager_(manager),
      routing_id_(routing_id),
      plugin_dispatcher_id_(plugin_dispatcher_id),
      socket_id_(socket_id),
      connection_state_(CONNECTED),
      end_of_file_reached_(false),
      socket_(socket) {
  DCHECK(manager);
}

PepperTCPSocket::~PepperTCPSocket() {
  // Make sure no further callbacks from socket_.
  if (socket_.get())
    socket_->Disconnect();
}

void PepperTCPSocket::Connect(const std::string& host, uint16_t port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (connection_state_ != BEFORE_CONNECT) {
    SendConnectACKError();
    return;
  }

  connection_state_ = CONNECT_IN_PROGRESS;
  net::HostResolver::RequestInfo request_info(net::HostPortPair(host, port));
  resolver_.reset(new net::SingleRequestHostResolver(
      manager_->GetHostResolver()));
  int result = resolver_->Resolve(
      request_info, &address_list_,
      base::Bind(&PepperTCPSocket::OnResolveCompleted, base::Unretained(this)),
      net::BoundNetLog());
  if (result != net::ERR_IO_PENDING)
    OnResolveCompleted(result);
}

void PepperTCPSocket::ConnectWithNetAddress(
    const PP_NetAddress_Private& net_addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::IPAddressNumber address;
  int port;
  if (connection_state_ != BEFORE_CONNECT ||
      !NetAddressPrivateImpl::NetAddressToIPEndPoint(net_addr,
                                                     &address,
                                                     &port)) {
    SendConnectACKError();
    return;
  }

  // Copy the single IPEndPoint to address_list_.
  address_list_.clear();
  address_list_.push_back(net::IPEndPoint(address, port));
  connection_state_ = CONNECT_IN_PROGRESS;
  StartConnect(address_list_);
}

void PepperTCPSocket::SSLHandshake(
    const std::string& server_name,
    uint16_t server_port,
    const std::vector<std::vector<char> >& trusted_certs,
    const std::vector<std::vector<char> >& untrusted_certs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Allow to do SSL handshake only if currently the socket has been connected
  // and there isn't pending read or write.
  // IsConnected() includes the state that SSL handshake has been finished and
  // therefore isn't suitable here.
  if (connection_state_ != CONNECTED || read_buffer_.get() ||
      write_buffer_base_.get() || write_buffer_.get()) {
    SendSSLHandshakeACK(false);
    return;
  }

  connection_state_ = SSL_HANDSHAKE_IN_PROGRESS;
  // TODO(raymes,rsleevi): Use trusted/untrusted certificates when connecting.

  net::ClientSocketHandle* handle = new net::ClientSocketHandle();
  handle->set_socket(socket_.release());
  net::ClientSocketFactory* factory =
      net::ClientSocketFactory::GetDefaultFactory();
  net::HostPortPair host_port_pair(server_name, server_port);
  net::SSLClientSocketContext ssl_context;
  ssl_context.cert_verifier = manager_->GetCertVerifier();
  socket_.reset(factory->CreateSSLClientSocket(
      handle, host_port_pair, manager_->ssl_config(), ssl_context));
  if (!socket_.get()) {
    LOG(WARNING) << "Failed to create an SSL client socket.";
    OnSSLHandshakeCompleted(net::ERR_UNEXPECTED);
    return;
  }

  int result = socket_->Connect(
      base::Bind(&PepperTCPSocket::OnSSLHandshakeCompleted,
                 base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnSSLHandshakeCompleted(result);
}

void PepperTCPSocket::Read(int32 bytes_to_read) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!IsConnected() || end_of_file_reached_ || read_buffer_.get() ||
      bytes_to_read <= 0) {
    SendReadACKError();
    return;
  }

  if (bytes_to_read > ppapi::TCPSocketPrivateImpl::kMaxReadSize) {
    NOTREACHED();
    bytes_to_read = ppapi::TCPSocketPrivateImpl::kMaxReadSize;
  }

  read_buffer_ = new net::IOBuffer(bytes_to_read);
  int result = socket_->Read(read_buffer_, bytes_to_read,
                             base::Bind(&PepperTCPSocket::OnReadCompleted,
                                        base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnReadCompleted(result);
}

void PepperTCPSocket::Write(const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!IsConnected() || write_buffer_base_.get() || write_buffer_.get() ||
      data.empty()) {
    SendWriteACKError();
    return;
  }

  int data_size = data.size();
  if (data_size > ppapi::TCPSocketPrivateImpl::kMaxWriteSize) {
    NOTREACHED();
    data_size = ppapi::TCPSocketPrivateImpl::kMaxWriteSize;
  }

  write_buffer_base_ = new net::IOBuffer(data_size);
  memcpy(write_buffer_base_->data(), data.data(), data_size);
  write_buffer_ = new net::DrainableIOBuffer(write_buffer_base_, data_size);
  DoWrite();
}

void PepperTCPSocket::SetBoolOption(uint32_t name, bool value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(socket_.get());

  switch (name) {
    case PP_TCPSOCKETOPTION_NO_DELAY:
      if (!IsSsl()) {
        net::TCPClientSocket* tcp_socket =
            static_cast<net::TCPClientSocket*>(socket_.get());
        SendSetBoolOptionACK(tcp_socket->SetNoDelay(value));
      } else {
        SendSetBoolOptionACK(false);
      }
      return;
    default:
      break;
  }

  NOTREACHED();
  SendSetBoolOptionACK(false);
}

void PepperTCPSocket::StartConnect(const net::AddressList& addresses) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS);

  socket_.reset(new net::TCPClientSocket(addresses, NULL,
                                         net::NetLog::Source()));
  int result = socket_->Connect(
      base::Bind(&PepperTCPSocket::OnConnectCompleted,
                 base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnConnectCompleted(result);
}

void PepperTCPSocket::SendConnectACKError() {
  manager_->Send(new PpapiMsg_PPBTCPSocket_ConnectACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false,
      NetAddressPrivateImpl::kInvalidNetAddress,
      NetAddressPrivateImpl::kInvalidNetAddress));
}

// static
bool PepperTCPSocket::GetCertificateFields(
    const net::X509Certificate& cert,
    ppapi::PPB_X509Certificate_Fields* fields) {
  const net::CertPrincipal& issuer = cert.issuer();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_COMMON_NAME,
      new base::StringValue(issuer.common_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_LOCALITY_NAME,
      new base::StringValue(issuer.locality_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_STATE_OR_PROVINCE_NAME,
      new base::StringValue(issuer.state_or_province_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_COUNTRY_NAME,
      new base::StringValue(issuer.country_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_ORGANIZATION_NAME,
      new base::StringValue(JoinString(issuer.organization_names, '\n')));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_ISSUER_ORGANIZATION_UNIT_NAME,
      new base::StringValue(JoinString(issuer.organization_unit_names, '\n')));

  const net::CertPrincipal& subject = cert.subject();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_COMMON_NAME,
      new base::StringValue(subject.common_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_LOCALITY_NAME,
      new base::StringValue(subject.locality_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_STATE_OR_PROVINCE_NAME,
      new base::StringValue(subject.state_or_province_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_COUNTRY_NAME,
      new base::StringValue(subject.country_name));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_ORGANIZATION_NAME,
      new base::StringValue(JoinString(subject.organization_names, '\n')));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SUBJECT_ORGANIZATION_UNIT_NAME,
      new base::StringValue(JoinString(subject.organization_unit_names, '\n')));

  const std::string& serial_number = cert.serial_number();
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_SERIAL_NUMBER,
      base::BinaryValue::CreateWithCopiedBuffer(serial_number.data(),
                                                serial_number.length()));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_VALIDITY_NOT_BEFORE,
      new base::FundamentalValue(cert.valid_start().ToDoubleT()));
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_VALIDITY_NOT_AFTER,
      new base::FundamentalValue(cert.valid_expiry().ToDoubleT()));
  std::string der;
  net::X509Certificate::GetDEREncoded(cert.os_cert_handle(), &der);
  fields->SetField(PP_X509CERTIFICATE_PRIVATE_RAW,
      base::BinaryValue::CreateWithCopiedBuffer(der.data(), der.length()));
  return true;
}

// static
bool PepperTCPSocket::GetCertificateFields(
    const char* der,
    uint32_t length,
    ppapi::PPB_X509Certificate_Fields* fields) {
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBytes(der, length);
  if (!cert.get())
    return false;
  return GetCertificateFields(*cert, fields);
}

void PepperTCPSocket::SendReadACKError() {
  manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
    routing_id_, plugin_dispatcher_id_, socket_id_, false, std::string()));
}

void PepperTCPSocket::SendWriteACKError() {
  manager_->Send(new PpapiMsg_PPBTCPSocket_WriteACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false, 0));
}

void PepperTCPSocket::SendSSLHandshakeACK(bool succeeded) {
  ppapi::PPB_X509Certificate_Fields certificate_fields;
  if (succeeded) {
    // Our socket is guaranteed to be an SSL socket if we get here.
    net::SSLClientSocket* ssl_socket =
        static_cast<net::SSLClientSocket*>(socket_.get());
    net::SSLInfo ssl_info;
    ssl_socket->GetSSLInfo(&ssl_info);
    if (ssl_info.cert.get())
      GetCertificateFields(*ssl_info.cert, &certificate_fields);
  }
  manager_->Send(new PpapiMsg_PPBTCPSocket_SSLHandshakeACK(
      routing_id_,
      plugin_dispatcher_id_,
      socket_id_,
      succeeded,
      certificate_fields));
}

void PepperTCPSocket::SendSetBoolOptionACK(bool succeeded) {
  manager_->Send(new PpapiMsg_PPBTCPSocket_SetBoolOptionACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, succeeded));
}

void PepperTCPSocket::OnResolveCompleted(int result) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS);

  if (result != net::OK) {
    SendConnectACKError();
    connection_state_ = BEFORE_CONNECT;
    return;
  }

  StartConnect(address_list_);
}

void PepperTCPSocket::OnConnectCompleted(int result) {
  DCHECK(connection_state_ == CONNECT_IN_PROGRESS && socket_.get());

  if (result != net::OK) {
    SendConnectACKError();
    connection_state_ = BEFORE_CONNECT;
  } else {
    net::IPEndPoint ip_end_point_local;
    net::IPEndPoint ip_end_point_remote;
    PP_NetAddress_Private local_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;
    PP_NetAddress_Private remote_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;

    if (socket_->GetLocalAddress(&ip_end_point_local) != net::OK ||
        !NetAddressPrivateImpl::IPEndPointToNetAddress(
            ip_end_point_local.address(),
            ip_end_point_local.port(),
            &local_addr) ||
        socket_->GetPeerAddress(&ip_end_point_remote) != net::OK ||
        !NetAddressPrivateImpl::IPEndPointToNetAddress(
            ip_end_point_remote.address(),
            ip_end_point_remote.port(),
            &remote_addr)) {
      SendConnectACKError();
      connection_state_ = BEFORE_CONNECT;
    } else {
      manager_->Send(new PpapiMsg_PPBTCPSocket_ConnectACK(
          routing_id_, plugin_dispatcher_id_, socket_id_, true,
          local_addr, remote_addr));
      connection_state_ = CONNECTED;
    }
  }
}

void PepperTCPSocket::OnSSLHandshakeCompleted(int result) {
  DCHECK(connection_state_ == SSL_HANDSHAKE_IN_PROGRESS);

  bool succeeded = result == net::OK;
  SendSSLHandshakeACK(succeeded);
  connection_state_ = succeeded ? SSL_CONNECTED : SSL_HANDSHAKE_FAILED;
}

void PepperTCPSocket::OnReadCompleted(int result) {
  DCHECK(read_buffer_.get());

  if (result > 0) {
    manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true,
        std::string(read_buffer_->data(), result)));
  } else if (result == 0) {
    end_of_file_reached_ = true;
    manager_->Send(new PpapiMsg_PPBTCPSocket_ReadACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true, std::string()));
  } else {
    SendReadACKError();
  }
  read_buffer_ = NULL;
}

void PepperTCPSocket::OnWriteCompleted(int result) {
  DCHECK(write_buffer_base_.get());
  DCHECK(write_buffer_.get());

  // Note: For partial writes of 0 bytes, don't continue writing to avoid a
  // likely infinite loop.
  if (result > 0) {
    write_buffer_->DidConsume(result);
    if (write_buffer_->BytesRemaining() > 0) {
      DoWrite();
      return;
    }
  }

  if (result >= 0) {
    manager_->Send(new PpapiMsg_PPBTCPSocket_WriteACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true,
        write_buffer_->BytesConsumed()));
  } else {
    SendWriteACKError();
  }

  write_buffer_ = NULL;
  write_buffer_base_ = NULL;
}

bool PepperTCPSocket::IsConnected() const {
  return connection_state_ == CONNECTED || connection_state_ == SSL_CONNECTED;
}

bool PepperTCPSocket::IsSsl() const {
 return connection_state_ == SSL_HANDSHAKE_IN_PROGRESS ||
     connection_state_ == SSL_CONNECTED ||
     connection_state_ == SSL_HANDSHAKE_FAILED;
}

void PepperTCPSocket::DoWrite() {
  DCHECK(write_buffer_base_.get());
  DCHECK(write_buffer_.get());
  DCHECK_GT(write_buffer_->BytesRemaining(), 0);

  int result = socket_->Write(write_buffer_, write_buffer_->BytesRemaining(),
                              base::Bind(&PepperTCPSocket::OnWriteCompleted,
                                         base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnWriteCompleted(result);
}

}  // namespace content
