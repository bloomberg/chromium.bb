// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/async_resource_handler.h"

#include <algorithm>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/loader/resource_buffer.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/upload_progress_tracker.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/resource_request_completion_status.h"
#include "content/public/common/resource_response.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/upload_progress.h"
#include "net/url_request/redirect_info.h"

using base::TimeDelta;
using base::TimeTicks;

namespace content {
namespace {

static int g_async_loader_buffer_size = 1024 * 512;
static int g_async_loader_min_buffer_allocation_size = 1024 * 4;
static int g_async_loader_max_buffer_allocation_size = 1024 * 32;

}  // namespace

// Used to write into an existing IOBuffer at a given offset. This is
// very similar to DependentIOBufferForRedirectToFile and
// DependentIOBufferForMimeSniffing but not identical.
class DependentIOBufferForAsyncLoading : public net::WrappedIOBuffer {
 public:
  DependentIOBufferForAsyncLoading(ResourceBuffer* backing, char* memory)
      : net::WrappedIOBuffer(memory), backing_(backing) {}

 private:
  ~DependentIOBufferForAsyncLoading() override {}
  scoped_refptr<ResourceBuffer> backing_;
};

AsyncResourceHandler::AsyncResourceHandler(net::URLRequest* request,
                                           ResourceDispatcherHostImpl* rdh)
    : ResourceHandler(request),
      ResourceMessageDelegate(request),
      rdh_(rdh),
      pending_data_count_(0),
      allocation_size_(0),
      total_read_body_bytes_(0),
      has_checked_for_sufficient_resources_(false),
      sent_received_response_msg_(false),
      sent_data_buffer_msg_(false),
      reported_transfer_size_(0) {
  DCHECK(GetRequestInfo()->requester_info()->IsRenderer());
  InitializeResourceBufferConstants();
}

AsyncResourceHandler::~AsyncResourceHandler() {
  if (has_checked_for_sufficient_resources_)
    rdh_->FinishedWithResourcesForRequest(request());
}

void AsyncResourceHandler::InitializeResourceBufferConstants() {
  static bool did_init = false;
  if (did_init)
    return;
  did_init = true;

  GetNumericArg("resource-buffer-size", &g_async_loader_buffer_size);
  GetNumericArg("resource-buffer-min-allocation-size",
                &g_async_loader_min_buffer_allocation_size);
  GetNumericArg("resource-buffer-max-allocation-size",
                &g_async_loader_max_buffer_allocation_size);
}

bool AsyncResourceHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AsyncResourceHandler, message)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_FollowRedirect, OnFollowRedirect)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_DataReceived_ACK, OnDataReceivedACK)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_UploadProgress_ACK, OnUploadProgressACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AsyncResourceHandler::OnFollowRedirect(int request_id) {
  if (!request()->status().is_success()) {
    DVLOG(1) << "OnFollowRedirect for invalid request";
    return;
  }

  ResumeIfDeferred();
}

void AsyncResourceHandler::OnDataReceivedACK(int request_id) {
  if (pending_data_count_) {
    --pending_data_count_;

    buffer_->RecycleLeastRecentlyAllocated();
    if (buffer_->CanAllocate())
      ResumeIfDeferred();
  }
}

void AsyncResourceHandler::OnUploadProgressACK(int request_id) {
  if (upload_progress_tracker_)
    upload_progress_tracker_->OnAckReceived();
}

void AsyncResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  ResourceMessageFilter* filter = GetFilter();
  if (!filter) {
    controller->Cancel();
    return;
  }

  response->head.encoded_data_length = request()->GetTotalReceivedBytes();
  reported_transfer_size_ = 0;
  response->head.request_start = request()->creation_time();
  response->head.response_start = TimeTicks::Now();
  // TODO(davidben): Is it necessary to pass the new first party URL for
  // cookies? The only case where it can change is top-level navigation requests
  // and hopefully those will eventually all be owned by the browser. It's
  // possible this is still needed while renderer-owned ones exist.
  if (filter->Send(new ResourceMsg_ReceivedRedirect(
          GetRequestID(), redirect_info, response->head))) {
    OnDefer(std::move(controller));
  } else {
    controller->Cancel();
  }
}

void AsyncResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  // For changes to the main frame, inform the renderer of the new URL's
  // per-host settings before the request actually commits.  This way the
  // renderer will be able to set these precisely at the time the
  // request commits, avoiding the possibility of e.g. zooming the old content
  // or of having to layout the new content twice.
  DCHECK(!has_controller());

  response_started_ticks_ = base::TimeTicks::Now();

  // We want to send a final upload progress message prior to sending the
  // response complete message even if we're waiting for an ack to to a
  // previous upload progress message.
  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  const ResourceRequestInfoImpl* info = GetRequestInfo();
  ResourceMessageFilter* filter = GetFilter();
  if (!filter) {
    controller->Cancel();
    return;
  }

  response->head.encoded_data_length = request()->raw_header_size();

  // If the parent handler downloaded the resource to a file, grant the child
  // read permissions on it.
  if (!response->head.download_file_path.empty()) {
    rdh_->RegisterDownloadedTempFile(
        info->GetChildID(), info->GetRequestID(),
        response->head.download_file_path);
  }

  response->head.request_start = request()->creation_time();
  response->head.response_start = TimeTicks::Now();
  filter->Send(
      new ResourceMsg_ReceivedResponse(GetRequestID(), response->head));
  sent_received_response_msg_ = true;

  if (request()->response_info().metadata.get()) {
    std::vector<uint8_t> copy(request()->response_info().metadata->data(),
                              request()->response_info().metadata->data() +
                                  request()->response_info().metadata->size());
    filter->Send(new ResourceMsg_ReceivedCachedMetadata(GetRequestID(), copy));
  }

  controller->Resume();
}

void AsyncResourceHandler::OnWillStart(
    const GURL& url,
    std::unique_ptr<ResourceController> controller) {
  ResourceMessageFilter* filter = GetFilter();
  if (!filter) {
    controller->Cancel();
    return;
  }

  if (GetRequestInfo()->is_upload_progress_enabled() &&
      request()->has_upload()) {
    upload_progress_tracker_ = base::MakeUnique<UploadProgressTracker>(
        FROM_HERE,
        base::BindRepeating(&AsyncResourceHandler::SendUploadProgress,
                            base::Unretained(this)),
        request());
  }
  controller->Resume();
}

void AsyncResourceHandler::OnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());

  if (!CheckForSufficientResource()) {
    controller->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  if (!EnsureResourceBufferIsInitialized()) {
    controller->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  DCHECK(buffer_->CanAllocate());
  char* memory = buffer_->Allocate(&allocation_size_);
  CHECK(memory);

  *buf = new DependentIOBufferForAsyncLoading(buffer_.get(), memory);
  *buf_size = allocation_size_;

  controller->Resume();
}

void AsyncResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(!has_controller());
  DCHECK_GE(bytes_read, 0);

  if (!bytes_read) {
    controller->Resume();
    return;
  }

  ResourceMessageFilter* filter = GetFilter();
  if (!filter) {
    controller->Cancel();
    return;
  }

  int encoded_data_length = CalculateEncodedDataLengthToReport();
  if (!first_chunk_read_)
    encoded_data_length -= request()->raw_header_size();

  first_chunk_read_ = true;

  buffer_->ShrinkLastAllocation(bytes_read);

  total_read_body_bytes_ += bytes_read;

  if (!sent_data_buffer_msg_) {
    base::SharedMemoryHandle handle = base::SharedMemory::DuplicateHandle(
        buffer_->GetSharedMemory().handle());
    if (!base::SharedMemory::IsHandleValid(handle)) {
      controller->Cancel();
      return;
    }
    filter->Send(new ResourceMsg_SetDataBuffer(
        GetRequestID(), handle, buffer_->GetSharedMemory().mapped_size(),
        filter->peer_pid()));
    sent_data_buffer_msg_ = true;
  }

  int data_offset = buffer_->GetLastAllocationOffset();

  filter->Send(new ResourceMsg_DataReceived(GetRequestID(), data_offset,
                                            bytes_read, encoded_data_length));
  ++pending_data_count_;

  if (!buffer_->CanAllocate()) {
    OnDefer(std::move(controller));
  } else {
    controller->Resume();
  }
}

void AsyncResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  int encoded_data_length = CalculateEncodedDataLengthToReport();

  ResourceMessageFilter* filter = GetFilter();
  if (filter) {
    filter->Send(new ResourceMsg_DataDownloaded(
        GetRequestID(), bytes_downloaded, encoded_data_length));
  }
}

void AsyncResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  ResourceMessageFilter* filter = GetFilter();
  if (!filter) {
    controller->Resume();
    return;
  }

  // Ensure sending the final upload progress message here, since
  // OnResponseCompleted can be called without OnResponseStarted on cancellation
  // or error cases.
  if (upload_progress_tracker_) {
    upload_progress_tracker_->OnUploadCompleted();
    upload_progress_tracker_ = nullptr;
  }

  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/107692
  char url_buf[128];
  base::strlcpy(url_buf, request()->url().spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);

  // TODO(gavinp): Remove this CHECK when we figure out the cause of
  // http://crbug.com/124680 . This check mirrors closely check in
  // WebURLLoaderImpl::OnCompletedRequest that routes this message to a WebCore
  // ResourceHandleInternal which asserts on its state and crashes. By crashing
  // when the message is sent, we should get better crash reports.
  CHECK(status.status() != net::URLRequestStatus::SUCCESS ||
        sent_received_response_msg_);

  int error_code = status.error();

  DCHECK(status.status() != net::URLRequestStatus::IO_PENDING);

  ResourceRequestCompletionStatus request_complete_data;
  request_complete_data.error_code = error_code;
  request_complete_data.exists_in_cache = request()->response_info().was_cached;
  request_complete_data.completion_time = TimeTicks::Now();
  request_complete_data.encoded_data_length =
      request()->GetTotalReceivedBytes();
  request_complete_data.encoded_body_length = request()->GetRawBodyBytes();
  request_complete_data.decoded_body_length = total_read_body_bytes_;
  filter->Send(
      new ResourceMsg_RequestComplete(GetRequestID(), request_complete_data));

  if (status.is_success())
    RecordHistogram();
  controller->Resume();
}

bool AsyncResourceHandler::EnsureResourceBufferIsInitialized() {
  DCHECK(has_checked_for_sufficient_resources_);

  if (buffer_.get() && buffer_->IsInitialized())
    return true;

  buffer_ = new ResourceBuffer();
  return buffer_->Initialize(g_async_loader_buffer_size,
                             g_async_loader_min_buffer_allocation_size,
                             g_async_loader_max_buffer_allocation_size);
}

void AsyncResourceHandler::ResumeIfDeferred() {
  if (has_controller()) {
    request()->LogUnblocked();
    Resume();
  }
}

void AsyncResourceHandler::OnDefer(
    std::unique_ptr<ResourceController> controller) {
  HoldController(std::move(controller));
  request()->LogBlockedBy("AsyncResourceHandler");
}

bool AsyncResourceHandler::CheckForSufficientResource() {
  if (has_checked_for_sufficient_resources_)
    return true;
  has_checked_for_sufficient_resources_ = true;

  if (rdh_->HasSufficientResourcesForRequest(request()))
    return true;

  return false;
}

int AsyncResourceHandler::CalculateEncodedDataLengthToReport() {
  const auto transfer_size = request()->GetTotalReceivedBytes();
  const auto difference =  transfer_size - reported_transfer_size_;
  reported_transfer_size_ = transfer_size;
  return difference;
}

void AsyncResourceHandler::RecordHistogram() {
  int64_t elapsed_time =
      (base::TimeTicks::Now() - response_started_ticks_).InMicroseconds();
  int64_t encoded_length = request()->GetTotalReceivedBytes();
  if (encoded_length < 2 * 1024) {
    // The resource was smaller than the smallest required buffer size.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.ResourceLoader.ResponseStartToEnd.LT_2kB",
                                elapsed_time, 1, 100000, 100);
  } else if (encoded_length < 32 * 1024) {
    // The resource was smaller than single chunk.
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.ResourceLoader.ResponseStartToEnd.LT_32kB",
                                elapsed_time, 1, 100000, 100);
  } else if (encoded_length < 512 * 1024) {
    // The resource was smaller than single chunk.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Net.ResourceLoader.ResponseStartToEnd.LT_512kB",
        elapsed_time, 1, 100000, 100);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Net.ResourceLoader.ResponseStartToEnd.Over_512kB",
        elapsed_time, 1, 100000, 100);
  }
}

void AsyncResourceHandler::SendUploadProgress(
    const net::UploadProgress& progress) {
  ResourceMessageFilter* filter = GetFilter();
  if (!filter)
    return;
  filter->Send(new ResourceMsg_UploadProgress(
      GetRequestID(), progress.position(), progress.size()));
}

}  // namespace content
