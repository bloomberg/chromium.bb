// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/platform_shared_buffer.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "base/sys_info.h"
#include "mojo/edk/embedder/platform_handle_utils.h"

namespace mojo {
namespace edk {

namespace {

// Takes ownership of |memory_handle|.
ScopedPlatformHandle SharedMemoryToPlatformHandle(
    base::SharedMemoryHandle memory_handle) {
#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
  return ScopedPlatformHandle(PlatformHandle(memory_handle.fd));
#elif defined(OS_WIN)
  return ScopedPlatformHandle(PlatformHandle(memory_handle.GetHandle()));
#else
  CHECK_EQ(memory_handle.GetType(), base::SharedMemoryHandle::POSIX);
  return ScopedPlatformHandle(
      PlatformHandle(memory_handle.GetFileDescriptor().fd));
#endif
}

}  // namespace

// static
PlatformSharedBuffer* PlatformSharedBuffer::Create(size_t num_bytes) {
  DCHECK_GT(num_bytes, 0u);

  PlatformSharedBuffer* rv = new PlatformSharedBuffer(num_bytes);
  if (!rv->Init()) {
    // We can't just delete it directly, due to the "in destructor" (debug)
    // check.
    scoped_refptr<PlatformSharedBuffer> deleter(rv);
    return nullptr;
  }

  return rv;
}

// static
PlatformSharedBuffer* PlatformSharedBuffer::CreateFromPlatformHandle(
    size_t num_bytes,
    ScopedPlatformHandle platform_handle) {
  DCHECK_GT(num_bytes, 0u);

  PlatformSharedBuffer* rv = new PlatformSharedBuffer(num_bytes);
  if (!rv->InitFromPlatformHandle(std::move(platform_handle))) {
    // We can't just delete it directly, due to the "in destructor" (debug)
    // check.
    scoped_refptr<PlatformSharedBuffer> deleter(rv);
    return nullptr;
  }

  return rv;
}

// static
PlatformSharedBuffer* PlatformSharedBuffer::CreateFromSharedMemoryHandle(
    size_t num_bytes,
    bool read_only,
    base::SharedMemoryHandle handle) {
  DCHECK_GT(num_bytes, 0u);
  DCHECK(!read_only);

  PlatformSharedBuffer* rv = new PlatformSharedBuffer(num_bytes);
  rv->InitFromSharedMemoryHandle(handle);

  return rv;
}

size_t PlatformSharedBuffer::GetNumBytes() const {
  return num_bytes_;
}

scoped_ptr<PlatformSharedBufferMapping> PlatformSharedBuffer::Map(
    size_t offset,
    size_t length) {
  if (!IsValidMap(offset, length))
    return nullptr;

  return MapNoCheck(offset, length);
}

bool PlatformSharedBuffer::IsValidMap(size_t offset, size_t length) {
  if (offset > num_bytes_ || length == 0)
    return false;

  // Note: This is an overflow-safe check of |offset + length > num_bytes_|
  // (that |num_bytes >= offset| is verified above).
  if (length > num_bytes_ - offset)
    return false;

  return true;
}

scoped_ptr<PlatformSharedBufferMapping> PlatformSharedBuffer::MapNoCheck(
    size_t offset,
    size_t length) {
  DCHECK(IsValidMap(offset, length));
  DCHECK(shared_memory_);
  base::SharedMemoryHandle handle;
  {
    base::AutoLock locker(lock_);
    handle = base::SharedMemory::DuplicateHandle(shared_memory_->handle());
  }
  if (handle == base::SharedMemory::NULLHandle())
    return nullptr;

  scoped_ptr<PlatformSharedBufferMapping> mapping(
      new PlatformSharedBufferMapping(handle, offset, length));
  if (mapping->Map())
    return make_scoped_ptr(mapping.release());

  return nullptr;
}

ScopedPlatformHandle PlatformSharedBuffer::DuplicatePlatformHandle() {
  DCHECK(shared_memory_);
  base::SharedMemoryHandle handle;
  {
    base::AutoLock locker(lock_);
    handle = base::SharedMemory::DuplicateHandle(shared_memory_->handle());
  }
  if (handle == base::SharedMemory::NULLHandle())
    return ScopedPlatformHandle();

  return SharedMemoryToPlatformHandle(handle);
}

ScopedPlatformHandle PlatformSharedBuffer::PassPlatformHandle() {
  DCHECK(HasOneRef());

  // The only way to pass a handle from base::SharedMemory is to duplicate it
  // and close the original.
  ScopedPlatformHandle handle = DuplicatePlatformHandle();

  base::AutoLock locker(lock_);
  shared_memory_->Close();
  return handle;
}

PlatformSharedBuffer::PlatformSharedBuffer(size_t num_bytes)
    : num_bytes_(num_bytes) {}

PlatformSharedBuffer::~PlatformSharedBuffer() {}

bool PlatformSharedBuffer::Init() {
  DCHECK(!shared_memory_);

  base::SharedMemoryCreateOptions options;
  options.size = num_bytes_;
#if defined(OS_MACOSX) && !defined(OS_IOS)
  // TODO(crbug.com/582468): Support Mach shared memory.
  options.type = base::SharedMemoryHandle::POSIX;
#endif

  shared_memory_.reset(new base::SharedMemory);
  return shared_memory_->Create(options);
}

bool PlatformSharedBuffer::InitFromPlatformHandle(
    ScopedPlatformHandle platform_handle) {
  DCHECK(!shared_memory_);

#if defined(OS_WIN)
  base::SharedMemoryHandle handle(platform_handle.release().handle,
                                  base::GetCurrentProcId());
#else
  base::SharedMemoryHandle handle(platform_handle.release().handle, false);
#endif

  shared_memory_.reset(new base::SharedMemory(handle, false));
  return true;
}

void PlatformSharedBuffer::InitFromSharedMemoryHandle(
    base::SharedMemoryHandle handle) {
  DCHECK(!shared_memory_);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // TODO(crbug.com/582468): Support Mach shared memory.
  CHECK(handle.GetType() == base::SharedMemoryHandle::POSIX);
#endif

  // TODO(crbug.com/556587): Support read-only handles.
  shared_memory_.reset(new base::SharedMemory(handle, false));
}

PlatformSharedBufferMapping::~PlatformSharedBufferMapping() {
  Unmap();
}

void* PlatformSharedBufferMapping::GetBase() const {
  return base_;
}

size_t PlatformSharedBufferMapping::GetLength() const {
  return length_;
}

bool PlatformSharedBufferMapping::Map() {
  // Mojo shared buffers can be mapped at any offset. However,
  // base::SharedMemory must be mapped at a page boundary. So calculate what the
  // nearest whole page offset is, and build a mapping that's offset from that.
  size_t offset_rounding = offset_ % base::SysInfo::VMAllocationGranularity();
  size_t real_offset = offset_ - offset_rounding;
  size_t real_length = length_ + offset_rounding;

  if (!shared_memory_.MapAt(static_cast<off_t>(real_offset), real_length))
    return false;

  base_ = static_cast<char*>(shared_memory_.memory()) + offset_rounding;
  return true;
}

void PlatformSharedBufferMapping::Unmap() {
  shared_memory_.Unmap();
}

}  // namespace edk
}  // namespace mojo
