// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <errno.h>
#include <mach/mach_vm.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_mach_vm.h"
#include "base/memory/shared_memory_helper.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/safe_strerror.h"
#include "base/process/process_metrics.h"
#include "base/scoped_generic.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif  // OS_MACOSX

namespace base {

namespace {

// Returns whether the operation succeeded.
// |new_handle| is an output variable, populated on success. The caller takes
// ownership of the underlying memory object.
// |handle| is the handle to copy.
// If |handle| is already mapped, |mapped_addr| is its mapped location.
// Otherwise, |mapped_addr| should be |nullptr|.
bool MakeMachSharedMemoryHandleReadOnly(SharedMemoryHandle* new_handle,
                                        SharedMemoryHandle handle,
                                        void* mapped_addr) {
  if (!handle.IsValid())
    return false;

  size_t size = handle.GetSize();

  // Map if necessary.
  void* temp_addr = mapped_addr;
  base::mac::ScopedMachVM scoper;
  if (!temp_addr) {
    // Intentionally lower current prot and max prot to |VM_PROT_READ|.
    kern_return_t kr = mach_vm_map(
        mach_task_self(), reinterpret_cast<mach_vm_address_t*>(&temp_addr),
        size, 0, VM_FLAGS_ANYWHERE, handle.GetMemoryObject(), 0, FALSE,
        VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS)
      return false;
    scoper.reset(reinterpret_cast<vm_address_t>(temp_addr),
                 mach_vm_round_page(size));
  }

  // Make new memory object.
  mach_port_t named_right;
  kern_return_t kr = mach_make_memory_entry_64(
      mach_task_self(), reinterpret_cast<memory_object_size_t*>(&size),
      reinterpret_cast<memory_object_offset_t>(temp_addr), VM_PROT_READ,
      &named_right, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS)
    return false;

  *new_handle = SharedMemoryHandle(named_right, size, handle.GetGUID());
  return true;
}

}  // namespace

SharedMemory::SharedMemory() {}

SharedMemory::SharedMemory(const SharedMemoryHandle& handle, bool read_only)
    : mapped_memory_mechanism_(SharedMemoryHandle::POSIX),
      shm_(handle),
      read_only_(read_only) {}

SharedMemory::~SharedMemory() {
  Unmap();
  Close();
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.IsValid();
}

// static
void SharedMemory::CloseHandle(const SharedMemoryHandle& handle) {
  handle.Close();
}

// static
size_t SharedMemory::GetHandleLimit() {
  return GetMaxFds();
}

// static
SharedMemoryHandle SharedMemory::DuplicateHandle(
    const SharedMemoryHandle& handle) {
  return handle.Duplicate();
}

// static
int SharedMemory::GetFdFromSharedMemoryHandle(
    const SharedMemoryHandle& handle) {
  return handle.file_descriptor_.fd;
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

// Chromium mostly only uses the unique/private shmem as specified by
// "name == L"". The exception is in the StatsTable.
bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  DCHECK(!shm_.IsValid());
  if (options.size == 0) {
    last_error_ = SharedMemoryError::BAD_PARAMS;
    return false;
  }

  if (options.size > static_cast<size_t>(std::numeric_limits<int>::max())) {
    last_error_ = SharedMemoryError::BAD_PARAMS;
    return false;
  }

  if (options.type == SharedMemoryHandle::MACH) {
    shm_ = SharedMemoryHandle(options.size, UnguessableToken::Create());
    requested_size_ = options.size;
    return shm_.IsValid();
  }

  // This function theoretically can block on the disk. Both profiling of real
  // users and local instrumentation shows that this is a real problem.
  // https://code.google.com/p/chromium/issues/detail?id=466437
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  ScopedFILE fp;
  ScopedFD readonly_fd;

  FilePath path;
  bool result = CreateAnonymousSharedMemory(options, &fp, &readonly_fd, &path,
                                            &last_error_);
  if (!result)
    return false;
  DCHECK(fp);  // Should be guaranteed by CreateAnonymousSharedMemory().

  // Get current size.
  struct stat stat;
  if (fstat(fileno(fp.get()), &stat) != 0) {
    last_error_ = SharedMemoryError::STAT_FAILED;
    return false;
  }
  const size_t current_size = stat.st_size;
  if (current_size != options.size) {
    if (HANDLE_EINTR(ftruncate(fileno(fp.get()), options.size)) != 0) {
      last_error_ = SharedMemoryError::TRUNCATE_FAILED;
      return false;
    }
  }
  requested_size_ = options.size;

  int mapped_file = -1;
  int readonly_mapped_file = -1;
  result = PrepareMapFile(std::move(fp), std::move(readonly_fd), &mapped_file,
                          &readonly_mapped_file, &last_error_);
  shm_ = SharedMemoryHandle(FileDescriptor(mapped_file, false), options.size,
                            UnguessableToken::Create());
  readonly_shm_ =
      SharedMemoryHandle(FileDescriptor(readonly_mapped_file, false),
                         options.size, shm_.GetGUID());
  return result;
}

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (!shm_.IsValid()) {
    last_error_ = SharedMemoryError::BAD_PARAMS;
    return false;
  }
  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max())) {
    last_error_ = SharedMemoryError::BAD_PARAMS;
    return false;
  }
  if (memory_) {
    last_error_ = SharedMemoryError::BAD_PARAMS;
    return false;
  }

  bool success = shm_.MapAt(offset, bytes, &memory_, read_only_);
  if (success) {
    mapped_size_ = bytes;
    DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(memory_) &
                      (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
    mapped_memory_mechanism_ = shm_.type_;
  } else {
    last_error_ = SharedMemoryError::MMAP_FAILED;
    memory_ = NULL;
  }

  return success;
}

bool SharedMemory::Unmap() {
  if (memory_ == NULL)
    return false;

  switch (mapped_memory_mechanism_) {
    case SharedMemoryHandle::POSIX:
      munmap(memory_, mapped_size_);
      break;
    case SharedMemoryHandle::MACH:
      mach_vm_deallocate(mach_task_self(),
                         reinterpret_cast<mach_vm_address_t>(memory_),
                         mapped_size_);
      break;
  }

  memory_ = NULL;
  mapped_size_ = 0;
  return true;
}

SharedMemoryHandle SharedMemory::handle() const {
  return shm_;
}

SharedMemoryHandle SharedMemory::TakeHandle() {
  SharedMemoryHandle dup = DuplicateHandle(handle());
  Close();
  return dup;
}

void SharedMemory::Close() {
  shm_.Close();
  shm_ = SharedMemoryHandle();
  if (shm_.type_ == SharedMemoryHandle::POSIX) {
    if (readonly_shm_.IsValid()) {
      readonly_shm_.Close();
      readonly_shm_ = SharedMemoryHandle();
    }
  }
}

SharedMemoryHandle SharedMemory::GetReadOnlyHandle() {
  if (shm_.type_ == SharedMemoryHandle::POSIX) {
    // We could imagine re-opening the file from /dev/fd, but that can't make it
    // readonly on Mac: https://codereview.chromium.org/27265002/#msg10.
    CHECK(readonly_shm_.IsValid());
    return readonly_shm_.Duplicate();
  }

  DCHECK(shm_.IsValid());
  base::SharedMemoryHandle new_handle;
  bool success = MakeMachSharedMemoryHandleReadOnly(&new_handle, shm_, memory_);
  if (success)
    new_handle.SetOwnershipPassesToIPC(true);
  return new_handle;
}

}  // namespace base
