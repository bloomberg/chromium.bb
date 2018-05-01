// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_CONSTANTS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_CONSTANTS_H_

#include <stddef.h>

#include "build/build_config.h"

namespace base {
#if defined(OS_WIN)
static const size_t kPageAllocationGranularityShift = 16;  // 64KB
#elif defined(_MIPS_ARCH_LOONGSON)
static const size_t kPageAllocationGranularityShift = 14;  // 16KB
#else
static const size_t kPageAllocationGranularityShift = 12;  // 4KB
#endif
static const size_t kPageAllocationGranularity =
    1 << kPageAllocationGranularityShift;
static const size_t kPageAllocationGranularityOffsetMask =
    kPageAllocationGranularity - 1;
static const size_t kPageAllocationGranularityBaseMask =
    ~kPageAllocationGranularityOffsetMask;

#if defined(_MIPS_ARCH_LOONGSON)
static const size_t kSystemPageSize = 16384;
#else
static const size_t kSystemPageSize = 4096;
#endif
static const size_t kSystemPageOffsetMask = kSystemPageSize - 1;
static_assert((kSystemPageSize & (kSystemPageSize - 1)) == 0,
              "kSystemPageSize must be power of 2");
static const size_t kSystemPageBaseMask = ~kSystemPageOffsetMask;

static const size_t kPageMetadataShift = 5;  // 32 bytes per partition page.
static const size_t kPageMetadataSize = 1 << kPageMetadataShift;

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_CONSTANTS_H
