// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_async_builder_host.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "storage/common/blob_storage/blob_storage_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {
const std::string kBlobUUID = "blobUUIDYAY";
const std::string kFakeBlobUUID = "fakeBlob";
const std::string kBlobType = "blobtypeYAY";

const size_t kTestBlobStorageIPCThresholdBytes = 5;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 20;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

void PopulateBytes(char* bytes, size_t length) {
  for (size_t i = 0; i < length; i++) {
    bytes[i] = static_cast<char>(i);
  }
}

void AddMemoryItem(size_t length, std::vector<DataElement>* out) {
  DataElement bytes;
  bytes.SetToBytesDescription(length);
  out->push_back(bytes);
}

void AddShortcutMemoryItem(size_t length, std::vector<DataElement>* out) {
  DataElement bytes;
  bytes.SetToAllocatedBytes(length);
  PopulateBytes(bytes.mutable_bytes(), length);
  out->push_back(bytes);
}

void AddShortcutMemoryItem(size_t length, BlobDataBuilder* out) {
  DataElement bytes;
  bytes.SetToAllocatedBytes(length);
  PopulateBytes(bytes.mutable_bytes(), length);
  out->AppendData(bytes.bytes(), length);
}

void AddBlobItem(std::vector<DataElement>* out) {
  DataElement blob;
  blob.SetToBlob(kFakeBlobUUID);
  out->push_back(blob);
}

class BlobAsyncBuilderHostTest : public testing::Test {
 protected:
  BlobAsyncBuilderHostTest()
      : matching_builder_(nullptr),
        done_called_(false),
        cancel_called_(false),
        cancel_code_(IPCBlobCreationCancelCode::UNKNOWN),
        request_called_(false) {}
  ~BlobAsyncBuilderHostTest() override {}

  void SetUp() override {
    matching_builder_ = nullptr;
    done_called_ = false;
    cancel_called_ = false;
    cancel_code_ = IPCBlobCreationCancelCode::UNKNOWN;
    request_called_ = false;
    requests_.clear();
    memory_handles_.clear();
    file_handles_.clear();
    host_.SetMemoryConstantsForTesting(kTestBlobStorageIPCThresholdBytes,
                                       kTestBlobStorageMaxSharedMemoryBytes,
                                       kTestBlobStorageMaxFileSizeBytes);
  }

  void SetMatchingBuilder(BlobDataBuilder* builder) {
    matching_builder_ = builder;
  }

  void CancelCallback(IPCBlobCreationCancelCode code) {
    cancel_called_ = true;
    cancel_code_ = code;
  }

  void DoneCallback(const BlobDataBuilder& builder) {
    // This does a deep comparison, including internal data items.
    if (matching_builder_)
      EXPECT_EQ(*matching_builder_, builder);
    done_called_ = true;
  }

  void RequestMemoryCallback(
      const std::vector<storage::BlobItemBytesRequest>& requests,
      const std::vector<base::SharedMemoryHandle>& shared_memory_handles,
      const std::vector<uint64_t>& file_sizes) {
    this->requests_ = requests;
    memory_handles_ = shared_memory_handles;
    file_handles_ = file_sizes;
    request_called_ = true;
  }

  bool BuildBlobAsync(const std::vector<DataElement>& descriptions,
                      size_t memory_available) {
    done_called_ = false;
    cancel_called_ = false;
    request_called_ = false;
    return host_.StartBuildingBlob(
        kBlobUUID, kBlobType, descriptions, memory_available,
        base::Bind(&BlobAsyncBuilderHostTest::RequestMemoryCallback,
                   base::Unretained(this)),
        base::Bind(&BlobAsyncBuilderHostTest::DoneCallback,
                   base::Unretained(this)),
        base::Bind(&BlobAsyncBuilderHostTest::CancelCallback,
                   base::Unretained(this)));
  }

  BlobDataBuilder* matching_builder_;
  BlobAsyncBuilderHost host_;
  bool done_called_;
  bool cancel_called_;
  IPCBlobCreationCancelCode cancel_code_;

  bool request_called_;
  std::vector<storage::BlobItemBytesRequest> requests_;
  std::vector<base::SharedMemoryHandle> memory_handles_;
  std::vector<uint64_t> file_handles_;
};

TEST_F(BlobAsyncBuilderHostTest, TestShortcut) {
  std::vector<DataElement> descriptions;

  AddShortcutMemoryItem(10, &descriptions);
  AddBlobItem(&descriptions);
  AddShortcutMemoryItem(5000, &descriptions);

  BlobDataBuilder expected(kBlobUUID);
  expected.set_content_type(kBlobType);
  AddShortcutMemoryItem(10, &expected);
  expected.AppendBlob(kFakeBlobUUID);
  AddShortcutMemoryItem(5000, &expected);
  SetMatchingBuilder(&expected);

  EXPECT_TRUE(BuildBlobAsync(descriptions, 5010));

  EXPECT_TRUE(done_called_);
  EXPECT_FALSE(cancel_called_);
  EXPECT_FALSE(request_called_);
  EXPECT_EQ(0u, host_.blob_building_count());
};

TEST_F(BlobAsyncBuilderHostTest, TestSingleSharedMemRequest) {
  std::vector<DataElement> descriptions;
  const size_t kSize = kTestBlobStorageIPCThresholdBytes + 1;
  AddMemoryItem(kSize, &descriptions);

  EXPECT_TRUE(
      BuildBlobAsync(descriptions, kTestBlobStorageIPCThresholdBytes + 1));

  EXPECT_FALSE(done_called_);
  EXPECT_FALSE(cancel_called_);
  EXPECT_TRUE(request_called_);
  EXPECT_EQ(1u, host_.blob_building_count());
  ASSERT_EQ(1u, requests_.size());
  request_called_ = false;

  EXPECT_EQ(
      BlobItemBytesRequest::CreateSharedMemoryRequest(0, 0, 0, kSize, 0, 0),
      requests_.at(0));
};

TEST_F(BlobAsyncBuilderHostTest, TestMultipleSharedMemRequests) {
  std::vector<DataElement> descriptions;
  const size_t kSize = kTestBlobStorageMaxSharedMemoryBytes + 1;
  const char kFirstBlockByte = 7;
  const char kSecondBlockByte = 19;
  AddMemoryItem(kSize, &descriptions);

  BlobDataBuilder expected(kBlobUUID);
  expected.set_content_type(kBlobType);
  char data[kSize];
  memset(data, kFirstBlockByte, kTestBlobStorageMaxSharedMemoryBytes);
  expected.AppendData(data, kTestBlobStorageMaxSharedMemoryBytes);
  expected.AppendData(&kSecondBlockByte, 1);
  SetMatchingBuilder(&expected);

  EXPECT_TRUE(
      BuildBlobAsync(descriptions, kTestBlobStorageMaxSharedMemoryBytes + 1));

  EXPECT_FALSE(done_called_);
  EXPECT_FALSE(cancel_called_);
  EXPECT_TRUE(request_called_);
  EXPECT_EQ(1u, host_.blob_building_count());
  ASSERT_EQ(1u, requests_.size());
  request_called_ = false;

  // We need to grab a duplicate handle so we can have two blocks open at the
  // same time.
  base::SharedMemoryHandle handle =
      base::SharedMemory::DuplicateHandle(memory_handles_.at(0));
  EXPECT_TRUE(base::SharedMemory::IsHandleValid(handle));
  base::SharedMemory shared_memory(handle, false);
  EXPECT_TRUE(shared_memory.Map(kTestBlobStorageMaxSharedMemoryBytes));

  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(
                0, 0, 0, kTestBlobStorageMaxSharedMemoryBytes, 0, 0),
            requests_.at(0));

  memset(shared_memory.memory(), kFirstBlockByte,
         kTestBlobStorageMaxSharedMemoryBytes);

  BlobItemBytesResponse response(0);
  std::vector<BlobItemBytesResponse> responses = {response};
  EXPECT_TRUE(host_.OnMemoryResponses(kBlobUUID, responses));

  EXPECT_FALSE(done_called_);
  EXPECT_FALSE(cancel_called_);
  EXPECT_TRUE(request_called_);
  EXPECT_EQ(1u, host_.blob_building_count());
  ASSERT_EQ(1u, requests_.size());
  request_called_ = false;

  EXPECT_EQ(BlobItemBytesRequest::CreateSharedMemoryRequest(
                1, 0, kTestBlobStorageMaxSharedMemoryBytes, 1, 0, 0),
            requests_.at(0));

  memset(shared_memory.memory(), kSecondBlockByte, 1);

  response.request_number = 1;
  responses[0] = response;
  EXPECT_TRUE(host_.OnMemoryResponses(kBlobUUID, responses));
  EXPECT_TRUE(done_called_);
  EXPECT_FALSE(cancel_called_);
  EXPECT_FALSE(request_called_);
  EXPECT_EQ(0u, host_.blob_building_count());
};

TEST_F(BlobAsyncBuilderHostTest, TestBasicIPCAndStopBuilding) {
  std::vector<DataElement> descriptions;

  AddMemoryItem(2, &descriptions);
  AddBlobItem(&descriptions);
  AddMemoryItem(2, &descriptions);

  BlobDataBuilder expected(kBlobUUID);
  expected.set_content_type(kBlobType);
  AddShortcutMemoryItem(2, &expected);
  expected.AppendBlob(kFakeBlobUUID);
  AddShortcutMemoryItem(2, &expected);
  SetMatchingBuilder(&expected);

  EXPECT_TRUE(BuildBlobAsync(descriptions, 5010));
  host_.StopBuildingBlob(kBlobUUID);
  EXPECT_TRUE(BuildBlobAsync(descriptions, 5010));

  EXPECT_FALSE(done_called_);
  EXPECT_FALSE(cancel_called_);
  EXPECT_TRUE(request_called_);
  EXPECT_EQ(1u, host_.blob_building_count());
  request_called_ = false;

  BlobItemBytesResponse response1(0);
  PopulateBytes(response1.allocate_mutable_data(2), 2);
  BlobItemBytesResponse response2(1);
  PopulateBytes(response2.allocate_mutable_data(2), 2);
  std::vector<BlobItemBytesResponse> responses = {response1, response2};
  EXPECT_TRUE(host_.OnMemoryResponses(kBlobUUID, responses));
  EXPECT_TRUE(done_called_);
  EXPECT_FALSE(cancel_called_);
  EXPECT_FALSE(request_called_);
  EXPECT_EQ(0u, host_.blob_building_count());
};

TEST_F(BlobAsyncBuilderHostTest, TestBadIPCs) {
  std::vector<DataElement> descriptions;

  // Test reusing same blob uuid.
  SetMatchingBuilder(nullptr);
  AddMemoryItem(10, &descriptions);
  AddBlobItem(&descriptions);
  AddMemoryItem(5000, &descriptions);
  EXPECT_TRUE(BuildBlobAsync(descriptions, 5010));
  EXPECT_FALSE(BuildBlobAsync(descriptions, 5010));
  EXPECT_FALSE(done_called_);
  EXPECT_FALSE(cancel_called_);
  EXPECT_FALSE(request_called_);
  host_.StopBuildingBlob(kBlobUUID);

  // Test we're _not_ an error if we get a bad uuid for responses.
  BlobItemBytesResponse response(0);
  std::vector<BlobItemBytesResponse> responses = {response};
  EXPECT_TRUE(host_.OnMemoryResponses(kBlobUUID, responses));

  // Test empty responses.
  responses.clear();
  EXPECT_FALSE(host_.OnMemoryResponses(kBlobUUID, responses));

  // Test response problems below here.
  descriptions.clear();
  AddMemoryItem(2, &descriptions);
  AddBlobItem(&descriptions);
  AddMemoryItem(2, &descriptions);
  EXPECT_TRUE(BuildBlobAsync(descriptions, 5010));

  // Invalid request number.
  BlobItemBytesResponse response1(3);
  PopulateBytes(response1.allocate_mutable_data(2), 2);
  responses = {response1};
  EXPECT_FALSE(host_.OnMemoryResponses(kBlobUUID, responses));

  // Duplicate request number responses.
  EXPECT_TRUE(BuildBlobAsync(descriptions, 5010));
  response1.request_number = 0;
  BlobItemBytesResponse response2(0);
  PopulateBytes(response2.allocate_mutable_data(2), 2);
  responses = {response1, response2};
  EXPECT_FALSE(host_.OnMemoryResponses(kBlobUUID, responses));
};

}  // namespace
}  // namespace storage
