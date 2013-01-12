// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/async_resource_handler.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/string_number_conversions.h"
#include "content/browser/devtools/devtools_netlog_observer.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/loader/resource_buffer.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "webkit/glue/resource_loader_bridge.h"

using base::TimeTicks;

namespace content {
namespace {

static int kBufferSize = 1024 * 512;
static int kMinAllocationSize = 1024 * 4;
static int kMaxAllocationSize = 1024 * 32;

void GetNumericArg(const std::string& name, int* result) {
  const std::string& value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(name);
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

int CalcUsedPercentage(int bytes_read, int buffer_size) {
  double ratio = static_cast<double>(bytes_read) / buffer_size;
  return static_cast<int>(ratio * 100.0 + 0.5);  // Round to nearest integer.
}

}  // namespace

class DependentIOBuffer : public net::WrappedIOBuffer {
 public:
  DependentIOBuffer(ResourceBuffer* backing, char* memory)
      : net::WrappedIOBuffer(memory),
        backing_(backing) {
  }
 private:
  ~DependentIOBuffer() {}
  scoped_refptr<ResourceBuffer> backing_;
};

AsyncResourceHandler::AsyncResourceHandler(
    ResourceMessageFilter* filter,
    int routing_id,
    net::URLRequest* request,
    ResourceDispatcherHostImpl* rdh)
    : filter_(filter),
      routing_id_(routing_id),
      request_(request),
      rdh_(rdh),
      pending_data_count_(0),
      allocation_size_(0),
      did_defer_(false),
      sent_received_response_msg_(false),
      sent_first_data_msg_(false) {
  // Set a back-pointer from ResourceRequestInfoImpl to |this|, so that the
  // ResourceDispatcherHostImpl can send us IPC messages.
  // TODO(darin): Implement an IPC message filter instead?
  ResourceRequestInfoImpl::ForRequest(request_)->set_async_handler(this);

  InitializeResourceBufferConstants();
}

AsyncResourceHandler::~AsyncResourceHandler() {
  // Cleanup back-pointer stored on the request info.
  ResourceRequestInfoImpl::ForRequest(request_)->set_async_handler(NULL);
}

void AsyncResourceHandler::OnFollowRedirect(
    bool has_new_first_party_for_cookies,
    const GURL& new_first_party_for_cookies) {
  if (!request_->status().is_success()) {
    DVLOG(1) << "OnFollowRedirect for invalid request";
    return;
  }

  if (has_new_first_party_for_cookies)
    request_->set_first_party_for_cookies(new_first_party_for_cookies);

  ResumeIfDeferred();
}

void AsyncResourceHandler::OnDataReceivedACK() {
  --pending_data_count_;

  buffer_->RecycleLeastRecentlyAllocated();
  if (buffer_->CanAllocate())
    ResumeIfDeferred();
}

bool AsyncResourceHandler::OnUploadProgress(int request_id,
                                            uint64 position,
                                            uint64 size) {
  return filter_->Send(new ResourceMsg_UploadProgress(routing_id_, request_id,
                                                      position, size));
}

bool AsyncResourceHandler::OnRequestRedirected(int request_id,
                                               const GURL& new_url,
                                               ResourceResponse* response,
                                               bool* defer) {
  *defer = did_defer_ = true;

  if (rdh_->delegate()) {
    rdh_->delegate()->OnRequestRedirected(new_url, request_,
                                          filter_->resource_context(),
                                          response);
  }

  DevToolsNetLogObserver::PopulateResponseInfo(request_, response);
  response->head.request_start = request_->creation_time();
  response->head.response_start = TimeTicks::Now();
  return filter_->Send(new ResourceMsg_ReceivedRedirect(
      routing_id_, request_id, new_url, response->head));
}

bool AsyncResourceHandler::OnResponseStarted(int request_id,
                                             ResourceResponse* response,
                                             bool* defer) {
  // For changes to the main frame, inform the renderer of the new URL's
  // per-host settings before the request actually commits.  This way the
  // renderer will be able to set these precisely at the time the
  // request commits, avoiding the possibility of e.g. zooming the old content
  // or of having to layout the new content twice.

  ResourceContext* resource_context = filter_->resource_context();
  if (rdh_->delegate()) {
    rdh_->delegate()->OnResponseStarted(request_, resource_context, response,
                                        filter_);
  }

  DevToolsNetLogObserver::PopulateResponseInfo(request_, response);

  HostZoomMap* host_zoom_map =
      GetHostZoomMapForResourceContext(resource_context);

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  if (info->GetResourceType() == ResourceType::MAIN_FRAME && host_zoom_map) {
    const GURL& request_url = request_->url();
    filter_->Send(new ViewMsg_SetZoomLevelForLoadingURL(
        info->GetRouteID(),
        request_url, host_zoom_map->GetZoomLevel(net::GetHostOrSpecFromURL(
            request_url))));
  }

  response->head.request_start = request_->creation_time();
  response->head.response_start = TimeTicks::Now();
  filter_->Send(new ResourceMsg_ReceivedResponse(
      routing_id_, request_id, response->head));
  sent_received_response_msg_ = true;

  if (request_->response_info().metadata) {
    std::vector<char> copy(request_->response_info().metadata->data(),
                           request_->response_info().metadata->data() +
                           request_->response_info().metadata->size());
    filter_->Send(new ResourceMsg_ReceivedCachedMetadata(
        routing_id_, request_id, copy));
  }

  return true;
}

bool AsyncResourceHandler::OnWillStart(int request_id,
                                       const GURL& url,
                                       bool* defer) {
  return true;
}

bool AsyncResourceHandler::OnWillRead(int request_id, net::IOBuffer** buf,
                                      int* buf_size, int min_size) {
  DCHECK_EQ(-1, min_size);

  if (!EnsureResourceBufferIsInitialized())
    return false;

  DCHECK(buffer_->CanAllocate());
  char* memory = buffer_->Allocate(&allocation_size_);
  CHECK(memory);

  *buf = new DependentIOBuffer(buffer_, memory);
  *buf_size = allocation_size_;

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Net.AsyncResourceHandler_SharedIOBuffer_Alloc",
      *buf_size, 0, kMaxAllocationSize, 100);
  return true;
}

bool AsyncResourceHandler::OnReadCompleted(int request_id, int bytes_read,
                                           bool* defer) {
  if (!bytes_read)
    return true;

  buffer_->ShrinkLastAllocation(bytes_read);

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Net.AsyncResourceHandler_SharedIOBuffer_Used",
      bytes_read, 0, kMaxAllocationSize, 100);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.AsyncResourceHandler_SharedIOBuffer_UsedPercentage",
      CalcUsedPercentage(bytes_read, allocation_size_));

  if (!sent_first_data_msg_) {
    base::SharedMemoryHandle handle;
    int size;
    if (!buffer_->ShareToProcess(filter_->peer_handle(), &handle, &size))
      return false;
    filter_->Send(
        new ResourceMsg_SetDataBuffer(routing_id_, request_id, handle, size,
                                      base::GetProcId(filter_->peer_handle())));
    sent_first_data_msg_ = true;
  }

  int data_offset = buffer_->GetLastAllocationOffset();
  int encoded_data_length =
      DevToolsNetLogObserver::GetAndResetEncodedDataLength(request_);

  filter_->Send(
      new ResourceMsg_DataReceived(routing_id_, request_id, data_offset,
                                   bytes_read, encoded_data_length));
  ++pending_data_count_;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Net.AsyncResourceHandler_PendingDataCount",
      pending_data_count_, 0, 100, 100);

  if (!buffer_->CanAllocate()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Net.AsyncResourceHandler_PendingDataCount_WhenFull",
        pending_data_count_, 0, 100, 100);
    *defer = did_defer_ = true;
  }

  return true;
}

void AsyncResourceHandler::OnDataDownloaded(
    int request_id, int bytes_downloaded) {
  filter_->Send(new ResourceMsg_DataDownloaded(
      routing_id_, request_id, bytes_downloaded));
}

bool AsyncResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/107692
  char url_buf[128];
  base::strlcpy(url_buf, request_->url().spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);

  // TODO(gavinp): Remove this CHECK when we figure out the cause of
  // http://crbug.com/124680 . This check mirrors closely check in
  // WebURLLoaderImpl::OnCompletedRequest that routes this message to a WebCore
  // ResourceHandleInternal which asserts on its state and crashes. By crashing
  // when the message is sent, we should get better crash reports.
  CHECK(status.status() != net::URLRequestStatus::SUCCESS ||
        sent_received_response_msg_);

  TimeTicks completion_time = TimeTicks::Now();

  int error_code = status.error();
  bool was_ignored_by_handler =
      ResourceRequestInfoImpl::ForRequest(request_)->WasIgnoredByHandler();

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

  filter_->Send(new ResourceMsg_RequestComplete(routing_id_,
                                                request_id,
                                                error_code,
                                                was_ignored_by_handler,
                                                security_info,
                                                completion_time));
  return true;
}

bool AsyncResourceHandler::EnsureResourceBufferIsInitialized() {
  if (buffer_ && buffer_->IsInitialized())
    return true;

  buffer_ = new ResourceBuffer();
  return buffer_->Initialize(kBufferSize,
                             kMinAllocationSize,
                             kMaxAllocationSize);
}

void AsyncResourceHandler::ResumeIfDeferred() {
  if (did_defer_) {
    did_defer_ = false;
    controller()->Resume();
  }
}

}  // namespace content
