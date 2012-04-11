// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_resource_handler.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/stringprintf.h"
#include "content/browser/download/download_buffer.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context.h"

using content::BrowserThread;
using content::DownloadId;
using content::DownloadItem;
using content::DownloadManager;
using content::ResourceDispatcherHostImpl;
using content::ResourceRequestInfoImpl;

namespace {

void CallStartedCBOnUIThread(
    const DownloadResourceHandler::OnStartedCallback& started_cb,
    DownloadId id,
    net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (started_cb.is_null())
    return;
  started_cb.Run(id, error);
}

}  // namespace

DownloadResourceHandler::DownloadResourceHandler(
    int render_process_host_id,
    int render_view_id,
    int request_id,
    const GURL& url,
    DownloadFileManager* download_file_manager,
    net::URLRequest* request,
    const DownloadResourceHandler::OnStartedCallback& started_cb,
    const content::DownloadSaveInfo& save_info)
    : download_id_(DownloadId::Invalid()),
      global_id_(render_process_host_id, request_id),
      render_view_id_(render_view_id),
      content_length_(0),
      download_file_manager_(download_file_manager),
      request_(request),
      started_cb_(started_cb),
      save_info_(save_info),
      buffer_(new content::DownloadBuffer),
      is_paused_(false),
      last_buffer_size_(0),
      bytes_read_(0) {
  download_stats::RecordDownloadCount(download_stats::UNTHROTTLED_COUNT);
}

bool DownloadResourceHandler::OnUploadProgress(int request_id,
                                               uint64 position,
                                               uint64 size) {
  return true;
}

// Not needed, as this event handler ought to be the final resource.
bool DownloadResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& url,
    content::ResourceResponse* response,
    bool* defer) {
  return true;
}

// Send the download creation information to the download thread.
bool DownloadResourceHandler::OnResponseStarted(
    int request_id,
    content::ResourceResponse* response) {
  VLOG(20) << __FUNCTION__ << "()" << DebugString()
           << " request_id = " << request_id;
  download_start_time_ = base::TimeTicks::Now();

  if (request_->url().scheme() == "data") {
    CallStartedCB(download_id_, net::ERR_DISALLOWED_URL_SCHEME);
    return false;
  }

  // If it's a download, we don't want to poison the cache with it.
  request_->StopCaching();

  std::string content_disposition;
  request_->GetResponseHeaderByName("content-disposition",
                                    &content_disposition);
  set_content_disposition(content_disposition);
  set_content_length(response->content_length);

  const ResourceRequestInfoImpl* request_info =
      ResourceRequestInfoImpl::ForRequest(request_);

  // Deleted in DownloadManager.
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo(
      base::Time::Now(), 0, content_length_, DownloadItem::IN_PROGRESS,
      request_->net_log(), request_info->has_user_gesture(),
      request_info->transition_type()));
  info->url_chain = request_->url_chain();
  info->referrer_url = GURL(request_->referrer());
  info->start_time = base::Time::Now();
  info->received_bytes = save_info_.offset;
  info->total_bytes = content_length_;
  info->state = DownloadItem::IN_PROGRESS;
  info->has_user_gesture = request_info->has_user_gesture();
  info->content_disposition = content_disposition_;
  info->mime_type = response->mime_type;
  info->remote_address = request_->GetSocketAddress().host();
  download_stats::RecordDownloadMimeType(info->mime_type);

  DownloadRequestHandle request_handle(global_id_.child_id,
                                       render_view_id_, global_id_.request_id);

  // Get the last modified time and etag.
  const net::HttpResponseHeaders* headers = request_->response_headers();
  if (headers) {
    std::string last_modified_hdr;
    std::string etag;
    if (headers->EnumerateHeader(NULL, "Last-Modified", &last_modified_hdr))
      info->last_modified = last_modified_hdr;
    if (headers->EnumerateHeader(NULL, "ETag", &etag))
      info->etag = etag;
  }

  std::string content_type_header;
  if (!response->headers ||
      !response->headers->GetMimeType(&content_type_header))
    content_type_header = "";
  info->original_mime_type = content_type_header;

  if (!response->headers ||
      !response->headers->EnumerateHeader(NULL,
                                          "Accept-Ranges",
                                          &accept_ranges_)) {
    accept_ranges_ = "";
  }

  info->prompt_user_for_save_location =
      save_info_.prompt_for_save_location && save_info_.file_path.empty();
  info->referrer_charset = request_->context()->referrer_charset();
  info->save_info = save_info_;


  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadResourceHandler::StartOnUIThread, this,
                 base::Passed(&info), request_handle));

  // We can't start saving the data before we create the file on disk and have a
  // download id. The request will be un-paused in
  // DownloadFileManager::CreateDownloadFile.
  ResourceDispatcherHostImpl::Get()->PauseRequest(global_id_.child_id,
                                                  global_id_.request_id,
                                                  true);

  return true;
}

void DownloadResourceHandler::CallStartedCB(DownloadId id, net::Error error) {
  if (started_cb_.is_null())
    return;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CallStartedCBOnUIThread, started_cb_, id, error));
  started_cb_.Reset();
}

bool DownloadResourceHandler::OnWillStart(int request_id,
                                          const GURL& url,
                                          bool* defer) {
  return true;
}

// Create a new buffer, which will be handed to the download thread for file
// writing and deletion.
bool DownloadResourceHandler::OnWillRead(int request_id, net::IOBuffer** buf,
                                         int* buf_size, int min_size) {
  DCHECK(buf && buf_size);
  if (!read_buffer_) {
    *buf_size = min_size < 0 ? kReadBufSize : min_size;
    last_buffer_size_ = *buf_size;
    read_buffer_ = new net::IOBuffer(*buf_size);
  }
  *buf = read_buffer_.get();
  return true;
}

// Pass the buffer to the download file writer.
bool DownloadResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  base::TimeTicks now(base::TimeTicks::Now());
  if (!last_read_time_.is_null()) {
    double seconds_since_last_read = (now - last_read_time_).InSecondsF();
    if (now == last_read_time_)
      // Use 1/10 ms as a "very small number" so that we avoid
      // divide-by-zero error and still record a very high potential bandwidth.
      seconds_since_last_read = 0.00001;

    double actual_bandwidth = (*bytes_read)/seconds_since_last_read;
    double potential_bandwidth = last_buffer_size_/seconds_since_last_read;
    download_stats::RecordBandwidth(actual_bandwidth, potential_bandwidth);
  }
  last_read_time_ = now;

  if (!*bytes_read)
    return true;
  bytes_read_ += *bytes_read;
  DCHECK(read_buffer_);
  // Swap the data.
  net::IOBuffer* io_buffer = NULL;
  read_buffer_.swap(&io_buffer);
  size_t vector_size = buffer_->AddData(io_buffer, *bytes_read);
  bool need_update = (vector_size == 1);  // Buffer was empty.

  // We are passing ownership of this buffer to the download file manager.
  if (need_update) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadFileManager::UpdateDownload,
                   download_file_manager_, download_id_, buffer_));
  }

  // We schedule a pause outside of the read loop if there is too much file
  // writing work to do.
  if (vector_size > kLoadsToWrite)
    StartPauseTimer();

  return true;
}

bool DownloadResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  VLOG(20) << __FUNCTION__ << "()" << DebugString()
           << " request_id = " << request_id
           << " status.status() = " << status.status()
           << " status.error() = " << status.error();
  int response = status.is_success() ? request_->GetResponseCode() : 0;
  if (download_id_.IsValid()) {
    OnResponseCompletedInternal(request_id, status, security_info, response);
  } else {
    // We got cancelled before the task which sets the id ran on the IO thread.
    // Wait for it.
    BrowserThread::PostTaskAndReply(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&base::DoNothing),
        base::Bind(&DownloadResourceHandler::OnResponseCompletedInternal, this,
                   request_id, status, security_info, response));
  }
  // Can't trust request_ being value after this point.
  request_ = NULL;
  return true;
}

void DownloadResourceHandler::OnResponseCompletedInternal(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info,
    int response_code) {
  // NOTE: |request_| may be a dangling pointer at this point.
  VLOG(20) << __FUNCTION__ << "()"
           << " request_id = " << request_id
           << " status.status() = " << status.status()
           << " status.error() = " << status.error();
  net::Error error_code = net::OK;
  if (status.status() == net::URLRequestStatus::FAILED)
    error_code = static_cast<net::Error>(status.error());  // Normal case.
  // ERR_CONNECTION_CLOSED is allowed since a number of servers in the wild
  // advertise a larger Content-Length than the amount of bytes in the message
  // body, and then close the connection. Other browsers - IE8, Firefox 4.0.1,
  // and Safari 5.0.4 - treat the download as complete in this case, so we
  // follow their lead.
  if (error_code == net::ERR_CONNECTION_CLOSED)
    error_code = net::OK;
  content::DownloadInterruptReason reason =
      content::ConvertNetErrorToInterruptReason(
        error_code, content::DOWNLOAD_INTERRUPT_FROM_NETWORK);

  if ((status.status() == net::URLRequestStatus::CANCELED) &&
      (status.error() == net::ERR_ABORTED)) {
    // TODO(ahendrickson) -- Find a better set of codes to use here, as
    // CANCELED/ERR_ABORTED can occur for reasons other than user cancel.
    reason = content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
  }

  if (status.is_success()) {
    if (response_code >= 400) {
      switch(response_code) {
        case 404:  // File Not Found.
          reason = content::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
          break;
        case 416:  // Range Not Satisfiable.
          reason = content::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;
          break;
        case 412:  // Precondition Failed.
          reason = content::DOWNLOAD_INTERRUPT_REASON_SERVER_PRECONDITION;
          break;
        default:
          reason = content::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
          break;
      }
    }
  }

  download_stats::RecordAcceptsRanges(accept_ranges_, bytes_read_);

  // If the callback was already run on the UI thread, this will be a noop.
  CallStartedCB(download_id_, error_code);

  // We transfer ownership to |DownloadFileManager| to delete |buffer_|,
  // so that any functions queued up on the FILE thread are executed
  // before deletion.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFileManager::OnResponseCompleted,
                 download_file_manager_, download_id_, reason, security_info));
  buffer_ = NULL;  // The buffer is longer needed by |DownloadResourceHandler|.
  read_buffer_ = NULL;
}

void DownloadResourceHandler::OnRequestClosed() {
  UMA_HISTOGRAM_TIMES("SB2.DownloadDuration",
                      base::TimeTicks::Now() - download_start_time_);
}

void DownloadResourceHandler::StartOnUIThread(
    scoped_ptr<DownloadCreateInfo> info,
    const DownloadRequestHandle& handle) {
  DownloadManager* download_manager = handle.GetDownloadManager();
  if (!download_manager) {
    // NULL in unittests or if the page closed right after starting the
    // download.
    CallStartedCB(download_id_, net::ERR_ACCESS_DENIED);
    return;
  }
  DownloadId download_id = download_manager->delegate()->GetNextId();
  info->download_id = download_id;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DownloadResourceHandler::set_download_id, this,
                 info->download_id));
  // It's safe to continue on with download initiation before we have
  // confirmation that that download_id_ has been set on the IO thread, as any
  // messages generated by the UI thread that affect the IO thread will be
  // behind the message posted above.
  download_file_manager_->StartDownload(info.release(), handle);
  CallStartedCB(download_id, net::OK);
}

void DownloadResourceHandler::set_download_id(content::DownloadId id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  download_id_ = id;
}

// If the content-length header is not present (or contains something other
// than numbers), the incoming content_length is -1 (unknown size).
// Set the content length to 0 to indicate unknown size to DownloadManager.
void DownloadResourceHandler::set_content_length(const int64& content_length) {
  content_length_ = 0;
  if (content_length > 0)
    content_length_ = content_length;
}

void DownloadResourceHandler::set_content_disposition(
    const std::string& content_disposition) {
  content_disposition_ = content_disposition;
}

void DownloadResourceHandler::CheckWriteProgress() {
  if (!buffer_.get())
    return;  // The download completed while we were waiting to run.

  size_t contents_size = buffer_->size();

  bool should_pause = contents_size > kLoadsToWrite;

  // We'll come back later and see if it's okay to unpause the request.
  if (should_pause)
    StartPauseTimer();

  if (is_paused_ != should_pause) {
    ResourceDispatcherHostImpl::Get()->PauseRequest(global_id_.child_id,
                                                    global_id_.request_id,
                                                    should_pause);
    is_paused_ = should_pause;
  }
}

DownloadResourceHandler::~DownloadResourceHandler() {
  // This won't do anything if the callback was called before.
  // If it goes through, it will likely be because OnWillStart() returned
  // false somewhere in the chain of resource handlers.
  CallStartedCB(download_id_, net::ERR_ACCESS_DENIED);
}

void DownloadResourceHandler::StartPauseTimer() {
  if (!pause_timer_.IsRunning())
    pause_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kThrottleTimeMs), this,
                       &DownloadResourceHandler::CheckWriteProgress);
}

std::string DownloadResourceHandler::DebugString() const {
  return base::StringPrintf("{"
                            " url_ = " "\"%s\""
                            " download_id_ = " "%d"
                            " global_id_ = {"
                            " child_id = " "%d"
                            " request_id = " "%d"
                            " }"
                            " render_view_id_ = " "%d"
                            " save_info_.file_path = \"%" PRFilePath "\""
                            " }",
                            request_ ?
                                request_->url().spec().c_str() :
                                "<NULL request>",
                            download_id_.local(),
                            global_id_.child_id,
                            global_id_.request_id,
                            render_view_id_,
                            save_info_.file_path.value().c_str());
}
