// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_socket.h"

#include <string.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "chrome/browser/extensions/api/cast_channel/cast_channel.pb.h"
#include "chrome/browser/extensions/api/cast_channel/cast_message_util.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_info.h"

namespace {

// Allowed schemes for Cast device URLs.
const char kCastInsecureScheme[] = "cast";
const char kCastSecureScheme[] = "casts";

// Size of the message header, in bytes.  Don't use sizeof(MessageHeader)
// because of alignment; instead, sum the sizeof() for the fields.
const uint32 kMessageHeaderSize = sizeof(uint32);

// The default keepalive delay.  On Linux, keepalives probes will be sent after
// the socket is idle for this length of time, and the socket will be closed
// after 9 failed probes.  So the total idle time before close is 10 *
// kTcpKeepAliveDelaySecs.
const int kTcpKeepAliveDelaySecs = 10;

}  // namespace

namespace extensions {

static base::LazyInstance<
  ProfileKeyedAPIFactory<ApiResourceManager<api::cast_channel::CastSocket> > >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
ProfileKeyedAPIFactory<ApiResourceManager<api::cast_channel::CastSocket> >*
ApiResourceManager<api::cast_channel::CastSocket>::GetFactoryInstance() {
  return &g_factory.Get();
}

namespace api {
namespace cast_channel {

const uint32 kMaxMessageSize = 65536;

CastSocket::CastSocket(const std::string& owner_extension_id,
                       const GURL& url, CastSocket::Delegate* delegate,
                       net::NetLog* net_log) :
    ApiResource(owner_extension_id),
    channel_id_(0),
    url_(url),
    delegate_(delegate),
    is_secure_(false),
    error_state_(CHANNEL_ERROR_NONE),
    ready_state_(READY_STATE_NONE),
    write_callback_pending_(false),
    read_callback_pending_(false),
    current_message_size_(0),
    net_log_(net_log),
    next_state_(CONN_STATE_NONE) {
  DCHECK(net_log_);
  net_log_source_.type = net::NetLog::SOURCE_SOCKET;
  net_log_source_.id = net_log_->NextID();

  // We reuse these buffers for each message.
  header_read_buffer_ = new net::GrowableIOBuffer();
  header_read_buffer_->SetCapacity(kMessageHeaderSize);
  body_read_buffer_ = new net::GrowableIOBuffer();
  body_read_buffer_->SetCapacity(kMaxMessageSize);
  current_read_buffer_ = header_read_buffer_;
}

CastSocket::~CastSocket() { }

const GURL& CastSocket::url() const {
  return url_;
}

bool CastSocket::ExtractPeerCert(std::string* cert) {
  CHECK(peer_cert_.empty());
  net::SSLInfo ssl_info;
  if (!socket_->GetSSLInfo(&ssl_info) || !ssl_info.cert.get())
    return false;
  bool result = net::X509Certificate::GetDEREncoded(
     ssl_info.cert->os_cert_handle(), cert);
  if (result)
    DVLOG(1) << "Successfully extracted peer certificate: " << *cert;
  return result;
}

scoped_ptr<net::TCPClientSocket> CastSocket::CreateTcpSocket() {
  net::AddressList addresses(ip_endpoint_);
  scoped_ptr<net::TCPClientSocket> tcp_socket(
      new net::TCPClientSocket(addresses, net_log_, net_log_source_));
  // Enable keepalive
  tcp_socket->SetKeepAlive(true, kTcpKeepAliveDelaySecs);
  return tcp_socket.Pass();
}

scoped_ptr<net::SSLClientSocket> CastSocket::CreateSslSocket() {
  net::SSLConfig ssl_config;
  // If a peer cert was extracted in a previous attempt to connect, then
  // whitelist that cert.
  if (!peer_cert_.empty()) {
    net::SSLConfig::CertAndStatus cert_and_status;
    cert_and_status.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
    cert_and_status.der_cert = peer_cert_;
    ssl_config.allowed_bad_certs.push_back(cert_and_status);
  }

  cert_verifier_.reset(net::CertVerifier::CreateDefault());
  transport_security_state_.reset(new net::TransportSecurityState);
  net::SSLClientSocketContext context;
  // CertVerifier and TransportSecurityState are owned by us, not the
  // context object.
  context.cert_verifier = cert_verifier_.get();
  context.transport_security_state = transport_security_state_.get();

  scoped_ptr<net::ClientSocketHandle> connection(new net::ClientSocketHandle);
  connection->SetSocket(tcp_socket_.PassAs<net::StreamSocket>());
  net::HostPortPair host_and_port = net::HostPortPair::FromIPEndPoint(
      ip_endpoint_);

  return net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
      connection.Pass(), host_and_port, ssl_config, context);
}

void CastSocket::OnConnectComplete(int result) {
  int rv = DoConnectLoop(result);
  if (rv != net::ERR_IO_PENDING)
    DoConnectCallback(rv);
}

void CastSocket::Connect(const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  int result = net::ERR_CONNECTION_FAILED;
  DVLOG(1) << "Connect readyState = " << ready_state_;
  if (ready_state_ != READY_STATE_NONE) {
    callback.Run(result);
    return;
  }
  if (!ParseChannelUrl(url_)) {
    CloseWithError(cast_channel::CHANNEL_ERROR_CONNECT_ERROR);
    // TODO(mfoltz): Signal channel errors via |callback|
    callback.Run(result);
    return;
  }
  connect_callback_ = callback;
  next_state_ = CONN_STATE_TCP_CONNECT;
  int rv = DoConnectLoop(net::OK);
  if (rv != net::ERR_IO_PENDING)
    DoConnectCallback(rv);
}

// This method performs the state machine transitions for connection flow.
// There are two entry points to this method:
// 1. public Connect method: this starts the flow
// 2. OnConnectComplete: callback method called when an async operation
//    is done. OnConnectComplete calls this method to continue the state
//    machine transitions.
int CastSocket::DoConnectLoop(int result) {
  // Network operations can either finish sycnronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // correct state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    ConnectionState state = next_state_;
    // All the Do* methods do not set next_state_ in case of an
    // error. So set next_state_ to NONE to figure out if the Do*
    // method changed state or not.
    next_state_ = CONN_STATE_NONE;
    switch (state) {
      case CONN_STATE_TCP_CONNECT:
        rv = DoTcpConnect();
        break;
      case CONN_STATE_TCP_CONNECT_COMPLETE:
        rv = DoTcpConnectComplete(rv);
        break;
      case CONN_STATE_SSL_CONNECT:
        DCHECK_EQ(net::OK, rv);
        rv = DoSslConnect();
        break;
      case CONN_STATE_SSL_CONNECT_COMPLETE:
        rv = DoSslConnectComplete(rv);
        break;
      default:
        NOTREACHED() << "BUG in CastSocket state machine code";
        break;
    }
  } while (rv != net::ERR_IO_PENDING && next_state_ != CONN_STATE_NONE);
  // Get out of the loop either when:
  // a. A network operation is pending, OR
  // b. The Do* method called did not change state

  return rv;
}

int CastSocket::DoTcpConnect() {
  next_state_ = CONN_STATE_TCP_CONNECT_COMPLETE;
  tcp_socket_ = CreateTcpSocket();
  return tcp_socket_->Connect(
      base::Bind(&CastSocket::OnConnectComplete, AsWeakPtr()));
}

int CastSocket::DoTcpConnectComplete(int result) {
  if (result == net::OK)
    next_state_ = CONN_STATE_SSL_CONNECT;
  return result;
}

int CastSocket::DoSslConnect() {
  next_state_ = CONN_STATE_SSL_CONNECT_COMPLETE;
  socket_ = CreateSslSocket();
  return socket_->Connect(
      base::Bind(&CastSocket::OnConnectComplete, AsWeakPtr()));
}

int CastSocket::DoSslConnectComplete(int result) {
  // TODO(mfoltz,munjal): Authenticate the channel if is_secure_ == true.
  if (result == net::ERR_CERT_AUTHORITY_INVALID &&
             peer_cert_.empty() &&
             ExtractPeerCert(&peer_cert_)) {
    next_state_ = CONN_STATE_TCP_CONNECT;
  }
  return result;
}

void CastSocket::DoConnectCallback(int result) {
  ready_state_ = (result == net::OK) ? READY_STATE_OPEN : READY_STATE_CLOSED;
  error_state_ = (result == net::OK) ?
      CHANNEL_ERROR_NONE : CHANNEL_ERROR_CONNECT_ERROR;
  base::ResetAndReturn(&connect_callback_).Run(result);
  if (result == net::OK)
    ReadData();
}

void CastSocket::Close(const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Close ReadyState = " << ready_state_;
  tcp_socket_.reset(NULL);
  socket_.reset(NULL);
  cert_verifier_.reset(NULL);
  transport_security_state_.reset(NULL);
  ready_state_ = READY_STATE_CLOSED;
  callback.Run(net::OK);
}

void CastSocket::SendMessage(const MessageInfo& message,
                             const net::CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "Send ReadyState " << ready_state_;
  int result = net::ERR_FAILED;
  if (ready_state_ != READY_STATE_OPEN) {
    callback.Run(result);
    return;
  }
  WriteRequest write_request(callback);
  CastMessage message_proto;
  if (!MessageInfoToCastMessage(message, &message_proto) ||
      !write_request.SetContent(message_proto)) {
    CloseWithError(cast_channel::CHANNEL_ERROR_INVALID_MESSAGE);
    // TODO(mfoltz): Do a better job of signaling cast_channel errors to the
    // caller.
    callback.Run(net::OK);
    return;
  }
  write_queue_.push(write_request);
  WriteData();
}

void CastSocket::WriteData() {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "WriteData q = " << write_queue_.size();
  if (write_queue_.empty() || write_callback_pending_)
    return;

  WriteRequest& request = write_queue_.front();
  if (ready_state_ != READY_STATE_OPEN) {
    request.callback.Run(net::ERR_FAILED);
    return;
  }

  DVLOG(1) << "WriteData byte_count = " << request.io_buffer->size() <<
    " bytes_written " << request.io_buffer->BytesConsumed();

  write_callback_pending_ = true;
  int result = socket_->Write(
      request.io_buffer.get(),
      request.io_buffer->BytesRemaining(),
      base::Bind(&CastSocket::OnWriteData, AsWeakPtr()));

  DVLOG(1) << "WriteData result = " << result;

  if (result != net::ERR_IO_PENDING)
    OnWriteData(result);
}

void CastSocket::OnWriteData(int result) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "OnWriteComplete result = " << result;
  DCHECK(write_callback_pending_);
  DCHECK(!write_queue_.empty());
  write_callback_pending_ = false;
  WriteRequest& request = write_queue_.front();
  scoped_refptr<net::DrainableIOBuffer> io_buffer = request.io_buffer;

  if (result >= 0) {
    io_buffer->DidConsume(result);
    if (io_buffer->BytesRemaining() > 0) {
      DVLOG(1) << "OnWriteComplete size = " << io_buffer->size() <<
        " consumed " << io_buffer->BytesConsumed() <<
        " remaining " << io_buffer->BytesRemaining() <<
        " # requests " << write_queue_.size();
      WriteData();
      return;
    }
    DCHECK_EQ(io_buffer->BytesConsumed(), io_buffer->size());
    DCHECK_EQ(io_buffer->BytesRemaining(), 0);
    result = io_buffer->BytesConsumed();
  }

  request.callback.Run(result);
  write_queue_.pop();

  DVLOG(1) << "OnWriteComplete size = " << io_buffer->size() <<
    " consumed " << io_buffer->BytesConsumed() <<
    " remaining " << io_buffer->BytesRemaining() <<
    " # requests " << write_queue_.size();

  if (result < 0) {
    CloseWithError(CHANNEL_ERROR_SOCKET_ERROR);
    return;
  }

  if (!write_queue_.empty())
    WriteData();
}

void CastSocket::ReadData() {
  DCHECK(CalledOnValidThread());
  if (!socket_.get() || ready_state_ != READY_STATE_OPEN) {
    return;
  }
  DCHECK(!read_callback_pending_);
  read_callback_pending_ = true;
  // Figure out if we are reading the header or body, and the remaining bytes.
  uint32 num_bytes_to_read = 0;
  if (header_read_buffer_->RemainingCapacity() > 0) {
    current_read_buffer_ = header_read_buffer_;
    num_bytes_to_read = header_read_buffer_->RemainingCapacity();
    DCHECK_LE(num_bytes_to_read, kMessageHeaderSize);
  } else {
    DCHECK_GT(current_message_size_, 0U);
    num_bytes_to_read = current_message_size_ - body_read_buffer_->offset();
    current_read_buffer_ = body_read_buffer_;
    DCHECK_LE(num_bytes_to_read, kMaxMessageSize);
  }
  DCHECK_GT(num_bytes_to_read, 0U);
  // We read up to num_bytes_to_read into |current_read_buffer_|.
  int result = socket_->Read(
      current_read_buffer_.get(),
      num_bytes_to_read,
      base::Bind(&CastSocket::OnReadData, AsWeakPtr()));
  DVLOG(1) << "ReadData result = " << result;
  if (result > 0) {
    OnReadData(result);
  } else if (result != net::ERR_IO_PENDING) {
    CloseWithError(CHANNEL_ERROR_SOCKET_ERROR);
  }
}

void CastSocket::OnReadData(int result) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "OnReadData result = " << result <<
    " header offset = " << header_read_buffer_->offset() <<
    " body offset = " << body_read_buffer_->offset();
  read_callback_pending_ = false;
  if (result <= 0) {
    CloseWithError(CHANNEL_ERROR_SOCKET_ERROR);
    return;
  }
  // We read some data.  Move the offset in the current buffer forward.
  DCHECK_LE(current_read_buffer_->offset() + result,
            current_read_buffer_->capacity());
  current_read_buffer_->set_offset(current_read_buffer_->offset() + result);

  bool should_continue = true;
  if (current_read_buffer_.get() == header_read_buffer_.get() &&
      current_read_buffer_->RemainingCapacity() == 0) {
  // If we have read a full header, process the contents.
    should_continue = ProcessHeader();
  } else if (current_read_buffer_.get() == body_read_buffer_.get() &&
             static_cast<uint32>(current_read_buffer_->offset()) ==
             current_message_size_) {
    // If we have read a full body, process the contents.
    should_continue = ProcessBody();
  }
  if (should_continue)
    ReadData();
}

bool CastSocket::ProcessHeader() {
  DCHECK_EQ(static_cast<uint32>(header_read_buffer_->offset()),
            kMessageHeaderSize);
  MessageHeader header;
  MessageHeader::ReadFromIOBuffer(header_read_buffer_.get(), &header);
  if (header.message_size > kMaxMessageSize) {
    CloseWithError(cast_channel::CHANNEL_ERROR_INVALID_MESSAGE);
    return false;
  }
  DVLOG(1) << "Parsed header { message_size: " << header.message_size << " }";
  current_message_size_ = header.message_size;
  return true;
}

bool CastSocket::ProcessBody() {
  DCHECK_EQ(static_cast<uint32>(body_read_buffer_->offset()),
            current_message_size_);
  if (!ParseMessageFromBody()) {
    CloseWithError(cast_channel::CHANNEL_ERROR_INVALID_MESSAGE);
    return false;
  }
  current_message_size_ = 0;
  header_read_buffer_->set_offset(0);
  body_read_buffer_->set_offset(0);
  current_read_buffer_ = header_read_buffer_;
  return true;
}

bool CastSocket::ParseMessageFromBody() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(static_cast<uint32>(body_read_buffer_->offset()),
            current_message_size_);
  CastMessage message_proto;
  if (!message_proto.ParseFromArray(
      body_read_buffer_->StartOfBuffer(),
      current_message_size_))
    return false;
  DVLOG(1) << "Parsed message " << MessageProtoToString(message_proto);
  if (delegate_) {
    MessageInfo message;
    if (!CastMessageToMessageInfo(message_proto, &message))
      return false;
    delegate_->OnMessage(this, message);
  }
  return true;
}

// static
bool CastSocket::Serialize(const CastMessage& message_proto,
                           std::string* message_data) {
  DCHECK(message_data);
  message_proto.SerializeToString(message_data);
  size_t message_size = message_data->size();
  if (message_size > kMaxMessageSize) {
    message_data->clear();
    return false;
  }
  CastSocket::MessageHeader header;
  header.SetMessageSize(message_size);
  header.PrependToString(message_data);
  return true;
};

void CastSocket::CloseWithError(ChannelError error) {
  DCHECK(CalledOnValidThread());
  socket_.reset(NULL);
  ready_state_ = READY_STATE_CLOSED;
  error_state_ = error;
  if (delegate_)
    delegate_->OnError(this, error);
}

bool CastSocket::ParseChannelUrl(const GURL& url) {
  DVLOG(1) << "url = " + url.spec();
  if (url.SchemeIs(kCastInsecureScheme)) {
    is_secure_ = false;
  } else if (url.SchemeIs(kCastSecureScheme)) {
    is_secure_ = true;
  } else {
    return false;
  }
  // TODO(mfoltz): Manual parsing, yech. Register cast[s] as standard schemes?
  // TODO(mfoltz): Test for IPv6 addresses.  Brackets or no brackets?
  // TODO(mfoltz): Maybe enforce restriction to IPv4 private and IPv6 link-local
  // networks
  const std::string& path = url.path();
  // Shortest possible: //A:B
  if (path.size() < 5) {
    return false;
  }
  if (path.find("//") != 0) {
    return false;
  }
  size_t colon = path.find_last_of(':');
  if (colon == std::string::npos || colon < 3 || colon > path.size() - 2) {
    return false;
  }
  const std::string& ip_address_str = path.substr(2, colon - 2);
  const std::string& port_str = path.substr(colon + 1);
  DVLOG(1) << "addr " << ip_address_str << " port " << port_str;
  int port;
  if (!base::StringToInt(port_str, &port))
    return false;
  net::IPAddressNumber ip_address;
  if (!net::ParseIPLiteralToNumber(ip_address_str, &ip_address))
    return false;
  ip_endpoint_ = net::IPEndPoint(ip_address, port);
  return true;
};

void CastSocket::FillChannelInfo(ChannelInfo* channel_info) const {
  DCHECK(CalledOnValidThread());
  channel_info->channel_id = channel_id_;
  channel_info->url = url_.spec();
  channel_info->ready_state = ready_state_;
  channel_info->error_state = error_state_;
}

CastSocket::MessageHeader::MessageHeader() : message_size(0) { }

void CastSocket::MessageHeader::SetMessageSize(size_t size) {
  DCHECK(size < static_cast<size_t>(kuint32max));
  DCHECK(size > 0);
  message_size = static_cast<size_t>(size);
}

void CastSocket::MessageHeader::PrependToString(std::string* str) {
  MessageHeader output = *this;
  output.message_size = base::HostToNet32(message_size);
  char char_array[kMessageHeaderSize];
  memcpy(&char_array, &output, arraysize(char_array));
  str->insert(0, char_array, arraysize(char_array));
}

void CastSocket::MessageHeader::ReadFromIOBuffer(
    net::GrowableIOBuffer* buffer, MessageHeader* header) {
  uint32 message_size;
  memcpy(&message_size, buffer->StartOfBuffer(), kMessageHeaderSize);
  header->message_size = base::NetToHost32(message_size);
}

std::string CastSocket::MessageHeader::ToString() {
  return "{message_size: " + base::UintToString(message_size) + "}";
}

CastSocket::WriteRequest::WriteRequest(const net::CompletionCallback& callback)
  : callback(callback) { }

bool CastSocket::WriteRequest::SetContent(const CastMessage& message_proto) {
  DCHECK(!io_buffer.get());
  std::string message_data;
  if (!Serialize(message_proto, &message_data))
    return false;
  io_buffer = new net::DrainableIOBuffer(new net::StringIOBuffer(message_data),
                                         message_data.size());
  return true;
}

CastSocket::WriteRequest::~WriteRequest() { }

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
