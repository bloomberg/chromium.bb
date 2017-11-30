// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/transfer_cache_entry.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"

namespace cc {

std::unique_ptr<ServiceTransferCacheEntry> ServiceTransferCacheEntry::Create(
    TransferCacheEntryType type) {
  switch (type) {
    case TransferCacheEntryType::kRawMemory:
      return base::MakeUnique<ServiceRawMemoryTransferCacheEntry>();
    case TransferCacheEntryType::kImage:
      return base::MakeUnique<ServiceImageTransferCacheEntry>();
  }

  NOTREACHED();
  return nullptr;
}

bool ServiceTransferCacheEntry::SafeConvertToType(
    uint32_t raw_type,
    TransferCacheEntryType* type) {
  if (raw_type > static_cast<uint32_t>(TransferCacheEntryType::kLast))
    return false;

  *type = static_cast<TransferCacheEntryType>(raw_type);
  return true;
}

}  // namespace cc
