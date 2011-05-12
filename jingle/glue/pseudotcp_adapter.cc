// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/pseudotcp_adapter.h"

#include "base/logging.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

using cricket::PseudoTcp;

namespace {
const int kReadBufferSize = 65536;  // Maximum size of a packet.
const uint16 kDefaultMtu = 1280;
}  // namespace

namespace jingle_glue {

PseudoTcpAdapter::PseudoTcpAdapter(net::Socket* socket)
    : socket_(socket),
      ALLOW_THIS_IN_INITIALIZER_LIST(pseudotcp_(this, 0)),
      connect_callback_(NULL),
      read_callback_(NULL),
      write_callback_(NULL),
      socket_write_pending_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_read_callback_(this, &PseudoTcpAdapter::OnRead)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_write_callback_(this, &PseudoTcpAdapter::OnWritten)) {
  pseudotcp_.NotifyMTU(kDefaultMtu);
}

PseudoTcpAdapter::~PseudoTcpAdapter() {
}

int PseudoTcpAdapter::Read(net::IOBuffer* buffer, int buffer_size,
                           net::CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());

  // Verify that there is no other pending read.
  DCHECK(read_callback_ == NULL);

  PseudoTcp::TcpState state = pseudotcp_.State();
  int result;
  if (state == PseudoTcp::TCP_SYN_SENT ||
      state == PseudoTcp::TCP_SYN_RECEIVED) {
    result = net::ERR_IO_PENDING;
  } else {
    result = pseudotcp_.Recv(buffer->data(), buffer_size);
    if (result < 0) {
      result = net::MapSystemError(pseudotcp_.GetError());
      DCHECK(result < 0);
    }
  }

  if (result == net::ERR_IO_PENDING) {
    read_buffer_ = buffer;
    read_buffer_size_ = buffer_size;
    read_callback_ = callback;
  }

  AdjustClock();

  return result;
}

int PseudoTcpAdapter::Write(net::IOBuffer* buffer, int buffer_size,
                            net::CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());

  // Verify that there is no other pending write.
  DCHECK(write_callback_ == NULL);

  PseudoTcp::TcpState state = pseudotcp_.State();
  int result;
  if (state == PseudoTcp::TCP_SYN_SENT ||
      state == PseudoTcp::TCP_SYN_RECEIVED) {
    result = net::ERR_IO_PENDING;
  } else {
    result = pseudotcp_.Send(buffer->data(), buffer_size);
    if (result < 0) {
      result = net::MapSystemError(pseudotcp_.GetError());
      DCHECK(result < 0);
    }
  }

  if (result == net::ERR_IO_PENDING) {
    write_buffer_ = buffer;
    write_buffer_size_ = buffer_size;
    write_callback_ = callback;
  }

  AdjustClock();

  return result;
}

bool PseudoTcpAdapter::SetReceiveBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  // TODO(sergeyu): Implement support for adjustable buffer size and
  // used it here.
  return false;
}

bool PseudoTcpAdapter::SetSendBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  // TODO(sergeyu): Implement support for adjustable buffer size and
  // used it here.
  return false;
}

int PseudoTcpAdapter::Connect(net::CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());

  // Start reading from the socket.
  DoReadFromSocket();

  int result = pseudotcp_.Connect();
  if (result < 0)
    return net::ERR_FAILED;

  AdjustClock();

  connect_callback_ = callback;
  return net::ERR_IO_PENDING;
}

void PseudoTcpAdapter::Disconnect() {
  DCHECK(CalledOnValidThread());
  pseudotcp_.Close(false);
}

bool PseudoTcpAdapter::IsConnected() const {
  DCHECK(CalledOnValidThread());
  return pseudotcp_.State() == PseudoTcp::TCP_ESTABLISHED;
}

bool PseudoTcpAdapter::IsConnectedAndIdle() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return false;
}

int PseudoTcpAdapter::GetPeerAddress(net::AddressList* address) const {
  DCHECK(CalledOnValidThread());

  // We actually don't know the peer address. Returning so the upper layers
  // won't complain.
  net::IPAddressNumber ip_address(4);
  *address = net::AddressList::CreateFromIPAddress(ip_address, 0);
  return net::OK;
}

int PseudoTcpAdapter::GetLocalAddress(net::IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

const net::BoundNetLog& PseudoTcpAdapter::NetLog() const {
  DCHECK(CalledOnValidThread());
  return net_log_;
}

void PseudoTcpAdapter::SetSubresourceSpeculation() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void PseudoTcpAdapter::SetOmniboxSpeculation() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

bool PseudoTcpAdapter::WasEverUsed() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return true;
}

bool PseudoTcpAdapter::UsingTCPFastOpen() const {
  DCHECK(CalledOnValidThread());
  return false;
}

void PseudoTcpAdapter::OnTcpOpen(PseudoTcp* tcp) {
  DCHECK(CalledOnValidThread());
  DCHECK(connect_callback_);
  connect_callback_->Run(net::OK);
  connect_callback_ = NULL;

  OnTcpReadable(tcp);
  OnTcpWriteable(tcp);
}

void PseudoTcpAdapter::OnTcpReadable(PseudoTcp* tcp) {
  DCHECK(CalledOnValidThread());

  if (!read_buffer_)
    return;

  // Try to send the data we have pending.
  int result = pseudotcp_.Recv(read_buffer_->data(), read_buffer_size_);
  if (result < 0) {
    result = net::MapSystemError(pseudotcp_.GetError());
    DCHECK(result < 0);
    if (result == net::ERR_IO_PENDING)
      return;
  }

  AdjustClock();

  net::CompletionCallback* cb = read_callback_;
  read_callback_ = NULL;
  read_buffer_ = NULL;
  cb->Run(result);
}

void PseudoTcpAdapter::OnTcpWriteable(PseudoTcp* tcp) {
  DCHECK(CalledOnValidThread());

  if (!write_buffer_)
    return;

  // Try to send the data we have pending.
  int result = pseudotcp_.Send(write_buffer_->data(), write_buffer_size_);
  if (result < 0) {
    result = net::MapSystemError(pseudotcp_.GetError());
    DCHECK(result < 0);
    if (result == net::ERR_IO_PENDING)
      return;
  }

  AdjustClock();

  net::CompletionCallback* cb = write_callback_;
  write_callback_ = NULL;
  write_buffer_ = NULL;
  cb->Run(result);
}

void PseudoTcpAdapter::OnTcpClosed(PseudoTcp* tcp, uint32 error) {
  DCHECK(CalledOnValidThread());

  if (connect_callback_) {
    connect_callback_->Run(net::MapSystemError(error));
    connect_callback_ = NULL;
  }

  if (read_callback_) {
    read_callback_->Run(net::MapSystemError(error));
    read_callback_ = NULL;
  }

  if (write_callback_) {
    write_callback_->Run(net::MapSystemError(error));
    write_callback_ = NULL;
  }
}

cricket::IPseudoTcpNotify::WriteResult PseudoTcpAdapter::TcpWritePacket(
    PseudoTcp* tcp,
    const char* buffer,
    size_t len) {
  DCHECK(CalledOnValidThread());

  if (socket_write_pending_)
    return IPseudoTcpNotify::WR_SUCCESS;

  scoped_refptr<net::IOBuffer> write_buffer = new net::IOBuffer(len);
  memcpy(write_buffer->data(), buffer, len);

  int result = socket_->Write(write_buffer, len, &socket_write_callback_);
  if (result == net::ERR_IO_PENDING) {
    socket_write_pending_ = true;
    return IPseudoTcpNotify::WR_SUCCESS;
  } if (result == net::ERR_MSG_TOO_BIG) {
    return IPseudoTcpNotify::WR_TOO_LARGE;
  } else if (result < 0) {
    return IPseudoTcpNotify::WR_FAIL;
  } else {
    return IPseudoTcpNotify::WR_SUCCESS;
  }
}

void  PseudoTcpAdapter::DoReadFromSocket() {
  if (!socket_read_buffer_) {
    socket_read_buffer_ = new net::IOBuffer(kReadBufferSize);
  }

  while (true) {
    int result = socket_->Read(socket_read_buffer_, kReadBufferSize,
                               &socket_read_callback_);
    if (result == net::ERR_IO_PENDING)
      break;

    HandleReadResults(result);
  }
}

void PseudoTcpAdapter::HandleReadResults(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Read returned " << result;
    return;
  }

  pseudotcp_.NotifyPacket(socket_read_buffer_->data(), result);
  AdjustClock();
}

void PseudoTcpAdapter::OnRead(int result) {
  HandleReadResults(result);
  if (result >= 0)
    DoReadFromSocket();
}

void PseudoTcpAdapter::OnWritten(int result) {
  socket_write_pending_ = false;
  if (result < 0) {
    LOG(WARNING) << "Write failed. Error code: " << result;
  }
}

void PseudoTcpAdapter::AdjustClock() {
  long timeout = 0;
  if (pseudotcp_.GetNextClock(PseudoTcp::Now(), timeout)) {
    timer_.Stop();
    timer_.Start(base::TimeDelta::FromMilliseconds(std::max(timeout, 0L)), this,
                 &PseudoTcpAdapter::HandleTcpClock);
  }
}

void PseudoTcpAdapter::HandleTcpClock() {
  pseudotcp_.NotifyClock(PseudoTcp::Now());
  AdjustClock();
}

}  // namespace jingle_glue
