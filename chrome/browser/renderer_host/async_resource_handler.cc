// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/async_resource_handler.h"

#include "base/logging.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/passive_log_collector.h"
#include "chrome/browser/renderer_host/global_request_id.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/common/render_messages.h"
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

// We know that this conversion is not solid and suffers from world clock
// changes, but it should be good enough for the load timing info.
static Time TimeTicksToTime(const TimeTicks& time_ticks) {
  static int64 tick_to_time_offset;
  static bool tick_to_time_offset_available = false;
  if (!tick_to_time_offset_available) {
    int64 cur_time = (Time::Now() - Time()).InMicroseconds();
    int64 cur_time_ticks = (TimeTicks::Now() - TimeTicks()).InMicroseconds();
    // If we add this number to a time tick value, it gives the timestamp.
    tick_to_time_offset = cur_time - cur_time_ticks;
    tick_to_time_offset_available = true;
  }
  return Time::FromInternalValue(time_ticks.ToInternalValue() +
                                 tick_to_time_offset);
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

void AsyncResourceHandler::PopulateTimingInfo(URLRequest* request,
                                              ResourceResponse* response) {
  uint32 source_id = request->net_log().source().id;
  ChromeNetLog* chrome_net_log = static_cast<ChromeNetLog*>(
      request->net_log().net_log());

  PassiveLogCollector* collector = chrome_net_log->passive_collector();
  PassiveLogCollector::SourceTracker* url_tracker =
      static_cast<PassiveLogCollector::SourceTracker*>(collector->
          GetTrackerForSourceType(net::NetLog::SOURCE_URL_REQUEST));

  PassiveLogCollector::SourceInfo* url_request =
      url_tracker->GetSourceInfo(source_id);

  if (!url_request)
    return;

  ResourceResponseHead& response_head = response->response_head;
  webkit_glue::ResourceLoaderBridge::LoadTimingInfo& timing =
      response_head.load_timing;

  uint32 connect_job_id = net::NetLog::Source::kInvalidId;

  base::TimeTicks base_time;

  for (PassiveLogCollector::EntryList::const_iterator it =
           url_request->entries.begin();
       it != url_request->entries.end(); ++it) {
    const PassiveLogCollector::Entry& entry = *it;

    bool is_begin = entry.phase == net::NetLog::PHASE_BEGIN;
    bool is_end = entry.phase == net::NetLog::PHASE_END;

    switch (entry.type) {
      case net::NetLog::TYPE_URL_REQUEST_START_JOB:
        if (is_begin) {
          // Reset state so that we captured last redirect only.
          timing.base_time = TimeTicksToTime(entry.time);
          base_time = entry.time;
          connect_job_id = net::NetLog::Source::kInvalidId;
        }
        break;
      case net::NetLog::TYPE_PROXY_SERVICE:
        if (is_begin) {
          timing.proxy_start = static_cast<int32>(
              (entry.time - base_time).InMillisecondsRoundedUp());
        } else if (is_end) {
          timing.proxy_end = static_cast<int32>(
              (entry.time - base_time).InMillisecondsRoundedUp());
        }
        break;
      case net::NetLog::TYPE_SOCKET_POOL:
        if (is_begin) {
          timing.connect_start = static_cast<int32>(
              (entry.time - base_time).InMillisecondsRoundedUp());
        } else if (is_end &&
                       connect_job_id != net::NetLog::Source::kInvalidId) {
          timing.connect_end = static_cast<int32>(
              (entry.time - base_time).InMillisecondsRoundedUp());
        }
        break;
      case net::NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB:
        connect_job_id = static_cast<net::NetLogSourceParameter*>(
            entry.params.get())->value().id;
        break;
      case net::NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET:
        {
          uint32 log_id = static_cast<net::NetLogSourceParameter*>(
              entry.params.get())->value().id;
          response->response_head.connection_id =
              log_id != net::NetLog::Source::kInvalidId ? log_id : 0;
        }
        break;
      case net::NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST:
      case net::NetLog::TYPE_SPDY_TRANSACTION_SEND_REQUEST:
        if (is_begin) {
          timing.send_start = static_cast<int32>(
              (entry.time - base_time).InMillisecondsRoundedUp());
        } else if (is_end) {
          timing.send_end = static_cast<int32>(
              (entry.time - base_time).InMillisecondsRoundedUp());
        }
        break;
      case net::NetLog::TYPE_HTTP_TRANSACTION_READ_HEADERS:
      case net::NetLog::TYPE_SPDY_TRANSACTION_READ_HEADERS:
        if (is_begin) {
          timing.receive_headers_start =  static_cast<int32>(
              (entry.time - base_time).InMillisecondsRoundedUp());
        } else if (is_end) {
          timing.receive_headers_end =  static_cast<int32>(
              (entry.time - base_time).InMillisecondsRoundedUp());
        }
        break;
      default:
        break;
    }
  }

  // For DNS time, get the ID of the "connect job" from the
  // BOUND_TO_CONNECT_JOB entry, in its source info look at the
  // HOST_RESOLVER_IMPL times.
  if (connect_job_id == net::NetLog::Source::kInvalidId) {
    // Clean up connection time to match contract.
    timing.connect_start = -1;
    timing.connect_end = -1;
    return;
  }

  PassiveLogCollector::SourceTracker* connect_job_tracker =
      static_cast<PassiveLogCollector::SourceTracker*>(
          collector->GetTrackerForSourceType(net::NetLog::SOURCE_CONNECT_JOB));
  PassiveLogCollector::SourceInfo* connect_job =
      connect_job_tracker->GetSourceInfo(connect_job_id);
  if (!connect_job)
    return;

  for (PassiveLogCollector::EntryList::const_iterator it =
           connect_job->entries.begin();
       it != connect_job->entries.end(); ++it) {
    const PassiveLogCollector::Entry& entry = *it;
    if (entry.phase == net::NetLog::PHASE_BEGIN &&
        entry.type == net::NetLog::TYPE_HOST_RESOLVER_IMPL) {
      timing.dns_start = static_cast<int32>(
          (entry.time - base_time).InMillisecondsRoundedUp());
    } else if (entry.phase == net::NetLog::PHASE_END &&
          entry.type == net::NetLog::TYPE_HOST_RESOLVER_IMPL) {
      timing.dns_end = static_cast<int32>(
          (entry.time - base_time).InMillisecondsRoundedUp());
      // Connect time already includes dns time, subtract it here.
      break;
    }
  }
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
  URLRequest* request = rdh_->GetURLRequest(
      GlobalRequestID(process_id_, request_id));
  // TODO(pfeldman): enable once migrated to LoadTimingObserver.
  if (false && request && (request->load_flags() & net::LOAD_ENABLE_LOAD_TIMING))
    PopulateTimingInfo(request, response);
  return receiver_->Send(new ViewMsg_Resource_ReceivedRedirect(
      routing_id_, request_id, new_url, response->response_head));
}

bool AsyncResourceHandler::OnResponseStarted(int request_id,
                                             ResourceResponse* response) {
  // For changes to the main frame, inform the renderer of the new URL's
  // per-host settings before the request actually commits.  This way the
  // renderer will be able to set these precisely at the time the
  // request commits, avoiding the possibility of e.g. zooming the old content
  // or of having to layout the new content twice.
  URLRequest* request = rdh_->GetURLRequest(
      GlobalRequestID(process_id_, request_id));

  // TODO(pfeldman): enable once migrated to LoadTimingObserver.
  if (false && request->load_flags() & net::LOAD_ENABLE_LOAD_TIMING)
    PopulateTimingInfo(request, response);

  ResourceDispatcherHostRequestInfo* info = rdh_->InfoForRequest(request);
  if (info->resource_type() == ResourceType::MAIN_FRAME) {
    GURL request_url(request->url());
    ChromeURLRequestContext* context =
        static_cast<ChromeURLRequestContext*>(request->context());
    if (context) {
      receiver_->Send(new ViewMsg_SetContentSettingsForLoadingURL(
          info->route_id(), request_url,
          context->host_content_settings_map()->GetContentSettings(
              request_url)));
      receiver_->Send(new ViewMsg_SetZoomLevelForLoadingURL(info->route_id(),
          request_url, context->host_zoom_map()->GetZoomLevel(request_url)));
    }
  }

  receiver_->Send(new ViewMsg_Resource_ReceivedResponse(
      routing_id_, request_id, response->response_head));

  if (request->response_info().metadata) {
    std::vector<char> copy(request->response_info().metadata->data(),
                           request->response_info().metadata->data() +
                           request->response_info().metadata->size());
    receiver_->Send(new ViewMsg_Resource_ReceivedCachedMetadata(
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
