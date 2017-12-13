// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TRANSFER_CACHE_SERIALIZE_HELPER_H_
#define CC_PAINT_TRANSFER_CACHE_SERIALIZE_HELPER_H_

#include <set>

#include "cc/paint/paint_export.h"
#include "cc/paint/transfer_cache_entry.h"

namespace cc {

class CC_PAINT_EXPORT TransferCacheSerializeHelper {
 public:
  TransferCacheSerializeHelper();
  virtual ~TransferCacheSerializeHelper();

  bool LockEntry(TransferCacheEntryType type, uint32_t id);
  void CreateEntry(const ClientTransferCacheEntry& entry);
  void FlushEntries();

  void AssertLocked(TransferCacheEntryType type, uint32_t id);

 private:
  using EntryKey = std::pair<TransferCacheEntryType, uint32_t>;
  virtual bool LockEntryInternal(TransferCacheEntryType type, uint32_t id) = 0;
  virtual void CreateEntryInternal(const ClientTransferCacheEntry& entry) = 0;
  virtual void FlushEntriesInternal(const std::set<EntryKey>& entries) = 0;

  std::set<EntryKey> added_entries_;
};

}  // namespace cc

#endif  // CC_PAINT_TRANSFER_CACHE_SERIALIZE_HELPER_H_
