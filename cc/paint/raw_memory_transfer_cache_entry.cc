// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/raw_memory_transfer_cache_entry.h"

namespace cc {

ClientRawMemoryTransferCacheEntry::ClientRawMemoryTransferCacheEntry(
    std::vector<uint8_t> data)
    : data_(std::move(data)) {}
ClientRawMemoryTransferCacheEntry::~ClientRawMemoryTransferCacheEntry() =
    default;

TransferCacheEntryType ClientRawMemoryTransferCacheEntry::Type() const {
  return TransferCacheEntryType::kRawMemory;
}

size_t ClientRawMemoryTransferCacheEntry::SerializedSize() const {
  return data_.size();
}

bool ClientRawMemoryTransferCacheEntry::Serialize(size_t size,
                                                  uint8_t* data) const {
  if (size != data_.size())
    return false;

  memcpy(data, data_.data(), size);
  return true;
}

ServiceRawMemoryTransferCacheEntry::ServiceRawMemoryTransferCacheEntry() =
    default;
ServiceRawMemoryTransferCacheEntry::~ServiceRawMemoryTransferCacheEntry() =
    default;

TransferCacheEntryType ServiceRawMemoryTransferCacheEntry::Type() const {
  return TransferCacheEntryType::kRawMemory;
}

size_t ServiceRawMemoryTransferCacheEntry::Size() const {
  return data_.size();
}

bool ServiceRawMemoryTransferCacheEntry::Deserialize(size_t size,
                                                     uint8_t* data) {
  data_.resize(size);
  memcpy(data_.data(), data, size);
  return true;
}

}  // namespace cc
