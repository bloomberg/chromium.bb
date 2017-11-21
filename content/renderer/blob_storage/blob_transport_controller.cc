// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/blob_storage/blob_transport_controller.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/child/child_process.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/fileapi/webblob_messages.h"
#include "content/renderer/blob_storage/blob_consolidation.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"
#include "storage/common/blob_storage/blob_item_bytes_response.h"
#include "storage/common/data_element.h"
#include "third_party/WebKit/public/platform/Platform.h"

using base::File;
using base::SharedMemory;
using base::SharedMemoryHandle;
using storage::BlobItemBytesRequest;
using storage::BlobItemBytesResponse;
using storage::IPCBlobItemRequestStrategy;
using storage::DataElement;

using storage::BlobItemBytesResponse;
using storage::BlobItemBytesRequest;
using storage::BlobStatus;

namespace content {
using ConsolidatedItem = BlobConsolidation::ConsolidatedItem;
using ReadStatus = BlobConsolidation::ReadStatus;

namespace {
static base::LazyInstance<BlobTransportController>::Leaky g_controller =
    LAZY_INSTANCE_INITIALIZER;

// This keeps the process alive while blobs are being transferred.
// These need to be called on the main thread.
void IncChildProcessRefCount() {
  blink::Platform::Current()->SuddenTerminationChanged(false);
  ChildProcess::current()->AddRefProcess();
}

void DecChildProcessRefCount() {
  blink::Platform::Current()->SuddenTerminationChanged(true);
  ChildProcess::current()->ReleaseProcess();
}

void DecChildProcessRefCountTimes(size_t times) {
  for (size_t i = 0; i < times; i++) {
    DecChildProcessRefCount();
  }
}

bool WriteSingleChunk(base::File* file, const char* memory, size_t size) {
  size_t written = 0;
  while (written < size) {
    size_t writing_size = base::saturated_cast<int>(size - written);
    int actual_written =
        file->WriteAtCurrentPos(memory, static_cast<int>(writing_size));
    bool write_failed = actual_written < 0;
    UMA_HISTOGRAM_BOOLEAN("Storage.Blob.RendererFileWriteFailed", write_failed);
    if (write_failed)
      return false;
    written += actual_written;
  }
  return true;
}

bool WriteSingleRequestToDisk(const BlobConsolidation* consolidation,
                              const BlobItemBytesRequest& request,
                              File* file) {
  if (!file->IsValid())
    return false;
  int64_t seek_distance = file->Seek(
      File::FROM_BEGIN, base::checked_cast<int64_t>(request.handle_offset));
  bool seek_failed = seek_distance < 0;
  UMA_HISTOGRAM_BOOLEAN("Storage.Blob.RendererFileSeekFailed", seek_failed);
  if (seek_failed)
    return false;
  BlobConsolidation::ReadStatus status = consolidation->VisitMemory(
      request.renderer_item_index, request.renderer_item_offset, request.size,
      base::Bind(&WriteSingleChunk, file));
  if (status != BlobConsolidation::ReadStatus::OK)
    return false;
  return true;
}

base::Optional<std::vector<BlobItemBytesResponse>> WriteDiskRequests(
    scoped_refptr<BlobConsolidation> consolidation,
    std::unique_ptr<std::vector<BlobItemBytesRequest>> requests,
    const std::vector<IPC::PlatformFileForTransit>& file_handles) {
  std::vector<BlobItemBytesResponse> responses;
  // We grab ownership of the file handles here. When this vector is destroyed
  // it will close the files.
  std::vector<File> files;
  files.reserve(file_handles.size());
  for (const auto& file_handle : file_handles) {
    files.emplace_back(IPC::PlatformFileForTransitToFile(file_handle));
  }
  for (const auto& request : *requests) {
    if (!WriteSingleRequestToDisk(consolidation.get(), request,
                                  &files[request.handle_index])) {
      return base::nullopt;
    }
  }
  // The last modified time needs to be collected after we flush the file.
  std::vector<base::Time> last_modified_times;
  last_modified_times.resize(file_handles.size());
  for (size_t i = 0; i < files.size(); ++i) {
    auto& file = files[i];
    if (!file.Flush())
      return base::nullopt;
    File::Info info;
    if (!file.GetInfo(&info))
      return base::nullopt;
    last_modified_times[i] = info.last_modified;
  }

  for (const auto& request : *requests) {
    responses.push_back(BlobItemBytesResponse(request.request_number));
    responses.back().time_file_modified =
        last_modified_times[request.handle_index];
  }

  return responses;
}

}  // namespace

BlobTransportController* BlobTransportController::GetInstance() {
  return g_controller.Pointer();
}

// static
void BlobTransportController::InitiateBlobTransfer(
    const std::string& uuid,
    const std::string& content_type,
    scoped_refptr<BlobConsolidation> consolidation,
    scoped_refptr<ThreadSafeSender> sender,
    base::SingleThreadTaskRunner* io_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_runner) {
  if (main_runner->BelongsToCurrentThread()) {
    IncChildProcessRefCount();
  } else {
    main_runner->PostTask(FROM_HERE, base::BindOnce(&IncChildProcessRefCount));
  }

  storage::BlobStorageLimits quotas;
  std::vector<storage::DataElement> descriptions;
  BlobTransportController::GetDescriptions(
      consolidation.get(), quotas.max_ipc_memory_size, &descriptions);
  // I post the task first to make sure that we store our consolidation before
  // we get a request back from the browser.
  io_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&BlobTransportController::StoreBlobDataForRequests,
                     base::Unretained(BlobTransportController::GetInstance()),
                     uuid, base::Passed(std::move(consolidation)),
                     base::Passed(std::move(main_runner))));

  // Measure how much jank the following synchronous IPC introduces.
  SCOPED_UMA_HISTOGRAM_TIMER("Storage.Blob.RegisterBlobTime");

  sender->Send(
      new BlobStorageMsg_RegisterBlob(uuid, content_type, "", descriptions));
}

void BlobTransportController::OnMemoryRequest(
    const std::string& uuid,
    const std::vector<storage::BlobItemBytesRequest>& requests,
    std::vector<base::SharedMemoryHandle>* memory_handles,
    const std::vector<IPC::PlatformFileForTransit>& file_handles,
    base::TaskRunner* file_runner,
    IPC::Sender* sender) {
  std::vector<BlobItemBytesResponse> responses;
  auto it = blob_storage_.find(uuid);
  // Ignore invalid messages.
  if (it == blob_storage_.end())
    return;

  BlobConsolidation* consolidation = it->second.get();
  const auto& consolidated_items = consolidation->consolidated_items();

  std::unique_ptr<std::vector<BlobItemBytesRequest>> file_requests(
      new std::vector<BlobItemBytesRequest>());

  // Since we can be writing to the same shared memory handle from multiple
  // requests, we keep them in a vector and lazily create them.
  std::vector<std::unique_ptr<SharedMemory>> opened_memory(
      memory_handles->size());

  // We need to calculate how much memory we expect to be writing to the memory
  // segments so we can correctly map it the first time.
  std::vector<size_t> shared_memory_sizes(memory_handles->size());
  for (const BlobItemBytesRequest& request : requests) {
    if (request.transport_strategy !=
        IPCBlobItemRequestStrategy::SHARED_MEMORY) {
      continue;
    }
    DCHECK_LT(request.handle_index, memory_handles->size())
        << "Invalid handle index.";
    shared_memory_sizes[request.handle_index] =
        std::max<size_t>(shared_memory_sizes[request.handle_index],
                         request.size + request.handle_offset);
  }

  for (const BlobItemBytesRequest& request : requests) {
    DCHECK_LT(request.renderer_item_index, consolidated_items.size())
        << "Invalid item index";

    const ConsolidatedItem& item =
        consolidated_items[request.renderer_item_index];
    DCHECK_LE(request.renderer_item_offset + request.size, item.length)
        << "Invalid data range";
    DCHECK_EQ(item.type, DataElement::TYPE_BYTES) << "Invalid element type";

    switch (request.transport_strategy) {
      case IPCBlobItemRequestStrategy::IPC: {
        responses.push_back(BlobItemBytesResponse(request.request_number));
        BlobItemBytesResponse& response = responses.back();
        ReadStatus status = consolidation->ReadMemory(
            request.renderer_item_index, request.renderer_item_offset,
            request.size, response.allocate_mutable_data(request.size));
        DCHECK(status == ReadStatus::OK)
            << "Error reading from consolidated blob: "
            << static_cast<int>(status);
        break;
      }
      case IPCBlobItemRequestStrategy::SHARED_MEMORY: {
        responses.push_back(BlobItemBytesResponse(request.request_number));
        if (!opened_memory[request.handle_index]) {
          SharedMemoryHandle& handle = (*memory_handles)[request.handle_index];
          size_t size = shared_memory_sizes[request.handle_index];
          DCHECK(SharedMemory::IsHandleValid(handle));
          auto shared_memory = std::make_unique<SharedMemory>(handle, false);

          if (!shared_memory->Map(size)) {
            // This would happen if the renderer process doesn't have enough
            // memory to map the shared memory, which is possible if we don't
            // have much memory. If this scenario happens often, we could delay
            // the response until we have enough memory.  For now we just fail.
            CHECK(false) << "Unable to map shared memory to send blob " << uuid
                         << ".";
            return;
          }
          opened_memory[request.handle_index] = std::move(shared_memory);
        }
        CHECK(opened_memory[request.handle_index]->memory())
            << "Couldn't map memory for blob transfer.";
        ReadStatus status = consolidation->ReadMemory(
            request.renderer_item_index, request.renderer_item_offset,
            request.size,
            static_cast<char*>(opened_memory[request.handle_index]->memory()) +
                request.handle_offset);
        DCHECK(status == ReadStatus::OK)
            << "Error reading from consolidated blob: "
            << static_cast<int>(status);
        break;
      }
      case IPCBlobItemRequestStrategy::FILE:
        DCHECK_LT(request.handle_index, file_handles.size())
            << "Invalid handle index.";
        file_requests->push_back(request);
        break;
      case IPCBlobItemRequestStrategy::UNKNOWN:
        NOTREACHED();
        break;
    }
  }
  if (!file_requests->empty()) {
    base::PostTaskAndReplyWithResult(
        file_runner, FROM_HERE,
        base::Bind(&WriteDiskRequests, base::WrapRefCounted(consolidation),
                   base::Passed(&file_requests), file_handles),
        base::Bind(&BlobTransportController::OnFileWriteComplete,
                   weak_factory_.GetWeakPtr(), sender, uuid));
  }

  if (!responses.empty())
    sender->Send(new BlobStorageMsg_MemoryItemResponse(uuid, responses));
}

void BlobTransportController::OnBlobFinalStatus(const std::string& uuid,
                                                storage::BlobStatus code) {
  DVLOG_IF(1, storage::BlobStatusIsError(code))
      << "Received blob error for blob " << uuid
      << " with code: " << static_cast<int>(code);
  DCHECK(!BlobStatusIsPending(code));
  ReleaseBlobConsolidation(uuid);
}

void BlobTransportController::CancelAllBlobTransfers() {
  weak_factory_.InvalidateWeakPtrs();
  if (!blob_storage_.empty() && main_thread_runner_) {
    main_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DecChildProcessRefCountTimes, blob_storage_.size()));
  }
  main_thread_runner_ = nullptr;
  blob_storage_.clear();
}

// static
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
      case DataElement::TYPE_RAW_FILE:
      case DataElement::TYPE_DISK_CACHE_ENTRY:
      case DataElement::TYPE_BYTES_DESCRIPTION:
      case DataElement::TYPE_DATA_PIPE:
      case DataElement::TYPE_UNKNOWN:
        NOTREACHED();
    }
    ++current_item;
  }
}

BlobTransportController::BlobTransportController() : weak_factory_(this) {}

BlobTransportController::~BlobTransportController() {}

void BlobTransportController::OnFileWriteComplete(
    IPC::Sender* sender,
    const std::string& uuid,
    const base::Optional<std::vector<storage::BlobItemBytesResponse>>& result) {
  if (blob_storage_.find(uuid) == blob_storage_.end())
    return;
  if (!result) {
    sender->Send(new BlobStorageMsg_SendBlobStatus(
        uuid, BlobStatus::ERR_FILE_WRITE_FAILED));
    ReleaseBlobConsolidation(uuid);
    return;
  }
  sender->Send(new BlobStorageMsg_MemoryItemResponse(uuid, result.value()));
}

void BlobTransportController::StoreBlobDataForRequests(
    const std::string& uuid,
    scoped_refptr<BlobConsolidation> consolidation,
    scoped_refptr<base::SingleThreadTaskRunner> main_runner) {
  if (!main_thread_runner_.get()) {
    main_thread_runner_ = std::move(main_runner);
  }
  blob_storage_[uuid] = std::move(consolidation);
}

void BlobTransportController::ReleaseBlobConsolidation(
    const std::string& uuid) {
  if (blob_storage_.erase(uuid)) {
    main_thread_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(&DecChildProcessRefCount));
  }
}

}  // namespace content
