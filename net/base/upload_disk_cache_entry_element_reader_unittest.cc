// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_disk_cache_entry_element_reader.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {
namespace {

const int kTestDiskCacheStreamIndex = 0;

const char kDataKey[] = "a key";

const char kData[] = "this is data in a disk cache entry";
const size_t kDataSize = arraysize(kData) - 1;

// A disk_cache::Entry that arbitrarily delays the completion of a read
// operation to allow testing some races without flake. This is particularly
// relevant in this unit test, which uses the always-synchronous MEMORY_CACHE.
class DelayedReadEntry : public disk_cache::Entry {
 public:
  explicit DelayedReadEntry(disk_cache::ScopedEntryPtr entry)
      : entry_(entry.Pass()) {}
  ~DelayedReadEntry() override { EXPECT_FALSE(HasPendingReadCallbacks()); }

  bool HasPendingReadCallbacks() { return !pending_read_callbacks_.empty(); }

  void RunPendingReadCallbacks() {
    std::vector<base::Callback<void(void)>> callbacks;
    pending_read_callbacks_.swap(callbacks);
    for (const auto& callback : callbacks)
      callback.Run();
  }

  // From disk_cache::Entry:
  void Doom() override { entry_->Doom(); }

  void Close() override { delete this; }  // Note this is required by the API.

  std::string GetKey() const override { return entry_->GetKey(); }

  base::Time GetLastUsed() const override { return entry_->GetLastUsed(); }

  base::Time GetLastModified() const override {
    return entry_->GetLastModified();
  }

  int32 GetDataSize(int index) const override {
    return entry_->GetDataSize(index);
  }

  int ReadData(int index,
               int offset,
               IOBuffer* buf,
               int buf_len,
               const CompletionCallback& original_callback) override {
    TestCompletionCallback callback;
    int rv = entry_->ReadData(index, offset, buf, buf_len, callback.callback());
    DCHECK_NE(rv, ERR_IO_PENDING)
        << "Test expects to use a MEMORY_CACHE instance, which is synchronous.";
    pending_read_callbacks_.push_back(base::Bind(original_callback, rv));
    return ERR_IO_PENDING;
  }

  int WriteData(int index,
                int offset,
                IOBuffer* buf,
                int buf_len,
                const CompletionCallback& callback,
                bool truncate) override {
    return entry_->WriteData(index, offset, buf, buf_len, callback, truncate);
  }

  int ReadSparseData(int64 offset,
                     IOBuffer* buf,
                     int buf_len,
                     const CompletionCallback& callback) override {
    return entry_->ReadSparseData(offset, buf, buf_len, callback);
  }

  int WriteSparseData(int64 offset,
                      IOBuffer* buf,
                      int buf_len,
                      const CompletionCallback& callback) override {
    return entry_->WriteSparseData(offset, buf, buf_len, callback);
  }

  int GetAvailableRange(int64 offset,
                        int len,
                        int64* start,
                        const CompletionCallback& callback) override {
    return entry_->GetAvailableRange(offset, len, start, callback);
  }

  bool CouldBeSparse() const override { return entry_->CouldBeSparse(); }

  void CancelSparseIO() override { entry_->CancelSparseIO(); }

  int ReadyForSparseIO(const CompletionCallback& callback) override {
    return entry_->ReadyForSparseIO(callback);
  }

 private:
  disk_cache::ScopedEntryPtr entry_;
  std::vector<base::Callback<void(void)>> pending_read_callbacks_;
};

class UploadDiskCacheEntryElementReaderTest : public PlatformTest {
 public:
  UploadDiskCacheEntryElementReaderTest() {}

  ~UploadDiskCacheEntryElementReaderTest() override {}

  void SetUp() override {
    TestCompletionCallback callback;
    int rv = disk_cache::CreateCacheBackend(
        MEMORY_CACHE, CACHE_BACKEND_DEFAULT, base::FilePath(), 0, false,
        nullptr, nullptr, &cache_, callback.callback());
    ASSERT_EQ(OK, callback.GetResult(rv));

    disk_cache::Entry* tmp_entry = nullptr;
    rv = cache_->CreateEntry(kDataKey, &tmp_entry, callback.callback());
    ASSERT_EQ(OK, callback.GetResult(rv));
    entry_.reset(tmp_entry);

    scoped_refptr<IOBuffer> io_buffer = new WrappedIOBuffer(kData);
    rv = entry_->WriteData(kTestDiskCacheStreamIndex, 0, io_buffer.get(),
                           kDataSize, callback.callback(), false);
    EXPECT_EQ(static_cast<int>(kDataSize), callback.GetResult(rv));
  }

  void set_entry(disk_cache::ScopedEntryPtr entry) { entry_.swap(entry); }
  disk_cache::Entry* entry() { return entry_.get(); }
  disk_cache::ScopedEntryPtr release_entry() { return entry_.Pass(); }

 private:
  scoped_ptr<disk_cache::Backend> cache_;
  disk_cache::ScopedEntryPtr entry_;
};

TEST_F(UploadDiskCacheEntryElementReaderTest, ReadAll) {
  UploadDiskCacheEntryElementReader reader(entry(), kTestDiskCacheStreamIndex,
                                           0, kDataSize);
  EXPECT_EQ(static_cast<uint64_t>(kDataSize), reader.BytesRemaining());

  char read_buffer[kDataSize];
  std::fill(read_buffer, read_buffer + arraysize(read_buffer), '\0');

  scoped_refptr<IOBuffer> io_buffer = new WrappedIOBuffer(read_buffer);
  TestCompletionCallback callback;
  int rv = reader.Read(io_buffer.get(), kDataSize, callback.callback());
  EXPECT_EQ(static_cast<int>(kDataSize), callback.GetResult(rv));
  EXPECT_EQ(0U, reader.BytesRemaining())
      << "Expected a single read of |kDataSize| to retrieve entire entry.";
  EXPECT_EQ(std::string(kData, kDataSize), std::string(read_buffer, kDataSize));
}

TEST_F(UploadDiskCacheEntryElementReaderTest, ReadPartially) {
  UploadDiskCacheEntryElementReader reader(entry(), kTestDiskCacheStreamIndex,
                                           0, kDataSize);
  EXPECT_EQ(static_cast<uint64_t>(kDataSize), reader.BytesRemaining());

  const size_t kReadBuffer1Size = kDataSize / 3;
  char read_buffer1[kReadBuffer1Size];
  std::fill(read_buffer1, read_buffer1 + arraysize(read_buffer1), '\0');

  scoped_refptr<IOBuffer> io_buffer1 = new WrappedIOBuffer(read_buffer1);

  const size_t kReadBuffer2Size = kDataSize - kReadBuffer1Size;
  char read_buffer2[kReadBuffer2Size];
  scoped_refptr<IOBuffer> io_buffer2 = new WrappedIOBuffer(read_buffer2);

  TestCompletionCallback callback;
  int rv = reader.Read(io_buffer1.get(), kReadBuffer1Size, callback.callback());
  EXPECT_EQ(static_cast<int>(kReadBuffer1Size), callback.GetResult(rv));
  EXPECT_EQ(static_cast<uint64_t>(kReadBuffer2Size), reader.BytesRemaining());

  rv = reader.Read(io_buffer2.get(), kReadBuffer2Size, callback.callback());
  EXPECT_EQ(static_cast<int>(kReadBuffer2Size), callback.GetResult(rv));
  EXPECT_EQ(0U, reader.BytesRemaining());

  EXPECT_EQ(std::string(kData, kDataSize),
            std::string(read_buffer1, kReadBuffer1Size) +
                std::string(read_buffer2, kReadBuffer2Size));
}

TEST_F(UploadDiskCacheEntryElementReaderTest, ReadTooMuch) {
  UploadDiskCacheEntryElementReader reader(entry(), kTestDiskCacheStreamIndex,
                                           0, kDataSize);
  EXPECT_EQ(static_cast<uint64_t>(kDataSize), reader.BytesRemaining());

  const size_t kTooLargeSize = kDataSize + kDataSize / 2;

  char read_buffer[kTooLargeSize];
  std::fill(read_buffer, read_buffer + arraysize(read_buffer), '\0');

  scoped_refptr<IOBuffer> io_buffer = new WrappedIOBuffer(read_buffer);
  TestCompletionCallback callback;
  int rv = reader.Read(io_buffer.get(), kTooLargeSize, callback.callback());
  EXPECT_EQ(static_cast<int>(kDataSize), callback.GetResult(rv));
  EXPECT_EQ(0U, reader.BytesRemaining());
  EXPECT_EQ(std::string(kData, kDataSize), std::string(read_buffer, kDataSize));
}

TEST_F(UploadDiskCacheEntryElementReaderTest, ReadAsync) {
  DelayedReadEntry* delayed_read_entry = new DelayedReadEntry(release_entry());
  set_entry(disk_cache::ScopedEntryPtr(delayed_read_entry));

  UploadDiskCacheEntryElementReader reader(entry(), kTestDiskCacheStreamIndex,
                                           0, kDataSize);

  char read_buffer[kDataSize];
  std::fill(read_buffer, read_buffer + arraysize(read_buffer), '\0');

  scoped_refptr<IOBuffer> io_buffer = new WrappedIOBuffer(read_buffer);
  TestCompletionCallback callback;
  int rv = reader.Read(io_buffer.get(), kDataSize, callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(delayed_read_entry->HasPendingReadCallbacks());
  EXPECT_EQ(static_cast<uint64_t>(kDataSize), reader.BytesRemaining());

  delayed_read_entry->RunPendingReadCallbacks();
  EXPECT_EQ(static_cast<int>(kDataSize), callback.GetResult(rv));
  EXPECT_EQ(0U, reader.BytesRemaining())
      << "Expected a single read of |kDataSize| to retrieve entire entry.";
  EXPECT_EQ(std::string(kData, kDataSize), std::string(read_buffer, kDataSize));
}

TEST_F(UploadDiskCacheEntryElementReaderTest, MultipleInit) {
  UploadDiskCacheEntryElementReader reader(entry(), kTestDiskCacheStreamIndex,
                                           0, kDataSize);
  char read_buffer[kDataSize];
  std::fill(read_buffer, read_buffer + arraysize(read_buffer), '\0');

  scoped_refptr<IOBuffer> io_buffer = new WrappedIOBuffer(read_buffer);
  TestCompletionCallback callback;
  int rv = reader.Read(io_buffer.get(), kDataSize, callback.callback());
  EXPECT_EQ(static_cast<int>(kDataSize), callback.GetResult(rv));
  EXPECT_EQ(std::string(kData, kDataSize), std::string(read_buffer, kDataSize));

  rv = reader.Init(callback.callback());
  EXPECT_EQ(OK, callback.GetResult(rv));
  EXPECT_EQ(static_cast<uint64_t>(kDataSize), reader.BytesRemaining());
  rv = reader.Read(io_buffer.get(), kDataSize, callback.callback());
  EXPECT_EQ(static_cast<int>(kDataSize), callback.GetResult(rv));
  EXPECT_EQ(std::string(kData, kDataSize), std::string(read_buffer, kDataSize));
}

TEST_F(UploadDiskCacheEntryElementReaderTest, InitDuringAsyncOperation) {
  DelayedReadEntry* delayed_read_entry = new DelayedReadEntry(release_entry());
  set_entry(disk_cache::ScopedEntryPtr(delayed_read_entry));

  UploadDiskCacheEntryElementReader reader(entry(), kTestDiskCacheStreamIndex,
                                           0, kDataSize);
  char read_buffer[kDataSize];
  std::fill(read_buffer, read_buffer + arraysize(read_buffer), '\0');

  scoped_refptr<IOBuffer> io_buffer = new WrappedIOBuffer(read_buffer);
  TestCompletionCallback read_callback;
  int rv = reader.Read(io_buffer.get(), kDataSize, read_callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(delayed_read_entry->HasPendingReadCallbacks());
  EXPECT_EQ(static_cast<uint64_t>(kDataSize), reader.BytesRemaining());

  TestCompletionCallback init_callback;
  rv = reader.Init(init_callback.callback());
  EXPECT_EQ(OK, init_callback.GetResult(rv));

  delayed_read_entry->RunPendingReadCallbacks();
  EXPECT_FALSE(delayed_read_entry->HasPendingReadCallbacks());
  EXPECT_EQ(static_cast<uint64_t>(kDataSize), reader.BytesRemaining());

  char read_buffer2[kDataSize];
  std::fill(read_buffer2, read_buffer2 + arraysize(read_buffer2), '\0');
  scoped_refptr<IOBuffer> io_buffer2 = new WrappedIOBuffer(read_buffer2);
  TestCompletionCallback read_callback2;
  rv = reader.Read(io_buffer2.get(), kDataSize, read_callback2.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(delayed_read_entry->HasPendingReadCallbacks());
  EXPECT_EQ(static_cast<uint64_t>(kDataSize), reader.BytesRemaining());

  delayed_read_entry->RunPendingReadCallbacks();
  EXPECT_FALSE(delayed_read_entry->HasPendingReadCallbacks());
  read_callback2.WaitForResult();  // Succeeds if this does not deadlock.
  EXPECT_EQ(std::string(kData, kDataSize),
            std::string(read_buffer2, kDataSize));
}

TEST_F(UploadDiskCacheEntryElementReaderTest, Range) {
  const size_t kOffset = kDataSize / 4;
  const size_t kLength = kDataSize / 3;

  UploadDiskCacheEntryElementReader reader(entry(), kTestDiskCacheStreamIndex,
                                           kOffset, kLength);
  EXPECT_EQ(static_cast<uint64_t>(kLength), reader.BytesRemaining());

  char read_buffer[kLength];
  std::fill(read_buffer, read_buffer + arraysize(read_buffer), '\0');

  scoped_refptr<IOBuffer> io_buffer = new WrappedIOBuffer(read_buffer);
  TestCompletionCallback callback;
  int rv = reader.Read(io_buffer.get(), kLength, callback.callback());
  EXPECT_EQ(static_cast<int>(kLength), callback.GetResult(rv));
  EXPECT_EQ(0U, reader.BytesRemaining());
  EXPECT_EQ(std::string(kData + kOffset, kLength),
            std::string(read_buffer, kLength));
}

}  // namespace
}  // namespace net
