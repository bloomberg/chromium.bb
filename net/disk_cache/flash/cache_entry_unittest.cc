// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/flash/cache_entry.h"
#include "net/disk_cache/flash/flash_cache_test_base.h"
#include "net/disk_cache/flash/format.h"
#include "testing/gtest/include/gtest/gtest.h"

using disk_cache::CacheEntry;

// Tests the behavior of a CacheEntry with empty streams.
TEST_F(FlashCacheTest, CacheEntryEmpty) {
  scoped_ptr<CacheEntry> entry(new CacheEntry(log_structured_store_.get()));
  EXPECT_TRUE(entry->Init());
  EXPECT_TRUE(entry->Close());

  entry.reset(new CacheEntry(log_structured_store_.get(), entry->id()));
  EXPECT_TRUE(entry->Init());

  for (int i = 0; i < disk_cache::kFlashCacheEntryNumStreams; ++i) {
    const int kSize = 1024;
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
    EXPECT_EQ(0, entry->GetDataSize(i));
    EXPECT_EQ(0, entry->ReadData(i, 0, buf, kSize));
  }
  EXPECT_TRUE(entry->Close());
}

TEST_F(FlashCacheTest, CacheEntryWriteRead) {
  scoped_ptr<CacheEntry> entry(new CacheEntry(log_structured_store_.get()));
  EXPECT_TRUE(entry->Init());

  int sizes[disk_cache::kFlashCacheEntryNumStreams] = {333, 444, 555, 666};
  scoped_refptr<net::IOBuffer> buffers[disk_cache::kFlashCacheEntryNumStreams];

  for (int i = 0; i < disk_cache::kFlashCacheEntryNumStreams; ++i) {
    buffers[i] = new net::IOBuffer(sizes[i]);
    CacheTestFillBuffer(buffers[i]->data(), sizes[i], false);
    EXPECT_EQ(sizes[i], entry->WriteData(i, 0, buffers[i], sizes[i]));
  }
  EXPECT_TRUE(entry->Close());

  int32 id = entry->id();
  entry.reset(new CacheEntry(log_structured_store_.get(), id));
  EXPECT_TRUE(entry->Init());

  for (int i = 0; i < disk_cache::kFlashCacheEntryNumStreams; ++i) {
    EXPECT_EQ(sizes[i], entry->GetDataSize(i));
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(sizes[i]));
    EXPECT_EQ(sizes[i], entry->ReadData(i, 0, buffer, sizes[i]));
    EXPECT_EQ(0, memcmp(buffers[i]->data(), buffer->data(), sizes[i]));
  }
  EXPECT_TRUE(entry->Close());
  EXPECT_EQ(id, entry->id());
}
