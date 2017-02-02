// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/async_resource_handler.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/browser/loader/netlog_observer.h"
#include "content/browser/loader/resource_buffer.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader/upload_progress_tracker.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request_completion_status.h"
#include "content/common/view_messages.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/content_features.h"
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

static int kBufferSize = 1024 * 512;
static int kMinAllocationSize = 1024 * 4;
static int kMaxAllocationSize = 1024 * 32;

// Used when kOptimizeLoadingIPCForSmallResources is enabled.
// Small resource typically issues two Read call: one for the content itself
// and another for getting zero response to detect EOF.
// Inline these two into the IPC message to avoid allocating an expensive
// SharedMemory for small resources.
const int kNumLeadingChunk = 2;
const int kInlinedLeadingChunkSize = 2048;

void GetNumericArg(const std::string& name, int* result) {
  const std::string& value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(name);
  if (!value.empty())
    base::StringToInt(value, result);
}

void InitializeResourceBufferConstants() {
  static bool did_init = false;
  if (did_init)
    return;
  did_init = true;

  GetNumericArg("resource-buffer-size", &kBufferSize);
  GetNumericArg("resource-buffer-min-allocation-size", &kMinAllocationSize);
  GetNumericArg("resource-buffer-max-allocation-size", &kMaxAllocationSize);
}

// This enum is used for logging a histogram and should not be reordered.
enum ExpectedContentSizeResult {
  EQ_RESPONSE_BODY = 0,
  EQ_RESPONSE_BODY_GT_EQ_BUFFER_SIZE = 1,
  GT_EQ_BUFFER_SIZE = 2,
  LT_RESPONSE_BODY = 3,
  GT_RESPONSE_BODY = 4,
  UNKNOWN = 5,
  EXPECTED_CONTENT_MAX,
};

}  // namespace

// Used when kOptimizeLoadingIPCForSmallResources is enabled.
// The instance hooks the buffer allocation of AsyncResourceHandler, and
// determine if we should use SharedMemory or should inline the data into
// the IPC message.
class AsyncResourceHandler::InliningHelper {
 public:

  InliningHelper() {}
  ~InliningHelper() {}

  void OnResponseReceived(const ResourceResponse& response) {
    InliningStatus status = IsInliningApplicable(response);
    UMA_HISTOGRAM_ENUMERATION(
        "Net.ResourceLoader.InliningStatus",
        static_cast<int>(status),
        static_cast<int>(InliningStatus::INLINING_STATUS_COUNT));
    inlining_applicable_ = status == InliningStatus::APPLICABLE;
  }

  // Returns true if InliningHelper allocates the buffer for inlining.
  bool PrepareInlineBufferIfApplicable(scoped_refptr<net::IOBuffer>* buf,
                                      int* buf_size) {
    ++num_allocation_;

    // If the server sends the resource in multiple small chunks,
    // |num_allocation_| may exceed |kNumLeadingChunk|. Disable inlining and
    // fall back to the regular resource loading path in that case.
    if (!inlining_applicable_ ||
        num_allocation_ > kNumLeadingChunk ||
        !base::FeatureList::IsEnabled(
            features::kOptimizeLoadingIPCForSmallResources)) {
      return false;
    }

    leading_chunk_buffer_ = new net::IOBuffer(kInlinedLeadingChunkSize);
    *buf = leading_chunk_buffer_;
    *buf_size = kInlinedLeadingChunkSize;
    return true;
  }

  // Returns true if the received data is sent to the consumer.
  bool SendInlinedDataIfApplicable(int bytes_read,
                                   int encoded_data_length,
                                   IPC::Sender* sender,
                                   int request_id) {
    DCHECK(sender);
    if (!leading_chunk_buffer_)
      return false;

    std::vector<char> data(
        leading_chunk_buffer_->data(),
        leading_chunk_buffer_->data() + bytes_read);
    leading_chunk_buffer_ = nullptr;

    sender->Send(new ResourceMsg_InlinedDataChunkReceived(
        request_id, data, encoded_data_length));
    return true;
  }

  void RecordHistogram(int64_t elapsed_time) {
    if (inlining_applicable_) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Net.ResourceLoader.ResponseStartToEnd.InliningApplicable",
          elapsed_time, 1, 100000, 100);
    }
  }

 private:
  enum class InliningStatus : int {
    APPLICABLE = 0,
    EARLY_ALLOCATION = 1,
    UNKNOWN_CONTENT_LENGTH = 2,
    LARGE_CONTENT = 3,
    HAS_TRANSFER_ENCODING = 4,
    HAS_CONTENT_ENCODING = 5,
    INLINING_STATUS_COUNT,
  };

  InliningStatus IsInliningApplicable(const ResourceResponse& response) {
    // Disable if the leading chunk is already arrived.
    if (num_allocation_)
      return InliningStatus::EARLY_ALLOCATION;

    // Disable if the content is known to be large.
    if (response.head.content_length > kInlinedLeadingChunkSize)
      return InliningStatus::LARGE_CONTENT;

    // Disable if the length of the content is unknown.
    if (response.head.content_length < 0)
      return InliningStatus::UNKNOWN_CONTENT_LENGTH;

    if (response.head.headers) {
      if (response.head.headers->HasHeader("Transfer-Encoding"))
        return InliningStatus::HAS_TRANSFER_ENCODING;
      if (response.head.headers->HasHeader("Content-Encoding"))
        return InliningStatus::HAS_CONTENT_ENCODING;
    }

    return InliningStatus::APPLICABLE;
  }

  int num_allocation_ = 0;
  bool inlining_applicable_ = false;
  scoped_refptr<net::IOBuffer> leading_chunk_buffer_;
};

class DependentIOBuffer : public net::WrappedIOBuffer {
 public:
  DependentIOBuffer(ResourceBuffer* backing, char* memory)
      : net::WrappedIOBuffer(memory),
        backing_(backing) {
  }
 private:
  ~DependentIOBuffer() override {}
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
      inlining_helper_(new InliningHelper),
      reported_transfer_size_(0) {
  DCHECK(GetRequestInfo()->requester_info()->IsRenderer());
  InitializeResourceBufferConstants();
}

AsyncResourceHandler::~AsyncResourceHandler() {
  if (has_checked_for_sufficient_resources_)
    rdh_->FinishedWithResourcesForRequest(request());
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

  NetLogObserver::PopulateResponseInfo(request(), response);
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
  if (rdh_->delegate()) {
    rdh_->delegate()->OnResponseStarted(request(), info->GetContext(),
                                        response);
  }

  ResourceMessageFilter* filter = GetFilter();
  if (!filter) {
    controller->Cancel();
    return;
  }

  NetLogObserver::PopulateResponseInfo(request(), response);
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
    std::vector<char> copy(request()->response_info().metadata->data(),
                           request()->response_info().metadata->data() +
                               request()->response_info().metadata->size());
    filter->Send(new ResourceMsg_ReceivedCachedMetadata(GetRequestID(), copy));
  }

  inlining_helper_->OnResponseReceived(*response);
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

bool AsyncResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                      int* buf_size) {
  DCHECK(!has_controller());

  // TODO(mmenke):  Should fail with ERR_INSUFFICIENT_RESOURCES here.
  if (!CheckForSufficientResource())
    return false;

  // Return early if InliningHelper allocates the buffer, so that we should
  // inline the data into the IPC message without allocating SharedMemory.
  if (inlining_helper_->PrepareInlineBufferIfApplicable(buf, buf_size))
    return true;

  if (!EnsureResourceBufferIsInitialized())
    return false;

  DCHECK(buffer_->CanAllocate());
  char* memory = buffer_->Allocate(&allocation_size_);
  CHECK(memory);

  *buf = new DependentIOBuffer(buffer_.get(), memory);
  *buf_size = allocation_size_;

  return true;
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

  // Return early if InliningHelper handled the received data.
  if (inlining_helper_->SendInlinedDataIfApplicable(
          bytes_read, encoded_data_length, filter, GetRequestID())) {
    controller->Resume();
    return;
  }

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
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  bool was_ignored_by_handler = info->WasIgnoredByHandler();

  DCHECK(status.status() != net::URLRequestStatus::IO_PENDING);
  // If this check fails, then we're in an inconsistent state because all
  // requests ignored by the handler should be canceled (which should result in
  // the ERR_ABORTED error code).
  DCHECK(!was_ignored_by_handler || error_code == net::ERR_ABORTED);

  ResourceRequestCompletionStatus request_complete_data;
  request_complete_data.error_code = error_code;
  request_complete_data.was_ignored_by_handler = was_ignored_by_handler;
  request_complete_data.exists_in_cache = request()->response_info().was_cached;
  request_complete_data.completion_time = TimeTicks::Now();
  request_complete_data.encoded_data_length =
      request()->GetTotalReceivedBytes();
  request_complete_data.encoded_body_length = request()->GetRawBodyBytes();
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
  return buffer_->Initialize(kBufferSize,
                             kMinAllocationSize,
                             kMaxAllocationSize);
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

  inlining_helper_->RecordHistogram(elapsed_time);

  // Record if content size was known in advance.
  int64_t expected_content_size = request()->GetExpectedContentSize();
  ExpectedContentSizeResult expected_content_size_result =
      ExpectedContentSizeResult::UNKNOWN;
  if (expected_content_size >= 0) {
    // Compare response body size to expected content size.
    if (expected_content_size == total_read_body_bytes_ &&
        expected_content_size >= kBufferSize) {
      expected_content_size_result =
          ExpectedContentSizeResult::EQ_RESPONSE_BODY_GT_EQ_BUFFER_SIZE;
    } else if (expected_content_size >= kBufferSize) {
      expected_content_size_result =
          ExpectedContentSizeResult::GT_EQ_BUFFER_SIZE;
    } else if (expected_content_size == total_read_body_bytes_) {
      expected_content_size_result =
          ExpectedContentSizeResult::EQ_RESPONSE_BODY;
    } else if (expected_content_size < total_read_body_bytes_) {
      expected_content_size_result =
          ExpectedContentSizeResult::LT_RESPONSE_BODY;
    } else {
      expected_content_size_result =
          ExpectedContentSizeResult::GT_RESPONSE_BODY;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Net.ResourceLoader.ExpectedContentSizeResult",
                            expected_content_size_result,
                            ExpectedContentSizeResult::EXPECTED_CONTENT_MAX);
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
