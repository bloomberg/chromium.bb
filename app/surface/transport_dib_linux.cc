// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "app/surface/transport_dib.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "skia/ext/platform_canvas.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/size.h"

// The shmat system call uses this as it's invalid return address
static void *const kInvalidAddress = (void*) -1;

TransportDIB::TransportDIB()
    : key_(-1),
      address_(kInvalidAddress),
      x_shm_(0),
      display_(NULL),
      size_(0) {
}

TransportDIB::~TransportDIB() {
  if (address_ != kInvalidAddress) {
    shmdt(address_);
    address_ = kInvalidAddress;
  }

  if (x_shm_) {
    DCHECK(display_);
    ui::DetachSharedMemory(display_, x_shm_);
  }
}

// static
TransportDIB* TransportDIB::Create(size_t size, uint32 sequence_num) {
  // We use a mode of 0666 since the X server won't attach to memory which is
  // 0600 since it can't know if it (as a root process) is being asked to map
  // someone else's private shared memory region.
  const int shmkey = shmget(IPC_PRIVATE, size, 0666);
  if (shmkey == -1) {
    DLOG(ERROR) << "Failed to create SysV shared memory region"
                << " errno:" << errno;
    return NULL;
  }

  void* address = shmat(shmkey, NULL /* desired address */, 0 /* flags */);
  // Here we mark the shared memory for deletion. Since we attached it in the
  // line above, it doesn't actually get deleted but, if we crash, this means
  // that the kernel will automatically clean it up for us.
  shmctl(shmkey, IPC_RMID, 0);
  if (address == kInvalidAddress)
    return NULL;

  TransportDIB* dib = new TransportDIB;

  dib->key_ = shmkey;
  dib->address_ = address;
  dib->size_ = size;
  return dib;
}

// static
TransportDIB* TransportDIB::Map(Handle handle) {
  scoped_ptr<TransportDIB> dib(CreateWithHandle(handle));
  if (!dib->Map())
    return NULL;
  return dib.release();
}

// static
TransportDIB* TransportDIB::CreateWithHandle(Handle shmkey) {
  TransportDIB* dib = new TransportDIB;
  dib->key_ = shmkey;
  return dib;
}

// static
bool TransportDIB::is_valid(Handle dib) {
  return dib >= 0;
}

skia::PlatformCanvas* TransportDIB::GetPlatformCanvas(int w, int h) {
  if (address_ == kInvalidAddress && !Map())
    return NULL;
  scoped_ptr<skia::PlatformCanvas> canvas(new skia::PlatformCanvas);
  if (!canvas->initialize(w, h, true, reinterpret_cast<uint8_t*>(memory())))
    return NULL;
  return canvas.release();
}

bool TransportDIB::Map() {
  if (!is_valid(key_))
    return false;
  if (address_ != kInvalidAddress)
    return true;

  struct shmid_ds shmst;
  if (shmctl(key_, IPC_STAT, &shmst) == -1)
    return false;

  void* address = shmat(key_, NULL /* desired address */, 0 /* flags */);
  if (address == kInvalidAddress)
    return false;

  address_ = address;
  size_ = shmst.shm_segsz;
  return true;
}

void* TransportDIB::memory() const {
  DCHECK_NE(address_, kInvalidAddress);
  return address_;
}

TransportDIB::Id TransportDIB::id() const {
  return key_;
}

TransportDIB::Handle TransportDIB::handle() const {
  return key_;
}

XID TransportDIB::MapToX(Display* display) {
  if (!x_shm_) {
    x_shm_ = ui::AttachSharedMemory(display, key_);
    display_ = display;
  }

  return x_shm_;
}
