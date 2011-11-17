// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/pseudotcp_adapter.h"

#include "base/compiler_specific.h"
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

class PseudoTcpAdapter::Core : public cricket::IPseudoTcpNotify,
                               public base::RefCounted<Core> {
 public:
  Core(net::Socket* socket);
  virtual ~Core();

  // Functions used to implement net::StreamSocket.
  int Read(net::IOBuffer* buffer, int buffer_size,
           net::OldCompletionCallback* callback);
  int Write(net::IOBuffer* buffer, int buffer_size,
            net::OldCompletionCallback* callback);
  int Connect(net::OldCompletionCallback* callback);
  void Disconnect();
  bool IsConnected() const;

  // cricket::IPseudoTcpNotify interface.
  // These notifications are triggered from NotifyPacket.
  virtual void OnTcpOpen(cricket::PseudoTcp* tcp) OVERRIDE;
  virtual void OnTcpReadable(cricket::PseudoTcp* tcp) OVERRIDE;
  virtual void OnTcpWriteable(cricket::PseudoTcp* tcp) OVERRIDE;
  // This is triggered by NotifyClock or NotifyPacket.
  virtual void OnTcpClosed(cricket::PseudoTcp* tcp, uint32 error) OVERRIDE;
  // This is triggered by NotifyClock, NotifyPacket, Recv and Send.
  virtual WriteResult TcpWritePacket(cricket::PseudoTcp* tcp,
                                     const char* buffer, size_t len) OVERRIDE;

  void SetAckDelay(int delay_ms);
  void SetNoDelay(bool no_delay);
  void SetReceiveBufferSize(int32 size);
  void SetSendBufferSize(int32 size);

 private:
  // These are invoked by the underlying Socket, and may trigger callbacks.
  // They hold a reference to |this| while running, to protect from deletion.
  void OnRead(int result);
  void OnWritten(int result);

  // These may trigger callbacks, so the holder must hold a reference on
  // the stack while calling them.
  void DoReadFromSocket();
  void HandleReadResults(int result);
  void HandleTcpClock();

  // This re-sets |timer| without triggering callbacks.
  void AdjustClock();

  net::OldCompletionCallback* connect_callback_;
  net::OldCompletionCallback* read_callback_;
  net::OldCompletionCallback* write_callback_;

  cricket::PseudoTcp pseudo_tcp_;
  scoped_ptr<net::Socket> socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  scoped_refptr<net::IOBuffer> write_buffer_;
  int write_buffer_size_;

  bool socket_write_pending_;
  scoped_refptr<net::IOBuffer> socket_read_buffer_;

  net::OldCompletionCallbackImpl<Core> socket_read_callback_;
  net::OldCompletionCallbackImpl<Core> socket_write_callback_;

  base::OneShotTimer<Core> timer_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};


PseudoTcpAdapter::Core::Core(net::Socket* socket)
    : connect_callback_(NULL),
      read_callback_(NULL),
      write_callback_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(pseudo_tcp_(this, 0)),
      socket_(socket),
      socket_write_pending_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_read_callback_(this, &PseudoTcpAdapter::Core::OnRead)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          socket_write_callback_(this, &PseudoTcpAdapter::Core::OnWritten)) {
  // Doesn't trigger callbacks.
  pseudo_tcp_.NotifyMTU(kDefaultMtu);
}

PseudoTcpAdapter::Core::~Core() {
}

int PseudoTcpAdapter::Core::Read(net::IOBuffer* buffer, int buffer_size,
                                 net::OldCompletionCallback* callback) {
  DCHECK(!read_callback_);

  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  int result = pseudo_tcp_.Recv(buffer->data(), buffer_size);
  if (result < 0) {
    result = net::MapSystemError(pseudo_tcp_.GetError());
    DCHECK(result < 0);
  }

  if (result == net::ERR_IO_PENDING) {
    read_buffer_ = buffer;
    read_buffer_size_ = buffer_size;
    read_callback_ = callback;
  }

  AdjustClock();

  return result;
}

int PseudoTcpAdapter::Core::Write(net::IOBuffer* buffer, int buffer_size,
                                  net::OldCompletionCallback* callback) {
  DCHECK(!write_callback_);

  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  int result = pseudo_tcp_.Send(buffer->data(), buffer_size);
  if (result < 0) {
    result = net::MapSystemError(pseudo_tcp_.GetError());
    DCHECK(result < 0);
  }

  if (result == net::ERR_IO_PENDING) {
    write_buffer_ = buffer;
    write_buffer_size_ = buffer_size;
    write_callback_ = callback;
  }

  AdjustClock();

  return result;
}

int PseudoTcpAdapter::Core::Connect(net::OldCompletionCallback* callback) {
  DCHECK_EQ(pseudo_tcp_.State(), cricket::PseudoTcp::TCP_LISTEN);

  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  // Start the connection attempt.
  int result = pseudo_tcp_.Connect();
  if (result < 0)
    return net::ERR_FAILED;

  AdjustClock();

  connect_callback_ = callback;
  DoReadFromSocket();

  return net::ERR_IO_PENDING;
}

void PseudoTcpAdapter::Core::Disconnect() {
  // Don't dispatch outstanding callbacks, as mandated by net::StreamSocket.
  read_callback_ = NULL;
  read_buffer_ = NULL;
  write_callback_ = NULL;
  write_buffer_ = NULL;
  connect_callback_ = NULL;

  // TODO(wez): Connect should succeed if called after Disconnect, which
  // PseudoTcp doesn't support, so we need to teardown the internal PseudoTcp
  // and create a new one in Connect.
  // TODO(wez): Close sets a shutdown flag inside PseudoTcp but has no other
  // effect.  This should be addressed in PseudoTcp, really.
  // In the meantime we can fake OnTcpClosed notification and tear down the
  // PseudoTcp.
  pseudo_tcp_.Close(true);
}

bool PseudoTcpAdapter::Core::IsConnected() const {
  return pseudo_tcp_.State() == PseudoTcp::TCP_ESTABLISHED;
}

void PseudoTcpAdapter::Core::OnTcpOpen(PseudoTcp* tcp) {
  DCHECK(tcp == &pseudo_tcp_);

  if (connect_callback_) {
    net::OldCompletionCallback* callback = connect_callback_;
    connect_callback_ = NULL;
    callback->Run(net::OK);
  }

  OnTcpReadable(tcp);
  OnTcpWriteable(tcp);
}

void PseudoTcpAdapter::Core::OnTcpReadable(PseudoTcp* tcp) {
  DCHECK_EQ(tcp, &pseudo_tcp_);
  if (!read_callback_)
    return;

  int result = pseudo_tcp_.Recv(read_buffer_->data(), read_buffer_size_);
  if (result < 0) {
    result = net::MapSystemError(pseudo_tcp_.GetError());
    DCHECK(result < 0);
    if (result == net::ERR_IO_PENDING)
      return;
  }

  AdjustClock();

  net::OldCompletionCallback* callback = read_callback_;
  read_callback_ = NULL;
  read_buffer_ = NULL;
  callback->Run(result);
}

void PseudoTcpAdapter::Core::OnTcpWriteable(PseudoTcp* tcp) {
  DCHECK_EQ(tcp, &pseudo_tcp_);
  if (!write_callback_)
    return;

  int result = pseudo_tcp_.Send(write_buffer_->data(), write_buffer_size_);
  if (result < 0) {
    result = net::MapSystemError(pseudo_tcp_.GetError());
    DCHECK(result < 0);
    if (result == net::ERR_IO_PENDING)
      return;
  }

  AdjustClock();

  net::OldCompletionCallback* callback = write_callback_;
  write_callback_ = NULL;
  write_buffer_ = NULL;
  callback->Run(result);
}

void PseudoTcpAdapter::Core::OnTcpClosed(PseudoTcp* tcp, uint32 error) {
  DCHECK_EQ(tcp, &pseudo_tcp_);

  if (connect_callback_) {
    net::OldCompletionCallback* callback = connect_callback_;
    connect_callback_ = NULL;
    callback->Run(net::MapSystemError(error));
  }

  if (read_callback_) {
    net::OldCompletionCallback* callback = read_callback_;
    read_callback_ = NULL;
    callback->Run(net::MapSystemError(error));
  }

  if (write_callback_) {
    net::OldCompletionCallback* callback = write_callback_;
    write_callback_ = NULL;
    callback->Run(net::MapSystemError(error));
  }
}

void PseudoTcpAdapter::Core::SetAckDelay(int delay_ms) {
  pseudo_tcp_.SetOption(cricket::PseudoTcp::OPT_ACKDELAY, delay_ms);
}

void PseudoTcpAdapter::Core::SetNoDelay(bool no_delay) {
  pseudo_tcp_.SetOption(cricket::PseudoTcp::OPT_NODELAY, no_delay ? 1 : 0);
}

void PseudoTcpAdapter::Core::SetReceiveBufferSize(int32 size) {
  pseudo_tcp_.SetOption(cricket::PseudoTcp::OPT_RCVBUF, size);
}

void PseudoTcpAdapter::Core::SetSendBufferSize(int32 size) {
  pseudo_tcp_.SetOption(cricket::PseudoTcp::OPT_SNDBUF, size);
}

cricket::IPseudoTcpNotify::WriteResult PseudoTcpAdapter::Core::TcpWritePacket(
    PseudoTcp* tcp,
    const char* buffer,
    size_t len) {
  DCHECK_EQ(tcp, &pseudo_tcp_);

  // If we already have a write pending, we behave like a congested network,
  // returning success for the write, but dropping the packet.  PseudoTcp will
  // back-off and retransmit, adjusting for the perceived congestion.
  if (socket_write_pending_)
    return IPseudoTcpNotify::WR_SUCCESS;

  scoped_refptr<net::IOBuffer> write_buffer = new net::IOBuffer(len);
  memcpy(write_buffer->data(), buffer, len);

  // Our underlying socket is datagram-oriented, which means it should either
  // send exactly as many bytes as we requested, or fail.
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

void PseudoTcpAdapter::Core::DoReadFromSocket() {
  if (!socket_read_buffer_)
    socket_read_buffer_ = new net::IOBuffer(kReadBufferSize);

  while (true) {
    int result = socket_->Read(socket_read_buffer_, kReadBufferSize,
                               &socket_read_callback_);
    if (result == net::ERR_IO_PENDING)
      break;

    HandleReadResults(result);
  }
}

void PseudoTcpAdapter::Core::HandleReadResults(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Read returned " << result;
    return;
  }

  // TODO(wez): Disconnect on failure of NotifyPacket?
  pseudo_tcp_.NotifyPacket(socket_read_buffer_->data(), result);
  AdjustClock();
}

void PseudoTcpAdapter::Core::OnRead(int result) {
  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  HandleReadResults(result);
  if (result >= 0)
    DoReadFromSocket();
}

void PseudoTcpAdapter::Core::OnWritten(int result) {
  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  socket_write_pending_ = false;
  if (result < 0) {
    LOG(WARNING) << "Write failed. Error code: " << result;
  }
}

void PseudoTcpAdapter::Core::AdjustClock() {
  long timeout = 0;
  if (pseudo_tcp_.GetNextClock(PseudoTcp::Now(), timeout)) {
    timer_.Stop();
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(std::max(timeout, 0L)), this,
                 &PseudoTcpAdapter::Core::HandleTcpClock);
  }
}

void PseudoTcpAdapter::Core::HandleTcpClock() {
  // Reference the Core in case a callback deletes the adapter.
  scoped_refptr<Core> core(this);

  pseudo_tcp_.NotifyClock(PseudoTcp::Now());
  AdjustClock();
}

// Public interface implemention.

PseudoTcpAdapter::PseudoTcpAdapter(net::Socket* socket)
    : core_(new Core(socket)) {
}

PseudoTcpAdapter::~PseudoTcpAdapter() {
  Disconnect();
}

int PseudoTcpAdapter::Read(net::IOBuffer* buffer, int buffer_size,
                           net::OldCompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  return core_->Read(buffer, buffer_size, callback);
}

int PseudoTcpAdapter::Write(net::IOBuffer* buffer, int buffer_size,
                            net::OldCompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  return core_->Write(buffer, buffer_size, callback);
}

bool PseudoTcpAdapter::SetReceiveBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());

  core_->SetReceiveBufferSize(size);
  return false;
}

bool PseudoTcpAdapter::SetSendBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());

  core_->SetSendBufferSize(size);
  return false;
}

int PseudoTcpAdapter::Connect(net::OldCompletionCallback* callback) {
  DCHECK(CalledOnValidThread());

  // net::StreamSocket requires that Connect return OK if already connected.
  if (IsConnected())
    return net::OK;

  return core_->Connect(callback);
}

void PseudoTcpAdapter::Disconnect() {
  DCHECK(CalledOnValidThread());
  core_->Disconnect();
}

bool PseudoTcpAdapter::IsConnected() const {
  return core_->IsConnected();
}

bool PseudoTcpAdapter::IsConnectedAndIdle() const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return false;
}

int PseudoTcpAdapter::GetPeerAddress(net::AddressList* address) const {
  DCHECK(CalledOnValidThread());

  // We don't have a meaningful peer address, but we can't return an
  // error, so we return a INADDR_ANY instead.
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

int64 PseudoTcpAdapter::NumBytesRead() const {
  DCHECK(CalledOnValidThread());
  return -1;
}

base::TimeDelta PseudoTcpAdapter::GetConnectTimeMicros() const {
  DCHECK(CalledOnValidThread());
  return base::TimeDelta::FromMicroseconds(-1);
}

void PseudoTcpAdapter::SetAckDelay(int delay_ms) {
  DCHECK(CalledOnValidThread());
  core_->SetAckDelay(delay_ms);
}

void PseudoTcpAdapter::SetNoDelay(bool no_delay) {
  DCHECK(CalledOnValidThread());
  core_->SetNoDelay(no_delay);
}

}  // namespace jingle_glue
