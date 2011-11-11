// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_sync_socket.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ui/gfx/surface/transport_dib.h"

namespace nacl {

DescWrapper* DescWrapperFactory::ImportPepperSharedMemory(intptr_t shm_int,
                                                          size_t size) {
  base::SharedMemory* shm = reinterpret_cast<base::SharedMemory*>(shm_int);
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_OPENBSD)
  return ImportShmHandle(shm->handle().fd, size);
#elif defined(OS_WIN)
  return ImportShmHandle(shm->handle(), size);
#else
# error "What platform?"
#endif
}

DescWrapper* DescWrapperFactory::ImportPepper2DSharedMemory(intptr_t shm_int) {
  TransportDIB* dib = reinterpret_cast<TransportDIB*>(shm_int);
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  // TransportDIBs use SysV (X) shared memory on Linux.
  return ImportSysvShm(dib->handle(), dib->size());
#elif defined(OS_MACOSX)
  // TransportDIBs use mmap shared memory on OSX.
  return ImportShmHandle(dib->handle().fd, dib->size());
#elif defined(OS_WIN)
  // TransportDIBs use MapViewOfFile shared memory on Windows.
  return ImportShmHandle(dib->handle(), dib->size());
#else
# error "What platform?"
#endif
}

DescWrapper* DescWrapperFactory::ImportPepperSync(intptr_t sync_int) {
  base::SyncSocket* sock = reinterpret_cast<base::SyncSocket*>(sync_int);
  struct NaClDescSyncSocket* ss_desc = NULL;
  DescWrapper* wrapper = NULL;

  ss_desc = static_cast<NaClDescSyncSocket*>(
      calloc(1, sizeof(*ss_desc)));
  if (NULL == ss_desc) {
    // TODO(sehr): Gotos are awful.  Add a scoped_ptr variant that
    // invokes NaClDescSafeUnref.
    goto cleanup;
  }
  if (!NaClDescSyncSocketCtor(ss_desc, sock->handle())) {
    free(ss_desc);
    ss_desc = NULL;
    goto cleanup;
  }
  wrapper = new DescWrapper(common_data_, &ss_desc->base);
  if (NULL == wrapper) {
    goto cleanup;
  }
  ss_desc = NULL;  // DescWrapper takes ownership of ss_desc.
  return wrapper;

 cleanup:
  NaClDescSafeUnref(&ss_desc->base);
  return NULL;
}

}  // namespace nacl
