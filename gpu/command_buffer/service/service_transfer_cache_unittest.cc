// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_transfer_cache.h"

#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

constexpr int kDecoderId = 2;
constexpr auto kEntryType = cc::TransferCacheEntryType::kRawMemory;

std::unique_ptr<cc::ServiceTransferCacheEntry> CreateEntry(size_t size) {
  auto entry = std::make_unique<cc::ServiceRawMemoryTransferCacheEntry>();
  std::vector<uint8_t> data(size, 0u);
  entry->Deserialize(nullptr, data);
  return entry;
}

TEST(ServiceTransferCacheTest, EnforcesOnPurgeMemory) {
  ServiceTransferCache cache;
  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;

  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);
  cache.PurgeMemory(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(cache.cache_size_for_testing(), 0u);
}

TEST(ServiceTransferCache, MultipleDecoderUse) {
  ServiceTransferCache cache;
  const uint32_t entry_id = 0u;
  const size_t entry_size = 1024u;

  // Decoder 1 entry.
  int decoder1 = 1;
  auto decoder_1_entry = CreateEntry(entry_size);
  auto* decoder_1_entry_ptr = decoder_1_entry.get();
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(decoder1, kEntryType, entry_id),
      std::move(decoder_1_entry));

  // Decoder 2 entry.
  int decoder2 = 2;
  auto decoder_2_entry = CreateEntry(entry_size);
  auto* decoder_2_entry_ptr = decoder_2_entry.get();
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(decoder2, kEntryType, entry_id),
      std::move(decoder_2_entry));

  EXPECT_EQ(decoder_1_entry_ptr, cache.GetEntry(ServiceTransferCache::EntryKey(
                                     decoder1, kEntryType, entry_id)));
  EXPECT_EQ(decoder_2_entry_ptr, cache.GetEntry(ServiceTransferCache::EntryKey(
                                     decoder2, kEntryType, entry_id)));
}

}  // namespace
}  // namespace gpu
