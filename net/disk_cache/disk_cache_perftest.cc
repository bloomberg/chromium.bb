// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>
#include <string>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/hash.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/perf_time_logger.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/blockfile/block_files.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;

namespace {

const size_t kNumEntries = 10000;
const int kHeadersSize = 2000;

const int kBodySize = 72 * 1024 - 1;

// HttpCache likes this chunk size.
const int kChunkSize = 32 * 1024;

// As of 2017-01-12, this is a typical per-tab limit on HTTP connections.
const int kMaxParallelOperations = 10;

void MaybeIncreaseFdLimitTo(unsigned int max_descriptors) {
#if defined(OS_POSIX) && !defined(OS_FUCHSIA)
  base::IncreaseFdLimitTo(max_descriptors);
#endif
}

struct TestEntry {
  std::string key;
  int data_len;
};

enum class WhatToRead {
  HEADERS_ONLY,
  HEADERS_AND_BODY,
};

class DiskCachePerfTest : public DiskCacheTestWithCache {
 public:
  DiskCachePerfTest() { MaybeIncreaseFdLimitTo(kFdLimitForCacheTests); }

  const std::vector<TestEntry>& entries() const { return entries_; }

 protected:
  // Helper methods for constructing tests.
  bool TimeWrites();
  bool TimeReads(WhatToRead what_to_read, const char* timer_message);
  void ResetAndEvictSystemDiskCache();

  // Callbacks used within tests for intermediate operations.
  void WriteCallback(const net::CompletionCallback& final_callback,
                     scoped_refptr<net::IOBuffer> headers_buffer,
                     scoped_refptr<net::IOBuffer> body_buffer,
                     disk_cache::Entry* cache_entry,
                     int entry_index,
                     size_t write_offset,
                     int result);

  // Complete perf tests.
  void CacheBackendPerformance();

  const size_t kFdLimitForCacheTests = 8192;

  std::vector<TestEntry> entries_;
};

class WriteHandler {
 public:
  WriteHandler(const DiskCachePerfTest* test,
               disk_cache::Backend* cache,
               net::CompletionCallback final_callback)
      : test_(test), cache_(cache), final_callback_(final_callback) {
    CacheTestFillBuffer(headers_buffer_->data(), kHeadersSize, false);
    CacheTestFillBuffer(body_buffer_->data(), kChunkSize, false);
  }

  void Run();

 protected:
  void CreateNextEntry();

  void CreateCallback(std::unique_ptr<disk_cache::Entry*> unique_entry_ptr,
                      int data_len,
                      int result);
  void WriteDataCallback(disk_cache::Entry* entry,
                         int next_offset,
                         int data_len,
                         int expected_result,
                         int result);

 private:
  bool CheckForErrorAndCancel(int result);

  const DiskCachePerfTest* test_;
  disk_cache::Backend* cache_;
  net::CompletionCallback final_callback_;

  size_t next_entry_index_ = 0;
  size_t pending_operations_count_ = 0;

  int pending_result_ = net::OK;

  scoped_refptr<net::IOBuffer> headers_buffer_ =
      new net::IOBuffer(kHeadersSize);
  scoped_refptr<net::IOBuffer> body_buffer_ = new net::IOBuffer(kChunkSize);
};

void WriteHandler::Run() {
  for (int i = 0; i < kMaxParallelOperations; ++i) {
    ++pending_operations_count_;
    CreateNextEntry();
  }
}

void WriteHandler::CreateNextEntry() {
  ASSERT_GT(kNumEntries, next_entry_index_);
  TestEntry test_entry = test_->entries()[next_entry_index_++];
  disk_cache::Entry** entry_ptr = new disk_cache::Entry*();
  std::unique_ptr<disk_cache::Entry*> unique_entry_ptr(entry_ptr);
  net::CompletionCallback callback =
      base::Bind(&WriteHandler::CreateCallback, base::Unretained(this),
                 base::Passed(&unique_entry_ptr), test_entry.data_len);
  int result = cache_->CreateEntry(test_entry.key, entry_ptr, callback);
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

void WriteHandler::CreateCallback(std::unique_ptr<disk_cache::Entry*> entry_ptr,
                                  int data_len,
                                  int result) {
  if (CheckForErrorAndCancel(result))
    return;

  disk_cache::Entry* entry = *entry_ptr;

  net::CompletionCallback callback =
      base::Bind(&WriteHandler::WriteDataCallback, base::Unretained(this),
                 entry, 0, data_len, kHeadersSize);
  int new_result = entry->WriteData(0, 0, headers_buffer_.get(), kHeadersSize,
                                    callback, false);
  if (new_result != net::ERR_IO_PENDING)
    callback.Run(new_result);
}

void WriteHandler::WriteDataCallback(disk_cache::Entry* entry,
                                     int next_offset,
                                     int data_len,
                                     int expected_result,
                                     int result) {
  if (CheckForErrorAndCancel(result)) {
    entry->Close();
    return;
  }
  DCHECK_LE(next_offset, data_len);
  if (next_offset == data_len) {
    entry->Close();
    if (next_entry_index_ < kNumEntries) {
      CreateNextEntry();
    } else {
      --pending_operations_count_;
      if (pending_operations_count_ == 0)
        final_callback_.Run(net::OK);
    }
    return;
  }

  int write_size = std::min(kChunkSize, data_len - next_offset);
  net::CompletionCallback callback =
      base::Bind(&WriteHandler::WriteDataCallback, base::Unretained(this),
                 entry, next_offset + write_size, data_len, write_size);
  int new_result = entry->WriteData(1, next_offset, body_buffer_.get(),
                                    write_size, callback, true);
  if (new_result != net::ERR_IO_PENDING)
    callback.Run(new_result);
}

bool WriteHandler::CheckForErrorAndCancel(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result != net::OK && !(result > 0))
    pending_result_ = result;
  if (pending_result_ != net::OK) {
    --pending_operations_count_;
    if (pending_operations_count_ == 0)
      final_callback_.Run(pending_result_);
    return true;
  }
  return false;
}

class ReadHandler {
 public:
  ReadHandler(const DiskCachePerfTest* test,
              WhatToRead what_to_read,
              disk_cache::Backend* cache,
              net::CompletionCallback final_callback)
      : test_(test),
        what_to_read_(what_to_read),
        cache_(cache),
        final_callback_(final_callback) {
    for (int i = 0; i < kMaxParallelOperations; ++i)
      read_buffers_[i] = new net::IOBuffer(std::max(kHeadersSize, kChunkSize));
  }

  void Run();

 protected:
  void OpenNextEntry(int parallel_operation_index);

  void OpenCallback(int parallel_operation_index,
                    std::unique_ptr<disk_cache::Entry*> unique_entry_ptr,
                    int data_len,
                    int result);
  void ReadDataCallback(int parallel_operation_index,
                        disk_cache::Entry* entry,
                        int next_offset,
                        int data_len,
                        int expected_result,
                        int result);

 private:
  bool CheckForErrorAndCancel(int result);

  const DiskCachePerfTest* test_;
  const WhatToRead what_to_read_;

  disk_cache::Backend* cache_;
  net::CompletionCallback final_callback_;

  size_t next_entry_index_ = 0;
  size_t pending_operations_count_ = 0;

  int pending_result_ = net::OK;

  scoped_refptr<net::IOBuffer> read_buffers_[kMaxParallelOperations];
};

void ReadHandler::Run() {
  for (int i = 0; i < kMaxParallelOperations; ++i) {
    OpenNextEntry(pending_operations_count_);
    ++pending_operations_count_;
  }
}

void ReadHandler::OpenNextEntry(int parallel_operation_index) {
  ASSERT_GT(kNumEntries, next_entry_index_);
  TestEntry test_entry = test_->entries()[next_entry_index_++];
  disk_cache::Entry** entry_ptr = new disk_cache::Entry*();
  std::unique_ptr<disk_cache::Entry*> unique_entry_ptr(entry_ptr);
  net::CompletionCallback callback =
      base::Bind(&ReadHandler::OpenCallback, base::Unretained(this),
                 parallel_operation_index, base::Passed(&unique_entry_ptr),
                 test_entry.data_len);
  int result = cache_->OpenEntry(test_entry.key, entry_ptr, callback);
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

void ReadHandler::OpenCallback(int parallel_operation_index,
                               std::unique_ptr<disk_cache::Entry*> entry_ptr,
                               int data_len,
                               int result) {
  if (CheckForErrorAndCancel(result))
    return;

  disk_cache::Entry* entry = *entry_ptr;

  EXPECT_EQ(data_len, entry->GetDataSize(1));

  net::CompletionCallback callback =
      base::Bind(&ReadHandler::ReadDataCallback, base::Unretained(this),
                 parallel_operation_index, entry, 0, data_len, kHeadersSize);
  int new_result =
      entry->ReadData(0, 0, read_buffers_[parallel_operation_index].get(),
                      kChunkSize, callback);
  if (new_result != net::ERR_IO_PENDING)
    callback.Run(new_result);
}

void ReadHandler::ReadDataCallback(int parallel_operation_index,
                                   disk_cache::Entry* entry,
                                   int next_offset,
                                   int data_len,
                                   int expected_result,
                                   int result) {
  if (CheckForErrorAndCancel(result)) {
    entry->Close();
    return;
  }
  DCHECK_LE(next_offset, data_len);
  if (what_to_read_ == WhatToRead::HEADERS_ONLY || next_offset == data_len) {
    entry->Close();
    if (next_entry_index_ < kNumEntries) {
      OpenNextEntry(parallel_operation_index);
    } else {
      --pending_operations_count_;
      if (pending_operations_count_ == 0)
        final_callback_.Run(net::OK);
    }
    return;
  }

  int expected_read_size = std::min(kChunkSize, data_len - next_offset);
  net::CompletionCallback callback = base::Bind(
      &ReadHandler::ReadDataCallback, base::Unretained(this),
      parallel_operation_index, entry, next_offset + expected_read_size,
      data_len, expected_read_size);
  int new_result = entry->ReadData(
      1, next_offset, read_buffers_[parallel_operation_index].get(), kChunkSize,
      callback);
  if (new_result != net::ERR_IO_PENDING)
    callback.Run(new_result);
}

bool ReadHandler::CheckForErrorAndCancel(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result != net::OK && !(result > 0))
    pending_result_ = result;
  if (pending_result_ != net::OK) {
    --pending_operations_count_;
    if (pending_operations_count_ == 0)
      final_callback_.Run(pending_result_);
    return true;
  }
  return false;
}

bool DiskCachePerfTest::TimeWrites() {
  for (size_t i = 0; i < kNumEntries; i++) {
    TestEntry entry;
    entry.key = GenerateKey(true);
    entry.data_len = base::RandInt(0, kBodySize);
    entries_.push_back(entry);
  }

  net::TestCompletionCallback cb;

  base::PerfTimeLogger timer("Write disk cache entries");

  WriteHandler write_handler(this, cache_.get(), cb.callback());
  write_handler.Run();
  return cb.WaitForResult() == net::OK;
}

bool DiskCachePerfTest::TimeReads(WhatToRead what_to_read,
                                  const char* timer_message) {
  base::PerfTimeLogger timer(timer_message);

  net::TestCompletionCallback cb;
  ReadHandler read_handler(this, what_to_read, cache_.get(), cb.callback());
  read_handler.Run();
  return cb.WaitForResult() == net::OK;
}

TEST_F(DiskCachePerfTest, BlockfileHashes) {
  base::PerfTimeLogger timer("Hash disk cache keys");
  for (int i = 0; i < 300000; i++) {
    std::string key = GenerateKey(true);
    base::Hash(key);
  }
  timer.Done();
}

void DiskCachePerfTest::ResetAndEvictSystemDiskCache() {
  base::RunLoop().RunUntilIdle();
  cache_.reset();

  // Flush all files in the cache out of system memory.
  const base::FilePath::StringType file_pattern = FILE_PATH_LITERAL("*");
  base::FileEnumerator enumerator(cache_path_, true /* recursive */,
                                  base::FileEnumerator::FILES, file_pattern);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    ASSERT_TRUE(base::EvictFileFromSystemCache(file_path));
  }
#if defined(OS_LINUX) || defined(OS_ANDROID)
  // And, cache directories, on platforms where the eviction utility supports
  // this (currently Linux and Android only).
  if (simple_cache_mode_) {
    ASSERT_TRUE(
        base::EvictFileFromSystemCache(cache_path_.AppendASCII("index-dir")));
  }
  ASSERT_TRUE(base::EvictFileFromSystemCache(cache_path_));
#endif

  DisableFirstCleanup();
  InitCache();
}

void DiskCachePerfTest::CacheBackendPerformance() {
  LOG(ERROR) << "Using cache at:" << cache_path_.MaybeAsASCII();
  SetMaxSize(500 * 1024 * 1024);
  InitCache();
  EXPECT_TRUE(TimeWrites());

  disk_cache::SimpleBackendImpl::FlushWorkerPoolForTesting();
  base::RunLoop().RunUntilIdle();

  ResetAndEvictSystemDiskCache();
  EXPECT_TRUE(TimeReads(WhatToRead::HEADERS_ONLY,
                        "Read disk cache headers only (cold)"));
  EXPECT_TRUE(TimeReads(WhatToRead::HEADERS_ONLY,
                        "Read disk cache headers only (warm)"));

  disk_cache::SimpleBackendImpl::FlushWorkerPoolForTesting();
  base::RunLoop().RunUntilIdle();

  ResetAndEvictSystemDiskCache();
  EXPECT_TRUE(TimeReads(WhatToRead::HEADERS_AND_BODY,
                        "Read disk cache entries (cold)"));
  EXPECT_TRUE(TimeReads(WhatToRead::HEADERS_AND_BODY,
                        "Read disk cache entries (warm)"));

  disk_cache::SimpleBackendImpl::FlushWorkerPoolForTesting();
  base::RunLoop().RunUntilIdle();
}

TEST_F(DiskCachePerfTest, CacheBackendPerformance) {
  CacheBackendPerformance();
}

TEST_F(DiskCachePerfTest, SimpleCacheBackendPerformance) {
  SetSimpleCacheMode();
  CacheBackendPerformance();
}

// Creating and deleting "entries" on a block-file is something quite frequent
// (after all, almost everything is stored on block files). The operation is
// almost free when the file is empty, but can be expensive if the file gets
// fragmented, or if we have multiple files. This test measures that scenario,
// by using multiple, highly fragmented files.
TEST_F(DiskCachePerfTest, BlockFilesPerformance) {
  ASSERT_TRUE(CleanupCacheDir());

  disk_cache::BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(true));

  const int kNumBlocks = 60000;
  disk_cache::Addr address[kNumBlocks];

  base::PerfTimeLogger timer1("Fill three block-files");

  // Fill up the 32-byte block file (use three files).
  for (int i = 0; i < kNumBlocks; i++) {
    int block_size = base::RandInt(1, 4);
    EXPECT_TRUE(
        files.CreateBlock(disk_cache::RANKINGS, block_size, &address[i]));
  }

  timer1.Done();
  base::PerfTimeLogger timer2("Create and delete blocks");

  for (int i = 0; i < 200000; i++) {
    int block_size = base::RandInt(1, 4);
    int entry = base::RandInt(0, kNumBlocks - 1);

    files.DeleteBlock(address[entry], false);
    EXPECT_TRUE(
        files.CreateBlock(disk_cache::RANKINGS, block_size, &address[entry]));
  }

  timer2.Done();
  base::RunLoop().RunUntilIdle();
}

void VerifyRvAndCallClosure(base::Closure* c, int expect_rv, int rv) {
  EXPECT_EQ(expect_rv, rv);
  c->Run();
}

TEST_F(DiskCachePerfTest, SimpleCacheInitialReadPortion) {
  // A benchmark that aims to measure how much time we take in I/O thread
  // for initial bookkeeping before returning to the caller, and how much
  // after (batched up some). The later portion includes some event loop
  // overhead.
  const int kBatchSize = 100;

  SetSimpleCacheMode();

  InitCache();
  // Write out the entries, and keep their objects around.
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kHeadersSize));
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kBodySize));

  CacheTestFillBuffer(buffer1->data(), kHeadersSize, false);
  CacheTestFillBuffer(buffer2->data(), kBodySize, false);

  disk_cache::Entry* cache_entry[kBatchSize];
  for (int i = 0; i < kBatchSize; ++i) {
    net::TestCompletionCallback cb;
    int rv = cache_->CreateEntry(base::IntToString(i), &cache_entry[i],
                                 cb.callback());
    ASSERT_EQ(net::OK, cb.GetResult(rv));

    rv = cache_entry[i]->WriteData(0, 0, buffer1.get(), kHeadersSize,
                                   cb.callback(), false);
    ASSERT_EQ(kHeadersSize, cb.GetResult(rv));
    rv = cache_entry[i]->WriteData(1, 0, buffer2.get(), kBodySize,
                                   cb.callback(), false);
    ASSERT_EQ(kBodySize, cb.GetResult(rv));
  }

  // Now repeatedly read these, batching up the waiting to try to
  // account for the two portions separately. Note that we need separate entries
  // since we are trying to keep interesting work from being on the delayed-done
  // portion.
  const int kIterations = 50000;

  double elapsed_early = 0.0;
  double elapsed_late = 0.0;

  for (int i = 0; i < kIterations; ++i) {
    base::RunLoop event_loop;
    base::Closure barrier =
        base::BarrierClosure(kBatchSize, event_loop.QuitWhenIdleClosure());
    net::CompletionCallback cb_batch(base::Bind(
        VerifyRvAndCallClosure, base::Unretained(&barrier), kHeadersSize));

    base::ElapsedTimer timer_early;
    for (int e = 0; e < kBatchSize; ++e) {
      int rv =
          cache_entry[e]->ReadData(0, 0, buffer1.get(), kHeadersSize, cb_batch);
      if (rv != net::ERR_IO_PENDING) {
        barrier.Run();
        ASSERT_EQ(kHeadersSize, rv);
      }
    }
    elapsed_early += timer_early.Elapsed().InMillisecondsF();

    base::ElapsedTimer timer_late;
    event_loop.Run();
    elapsed_late += timer_late.Elapsed().InMillisecondsF();
  }

  // Cleanup
  for (int i = 0; i < kBatchSize; ++i)
    cache_entry[i]->Close();

  disk_cache::SimpleBackendImpl::FlushWorkerPoolForTesting();
  base::RunLoop().RunUntilIdle();
  LOG(ERROR) << "Early portion:" << elapsed_early << " ms";
  LOG(ERROR) << "\tPer entry:"
             << 1000 * (elapsed_early / (kIterations * kBatchSize)) << " us";
  LOG(ERROR) << "Event loop portion: " << elapsed_late << " ms";
  LOG(ERROR) << "\tPer entry:"
             << 1000 * (elapsed_late / (kIterations * kBatchSize)) << " us";
}

// Measures how quickly SimpleIndex can compute which entries to evict.
TEST(SimpleIndexPerfTest, EvictionPerformance) {
  const int kEntries = 10000;

  class NoOpDelegate : public disk_cache::SimpleIndexDelegate {
    void DoomEntries(std::vector<uint64_t>* entry_hashes,
                     const net::CompletionCallback& callback) override {}
  };

  NoOpDelegate delegate;
  base::Time start(base::Time::Now());

  double evict_elapsed_ms = 0;
  int iterations = 0;
  while (iterations < 61000) {
    ++iterations;
    disk_cache::SimpleIndex index(/* io_thread = */ nullptr,
                                  /* cleanup_tracker = */ nullptr, &delegate,
                                  net::DISK_CACHE,
                                  /* simple_index_file = */ nullptr);

    // Make sure large enough to not evict on insertion.
    index.SetMaxSize(kEntries * 2);

    for (int i = 0; i < kEntries; ++i) {
      index.InsertEntryForTesting(
          i, disk_cache::EntryMetadata(start + base::TimeDelta::FromSeconds(i),
                                       1u));
    }

    // Trigger an eviction.
    base::ElapsedTimer timer;
    index.SetMaxSize(kEntries);
    index.UpdateEntrySize(0, 1u);
    evict_elapsed_ms += timer.Elapsed().InMillisecondsF();
  }

  LOG(ERROR) << "Average time to evict:" << (evict_elapsed_ms / iterations)
             << "ms";
}

}  // namespace
