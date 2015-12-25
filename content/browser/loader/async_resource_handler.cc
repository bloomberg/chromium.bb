// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/async_resource_handler.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_netlog_observer.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/loader/resource_buffer.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_util.h"
#include "net/log/net_log.h"
#include "net/url_request/redirect_info.h"

using base::TimeDelta;
using base::TimeTicks;

namespace content {
namespace {

static int kBufferSize = 1024 * 512;
static int kMinAllocationSize = 1024 * 4;
static int kMaxAllocationSize = 1024 * 32;
// The interval for calls to ReportUploadProgress.
static int kUploadProgressIntervalMsec = 10;

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

}  // namespace

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

AsyncResourceHandler::AsyncResourceHandler(
    net::URLRequest* request,
    ResourceDispatcherHostImpl* rdh)
    : ResourceHandler(request),
      ResourceMessageDelegate(request),
      rdh_(rdh),
      pending_data_count_(0),
      allocation_size_(0),
      did_defer_(false),
      has_checked_for_sufficient_resources_(false),
      sent_received_response_msg_(false),
      sent_first_data_msg_(false),
      last_upload_position_(0),
      waiting_for_upload_progress_ack_(false),
      reported_transfer_size_(0) {
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
  waiting_for_upload_progress_ack_ = false;
}

void AsyncResourceHandler::ReportUploadProgress() {
  DCHECK(GetRequestInfo()->is_upload_progress_enabled());
  if (waiting_for_upload_progress_ack_)
    return;  // Send one progress event at a time.

  net::UploadProgress progress = request()->GetUploadProgress();
  if (!progress.size())
    return;  // Nothing to upload.

  if (progress.position() == last_upload_position_)
    return;  // No progress made since last time.

  const uint64_t kHalfPercentIncrements = 200;
  const TimeDelta kOneSecond = TimeDelta::FromMilliseconds(1000);

  uint64_t amt_since_last = progress.position() - last_upload_position_;
  TimeDelta time_since_last = TimeTicks::Now() - last_upload_ticks_;

  bool is_finished = (progress.size() == progress.position());
  bool enough_new_progress =
      (amt_since_last > (progress.size() / kHalfPercentIncrements));
  bool too_much_time_passed = time_since_last > kOneSecond;

  if (is_finished || enough_new_progress || too_much_time_passed) {
    ResourceMessageFilter* filter = GetFilter();
    if (filter) {
      filter->Send(
        new ResourceMsg_UploadProgress(GetRequestID(),
                                       progress.position(),
                                       progress.size()));
    }
    waiting_for_upload_progress_ack_ = true;
    last_upload_ticks_ = TimeTicks::Now();
    last_upload_position_ = progress.position();
  }
}

bool AsyncResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!info->filter())
    return false;

  *defer = did_defer_ = true;
  OnDefer();

  if (rdh_->delegate()) {
    rdh_->delegate()->OnRequestRedirected(
        redirect_info.new_url, request(), info->GetContext(), response);
  }

  DevToolsNetLogObserver::PopulateResponseInfo(request(), response);
  response->head.encoded_data_length = request()->GetTotalReceivedBytes();
  reported_transfer_size_ = 0;
  response->head.request_start = request()->creation_time();
  response->head.response_start = TimeTicks::Now();
  // TODO(davidben): Is it necessary to pass the new first party URL for
  // cookies? The only case where it can change is top-level navigation requests
  // and hopefully those will eventually all be owned by the browser. It's
  // possible this is still needed while renderer-owned ones exist.
  return info->filter()->Send(new ResourceMsg_ReceivedRedirect(
      GetRequestID(), redirect_info, response->head));
}

bool AsyncResourceHandler::OnResponseStarted(ResourceResponse* response,
                                             bool* defer) {
  // For changes to the main frame, inform the renderer of the new URL's
  // per-host settings before the request actually commits.  This way the
  // renderer will be able to set these precisely at the time the
  // request commits, avoiding the possibility of e.g. zooming the old content
  // or of having to layout the new content twice.

  progress_timer_.Stop();
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!info->filter())
    return false;

  // We want to send a final upload progress message prior to sending the
  // response complete message even if we're waiting for an ack to to a
  // previous upload progress message.
  if (info->is_upload_progress_enabled()) {
    waiting_for_upload_progress_ack_ = false;
    ReportUploadProgress();
  }

  if (rdh_->delegate()) {
    rdh_->delegate()->OnResponseStarted(
        request(), info->GetContext(), response, info->filter());
  }

  DevToolsNetLogObserver::PopulateResponseInfo(request(), response);

  const HostZoomMapImpl* host_zoom_map =
      static_cast<const HostZoomMapImpl*>(info->filter()->GetHostZoomMap());

  if (info->GetResourceType() == RESOURCE_TYPE_MAIN_FRAME && host_zoom_map) {
    const GURL& request_url = request()->url();
    int render_process_id = info->GetChildID();
    int render_view_id = info->GetRouteID();

    double zoom_level = host_zoom_map->GetZoomLevelForView(
        request_url, render_process_id, render_view_id);

    info->filter()->Send(new ViewMsg_SetZoomLevelForLoadingURL(
        render_view_id, request_url, zoom_level));
  }

  // If the parent handler downloaded the resource to a file, grant the child
  // read permissions on it.
  if (!response->head.download_file_path.empty()) {
    rdh_->RegisterDownloadedTempFile(
        info->GetChildID(), info->GetRequestID(),
        response->head.download_file_path);
  }

  response->head.request_start = request()->creation_time();
  response->head.response_start = TimeTicks::Now();
  info->filter()->Send(new ResourceMsg_ReceivedResponse(GetRequestID(),
                                                        response->head));
  sent_received_response_msg_ = true;

  if (request()->response_info().metadata.get()) {
    std::vector<char> copy(request()->response_info().metadata->data(),
                           request()->response_info().metadata->data() +
                               request()->response_info().metadata->size());
    info->filter()->Send(new ResourceMsg_ReceivedCachedMetadata(GetRequestID(),
                                                                copy));
  }

  return true;
}

bool AsyncResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  if (GetRequestInfo()->is_upload_progress_enabled() &&
      request()->has_upload()) {
    ReportUploadProgress();
    progress_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kUploadProgressIntervalMsec),
        this,
        &AsyncResourceHandler::ReportUploadProgress);
  }
  return true;
}

bool AsyncResourceHandler::OnBeforeNetworkStart(const GURL& url, bool* defer) {
  return true;
}

bool AsyncResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                      int* buf_size,
                                      int min_size) {
  DCHECK_EQ(-1, min_size);

  if (!EnsureResourceBufferIsInitialized())
    return false;

  DCHECK(buffer_->CanAllocate());
  char* memory = buffer_->Allocate(&allocation_size_);
  CHECK(memory);

  *buf = new DependentIOBuffer(buffer_.get(), memory);
  *buf_size = allocation_size_;

  return true;
}

bool AsyncResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK_GE(bytes_read, 0);

  if (!bytes_read)
    return true;

  ResourceMessageFilter* filter = GetFilter();
  if (!filter)
    return false;

  buffer_->ShrinkLastAllocation(bytes_read);

  if (!sent_first_data_msg_) {
    base::SharedMemoryHandle handle;
    int size;
    if (!buffer_->ShareToProcess(filter->PeerHandle(), &handle, &size))
      return false;

    // TODO(erikchen): Temporary debugging. http://crbug.com/527588.
    CHECK_LE(size, kBufferSize);
    filter->Send(new ResourceMsg_SetDataBuffer(
        GetRequestID(), handle, size, filter->peer_pid()));
    sent_first_data_msg_ = true;
  }

  int data_offset = buffer_->GetLastAllocationOffset();

  int64_t current_transfer_size = request()->GetTotalReceivedBytes();
  int encoded_data_length = current_transfer_size - reported_transfer_size_;
  reported_transfer_size_ = current_transfer_size;

  // TODO(erikchen): Temporary debugging. http://crbug.com/527588.
  CHECK_LE(data_offset, kBufferSize);

  filter->Send(new ResourceMsg_DataReceivedDebug(GetRequestID(), data_offset));
  filter->Send(new ResourceMsg_DataReceived(
      GetRequestID(), data_offset, bytes_read, encoded_data_length));
  ++pending_data_count_;

  if (!buffer_->CanAllocate()) {
    *defer = did_defer_ = true;
    OnDefer();
  }

  return true;
}

void AsyncResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  int64_t current_transfer_size = request()->GetTotalReceivedBytes();
  int encoded_data_length = current_transfer_size - reported_transfer_size_;
  reported_transfer_size_ = current_transfer_size;

  ResourceMessageFilter* filter = GetFilter();
  if (filter) {
    filter->Send(new ResourceMsg_DataDownloaded(
        GetRequestID(), bytes_downloaded, encoded_data_length));
  }
}

void AsyncResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    bool* defer) {
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!info->filter())
    return;

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
  bool was_ignored_by_handler = info->WasIgnoredByHandler();

  DCHECK(status.status() != net::URLRequestStatus::IO_PENDING);
  // If this check fails, then we're in an inconsistent state because all
  // requests ignored by the handler should be canceled (which should result in
  // the ERR_ABORTED error code).
  DCHECK(!was_ignored_by_handler || error_code == net::ERR_ABORTED);

  // TODO(mkosiba): Fix up cases where we create a URLRequestStatus
  // with a status() != SUCCESS and an error_code() == net::OK.
  if (status.status() == net::URLRequestStatus::CANCELED &&
      error_code == net::OK) {
    error_code = net::ERR_ABORTED;
  } else if (status.status() == net::URLRequestStatus::FAILED &&
             error_code == net::OK) {
    error_code = net::ERR_FAILED;
  }

  ResourceMsg_RequestCompleteData request_complete_data;
  request_complete_data.error_code = error_code;
  request_complete_data.was_ignored_by_handler = was_ignored_by_handler;
  request_complete_data.exists_in_cache = request()->response_info().was_cached;
  request_complete_data.security_info = security_info;
  request_complete_data.completion_time = TimeTicks::Now();
  request_complete_data.encoded_data_length =
      request()->GetTotalReceivedBytes();
  info->filter()->Send(
      new ResourceMsg_RequestComplete(GetRequestID(), request_complete_data));
}

bool AsyncResourceHandler::EnsureResourceBufferIsInitialized() {
  if (buffer_.get() && buffer_->IsInitialized())
    return true;

  if (!has_checked_for_sufficient_resources_) {
    has_checked_for_sufficient_resources_ = true;
    if (!rdh_->HasSufficientResourcesForRequest(request())) {
      controller()->CancelWithError(net::ERR_INSUFFICIENT_RESOURCES);
      return false;
    }
  }

  buffer_ = new ResourceBuffer();
  return buffer_->Initialize(kBufferSize,
                             kMinAllocationSize,
                             kMaxAllocationSize);
}

void AsyncResourceHandler::ResumeIfDeferred() {
  if (did_defer_) {
    did_defer_ = false;
    request()->LogUnblocked();
    controller()->Resume();
  }
}

void AsyncResourceHandler::OnDefer() {
  request()->LogBlockedBy("AsyncResourceHandler");
}

}  // namespace content
