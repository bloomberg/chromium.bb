// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/async_resource_handler.h"

#include <algorithm>
#include <vector>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/shared_memory.h"
#include "content/browser/debugger/devtools_netlog_observer.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/renderer_host/global_request_id.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/browser/renderer_host/resource_message_filter.h"
#include "content/browser/resource_context.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_log.h"
#include "webkit/glue/resource_loader_bridge.h"

using base::Time;
using base::TimeTicks;

namespace {

// When reading, we don't know if we are going to get EOF (0 bytes read), so
// we typically have a buffer that we allocated but did not use.  We keep
// this buffer around for the next read as a small optimization.
SharedIOBuffer* g_spare_read_buffer = NULL;

// The initial size of the shared memory buffer. (32 kilobytes).
const int kInitialReadBufSize = 32768;

// The maximum size of the shared memory buffer. (512 kilobytes).
const int kMaxReadBufSize = 524288;

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
    const GURL& url,
    ResourceDispatcherHost* resource_dispatcher_host)
    : filter_(filter),
      routing_id_(routing_id),
      host_zoom_map_(NULL),
      rdh_(resource_dispatcher_host),
      next_buffer_size_(kInitialReadBufSize) {
}

AsyncResourceHandler::~AsyncResourceHandler() {
}

bool AsyncResourceHandler::OnUploadProgress(int request_id,
                                            uint64 position,
                                            uint64 size) {
  return filter_->Send(new ResourceMsg_UploadProgress(routing_id_, request_id,
                                                      position, size));
}

bool AsyncResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& new_url,
    content::ResourceResponse* response,
    bool* defer) {
  *defer = true;
  net::URLRequest* request = rdh_->GetURLRequest(
      GlobalRequestID(filter_->child_id(), request_id));
  if (rdh_->delegate())
    rdh_->delegate()->OnRequestRedirected(request, response, filter_);

  DevToolsNetLogObserver::PopulateResponseInfo(request, response);
  return filter_->Send(new ResourceMsg_ReceivedRedirect(
      routing_id_, request_id, new_url, *response));
}

bool AsyncResourceHandler::OnResponseStarted(
    int request_id,
    content::ResourceResponse* response) {
  // For changes to the main frame, inform the renderer of the new URL's
  // per-host settings before the request actually commits.  This way the
  // renderer will be able to set these precisely at the time the
  // request commits, avoiding the possibility of e.g. zooming the old content
  // or of having to layout the new content twice.
  net::URLRequest* request = rdh_->GetURLRequest(
      GlobalRequestID(filter_->child_id(), request_id));

  if (rdh_->delegate())
    rdh_->delegate()->OnResponseStarted(request, response, filter_);

  DevToolsNetLogObserver::PopulateResponseInfo(request, response);

  const content::ResourceContext& resource_context =
      filter_->resource_context();
  HostZoomMap* host_zoom_map = resource_context.host_zoom_map();

  ResourceDispatcherHostRequestInfo* info = rdh_->InfoForRequest(request);
  if (info->resource_type() == ResourceType::MAIN_FRAME && host_zoom_map) {
    GURL request_url(request->url());
    filter_->Send(new ViewMsg_SetZoomLevelForLoadingURL(
        info->route_id(),
        request_url, host_zoom_map->GetZoomLevel(net::GetHostOrSpecFromURL(
            request_url))));
  }

  filter_->Send(new ResourceMsg_ReceivedResponse(
      routing_id_, request_id, *response));

  if (request->response_info().metadata) {
    std::vector<char> copy(request->response_info().metadata->data(),
                           request->response_info().metadata->data() +
                           request->response_info().metadata->size());
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
  }

  return true;
}

bool AsyncResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  if (!*bytes_read)
    return true;
  DCHECK(read_buffer_.get());

  if (read_buffer_->buffer_size() == *bytes_read) {
    // The network layer has saturated our buffer. Next time, we should give it
    // a bigger buffer for it to fill, to minimize the number of round trips we
    // do with the renderer process.
    next_buffer_size_ = std::min(next_buffer_size_ * 2, kMaxReadBufSize);
  }

  if (!rdh_->WillSendData(filter_->child_id(), request_id)) {
    // We should not send this data now, we have too many pending requests.
    return true;
  }

  base::SharedMemoryHandle handle;
  if (!read_buffer_->shared_memory()->GiveToProcess(
          filter_->peer_handle(), &handle)) {
    // We wrongfully incremented the pending data count. Fake an ACK message
    // to fix this. We can't move this call above the WillSendData because
    // it's killing our read_buffer_, and we don't want that when we pause
    // the request.
    rdh_->DataReceivedACK(filter_->child_id(), request_id);
    // We just unmapped the memory.
    read_buffer_ = NULL;
    return false;
  }
  // We just unmapped the memory.
  read_buffer_ = NULL;

  net::URLRequest* request = rdh_->GetURLRequest(
      GlobalRequestID(filter_->child_id(), request_id));
  int encoded_data_length =
      DevToolsNetLogObserver::GetAndResetEncodedDataLength(request);
  filter_->Send(new ResourceMsg_DataReceived(
      routing_id_, request_id, handle, *bytes_read, encoded_data_length));

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
  Time completion_time = Time::Now();
  filter_->Send(new ResourceMsg_RequestComplete(routing_id_,
                                                request_id,
                                                status,
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

void AsyncResourceHandler::OnRequestClosed() {
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
