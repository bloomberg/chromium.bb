// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/ssl_socket_adapter.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/base/sys_addrinfo.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context.h"

namespace notifier {

namespace {

// Convert values from <errno.h> to values from "net/base/net_errors.h"
int MapPosixError(int err) {
  // There are numerous posix error codes, but these are the ones we thus far
  // find interesting.
  switch (err) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return net::ERR_IO_PENDING;
    case ENETDOWN:
      return net::ERR_INTERNET_DISCONNECTED;
    case ETIMEDOUT:
      return net::ERR_TIMED_OUT;
    case ECONNRESET:
    case ENETRESET:  // Related to keep-alive
      return net::ERR_CONNECTION_RESET;
    case ECONNABORTED:
      return net::ERR_CONNECTION_ABORTED;
    case ECONNREFUSED:
      return net::ERR_CONNECTION_REFUSED;
    case EHOSTUNREACH:
    case ENETUNREACH:
      return net::ERR_ADDRESS_UNREACHABLE;
    case EADDRNOTAVAIL:
      return net::ERR_ADDRESS_INVALID;
    case 0:
      return net::OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return net::ERR_FAILED;
  }
}

}  // namespace

SSLSocketAdapter* SSLSocketAdapter::Create(AsyncSocket* socket) {
  return new SSLSocketAdapter(socket);
}

SSLSocketAdapter::SSLSocketAdapter(AsyncSocket* socket)
    : SSLAdapter(socket),
      ignore_bad_cert_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          connected_callback_(this, &SSLSocketAdapter::OnConnected)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &SSLSocketAdapter::OnRead)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &SSLSocketAdapter::OnWrite)),
      ssl_state_(SSLSTATE_NONE),
      read_state_(IOSTATE_NONE),
      write_state_(IOSTATE_NONE) {
  transport_socket_ = new TransportSocket(socket, this);
}

SSLSocketAdapter::~SSLSocketAdapter() {
}

int SSLSocketAdapter::StartSSL(const char* hostname, bool restartable) {
  DCHECK(!restartable);
  hostname_ = hostname;

  if (socket_->GetState() != Socket::CS_CONNECTED) {
    ssl_state_ = SSLSTATE_WAIT;
    return 0;
  } else {
    return BeginSSL();
  }
}

int SSLSocketAdapter::BeginSSL() {
  if (!MessageLoop::current()) {
    // Certificate verification is done via the Chrome message loop.
    // Without this check, if we don't have a chrome message loop the
    // SSL connection just hangs silently.
    LOG(DFATAL) << "Chrome message loop (needed by SSL certificate "
                << "verification) does not exist";
    return net::ERR_UNEXPECTED;
  }

  // SSLConfigService is not thread-safe, and the default values for SSLConfig
  // are correct for us, so we don't use the config service to initialize this
  // object.
  net::SSLConfig ssl_config;
  transport_socket_->set_addr(talk_base::SocketAddress(hostname_, 0));
  ssl_socket_.reset(
      net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
          transport_socket_, hostname_.c_str(), ssl_config));

  int result = ssl_socket_->Connect(&connected_callback_);

  if (result == net::ERR_IO_PENDING || result == net::OK) {
    return 0;
  } else {
    LOG(ERROR) << "Could not start SSL: " << net::ErrorToString(result);
    return result;
  }
}

int SSLSocketAdapter::Send(const void* buf, size_t len) {
  if (ssl_state_ != SSLSTATE_CONNECTED) {
    return AsyncSocketAdapter::Send(buf, len);
  } else {
    scoped_refptr<net::IOBuffer> transport_buf = new net::IOBuffer(len);
    memcpy(transport_buf->data(), buf, len);

    int result = ssl_socket_->Write(transport_buf, len, NULL);
    if (result == net::ERR_IO_PENDING) {
      SetError(EWOULDBLOCK);
    }
    transport_buf = NULL;
    return result;
  }
}

int SSLSocketAdapter::Recv(void* buf, size_t len) {
  switch (ssl_state_) {
    case SSLSTATE_NONE:
      return AsyncSocketAdapter::Recv(buf, len);

    case SSLSTATE_WAIT:
      SetError(EWOULDBLOCK);
      return -1;

    case SSLSTATE_CONNECTED:
      switch (read_state_) {
        case IOSTATE_NONE: {
          transport_buf_ = new net::IOBuffer(len);
          int result = ssl_socket_->Read(transport_buf_, len, &read_callback_);
          if (result >= 0) {
            memcpy(buf, transport_buf_->data(), len);
          }

          if (result == net::ERR_IO_PENDING) {
            read_state_ = IOSTATE_PENDING;
            SetError(EWOULDBLOCK);
          } else {
            if (result < 0) {
              SetError(result);
              LOG(INFO) << "Socket error " << result;
            }
            transport_buf_ = NULL;
          }
          return result;
        }
        case IOSTATE_PENDING:
          SetError(EWOULDBLOCK);
          return -1;

        case IOSTATE_COMPLETE:
          memcpy(buf, transport_buf_->data(), len);
          transport_buf_ = NULL;
          read_state_ = IOSTATE_NONE;
          return data_transferred_;
      }
  }

  NOTREACHED();
  return -1;
}

void SSLSocketAdapter::OnConnected(int result) {
  if (result == net::OK) {
    ssl_state_ = SSLSTATE_CONNECTED;
    OnConnectEvent(this);
  } else {
    LOG(WARNING) << "OnConnected failed with error " << result;
  }
}

void SSLSocketAdapter::OnRead(int result) {
  DCHECK(read_state_ == IOSTATE_PENDING);
  read_state_ = IOSTATE_COMPLETE;
  data_transferred_ = result;
  AsyncSocketAdapter::OnReadEvent(this);
}

void SSLSocketAdapter::OnWrite(int result) {
  DCHECK(write_state_ == IOSTATE_PENDING);
  write_state_ = IOSTATE_COMPLETE;
  data_transferred_ = result;
  AsyncSocketAdapter::OnWriteEvent(this);
}

void SSLSocketAdapter::OnConnectEvent(talk_base::AsyncSocket* socket) {
  if (ssl_state_ != SSLSTATE_WAIT) {
    AsyncSocketAdapter::OnConnectEvent(socket);
  } else {
    ssl_state_ = SSLSTATE_NONE;
    int result = BeginSSL();
    if (0 != result) {
      // TODO(zork): Handle this case gracefully.
      LOG(WARNING) << "BeginSSL() failed with " << result;
    }
  }
}

TransportSocket::TransportSocket(talk_base::AsyncSocket* socket,
                                 SSLSocketAdapter *ssl_adapter)
    : read_callback_(NULL),
      write_callback_(NULL),
      read_buffer_len_(0),
      write_buffer_len_(0),
      socket_(socket),
      was_used_to_convey_data_(false) {
  socket_->SignalReadEvent.connect(this, &TransportSocket::OnReadEvent);
  socket_->SignalWriteEvent.connect(this, &TransportSocket::OnWriteEvent);
}

TransportSocket::~TransportSocket() {
}

int TransportSocket::Connect(net::CompletionCallback* callback) {
  // Connect is never called by SSLClientSocket, instead SSLSocketAdapter
  // calls Connect() on socket_ directly.
  NOTREACHED();
  return false;
}

void TransportSocket::Disconnect() {
  socket_->Close();
}

bool TransportSocket::IsConnected() const {
  return (socket_->GetState() == talk_base::Socket::CS_CONNECTED);
}

bool TransportSocket::IsConnectedAndIdle() const {
  // Not implemented.
  NOTREACHED();
  return false;
}

int TransportSocket::GetPeerAddress(net::AddressList* address) const {
  talk_base::SocketAddress socket_address = socket_->GetRemoteAddress();

  // libjingle supports only IPv4 addresses.
  sockaddr_in ipv4addr;
  socket_address.ToSockAddr(&ipv4addr);

  struct addrinfo ai;
  memset(&ai, 0, sizeof(ai));
  ai.ai_family = ipv4addr.sin_family;
  ai.ai_socktype = SOCK_STREAM;
  ai.ai_protocol = IPPROTO_TCP;
  ai.ai_addr = reinterpret_cast<struct sockaddr*>(&ipv4addr);
  ai.ai_addrlen = sizeof(ipv4addr);

  address->Copy(&ai, false);
  return net::OK;
}

void TransportSocket::SetSubresourceSpeculation() {
  NOTREACHED();
}

void TransportSocket::SetOmniboxSpeculation() {
  NOTREACHED();
}

bool TransportSocket::WasEverUsed() const {
  // We don't use this in ClientSocketPools, so this should never be used.
  NOTREACHED();
  return was_used_to_convey_data_;
}

int TransportSocket::Read(net::IOBuffer* buf, int buf_len,
                          net::CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK(!read_callback_);
  DCHECK(!read_buffer_.get());
  int result = socket_->Recv(buf->data(), buf_len);
  if (result < 0) {
    result = MapPosixError(socket_->GetError());
    if (result == net::ERR_IO_PENDING) {
      read_callback_ = callback;
      read_buffer_ = buf;
      read_buffer_len_ = buf_len;
    }
  }
  if (result != net::ERR_IO_PENDING)
    was_used_to_convey_data_ = true;
  return result;
}

int TransportSocket::Write(net::IOBuffer* buf, int buf_len,
                           net::CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK(!write_callback_);
  DCHECK(!write_buffer_.get());
  int result = socket_->Send(buf->data(), buf_len);
  if (result < 0) {
    result = MapPosixError(socket_->GetError());
    if (result == net::ERR_IO_PENDING) {
      write_callback_ = callback;
      write_buffer_ = buf;
      write_buffer_len_ = buf_len;
    }
  }
  if (result != net::ERR_IO_PENDING)
    was_used_to_convey_data_ = true;
  return result;
}

bool TransportSocket::SetReceiveBufferSize(int32 size) {
  // Not implemented.
  return false;
}

bool TransportSocket::SetSendBufferSize(int32 size) {
  // Not implemented.
  return false;
}

void TransportSocket::OnReadEvent(talk_base::AsyncSocket* socket) {
  if (read_callback_) {
    DCHECK(read_buffer_.get());
    net::CompletionCallback* callback = read_callback_;
    scoped_refptr<net::IOBuffer> buffer = read_buffer_;
    int buffer_len = read_buffer_len_;

    read_callback_ = NULL;
    read_buffer_ = NULL;
    read_buffer_len_ = 0;

    int result = socket_->Recv(buffer->data(), buffer_len);
    if (result < 0) {
      result = MapPosixError(socket_->GetError());
      if (result == net::ERR_IO_PENDING) {
        read_callback_ = callback;
        read_buffer_ = buffer;
        read_buffer_len_ = buffer_len;
        return;
      }
    }
    was_used_to_convey_data_ = true;
    callback->RunWithParams(Tuple1<int>(result));
  }
}

void TransportSocket::OnWriteEvent(talk_base::AsyncSocket* socket) {
  if (write_callback_) {
    DCHECK(write_buffer_.get());
    net::CompletionCallback* callback = write_callback_;
    scoped_refptr<net::IOBuffer> buffer = write_buffer_;
    int buffer_len = write_buffer_len_;

    write_callback_ = NULL;
    write_buffer_ = NULL;
    write_buffer_len_ = 0;

    int result = socket_->Send(buffer->data(), buffer_len);
    if (result < 0) {
      result = MapPosixError(socket_->GetError());
      if (result == net::ERR_IO_PENDING) {
        write_callback_ = callback;
        write_buffer_ = buffer;
        write_buffer_len_ = buffer_len;
        return;
      }
    }
    was_used_to_convey_data_ = true;
    callback->RunWithParams(Tuple1<int>(result));
  }
}

}  // namespace notifier
