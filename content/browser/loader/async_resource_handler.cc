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
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
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
  virtual ~DependentIOBuffer() {}
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
      sent_first_data_msg_(false) {
  InitializeResourceBufferConstants();
}

AsyncResourceHandler::~AsyncResourceHandler() {
  if (has_checked_for_sufficient_resources_)
    rdh_->FinishedWithResourcesForRequest(request());
}

bool AsyncResourceHandler::OnMessageReceived(const IPC::Message& message,
                                             bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AsyncResourceHandler, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_FollowRedirect, OnFollowRedirect)
    IPC_MESSAGE_HANDLER(ResourceHostMsg_DataReceived_ACK, OnDataReceivedACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void AsyncResourceHandler::OnFollowRedirect(
    int request_id,
    bool has_new_first_party_for_cookies,
    const GURL& new_first_party_for_cookies) {
  if (!request()->status().is_success()) {
    DVLOG(1) << "OnFollowRedirect for invalid request";
    return;
  }

  if (has_new_first_party_for_cookies)
    request()->set_first_party_for_cookies(new_first_party_for_cookies);

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

bool AsyncResourceHandler::OnUploadProgress(int request_id,
                                            uint64 position,
                                            uint64 size) {
  ResourceMessageFilter* filter = GetFilter();
  if (!filter)
    return false;
  return filter->Send(
      new ResourceMsg_UploadProgress(request_id, position, size));
}

bool AsyncResourceHandler::OnRequestRedirected(int request_id,
                                               const GURL& new_url,
                                               ResourceResponse* response,
                                               bool* defer) {
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!info->filter())
    return false;

  *defer = did_defer_ = true;
  OnDefer();

  if (rdh_->delegate()) {
    rdh_->delegate()->OnRequestRedirected(
        new_url, request(), info->GetContext(), response);
  }

  DevToolsNetLogObserver::PopulateResponseInfo(request(), response);
  response->head.request_start = request()->creation_time();
  response->head.response_start = TimeTicks::Now();
  return info->filter()->Send(new ResourceMsg_ReceivedRedirect(
      request_id, new_url, response->head));
}

bool AsyncResourceHandler::OnResponseStarted(int request_id,
                                             ResourceResponse* response,
                                             bool* defer) {
  // For changes to the main frame, inform the renderer of the new URL's
  // per-host settings before the request actually commits.  This way the
  // renderer will be able to set these precisely at the time the
  // request commits, avoiding the possibility of e.g. zooming the old content
  // or of having to layout the new content twice.

  const ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!info->filter())
    return false;

  if (rdh_->delegate()) {
    rdh_->delegate()->OnResponseStarted(
        request(), info->GetContext(), response, info->filter());
  }

  DevToolsNetLogObserver::PopulateResponseInfo(request(), response);

  HostZoomMap* host_zoom_map =
      GetHostZoomMapForResourceContext(info->GetContext());

  if (info->GetResourceType() == ResourceType::MAIN_FRAME && host_zoom_map) {
    const GURL& request_url = request()->url();
    info->filter()->Send(new ViewMsg_SetZoomLevelForLoadingURL(
        info->GetRouteID(),
        request_url, host_zoom_map->GetZoomLevelForHostAndScheme(
            request_url.scheme(),
            net::GetHostOrSpecFromURL(request_url))));
  }

  response->head.request_start = request()->creation_time();
  response->head.response_start = TimeTicks::Now();
  info->filter()->Send(new ResourceMsg_ReceivedResponse(request_id,
                                                        response->head));
  sent_received_response_msg_ = true;

  if (request()->response_info().metadata.get()) {
    std::vector<char> copy(request()->response_info().metadata->data(),
                           request()->response_info().metadata->data() +
                               request()->response_info().metadata->size());
    info->filter()->Send(new ResourceMsg_ReceivedCachedMetadata(request_id,
                                                                copy));
  }

  return true;
}

bool AsyncResourceHandler::OnWillStart(int request_id,
                                       const GURL& url,
                                       bool* defer) {
  return true;
}

bool AsyncResourceHandler::OnWillRead(int request_id,
                                      scoped_refptr<net::IOBuffer>* buf,
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

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Net.AsyncResourceHandler_SharedIOBuffer_Alloc",
      *buf_size, 0, kMaxAllocationSize, 100);
  return true;
}

bool AsyncResourceHandler::OnReadCompleted(int request_id, int bytes_read,
                                           bool* defer) {
  DCHECK_GE(bytes_read, 0);

  if (!bytes_read)
    return true;

  ResourceMessageFilter* filter = GetFilter();
  if (!filter)
    return false;

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
    if (!buffer_->ShareToProcess(filter->PeerHandle(), &handle, &size))
      return false;
    filter->Send(new ResourceMsg_SetDataBuffer(
        request_id, handle, size, filter->peer_pid()));
    sent_first_data_msg_ = true;
  }

  int data_offset = buffer_->GetLastAllocationOffset();
  int encoded_data_length =
      DevToolsNetLogObserver::GetAndResetEncodedDataLength(request());

  filter->Send(new ResourceMsg_DataReceived(
      request_id, data_offset, bytes_read, encoded_data_length));
  ++pending_data_count_;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Net.AsyncResourceHandler_PendingDataCount",
      pending_data_count_, 0, 100, 100);

  if (!buffer_->CanAllocate()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Net.AsyncResourceHandler_PendingDataCount_WhenFull",
        pending_data_count_, 0, 100, 100);
    *defer = did_defer_ = true;
    OnDefer();
  }

  return true;
}

void AsyncResourceHandler::OnDataDownloaded(
    int request_id, int bytes_downloaded) {
  int encoded_data_length =
      DevToolsNetLogObserver::GetAndResetEncodedDataLength(request());

  ResourceMessageFilter* filter = GetFilter();
  if (filter) {
    filter->Send(new ResourceMsg_DataDownloaded(
        request_id, bytes_downloaded, encoded_data_length));
  }
}

void AsyncResourceHandler::OnResponseCompleted(
    int request_id,
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

  TimeTicks completion_time = TimeTicks::Now();

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

  info->filter()->Send(
      new ResourceMsg_RequestComplete(request_id,
                                      error_code,
                                      was_ignored_by_handler,
                                      security_info,
                                      completion_time));
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
