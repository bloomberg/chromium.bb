// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TRANSFER_CACHE_DESERIALIZE_HELPER_H_
#define CC_PAINT_TRANSFER_CACHE_DESERIALIZE_HELPER_H_

#include <cstdint>

#include "cc/paint/paint_export.h"
#include "cc/paint/transfer_cache_entry.h"

namespace cc {

// Helper interface consumed by cc/paint during OOP raster deserialization.
// Provides access to the transfer cache.
// TODO(ericrk): We should use TransferCacheEntryId, not uint64_t here, but
// we need to figure out layering. crbug.com/777622
class CC_PAINT_EXPORT TransferCacheDeserializeHelper {
 public:
  virtual ~TransferCacheDeserializeHelper() {}

  // Type safe access to an entry in the transfer cache. Returns null if the
  // entry is missing or of the wrong type.
  template <typename T>
  T* GetEntryAs(uint64_t id) {
    ServiceTransferCacheEntry* entry = GetEntryInternal(id);
    if (entry == nullptr) {
      return nullptr;
    }
    if (entry->Type() != T::kType)
      return nullptr;
    return static_cast<T*>(entry);
  }

 private:
  virtual ServiceTransferCacheEntry* GetEntryInternal(uint64_t id) = 0;
};

};  // namespace cc

#endif  // CC_PAINT_TRANSFER_CACHE_DESERIALIZE_HELPER_H_
