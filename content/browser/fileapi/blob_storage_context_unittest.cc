// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_context.h"

#include <stdint.h>

#include <limits>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "content/browser/fileapi/blob_storage_host.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/browser/blob/blob_data_snapshot.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::BlobDataBuilder;
using storage::BlobDataHandle;
using storage::BlobDataItem;
using storage::BlobDataSnapshot;
using storage::BlobStorageContext;
using storage::DataElement;

namespace content {
namespace {

const int kTestDiskCacheStreamIndex = 0;

// Our disk cache tests don't need a real data handle since the tests themselves
// scope the disk cache and entries.
class EmptyDataHandle : public storage::BlobDataBuilder::DataHandle {
 private:
  ~EmptyDataHandle() override {}
};

scoped_ptr<disk_cache::Backend> CreateInMemoryDiskCache() {
  scoped_ptr<disk_cache::Backend> cache;
  net::TestCompletionCallback callback;
  int rv = disk_cache::CreateCacheBackend(net::MEMORY_CACHE,
                                          net::CACHE_BACKEND_DEFAULT,
                                          base::FilePath(), 0,
                                          false, nullptr, nullptr, &cache,
                                          callback.callback());
  EXPECT_EQ(net::OK, callback.GetResult(rv));

  return cache;
}

disk_cache::ScopedEntryPtr CreateDiskCacheEntry(disk_cache::Backend* cache,
                                                const char* key,
                                                const std::string& data) {
  disk_cache::Entry* temp_entry = nullptr;
  net::TestCompletionCallback callback;
  int rv = cache->CreateEntry(key, &temp_entry, callback.callback());
  if (callback.GetResult(rv) != net::OK)
    return nullptr;
  disk_cache::ScopedEntryPtr entry(temp_entry);

  scoped_refptr<net::StringIOBuffer> iobuffer = new net::StringIOBuffer(data);
  rv = entry->WriteData(kTestDiskCacheStreamIndex, 0, iobuffer.get(),
                        iobuffer->size(), callback.callback(), false);
  EXPECT_EQ(static_cast<int>(data.size()), callback.GetResult(rv));
  return entry;
}

void SetupBasicBlob(BlobStorageHost* host, const std::string& id) {
  EXPECT_TRUE(host->StartBuildingBlob(id));
  DataElement item;
  item.SetToBytes("1", 1);
  EXPECT_TRUE(host->AppendBlobDataItem(id, item));
  EXPECT_TRUE(host->FinishBuildingBlob(id, "text/plain"));
  EXPECT_FALSE(host->StartBuildingBlob(id));
}

}  // namespace

TEST(BlobStorageContextTest, IncrementDecrementRef) {
  BlobStorageContext context;
  BlobStorageHost host(&context);
  base::MessageLoop fake_io_message_loop;

  // Build up a basic blob.
  const std::string kId("id");
  SetupBasicBlob(&host, kId);

  // Make sure it's there, finish building implies a ref of one.
  scoped_ptr<BlobDataHandle> blob_data_handle;
  blob_data_handle = context.GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  // Make sure its still there after inc/dec.
  EXPECT_TRUE(host.IncrementBlobRefCount(kId));
  EXPECT_TRUE(host.DecrementBlobRefCount(kId));
  blob_data_handle = context.GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  // Make sure it goes away in the end.
  EXPECT_TRUE(host.DecrementBlobRefCount(kId));
  blob_data_handle = context.GetBlobDataFromUUID(kId);
  EXPECT_FALSE(blob_data_handle);
  EXPECT_FALSE(host.DecrementBlobRefCount(kId));
  EXPECT_FALSE(host.IncrementBlobRefCount(kId));
}

TEST(BlobStorageContextTest, CancelBuildingBlob) {
  BlobStorageContext context;
  BlobStorageHost host(&context);
  base::MessageLoop fake_io_message_loop;

  // Build up a basic blob.
  const std::string kId("id");
  EXPECT_TRUE(host.StartBuildingBlob(kId));
  DataElement item;
  item.SetToBytes("1", 1);
  EXPECT_TRUE(host.AppendBlobDataItem(kId, item));
  EXPECT_TRUE(host.CancelBuildingBlob(kId));
  EXPECT_FALSE(host.FinishBuildingBlob(kId, "text/plain"));
  EXPECT_TRUE(host.StartBuildingBlob(kId));
}

TEST(BlobStorageContextTest, BlobDataHandle) {
  BlobStorageContext context;
  BlobStorageHost host(&context);
  base::MessageLoop fake_io_message_loop;

  // Build up a basic blob.
  const std::string kId("id");
  SetupBasicBlob(&host, kId);

  // Get a handle to it.
  scoped_ptr<BlobDataHandle> blob_data_handle =
      context.GetBlobDataFromUUID(kId);
  EXPECT_TRUE(blob_data_handle);

  // Drop the host's ref to it.
  EXPECT_TRUE(host.DecrementBlobRefCount(kId));

  // Should still be there due to the handle.
  scoped_ptr<BlobDataHandle> another_handle =
      context.GetBlobDataFromUUID(kId);
  EXPECT_TRUE(another_handle);

  // Should disappear after dropping both handles.
  blob_data_handle.reset();
  another_handle.reset();
  base::RunLoop().RunUntilIdle();

  blob_data_handle = context.GetBlobDataFromUUID(kId);
  EXPECT_FALSE(blob_data_handle);
}

TEST(BlobStorageContextTest, MemoryUsage) {
  const std::string kId1("id1");
  const std::string kId2("id2");

  base::MessageLoop fake_io_message_loop;

  BlobDataBuilder builder1(kId1);
  BlobDataBuilder builder2(kId2);
  builder1.AppendData("Data1Data2");
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);
  builder2.AppendBlob(kId1);

  BlobStorageContext context;
  EXPECT_EQ(0lu, context.memory_usage());

  scoped_ptr<BlobDataHandle> blob_data_handle =
      context.AddFinishedBlob(&builder1);
  EXPECT_EQ(10lu, context.memory_usage());
  scoped_ptr<BlobDataHandle> blob_data_handle2 =
      context.AddFinishedBlob(&builder2);
  EXPECT_EQ(10lu, context.memory_usage());

  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(10lu, context.memory_usage());
  blob_data_handle2.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0lu, context.memory_usage());
}

TEST(BlobStorageContextTest, AddFinishedBlob) {
  const std::string kId1("id1");
  const std::string kId2("id12");
  const std::string kId2Prime("id2.prime");
  const std::string kId3("id3");
  const std::string kId3Prime("id3.prime");

  base::MessageLoop fake_io_message_loop;

  BlobDataBuilder builder1(kId1);
  BlobDataBuilder builder2(kId2);
  BlobDataBuilder canonicalized_blob_data2(kId2Prime);
  builder1.AppendData("Data1Data2");
  builder2.AppendBlob(kId1, 5, 5);
  builder2.AppendData(" is the best");
  canonicalized_blob_data2.AppendData("Data2");
  canonicalized_blob_data2.AppendData(" is the best");

  BlobStorageContext context;

  scoped_ptr<BlobDataHandle> blob_data_handle =
      context.AddFinishedBlob(&builder1);
  scoped_ptr<BlobDataHandle> blob_data_handle2 =
      context.AddFinishedBlob(&builder2);

  ASSERT_TRUE(blob_data_handle);
  ASSERT_TRUE(blob_data_handle2);
  scoped_ptr<BlobDataSnapshot> data1 = blob_data_handle->CreateSnapshot();
  scoped_ptr<BlobDataSnapshot> data2 = blob_data_handle2->CreateSnapshot();
  EXPECT_EQ(*data1, builder1);
  EXPECT_EQ(*data2, canonicalized_blob_data2);
  blob_data_handle.reset();
  data2.reset();

  base::RunLoop().RunUntilIdle();

  blob_data_handle = context.GetBlobDataFromUUID(kId1);
  EXPECT_FALSE(blob_data_handle);
  EXPECT_TRUE(blob_data_handle2);
  data2 = blob_data_handle2->CreateSnapshot();
  EXPECT_EQ(*data2, canonicalized_blob_data2);

  // Test shared elements stick around.
  BlobDataBuilder builder3(kId3);
  builder3.AppendBlob(kId2);
  builder3.AppendBlob(kId2);
  scoped_ptr<BlobDataHandle> blob_data_handle3 =
      context.AddFinishedBlob(&builder3);
  blob_data_handle2.reset();
  base::RunLoop().RunUntilIdle();

  blob_data_handle2 = context.GetBlobDataFromUUID(kId2);
  EXPECT_FALSE(blob_data_handle2);
  EXPECT_TRUE(blob_data_handle3);
  scoped_ptr<BlobDataSnapshot> data3 = blob_data_handle3->CreateSnapshot();

  BlobDataBuilder canonicalized_blob_data3(kId3Prime);
  canonicalized_blob_data3.AppendData("Data2");
  canonicalized_blob_data3.AppendData(" is the best");
  canonicalized_blob_data3.AppendData("Data2");
  canonicalized_blob_data3.AppendData(" is the best");
  EXPECT_EQ(*data3, canonicalized_blob_data3);

  blob_data_handle.reset();
  blob_data_handle2.reset();
  blob_data_handle3.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(BlobStorageContextTest, AddFinishedBlob_LargeOffset) {
  // A value which does not fit in a 4-byte data type. Used to confirm that
  // large values are supported on 32-bit Chromium builds. Regression test for:
  // crbug.com/458122.
  const uint64_t kLargeSize = std::numeric_limits<uint64_t>::max();

  const uint64_t kBlobLength = 5;
  const std::string kId1("id1");
  const std::string kId2("id2");
  base::MessageLoop fake_io_message_loop;

  BlobDataBuilder builder1(kId1);
  builder1.AppendFileSystemFile(GURL(), 0, kLargeSize, base::Time::Now());

  BlobDataBuilder builder2(kId2);
  builder2.AppendBlob(kId1, kLargeSize - kBlobLength, kBlobLength);

  BlobStorageContext context;
  scoped_ptr<BlobDataHandle> blob_data_handle1 =
      context.AddFinishedBlob(&builder1);
  scoped_ptr<BlobDataHandle> blob_data_handle2 =
      context.AddFinishedBlob(&builder2);

  ASSERT_TRUE(blob_data_handle1);
  ASSERT_TRUE(blob_data_handle2);
  scoped_ptr<BlobDataSnapshot> data = blob_data_handle2->CreateSnapshot();
  ASSERT_EQ(1u, data->items().size());
  const scoped_refptr<BlobDataItem> item = data->items()[0];
  EXPECT_EQ(kLargeSize - kBlobLength, item->offset());
  EXPECT_EQ(kBlobLength, item->length());

  blob_data_handle1.reset();
  blob_data_handle2.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(BlobStorageContextTest, BuildDiskCacheBlob) {
  base::MessageLoop fake_io_message_loop;
  scoped_refptr<BlobDataBuilder::DataHandle>
      data_handle = new EmptyDataHandle();

  {
    scoped_ptr<BlobStorageContext> context(new BlobStorageContext);
    BlobStorageHost host(context.get());

    scoped_ptr<disk_cache::Backend> cache = CreateInMemoryDiskCache();
    ASSERT_TRUE(cache);

    const std::string kTestBlobData = "Test Blob Data";
    disk_cache::ScopedEntryPtr entry =
        CreateDiskCacheEntry(cache.get(), "test entry", kTestBlobData);

    const std::string kId1Prime("id1.prime");
    BlobDataBuilder canonicalized_blob_data(kId1Prime);
    canonicalized_blob_data.AppendData(kTestBlobData.c_str());

    const std::string kId1("id1");
    BlobDataBuilder builder(kId1);

    builder.AppendDiskCacheEntry(
        data_handle, entry.get(), kTestDiskCacheStreamIndex);

    scoped_ptr<BlobDataHandle> blob_data_handle =
        context->AddFinishedBlob(&builder);
    scoped_ptr<BlobDataSnapshot> data = blob_data_handle->CreateSnapshot();
    EXPECT_EQ(*data, builder);
    EXPECT_FALSE(data_handle->HasOneRef())
        << "Data handle was destructed while context and builder still exist.";
  }
  EXPECT_TRUE(data_handle->HasOneRef())
      << "Data handle was not destructed along with blob storage context.";
  base::RunLoop().RunUntilIdle();
}

TEST(BlobStorageContextTest, CompoundBlobs) {
  const std::string kId1("id1");
  const std::string kId2("id2");
  const std::string kId3("id3");
  const std::string kId2Prime("id2.prime");

  base::MessageLoop fake_io_message_loop;

  // Setup a set of blob data for testing.
  base::Time time1, time2;
  base::Time::FromString("Tue, 15 Nov 1994, 12:45:26 GMT", &time1);
  base::Time::FromString("Mon, 14 Nov 1994, 11:30:49 GMT", &time2);

  BlobDataBuilder blob_data1(kId1);
  blob_data1.AppendData("Data1");
  blob_data1.AppendData("Data2");
  blob_data1.AppendFile(base::FilePath(FILE_PATH_LITERAL("File1.txt")), 10,
                        1024, time1);

  BlobDataBuilder blob_data2(kId2);
  blob_data2.AppendData("Data3");
  blob_data2.AppendBlob(kId1, 8, 100);
  blob_data2.AppendFile(base::FilePath(FILE_PATH_LITERAL("File2.txt")), 0, 20,
                        time2);

  BlobDataBuilder blob_data3(kId3);
  blob_data3.AppendData("Data4");
  scoped_ptr<disk_cache::Backend> cache = CreateInMemoryDiskCache();
  ASSERT_TRUE(cache);
  disk_cache::ScopedEntryPtr disk_cache_entry =
      CreateDiskCacheEntry(cache.get(), "another key", "Data5");
  blob_data3.AppendDiskCacheEntry(new EmptyDataHandle(), disk_cache_entry.get(),
                                  kTestDiskCacheStreamIndex);

  BlobDataBuilder canonicalized_blob_data2(kId2Prime);
  canonicalized_blob_data2.AppendData("Data3");
  canonicalized_blob_data2.AppendData("a2___", 2);
  canonicalized_blob_data2.AppendFile(
      base::FilePath(FILE_PATH_LITERAL("File1.txt")), 10, 98, time1);
  canonicalized_blob_data2.AppendFile(
      base::FilePath(FILE_PATH_LITERAL("File2.txt")), 0, 20, time2);

  BlobStorageContext context;
  scoped_ptr<BlobDataHandle> blob_data_handle;

  // Test a blob referring to only data and a file.
  blob_data_handle = context.AddFinishedBlob(&blob_data1);

  ASSERT_TRUE(blob_data_handle);
  scoped_ptr<BlobDataSnapshot> data = blob_data_handle->CreateSnapshot();
  ASSERT_TRUE(blob_data_handle);
  EXPECT_EQ(*data, blob_data1);

  // Test a blob composed in part with another blob.
  blob_data_handle = context.AddFinishedBlob(&blob_data2);
  data = blob_data_handle->CreateSnapshot();
  ASSERT_TRUE(blob_data_handle);
  ASSERT_TRUE(data);
  EXPECT_EQ(*data, canonicalized_blob_data2);

  // Test a blob referring to only data and a disk cache entry.
  blob_data_handle = context.AddFinishedBlob(&blob_data3);
  data = blob_data_handle->CreateSnapshot();
  ASSERT_TRUE(blob_data_handle);
  EXPECT_EQ(*data, blob_data3);

  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(BlobStorageContextTest, PublicBlobUrls) {
  BlobStorageContext context;
  BlobStorageHost host(&context);
  base::MessageLoop fake_io_message_loop;

  // Build up a basic blob.
  const std::string kId("id");
  SetupBasicBlob(&host, kId);

  // Now register a url for that blob.
  GURL kUrl("blob:id");
  EXPECT_TRUE(host.RegisterPublicBlobURL(kUrl, kId));
  scoped_ptr<BlobDataHandle> blob_data_handle =
      context.GetBlobDataFromPublicURL(kUrl);
  ASSERT_TRUE(blob_data_handle.get());
  EXPECT_EQ(kId, blob_data_handle->uuid());
  scoped_ptr<BlobDataSnapshot> data = blob_data_handle->CreateSnapshot();
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  // The url registration should keep the blob alive even after
  // explicit references are dropped.
  EXPECT_TRUE(host.DecrementBlobRefCount(kId));
  blob_data_handle = context.GetBlobDataFromPublicURL(kUrl);
  EXPECT_TRUE(blob_data_handle);
  blob_data_handle.reset();
  base::RunLoop().RunUntilIdle();

  // Finally get rid of the url registration and the blob.
  EXPECT_TRUE(host.RevokePublicBlobURL(kUrl));
  blob_data_handle = context.GetBlobDataFromPublicURL(kUrl);
  EXPECT_TRUE(!blob_data_handle.get());
  EXPECT_FALSE(host.RevokePublicBlobURL(kUrl));
}

TEST(BlobStorageContextTest, HostCleanup) {
  BlobStorageContext context;
  scoped_ptr<BlobStorageHost> host(new BlobStorageHost(&context));
  base::MessageLoop fake_io_message_loop;

  // Build up a basic blob and register a url
  const std::string kId("id");
  GURL kUrl("blob:id");
  SetupBasicBlob(host.get(), kId);
  EXPECT_TRUE(host->RegisterPublicBlobURL(kUrl, kId));

  // All should disappear upon host deletion.
  host.reset();
  scoped_ptr<BlobDataHandle> handle = context.GetBlobDataFromPublicURL(kUrl);
  EXPECT_TRUE(!handle.get());
  handle = context.GetBlobDataFromUUID(kId);
  EXPECT_TRUE(!handle.get());
}

TEST(BlobStorageContextTest, EarlyContextDeletion) {
  scoped_ptr<BlobStorageContext> context(new BlobStorageContext);
  BlobStorageHost host(context.get());
  base::MessageLoop fake_io_message_loop;

  // Deleting the context should not induce crashes.
  context.reset();

  const std::string kId("id");
  GURL kUrl("blob:id");
  EXPECT_FALSE(host.StartBuildingBlob(kId));
  DataElement item;
  item.SetToBytes("1", 1);
  EXPECT_FALSE(host.AppendBlobDataItem(kId, item));
  EXPECT_FALSE(host.FinishBuildingBlob(kId, "text/plain"));
  EXPECT_FALSE(host.RegisterPublicBlobURL(kUrl, kId));
  EXPECT_FALSE(host.IncrementBlobRefCount(kId));
  EXPECT_FALSE(host.DecrementBlobRefCount(kId));
  EXPECT_FALSE(host.RevokePublicBlobURL(kUrl));
}

// TODO(michaeln): tests for the depcrecated url stuff

}  // namespace content
