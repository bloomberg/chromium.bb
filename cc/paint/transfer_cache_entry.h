// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_TRANSFER_CACHE_ENTRY_H_

#include <memory>

#include "cc/paint/paint_export.h"

namespace cc {

// To add a new transfer cache entry type:
//  - Add a type name to the TransferCacheEntryType enum.
//  - Implement a ClientTransferCacheEntry and ServiceTransferCacheEntry for
//    your new type.
//  - Update ServiceTransferCacheEntry::Create and ServiceTransferCacheEntry::
//    DeduceType in transfer_cache_entry.cc.
enum class TransferCacheEntryType : uint32_t {
  kRawMemory,
  // Add new entries above this line, make sure to update kLast.
  kLast = kRawMemory,
};

// An interface used on the client to serialize a transfer cache entry
// into raw bytes that can be sent to the service.
class CC_PAINT_EXPORT ClientTransferCacheEntry {
 public:
  virtual ~ClientTransferCacheEntry() {}
  virtual TransferCacheEntryType Type() const = 0;
  virtual size_t SerializedSize() const = 0;
  virtual bool Serialize(size_t size, uint8_t* data) const = 0;
};

// An interface which receives the raw data sent by the client and
// deserializes it into the appropriate service-side object.
class CC_PAINT_EXPORT ServiceTransferCacheEntry {
 public:
  static std::unique_ptr<ServiceTransferCacheEntry> Create(
      TransferCacheEntryType type);

  // Checks that |raw_type| represents a valid TransferCacheEntryType and
  // populates |type|. If |raw_type| is not valid, the function returns false
  // and |type| is not modified.
  static bool SafeConvertToType(uint32_t raw_type,
                                TransferCacheEntryType* type);

  virtual ~ServiceTransferCacheEntry() {}
  virtual TransferCacheEntryType Type() const = 0;
  virtual size_t Size() const = 0;
  virtual bool Deserialize(size_t size, uint8_t* data) = 0;
};

};  // namespace cc

#endif  // CC_PAINT_TRANSFER_CACHE_ENTRY_H_
