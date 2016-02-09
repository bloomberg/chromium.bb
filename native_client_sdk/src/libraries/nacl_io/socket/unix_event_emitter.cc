// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/socket/unix_event_emitter.h"

#include <stdlib.h>
#include <sys/socket.h>

#include "nacl_io/fifo_char.h"
#include "nacl_io/socket/fifo_packet.h"
#include "sdk_util/scoped_ref.h"

namespace nacl_io {

class UnixChildEventEmitter;
class UnixMasterEventEmitter;

typedef sdk_util::ScopedRef<UnixMasterEventEmitter>
    ScopedUnixMasterEventEmitter;

class UnixMasterEventEmitter : public UnixEventEmitter {
 public:
  explicit UnixMasterEventEmitter(size_t size, int type)
      : child_emitter_created_(false), child_emitter_(NULL) {
    if (type == SOCK_STREAM) {
      in_fifo_ = new FIFOChar(size);
      out_fifo_ = new FIFOChar(size);
    } else {
      in_fifo_ = new FIFOPacket(size);
      out_fifo_ = new FIFOPacket(size);
    }
    UpdateStatus_Locked();
  }

  ~UnixMasterEventEmitter() {
    delete in_fifo_;
    delete out_fifo_;
  }

  virtual ScopedUnixEventEmitter GetPeerEmitter();

 protected:
  virtual FIFOInterface* in_fifo() { return in_fifo_; }
  virtual FIFOInterface* out_fifo() { return out_fifo_; }
  virtual const sdk_util::SimpleLock& GetFifoLock() { return fifo_lock_; }

 private:
  FIFOInterface* in_fifo_;
  FIFOInterface* out_fifo_;
  sdk_util::SimpleLock fifo_lock_;
  bool child_emitter_created_;
  UnixChildEventEmitter* child_emitter_;

  friend class UnixChildEventEmitter;
};

class UnixChildEventEmitter : public UnixEventEmitter {
 public:
  explicit UnixChildEventEmitter(UnixMasterEventEmitter* parent)
      : parent_emitter_(parent) {
    UpdateStatus_Locked();
  }
  virtual ScopedUnixEventEmitter GetPeerEmitter() { return parent_emitter_; }

 protected:
  virtual void Destroy() { parent_emitter_->child_emitter_ = NULL; }

  virtual FIFOInterface* in_fifo() { return parent_emitter_->out_fifo(); }
  virtual FIFOInterface* out_fifo() { return parent_emitter_->in_fifo(); }
  virtual const sdk_util::SimpleLock& GetFifoLock() {
    return parent_emitter_->GetFifoLock();
  }

 private:
  ScopedUnixMasterEventEmitter parent_emitter_;
};

ScopedUnixEventEmitter UnixMasterEventEmitter::GetPeerEmitter() {
  if (!child_emitter_created_) {
    child_emitter_created_ = true;
    child_emitter_ = new UnixChildEventEmitter(this);
  }
  return sdk_util::ScopedRef<UnixChildEventEmitter>(child_emitter_);
}

uint32_t UnixEventEmitter::ReadIn_Locked(char* data, uint32_t len) {
  AUTO_LOCK(GetFifoLock());
  uint32_t count = in_fifo()->Read(data, len);
  ScopedUnixEventEmitter peer = GetPeerEmitter();
  if (peer) {
    peer->UpdateStatus_Locked();
  }
  UpdateStatus_Locked();
  return count;
}

uint32_t UnixEventEmitter::WriteOut_Locked(const char* data, uint32_t len) {
  AUTO_LOCK(GetFifoLock());
  uint32_t count = out_fifo()->Write(data, len);
  ScopedUnixEventEmitter peer = GetPeerEmitter();
  if (peer) {
    peer->UpdateStatus_Locked();
  }
  UpdateStatus_Locked();
  return count;
}

ScopedUnixEventEmitter UnixEventEmitter::MakeUnixEventEmitter(size_t size,
                                                              int type) {
  return ScopedUnixEventEmitter(new UnixMasterEventEmitter(size, type));
}

}  // namespace nacl_io
