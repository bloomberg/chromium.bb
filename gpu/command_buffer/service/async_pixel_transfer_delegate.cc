// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_delegate.h"

#include "base/memory/shared_memory.h"
#include "gpu/command_buffer/service/safe_shared_memory_pool.h"

namespace gpu {

namespace {

void* GetAddressImpl(base::SharedMemory* shared_memory,
                     uint32 shm_size,
                     uint32 shm_data_offset,
                     uint32 shm_data_size) {
  // Memory bounds have already been validated, so there
  // are just DCHECKS here.
  DCHECK(shared_memory);
  DCHECK(shared_memory->memory());
  DCHECK_LE(shm_data_offset + shm_data_size, shm_size);
  return static_cast<int8*>(shared_memory->memory()) + shm_data_offset;
}

}  // namespace

AsyncPixelTransferState::AsyncPixelTransferState(){}

AsyncPixelTransferState::~AsyncPixelTransferState(){}

AsyncPixelTransferDelegate::AsyncPixelTransferDelegate(){}

AsyncPixelTransferDelegate::~AsyncPixelTransferDelegate(){}

// static
void* AsyncPixelTransferDelegate::GetAddress(
    const AsyncMemoryParams& mem_params) {
  return GetAddressImpl(mem_params.shared_memory,
                        mem_params.shm_size,
                        mem_params.shm_data_offset,
                        mem_params.shm_data_size);
}

// static
void* AsyncPixelTransferDelegate::GetAddress(
    ScopedSafeSharedMemory* safe_shared_memory,
    const AsyncMemoryParams& mem_params) {
  return GetAddressImpl(safe_shared_memory->shared_memory(),
                        mem_params.shm_size,
                        mem_params.shm_data_offset,
                        mem_params.shm_data_size);
}

}// namespace gpu
