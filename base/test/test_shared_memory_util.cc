// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_shared_memory_util.h"

#include <gtest/gtest.h>

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(OS_FUCHSIA)
#include <zircon/process.h>
#include <zircon/rights.h>
#include <zircon/syscalls.h>
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach_vm.h>
#endif

#if defined(OS_WIN)
#include <aclapi.h>
#endif

namespace base {

#if !defined(OS_NACL)

static const size_t kDataSize = 1024;

// Common routine used with Posix file descriptors. Check that shared memory
// file descriptor |fd| does not allow writable mappings. Return true on
// success, false otherwise.
#if defined(OS_POSIX) && !defined(OS_FUCHSIA)
static bool CheckReadOnlySharedMemoryFdPosix(int fd) {
// Note that the error on Android is EPERM, unlike other platforms where
// it will be EACCES.
#if defined(OS_ANDROID)
  const int kExpectedErrno = EPERM;
#else
  const int kExpectedErrno = EACCES;
#endif
  errno = 0;
  void* address =
      mmap(nullptr, kDataSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  const bool success = (address != nullptr) && (address != MAP_FAILED);
  if (success) {
    LOG(ERROR) << "mmap() should have failed!";
    munmap(address, kDataSize);  // Cleanup.
    return false;
  }
  if (errno != kExpectedErrno) {
    LOG(ERROR) << "Expected mmap() to return " << kExpectedErrno
               << " but returned " << errno << ": " << strerror(errno) << "\n";
    return false;
  }
  return true;
}
#endif  // OS_POSIX && !OS_FUCHSIA

#if defined(OS_FUCHSIA)
// Fuchsia specific implementation.
bool CheckReadOnlySharedMemoryHandleForTesting(SharedMemoryHandle handle) {
  const uint32_t flags = ZX_VM_FLAG_PERM_READ | ZX_VM_FLAG_PERM_WRITE;
  uintptr_t addr;
  const zx_handle_t root = zx_vmar_root_self();
  const zx_status_t status =
      zx_vmar_map(root, 0, handle.GetHandle(), 0U, kDataSize, flags, &addr);
  if (status == ZX_OK) {
    LOG(ERROR) << "zx_vmar_map() should have failed!";
    zx_vmar_unmap(root, addr, kDataSize);
    return false;
  }
  if (status != ZX_ERR_ACCESS_DENIED) {
    LOG(ERROR) << "Expected zx_vmar_map() to return " << ZX_ERR_ACCESS_DENIED
               << " (ZX_ERR_ACCESS_DENIED) but returned " << status << "\n";
    return false;
  }
  return true;
}
#elif defined(OS_MACOSX) && !defined(OS_IOS)
// For OSX, the code has to deal with both POSIX and MACH handles.
bool CheckReadOnlySharedMemoryHandleForTesting(SharedMemoryHandle handle) {
  if (handle.type_ == SharedMemoryHandle::POSIX)
    return CheckReadOnlySharedMemoryFdPosix(handle.file_descriptor_.fd);

  mach_vm_address_t memory;
  const kern_return_t kr = mach_vm_map(
      mach_task_self(), &memory, kDataSize, 0, VM_FLAGS_ANYWHERE,
      handle.memory_object_, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE,
      VM_PROT_READ | VM_PROT_WRITE | VM_PROT_IS_MASK, VM_INHERIT_NONE);
  if (kr == KERN_SUCCESS) {
    LOG(ERROR) << "mach_vm_map() should have failed!";
    mach_vm_deallocate(mach_task_self(), memory, kDataSize);  // Cleanup.
    return false;
  }
  return true;
}
#elif defined(OS_WIN)
bool CheckReadOnlySharedMemoryHandleForTesting(SharedMemoryHandle handle) {
  void* memory = MapViewOfFile(handle.GetHandle(),
                               FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, kDataSize);
  if (memory != nullptr) {
    LOG(ERROR) << "MapViewOfFile() should have failed!";
    UnmapViewOfFile(memory);
    return false;
  }
  return true;
}
#else
bool CheckReadOnlySharedMemoryHandleForTesting(SharedMemoryHandle handle) {
  return CheckReadOnlySharedMemoryFdPosix(handle.GetHandle());
}
#endif

#endif  // !OS_NACL

}  // namespace base
