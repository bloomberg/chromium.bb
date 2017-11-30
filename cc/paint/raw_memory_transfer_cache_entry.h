// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_RAW_MEMORY_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_RAW_MEMORY_TRANSFER_CACHE_ENTRY_H_

#include <vector>

#include "cc/paint/transfer_cache_entry.h"

namespace cc {

// Client/ServiceRawMemoryTransferCacheEntry implement a transfer cache entry
// backed by raw memory, with no conversion during serialization or
// deserialization.
class CC_PAINT_EXPORT ClientRawMemoryTransferCacheEntry
    : public ClientTransferCacheEntry {
 public:
  explicit ClientRawMemoryTransferCacheEntry(std::vector<uint8_t> data);
  ~ClientRawMemoryTransferCacheEntry() override;
  TransferCacheEntryType Type() const override;
  size_t SerializedSize() const override;
  bool Serialize(size_t size, uint8_t* data) const override;

 private:
  std::vector<uint8_t> data_;
};

class CC_PAINT_EXPORT ServiceRawMemoryTransferCacheEntry
    : public ServiceTransferCacheEntry {
 public:
  ServiceRawMemoryTransferCacheEntry();
  ~ServiceRawMemoryTransferCacheEntry() override;
  TransferCacheEntryType Type() const override;
  size_t Size() const override;
  bool Deserialize(GrContext* context, size_t size, uint8_t* data) override;
  const std::vector<uint8_t>& data() { return data_; }

 private:
  std::vector<uint8_t> data_;
};

}  // namespace cc

#endif  // CC_PAINT_RAW_MEMORY_TRANSFER_CACHE_ENTRY_H_
