// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_android.h"

#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include <limits>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_allocator_android.h"
#include "base/synchronization/lock.h"
#include "third_party/ashmem/ashmem.h"

namespace base {
namespace {

const size_t kPageSize = 4096;

const char kAshmemAllocatorName[] = "DiscardableMemoryAllocator";

struct GlobalContext {
  GlobalContext()
      : ashmem_fd_limit(GetSoftFDLimit()),
        allocator(kAshmemAllocatorName),
        ashmem_fd_count_(0) {
  }

  const int ashmem_fd_limit;
  internal::DiscardableMemoryAllocator allocator;
  Lock lock;

  int ashmem_fd_count() const {
    lock.AssertAcquired();
    return ashmem_fd_count_;
  }

  void decrement_ashmem_fd_count() {
    lock.AssertAcquired();
    --ashmem_fd_count_;
  }

  void increment_ashmem_fd_count() {
    lock.AssertAcquired();
    ++ashmem_fd_count_;
  }

 private:
  static int GetSoftFDLimit() {
    struct rlimit limit_info;
    if (getrlimit(RLIMIT_NOFILE, &limit_info) != 0)
      return 128;
    // Allow 25% of file descriptor capacity for ashmem.
    return limit_info.rlim_cur / 4;
  }

  int ashmem_fd_count_;
};

LazyInstance<GlobalContext>::Leaky g_context = LAZY_INSTANCE_INITIALIZER;

// This is the default implementation of DiscardableMemory on Android which is
// used when file descriptor usage is under the soft limit. When file descriptor
// usage gets too high the discardable memory allocator is used instead. See
// ShouldUseAllocator() below for more details.
class DiscardableMemoryAndroidSimple : public DiscardableMemory {
 public:
  DiscardableMemoryAndroidSimple(int fd, void* address, size_t size)
      : fd_(fd),
        memory_(address),
        size_(size) {
    DCHECK_GE(fd_, 0);
    DCHECK(memory_);
  }

  virtual ~DiscardableMemoryAndroidSimple() {
    internal::CloseAshmemRegion(fd_, size_, memory_);
  }

  // DiscardableMemory:
  virtual LockDiscardableMemoryStatus Lock() OVERRIDE {
    return internal::LockAshmemRegion(fd_, 0, size_, memory_);
  }

  virtual void Unlock() OVERRIDE {
    internal::UnlockAshmemRegion(fd_, 0, size_, memory_);
  }

  virtual void* Memory() const OVERRIDE {
    return memory_;
  }

 private:
  const int fd_;
  void* const memory_;
  const size_t size_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryAndroidSimple);
};

int GetCurrentNumberOfAshmemFDs() {
  AutoLock lock(g_context.Get().lock);
  return g_context.Get().ashmem_fd_count();
}

// Returns whether the provided size can be safely page-aligned (without causing
// an overflow).
bool CheckSizeCanBeAlignedToNextPage(size_t size) {
  return size <= std::numeric_limits<size_t>::max() - kPageSize + 1;
}

}  // namespace

namespace internal {

size_t AlignToNextPage(size_t size) {
  DCHECK_EQ(static_cast<int>(kPageSize), getpagesize());
  DCHECK(CheckSizeCanBeAlignedToNextPage(size));
  const size_t mask = ~(kPageSize - 1);
  return (size + kPageSize - 1) & mask;
}

bool CreateAshmemRegion(const char* name,
                        size_t size,
                        int* out_fd,
                        void** out_address) {
  AutoLock lock(g_context.Get().lock);
  if (g_context.Get().ashmem_fd_count() + 1 > g_context.Get().ashmem_fd_limit)
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
  g_context.Get().increment_ashmem_fd_count();
  *out_fd = fd;
  *out_address = address;
  return true;
}

bool CloseAshmemRegion(int fd, size_t size, void* address) {
  AutoLock lock(g_context.Get().lock);
  g_context.Get().decrement_ashmem_fd_count();
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

}  // namespace internal

// static
bool DiscardableMemory::SupportedNatively() {
  return true;
}

// Allocation can happen in two ways:
// - Each client-requested allocation is backed by an individual ashmem region.
// This allows deleting ashmem regions individually by closing the ashmem file
// descriptor. This is the default path that is taken when file descriptor usage
// allows us to do so or when the allocation size would require and entire
// ashmem region.
// - Allocations are performed by the global allocator when file descriptor
// usage gets too high. This still allows unpinning but does not allow deleting
// (i.e. releasing the physical pages backing) individual regions.
//
// TODO(pliard): consider tuning the size threshold used below. For instance we
// might want to make it a fraction of kMinAshmemRegionSize and also
// systematically have small allocations go through the allocator to let big
// allocations systematically go through individual ashmem regions.
//
// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemory(
    size_t size) {
  if (!CheckSizeCanBeAlignedToNextPage(size))
    return scoped_ptr<DiscardableMemory>();
  // Pinning & unpinning works with page granularity therefore align the size
  // upfront.
  const size_t aligned_size = internal::AlignToNextPage(size);
  // Note that the following code is slightly racy. The worst that can happen in
  // practice though is taking the wrong decision (e.g. using the allocator
  // rather than DiscardableMemoryAndroidSimple). Moreover keeping the lock
  // acquired for the whole allocation would cause a deadlock when the allocator
  // tries to create an ashmem region.
  const size_t kAllocatorRegionSize =
      internal::DiscardableMemoryAllocator::kMinAshmemRegionSize;
  GlobalContext* const global_context = g_context.Pointer();
  if (aligned_size >= kAllocatorRegionSize ||
      GetCurrentNumberOfAshmemFDs() < 0.9 * global_context->ashmem_fd_limit) {
    int fd;
    void* address;
    if (internal::CreateAshmemRegion("", aligned_size, &fd, &address)) {
      return scoped_ptr<DiscardableMemory>(
          new DiscardableMemoryAndroidSimple(fd, address, aligned_size));
    }
  }
  return global_context->allocator.Allocate(size);
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
