// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/async_resource_handler.h"

#include "base/logging.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/common/render_messages.h"
#include "net/base/io_buffer.h"

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
    if (shared_memory_.Create(std::wstring(), false, false, buffer_size_) &&
        shared_memory_.Map(buffer_size_)) {
      data_ = reinterpret_cast<char*>(shared_memory_.memory());
      // TODO(hawk): Remove after debugging bug 16371.
      CHECK(data_);
      ok_ = true;
    }
    return ok_;
  }

  base::SharedMemory* shared_memory() { return &shared_memory_; }
  bool ok() { return ok_; }
  int buffer_size() { return buffer_size_; }

 private:
  ~SharedIOBuffer() {
    // TODO(willchan): Remove after debugging bug 16371.
    CHECK(g_spare_read_buffer != this);
    data_ = NULL;
  }

  base::SharedMemory shared_memory_;
  bool ok_;
  int buffer_size_;
};

AsyncResourceHandler::AsyncResourceHandler(
    ResourceDispatcherHost::Receiver* receiver,
    int process_id,
    int routing_id,
    base::ProcessHandle process_handle,
    const GURL& url,
    ResourceDispatcherHost* resource_dispatcher_host)
    : receiver_(receiver),
      process_id_(process_id),
      routing_id_(routing_id),
      process_handle_(process_handle),
      rdh_(resource_dispatcher_host),
      next_buffer_size_(kInitialReadBufSize) {
}

AsyncResourceHandler::~AsyncResourceHandler() {
}

bool AsyncResourceHandler::OnUploadProgress(int request_id,
                                            uint64 position,
                                            uint64 size) {
  return receiver_->Send(new ViewMsg_Resource_UploadProgress(routing_id_,
                                                             request_id,
                                                             position, size));
}

bool AsyncResourceHandler::OnRequestRedirected(int request_id,
                                               const GURL& new_url,
                                               ResourceResponse* response,
                                               bool* defer) {
  *defer = true;
  return receiver_->Send(new ViewMsg_Resource_ReceivedRedirect(
      routing_id_, request_id, new_url, response->response_head));
}

bool AsyncResourceHandler::OnResponseStarted(int request_id,
                                             ResourceResponse* response) {
  // For changes to the main frame, inform the renderer of the new URL's zoom
  // level before the request actually commits.  This way the renderer will be
  // able to set the zoom level precisely at the time the request commits,
  // avoiding the possibility of zooming the old content or of having to layout
  // the new content twice.
  URLRequest* request = rdh_->GetURLRequest(
      ResourceDispatcherHost::GlobalRequestID(process_id_, request_id));
  ResourceDispatcherHostRequestInfo* info = rdh_->InfoForRequest(request);
  if (info->resource_type() == ResourceType::MAIN_FRAME) {
    std::string host(request->url().host());
    ChromeURLRequestContext* context =
        static_cast<ChromeURLRequestContext*>(request->context());
    if (!host.empty() && context) {
      receiver_->Send(new ViewMsg_SetZoomLevelForLoadingHost(info->route_id(),
          host, context->host_zoom_map()->GetZoomLevel(host)));
    }
  }

  receiver_->Send(new ViewMsg_Resource_ReceivedResponse(
      routing_id_, request_id, response->response_head));
  return true;
}

bool AsyncResourceHandler::OnWillRead(int request_id, net::IOBuffer** buf,
                                      int* buf_size, int min_size) {
  DCHECK(min_size == -1);

  if (g_spare_read_buffer) {
    DCHECK(!read_buffer_);
    read_buffer_.swap(&g_spare_read_buffer);
    // TODO(willchan): Remove after debugging bug 16371.
    CHECK(read_buffer_->data());

    *buf = read_buffer_.get();
    *buf_size = read_buffer_->buffer_size();
  } else {
    read_buffer_ = new SharedIOBuffer(next_buffer_size_);
    if (!read_buffer_->Init()) {
      DLOG(ERROR) << "Couldn't allocate shared io buffer";
      read_buffer_ = NULL;
      return false;
    }
    // TODO(willchan): Remove after debugging bug 16371.
    CHECK(read_buffer_->data());
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

  if (!rdh_->WillSendData(process_id_, request_id)) {
    // We should not send this data now, we have too many pending requests.
    return true;
  }

  base::SharedMemoryHandle handle;
  if (!read_buffer_->shared_memory()->GiveToProcess(process_handle_, &handle)) {
    // We wrongfully incremented the pending data count. Fake an ACK message
    // to fix this. We can't move this call above the WillSendData because
    // it's killing our read_buffer_, and we don't want that when we pause
    // the request.
    rdh_->DataReceivedACK(process_id_, request_id);
    // We just unmapped the memory.
    read_buffer_ = NULL;
    return false;
  }
  // We just unmapped the memory.
  read_buffer_ = NULL;

  receiver_->Send(new ViewMsg_Resource_DataReceived(
      routing_id_, request_id, handle, *bytes_read));

  return true;
}

bool AsyncResourceHandler::OnResponseCompleted(
    int request_id,
    const URLRequestStatus& status,
    const std::string& security_info) {
  receiver_->Send(new ViewMsg_Resource_RequestComplete(routing_id_,
                                                       request_id,
                                                       status,
                                                       security_info));

  // If we still have a read buffer, then see about caching it for later...
  if (g_spare_read_buffer) {
    read_buffer_ = NULL;
  } else if (read_buffer_.get()) {
    // TODO(willchan): Remove after debugging bug 16371.
    CHECK(read_buffer_->data());
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
