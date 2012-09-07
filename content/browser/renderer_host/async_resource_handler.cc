// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/async_resource_handler.h"

#include <algorithm>
#include <vector>

#include "base/debug/alias.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/shared_memory.h"
#include "content/browser/debugger/devtools_netlog_observer.h"
#include "content/browser/host_zoom_map_impl.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "content/browser/resource_context_impl.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_log.h"
#include "webkit/glue/resource_loader_bridge.h"

using base::TimeTicks;

namespace content {
namespace {

// When reading, we don't know if we are going to get EOF (0 bytes read), so
// we typically have a buffer that we allocated but did not use.  We keep
// this buffer around for the next read as a small optimization.
SharedIOBuffer* g_spare_read_buffer = NULL;

// The initial size of the shared memory buffer. (32 kilobytes).
const int kInitialReadBufSize = 32768;

// The maximum size of the shared memory buffer. (512 kilobytes).
const int kMaxReadBufSize = 524288;

// Maximum number of pending data messages sent to the renderer at any
// given time for a given request.
const int kMaxPendingDataMessages = 20;

int CalcUsedPercentage(int bytes_read, int buffer_size) {
  double ratio = static_cast<double>(bytes_read) / buffer_size;
  return static_cast<int>(ratio * 100.0 + 0.5);  // Round to nearest integer.
}

}  // namespace

// Our version of IOBuffer that uses shared memory.
class SharedIOBuffer : public net::IOBuffer {
 public:
  explicit SharedIOBuffer(int buffer_size)
      : net::IOBuffer(),
        ok_(false),
        buffer_size_(buffer_size) {}

  bool Init() {
    if (shared_memory_.CreateAndMapAnonymous(buffer_size_)) {
      data_ = reinterpret_cast<char*>(shared_memory_.memory());
      DCHECK(data_);
      ok_ = true;
    }
    return ok_;
  }

  base::SharedMemory* shared_memory() { return &shared_memory_; }
  bool ok() { return ok_; }
  int buffer_size() { return buffer_size_; }

 private:
  ~SharedIOBuffer() {
    DCHECK(g_spare_read_buffer != this);
    data_ = NULL;
  }

  base::SharedMemory shared_memory_;
  bool ok_;
  int buffer_size_;
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
      next_buffer_size_(kInitialReadBufSize),
      pending_data_count_(0),
      did_defer_(false),
      sent_received_response_msg_(false) {
  // Set a back-pointer from ResourceRequestInfoImpl to |this|, so that the
  // ResourceDispatcherHostImpl can send us IPC messages.
  // TODO(darin): Implement an IPC message filter instead?
  ResourceRequestInfoImpl::ForRequest(request_)->set_async_handler(this);
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
  if (pending_data_count_-- == kMaxPendingDataMessages)
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

  if (g_spare_read_buffer) {
    DCHECK(!read_buffer_);
    read_buffer_.swap(&g_spare_read_buffer);
    DCHECK(read_buffer_->data());

    *buf = read_buffer_.get();
    *buf_size = read_buffer_->buffer_size();
  } else {
    read_buffer_ = new SharedIOBuffer(next_buffer_size_);
    if (!read_buffer_->Init()) {
      DLOG(ERROR) << "Couldn't allocate shared io buffer";
      read_buffer_ = NULL;
      return false;
    }
    DCHECK(read_buffer_->data());
    *buf = read_buffer_.get();
    *buf_size = next_buffer_size_;

    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Net.AsyncResourceHandler_SharedIOBuffer_Alloc",
        next_buffer_size_, kInitialReadBufSize, kMaxReadBufSize, 16);
  }

  return true;
}

bool AsyncResourceHandler::OnReadCompleted(int request_id, int bytes_read,
                                           bool* defer) {
  if (!bytes_read)
    return true;
  DCHECK(read_buffer_.get());

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Net.AsyncResourceHandler_SharedIOBuffer_Used",
      bytes_read, 0, kMaxReadBufSize, 100);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.AsyncResourceHandler_SharedIOBuffer_UsedPercentage",
      CalcUsedPercentage(bytes_read, read_buffer_->buffer_size()));

  if (read_buffer_->buffer_size() == bytes_read) {
    // The network layer has saturated our buffer. Next time, we should give it
    // a bigger buffer for it to fill, to minimize the number of round trips we
    // do with the renderer process.
    next_buffer_size_ = std::min(next_buffer_size_ * 2, kMaxReadBufSize);
  }

  WillSendData(defer);

  base::SharedMemoryHandle handle;
  if (!read_buffer_->shared_memory()->GiveToProcess(
          filter_->peer_handle(), &handle)) {
    // We wrongfully incremented the pending data count. Fake an ACK message
    // to fix this. We can't move this call above the WillSendData because
    // it's killing our read_buffer_, and we don't want that when we pause
    // the request.
    OnDataReceivedACK();

    // We just unmapped the memory.
    read_buffer_ = NULL;
    return false;
  }
  // We just unmapped the memory.
  read_buffer_ = NULL;

  int encoded_data_length =
      DevToolsNetLogObserver::GetAndResetEncodedDataLength(request_);
  filter_->Send(new ResourceMsg_DataReceived(
      routing_id_, request_id, handle, bytes_read, encoded_data_length));

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

  // If we still have a read buffer, then see about caching it for later...
  // Note that we have to make sure the buffer is not still being used, so we
  // have to perform an explicit check on the status code.
  if (g_spare_read_buffer ||
      net::URLRequestStatus::SUCCESS != status.status()) {
    read_buffer_ = NULL;
  } else if (read_buffer_.get()) {
    DCHECK(read_buffer_->data());
    read_buffer_.swap(&g_spare_read_buffer);
  }
  return true;
}

// static
void AsyncResourceHandler::GlobalCleanup() {
  if (g_spare_read_buffer) {
    // Avoid the CHECK in SharedIOBuffer::~SharedIOBuffer().
    SharedIOBuffer* tmp = g_spare_read_buffer;
    g_spare_read_buffer = NULL;
    tmp->Release();
  }
}

void AsyncResourceHandler::WillSendData(bool* defer) {
  if (++pending_data_count_ >= kMaxPendingDataMessages) {
    // We reached the max number of data messages that can be sent to
    // the renderer for a given request. Pause the request and wait for
    // the renderer to start processing them before resuming it.
    *defer = did_defer_ = true;
  }

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Net.AsyncResourceHandler_PendingDataCount",
      pending_data_count_, 0, kMaxPendingDataMessages, kMaxPendingDataMessages);
}

void AsyncResourceHandler::ResumeIfDeferred() {
  if (did_defer_) {
    did_defer_ = false;
    controller()->Resume();
  }
}

}  // namespace content
