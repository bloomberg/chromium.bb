// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/flash/flash_cache_test_base.h"
#include "net/disk_cache/flash/flash_entry_impl.h"
#include "net/disk_cache/flash/format.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::CompletionCallback;

TEST_F(FlashCacheTest, FlashEntryCreate) {
  base::Thread cache_thread("CacheThread");
  ASSERT_TRUE(cache_thread.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0)));

  const std::string key = "foo.com";
  disk_cache::FlashEntryImpl* entry =
      new disk_cache::FlashEntryImpl(key,
                                     log_store_.get(),
                                     cache_thread.message_loop_proxy());
  entry->AddRef();
  EXPECT_EQ(net::OK, entry->Init(CompletionCallback()));
  EXPECT_EQ(key, entry->GetKey());
  EXPECT_EQ(0, entry->GetDataSize(0));
  EXPECT_EQ(0, entry->GetDataSize(1));
  EXPECT_EQ(0, entry->GetDataSize(2));

  const int kSize1 = 100;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  EXPECT_EQ(0, entry->ReadData(0, 0, buffer1, kSize1, CompletionCallback()));
  base::strlcpy(buffer1->data(), "the data", kSize1);
  EXPECT_EQ(kSize1, entry->WriteData(0, 0, buffer1, kSize1,
                                     CompletionCallback(), false));
  memset(buffer1->data(), 0, kSize1);
  EXPECT_EQ(kSize1, entry->ReadData(0, 0, buffer1, kSize1,
                                    CompletionCallback()));
  EXPECT_STREQ("the data", buffer1->data());
  entry->Close();
}
