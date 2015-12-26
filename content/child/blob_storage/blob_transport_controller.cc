// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blob_storage/blob_transport_controller.h"

#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/stl_util.h"
#include "content/child/blob_storage/blob_consolidation.h"
#include "content/child/thread_safe_sender.h"
#include "ipc/ipc_sender.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "storage/common/data_element.h"

using base::SharedMemory;
using base::SharedMemoryHandle;
using storage::BlobItemBytesRequest;
using storage::BlobItemBytesResponse;
using storage::IPCBlobItemRequestStrategy;
using storage::DataElement;

namespace content {

using storage::IPCBlobCreationCancelCode;

using ConsolidatedItem = BlobConsolidation::ConsolidatedItem;
using ReadStatus = BlobConsolidation::ReadStatus;

namespace {
const size_t kLargeThresholdBytes = 250 * 1024;
static base::LazyInstance<BlobTransportController> g_controller =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

BlobTransportController* BlobTransportController::GetInstance() {
  return g_controller.Pointer();
}

BlobTransportController::~BlobTransportController() {}

void BlobTransportController::InitiateBlobTransfer(
    const std::string& uuid,
    const std::string& type,
    scoped_ptr<BlobConsolidation> consolidation,
    IPC::Sender* sender) {
  BlobConsolidation* consolidation_ptr = consolidation.get();
  blob_storage_.insert(std::make_pair(uuid, std::move(consolidation)));
  std::vector<storage::DataElement> descriptions;
  GetDescriptions(consolidation_ptr, kLargeThresholdBytes, &descriptions);
  // TODO(dmurph): Uncomment when IPC messages are added.
  // sender->Send(new BlobStorageMsg_StartBuildingBlob(uuid, type,
  // descriptions));
}

void BlobTransportController::OnMemoryRequest(
    const std::string& uuid,
    const std::vector<storage::BlobItemBytesRequest>& requests,
    std::vector<base::SharedMemoryHandle>* memory_handles,
    const std::vector<IPC::PlatformFileForTransit>& file_handles,
    IPC::Sender* sender) {
  std::vector<storage::BlobItemBytesResponse> responses;
  ResponsesStatus status =
      GetResponses(uuid, requests, memory_handles, file_handles, &responses);

  switch (status) {
    case ResponsesStatus::BLOB_NOT_FOUND:
      // sender->Send(new BlobStorageMsg_CancelBuildingBlob(uuid,
      // IPCBlobCreationCancelCode::UNKNOWN));
      return;
    case ResponsesStatus::SHARED_MEMORY_MAP_FAILED:
      // This would happen if the renderer process doesn't have enough memory
      // to map the shared memory, which is possible if we don't have much
      // memory. If this scenario happens often, we could delay the response
      // until we have enough memory.  For now we just fail.
      CHECK(false) << "Unable to map shared memory to send blob " << uuid
                   << ".";
      break;
    case ResponsesStatus::SUCCESS:
      break;
  }

  // TODO(dmurph): Uncomment when IPC messages are added.
  // sender->Send(new BlobStorageMsg_MemoryItemResponse(uuid, responses));
}

void BlobTransportController::OnCancel(
    const std::string& uuid,
    storage::IPCBlobCreationCancelCode code) {
  DVLOG(1) << "Received blob cancel for blob " << uuid << " with reason:";
  switch (code) {
    case IPCBlobCreationCancelCode::UNKNOWN:
      DVLOG(1) << "Unknown.";
      break;
    case IPCBlobCreationCancelCode::OUT_OF_MEMORY:
      DVLOG(1) << "Out of Memory.";
      break;
    case IPCBlobCreationCancelCode::FILE_WRITE_FAILED:
      DVLOG(1) << "File Write Failed (Invalid cancel reason!).";
      break;
  }
  ReleaseBlobConsolidation(uuid);
}

void BlobTransportController::OnDone(const std::string& uuid) {
  ReleaseBlobConsolidation(uuid);
}

void BlobTransportController::Clear() {
  blob_storage_.clear();
}

BlobTransportController::BlobTransportController() {}

void BlobTransportController::CancelBlobTransfer(const std::string& uuid,
                                                 IPCBlobCreationCancelCode code,
                                                 IPC::Sender* sender) {
  // TODO(dmurph): Uncomment when IPC messages are added.
  // sender->Send(new BlobStorageMsg_CancelBuildingBlob(uuid, code));
  ReleaseBlobConsolidation(uuid);
}

void BlobTransportController::GetDescriptions(
    BlobConsolidation* consolidation,
    size_t max_data_population,
    std::vector<storage::DataElement>* out) {
  DCHECK(out->empty());
  DCHECK(consolidation);
  const auto& consolidated_items = consolidation->consolidated_items();

  size_t current_memory_population = 0;
  size_t current_item = 0;
  out->reserve(consolidated_items.size());
  for (const ConsolidatedItem& item : consolidated_items) {
    out->push_back(DataElement());
    auto& element = out->back();

    switch (item.type) {
      case DataElement::TYPE_BYTES: {
        size_t bytes_length = static_cast<size_t>(item.length);
        if (current_memory_population + bytes_length <= max_data_population) {
          element.SetToAllocatedBytes(bytes_length);
          consolidation->ReadMemory(current_item, 0, bytes_length,
                                    element.mutable_bytes());
          current_memory_population += bytes_length;
        } else {
          element.SetToBytesDescription(bytes_length);
        }
        break;
      }
      case DataElement::TYPE_FILE: {
        element.SetToFilePathRange(
            item.path, item.offset, item.length,
            base::Time::FromDoubleT(item.expected_modification_time));
        break;
      }
      case DataElement::TYPE_BLOB: {
        element.SetToBlobRange(item.blob_uuid, item.offset, item.length);
        break;
      }
      case DataElement::TYPE_FILE_FILESYSTEM: {
        element.SetToFileSystemUrlRange(
            item.filesystem_url, item.offset, item.length,
            base::Time::FromDoubleT(item.expected_modification_time));
        break;
      }
      case DataElement::TYPE_DISK_CACHE_ENTRY:
      case DataElement::TYPE_BYTES_DESCRIPTION:
      case DataElement::TYPE_UNKNOWN:
        NOTREACHED();
    }
    ++current_item;
  }
}

BlobTransportController::ResponsesStatus BlobTransportController::GetResponses(
    const std::string& uuid,
    const std::vector<BlobItemBytesRequest>& requests,
    std::vector<SharedMemoryHandle>* memory_handles,
    const std::vector<IPC::PlatformFileForTransit>& file_handles,
    std::vector<BlobItemBytesResponse>* out) {
  DCHECK(out->empty());
  auto it = blob_storage_.find(uuid);
  if (it == blob_storage_.end())
    return ResponsesStatus::BLOB_NOT_FOUND;

  BlobConsolidation* consolidation = it->second.get();
  const auto& consolidated_items = consolidation->consolidated_items();

  // Since we can be writing to the same shared memory handle from multiple
  // requests, we keep them in a vector and lazily create them.
  ScopedVector<SharedMemory> opened_memory;
  opened_memory.resize(memory_handles->size());
  for (const BlobItemBytesRequest& request : requests) {
    DCHECK_LT(request.renderer_item_index, consolidated_items.size())
        << "Invalid item index";

    const ConsolidatedItem& item =
        consolidated_items[request.renderer_item_index];
    DCHECK_LE(request.renderer_item_offset + request.size, item.length)
        << "Invalid data range";
    DCHECK_EQ(item.type, DataElement::TYPE_BYTES) << "Invalid element type";

    out->push_back(BlobItemBytesResponse(request.request_number));
    switch (request.transport_strategy) {
      case IPCBlobItemRequestStrategy::IPC: {
        BlobItemBytesResponse& response = out->back();
        ReadStatus status = consolidation->ReadMemory(
            request.renderer_item_index, request.renderer_item_offset,
            request.size, response.allocate_mutable_data(request.size));
        DCHECK(status == ReadStatus::OK)
            << "Error reading from consolidated blob: "
            << static_cast<int>(status);
        break;
      }
      case IPCBlobItemRequestStrategy::SHARED_MEMORY: {
        DCHECK_LT(request.handle_index, memory_handles->size())
            << "Invalid handle index.";
        SharedMemory* memory = opened_memory[request.handle_index];
        if (!memory) {
          SharedMemoryHandle& handle = (*memory_handles)[request.handle_index];
          DCHECK(SharedMemory::IsHandleValid(handle));
          scoped_ptr<SharedMemory> shared_memory(
              new SharedMemory(handle, false));
          if (!shared_memory->Map(request.size))
            return ResponsesStatus::SHARED_MEMORY_MAP_FAILED;
          memory = shared_memory.get();
          opened_memory[request.handle_index] = shared_memory.release();
        }
        CHECK(memory->memory()) << "Couldn't map memory for blob transfer.";
        ReadStatus status = consolidation->ReadMemory(
            request.renderer_item_index, request.renderer_item_offset,
            request.size,
            static_cast<char*>(memory->memory()) + request.handle_offset);
        DCHECK(status == ReadStatus::OK)
            << "Error reading from consolidated blob: "
            << static_cast<int>(status);
        break;
      }
      case IPCBlobItemRequestStrategy::FILE:
        NOTREACHED() << "TODO(dmurph): Not implemented.";
        break;
      case IPCBlobItemRequestStrategy::UNKNOWN:
        NOTREACHED();
        break;
    }
  }
  return ResponsesStatus::SUCCESS;
}

void BlobTransportController::ReleaseBlobConsolidation(
    const std::string& uuid) {
  blob_storage_.erase(uuid);
}

}  // namespace content
