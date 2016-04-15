// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/fuzzed_socket.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"

namespace net {

namespace {

// Subset of the socket errors that can be returned by normal socket reads /
// writes. The first one is returned when no more input data remains, so it's
// one of the most common ones.
const Error kReadWriteErrors[] = {ERR_CONNECTION_CLOSED, ERR_FAILED,
                                  ERR_TIMED_OUT, ERR_CONNECTION_RESET};

}  // namespace

FuzzedSocket::FuzzedSocket(const uint8_t* data,
                           size_t data_size,
                           const BoundNetLog& bound_net_log)
    : data_(reinterpret_cast<const char*>(data), data_size),
      bound_net_log_(bound_net_log),
      weak_factory_(this) {}

FuzzedSocket::~FuzzedSocket() {}

int FuzzedSocket::Read(IOBuffer* buf,
                       int buf_len,
                       const CompletionCallback& callback) {
  DCHECK(!read_pending_);

  bool sync;
  int result;

  if (net_error_ != OK) {
    // If an error has already been generated, use it to determine what to do.
    result = net_error_;
    sync = !error_pending_;
  } else {
    // Otherwise, use |data_|.
    uint8_t random_val = ConsumeUint8FromData();
    sync = !!(random_val & 0x01);
    result = random_val >> 1;
    if (result > buf_len)
      result = buf_len;
    // Can't read more data than is available in |data_|.
    if (static_cast<size_t>(result) > data_.length())
      result = data_.length();

    if (result == 0) {
      net_error_ = ConsumeReadWriteErrorFromData();
      result = net_error_;
      if (!sync)
        error_pending_ = true;
    } else {
      memcpy(buf->data(), data_.data(), result);
      data_ = data_.substr(result);
    }
  }

  // Graceful close of a socket returns OK, at least in theory. This doesn't
  // perfectly reflect real socket behavior, but close enough.
  if (result == ERR_CONNECTION_CLOSED)
    result = 0;

  if (sync) {
    if (result > 0)
      total_bytes_read_ += result;
    return result;
  }

  read_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FuzzedSocket::OnReadComplete,
                            weak_factory_.GetWeakPtr(), callback, result));
  return ERR_IO_PENDING;
}

int FuzzedSocket::Write(IOBuffer* buf,
                        int buf_len,
                        const CompletionCallback& callback) {
  DCHECK(!write_pending_);

  bool sync;
  int result;

  if (net_error_ != OK) {
    // If an error has already been generated, use it to determine what to do.
    result = net_error_;
    sync = !error_pending_;
  } else {
    // Otherwise, use |data_|.
    uint8_t random_val = ConsumeUint8FromData();
    sync = !!(random_val & 0x01);
    result = random_val >> 1;
    if (result > buf_len)
      result = buf_len;
    if (result == 0) {
      net_error_ = ConsumeReadWriteErrorFromData();
      result = net_error_;
      if (!sync)
        error_pending_ = true;
    }
  }

  if (sync) {
    if (result > 0)
      total_bytes_written_ += result;
    return result;
  }

  write_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FuzzedSocket::OnWriteComplete,
                            weak_factory_.GetWeakPtr(), callback, result));
  return ERR_IO_PENDING;
}

int FuzzedSocket::SetReceiveBufferSize(int32_t size) {
  return OK;
}

int FuzzedSocket::SetSendBufferSize(int32_t size) {
  return OK;
}

int FuzzedSocket::Connect(const CompletionCallback& callback) {
  // Sockets can normally be reused, but don't support it here.
  DCHECK_NE(net_error_, OK);
  DCHECK(!read_pending_);
  DCHECK(!write_pending_);
  DCHECK(!error_pending_);
  DCHECK(!total_bytes_read_);
  DCHECK(!total_bytes_written_);

  net_error_ = OK;
  return OK;
}

void FuzzedSocket::Disconnect() {
  net_error_ = ERR_CONNECTION_CLOSED;
  weak_factory_.InvalidateWeakPtrs();
  read_pending_ = false;
  write_pending_ = false;
  error_pending_ = false;
}

bool FuzzedSocket::IsConnected() const {
  return net_error_ == OK && !error_pending_;
}

bool FuzzedSocket::IsConnectedAndIdle() const {
  return IsConnected();
}

int FuzzedSocket::GetPeerAddress(IPEndPoint* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  *address = IPEndPoint(IPAddress(127, 0, 0, 1), 80);
  return OK;
}

int FuzzedSocket::GetLocalAddress(IPEndPoint* address) const {
  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;
  *address = IPEndPoint(IPAddress(127, 0, 0, 1), 43434);
  return OK;
}

const BoundNetLog& FuzzedSocket::NetLog() const {
  return bound_net_log_;
}

void FuzzedSocket::SetSubresourceSpeculation() {}

void FuzzedSocket::SetOmniboxSpeculation() {}

bool FuzzedSocket::WasEverUsed() const {
  return total_bytes_written_ != 0 || total_bytes_read_ != 0;
}

void FuzzedSocket::EnableTCPFastOpenIfSupported() {}

bool FuzzedSocket::WasNpnNegotiated() const {
  return false;
}

NextProto FuzzedSocket::GetNegotiatedProtocol() const {
  return kProtoUnknown;
}

bool FuzzedSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return false;
}

void FuzzedSocket::GetConnectionAttempts(ConnectionAttempts* out) const {
  out->clear();
}

void FuzzedSocket::ClearConnectionAttempts() {}

void FuzzedSocket::AddConnectionAttempts(const ConnectionAttempts& attempts) {}

int64_t FuzzedSocket::GetTotalReceivedBytes() const {
  return total_bytes_read_;
}

uint8_t FuzzedSocket::ConsumeUint8FromData() {
  size_t length = data_.length();
  if (!length)
    return 0;
  uint8_t out = data_[length - 1];
  data_ = data_.substr(0, length - 1);
  return out;
}

Error FuzzedSocket::ConsumeReadWriteErrorFromData() {
  return kReadWriteErrors[ConsumeUint8FromData() % arraysize(kReadWriteErrors)];
}

void FuzzedSocket::OnReadComplete(const CompletionCallback& callback,
                                  int result) {
  CHECK(read_pending_);
  read_pending_ = false;
  if (result <= 0) {
    error_pending_ = false;
  } else {
    total_bytes_read_ += result;
  }
  callback.Run(result);
}

void FuzzedSocket::OnWriteComplete(const CompletionCallback& callback,
                                   int result) {
  CHECK(write_pending_);
  write_pending_ = false;
  if (result <= 0) {
    error_pending_ = false;
  } else {
    total_bytes_written_ += result;
  }
  callback.Run(result);
}

}  // namespace net
