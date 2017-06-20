// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/platform_shared_buffer.h"

#include <stddef.h>

#include <utility>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "base/sys_info.h"
#include "mojo/edk/embedder/platform_handle_utils.h"

#if defined(OS_NACL)
// For getpagesize() on NaCl.
#include <unistd.h>
#endif

namespace mojo {
namespace edk {

namespace {

// Takes ownership of |memory_handle|.
ScopedPlatformHandle SharedMemoryToPlatformHandle(
    base::SharedMemoryHandle memory_handle) {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  return ScopedPlatformHandle(PlatformHandle(memory_handle.GetMemoryObject()));
#else
  return ScopedPlatformHandle(PlatformHandle(memory_handle.GetHandle()));
#endif
}

}  // namespace

// static
PlatformSharedBuffer* PlatformSharedBuffer::Create(size_t num_bytes) {
  DCHECK_GT(num_bytes, 0u);

  PlatformSharedBuffer* rv = new PlatformSharedBuffer(num_bytes, false);
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
    bool read_only,
    const base::UnguessableToken& guid,
    ScopedPlatformHandle platform_handle) {
  DCHECK_GT(num_bytes, 0u);

  PlatformSharedBuffer* rv = new PlatformSharedBuffer(num_bytes, read_only);
  if (!rv->InitFromPlatformHandle(guid, std::move(platform_handle))) {
    // We can't just delete it directly, due to the "in destructor" (debug)
    // check.
    scoped_refptr<PlatformSharedBuffer> deleter(rv);
    return nullptr;
  }

  return rv;
}

// static
PlatformSharedBuffer* PlatformSharedBuffer::CreateFromPlatformHandlePair(
    size_t num_bytes,
    const base::UnguessableToken& guid,
    ScopedPlatformHandle rw_platform_handle,
    ScopedPlatformHandle ro_platform_handle) {
  DCHECK_GT(num_bytes, 0u);
  DCHECK(rw_platform_handle.is_valid());
  DCHECK(ro_platform_handle.is_valid());

  PlatformSharedBuffer* rv = new PlatformSharedBuffer(num_bytes, false);
  if (!rv->InitFromPlatformHandlePair(guid, std::move(rw_platform_handle),
                                      std::move(ro_platform_handle))) {
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

  PlatformSharedBuffer* rv = new PlatformSharedBuffer(num_bytes, read_only);
  rv->InitFromSharedMemoryHandle(handle);

  return rv;
}

size_t PlatformSharedBuffer::GetNumBytes() const {
  return num_bytes_;
}

bool PlatformSharedBuffer::IsReadOnly() const {
  return read_only_;
}

base::UnguessableToken PlatformSharedBuffer::GetGUID() const {
  DCHECK(shared_memory_);
  return shared_memory_->handle().GetGUID();
}

std::unique_ptr<PlatformSharedBufferMapping> PlatformSharedBuffer::Map(
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

std::unique_ptr<PlatformSharedBufferMapping> PlatformSharedBuffer::MapNoCheck(
    size_t offset,
    size_t length) {
  DCHECK(IsValidMap(offset, length));
  DCHECK(shared_memory_);
  base::SharedMemoryHandle handle;
  {
    base::AutoLock locker(lock_);
    handle = base::SharedMemory::DuplicateHandle(shared_memory_->handle());
  }

  if (!handle.IsValid())
    return nullptr;

  std::unique_ptr<PlatformSharedBufferMapping> mapping(
      new PlatformSharedBufferMapping(handle, read_only_, offset, length));
  if (mapping->Map())
    return base::WrapUnique(mapping.release());

  return nullptr;
}

ScopedPlatformHandle PlatformSharedBuffer::DuplicatePlatformHandle() {
  DCHECK(shared_memory_);
  base::SharedMemoryHandle handle;
  {
    base::AutoLock locker(lock_);
    handle = base::SharedMemory::DuplicateHandle(shared_memory_->handle());
  }
  if (!handle.IsValid())
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

base::SharedMemoryHandle PlatformSharedBuffer::DuplicateSharedMemoryHandle() {
  DCHECK(shared_memory_);

  base::AutoLock locker(lock_);
  return base::SharedMemory::DuplicateHandle(shared_memory_->handle());
}

PlatformSharedBuffer* PlatformSharedBuffer::CreateReadOnlyDuplicate() {
  DCHECK(shared_memory_);

  if (ro_shared_memory_) {
    base::AutoLock locker(lock_);
    base::SharedMemoryHandle handle;
    handle = base::SharedMemory::DuplicateHandle(ro_shared_memory_->handle());
    if (!handle.IsValid())
      return nullptr;
    return CreateFromSharedMemoryHandle(num_bytes_, true, handle);
  }

  base::SharedMemoryHandle handle;
  {
    base::AutoLock locker(lock_);
    handle = shared_memory_->GetReadOnlyHandle();
  }
  if (!handle.IsValid())
    return nullptr;

  return CreateFromSharedMemoryHandle(num_bytes_, true, handle);
}

PlatformSharedBuffer::PlatformSharedBuffer(size_t num_bytes, bool read_only)
    : num_bytes_(num_bytes), read_only_(read_only) {}

PlatformSharedBuffer::~PlatformSharedBuffer() {}

bool PlatformSharedBuffer::Init() {
  DCHECK(!shared_memory_);
  DCHECK(!read_only_);

  base::SharedMemoryCreateOptions options;
  options.size = num_bytes_;
  // By default, we can share as read-only.
  options.share_read_only = true;

  shared_memory_.reset(new base::SharedMemory);
  return shared_memory_->Create(options);
}

bool PlatformSharedBuffer::InitFromPlatformHandle(
    const base::UnguessableToken& guid,
    ScopedPlatformHandle platform_handle) {
  DCHECK(!shared_memory_);

#if defined(OS_WIN)
  base::SharedMemoryHandle handle(platform_handle.release().handle, num_bytes_,
                                  guid);
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  base::SharedMemoryHandle handle = base::SharedMemoryHandle(
      platform_handle.release().port, num_bytes_, guid);
#else
  base::SharedMemoryHandle handle(
      base::FileDescriptor(platform_handle.release().handle, false), num_bytes_,
      guid);
#endif

  shared_memory_.reset(new base::SharedMemory(handle, read_only_));
  return true;
}

bool PlatformSharedBuffer::InitFromPlatformHandlePair(
    const base::UnguessableToken& guid,
    ScopedPlatformHandle rw_platform_handle,
    ScopedPlatformHandle ro_platform_handle) {
#if defined(OS_MACOSX)
  NOTREACHED();
  return false;
#else  // defined(OS_MACOSX)

#if defined(OS_WIN)
  base::SharedMemoryHandle handle(rw_platform_handle.release().handle,
                                  num_bytes_, guid);
  base::SharedMemoryHandle ro_handle(ro_platform_handle.release().handle,
                                     num_bytes_, guid);
#else   // defined(OS_WIN)
  base::SharedMemoryHandle handle(
      base::FileDescriptor(rw_platform_handle.release().handle, false),
      num_bytes_, guid);
  base::SharedMemoryHandle ro_handle(
      base::FileDescriptor(ro_platform_handle.release().handle, false),
      num_bytes_, guid);
#endif  // defined(OS_WIN)

  DCHECK(!shared_memory_);
  shared_memory_.reset(new base::SharedMemory(handle, false));
  ro_shared_memory_.reset(new base::SharedMemory(ro_handle, true));
  return true;

#endif  // defined(OS_MACOSX)
}

void PlatformSharedBuffer::InitFromSharedMemoryHandle(
    base::SharedMemoryHandle handle) {
  DCHECK(!shared_memory_);

  shared_memory_.reset(new base::SharedMemory(handle, read_only_));
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
#if defined(OS_NACL)
  // base::SysInfo isn't available under NaCl.
  size_t page_size = getpagesize();
#else
  size_t page_size = base::SysInfo::VMAllocationGranularity();
#endif
  size_t offset_rounding = offset_ % page_size;
  size_t real_offset = offset_ - offset_rounding;
  size_t real_length = length_ + offset_rounding;

  bool result =
      shared_memory_.MapAt(static_cast<off_t>(real_offset), real_length);
  DCHECK(result);
  base_ = static_cast<char*>(shared_memory_.memory()) + offset_rounding;
  return true;
}

void PlatformSharedBufferMapping::Unmap() {
  shared_memory_.Unmap();
}

}  // namespace edk
}  // namespace mojo
