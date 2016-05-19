// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blob_storage/blob_transport_controller.h"

#include <stddef.h>
#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/test/test_simple_task_runner.h"
#include "base/tuple.h"
#include "content/child/blob_storage/blob_consolidation.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/fileapi/webblob_messages.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::BlobItemBytesRequest;
using storage::BlobItemBytesResponse;
using storage::DataElement;

namespace content {
namespace {

class OtherThreadTestSimpleTaskRunner : public base::TestSimpleTaskRunner {
 public:
  bool RunsTasksOnCurrentThread() const override { return false; }

 protected:
  ~OtherThreadTestSimpleTaskRunner() override {}
};

class BlobTransportControllerTestSender : public ThreadSafeSender {
 public:
  explicit BlobTransportControllerTestSender(IPC::TestSink* ipc_sink)
      : ThreadSafeSender(nullptr, nullptr), ipc_sink_(ipc_sink) {}

  bool Send(IPC::Message* message) override { return ipc_sink_->Send(message); }

 private:
  ~BlobTransportControllerTestSender() override {}

  IPC::TestSink* ipc_sink_;

  DISALLOW_COPY_AND_ASSIGN(BlobTransportControllerTestSender);
};

BlobItemBytesResponse ResponseWithData(size_t request_number,
                                       const std::string& str) {
  BlobItemBytesResponse response(request_number);
  response.inline_data.assign(str.begin(), str.end());
  return response;
}

static DataElement MakeBlobElement(const std::string& uuid,
                                   uint64_t offset,
                                   uint64_t size) {
  DataElement element;
  element.SetToBlobRange(uuid, offset, size);
  return element;
}

static DataElement MakeDataDescriptionElement(size_t size) {
  DataElement element;
  element.SetToBytesDescription(size);
  return element;
}

static DataElement MakeDataElement(const std::string& str) {
  DataElement element;
  element.SetToBytes(str.c_str(), str.size());
  return element;
}

static blink::WebThreadSafeData CreateData(const std::string& str) {
  return blink::WebThreadSafeData(str.c_str(), str.size());
}

}  // namespace

class BlobTransportControllerTest : public testing::Test {
 public:
  BlobTransportControllerTest()
      : io_thread_runner_(new base::TestSimpleTaskRunner()),
        main_thread_runner_(new OtherThreadTestSimpleTaskRunner()) {}

  void SetUp() override {
    sender_ = new BlobTransportControllerTestSender(&sink_);
    BlobTransportController::GetInstance()->ClearForTesting();
  }

  void ExpectRegisterAndStartMessage(const std::string& expected_uuid,
                                     const std::string& expected_content_type,
                                     std::vector<DataElement>* descriptions) {
    const IPC::Message* register_message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_RegisterBlobUUID::ID);
    const IPC::Message* start_message =
        sink_.GetUniqueMessageMatching(BlobStorageMsg_StartBuildingBlob::ID);
    ASSERT_TRUE(register_message);
    ASSERT_TRUE(start_message);
    base::Tuple<std::string, std::string, std::string, std::set<std::string>>
        register_contents;
    base::Tuple<std::string, std::vector<DataElement>> start_contents;
    BlobStorageMsg_RegisterBlobUUID::Read(register_message, &register_contents);
    BlobStorageMsg_StartBuildingBlob::Read(start_message, &start_contents);
    EXPECT_EQ(expected_uuid, base::get<0>(register_contents));
    EXPECT_EQ(expected_uuid, base::get<0>(start_contents));
    EXPECT_EQ(expected_content_type, base::get<1>(register_contents));
    if (descriptions)
      *descriptions = base::get<1>(start_contents);
    // We don't have dispositions from the renderer.
    EXPECT_TRUE(base::get<2>(register_contents).empty());
  }

  void TearDown() override {
    BlobTransportController::GetInstance()->ClearForTesting();
  }

  IPC::TestSink sink_;
  scoped_refptr<BlobTransportControllerTestSender> sender_;
  scoped_refptr<base::TestSimpleTaskRunner> io_thread_runner_;
  scoped_refptr<OtherThreadTestSimpleTaskRunner> main_thread_runner_;
};

TEST_F(BlobTransportControllerTest, Descriptions) {
  const std::string kBlobUUID = "uuid";
  const std::string KRefBlobUUID = "refuuid";
  const std::string kBadBlobUUID = "uuuidBad";
  const size_t kShortcutSize = 11;

  // The first two data elements should be combined and the data shortcut.
  std::unique_ptr<BlobConsolidation> consolidation(new BlobConsolidation());
  consolidation->AddBlobItem(KRefBlobUUID, 10, 10);
  consolidation->AddDataItem(CreateData("Hello"));
  consolidation->AddDataItem(CreateData("Hello2"));
  consolidation->AddBlobItem(KRefBlobUUID, 0, 10);
  consolidation->AddDataItem(CreateData("Hello3"));

  std::vector<DataElement> out;
  BlobTransportController::GetDescriptions(consolidation.get(), kShortcutSize,
                                           &out);

  std::vector<DataElement> expected;
  expected.push_back(MakeBlobElement(KRefBlobUUID, 10, 10));
  expected.push_back(MakeDataElement("HelloHello2"));
  expected.push_back(MakeBlobElement(KRefBlobUUID, 0, 10));
  expected.push_back(MakeDataDescriptionElement(6));
  EXPECT_EQ(expected, out);
}

TEST_F(BlobTransportControllerTest, Responses) {
  using ResponsesStatus = BlobTransportController::ResponsesStatus;
  const std::string kBlobUUID = "uuid";
  const std::string KRefBlobUUID = "refuuid";
  const std::string kBadBlobUUID = "uuuidBad";
  BlobTransportController* holder = BlobTransportController::GetInstance();

  // The first two data elements should be combined.
  BlobConsolidation* consolidation = new BlobConsolidation();
  consolidation->AddBlobItem(KRefBlobUUID, 10, 10);
  consolidation->AddDataItem(CreateData("Hello"));
  consolidation->AddDataItem(CreateData("Hello2"));
  consolidation->AddBlobItem(KRefBlobUUID, 0, 10);
  consolidation->AddDataItem(CreateData("Hello3"));
  // See the above test for the expected descriptions layout.

  holder->blob_storage_[kBlobUUID] = base::WrapUnique(consolidation);

  std::vector<BlobItemBytesRequest> requests;
  std::vector<base::SharedMemoryHandle> memory_handles;
  std::vector<IPC::PlatformFileForTransit> file_handles;
  std::vector<storage::BlobItemBytesResponse> output;

  // Request for all of first data
  requests.push_back(BlobItemBytesRequest::CreateIPCRequest(0, 1, 0, 11));
  EXPECT_EQ(ResponsesStatus::SUCCESS,
            holder->GetResponses(kBlobUUID, requests, &memory_handles,
                                 file_handles, &output));
  EXPECT_EQ(1u, output.size());
  std::vector<storage::BlobItemBytesResponse> expected;
  expected.push_back(ResponseWithData(0, "HelloHello2"));
  EXPECT_EQ(expected, output);

  // Part of second data
  output.clear();
  requests[0] = BlobItemBytesRequest::CreateIPCRequest(1000, 3, 1, 5);
  EXPECT_EQ(ResponsesStatus::SUCCESS,
            holder->GetResponses(kBlobUUID, requests, &memory_handles,
                                 file_handles, &output));
  EXPECT_EQ(1u, output.size());
  expected.clear();
  expected.push_back(ResponseWithData(1000, "ello3"));
  EXPECT_EQ(expected, output);

  // Both data segments
  output.clear();
  requests[0] = BlobItemBytesRequest::CreateIPCRequest(0, 1, 0, 11);
  requests.push_back(BlobItemBytesRequest::CreateIPCRequest(1, 3, 0, 6));
  EXPECT_EQ(ResponsesStatus::SUCCESS,
            holder->GetResponses(kBlobUUID, requests, &memory_handles,
                                 file_handles, &output));
  EXPECT_EQ(2u, output.size());
  expected.clear();
  expected.push_back(ResponseWithData(0, "HelloHello2"));
  expected.push_back(ResponseWithData(1, "Hello3"));
  EXPECT_EQ(expected, output);
}

TEST_F(BlobTransportControllerTest, SharedMemory) {
  using ResponsesStatus = BlobTransportController::ResponsesStatus;
  const std::string kBlobUUID = "uuid";
  const std::string KRefBlobUUID = "refuuid";
  const std::string kBadBlobUUID = "uuuidBad";
  BlobTransportController* holder = BlobTransportController::GetInstance();

  // The first two data elements should be combined.
  BlobConsolidation* consolidation = new BlobConsolidation();
  consolidation->AddBlobItem(KRefBlobUUID, 10, 10);
  consolidation->AddDataItem(CreateData("Hello"));
  consolidation->AddDataItem(CreateData("Hello2"));
  consolidation->AddBlobItem(KRefBlobUUID, 0, 10);
  consolidation->AddDataItem(CreateData("Hello3"));
  // See the above test for the expected descriptions layout.

  holder->blob_storage_[kBlobUUID] = base::WrapUnique(consolidation);

  std::vector<BlobItemBytesRequest> requests;
  std::vector<base::SharedMemoryHandle> memory_handles;
  std::vector<IPC::PlatformFileForTransit> file_handles;
  std::vector<storage::BlobItemBytesResponse> output;

  // Request for all data in shared memory
  requests.push_back(
      BlobItemBytesRequest::CreateSharedMemoryRequest(0, 1, 0, 11, 0, 0));
  requests.push_back(
      BlobItemBytesRequest::CreateSharedMemoryRequest(1, 3, 0, 6, 0, 11));
  base::SharedMemory memory;
  memory.CreateAndMapAnonymous(11 + 6);
  base::SharedMemoryHandle handle =
      base::SharedMemory::DuplicateHandle(memory.handle());
  CHECK(base::SharedMemory::NULLHandle() != handle);
  memory_handles.push_back(handle);

  EXPECT_EQ(ResponsesStatus::SUCCESS,
            holder->GetResponses(kBlobUUID, requests, &memory_handles,
                                 file_handles, &output));
  EXPECT_EQ(2u, output.size());
  std::vector<storage::BlobItemBytesResponse> expected;
  expected.push_back(BlobItemBytesResponse(0));
  expected.push_back(BlobItemBytesResponse(1));
  EXPECT_EQ(expected, output);
  std::string expected_memory = "HelloHello2Hello3";
  const char* mem_location = static_cast<const char*>(memory.memory());
  std::vector<char> value(mem_location, mem_location + memory.requested_size());
  EXPECT_THAT(value, testing::ElementsAreArray(expected_memory.c_str(),
                                               expected_memory.size()));
}

TEST_F(BlobTransportControllerTest, TestPublicMethods) {
  const std::string kBlobUUID = "uuid";
  const std::string kBlobContentType = "content_type";
  const std::string kBlob2UUID = "uuid2";
  const std::string kBlob2ContentType = "content_type2";
  const std::string KRefBlobUUID = "refuuid";
  std::vector<DataElement> message_descriptions;
  BlobTransportController* holder = BlobTransportController::GetInstance();

  BlobConsolidation* consolidation = new BlobConsolidation();
  consolidation->AddBlobItem(KRefBlobUUID, 10, 10);
  BlobTransportController::InitiateBlobTransfer(
      kBlobUUID, kBlobContentType, base::WrapUnique(consolidation), sender_,
      io_thread_runner_.get(), main_thread_runner_);
  // Check that we have the 'increase ref' pending task.
  EXPECT_TRUE(main_thread_runner_->HasPendingTask());
  // Check that we have the 'store' pending task.
  EXPECT_TRUE(io_thread_runner_->HasPendingTask());
  // Check that we've sent the data.
  ExpectRegisterAndStartMessage(kBlobUUID, kBlobContentType,
                                &message_descriptions);
  main_thread_runner_->ClearPendingTasks();

  // Check that we got the correct start message.
  EXPECT_FALSE(holder->IsTransporting(kBlobUUID));
  io_thread_runner_->RunPendingTasks();
  EXPECT_TRUE(holder->IsTransporting(kBlobUUID));
  base::Tuple<std::string, std::vector<DataElement>> message_contents;
  EXPECT_EQ(MakeBlobElement(KRefBlobUUID, 10, 10), message_descriptions[0]);

  holder->OnCancel(kBlobUUID,
                   storage::IPCBlobCreationCancelCode::OUT_OF_MEMORY);
  EXPECT_FALSE(holder->IsTransporting(kBlobUUID));
  // Check we have the 'decrease ref' task.
  EXPECT_TRUE(main_thread_runner_->HasPendingTask());
  main_thread_runner_->ClearPendingTasks();
  sink_.ClearMessages();

  // Add the second.
  BlobConsolidation* consolidation2 = new BlobConsolidation();
  consolidation2->AddBlobItem(KRefBlobUUID, 10, 10);
  BlobTransportController::InitiateBlobTransfer(
      kBlob2UUID, kBlob2ContentType, base::WrapUnique(consolidation2), sender_,
      io_thread_runner_.get(), main_thread_runner_);
  EXPECT_TRUE(main_thread_runner_->HasPendingTask());
  main_thread_runner_->ClearPendingTasks();
  sink_.ClearMessages();

  io_thread_runner_->RunPendingTasks();
  EXPECT_TRUE(holder->IsTransporting(kBlob2UUID));

  // Finish the second one.
  holder->OnDone(kBlob2UUID);
  EXPECT_FALSE(holder->IsTransporting(kBlob2UUID));
  EXPECT_TRUE(main_thread_runner_->HasPendingTask());
  main_thread_runner_->ClearPendingTasks();
}

TEST_F(BlobTransportControllerTest, ResponsesErrors) {
  using ResponsesStatus = BlobTransportController::ResponsesStatus;
  const std::string kBlobUUID = "uuid";
  const std::string KRefBlobUUID = "refuuid";
  const std::string kBadBlobUUID = "uuuidBad";
  BlobTransportController* holder = BlobTransportController::GetInstance();

  BlobConsolidation* consolidation = new BlobConsolidation();
  consolidation->AddBlobItem(KRefBlobUUID, 10, 10);

  holder->blob_storage_[kBlobUUID] = base::WrapUnique(consolidation);

  std::vector<BlobItemBytesRequest> requests;
  std::vector<base::SharedMemoryHandle> memory_handles;
  std::vector<IPC::PlatformFileForTransit> file_handles;
  std::vector<storage::BlobItemBytesResponse> output;

  // Error conditions
  EXPECT_EQ(ResponsesStatus::BLOB_NOT_FOUND,
            holder->GetResponses(kBadBlobUUID, requests, &memory_handles,
                                 file_handles, &output));
  EXPECT_EQ(0u, output.size());
}

}  // namespace content
