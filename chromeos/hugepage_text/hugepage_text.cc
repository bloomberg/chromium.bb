// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Author: Ken Chen <kenchen@google.com>
//
// hugepage text library to remap process executable segment with hugepages.

#include "chromeos/hugepage_text/hugepage_text.h"

#include <link.h>
#include <sys/mman.h>

#include "base/bit_cast.h"
#include "base/logging.h"

namespace chromeos {

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000
#endif

#ifndef TMPFS_MAGIC
#define TMPFS_MAGIC 0x01021994
#endif

#ifndef MADV_HUGEPAGE
#define MADV_HUGEPAGE 14
#endif

const int kHpageShift = 21;
const int kHpageSize = (1 << kHpageShift);
const int kHpageMask = (~(kHpageSize - 1));

const int kProtection = (PROT_READ | PROT_WRITE);
const int kMmapFlags = (MAP_ANONYMOUS | MAP_SHARED);
const int kMmapHtlbFlags = (kMmapFlags | MAP_HUGETLB);
const int kMremapFlags = (MREMAP_MAYMOVE | MREMAP_FIXED);

// mremap syscall is always supported for small page segment on all kernels.
// However, it is not the case for hugepage.
// If not used carefully, mremap() a hugepage segment directly onto small page
// text segment will cause irreversible damage to the existing text mapping
// and cause process to segfault. This function will dynamically at run time
// determine whether a process can safely execute mremap on a hugepage segment
// without taking the process down.
//
// Inputs: none
// Return: true if mremap on hugepage segment is supported on the host OS.
static int HugetlbMremapSupported(void) {
  void *haddr = 0, *raddr = 0, *taddr;
  const size_t size = kHpageSize;
  int ret = 0;

  // use a pair of hugepage memory segments to test whether mremap() is
  // supported
  haddr = mmap(NULL, size, kProtection, kMmapHtlbFlags | MAP_NORESERVE, 0, 0);
  if (haddr == MAP_FAILED) {
    return 0;
  }
  taddr = mmap(NULL, size, kProtection, kMmapHtlbFlags | MAP_NORESERVE, 0, 0);
  if (taddr == MAP_FAILED) {
    munmap(haddr, size);
    return 0;
  }

  raddr = mremap(haddr, size, size, kMremapFlags, taddr);
  if (raddr != MAP_FAILED) {
    ret = 1;
    // clean up.  raddr == taddr; also haddr is implicitly unmapped
    munmap(raddr, size);
  } else {
    // mremap fail, clean up both src and dst segments
    munmap(haddr, size);
    munmap(taddr, size);
  }

  return ret;
}

// Get an anonymous mapping backed by explicit transparent hugepage
// Return NULL if such mapping can not be established.
static void* GetTransparentHugepageMapping(const size_t hsize) {
  // setup explicit transparent hugepage segment
  char* addr = static_cast<char*>(mmap(NULL, hsize + kHpageSize, kProtection,
                                       MAP_ANONYMOUS | MAP_PRIVATE, 0, 0));
  if (addr == MAP_FAILED) {
    PLOG(INFO) << "unable to mmap anon pages, fall back to small page";
    return NULL;
  }
  // remove unaligned head and tail regions
  size_t head_gap = kHpageSize - (size_t)addr % kHpageSize;
  size_t tail_gap = kHpageSize - head_gap;
  munmap(addr, head_gap);
  munmap(addr + head_gap + hsize, tail_gap);

  void* haddr = addr + head_gap;
  if (madvise(haddr, hsize, MADV_HUGEPAGE)) {
    PLOG(INFO) << "no transparent hugepage support, fall back to small page";
    munmap(haddr, hsize);
    return NULL;
  }
  return haddr;
}

// memcpy for word-aligned data which is not instrumented by AddressSanitizer.
ATTRIBUTE_NO_SANITIZE_ADDRESS
static void NoAsanAlignedMemcpy(void* dst, void* src, size_t size) {
  DCHECK_EQ(0U, size % sizeof(uintptr_t));  // size is a multiple of word size.
  DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(dst) % sizeof(uintptr_t));
  DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(src) % sizeof(uintptr_t));
  uintptr_t* d = reinterpret_cast<uintptr_t*>(dst);
  uintptr_t* s = reinterpret_cast<uintptr_t*>(src);
  for (size_t i = 0; i < size / sizeof(uintptr_t); i++)
    d[i] = s[i];
}

// Remaps text segment at address "vaddr" to hugepage backed mapping via mremap
// syscall.  The virtual address does not change.  When this call returns, the
// backing physical memory will be changed from small page to hugetlb page.
//
// Inputs: vaddr, the starting virtual address to remap to hugepage
//         hsize, size of the memory segment to remap in bytes
// Return: none
// Effect: physical backing page changed from small page to hugepage. If there
//         are error condition, the remapping operation is aborted.
static void MremapHugetlbText(void* vaddr, const size_t hsize) {
  DCHECK_EQ(0ul, reinterpret_cast<uintptr_t>(vaddr) & ~kHpageMask);
  void* haddr = MAP_FAILED;
  if (HugetlbMremapSupported()) {
    // Try anon hugepage from static hugepage pool only if the source address
    // is hugepage aligned, otherwise, mremap below has non-recoverable error.
    haddr = mmap(NULL, hsize, kProtection, kMmapHtlbFlags, 0, 0);
  }

  if (haddr == MAP_FAILED) {
    PLOG(INFO) << "static hugepage not available, trying transparent hugepage";
    haddr = GetTransparentHugepageMapping(hsize);
    if (haddr == NULL)
      return;
  }

  // Copy text segment to hugepage mapping. We are using a non-asan memcpy,
  // otherwise it would be flagged as a bunch of out of bounds reads.
  NoAsanAlignedMemcpy(haddr, vaddr, hsize);

  // change mapping protection to read only now that it has done the copy
  if (mprotect(haddr, hsize, PROT_READ | PROT_EXEC)) {
    PLOG(INFO) << "can not change protection to r-x, fall back to small page";
    munmap(haddr, hsize);
    return;
  }

  // remap hugepage text on top of existing small page mapping
  if (mremap(haddr, hsize, hsize, kMremapFlags, vaddr) == MAP_FAILED) {
    PLOG(INFO) << "unable to mremap hugepage mapping, fall back to small page";
    munmap(haddr, hsize);
    return;
  }
}

// Top level text remapping function.
//
// Inputs: vaddr, the starting virtual address to remap to hugepage
//         hsize, size of the memory segment to remap in bytes
// Return: none
// Effect: physical backing page changed from small page to hugepage. If there
//         are error condition, the remaping operation is aborted.
static void RemapHugetlbText(void* vaddr, const size_t segsize) {
  // remove unaligned head regions
  size_t head_gap = (kHpageSize - reinterpret_cast<size_t>(vaddr) % kHpageSize)
                    % kHpageSize;
  size_t addr = reinterpret_cast<size_t>(vaddr) + head_gap;

  int hsize = segsize - head_gap;
  hsize = hsize & kHpageMask;

  if (hsize == 0)
    return;

  MremapHugetlbText(reinterpret_cast<void *>(addr), hsize);
}

// For a given ELF program header descriptor, iterates over all segments within
// it and find the first segment that has PT_LOAD and is executable, call
// RemapHugetlbText().
//
// Inputs: info: pointer to a struct dl_phdr_info that describes the DSO.
//         size: size of the above structure (not used in this function).
//         data: user param (not used in this function).
// Return: always return true.  The value is propagated by dl_iterate_phdr().
static int FilterElfHeader(struct dl_phdr_info* info, size_t size, void* data) {
  void* vaddr;
  int segsize;

  for (int i = 0; i < info->dlpi_phnum; i++) {
    if (info->dlpi_phdr[i].p_type == PT_LOAD &&
        info->dlpi_phdr[i].p_flags == (PF_R | PF_X)) {
      vaddr = bit_cast<void*>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
      segsize = info->dlpi_phdr[i].p_filesz;

      RemapHugetlbText(vaddr, segsize);
      // Only re-map the first text segment.
      return 1;
    }
  }

  return 1;
}

// Main library function.  This function will iterate all ELF segments and
// attempt to remap text segment from small page to hugepage.
// If remapping is successful.  All error conditions are soft fail such that
// effect will be rolled back and remap operation will be aborted.
void ReloadElfTextInHugePages(void) {
  dl_iterate_phdr(FilterElfHeader, 0);
}

}  // namespace chromeos
