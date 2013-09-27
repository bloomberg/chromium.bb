// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <errno.h>
#include <string.h>
#include <algorithm>

#include "nacl_io/mount_node_tcp.h"
#include "nacl_io/mount_stream.h"
#include "nacl_io/pepper_interface.h"

namespace {
  const size_t kMaxPacketSize = 65536;
  const size_t kDefaultFifoSize = kMaxPacketSize * 8;
}

namespace nacl_io {

class TCPWork : public MountStream::Work {
 public:
  explicit TCPWork(const ScopedEventEmitterTCP& emitter)
      : MountStream::Work(emitter->stream()->mount_stream()),
        emitter_(emitter),
        data_(NULL) {
  }

  ~TCPWork() {
    delete[] data_;
  }

  TCPSocketInterface* TCPInterface() {
    return mount()->ppapi()->GetTCPSocketInterface();
  }

 protected:
  ScopedEventEmitterTCP emitter_;
  char* data_;
};


class TCPSendWork : public TCPWork {
 public:
  explicit TCPSendWork(const ScopedEventEmitterTCP& emitter)
      : TCPWork(emitter) {}

  virtual bool Start(int32_t val) {
    AUTO_LOCK(emitter_->GetLock());
    MountNodeTCP* stream = static_cast<MountNodeTCP*>(emitter_->stream());

    // Does the stream exist, and can it send?
    if (NULL == stream || !stream->TestStreamFlags(SSF_CAN_SEND))
      return false;

    // If not currently sending...
    if (!stream->TestStreamFlags(SSF_SENDING)) {
      size_t tx_data_avail = emitter_->out_fifo()->ReadAvailable();
      int capped_len = std::min(tx_data_avail, kMaxPacketSize);

      if (capped_len == 0)
        return false;

      data_ = new char[capped_len];
      emitter_->ReadOut_Locked(data_, capped_len);

      stream->SetStreamFlags(SSF_SENDING);
      int err = TCPInterface()->Write(stream->socket_resource(),
                                      data_,
                                      capped_len,
                                      mount()->GetRunCompletion(this));
      if (err == PP_OK_COMPLETIONPENDING)
        return true;

      // Anything else, we should assume the socket has gone bad.
      stream->SetError_Locked(err);
    }
    return false;
  }

  virtual void Run(int32_t length_error) {
    AUTO_LOCK(emitter_->GetLock());
    MountNodeTCP* stream = static_cast<MountNodeTCP*>(emitter_->stream());

    // If the stream is still there...
    if (stream) {
      // And we did send, then Q more work.
      if (length_error >= 0) {
        stream->ClearStreamFlags(SSF_SENDING);
        stream->QueueOutput();
      } else {
        // Otherwise this socket has gone bad.
        stream->SetError_Locked(length_error);
      }
    }
  }
};

class TCPRecvWork : public TCPWork {
 public:
  explicit TCPRecvWork(const ScopedEventEmitterTCP& emitter)
      : TCPWork(emitter) {}

  virtual bool Start(int32_t val) {
    AUTO_LOCK(emitter_->GetLock());
    MountNodeTCP* stream = static_cast<MountNodeTCP*>(emitter_->stream());

    // Does the stream exist, and can it recv?
    if (NULL == stream || !stream->TestStreamFlags(SSF_CAN_RECV))
      return false;

    // If we are not currently receiving
    if (!stream->TestStreamFlags(SSF_RECVING)) {
      size_t rx_space_avail = emitter_->in_fifo()->WriteAvailable();
      int capped_len =
          static_cast<int32_t>(std::min(rx_space_avail, kMaxPacketSize));

      if (capped_len == 0)
        return false;

      stream->SetStreamFlags(SSF_RECVING);
      data_ = new char[capped_len];
      int err = TCPInterface()->Read(stream->socket_resource(),
                                     data_,
                                     capped_len,
                                     mount()->GetRunCompletion(this));
      if (err == PP_OK_COMPLETIONPENDING)
        return true;

      // Anything else, we should assume the socket has gone bad.
      stream->SetError_Locked(err);
    }
    return false;
  }

  virtual void Run(int32_t length_error) {
    AUTO_LOCK(emitter_->GetLock());
    MountNodeTCP* stream = static_cast<MountNodeTCP*>(emitter_->stream());

    // If the stream is still there, see if we can queue more input
    if (stream) {
      if (length_error > 0) {
        emitter_->WriteIn_Locked(data_, length_error);
        stream->ClearStreamFlags(SSF_RECVING);
        stream->QueueInput();
      } else {
        stream->SetError_Locked(length_error);
      }
    }
  }
};


MountNodeTCP::MountNodeTCP(Mount* mount)
    : MountNodeSocket(mount),
      emitter_(new EventEmitterTCP(kDefaultFifoSize, kDefaultFifoSize)) {
  emitter_->AttachStream(this);
}

void MountNodeTCP::Destroy() {
  emitter_->DetachStream();
  MountNodeSocket::Destroy();
}

Error MountNodeTCP::Init(int flags) {
  if (TCPInterface() == NULL)
    return EACCES;

  socket_resource_ = TCPInterface()->Create(mount_->ppapi()->GetInstance());
  if (0 == socket_resource_)
    return EACCES;

  return 0;
}

EventEmitterTCP* MountNodeTCP::GetEventEmitter() {
  return emitter_.get();
}

void MountNodeTCP::QueueInput() {
  TCPRecvWork* work = new TCPRecvWork(emitter_);
  mount_stream()->EnqueueWork(work);
}

void MountNodeTCP::QueueOutput() {
  TCPSendWork* work = new TCPSendWork(emitter_);
  mount_stream()->EnqueueWork(work);
}


// We can not bind a client socket with PPAPI.  For now we ignore the
// bind but report the correct address later, just in case someone is
// binding without really caring what the address is (for example to
// select a more optimized interface/route.)
Error MountNodeTCP::Bind(const struct sockaddr* addr, socklen_t len) {
  AUTO_LOCK(node_lock_);

  if (0 == socket_resource_)
    return EBADF;

  /* Only bind once. */
  if (local_addr_ != 0)
    return EINVAL;

  /* Lie, we won't know until we connect. */
  return 0;
}

Error MountNodeTCP::Connect(const struct sockaddr* addr, socklen_t len) {
  AUTO_LOCK(node_lock_);

  if (0 == socket_resource_)
    return EBADF;

  if (remote_addr_ != 0)
    return EISCONN;

  remote_addr_ = SockAddrToResource(addr, len);
  if (0 == remote_addr_)
    return EINVAL;

  int err = TCPInterface()->Connect(socket_resource_,
                                    remote_addr_,
                                    PP_BlockUntilComplete());

  // If we fail, release the dest addr resource
  if (err != PP_OK) {
    mount_->ppapi()->ReleaseResource(remote_addr_);
    remote_addr_ = 0;
    return PPErrorToErrno(err);
  }

  local_addr_ = TCPInterface()->GetLocalAddress(socket_resource_);
  mount_->ppapi()->AddRefResource(local_addr_);

  // Now that we are connected, we can start sending and receiving.
  SetStreamFlags(SSF_CAN_SEND | SSF_CAN_RECV);

  // Begin the input pump
  QueueInput();
  return 0;
}


Error MountNodeTCP::Recv_Locked(void* buf,
                                size_t len,
                                PP_Resource* out_addr,
                                int* out_len) {
  *out_len = emitter_->ReadIn_Locked((char*)buf, len);
  *out_addr = remote_addr_;

  // Ref the address copy we pass back.
  mount_->ppapi()->AddRefResource(remote_addr_);
  return 0;
}

// TCP ignores dst addr passed to send_to, and always uses bound address
Error MountNodeTCP::Send_Locked(const void* buf,
                                size_t len,
                                PP_Resource,
                                int* out_len) {
  *out_len = emitter_->WriteOut_Locked((char*)buf, len);
  return 0;
}


}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API