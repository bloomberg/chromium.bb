// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/chrome_async_socket.h"

#if defined(OS_WIN)
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <cstring>
#include <cstdlib>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ssl_config_service.h"
#include "net/base/sys_addrinfo.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "talk/base/socketaddress.h"

namespace notifier {

ChromeAsyncSocket::ChromeAsyncSocket(
    net::ClientSocketFactory* client_socket_factory,
    const net::SSLConfig& ssl_config,
    size_t read_buf_size,
    size_t write_buf_size,
    net::NetLog* net_log)
    : connect_callback_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                        &ChromeAsyncSocket::ProcessConnectDone),
      read_callback_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                     &ChromeAsyncSocket::ProcessReadDone),
      write_callback_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                     &ChromeAsyncSocket::ProcessWriteDone),
      ssl_connect_callback_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                            &ChromeAsyncSocket::ProcessSSLConnectDone),
      client_socket_factory_(client_socket_factory),
      ssl_config_(ssl_config),
      bound_net_log_(
          net::BoundNetLog::Make(net_log, net::NetLog::SOURCE_SOCKET)),
      state_(STATE_CLOSED),
      error_(ERROR_NONE),
      net_error_(net::OK),
      scoped_runnable_method_factory_(
          ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      read_state_(IDLE),
      read_buf_(new net::IOBufferWithSize(read_buf_size)),
      read_start_(0U),
      read_end_(0U),
      write_state_(IDLE),
      write_buf_(new net::IOBufferWithSize(write_buf_size)),
      write_end_(0U) {
  DCHECK(client_socket_factory_.get());
  DCHECK_GT(read_buf_size, 0U);
  DCHECK_GT(write_buf_size, 0U);
}

ChromeAsyncSocket::~ChromeAsyncSocket() {}

ChromeAsyncSocket::State ChromeAsyncSocket::state() {
  return state_;
}

ChromeAsyncSocket::Error ChromeAsyncSocket::error() {
  return error_;
}

int ChromeAsyncSocket::GetError() {
  return net_error_;
}

bool ChromeAsyncSocket::IsOpen() const {
  return (state_ == STATE_OPEN) || (state_ == STATE_TLS_OPEN);
}

void ChromeAsyncSocket::DoNonNetError(Error error) {
  DCHECK_NE(error, ERROR_NONE);
  DCHECK_NE(error, ERROR_WINSOCK);
  error_ = error;
  net_error_ = net::OK;
}

void ChromeAsyncSocket::DoNetError(net::Error net_error) {
  error_ = ERROR_WINSOCK;
  net_error_ = net_error;
}

void ChromeAsyncSocket::DoNetErrorFromStatus(int status) {
  DCHECK_LT(status, net::OK);
  DoNetError(static_cast<net::Error>(status));
}

namespace {

net::AddressList SocketAddressToAddressList(
    const talk_base::SocketAddress& address) {
  DCHECK_NE(address.ip(), 0U);
  // Use malloc() as net::AddressList uses free().
  addrinfo* ai = static_cast<addrinfo*>(std::malloc(sizeof *ai));
  memset(ai, 0, sizeof *ai);
  ai->ai_family = AF_INET;
  ai->ai_socktype = SOCK_STREAM;
  ai->ai_addrlen = sizeof(sockaddr_in);

  sockaddr_in* addr = static_cast<sockaddr_in*>(std::malloc(sizeof *addr));
  memset(addr, 0, sizeof *addr);
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = htonl(address.ip());
  addr->sin_port = htons(address.port());
  ai->ai_addr = reinterpret_cast<sockaddr*>(addr);

  net::AddressList address_list;
  address_list.Adopt(ai);
  return address_list;
}

}  // namespace

// STATE_CLOSED -> STATE_CONNECTING

bool ChromeAsyncSocket::Connect(const talk_base::SocketAddress& address) {
  if (state_ != STATE_CLOSED) {
    LOG(DFATAL) << "Connect() called on non-closed socket";
    DoNonNetError(ERROR_WRONGSTATE);
    return false;
  }
  if (address.ip() == 0) {
    DoNonNetError(ERROR_DNS);
    return false;
  }

  DCHECK_EQ(state_, buzz::AsyncSocket::STATE_CLOSED);
  DCHECK_EQ(read_state_, IDLE);
  DCHECK_EQ(write_state_, IDLE);

  state_ = STATE_CONNECTING;

  DCHECK(scoped_runnable_method_factory_.empty());
  scoped_runnable_method_factory_.RevokeAll();

  net::AddressList address_list = SocketAddressToAddressList(address);
  transport_socket_.reset(
      client_socket_factory_->CreateTCPClientSocket(address_list,
                                                    bound_net_log_.net_log(),
                                                    net::NetLog::Source()));
  int status = transport_socket_->Connect(&connect_callback_);
  if (status != net::ERR_IO_PENDING) {
    // We defer execution of ProcessConnectDone instead of calling it
    // directly here as the caller may not expect an error/close to
    // happen here.  This is okay, as from the caller's point of view,
    // the connect always happens asynchronously.
    MessageLoop* message_loop = MessageLoop::current();
    CHECK(message_loop);
    message_loop->PostTask(
        FROM_HERE,
        scoped_runnable_method_factory_.NewRunnableMethod(
            &ChromeAsyncSocket::ProcessConnectDone, status));
  }
  return true;
}

// STATE_CONNECTING -> STATE_OPEN
// read_state_ == IDLE -> read_state_ == POSTED (via PostDoRead())

void ChromeAsyncSocket::ProcessConnectDone(int status) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  DCHECK_EQ(read_state_, IDLE);
  DCHECK_EQ(write_state_, IDLE);
  DCHECK_EQ(state_, STATE_CONNECTING);
  if (status != net::OK) {
    DoNetErrorFromStatus(status);
    DoClose();
    return;
  }
  state_ = STATE_OPEN;
  PostDoRead();
  // Write buffer should be empty.
  DCHECK_EQ(write_end_, 0U);
  SignalConnected();
}

// read_state_ == IDLE -> read_state_ == POSTED

void ChromeAsyncSocket::PostDoRead() {
  DCHECK(IsOpen());
  DCHECK_EQ(read_state_, IDLE);
  DCHECK_EQ(read_start_, 0U);
  DCHECK_EQ(read_end_, 0U);
  MessageLoop* message_loop = MessageLoop::current();
  CHECK(message_loop);
  message_loop->PostTask(
      FROM_HERE,
      scoped_runnable_method_factory_.NewRunnableMethod(
          &ChromeAsyncSocket::DoRead));
  read_state_ = POSTED;
}

// read_state_ == POSTED -> read_state_ == PENDING

void ChromeAsyncSocket::DoRead() {
  DCHECK(IsOpen());
  DCHECK_EQ(read_state_, POSTED);
  DCHECK_EQ(read_start_, 0U);
  DCHECK_EQ(read_end_, 0U);
  // Once we call Read(), we cannot call StartTls() until the read
  // finishes.  This is okay, as StartTls() is called only from a read
  // handler (i.e., after a read finishes and before another read is
  // done).
  int status =
      transport_socket_->Read(
          read_buf_.get(), read_buf_->size(), &read_callback_);
  read_state_ = PENDING;
  if (status != net::ERR_IO_PENDING) {
    ProcessReadDone(status);
  }
}

// read_state_ == PENDING -> read_state_ == IDLE

void ChromeAsyncSocket::ProcessReadDone(int status) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  DCHECK(IsOpen());
  DCHECK_EQ(read_state_, PENDING);
  DCHECK_EQ(read_start_, 0U);
  DCHECK_EQ(read_end_, 0U);
  read_state_ = IDLE;
  if (status > 0) {
    read_end_ = static_cast<size_t>(status);
    SignalRead();
  } else if (status == 0) {
    // Other side closed the connection.
    error_ = ERROR_NONE;
    net_error_ = net::OK;
    DoClose();
  } else {  // status < 0
    DoNetErrorFromStatus(status);
    DoClose();
  }
}

// (maybe) read_state_ == IDLE -> read_state_ == POSTED (via
// PostDoRead())

bool ChromeAsyncSocket::Read(char* data, size_t len, size_t* len_read) {
  if (!IsOpen() && (state_ != STATE_TLS_CONNECTING)) {
    // Read() may be called on a closed socket if a previous read
    // causes a socket close (e.g., client sends wrong password and
    // server terminates connection).
    //
    // TODO(akalin): Fix handling of this on the libjingle side.
    if (state_ != STATE_CLOSED) {
      LOG(DFATAL) << "Read() called on non-open non-tls-connecting socket";
    }
    DoNonNetError(ERROR_WRONGSTATE);
    return false;
  }
  DCHECK_LE(read_start_, read_end_);
  if ((state_ == STATE_TLS_CONNECTING) || read_end_ == 0U) {
    if (state_ == STATE_TLS_CONNECTING) {
      DCHECK_EQ(read_state_, IDLE);
      DCHECK_EQ(read_end_, 0U);
    } else {
      DCHECK_NE(read_state_, IDLE);
    }
    *len_read = 0;
    return true;
  }
  DCHECK_EQ(read_state_, IDLE);
  *len_read = std::min(len, read_end_ - read_start_);
  DCHECK_GT(*len_read, 0U);
  std::memcpy(data, read_buf_->data() + read_start_, *len_read);
  read_start_ += *len_read;
  if (read_start_ == read_end_) {
    read_start_ = 0U;
    read_end_ = 0U;
    // We defer execution of DoRead() here for similar reasons as
    // ProcessConnectDone().
    PostDoRead();
  }
  return true;
}

// (maybe) write_state_ == IDLE -> write_state_ == POSTED (via
// PostDoWrite())

bool ChromeAsyncSocket::Write(const char* data, size_t len) {
  if (!IsOpen() && (state_ != STATE_TLS_CONNECTING)) {
    LOG(DFATAL) << "Write() called on non-open non-tls-connecting socket";
    DoNonNetError(ERROR_WRONGSTATE);
    return false;
  }
  // TODO(akalin): Avoid this check by modifying the interface to have
  // a "ready for writing" signal.
  if ((static_cast<size_t>(write_buf_->size()) - write_end_) < len) {
    LOG(DFATAL) << "queueing " << len << " bytes would exceed the "
                << "max write buffer size = " << write_buf_->size()
                << " by " << (len - write_buf_->size()) << " bytes";
    DoNetError(net::ERR_INSUFFICIENT_RESOURCES);
    return false;
  }
  std::memcpy(write_buf_->data() + write_end_, data, len);
  write_end_ += len;
  // If we're TLS-connecting, the write buffer will get flushed once
  // the TLS-connect finishes.  Otherwise, start writing if we're not
  // already writing and we have something to write.
  if ((state_ != STATE_TLS_CONNECTING) &&
      (write_state_ == IDLE) && (write_end_ > 0U)) {
    // We defer execution of DoWrite() here for similar reasons as
    // ProcessConnectDone().
    PostDoWrite();
  }
  return true;
}

// write_state_ == IDLE -> write_state_ == POSTED

void ChromeAsyncSocket::PostDoWrite() {
  DCHECK(IsOpen());
  DCHECK_EQ(write_state_, IDLE);
  DCHECK_GT(write_end_, 0U);
  MessageLoop* message_loop = MessageLoop::current();
  CHECK(message_loop);
  message_loop->PostTask(
      FROM_HERE,
      scoped_runnable_method_factory_.NewRunnableMethod(
          &ChromeAsyncSocket::DoWrite));
  write_state_ = POSTED;
}

// write_state_ == POSTED -> write_state_ == PENDING

void ChromeAsyncSocket::DoWrite() {
  DCHECK(IsOpen());
  DCHECK_EQ(write_state_, POSTED);
  DCHECK_GT(write_end_, 0U);
  // Once we call Write(), we cannot call StartTls() until the write
  // finishes.  This is okay, as StartTls() is called only after we
  // have received a reply to a message we sent to the server and
  // before we send the next message.
  int status =
      transport_socket_->Write(
          write_buf_.get(), write_end_, &write_callback_);
  write_state_ = PENDING;
  if (status != net::ERR_IO_PENDING) {
    ProcessWriteDone(status);
  }
}

// write_state_ == PENDING -> write_state_ == IDLE or POSTED (the
// latter via PostDoWrite())

void ChromeAsyncSocket::ProcessWriteDone(int status) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  DCHECK(IsOpen());
  DCHECK_EQ(write_state_, PENDING);
  DCHECK_GT(write_end_, 0U);
  write_state_ = IDLE;
  if (status < net::OK) {
    DoNetErrorFromStatus(status);
    DoClose();
    return;
  }
  size_t written = static_cast<size_t>(status);
  if (written > write_end_) {
    LOG(DFATAL) << "bytes written = " << written
                << " exceeds bytes requested = " << write_end_;
    DoNetError(net::ERR_UNEXPECTED);
    DoClose();
    return;
  }
  // TODO(akalin): Figure out a better way to do this; perhaps a queue
  // of DrainableIOBuffers.  This'll also allow us to not have an
  // artificial buffer size limit.
  std::memmove(write_buf_->data(),
               write_buf_->data() + written,
               write_end_ - written);
  write_end_ -= written;
  if (write_end_ > 0U) {
    PostDoWrite();
  }
}

// * -> STATE_CLOSED

bool ChromeAsyncSocket::Close() {
  DoClose();
  return true;
}

// (not STATE_CLOSED) -> STATE_CLOSED

void ChromeAsyncSocket::DoClose() {
  scoped_runnable_method_factory_.RevokeAll();
  if (transport_socket_.get()) {
    transport_socket_->Disconnect();
  }
  transport_socket_.reset();
  read_state_ = IDLE;
  read_start_ = 0U;
  read_end_ = 0U;
  write_state_ = IDLE;
  write_end_ = 0U;
  if (state_ != STATE_CLOSED) {
    state_ = STATE_CLOSED;
    SignalClosed();
  }
  // Reset error variables after SignalClosed() so slots connected
  // to it can read it.
  error_ = ERROR_NONE;
  net_error_ = net::OK;
}

// STATE_OPEN -> STATE_TLS_CONNECTING

bool ChromeAsyncSocket::StartTls(const std::string& domain_name) {
  if ((state_ != STATE_OPEN) || (read_state_ == PENDING) ||
      (write_state_ != IDLE)) {
    LOG(DFATAL) << "StartTls() called in wrong state";
    DoNonNetError(ERROR_WRONGSTATE);
    return false;
  }

  state_ = STATE_TLS_CONNECTING;
  read_state_ = IDLE;
  read_start_ = 0U;
  read_end_ = 0U;
  DCHECK_EQ(write_end_, 0U);

  // Clear out any posted DoRead() tasks.
  scoped_runnable_method_factory_.RevokeAll();

  DCHECK(transport_socket_.get());
  transport_socket_.reset(
      client_socket_factory_->CreateSSLClientSocket(
          transport_socket_.release(), domain_name, ssl_config_));
  int status = transport_socket_->Connect(&ssl_connect_callback_);
  if (status != net::ERR_IO_PENDING) {
    MessageLoop* message_loop = MessageLoop::current();
    CHECK(message_loop);
    message_loop->PostTask(
        FROM_HERE,
        scoped_runnable_method_factory_.NewRunnableMethod(
            &ChromeAsyncSocket::ProcessSSLConnectDone, status));
  }
  return true;
}

// STATE_TLS_CONNECTING -> STATE_TLS_OPEN
// read_state_ == IDLE -> read_state_ == POSTED (via PostDoRead())
// (maybe) write_state_ == IDLE -> write_state_ == POSTED (via
// PostDoWrite())

void ChromeAsyncSocket::ProcessSSLConnectDone(int status) {
  DCHECK_NE(status, net::ERR_IO_PENDING);
  DCHECK_EQ(state_, STATE_TLS_CONNECTING);
  DCHECK_EQ(read_state_, IDLE);
  DCHECK_EQ(read_start_, 0U);
  DCHECK_EQ(read_end_, 0U);
  DCHECK_EQ(write_state_, IDLE);
  if (status != net::OK) {
    DoNetErrorFromStatus(status);
    DoClose();
    return;
  }
  state_ = STATE_TLS_OPEN;
  PostDoRead();
  if (write_end_ > 0U) {
    PostDoWrite();
  }
  SignalSSLConnected();
}

}  // namespace notifier
