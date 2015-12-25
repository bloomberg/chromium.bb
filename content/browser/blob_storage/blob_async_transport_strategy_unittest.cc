// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_async_transport_strategy.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {

const char kNewUUID[] = "newUUID";
const base::FilePath kFuturePopulatingFilePath = base::FilePath::FromUTF8Unsafe(
    std::string(BlobDataBuilder::kAppendFutureFileTemporaryFileName));
const char kFakeBlobUUID[] = "fakeBlob";

void AddMemoryItem(size_t length, std::vector<DataElement>* out) {
  DataElement bytes;
  bytes.SetToBytesDescription(length);
  out->push_back(bytes);
}

void AddShortcutMemoryItem(size_t length, std::vector<DataElement>* out) {
  DataElement bytes;
  bytes.SetToAllocatedBytes(length);
  for (size_t i = 0; i < length; i++) {
    bytes.mutable_bytes()[i] = static_cast<char>(i);
  }
  out->push_back(bytes);
}

void AddBlobItem(std::vector<DataElement>* out) {
  DataElement blob;
  blob.SetToBlob(kFakeBlobUUID);
  out->push_back(blob);
}

TEST(BlobAsyncTransportStrategyTest, TestNoMemoryItems) {
  BlobAsyncTransportStrategy strategy;
  std::vector<DataElement> infos;

  // Here we test that we don't do any requests when there are no memory items.
  AddBlobItem(&infos);
  AddBlobItem(&infos);
  AddBlobItem(&infos);
  strategy.Initialize(100,   // max_ipc_memory_size
                      200,   // max_shared_memory_size
                      400,   // max_file_size
                      5000,  // disk_space_left
                      100,   // memory_available
                      kNewUUID, infos);

  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_NONE, strategy.error());

  EXPECT_EQ(0u, strategy.handle_sizes().size());
  EXPECT_EQ(0u, strategy.requests().size());

  BlobDataBuilder builder(kNewUUID);
  builder.AppendBlob(kFakeBlobUUID);
  builder.AppendBlob(kFakeBlobUUID);
  builder.AppendBlob(kFakeBlobUUID);
  EXPECT_EQ(builder, *strategy.blob_builder());
}

TEST(BlobAsyncTransportStrategyTest, TestLargeBlockToFile) {
  BlobAsyncTransportStrategy strategy;
  std::vector<DataElement> infos;

  // Here we test our size > max_blob_in_memory_size (100),
  // and we save to one file. (size < max_file_size)
  AddMemoryItem(305, &infos);
  strategy.Initialize(100,   // max_ipc_memory_size
                      200,   // max_shared_memory_size
                      400,   // max_file_size
                      5000,  // disk_space_left
                      100,   // memory_available
                      kNewUUID, infos);

  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_NONE, strategy.error());

  EXPECT_EQ(1u, strategy.handle_sizes().size());
  EXPECT_EQ(305ul, strategy.handle_sizes().at(0));
  EXPECT_EQ(1u, strategy.requests().size());

  auto& memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(0u, 0u, 0ull, 305ull, 0u, 0ull),
      memory_item_request.message);

  BlobDataBuilder builder(kNewUUID);
  builder.AppendFile(kFuturePopulatingFilePath, 0, 305,
                     base::Time::FromDoubleT(0));
  EXPECT_EQ(builder, *strategy.blob_builder());
}

TEST(BlobAsyncTransportStrategyTest, TestLargeBlockToFiles) {
  BlobAsyncTransportStrategy strategy;
  std::vector<DataElement> infos;

  // Here we test our size > max_blob_in_memory_size (300),
  // and we save 3 files. (size > max_file_size)
  AddMemoryItem(1000, &infos);
  strategy.Initialize(100,   // max_ipc_memory_size
                      200,   // max_shared_memory_size
                      400,   // max_file_size
                      5000,  // disk_space_left
                      100,   // memory_available
                      kNewUUID, infos);
  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_NONE, strategy.error());

  EXPECT_EQ(3u, strategy.handle_sizes().size());
  EXPECT_EQ(400ul, strategy.handle_sizes().at(0));
  EXPECT_EQ(400ul, strategy.handle_sizes().at(1));
  EXPECT_EQ(200ul, strategy.handle_sizes().at(2));
  EXPECT_EQ(3u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(0u, 0u, 0ull, 400ull, 0u, 0ull),
      memory_item_request.message);

  memory_item_request = strategy.requests().at(1);
  EXPECT_EQ(1u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(1u, 0u, 400ull, 400ull, 1u, 0ull),
      memory_item_request.message);

  memory_item_request = strategy.requests().at(2);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(2u, 0u, 800ull, 200ull, 2u, 0ull),
      memory_item_request.message);

  BlobDataBuilder builder(kNewUUID);
  builder.AppendFile(kFuturePopulatingFilePath, 0, 400,
                     base::Time::FromDoubleT(0));
  builder.AppendFile(kFuturePopulatingFilePath, 0, 400,
                     base::Time::FromDoubleT(0));
  builder.AppendFile(kFuturePopulatingFilePath, 0, 200,
                     base::Time::FromDoubleT(0));
  EXPECT_EQ(builder, *strategy.blob_builder());
}

TEST(BlobAsyncTransportStrategyTest, TestLargeBlocksConsolidatingInFiles) {
  BlobAsyncTransportStrategy strategy;
  std::vector<DataElement> infos;

  // We should have 3 storage items for the memory, two files, 400 each.
  // We end up with storage items:
  // 1: File A, 300MB
  // 2: Blob
  // 3: File A, 100MB (300MB offset)
  // 4: File B, 400MB
  AddMemoryItem(300, &infos);
  AddBlobItem(&infos);
  AddMemoryItem(500, &infos);

  strategy.Initialize(100,   // max_ipc_memory_size
                      200,   // max_shared_memory_size
                      400,   // max_file_size
                      5000,  // disk_space_left
                      100,   // memory_available
                      kNewUUID, infos);
  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_NONE, strategy.error());

  EXPECT_EQ(2u, strategy.handle_sizes().size());
  EXPECT_EQ(400ul, strategy.handle_sizes().at(0));
  EXPECT_EQ(400ul, strategy.handle_sizes().at(1));
  EXPECT_EQ(3u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(0u, 0u, 0ull, 300ull, 0u, 0ull),
      memory_item_request.message);

  memory_item_request = strategy.requests().at(1);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(1u, 2u, 0ull, 100ull, 0u, 300ull),
      memory_item_request.message);

  memory_item_request = strategy.requests().at(2);
  EXPECT_EQ(3u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(
      BlobItemBytesRequest::CreateFileRequest(2u, 2u, 100ull, 400ull, 1u, 0ull),
      memory_item_request.message);
}

TEST(BlobAsyncTransportStrategyTest, TestSharedMemorySegmentation) {
  BlobAsyncTransportStrategy strategy;
  std::vector<DataElement> infos;

  // For transport we should have 3 shared memories, and then storage in 3
  // browser items.
  // (size > max_shared_memory_size and size < max_blob_in_memory_size
  AddMemoryItem(500, &infos);
  strategy.Initialize(100,   // max_ipc_memory_size
                      200,   // max_shared_memory_size
                      300,   // max_file_size
                      5000,  // disk_space_left
                      500,   // memory_available
                      kNewUUID, infos);
  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_NONE, strategy.error());

  EXPECT_EQ(3u, strategy.handle_sizes().size());
  EXPECT_EQ(200u, strategy.handle_sizes().at(0));
  EXPECT_EQ(200u, strategy.handle_sizes().at(1));
  EXPECT_EQ(100u, strategy.handle_sizes().at(2));
  EXPECT_EQ(3u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(0u, 0u, 0ull,
                                                            200ull, 0u, 0ull),
            memory_item_request.message);

  memory_item_request = strategy.requests().at(1);
  EXPECT_EQ(1u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(1u, 0u, 200ull,
                                                            200ull, 1u, 0ull),
            memory_item_request.message);

  memory_item_request = strategy.requests().at(2);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(2u, 0u, 400ull,
                                                            100ull, 2u, 0ull),
            memory_item_request.message);

  BlobDataBuilder builder(kNewUUID);
  builder.AppendFutureData(200);
  builder.AppendFutureData(200);
  builder.AppendFutureData(100);

  EXPECT_EQ(builder, *strategy.blob_builder());
}

TEST(BlobAsyncTransportStrategyTest, TestSharedMemorySegmentationAndStorage) {
  BlobAsyncTransportStrategy strategy;
  std::vector<DataElement> infos;

  // For transport, we should have 2 shared memories, where the first one
  // have half 0 and half 3, and then the last one has half 3.
  //
  // For storage, we should have 3 browser items that match the pre-transport
  // version:
  // 1: Bytes 100MB
  // 2: Blob
  // 3: Bytes 200MB
  AddShortcutMemoryItem(100, &infos);  // should have no behavior change
  AddBlobItem(&infos);
  AddMemoryItem(200, &infos);

  strategy.Initialize(100,   // max_ipc_memory_size
                      200,   // max_shared_memory_size
                      300,   // max_file_size
                      5000,  // disk_space_left
                      300,   // memory_available
                      kNewUUID, infos);
  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_NONE, strategy.error());

  EXPECT_EQ(2u, strategy.handle_sizes().size());
  EXPECT_EQ(200u, strategy.handle_sizes().at(0));
  EXPECT_EQ(100u, strategy.handle_sizes().at(1));
  EXPECT_EQ(3u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(0u, 0u, 0ull,
                                                            100ull, 0u, 0ull),
            memory_item_request.message);

  memory_item_request = strategy.requests().at(1);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(1u, 2u, 0ull,
                                                            100ull, 0u, 100ull),
            memory_item_request.message);

  memory_item_request = strategy.requests().at(2);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(100u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(2u, 2u, 100ull,
                                                            100ull, 1u, 0ull),
            memory_item_request.message);

  BlobDataBuilder builder(kNewUUID);
  builder.AppendFutureData(100);
  builder.AppendBlob(kFakeBlobUUID);
  builder.AppendFutureData(200);

  EXPECT_EQ(builder, *strategy.blob_builder());
}

TEST(BlobAsyncTransportStrategyTest, TestTooLarge) {
  BlobAsyncTransportStrategy strategy;
  std::vector<DataElement> infos;

  // Our item is too large for disk, so error out.
  AddMemoryItem(5001, &infos);

  strategy.Initialize(100,   // max_ipc_memory_size
                      200,   // max_shared_memory_size
                      400,   // max_file_size
                      5000,  // disk_space_left
                      100,   // memory_left
                      kNewUUID, infos);

  EXPECT_EQ(0u, strategy.handle_sizes().size());
  EXPECT_EQ(0u, strategy.handle_sizes().size());
  EXPECT_EQ(0u, strategy.requests().size());
  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_TOO_LARGE, strategy.error());
}

TEST(BlobAsyncTransportStrategyTest, TestNoDisk) {
  BlobAsyncTransportStrategy strategy;
  std::vector<DataElement> infos;

  // Our item is too large for memory, and we are in no_disk mode (incognito)
  AddMemoryItem(301, &infos);

  strategy.Initialize(100,  // max_ipc_memory_size
                      200,  // max_shared_memory_size
                      400,  // max_file_size
                      0,    // disk_space_left
                      300,  // memory_available
                      kNewUUID, infos);

  EXPECT_EQ(0u, strategy.handle_sizes().size());
  EXPECT_EQ(0u, strategy.handle_sizes().size());
  EXPECT_EQ(0u, strategy.requests().size());
  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_TOO_LARGE, strategy.error());
}

TEST(BlobAsyncTransportStrategyTest, TestSimpleIPC) {
  // Test simple IPC strategy, where size < max_ipc_memory_size and we have
  // just one item.
  std::vector<DataElement> infos;
  BlobAsyncTransportStrategy strategy;
  AddMemoryItem(10, &infos);
  AddBlobItem(&infos);

  strategy.Initialize(100,   // max_ipc_memory_size
                      200,   // max_shared_memory_size
                      400,   // max_file_size
                      5000,  // disk_space_left
                      100,   // memory_left
                      kNewUUID, infos);
  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_NONE, strategy.error());

  ASSERT_EQ(1u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateIPCRequest(0u, 0u, 0ull, 10ull),
            memory_item_request.message);
}

TEST(BlobAsyncTransportStrategyTest, TestMultipleIPC) {
  // Same as above, but with 2 items and a blob in-between.
  std::vector<DataElement> infos;
  BlobAsyncTransportStrategy strategy;
  infos.clear();
  AddShortcutMemoryItem(10, &infos);  // should have no behavior change
  AddBlobItem(&infos);
  AddMemoryItem(80, &infos);

  strategy.Initialize(100,   // max_ipc_memory_size
                      200,   // max_shared_memory_size
                      400,   // max_file_size
                      5000,  // disk_space_left
                      100,   // memory_left
                      kNewUUID, infos);
  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_NONE, strategy.error());

  ASSERT_EQ(2u, strategy.requests().size());

  auto memory_item_request = strategy.requests().at(0);
  EXPECT_EQ(0u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateIPCRequest(0u, 0u, 0ull, 10ull),
            memory_item_request.message);

  memory_item_request = strategy.requests().at(1);
  EXPECT_EQ(2u, memory_item_request.browser_item_index);
  EXPECT_EQ(0u, memory_item_request.browser_item_offset);
  EXPECT_EQ(BlobItemBytesRequest::CreateIPCRequest(1u, 2u, 0ull, 80ull),
            memory_item_request.message);

  // We still populate future data, as the strategy assumes we will be
  // requesting the data.
  BlobDataBuilder builder(kNewUUID);
  builder.AppendFutureData(10);
  builder.AppendBlob(kFakeBlobUUID);
  builder.AppendFutureData(80);

  EXPECT_EQ(builder, *strategy.blob_builder());
}

TEST(BlobAsyncTransportStrategyTest, Shortcut) {
  std::vector<DataElement> infos;
  AddMemoryItem(100, &infos);
  AddBlobItem(&infos);
  EXPECT_FALSE(BlobAsyncTransportStrategy::ShouldBeShortcut(infos, 200));

  infos.clear();
  AddShortcutMemoryItem(100, &infos);
  AddBlobItem(&infos);
  EXPECT_TRUE(BlobAsyncTransportStrategy::ShouldBeShortcut(infos, 200));

  infos.clear();
  AddShortcutMemoryItem(100, &infos);
  EXPECT_FALSE(BlobAsyncTransportStrategy::ShouldBeShortcut(infos, 99));
}
}  // namespace

TEST(BlobAsyncTransportStrategyTest, TestInvalidParams) {
  std::vector<DataElement> infos;
  // In order to test uin64_t overflow, we would need to have an array with more
  // than size_t entries (for 32 byte stuff). So this would only happen if the
  // IPC was malformed. We instead have to friend this test from DataElement so
  // we can modify the length to be > size_t.

  // Test uint64_t overflow.
  BlobAsyncTransportStrategy strategy;
  AddMemoryItem(1, &infos);
  AddMemoryItem(1, &infos);
  infos.back().length_ = std::numeric_limits<uint64_t>::max();
  strategy.Initialize(100,   // max_ipc_memory_size
                      200,   // max_shared_memory_size
                      400,   // max_file_size
                      5000,  // disk_space_left
                      100,   // memory_left
                      kNewUUID, infos);
  EXPECT_EQ(BlobAsyncTransportStrategy::ERROR_INVALID_PARAMS,
            strategy.error());
}
}  // namespace storage
