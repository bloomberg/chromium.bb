// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/timer.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/entry_impl.h"
#include "net/disk_cache/mem_entry_impl.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_entry_impl.h"
#include "net/disk_cache/simple/simple_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

// Tests that can run with different types of caches.
class DiskCacheEntryTest : public DiskCacheTestWithCache {
 public:
  void InternalSyncIOBackground(disk_cache::Entry* entry);
  void ExternalSyncIOBackground(disk_cache::Entry* entry);

 protected:
  void InternalSyncIO();
  void InternalAsyncIO();
  void ExternalSyncIO();
  void ExternalAsyncIO();
  void ReleaseBuffer();
  void StreamAccess();
  void GetKey();
  void GetTimes();
  void GrowData();
  void TruncateData();
  void ZeroLengthIO();
  void Buffering();
  void SizeAtCreate();
  void SizeChanges();
  void ReuseEntry(int size);
  void InvalidData();
  void ReadWriteDestroyBuffer();
  void DoomNormalEntry();
  void DoomEntryNextToOpenEntry();
  void DoomedEntry();
  void BasicSparseIO();
  void HugeSparseIO();
  void GetAvailableRange();
  void CouldBeSparse();
  void UpdateSparseEntry();
  void DoomSparseEntry();
  void PartialSparseEntry();
  void SimpleCacheMakeBadChecksumEntry(const char* key);
};

// This part of the test runs on the background thread.
void DiskCacheEntryTest::InternalSyncIOBackground(disk_cache::Entry* entry) {
  const int kSize1 = 10;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  EXPECT_EQ(0, entry->ReadData(
      0, 0, buffer1, kSize1, net::CompletionCallback()));
  base::strlcpy(buffer1->data(), "the data", kSize1);
  EXPECT_EQ(10, entry->WriteData(
      0, 0, buffer1, kSize1, net::CompletionCallback(), false));
  memset(buffer1->data(), 0, kSize1);
  EXPECT_EQ(10, entry->ReadData(
      0, 0, buffer1, kSize1, net::CompletionCallback()));
  EXPECT_STREQ("the data", buffer1->data());

  const int kSize2 = 5000;
  const int kSize3 = 10000;
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize2));
  scoped_refptr<net::IOBuffer> buffer3(new net::IOBuffer(kSize3));
  memset(buffer3->data(), 0, kSize3);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  EXPECT_EQ(5000, entry->WriteData(
      1, 1500, buffer2, kSize2, net::CompletionCallback(), false));
  memset(buffer2->data(), 0, kSize2);
  EXPECT_EQ(4989, entry->ReadData(
      1, 1511, buffer2, kSize2, net::CompletionCallback()));
  EXPECT_STREQ("big data goes here", buffer2->data());
  EXPECT_EQ(5000, entry->ReadData(
      1, 0, buffer2, kSize2, net::CompletionCallback()));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer3->data(), 1500));
  EXPECT_EQ(1500, entry->ReadData(
      1, 5000, buffer2, kSize2, net::CompletionCallback()));

  EXPECT_EQ(0, entry->ReadData(
      1, 6500, buffer2, kSize2, net::CompletionCallback()));
  EXPECT_EQ(6500, entry->ReadData(
      1, 0, buffer3, kSize3, net::CompletionCallback()));
  EXPECT_EQ(8192, entry->WriteData(
      1, 0, buffer3, 8192, net::CompletionCallback(), false));
  EXPECT_EQ(8192, entry->ReadData(
      1, 0, buffer3, kSize3, net::CompletionCallback()));
  EXPECT_EQ(8192, entry->GetDataSize(1));

  // We need to delete the memory buffer on this thread.
  EXPECT_EQ(0, entry->WriteData(
      0, 0, NULL, 0, net::CompletionCallback(), true));
  EXPECT_EQ(0, entry->WriteData(
      1, 0, NULL, 0, net::CompletionCallback(), true));
}

// We need to support synchronous IO even though it is not a supported operation
// from the point of view of the disk cache's public interface, because we use
// it internally, not just by a few tests, but as part of the implementation
// (see sparse_control.cc, for example).
void DiskCacheEntryTest::InternalSyncIO() {
  disk_cache::Entry* entry = NULL;
  ASSERT_EQ(net::OK, CreateEntry("the first key", &entry));
  ASSERT_TRUE(NULL != entry);

  // The bulk of the test runs from within the callback, on the cache thread.
  RunTaskForTest(base::Bind(&DiskCacheEntryTest::InternalSyncIOBackground,
                            base::Unretained(this),
                            entry));


  entry->Doom();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, InternalSyncIO) {
  InitCache();
  InternalSyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyInternalSyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  InternalSyncIO();
}

void DiskCacheEntryTest::InternalAsyncIO() {
  disk_cache::Entry* entry = NULL;
  ASSERT_EQ(net::OK, CreateEntry("the first key", &entry));
  ASSERT_TRUE(NULL != entry);

  // Avoid using internal buffers for the test. We have to write something to
  // the entry and close it so that we flush the internal buffer to disk. After
  // that, IO operations will be really hitting the disk. We don't care about
  // the content, so just extending the entry is enough (all extensions zero-
  // fill any holes).
  EXPECT_EQ(0, WriteData(entry, 0, 15 * 1024, NULL, 0, false));
  EXPECT_EQ(0, WriteData(entry, 1, 15 * 1024, NULL, 0, false));
  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry("the first key", &entry));

  MessageLoopHelper helper;
  // Let's verify that each IO goes to the right callback object.
  CallbackTest callback1(&helper, false);
  CallbackTest callback2(&helper, false);
  CallbackTest callback3(&helper, false);
  CallbackTest callback4(&helper, false);
  CallbackTest callback5(&helper, false);
  CallbackTest callback6(&helper, false);
  CallbackTest callback7(&helper, false);
  CallbackTest callback8(&helper, false);
  CallbackTest callback9(&helper, false);
  CallbackTest callback10(&helper, false);
  CallbackTest callback11(&helper, false);
  CallbackTest callback12(&helper, false);
  CallbackTest callback13(&helper, false);

  const int kSize1 = 10;
  const int kSize2 = 5000;
  const int kSize3 = 10000;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize2));
  scoped_refptr<net::IOBuffer> buffer3(new net::IOBuffer(kSize3));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  CacheTestFillBuffer(buffer3->data(), kSize3, false);

  EXPECT_EQ(0, entry->ReadData(
      0, 15 * 1024, buffer1, kSize1,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback1))));
  base::strlcpy(buffer1->data(), "the data", kSize1);
  int expected = 0;
  int ret = entry->WriteData(
      0, 0, buffer1, kSize1,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback2)), false);
  EXPECT_TRUE(10 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  memset(buffer2->data(), 0, kSize2);
  ret = entry->ReadData(
      0, 0, buffer2, kSize1,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback3)));
  EXPECT_TRUE(10 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("the data", buffer2->data());

  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  ret = entry->WriteData(
      1, 1500, buffer2, kSize2,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback4)), true);
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  memset(buffer3->data(), 0, kSize3);
  ret = entry->ReadData(
      1, 1511, buffer3, kSize2,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback5)));
  EXPECT_TRUE(4989 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("big data goes here", buffer3->data());
  ret = entry->ReadData(
      1, 0, buffer2, kSize2,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback6)));
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  memset(buffer3->data(), 0, kSize3);

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer3->data(), 1500));
  ret = entry->ReadData(
      1, 5000, buffer2, kSize2,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback7)));
  EXPECT_TRUE(1500 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry->ReadData(
      1, 0, buffer3, kSize3,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback9)));
  EXPECT_TRUE(6500 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry->WriteData(
      1, 0, buffer3, 8192,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback10)), true);
  EXPECT_TRUE(8192 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  ret = entry->ReadData(
      1, 0, buffer3, kSize3,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback11)));
  EXPECT_TRUE(8192 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_EQ(8192, entry->GetDataSize(1));

  ret = entry->ReadData(
      0, 0, buffer1, kSize1,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback12)));
  EXPECT_TRUE(10 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry->ReadData(
      1, 0, buffer2, kSize2,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback13)));
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));

  EXPECT_FALSE(helper.callback_reused_error());

  entry->Doom();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, InternalAsyncIO) {
  InitCache();
  InternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyInternalAsyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  InternalAsyncIO();
}

// This part of the test runs on the background thread.
void DiskCacheEntryTest::ExternalSyncIOBackground(disk_cache::Entry* entry) {
  const int kSize1 = 17000;
  const int kSize2 = 25000;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize2));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  base::strlcpy(buffer1->data(), "the data", kSize1);
  EXPECT_EQ(17000, entry->WriteData(
      0, 0, buffer1, kSize1, net::CompletionCallback(), false));
  memset(buffer1->data(), 0, kSize1);
  EXPECT_EQ(17000, entry->ReadData(
      0, 0, buffer1, kSize1, net::CompletionCallback()));
  EXPECT_STREQ("the data", buffer1->data());

  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  EXPECT_EQ(25000, entry->WriteData(
      1, 10000, buffer2, kSize2, net::CompletionCallback(), false));
  memset(buffer2->data(), 0, kSize2);
  EXPECT_EQ(24989, entry->ReadData(
      1, 10011, buffer2, kSize2, net::CompletionCallback()));
  EXPECT_STREQ("big data goes here", buffer2->data());
  EXPECT_EQ(25000, entry->ReadData(
      1, 0, buffer2, kSize2, net::CompletionCallback()));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer2->data(), 10000));
  EXPECT_EQ(5000, entry->ReadData(
      1, 30000, buffer2, kSize2, net::CompletionCallback()));

  EXPECT_EQ(0, entry->ReadData(
      1, 35000, buffer2, kSize2, net::CompletionCallback()));
  EXPECT_EQ(17000, entry->ReadData(
      1, 0, buffer1, kSize1, net::CompletionCallback()));
  EXPECT_EQ(17000, entry->WriteData(
      1, 20000, buffer1, kSize1, net::CompletionCallback(), false));
  EXPECT_EQ(37000, entry->GetDataSize(1));

  // We need to delete the memory buffer on this thread.
  EXPECT_EQ(0, entry->WriteData(
      0, 0, NULL, 0, net::CompletionCallback(), true));
  EXPECT_EQ(0, entry->WriteData(
      1, 0, NULL, 0, net::CompletionCallback(), true));
}

void DiskCacheEntryTest::ExternalSyncIO() {
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry("the first key", &entry));

  // The bulk of the test runs from within the callback, on the cache thread.
  RunTaskForTest(base::Bind(&DiskCacheEntryTest::ExternalSyncIOBackground,
                            base::Unretained(this),
                            entry));

  entry->Doom();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, ExternalSyncIO) {
  InitCache();
  ExternalSyncIO();
}

TEST_F(DiskCacheEntryTest, ExternalSyncIONoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  ExternalSyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyExternalSyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  ExternalSyncIO();
}

void DiskCacheEntryTest::ExternalAsyncIO() {
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry("the first key", &entry));

  int expected = 0;

  MessageLoopHelper helper;
  // Let's verify that each IO goes to the right callback object.
  CallbackTest callback1(&helper, false);
  CallbackTest callback2(&helper, false);
  CallbackTest callback3(&helper, false);
  CallbackTest callback4(&helper, false);
  CallbackTest callback5(&helper, false);
  CallbackTest callback6(&helper, false);
  CallbackTest callback7(&helper, false);
  CallbackTest callback8(&helper, false);
  CallbackTest callback9(&helper, false);

  const int kSize1 = 17000;
  const int kSize2 = 25000;
  const int kSize3 = 25000;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize2));
  scoped_refptr<net::IOBuffer> buffer3(new net::IOBuffer(kSize3));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  CacheTestFillBuffer(buffer3->data(), kSize3, false);
  base::strlcpy(buffer1->data(), "the data", kSize1);
  int ret = entry->WriteData(
      0, 0, buffer1, kSize1,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback1)), false);
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));

  memset(buffer2->data(), 0, kSize1);
  ret = entry->ReadData(
      0, 0, buffer2, kSize1,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback2)));
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("the data", buffer2->data());

  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  ret = entry->WriteData(
      1, 10000, buffer2, kSize2,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback3)), false);
  EXPECT_TRUE(25000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));

  memset(buffer3->data(), 0, kSize3);
  ret = entry->ReadData(
      1, 10011, buffer3, kSize3,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback4)));
  EXPECT_TRUE(24989 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("big data goes here", buffer3->data());
  ret = entry->ReadData(
      1, 0, buffer2, kSize2,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback5)));
  EXPECT_TRUE(25000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  memset(buffer3->data(), 0, kSize3);
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer3->data(), 10000));
  ret = entry->ReadData(
      1, 30000, buffer2, kSize2,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback6)));
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_EQ(0, entry->ReadData(
      1, 35000, buffer2, kSize2,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback7))));
  ret = entry->ReadData(
      1, 0, buffer1, kSize1,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback8)));
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;
  ret = entry->WriteData(
      1, 20000, buffer3, kSize1,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback9)), false);
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(37000, entry->GetDataSize(1));

  EXPECT_FALSE(helper.callback_reused_error());

  entry->Doom();
  entry->Close();
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, ExternalAsyncIO) {
  InitCache();
  ExternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, ExternalAsyncIONoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  ExternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyExternalAsyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  ExternalAsyncIO();
}

// Tests that IOBuffers are not referenced after IO completes.
void DiskCacheEntryTest::ReleaseBuffer() {
  disk_cache::Entry* entry = NULL;
  ASSERT_EQ(net::OK, CreateEntry("the first key", &entry));
  ASSERT_TRUE(NULL != entry);

  const int kBufferSize = 1024;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
  CacheTestFillBuffer(buffer->data(), kBufferSize, false);

  net::ReleaseBufferCompletionCallback cb(buffer);
  int rv = entry->WriteData(0, 0, buffer, kBufferSize, cb.callback(), false);
  EXPECT_EQ(kBufferSize, cb.GetResult(rv));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, ReleaseBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  ReleaseBuffer();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyReleaseBuffer) {
  SetMemoryOnlyMode();
  InitCache();
  ReleaseBuffer();
}

void DiskCacheEntryTest::StreamAccess() {
  disk_cache::Entry* entry = NULL;
  ASSERT_EQ(net::OK, CreateEntry("the first key", &entry));
  ASSERT_TRUE(NULL != entry);

  const int kBufferSize = 1024;
  const int kNumStreams = 3;
  scoped_refptr<net::IOBuffer> reference_buffers[kNumStreams];
  for (int i = 0; i < kNumStreams; i++) {
    reference_buffers[i] = new net::IOBuffer(kBufferSize);
    CacheTestFillBuffer(reference_buffers[i]->data(), kBufferSize, false);
  }
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kBufferSize));
  for (int i = 0; i < kNumStreams; i++) {
    EXPECT_EQ(kBufferSize, WriteData(entry, i, 0, reference_buffers[i],
                                     kBufferSize, false));
    memset(buffer1->data(), 0, kBufferSize);
    EXPECT_EQ(kBufferSize, ReadData(entry, i, 0, buffer1, kBufferSize));
    EXPECT_EQ(0, memcmp(reference_buffers[i]->data(), buffer1->data(),
                        kBufferSize));
  }
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            ReadData(entry, kNumStreams, 0, buffer1, kBufferSize));
  entry->Close();

  // Open the entry and read it in chunks, including a read past the end.
  ASSERT_EQ(net::OK, OpenEntry("the first key", &entry));
  ASSERT_TRUE(NULL != entry);
  const int kReadBufferSize = 600;
  const int kFinalReadSize = kBufferSize - kReadBufferSize;
  COMPILE_ASSERT(kFinalReadSize < kReadBufferSize, should_be_exactly_two_reads);
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kReadBufferSize));
  for (int i = 0; i < kNumStreams; i++) {
    memset(buffer2->data(), 0, kReadBufferSize);
    EXPECT_EQ(kReadBufferSize, ReadData(entry, i, 0, buffer2, kReadBufferSize));
    EXPECT_EQ(0, memcmp(reference_buffers[i]->data(), buffer2->data(),
                        kReadBufferSize));

    memset(buffer2->data(), 0, kReadBufferSize);
    EXPECT_EQ(kFinalReadSize, ReadData(entry, i, kReadBufferSize,
                                       buffer2, kReadBufferSize));
    EXPECT_EQ(0, memcmp(reference_buffers[i]->data() + kReadBufferSize,
                        buffer2->data(), kFinalReadSize));
  }

  entry->Close();
}

TEST_F(DiskCacheEntryTest, StreamAccess) {
  InitCache();
  StreamAccess();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyStreamAccess) {
  SetMemoryOnlyMode();
  InitCache();
  StreamAccess();
}

void DiskCacheEntryTest::GetKey() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  EXPECT_EQ(key, entry->GetKey()) << "short key";
  entry->Close();

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);
  char key_buffer[20000];

  CacheTestFillBuffer(key_buffer, 3000, true);
  key_buffer[1000] = '\0';

  key = key_buffer;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  EXPECT_TRUE(key == entry->GetKey()) << "1000 bytes key";
  entry->Close();

  key_buffer[1000] = 'p';
  key_buffer[3000] = '\0';
  key = key_buffer;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  EXPECT_TRUE(key == entry->GetKey()) << "medium size key";
  entry->Close();

  CacheTestFillBuffer(key_buffer, sizeof(key_buffer), true);
  key_buffer[19999] = '\0';

  key = key_buffer;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  EXPECT_TRUE(key == entry->GetKey()) << "long key";
  entry->Close();

  CacheTestFillBuffer(key_buffer, 0x4000, true);
  key_buffer[0x4000] = '\0';

  key = key_buffer;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  EXPECT_TRUE(key == entry->GetKey()) << "16KB key";
  entry->Close();
}

TEST_F(DiskCacheEntryTest, GetKey) {
  InitCache();
  GetKey();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyGetKey) {
  SetMemoryOnlyMode();
  InitCache();
  GetKey();
}

void DiskCacheEntryTest::GetTimes() {
  std::string key("the first key");
  disk_cache::Entry* entry;

  Time t1 = Time::Now();
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  EXPECT_TRUE(entry->GetLastModified() >= t1);
  EXPECT_TRUE(entry->GetLastModified() == entry->GetLastUsed());

  AddDelay();
  Time t2 = Time::Now();
  EXPECT_TRUE(t2 > t1);
  EXPECT_EQ(0, WriteData(entry, 0, 200, NULL, 0, false));
  if (type_ == net::APP_CACHE) {
    EXPECT_TRUE(entry->GetLastModified() < t2);
  } else {
    EXPECT_TRUE(entry->GetLastModified() >= t2);
  }
  EXPECT_TRUE(entry->GetLastModified() == entry->GetLastUsed());

  AddDelay();
  Time t3 = Time::Now();
  EXPECT_TRUE(t3 > t2);
  const int kSize = 200;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  EXPECT_EQ(kSize, ReadData(entry, 0, 0, buffer, kSize));
  if (type_ == net::APP_CACHE) {
    EXPECT_TRUE(entry->GetLastUsed() < t2);
    EXPECT_TRUE(entry->GetLastModified() < t2);
  } else if (type_ == net::SHADER_CACHE) {
    EXPECT_TRUE(entry->GetLastUsed() < t3);
    EXPECT_TRUE(entry->GetLastModified() < t3);
  } else {
    EXPECT_TRUE(entry->GetLastUsed() >= t3);
    EXPECT_TRUE(entry->GetLastModified() < t3);
  }
  entry->Close();
}

TEST_F(DiskCacheEntryTest, GetTimes) {
  InitCache();
  GetTimes();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyGetTimes) {
  SetMemoryOnlyMode();
  InitCache();
  GetTimes();
}

TEST_F(DiskCacheEntryTest, AppCacheGetTimes) {
  SetCacheType(net::APP_CACHE);
  InitCache();
  GetTimes();
}

TEST_F(DiskCacheEntryTest, ShaderCacheGetTimes) {
  SetCacheType(net::SHADER_CACHE);
  InitCache();
  GetTimes();
}

void DiskCacheEntryTest::GrowData() {
  std::string key1("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key1, &entry));

  const int kSize = 20000;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  memset(buffer2->data(), 0, kSize);

  base::strlcpy(buffer1->data(), "the data", kSize);
  EXPECT_EQ(10, WriteData(entry, 0, 0, buffer1, 10, false));
  EXPECT_EQ(10, ReadData(entry, 0, 0, buffer2, 10));
  EXPECT_STREQ("the data", buffer2->data());
  EXPECT_EQ(10, entry->GetDataSize(0));

  EXPECT_EQ(2000, WriteData(entry, 0, 0, buffer1, 2000, false));
  EXPECT_EQ(2000, entry->GetDataSize(0));
  EXPECT_EQ(2000, ReadData(entry, 0, 0, buffer2, 2000));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 2000));

  EXPECT_EQ(20000, WriteData(entry, 0, 0, buffer1, kSize, false));
  EXPECT_EQ(20000, entry->GetDataSize(0));
  EXPECT_EQ(20000, ReadData(entry, 0, 0, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), kSize));
  entry->Close();

  memset(buffer2->data(), 0, kSize);
  std::string key2("Second key");
  ASSERT_EQ(net::OK, CreateEntry(key2, &entry));
  EXPECT_EQ(10, WriteData(entry, 0, 0, buffer1, 10, false));
  EXPECT_EQ(10, entry->GetDataSize(0));
  entry->Close();

  // Go from an internal address to a bigger block size.
  ASSERT_EQ(net::OK, OpenEntry(key2, &entry));
  EXPECT_EQ(2000, WriteData(entry, 0, 0, buffer1, 2000, false));
  EXPECT_EQ(2000, entry->GetDataSize(0));
  EXPECT_EQ(2000, ReadData(entry, 0, 0, buffer2, 2000));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 2000));
  entry->Close();
  memset(buffer2->data(), 0, kSize);

  // Go from an internal address to an external one.
  ASSERT_EQ(net::OK, OpenEntry(key2, &entry));
  EXPECT_EQ(20000, WriteData(entry, 0, 0, buffer1, kSize, false));
  EXPECT_EQ(20000, entry->GetDataSize(0));
  EXPECT_EQ(20000, ReadData(entry, 0, 0, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), kSize));
  entry->Close();

  // Double check the size from disk.
  ASSERT_EQ(net::OK, OpenEntry(key2, &entry));
  EXPECT_EQ(20000, entry->GetDataSize(0));

  // Now extend the entry without actual data.
  EXPECT_EQ(0, WriteData(entry, 0, 45500, buffer1, 0, false));
  entry->Close();

  // And check again from disk.
  ASSERT_EQ(net::OK, OpenEntry(key2, &entry));
  EXPECT_EQ(45500, entry->GetDataSize(0));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, GrowData) {
  InitCache();
  GrowData();
}

TEST_F(DiskCacheEntryTest, GrowDataNoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  GrowData();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyGrowData) {
  SetMemoryOnlyMode();
  InitCache();
  GrowData();
}

void DiskCacheEntryTest::TruncateData() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize1 = 20000;
  const int kSize2 = 20000;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize2));

  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  memset(buffer2->data(), 0, kSize2);

  // Simple truncation:
  EXPECT_EQ(200, WriteData(entry, 0, 0, buffer1, 200, false));
  EXPECT_EQ(200, entry->GetDataSize(0));
  EXPECT_EQ(100, WriteData(entry, 0, 0, buffer1, 100, false));
  EXPECT_EQ(200, entry->GetDataSize(0));
  EXPECT_EQ(100, WriteData(entry, 0, 0, buffer1, 100, true));
  EXPECT_EQ(100, entry->GetDataSize(0));
  EXPECT_EQ(0, WriteData(entry, 0, 50, buffer1, 0, true));
  EXPECT_EQ(50, entry->GetDataSize(0));
  EXPECT_EQ(0, WriteData(entry, 0, 0, buffer1, 0, true));
  EXPECT_EQ(0, entry->GetDataSize(0));
  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));

  // Go to an external file.
  EXPECT_EQ(20000, WriteData(entry, 0, 0, buffer1, 20000, true));
  EXPECT_EQ(20000, entry->GetDataSize(0));
  EXPECT_EQ(20000, ReadData(entry, 0, 0, buffer2, 20000));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 20000));
  memset(buffer2->data(), 0, kSize2);

  // External file truncation
  EXPECT_EQ(18000, WriteData(entry, 0, 0, buffer1, 18000, false));
  EXPECT_EQ(20000, entry->GetDataSize(0));
  EXPECT_EQ(18000, WriteData(entry, 0, 0, buffer1, 18000, true));
  EXPECT_EQ(18000, entry->GetDataSize(0));
  EXPECT_EQ(0, WriteData(entry, 0, 17500, buffer1, 0, true));
  EXPECT_EQ(17500, entry->GetDataSize(0));

  // And back to an internal block.
  EXPECT_EQ(600, WriteData(entry, 0, 1000, buffer1, 600, true));
  EXPECT_EQ(1600, entry->GetDataSize(0));
  EXPECT_EQ(600, ReadData(entry, 0, 1000, buffer2, 600));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 600));
  EXPECT_EQ(1000, ReadData(entry, 0, 0, buffer2, 1000));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 1000)) <<
      "Preserves previous data";

  // Go from external file to zero length.
  EXPECT_EQ(20000, WriteData(entry, 0, 0, buffer1, 20000, true));
  EXPECT_EQ(20000, entry->GetDataSize(0));
  EXPECT_EQ(0, WriteData(entry, 0, 0, buffer1, 0, true));
  EXPECT_EQ(0, entry->GetDataSize(0));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, TruncateData) {
  InitCache();
  TruncateData();
}

TEST_F(DiskCacheEntryTest, TruncateDataNoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  TruncateData();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyTruncateData) {
  SetMemoryOnlyMode();
  InitCache();
  TruncateData();
}

void DiskCacheEntryTest::ZeroLengthIO() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  EXPECT_EQ(0, ReadData(entry, 0, 0, NULL, 0));
  EXPECT_EQ(0, WriteData(entry, 0, 0, NULL, 0, false));

  // This write should extend the entry.
  EXPECT_EQ(0, WriteData(entry, 0, 1000, NULL, 0, false));
  EXPECT_EQ(0, ReadData(entry, 0, 500, NULL, 0));
  EXPECT_EQ(0, ReadData(entry, 0, 2000, NULL, 0));
  EXPECT_EQ(1000, entry->GetDataSize(0));

  EXPECT_EQ(0, WriteData(entry, 0, 100000, NULL, 0, true));
  EXPECT_EQ(0, ReadData(entry, 0, 50000, NULL, 0));
  EXPECT_EQ(100000, entry->GetDataSize(0));

  // Let's verify the actual content.
  const int kSize = 20;
  const char zeros[kSize] = {};
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));

  CacheTestFillBuffer(buffer->data(), kSize, false);
  EXPECT_EQ(kSize, ReadData(entry, 0, 500, buffer, kSize));
  EXPECT_TRUE(!memcmp(buffer->data(), zeros, kSize));

  CacheTestFillBuffer(buffer->data(), kSize, false);
  EXPECT_EQ(kSize, ReadData(entry, 0, 5000, buffer, kSize));
  EXPECT_TRUE(!memcmp(buffer->data(), zeros, kSize));

  CacheTestFillBuffer(buffer->data(), kSize, false);
  EXPECT_EQ(kSize, ReadData(entry, 0, 50000, buffer, kSize));
  EXPECT_TRUE(!memcmp(buffer->data(), zeros, kSize));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, ZeroLengthIO) {
  InitCache();
  ZeroLengthIO();
}

TEST_F(DiskCacheEntryTest, ZeroLengthIONoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  ZeroLengthIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyZeroLengthIO) {
  SetMemoryOnlyMode();
  InitCache();
  ZeroLengthIO();
}

// Tests that we handle the content correctly when buffering.
void DiskCacheEntryTest::Buffering() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 200;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buffer1->data(), kSize, true);
  CacheTestFillBuffer(buffer2->data(), kSize, true);

  EXPECT_EQ(kSize, WriteData(entry, 1, 0, buffer1, kSize, false));
  entry->Close();

  // Write a little more and read what we wrote before.
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  EXPECT_EQ(kSize, WriteData(entry, 1, 5000, buffer1, kSize, false));
  EXPECT_EQ(kSize, ReadData(entry, 1, 0, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));

  // Now go to an external file.
  EXPECT_EQ(kSize, WriteData(entry, 1, 18000, buffer1, kSize, false));
  entry->Close();

  // Write something else and verify old data.
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  EXPECT_EQ(kSize, WriteData(entry, 1, 10000, buffer1, kSize, false));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 5000, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 0, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 18000, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));

  // Extend the file some more.
  EXPECT_EQ(kSize, WriteData(entry, 1, 23000, buffer1, kSize, false));
  entry->Close();

  // And now make sure that we can deal with data in both places (ram/disk).
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  EXPECT_EQ(kSize, WriteData(entry, 1, 17000, buffer1, kSize, false));

  // We should not overwrite the data at 18000 with this.
  EXPECT_EQ(kSize, WriteData(entry, 1, 19000, buffer1, kSize, false));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 18000, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 17000, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));

  EXPECT_EQ(kSize, WriteData(entry, 1, 22900, buffer1, kSize, false));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(100, ReadData(entry, 1, 23000, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data() + 100, 100));

  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(100, ReadData(entry, 1, 23100, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data() + 100, 100));

  // Extend the file again and read before without closing the entry.
  EXPECT_EQ(kSize, WriteData(entry, 1, 25000, buffer1, kSize, false));
  EXPECT_EQ(kSize, WriteData(entry, 1, 45000, buffer1, kSize, false));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 25000, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 45000, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data(), kSize));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, Buffering) {
  InitCache();
  Buffering();
}

TEST_F(DiskCacheEntryTest, BufferingNoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  Buffering();
}

// Checks that entries are zero length when created.
void DiskCacheEntryTest::SizeAtCreate() {
  const char key[]  = "the first key";
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kNumStreams = 3;
  for (int i = 0; i < kNumStreams; ++i)
    EXPECT_EQ(0, entry->GetDataSize(i));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, SizeAtCreate) {
  InitCache();
  SizeAtCreate();
}

TEST_F(DiskCacheEntryTest, MemoryOnlySizeAtCreate) {
  SetMemoryOnlyMode();
  InitCache();
  SizeAtCreate();
}

// Some extra tests to make sure that buffering works properly when changing
// the entry size.
void DiskCacheEntryTest::SizeChanges() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 200;
  const char zeros[kSize] = {};
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buffer1->data(), kSize, true);
  CacheTestFillBuffer(buffer2->data(), kSize, true);

  EXPECT_EQ(kSize, WriteData(entry, 1, 0, buffer1, kSize, true));
  EXPECT_EQ(kSize, WriteData(entry, 1, 17000, buffer1, kSize, true));
  EXPECT_EQ(kSize, WriteData(entry, 1, 23000, buffer1, kSize, true));
  entry->Close();

  // Extend the file and read between the old size and the new write.
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  EXPECT_EQ(23000 + kSize, entry->GetDataSize(1));
  EXPECT_EQ(kSize, WriteData(entry, 1, 25000, buffer1, kSize, true));
  EXPECT_EQ(25000 + kSize, entry->GetDataSize(1));
  EXPECT_EQ(kSize, ReadData(entry, 1, 24000, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), zeros, kSize));

  // Read at the end of the old file size.
  EXPECT_EQ(kSize, ReadData(entry, 1, 23000 + kSize - 35, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data() + kSize - 35, 35));

  // Read slightly before the last write.
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 24900, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), zeros, 100));
  EXPECT_TRUE(!memcmp(buffer2->data() + 100, buffer1->data(), kSize - 100));

  // Extend the entry a little more.
  EXPECT_EQ(kSize, WriteData(entry, 1, 26000, buffer1, kSize, true));
  EXPECT_EQ(26000 + kSize, entry->GetDataSize(1));
  CacheTestFillBuffer(buffer2->data(), kSize, true);
  EXPECT_EQ(kSize, ReadData(entry, 1, 25900, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), zeros, 100));
  EXPECT_TRUE(!memcmp(buffer2->data() + 100, buffer1->data(), kSize - 100));

  // And now reduce the size.
  EXPECT_EQ(kSize, WriteData(entry, 1, 25000, buffer1, kSize, true));
  EXPECT_EQ(25000 + kSize, entry->GetDataSize(1));
  EXPECT_EQ(28, ReadData(entry, 1, 25000 + kSize - 28, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), buffer1->data() + kSize - 28, 28));

  // Reduce the size with a buffer that is not extending the size.
  EXPECT_EQ(kSize, WriteData(entry, 1, 24000, buffer1, kSize, false));
  EXPECT_EQ(25000 + kSize, entry->GetDataSize(1));
  EXPECT_EQ(kSize, WriteData(entry, 1, 24500, buffer1, kSize, true));
  EXPECT_EQ(24500 + kSize, entry->GetDataSize(1));
  EXPECT_EQ(kSize, ReadData(entry, 1, 23900, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), zeros, 100));
  EXPECT_TRUE(!memcmp(buffer2->data() + 100, buffer1->data(), kSize - 100));

  // And now reduce the size below the old size.
  EXPECT_EQ(kSize, WriteData(entry, 1, 19000, buffer1, kSize, true));
  EXPECT_EQ(19000 + kSize, entry->GetDataSize(1));
  EXPECT_EQ(kSize, ReadData(entry, 1, 18900, buffer2, kSize));
  EXPECT_TRUE(!memcmp(buffer2->data(), zeros, 100));
  EXPECT_TRUE(!memcmp(buffer2->data() + 100, buffer1->data(), kSize - 100));

  // Verify that the actual file is truncated.
  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  EXPECT_EQ(19000 + kSize, entry->GetDataSize(1));

  // Extend the newly opened file with a zero length write, expect zero fill.
  EXPECT_EQ(0, WriteData(entry, 1, 20000 + kSize, buffer1, 0, false));
  EXPECT_EQ(kSize, ReadData(entry, 1, 19000 + kSize, buffer1, kSize));
  EXPECT_EQ(0, memcmp(buffer1->data(), zeros, kSize));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, SizeChanges) {
  InitCache();
  SizeChanges();
}

TEST_F(DiskCacheEntryTest, SizeChangesNoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  SizeChanges();
}

// Write more than the total cache capacity but to a single entry. |size| is the
// amount of bytes to write each time.
void DiskCacheEntryTest::ReuseEntry(int size) {
  std::string key1("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key1, &entry));

  entry->Close();
  std::string key2("the second key");
  ASSERT_EQ(net::OK, CreateEntry(key2, &entry));

  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(size));
  CacheTestFillBuffer(buffer->data(), size, false);

  for (int i = 0; i < 15; i++) {
    EXPECT_EQ(0, WriteData(entry, 0, 0, buffer, 0, true));
    EXPECT_EQ(size, WriteData(entry, 0, 0, buffer, size, false));
    entry->Close();
    ASSERT_EQ(net::OK, OpenEntry(key2, &entry));
  }

  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry(key1, &entry)) << "have not evicted this entry";
  entry->Close();
}

TEST_F(DiskCacheEntryTest, ReuseExternalEntry) {
  SetMaxSize(200 * 1024);
  InitCache();
  ReuseEntry(20 * 1024);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyReuseExternalEntry) {
  SetMemoryOnlyMode();
  SetMaxSize(200 * 1024);
  InitCache();
  ReuseEntry(20 * 1024);
}

TEST_F(DiskCacheEntryTest, ReuseInternalEntry) {
  SetMaxSize(100 * 1024);
  InitCache();
  ReuseEntry(10 * 1024);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyReuseInternalEntry) {
  SetMemoryOnlyMode();
  SetMaxSize(100 * 1024);
  InitCache();
  ReuseEntry(10 * 1024);
}

// Reading somewhere that was not written should return zeros.
void DiskCacheEntryTest::InvalidData() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize1 = 20000;
  const int kSize2 = 20000;
  const int kSize3 = 20000;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize2));
  scoped_refptr<net::IOBuffer> buffer3(new net::IOBuffer(kSize3));

  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  memset(buffer2->data(), 0, kSize2);

  // Simple data grow:
  EXPECT_EQ(200, WriteData(entry, 0, 400, buffer1, 200, false));
  EXPECT_EQ(600, entry->GetDataSize(0));
  EXPECT_EQ(100, ReadData(entry, 0, 300, buffer3, 100));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 100));
  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));

  // The entry is now on disk. Load it and extend it.
  EXPECT_EQ(200, WriteData(entry, 0, 800, buffer1, 200, false));
  EXPECT_EQ(1000, entry->GetDataSize(0));
  EXPECT_EQ(100, ReadData(entry, 0, 700, buffer3, 100));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 100));
  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));

  // This time using truncate.
  EXPECT_EQ(200, WriteData(entry, 0, 1800, buffer1, 200, true));
  EXPECT_EQ(2000, entry->GetDataSize(0));
  EXPECT_EQ(100, ReadData(entry, 0, 1500, buffer3, 100));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 100));

  // Go to an external file.
  EXPECT_EQ(200, WriteData(entry, 0, 19800, buffer1, 200, false));
  EXPECT_EQ(20000, entry->GetDataSize(0));
  EXPECT_EQ(4000, ReadData(entry, 0, 14000, buffer3, 4000));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 4000));

  // And back to an internal block.
  EXPECT_EQ(600, WriteData(entry, 0, 1000, buffer1, 600, true));
  EXPECT_EQ(1600, entry->GetDataSize(0));
  EXPECT_EQ(600, ReadData(entry, 0, 1000, buffer3, 600));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer1->data(), 600));

  // Extend it again.
  EXPECT_EQ(600, WriteData(entry, 0, 2000, buffer1, 600, false));
  EXPECT_EQ(2600, entry->GetDataSize(0));
  EXPECT_EQ(200, ReadData(entry, 0, 1800, buffer3, 200));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 200));

  // And again (with truncation flag).
  EXPECT_EQ(600, WriteData(entry, 0, 3000, buffer1, 600, true));
  EXPECT_EQ(3600, entry->GetDataSize(0));
  EXPECT_EQ(200, ReadData(entry, 0, 2800, buffer3, 200));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 200));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, InvalidData) {
  InitCache();
  InvalidData();
}

TEST_F(DiskCacheEntryTest, InvalidDataNoBuffer) {
  InitCache();
  cache_impl_->SetFlags(disk_cache::kNoBuffering);
  InvalidData();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyInvalidData) {
  SetMemoryOnlyMode();
  InitCache();
  InvalidData();
}

// Tests that the cache preserves the buffer of an IO operation.
void DiskCacheEntryTest::ReadWriteDestroyBuffer() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 200;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buffer->data(), kSize, false);

  net::TestCompletionCallback cb;
  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->WriteData(0, 0, buffer, kSize, cb.callback(), false));

  // Release our reference to the buffer.
  buffer = NULL;
  EXPECT_EQ(kSize, cb.WaitForResult());

  // And now test with a Read().
  buffer = new net::IOBuffer(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, false);

  EXPECT_EQ(net::ERR_IO_PENDING,
            entry->ReadData(0, 0, buffer, kSize, cb.callback()));
  buffer = NULL;
  EXPECT_EQ(kSize, cb.WaitForResult());

  entry->Close();
}

TEST_F(DiskCacheEntryTest, ReadWriteDestroyBuffer) {
  InitCache();
  ReadWriteDestroyBuffer();
}

void DiskCacheEntryTest::DoomNormalEntry() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  entry->Doom();
  entry->Close();

  const int kSize = 20000;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buffer->data(), kSize, true);
  buffer->data()[19999] = '\0';

  key = buffer->data();
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  EXPECT_EQ(20000, WriteData(entry, 0, 0, buffer, kSize, false));
  EXPECT_EQ(20000, WriteData(entry, 1, 0, buffer, kSize, false));
  entry->Doom();
  entry->Close();

  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, DoomEntry) {
  InitCache();
  DoomNormalEntry();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyDoomEntry) {
  SetMemoryOnlyMode();
  InitCache();
  DoomNormalEntry();
}

// Tests dooming an entry that's linked to an open entry.
void DiskCacheEntryTest::DoomEntryNextToOpenEntry() {
  disk_cache::Entry* entry1;
  disk_cache::Entry* entry2;
  ASSERT_EQ(net::OK, CreateEntry("fixed", &entry1));
  entry1->Close();
  ASSERT_EQ(net::OK, CreateEntry("foo", &entry1));
  entry1->Close();
  ASSERT_EQ(net::OK, CreateEntry("bar", &entry1));
  entry1->Close();

  ASSERT_EQ(net::OK, OpenEntry("foo", &entry1));
  ASSERT_EQ(net::OK, OpenEntry("bar", &entry2));
  entry2->Doom();
  entry2->Close();

  ASSERT_EQ(net::OK, OpenEntry("foo", &entry2));
  entry2->Doom();
  entry2->Close();
  entry1->Close();

  ASSERT_EQ(net::OK, OpenEntry("fixed", &entry1));
  entry1->Close();
}

TEST_F(DiskCacheEntryTest, DoomEntryNextToOpenEntry) {
  InitCache();
  DoomEntryNextToOpenEntry();
}

TEST_F(DiskCacheEntryTest, NewEvictionDoomEntryNextToOpenEntry) {
  SetNewEviction();
  InitCache();
  DoomEntryNextToOpenEntry();
}

TEST_F(DiskCacheEntryTest, AppCacheDoomEntryNextToOpenEntry) {
  SetCacheType(net::APP_CACHE);
  InitCache();
  DoomEntryNextToOpenEntry();
}

// Verify that basic operations work as expected with doomed entries.
void DiskCacheEntryTest::DoomedEntry() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  entry->Doom();

  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
  Time initial = Time::Now();
  AddDelay();

  const int kSize1 = 2000;
  const int kSize2 = 2000;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize2));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  memset(buffer2->data(), 0, kSize2);

  EXPECT_EQ(2000, WriteData(entry, 0, 0, buffer1, 2000, false));
  EXPECT_EQ(2000, ReadData(entry, 0, 0, buffer2, 2000));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer2->data(), kSize1));
  EXPECT_EQ(key, entry->GetKey());
  EXPECT_TRUE(initial < entry->GetLastModified());
  EXPECT_TRUE(initial < entry->GetLastUsed());

  entry->Close();
}

TEST_F(DiskCacheEntryTest, DoomedEntry) {
  InitCache();
  DoomedEntry();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyDoomedEntry) {
  SetMemoryOnlyMode();
  InitCache();
  DoomedEntry();
}

// Tests that we discard entries if the data is missing.
TEST_F(DiskCacheEntryTest, MissingData) {
  InitCache();

  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  // Write to an external file.
  const int kSize = 20000;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buffer->data(), kSize, false);
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer, kSize, false));
  entry->Close();
  FlushQueueForTest();

  disk_cache::Addr address(0x80000001);
  base::FilePath name = cache_impl_->GetFileName(address);
  EXPECT_TRUE(file_util::Delete(name, false));

  // Attempt to read the data.
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, ReadData(entry, 0, 0, buffer, kSize));
  entry->Close();

  // The entry should be gone.
  ASSERT_NE(net::OK, OpenEntry(key, &entry));
}

// Test that child entries in a memory cache backend are not visible from
// enumerations.
TEST_F(DiskCacheEntryTest, MemoryOnlyEnumerationWithSparseEntries) {
  SetMemoryOnlyMode();
  InitCache();

  const int kSize = 4096;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf->data(), kSize, false);

  std::string key("the first key");
  disk_cache::Entry* parent_entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &parent_entry));

  // Writes to the parent entry.
  EXPECT_EQ(kSize, parent_entry->WriteSparseData(0, buf, kSize,
                                                 net::CompletionCallback()));

  // This write creates a child entry and writes to it.
  EXPECT_EQ(kSize, parent_entry->WriteSparseData(8192, buf, kSize,
                                                 net::CompletionCallback()));

  parent_entry->Close();

  // Perform the enumerations.
  void* iter = NULL;
  disk_cache::Entry* entry = NULL;
  int count = 0;
  while (OpenNextEntry(&iter, &entry) == net::OK) {
    ASSERT_TRUE(entry != NULL);
    ++count;
    disk_cache::MemEntryImpl* mem_entry =
        reinterpret_cast<disk_cache::MemEntryImpl*>(entry);
    EXPECT_EQ(disk_cache::MemEntryImpl::kParentEntry, mem_entry->type());
    mem_entry->Close();
  }
  EXPECT_EQ(1, count);
}

// Writes |buf_1| to offset and reads it back as |buf_2|.
void VerifySparseIO(disk_cache::Entry* entry, int64 offset,
                    net::IOBuffer* buf_1, int size, net::IOBuffer* buf_2) {
  net::TestCompletionCallback cb;

  memset(buf_2->data(), 0, size);
  int ret = entry->ReadSparseData(offset, buf_2, size, cb.callback());
  EXPECT_EQ(0, cb.GetResult(ret));

  ret = entry->WriteSparseData(offset, buf_1, size, cb.callback());
  EXPECT_EQ(size, cb.GetResult(ret));

  ret = entry->ReadSparseData(offset, buf_2, size, cb.callback());
  EXPECT_EQ(size, cb.GetResult(ret));

  EXPECT_EQ(0, memcmp(buf_1->data(), buf_2->data(), size));
}

// Reads |size| bytes from |entry| at |offset| and verifies that they are the
// same as the content of the provided |buffer|.
void VerifyContentSparseIO(disk_cache::Entry* entry, int64 offset, char* buffer,
                           int size) {
  net::TestCompletionCallback cb;

  scoped_refptr<net::IOBuffer> buf_1(new net::IOBuffer(size));
  memset(buf_1->data(), 0, size);
  int ret = entry->ReadSparseData(offset, buf_1, size, cb.callback());
  EXPECT_EQ(size, cb.GetResult(ret));
  EXPECT_EQ(0, memcmp(buf_1->data(), buffer, size));
}

void DiskCacheEntryTest::BasicSparseIO() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 2048;
  scoped_refptr<net::IOBuffer> buf_1(new net::IOBuffer(kSize));
  scoped_refptr<net::IOBuffer> buf_2(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  // Write at offset 0.
  VerifySparseIO(entry, 0, buf_1, kSize, buf_2);

  // Write at offset 0x400000 (4 MB).
  VerifySparseIO(entry, 0x400000, buf_1, kSize, buf_2);

  // Write at offset 0x800000000 (32 GB).
  VerifySparseIO(entry, 0x800000000LL, buf_1, kSize, buf_2);

  entry->Close();

  // Check everything again.
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  VerifyContentSparseIO(entry, 0, buf_1->data(), kSize);
  VerifyContentSparseIO(entry, 0x400000, buf_1->data(), kSize);
  VerifyContentSparseIO(entry, 0x800000000LL, buf_1->data(), kSize);
  entry->Close();
}

TEST_F(DiskCacheEntryTest, BasicSparseIO) {
  InitCache();
  BasicSparseIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyBasicSparseIO) {
  SetMemoryOnlyMode();
  InitCache();
  BasicSparseIO();
}

void DiskCacheEntryTest::HugeSparseIO() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  // Write 1.2 MB so that we cover multiple entries.
  const int kSize = 1200 * 1024;
  scoped_refptr<net::IOBuffer> buf_1(new net::IOBuffer(kSize));
  scoped_refptr<net::IOBuffer> buf_2(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  // Write at offset 0x20F0000 (33 MB - 64 KB).
  VerifySparseIO(entry, 0x20F0000, buf_1, kSize, buf_2);
  entry->Close();

  // Check it again.
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  VerifyContentSparseIO(entry, 0x20F0000, buf_1->data(), kSize);
  entry->Close();
}

TEST_F(DiskCacheEntryTest, HugeSparseIO) {
  InitCache();
  HugeSparseIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyHugeSparseIO) {
  SetMemoryOnlyMode();
  InitCache();
  HugeSparseIO();
}

void DiskCacheEntryTest::GetAvailableRange() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 16 * 1024;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf->data(), kSize, false);

  // Write at offset 0x20F0000 (33 MB - 64 KB), and 0x20F4400 (33 MB - 47 KB).
  EXPECT_EQ(kSize, WriteSparseData(entry, 0x20F0000, buf, kSize));
  EXPECT_EQ(kSize, WriteSparseData(entry, 0x20F4400, buf, kSize));

  // We stop at the first empty block.
  int64 start;
  net::TestCompletionCallback cb;
  int rv = entry->GetAvailableRange(
      0x20F0000, kSize * 2, &start, cb.callback());
  EXPECT_EQ(kSize, cb.GetResult(rv));
  EXPECT_EQ(0x20F0000, start);

  start = 0;
  rv = entry->GetAvailableRange(0, kSize, &start, cb.callback());
  EXPECT_EQ(0, cb.GetResult(rv));
  rv = entry->GetAvailableRange(
      0x20F0000 - kSize, kSize, &start, cb.callback());
  EXPECT_EQ(0, cb.GetResult(rv));
  rv = entry->GetAvailableRange(0, 0x2100000, &start, cb.callback());
  EXPECT_EQ(kSize, cb.GetResult(rv));
  EXPECT_EQ(0x20F0000, start);

  // We should be able to Read based on the results of GetAvailableRange.
  start = -1;
  rv = entry->GetAvailableRange(0x2100000, kSize, &start, cb.callback());
  EXPECT_EQ(0, cb.GetResult(rv));
  rv = entry->ReadSparseData(start, buf, kSize, cb.callback());
  EXPECT_EQ(0, cb.GetResult(rv));

  start = 0;
  rv = entry->GetAvailableRange(0x20F2000, kSize, &start, cb.callback());
  EXPECT_EQ(0x2000, cb.GetResult(rv));
  EXPECT_EQ(0x20F2000, start);
  EXPECT_EQ(0x2000, ReadSparseData(entry, start, buf, kSize));

  // Make sure that we respect the |len| argument.
  start = 0;
  rv = entry->GetAvailableRange(
      0x20F0001 - kSize, kSize, &start, cb.callback());
  EXPECT_EQ(1, cb.GetResult(rv));
  EXPECT_EQ(0x20F0000, start);

  entry->Close();
}

TEST_F(DiskCacheEntryTest, GetAvailableRange) {
  InitCache();
  GetAvailableRange();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyGetAvailableRange) {
  SetMemoryOnlyMode();
  InitCache();
  GetAvailableRange();
}

void DiskCacheEntryTest::CouldBeSparse() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 16 * 1024;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf->data(), kSize, false);

  // Write at offset 0x20F0000 (33 MB - 64 KB).
  EXPECT_EQ(kSize, WriteSparseData(entry, 0x20F0000, buf, kSize));

  EXPECT_TRUE(entry->CouldBeSparse());
  entry->Close();

  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  EXPECT_TRUE(entry->CouldBeSparse());
  entry->Close();

  // Now verify a regular entry.
  key.assign("another key");
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  EXPECT_FALSE(entry->CouldBeSparse());

  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buf, kSize, false));
  EXPECT_EQ(kSize, WriteData(entry, 1, 0, buf, kSize, false));
  EXPECT_EQ(kSize, WriteData(entry, 2, 0, buf, kSize, false));

  EXPECT_FALSE(entry->CouldBeSparse());
  entry->Close();

  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  EXPECT_FALSE(entry->CouldBeSparse());
  entry->Close();
}

TEST_F(DiskCacheEntryTest, CouldBeSparse) {
  InitCache();
  CouldBeSparse();
}

TEST_F(DiskCacheEntryTest, MemoryCouldBeSparse) {
  SetMemoryOnlyMode();
  InitCache();
  CouldBeSparse();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyMisalignedSparseIO) {
  SetMemoryOnlyMode();
  InitCache();

  const int kSize = 8192;
  scoped_refptr<net::IOBuffer> buf_1(new net::IOBuffer(kSize));
  scoped_refptr<net::IOBuffer> buf_2(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  // This loop writes back to back starting from offset 0 and 9000.
  for (int i = 0; i < kSize; i += 1024) {
    scoped_refptr<net::WrappedIOBuffer> buf_3(
      new net::WrappedIOBuffer(buf_1->data() + i));
    VerifySparseIO(entry, i, buf_3, 1024, buf_2);
    VerifySparseIO(entry, 9000 + i, buf_3, 1024, buf_2);
  }

  // Make sure we have data written.
  VerifyContentSparseIO(entry, 0, buf_1->data(), kSize);
  VerifyContentSparseIO(entry, 9000, buf_1->data(), kSize);

  // This tests a large write that spans 3 entries from a misaligned offset.
  VerifySparseIO(entry, 20481, buf_1, 8192, buf_2);

  entry->Close();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyMisalignedGetAvailableRange) {
  SetMemoryOnlyMode();
  InitCache();

  const int kSize = 8192;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf->data(), kSize, false);

  disk_cache::Entry* entry;
  std::string key("the first key");
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  // Writes in the middle of an entry.
  EXPECT_EQ(1024, entry->WriteSparseData(
      0, buf, 1024, net::CompletionCallback()));
  EXPECT_EQ(1024, entry->WriteSparseData(
      5120, buf, 1024, net::CompletionCallback()));
  EXPECT_EQ(1024, entry->WriteSparseData(
      10000, buf, 1024, net::CompletionCallback()));

  // Writes in the middle of an entry and spans 2 child entries.
  EXPECT_EQ(8192, entry->WriteSparseData(
      50000, buf, 8192, net::CompletionCallback()));

  int64 start;
  net::TestCompletionCallback cb;
  // Test that we stop at a discontinuous child at the second block.
  int rv = entry->GetAvailableRange(0, 10000, &start, cb.callback());
  EXPECT_EQ(1024, cb.GetResult(rv));
  EXPECT_EQ(0, start);

  // Test that number of bytes is reported correctly when we start from the
  // middle of a filled region.
  rv = entry->GetAvailableRange(512, 10000, &start, cb.callback());
  EXPECT_EQ(512, cb.GetResult(rv));
  EXPECT_EQ(512, start);

  // Test that we found bytes in the child of next block.
  rv = entry->GetAvailableRange(1024, 10000, &start, cb.callback());
  EXPECT_EQ(1024, cb.GetResult(rv));
  EXPECT_EQ(5120, start);

  // Test that the desired length is respected. It starts within a filled
  // region.
  rv = entry->GetAvailableRange(5500, 512, &start, cb.callback());
  EXPECT_EQ(512, cb.GetResult(rv));
  EXPECT_EQ(5500, start);

  // Test that the desired length is respected. It starts before a filled
  // region.
  rv = entry->GetAvailableRange(5000, 620, &start, cb.callback());
  EXPECT_EQ(500, cb.GetResult(rv));
  EXPECT_EQ(5120, start);

  // Test that multiple blocks are scanned.
  rv = entry->GetAvailableRange(40000, 20000, &start, cb.callback());
  EXPECT_EQ(8192, cb.GetResult(rv));
  EXPECT_EQ(50000, start);

  entry->Close();
}

void DiskCacheEntryTest::UpdateSparseEntry() {
  std::string key("the first key");
  disk_cache::Entry* entry1;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry1));

  const int kSize = 2048;
  scoped_refptr<net::IOBuffer> buf_1(new net::IOBuffer(kSize));
  scoped_refptr<net::IOBuffer> buf_2(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf_1->data(), kSize, false);

  // Write at offset 0.
  VerifySparseIO(entry1, 0, buf_1, kSize, buf_2);
  entry1->Close();

  // Write at offset 2048.
  ASSERT_EQ(net::OK, OpenEntry(key, &entry1));
  VerifySparseIO(entry1, 2048, buf_1, kSize, buf_2);

  disk_cache::Entry* entry2;
  ASSERT_EQ(net::OK, CreateEntry("the second key", &entry2));

  entry1->Close();
  entry2->Close();
  FlushQueueForTest();
  if (memory_only_)
    EXPECT_EQ(2, cache_->GetEntryCount());
  else
    EXPECT_EQ(3, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, UpdateSparseEntry) {
  SetCacheType(net::MEDIA_CACHE);
  InitCache();
  UpdateSparseEntry();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyUpdateSparseEntry) {
  SetMemoryOnlyMode();
  SetCacheType(net::MEDIA_CACHE);
  InitCache();
  UpdateSparseEntry();
}

void DiskCacheEntryTest::DoomSparseEntry() {
  std::string key1("the first key");
  std::string key2("the second key");
  disk_cache::Entry *entry1, *entry2;
  ASSERT_EQ(net::OK, CreateEntry(key1, &entry1));
  ASSERT_EQ(net::OK, CreateEntry(key2, &entry2));

  const int kSize = 4 * 1024;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf->data(), kSize, false);

  int64 offset = 1024;
  // Write to a bunch of ranges.
  for (int i = 0; i < 12; i++) {
    EXPECT_EQ(kSize, entry1->WriteSparseData(offset, buf, kSize,
                                             net::CompletionCallback()));
    // Keep the second map under the default size.
    if (i < 9) {
      EXPECT_EQ(kSize, entry2->WriteSparseData(offset, buf, kSize,
                                               net::CompletionCallback()));
    }

    offset *= 4;
  }

  if (memory_only_)
    EXPECT_EQ(2, cache_->GetEntryCount());
  else
    EXPECT_EQ(15, cache_->GetEntryCount());

  // Doom the first entry while it's still open.
  entry1->Doom();
  entry1->Close();
  entry2->Close();

  // Doom the second entry after it's fully saved.
  EXPECT_EQ(net::OK, DoomEntry(key2));

  // Make sure we do all needed work. This may fail for entry2 if between Close
  // and DoomEntry the system decides to remove all traces of the file from the
  // system cache so we don't see that there is pending IO.
  MessageLoop::current()->RunUntilIdle();

  if (memory_only_) {
    EXPECT_EQ(0, cache_->GetEntryCount());
  } else {
    if (5 == cache_->GetEntryCount()) {
      // Most likely we are waiting for the result of reading the sparse info
      // (it's always async on Posix so it is easy to miss). Unfortunately we
      // don't have any signal to watch for so we can only wait.
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(500));
      MessageLoop::current()->RunUntilIdle();
    }
    EXPECT_EQ(0, cache_->GetEntryCount());
  }
}

TEST_F(DiskCacheEntryTest, DoomSparseEntry) {
  UseCurrentThread();
  InitCache();
  DoomSparseEntry();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyDoomSparseEntry) {
  SetMemoryOnlyMode();
  InitCache();
  DoomSparseEntry();
}

// A CompletionCallback wrapper that deletes the cache from within the callback.
// The way a CompletionCallback works means that all tasks (even new ones)
// are executed by the message loop before returning to the caller so the only
// way to simulate a race is to execute what we want on the callback.
class SparseTestCompletionCallback: public net::TestCompletionCallback {
 public:
  explicit SparseTestCompletionCallback(disk_cache::Backend* cache)
      : cache_(cache) {
  }

 private:
  virtual void SetResult(int result) OVERRIDE {
    delete cache_;
    TestCompletionCallback::SetResult(result);
  }

  disk_cache::Backend* cache_;
  DISALLOW_COPY_AND_ASSIGN(SparseTestCompletionCallback);
};

// Tests that we don't crash when the backend is deleted while we are working
// deleting the sub-entries of a sparse entry.
TEST_F(DiskCacheEntryTest, DoomSparseEntry2) {
  UseCurrentThread();
  InitCache();
  std::string key("the key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 4 * 1024;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf->data(), kSize, false);

  int64 offset = 1024;
  // Write to a bunch of ranges.
  for (int i = 0; i < 12; i++) {
    EXPECT_EQ(kSize, entry->WriteSparseData(offset, buf, kSize,
                                            net::CompletionCallback()));
    offset *= 4;
  }
  EXPECT_EQ(9, cache_->GetEntryCount());

  entry->Close();
  SparseTestCompletionCallback cb(cache_);
  int rv = cache_->DoomEntry(key, cb.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  EXPECT_EQ(net::OK, cb.WaitForResult());

  // TearDown will attempt to delete the cache_.
  cache_ = NULL;
}

void DiskCacheEntryTest::PartialSparseEntry() {
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  // We should be able to deal with IO that is not aligned to the block size
  // of a sparse entry, at least to write a big range without leaving holes.
  const int kSize = 4 * 1024;
  const int kSmallSize = 128;
  scoped_refptr<net::IOBuffer> buf1(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf1->data(), kSize, false);

  // The first write is just to extend the entry. The third write occupies
  // a 1KB block partially, it may not be written internally depending on the
  // implementation.
  EXPECT_EQ(kSize, WriteSparseData(entry, 20000, buf1, kSize));
  EXPECT_EQ(kSize, WriteSparseData(entry, 500, buf1, kSize));
  EXPECT_EQ(kSmallSize, WriteSparseData(entry, 1080321, buf1, kSmallSize));
  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));

  scoped_refptr<net::IOBuffer> buf2(new net::IOBuffer(kSize));
  memset(buf2->data(), 0, kSize);
  EXPECT_EQ(0, ReadSparseData(entry, 8000, buf2, kSize));

  EXPECT_EQ(500, ReadSparseData(entry, kSize, buf2, kSize));
  EXPECT_EQ(0, memcmp(buf2->data(), buf1->data() + kSize - 500, 500));
  EXPECT_EQ(0, ReadSparseData(entry, 0, buf2, kSize));

  // This read should not change anything.
  EXPECT_EQ(96, ReadSparseData(entry, 24000, buf2, kSize));
  EXPECT_EQ(500, ReadSparseData(entry, kSize, buf2, kSize));
  EXPECT_EQ(0, ReadSparseData(entry, 99, buf2, kSize));

  int rv;
  int64 start;
  net::TestCompletionCallback cb;
  if (memory_only_) {
    rv = entry->GetAvailableRange(0, 600, &start, cb.callback());
    EXPECT_EQ(100, cb.GetResult(rv));
    EXPECT_EQ(500, start);
  } else {
    rv = entry->GetAvailableRange(0, 2048, &start, cb.callback());
    EXPECT_EQ(1024, cb.GetResult(rv));
    EXPECT_EQ(1024, start);
  }
  rv = entry->GetAvailableRange(kSize, kSize, &start, cb.callback());
  EXPECT_EQ(500, cb.GetResult(rv));
  EXPECT_EQ(kSize, start);
  rv = entry->GetAvailableRange(20 * 1024, 10000, &start, cb.callback());
  EXPECT_EQ(3616, cb.GetResult(rv));
  EXPECT_EQ(20 * 1024, start);

  // 1. Query before a filled 1KB block.
  // 2. Query within a filled 1KB block.
  // 3. Query beyond a filled 1KB block.
  if (memory_only_) {
    rv = entry->GetAvailableRange(19400, kSize, &start, cb.callback());
    EXPECT_EQ(3496, cb.GetResult(rv));
    EXPECT_EQ(20000, start);
  } else {
    rv = entry->GetAvailableRange(19400, kSize, &start, cb.callback());
    EXPECT_EQ(3016, cb.GetResult(rv));
    EXPECT_EQ(20480, start);
  }
  rv = entry->GetAvailableRange(3073, kSize, &start, cb.callback());
  EXPECT_EQ(1523, cb.GetResult(rv));
  EXPECT_EQ(3073, start);
  rv = entry->GetAvailableRange(4600, kSize, &start, cb.callback());
  EXPECT_EQ(0, cb.GetResult(rv));
  EXPECT_EQ(4600, start);

  // Now make another write and verify that there is no hole in between.
  EXPECT_EQ(kSize, WriteSparseData(entry, 500 + kSize, buf1, kSize));
  rv = entry->GetAvailableRange(1024, 10000, &start, cb.callback());
  EXPECT_EQ(7 * 1024 + 500, cb.GetResult(rv));
  EXPECT_EQ(1024, start);
  EXPECT_EQ(kSize, ReadSparseData(entry, kSize, buf2, kSize));
  EXPECT_EQ(0, memcmp(buf2->data(), buf1->data() + kSize - 500, 500));
  EXPECT_EQ(0, memcmp(buf2->data() + 500, buf1->data(), kSize - 500));

  entry->Close();
}

TEST_F(DiskCacheEntryTest, PartialSparseEntry) {
  InitCache();
  PartialSparseEntry();
}

TEST_F(DiskCacheEntryTest, MemoryPartialSparseEntry) {
  SetMemoryOnlyMode();
  InitCache();
  PartialSparseEntry();
}

// Tests that corrupt sparse children are removed automatically.
TEST_F(DiskCacheEntryTest, CleanupSparseEntry) {
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 4 * 1024;
  scoped_refptr<net::IOBuffer> buf1(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf1->data(), kSize, false);

  const int k1Meg = 1024 * 1024;
  EXPECT_EQ(kSize, WriteSparseData(entry, 8192, buf1, kSize));
  EXPECT_EQ(kSize, WriteSparseData(entry, k1Meg + 8192, buf1, kSize));
  EXPECT_EQ(kSize, WriteSparseData(entry, 2 * k1Meg + 8192, buf1, kSize));
  entry->Close();
  EXPECT_EQ(4, cache_->GetEntryCount());

  void* iter = NULL;
  int count = 0;
  std::string child_key[2];
  while (OpenNextEntry(&iter, &entry) == net::OK) {
    ASSERT_TRUE(entry != NULL);
    // Writing to an entry will alter the LRU list and invalidate the iterator.
    if (entry->GetKey() != key && count < 2)
      child_key[count++] = entry->GetKey();
    entry->Close();
  }
  for (int i = 0; i < 2; i++) {
    ASSERT_EQ(net::OK, OpenEntry(child_key[i], &entry));
    // Overwrite the header's magic and signature.
    EXPECT_EQ(12, WriteData(entry, 2, 0, buf1, 12, false));
    entry->Close();
  }

  EXPECT_EQ(4, cache_->GetEntryCount());
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));

  // Two children should be gone. One while reading and one while writing.
  EXPECT_EQ(0, ReadSparseData(entry, 2 * k1Meg + 8192, buf1, kSize));
  EXPECT_EQ(kSize, WriteSparseData(entry, k1Meg + 16384, buf1, kSize));
  EXPECT_EQ(0, ReadSparseData(entry, k1Meg + 8192, buf1, kSize));

  // We never touched this one.
  EXPECT_EQ(kSize, ReadSparseData(entry, 8192, buf1, kSize));
  entry->Close();

  // We re-created one of the corrupt children.
  EXPECT_EQ(3, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, CancelSparseIO) {
  UseCurrentThread();
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 40 * 1024;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buf->data(), kSize, false);

  // This will open and write two "real" entries.
  net::TestCompletionCallback cb1, cb2, cb3, cb4, cb5;
  int rv = entry->WriteSparseData(
      1024 * 1024 - 4096, buf, kSize, cb1.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  int64 offset = 0;
  rv = entry->GetAvailableRange(offset, kSize, &offset, cb5.callback());
  rv = cb5.GetResult(rv);
  if (!cb1.have_result()) {
    // We may or may not have finished writing to the entry. If we have not,
    // we cannot start another operation at this time.
    EXPECT_EQ(net::ERR_CACHE_OPERATION_NOT_SUPPORTED, rv);
  }

  // We cancel the pending operation, and register multiple notifications.
  entry->CancelSparseIO();
  EXPECT_EQ(net::ERR_IO_PENDING, entry->ReadyForSparseIO(cb2.callback()));
  EXPECT_EQ(net::ERR_IO_PENDING, entry->ReadyForSparseIO(cb3.callback()));
  entry->CancelSparseIO();  // Should be a no op at this point.
  EXPECT_EQ(net::ERR_IO_PENDING, entry->ReadyForSparseIO(cb4.callback()));

  if (!cb1.have_result()) {
    EXPECT_EQ(net::ERR_CACHE_OPERATION_NOT_SUPPORTED,
              entry->ReadSparseData(offset, buf, kSize,
                                    net::CompletionCallback()));
    EXPECT_EQ(net::ERR_CACHE_OPERATION_NOT_SUPPORTED,
              entry->WriteSparseData(offset, buf, kSize,
                                     net::CompletionCallback()));
  }

  // Now see if we receive all notifications. Note that we should not be able
  // to write everything (unless the timing of the system is really weird).
  rv = cb1.WaitForResult();
  EXPECT_TRUE(rv == 4096 || rv == kSize);
  EXPECT_EQ(net::OK, cb2.WaitForResult());
  EXPECT_EQ(net::OK, cb3.WaitForResult());
  EXPECT_EQ(net::OK, cb4.WaitForResult());

  rv = entry->GetAvailableRange(offset, kSize, &offset, cb5.callback());
  EXPECT_EQ(0, cb5.GetResult(rv));
  entry->Close();
}

// Tests that we perform sanity checks on an entry's key. Note that there are
// other tests that exercise sanity checks by using saved corrupt files.
TEST_F(DiskCacheEntryTest, KeySanityCheck) {
  UseCurrentThread();
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  disk_cache::EntryImpl* entry_impl =
      static_cast<disk_cache::EntryImpl*>(entry);
  disk_cache::EntryStore* store = entry_impl->entry()->Data();

  // We have reserved space for a short key (one block), let's say that the key
  // takes more than one block, and remove the NULLs after the actual key.
  store->key_len = 800;
  memset(store->key + key.size(), 'k', sizeof(store->key) - key.size());
  entry_impl->entry()->set_modified();
  entry->Close();

  // We have a corrupt entry. Now reload it. We should NOT read beyond the
  // allocated buffer here.
  ASSERT_NE(net::OK, OpenEntry(key, &entry));
  DisableIntegrityCheck();
}

// The simple cache backend isn't intended to work on Windows, which has very
// different file system guarantees from Linux.
#if defined(OS_POSIX)

TEST_F(DiskCacheEntryTest, SimpleCacheInternalAsyncIO) {
  SetSimpleCacheMode();
  InitCache();
  InternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, SimpleCacheExternalAsyncIO) {
  SetSimpleCacheMode();
  InitCache();
  ExternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, SimpleCacheReleaseBuffer) {
  SetSimpleCacheMode();
  InitCache();
  ReleaseBuffer();
}

TEST_F(DiskCacheEntryTest, SimpleCacheStreamAccess) {
  SetSimpleCacheMode();
  InitCache();
  StreamAccess();
}

TEST_F(DiskCacheEntryTest, SimpleCacheGetKey) {
  SetSimpleCacheMode();
  InitCache();
  GetKey();
}

TEST_F(DiskCacheEntryTest, SimpleCacheGetTimes) {
  SetSimpleCacheMode();
  InitCache();
  GetTimes();
}

TEST_F(DiskCacheEntryTest, SimpleCacheGrowData) {
  SetSimpleCacheMode();
  InitCache();
  GrowData();
}

TEST_F(DiskCacheEntryTest, SimpleCacheTruncateData) {
  SetSimpleCacheMode();
  InitCache();
  TruncateData();
}

TEST_F(DiskCacheEntryTest, SimpleCacheZeroLengthIO) {
  SetSimpleCacheMode();
  InitCache();
  ZeroLengthIO();
}

TEST_F(DiskCacheEntryTest, DISABLED_SimpleCacheBuffering) {
  SetSimpleCacheMode();
  InitCache();
  Buffering();
}

TEST_F(DiskCacheEntryTest, SimpleCacheSizeAtCreate) {
  SetSimpleCacheMode();
  InitCache();
  SizeAtCreate();
}

TEST_F(DiskCacheEntryTest, SimpleCacheReuseExternalEntry) {
  SetSimpleCacheMode();
  SetMaxSize(200 * 1024);
  InitCache();
  ReuseEntry(20 * 1024);
}

TEST_F(DiskCacheEntryTest, SimpleCacheReuseInternalEntry) {
  SetSimpleCacheMode();
  SetMaxSize(100 * 1024);
  InitCache();
  ReuseEntry(10 * 1024);
}

TEST_F(DiskCacheEntryTest, SimpleCacheSizeChanges) {
  SetSimpleCacheMode();
  InitCache();
  SizeChanges();
}

TEST_F(DiskCacheEntryTest, SimpleCacheInvalidData) {
  SetSimpleCacheMode();
  InitCache();
  InvalidData();
}

TEST_F(DiskCacheEntryTest, SimpleCacheReadWriteDestroyBuffer) {
  SetSimpleCacheMode();
  InitCache();
  ReadWriteDestroyBuffer();
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomEntry) {
  SetSimpleCacheMode();
  InitCache();
  DoomNormalEntry();
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomEntryNextToOpenEntry) {
  SetSimpleCacheMode();
  InitCache();
  DoomEntryNextToOpenEntry();
}

TEST_F(DiskCacheEntryTest, SimpleCacheDoomedEntry) {
  SetSimpleCacheMode();
  InitCache();
  DoomedEntry();
}

void DiskCacheEntryTest::SimpleCacheMakeBadChecksumEntry(const char* key) {
  disk_cache::Entry* entry = NULL;

  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  disk_cache::Entry* null = NULL;
  EXPECT_NE(null, entry);

  const std::string data = "this is very good data";
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(data.size()));
  std::copy(data.begin(), data.end(), buffer->data());

  ASSERT_EQ(implicit_cast<int>(data.size()),
            WriteData(entry, 0, 0, buffer, data.size(), false));
  entry->Close();
  entry = NULL;

  // Corrupt the data.
  base::FilePath entry_file0_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndIndex(key, 0));
  int flags = base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_OPEN;
  base::PlatformFile entry_file0 =
      base::CreatePlatformFile(entry_file0_path, flags, NULL, NULL);
  ASSERT_NE(base::kInvalidPlatformFileValue, entry_file0);

  const std::string bad_data = "HAHAHA";
  DCHECK_LE(bad_data.size(), data.size());
  int64 file_offset =
      disk_cache::simple_util::GetFileOffsetFromKeyAndDataOffset(key, 0);
  ASSERT_EQ(implicit_cast<int>(bad_data.size()),
            base::WritePlatformFile(entry_file0, file_offset,
                                    bad_data.data(), bad_data.size()));
  EXPECT_TRUE(base::ClosePlatformFile(entry_file0));
}

// Tests that the simple cache can detect entries that have bad data.
TEST_F(DiskCacheEntryTest, SimpleCacheBadChecksum) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";
  SimpleCacheMakeBadChecksumEntry(key);

  disk_cache::Entry* entry = NULL;

  // Open the entry.
  EXPECT_EQ(net::OK, OpenEntry(key, &entry));

  const int kReadBufferSize = 200;
  DCHECK_GE(kReadBufferSize, entry->GetDataSize(0));
  scoped_refptr<net::IOBuffer> read_buffer(new net::IOBuffer(kReadBufferSize));
  EXPECT_EQ(net::ERR_FAILED,
            ReadData(entry, 0, 0, read_buffer, kReadBufferSize));

  entry->Close();
}

// Tests that an entry that has had an IO error occur can still be Doomed().
TEST_F(DiskCacheEntryTest, SimpleCacheErrorThenDoom) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";
  SimpleCacheMakeBadChecksumEntry(key);

  disk_cache::Entry* entry = NULL;

  // Open the entry, forcing an IO error.
  EXPECT_EQ(net::OK, OpenEntry(key, &entry));

  const int kReadBufferSize = 200;
  DCHECK_GE(kReadBufferSize, entry->GetDataSize(0));
  scoped_refptr<net::IOBuffer> read_buffer(new net::IOBuffer(kReadBufferSize));
  EXPECT_EQ(net::ERR_FAILED,
            ReadData(entry, 0, 0, read_buffer, kReadBufferSize));

  entry->Doom();  // Should not crash.
  entry->Close();
}

bool TruncatePath(const base::FilePath& file_path, int64 length)  {
  const int flags = base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_OPEN;
  base::PlatformFile file =
      base::CreatePlatformFile(file_path, flags, NULL, NULL);
  if (base::kInvalidPlatformFileValue == file)
    return false;
  const bool result = base::TruncatePlatformFile(file, length);
  base::ClosePlatformFile(file);
  return result;
}

TEST_F(DiskCacheEntryTest, SimpleCacheNoEOF) {
  SetSimpleCacheMode();
  InitCache();

  const char key[] = "the first key";

  disk_cache::Entry* entry = NULL;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));
  disk_cache::Entry* null = NULL;
  EXPECT_NE(null, entry);
  entry->Close();
  entry = NULL;

  // Force the entry to flush to disk, so subsequent platform file operations
  // succed.
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  entry->Close();
  entry = NULL;

  // Truncate the file such that the length isn't sufficient to have an EOF
  // record.
  int kTruncationBytes = -implicit_cast<int>(sizeof(disk_cache::SimpleFileEOF));
  const base::FilePath entry_path = cache_path_.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndIndex(key, 0));
  const int64 invalid_size =
      disk_cache::simple_util::GetFileSizeFromKeyAndDataSize(key,
                                                             kTruncationBytes);
  EXPECT_TRUE(TruncatePath(entry_path, invalid_size));
  EXPECT_EQ(net::ERR_FAILED, OpenEntry(key, &entry));
  DisableIntegrityCheck();
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic) {
  // Test sequence:
  // Create, Write, Read, Write, Read, Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = NULL;
  const char key[] = "the first key";

  MessageLoopHelper helper;
  CallbackTest callback1(&helper, false);
  CallbackTest callback2(&helper, false);
  CallbackTest callback3(&helper, false);
  CallbackTest callback4(&helper, false);
  CallbackTest callback5(&helper, false);

  int expected = 0;
  const int kSize1 = 10;
  const int kSize2 = 20;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer1_read(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize2));
  scoped_refptr<net::IOBuffer> buffer2_read(new net::IOBuffer(kSize2));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);

  disk_cache::Entry* entry = NULL;
  // Create is optimistic, must return OK.
  ASSERT_EQ(net::OK,
            cache_->CreateEntry(key, &entry,
                                base::Bind(&CallbackTest::Run,
                                           base::Unretained(&callback1))));
  EXPECT_NE(null, entry);

  // This write may or may not be optimistic (it depends if the previous
  // optimistic create already finished by the time we call the write here).
  int ret = entry->WriteData(
      0, 0, buffer1, kSize1,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback2)), false);
  EXPECT_TRUE(kSize1 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  // This Read must not be optimistic, since we don't support that yet.
  EXPECT_EQ(net::ERR_IO_PENDING, entry->ReadData(
      0, 0, buffer1_read, kSize1,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback3))));
  expected++;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer1_read->data(), kSize1));

  // At this point after waiting, the pending operations queue on the entry
  // should be empty, so the next Write operation must run as optimistic.
  EXPECT_EQ(kSize2,
            entry->WriteData(
                0, 0, buffer2, kSize2,
                base::Bind(&CallbackTest::Run,
                           base::Unretained(&callback4)), false));

  // Lets do another read so we block until both the write and the read
  // operation finishes and we can then test for HasOneRef() below.
  EXPECT_EQ(net::ERR_IO_PENDING, entry->ReadData(
      0, 0, buffer2_read, kSize2,
      base::Bind(&CallbackTest::Run, base::Unretained(&callback5))));
  expected++;

  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer2_read->data(), kSize2));

  // Check that we are not leaking.
  EXPECT_NE(entry, null);
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());
  entry->Close();
  entry = NULL;
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic2) {
  // Test sequence:
  // Create, Open, Close, Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = NULL;
  const char key[] = "the first key";

  MessageLoopHelper helper;
  CallbackTest callback1(&helper, false);
  CallbackTest callback2(&helper, false);

  disk_cache::Entry* entry = NULL;
  ASSERT_EQ(net::OK,
            cache_->CreateEntry(key, &entry,
                                base::Bind(&CallbackTest::Run,
                                           base::Unretained(&callback1))));
  EXPECT_NE(null, entry);

  disk_cache::Entry* entry2 = NULL;
  EXPECT_EQ(net::ERR_IO_PENDING,
            cache_->OpenEntry(key, &entry2,
                              base::Bind(&CallbackTest::Run,
                                         base::Unretained(&callback2))));
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(1));

  EXPECT_NE(null, entry2);
  EXPECT_EQ(entry, entry2);

  // We have to call close twice, since we called create and open above.
  entry->Close();

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());
  entry->Close();
  entry = NULL;
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic3) {
  // Test sequence:
  // Create, Close, Open, Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = NULL;
  const char key[] = "the first key";

  disk_cache::Entry* entry = NULL;
  ASSERT_EQ(net::OK,
            cache_->CreateEntry(key, &entry, net::CompletionCallback()));
  EXPECT_NE(null, entry);
  entry->Close();

  net::TestCompletionCallback cb;
  disk_cache::Entry* entry2 = NULL;
  EXPECT_EQ(net::ERR_IO_PENDING,
            cache_->OpenEntry(key, &entry2, cb.callback()));
  EXPECT_EQ(net::OK, cb.GetResult(net::ERR_IO_PENDING));

  EXPECT_NE(null, entry2);
  EXPECT_EQ(entry, entry2);

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry2)->HasOneRef());
  entry2->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic4) {
  // Test sequence:
  // Create, Close, Write, Open, Open, Close, Write, Read, Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = NULL;
  const char key[] = "the first key";

  net::TestCompletionCallback cb;
  const int kSize1 = 10;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  disk_cache::Entry* entry = NULL;

  ASSERT_EQ(net::OK,
            cache_->CreateEntry(key, &entry, net::CompletionCallback()));
  EXPECT_NE(null, entry);
  entry->Close();

  // Lets do a Write so we block until both the Close and the Write
  // operation finishes. Write must fail since we are writing in a closed entry.
  EXPECT_EQ(net::ERR_IO_PENDING, entry->WriteData(
      0, 0, buffer1, kSize1, cb.callback(), false));
  EXPECT_EQ(net::ERR_FAILED, cb.GetResult(net::ERR_IO_PENDING));

  // Finish running the pending tasks so that we fully complete the close
  // operation and destroy the entry object.
  MessageLoop::current()->RunUntilIdle();

  // At this point the |entry| must have been destroyed, and called
  // RemoveSelfFromBackend().
  disk_cache::Entry* entry2 = NULL;
  EXPECT_EQ(net::ERR_IO_PENDING,
            cache_->OpenEntry(key, &entry2, cb.callback()));
  EXPECT_EQ(net::OK, cb.GetResult(net::ERR_IO_PENDING));
  EXPECT_NE(null, entry2);

  disk_cache::Entry* entry3 = NULL;
  EXPECT_EQ(net::ERR_IO_PENDING,
            cache_->OpenEntry(key, &entry3, cb.callback()));
  EXPECT_EQ(net::OK, cb.GetResult(net::ERR_IO_PENDING));
  EXPECT_NE(null, entry3);
  EXPECT_EQ(entry2, entry3);
  entry3->Close();

  // The previous Close doesn't actually closes the entry since we opened it
  // twice, so the next Write operation must succeed and it must be able to
  // perform it optimistically, since there is no operation running on this
  // entry.
  EXPECT_EQ(kSize1, entry2->WriteData(
      0, 0, buffer1, kSize1, net::CompletionCallback(), false));

  // Lets do another read so we block until both the write and the read
  // operation finishes and we can then test for HasOneRef() below.
  EXPECT_EQ(net::ERR_IO_PENDING, entry2->ReadData(
      0, 0, buffer1, kSize1, cb.callback()));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry2)->HasOneRef());
  entry2->Close();
}

// This test is flaky because of the race of Create followed by a Doom.
// See test SimpleCacheCreateDoomRace.
TEST_F(DiskCacheEntryTest, DISABLED_SimpleCacheOptimistic5) {
  // Test sequence:
  // Create, Doom, Write, Read, Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = NULL;
  const char key[] = "the first key";

  net::TestCompletionCallback cb;
  const int kSize1 = 10;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  disk_cache::Entry* entry = NULL;

  ASSERT_EQ(net::OK,
            cache_->CreateEntry(key, &entry, net::CompletionCallback()));
  EXPECT_NE(null, entry);
  entry->Doom();

  EXPECT_EQ(net::ERR_IO_PENDING, entry->WriteData(
      0, 0, buffer1, kSize1, cb.callback(), false));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));

  EXPECT_EQ(net::ERR_IO_PENDING, entry->ReadData(
      0, 0, buffer1, kSize1, cb.callback()));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());
  entry->Close();
}

TEST_F(DiskCacheEntryTest, SimpleCacheOptimistic6) {
  // Test sequence:
  // Create, Write, Doom, Doom, Read, Doom, Close.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = NULL;
  const char key[] = "the first key";

  net::TestCompletionCallback cb;
  const int kSize1 = 10;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  scoped_refptr<net::IOBuffer> buffer1_read(new net::IOBuffer(kSize1));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  disk_cache::Entry* entry = NULL;

  ASSERT_EQ(net::OK,
            cache_->CreateEntry(key, &entry, net::CompletionCallback()));
  EXPECT_NE(null, entry);

  EXPECT_EQ(net::ERR_IO_PENDING, entry->WriteData(
      0, 0, buffer1, kSize1, cb.callback(), false));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));

  entry->Doom();
  entry->Doom();

  // This Read must not be optimistic, since we don't support that yet.
  EXPECT_EQ(net::ERR_IO_PENDING, entry->ReadData(
      0, 0, buffer1_read, kSize1, cb.callback()));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer1_read->data(), kSize1));

  entry->Doom();

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());
  entry->Close();
}

TEST_F(DiskCacheEntryTest, DISABLED_SimpleCacheCreateDoomRace) {
  // Test sequence:
  // Create, Doom, Write, Close, Check files are not on disk anymore.
  SetSimpleCacheMode();
  InitCache();
  disk_cache::Entry* null = NULL;
  const char key[] = "the first key";

  net::TestCompletionCallback cb;
  const int kSize1 = 10;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize1));
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  disk_cache::Entry* entry = NULL;

  ASSERT_EQ(net::OK,
            cache_->CreateEntry(key, &entry, net::CompletionCallback()));
  EXPECT_NE(null, entry);

  cache_->DoomEntry(key, cb.callback());
  EXPECT_EQ(net::OK, cb.GetResult(net::ERR_IO_PENDING));

  // Lets do a Write so we block until all operations are done, so we can check
  // the HasOneRef() below. This call can't be optimistic and we are checking
  // that here.
  EXPECT_EQ(net::ERR_IO_PENDING, entry->WriteData(
      0, 0, buffer1, kSize1, cb.callback(), false));
  EXPECT_EQ(kSize1, cb.GetResult(net::ERR_IO_PENDING));

  // Check that we are not leaking.
  EXPECT_TRUE(
      static_cast<disk_cache::SimpleEntryImpl*>(entry)->HasOneRef());
  entry->Close();

  // Finish running the pending tasks so that we fully complete the close
  // operation and destroy the entry object.
  MessageLoop::current()->RunUntilIdle();

  for (int i = 0; i < disk_cache::kSimpleEntryFileCount; ++i) {
    base::FilePath entry_file_path = cache_path_.AppendASCII(
        disk_cache::simple_util::GetFilenameFromKeyAndIndex(key, i));
    base::PlatformFileInfo info;
    EXPECT_FALSE(file_util::GetFileInfo(entry_file_path, &info));
  }
}

// Tests that old entries are evicted while new entries remain in the index.
// This test relies on non-mandatory properties of the simple Cache Backend:
// LRU eviction, specific values of high-watermark and low-watermark etc.
// When changing the eviction algorithm, the test will have to be re-engineered.
TEST_F(DiskCacheEntryTest, SimpleCacheEvictOldEntries) {
  const int kMaxSize = 200 * 1024;
  const int kWriteSize = kMaxSize / 10;
  const int kNumExtraEntries = 12;
  SetSimpleCacheMode();
  SetMaxSize(kMaxSize);
  InitCache();

  std::string key1("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key1, &entry));
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kWriteSize));
  CacheTestFillBuffer(buffer->data(), kWriteSize, false);
  EXPECT_EQ(kWriteSize, WriteData(entry, 0, 0, buffer, kWriteSize, false));
  entry->Close();

  std::string key2("the key prefix");
  for (int i = 0; i < kNumExtraEntries; i++) {
    ASSERT_EQ(net::OK, CreateEntry(key2 + base::StringPrintf("%d", i), &entry));
    EXPECT_EQ(kWriteSize, WriteData(entry, 0, 0, buffer, kWriteSize, false));
    entry->Close();
  }

  // TODO(pasko): Find a way to wait for the eviction task(s) to finish by using
  // the internal knowledge about |SimpleBackendImpl|.
  ASSERT_NE(net::OK, OpenEntry(key1, &entry))
      << "Should have evicted the old entry";
  for (int i = 0; i < 2; i++) {
    int entry_no = kNumExtraEntries - i - 1;
    // Generally there is no guarantee that at this point the backround eviction
    // is finished. We are testing the positive case, i.e. when the eviction
    // never reaches this entry, should be non-flaky.
    ASSERT_EQ(net::OK, OpenEntry(key2 + base::StringPrintf("%d", entry_no),
                                 &entry))
        << "Should not have evicted fresh entry " << entry_no;
    entry->Close();
  }
}

#endif  // defined(OS_POSIX)
