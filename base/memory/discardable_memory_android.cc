// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include <sys/mman.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "third_party/ashmem/ashmem.h"

namespace base {
namespace {

// Protects |g_num_discardable_memory| below.
base::LazyInstance<base::Lock>::Leaky g_discardable_memory_lock =
    LAZY_INSTANCE_INITIALIZER;

// Total number of discardable memory in the process.
int g_num_discardable_memory = 0;

// Upper limit on the number of discardable memory to avoid hitting file
// descriptor limit.
const int kDiscardableMemoryNumLimit = 128;

bool CreateAshmemRegion(const char* name,
                        size_t size,
                        int* out_fd,
                        void** out_address) {
  base::AutoLock lock(g_discardable_memory_lock.Get());
  if (g_num_discardable_memory + 1 > kDiscardableMemoryNumLimit)
    return false;
  int fd = ashmem_create_region(name, size);
  if (fd < 0) {
    DLOG(ERROR) << "ashmem_create_region() failed";
    return false;
  }
  file_util::ScopedFD fd_closer(&fd);

  const int err = ashmem_set_prot_region(fd, PROT_READ | PROT_WRITE);
  if (err < 0) {
    DLOG(ERROR) << "Error " << err << " when setting protection of ashmem";
    return false;
  }

  // There is a problem using MAP_PRIVATE here. As we are constantly calling
  // Lock() and Unlock(), data could get lost if they are not written to the
  // underlying file when Unlock() gets called.
  void* const address = mmap(
      NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (address == MAP_FAILED) {
    DPLOG(ERROR) << "Failed to map memory.";
    return false;
  }

  ignore_result(fd_closer.release());
  ++g_num_discardable_memory;
  *out_fd = fd;
  *out_address = address;
  return true;
}

bool DeleteAshmemRegion(int fd, size_t size, void* address) {
  base::AutoLock lock(g_discardable_memory_lock.Get());
  --g_num_discardable_memory;
  if (munmap(address, size) == -1) {
    DPLOG(ERROR) << "Failed to unmap memory.";
    close(fd);
    return false;
  }
  return close(fd) == 0;
}

LockDiscardableMemoryStatus LockAshmemRegion(int fd,
                                             size_t off,
                                             size_t size,
                                             const void* address) {
  const int result = ashmem_pin_region(fd, off, size);
  DCHECK_EQ(0, mprotect(address, size, PROT_READ | PROT_WRITE));
  return result == ASHMEM_WAS_PURGED ?
      DISCARDABLE_MEMORY_PURGED : DISCARDABLE_MEMORY_SUCCESS;
}

bool UnlockAshmemRegion(int fd, size_t off, size_t size, const void* address) {
  const int failed = ashmem_unpin_region(fd, off, size);
  if (failed)
    DLOG(ERROR) << "Failed to unpin memory.";
  // This allows us to catch accesses to unlocked memory.
  DCHECK_EQ(0, mprotect(address, size, PROT_NONE));
  return !failed;
}

class DiscardableMemoryAndroid : public DiscardableMemory {
 public:
  DiscardableMemoryAndroid(int fd, void* address, size_t size)
      : fd_(fd),
        memory_(address),
        size_(size) {
    DCHECK_GE(fd_, 0);
    DCHECK(memory_);
  }

  virtual ~DiscardableMemoryAndroid() {
    DeleteAshmemRegion(fd_, size_, memory_);
  }

  // DiscardableMemory:
  virtual LockDiscardableMemoryStatus Lock() OVERRIDE {
    return LockAshmemRegion(fd_, 0, size_, memory_);
  }

  virtual void Unlock() OVERRIDE {
    UnlockAshmemRegion(fd_, 0, size_, memory_);
  }

  virtual void* Memory() const OVERRIDE {
    return memory_;
  }

 private:
  const int fd_;
  void* const memory_;
  const size_t size_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryAndroid);
};

}  // namespace

// static
bool DiscardableMemory::SupportedNatively() {
  return true;
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemory(
    size_t size) {
  // Pinning & unpinning works with page granularity therefore align the size
  // upfront.
  const size_t kPageSize = 4096;
  const size_t mask = ~(kPageSize - 1);
  size = (size + kPageSize - 1) & mask;
  int fd;
  void* address;
  if (!CreateAshmemRegion("", size, &fd, &address))
    return scoped_ptr<DiscardableMemory>();
  return scoped_ptr<DiscardableMemory>(
      new DiscardableMemoryAndroid(fd, address, size));
}

// static
bool DiscardableMemory::PurgeForTestingSupported() {
  return false;
}

// static
void DiscardableMemory::PurgeForTesting() {
  NOTIMPLEMENTED();
}

}  // namespace base
