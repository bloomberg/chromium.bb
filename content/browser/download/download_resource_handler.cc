// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/download/interrupt_reasons.h"
#include "content/browser/renderer_host/global_request_id.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context.h"

using content::BrowserThread;

DownloadResourceHandler::DownloadResourceHandler(
    ResourceDispatcherHost* rdh,
    int render_process_host_id,
    int render_view_id,
    int request_id,
    const GURL& url,
    DownloadId dl_id,
    DownloadFileManager* download_file_manager,
    net::URLRequest* request,
    bool save_as,
    const DownloadResourceHandler::OnStartedCallback& started_cb,
    const DownloadSaveInfo& save_info)
    : download_id_(dl_id),
      global_id_(render_process_host_id, request_id),
      render_view_id_(render_view_id),
      content_length_(0),
      download_file_manager_(download_file_manager),
      request_(request),
      save_as_(save_as),
      started_cb_(started_cb),
      save_info_(save_info),
      buffer_(new content::DownloadBuffer),
      rdh_(rdh),
      is_paused_(false),
      last_buffer_size_(0),
      bytes_read_(0) {
  DCHECK(dl_id.IsValid());
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
  DCHECK(download_id_.IsValid());
  VLOG(20) << __FUNCTION__ << "()" << DebugString()
           << " request_id = " << request_id;
  download_start_time_ = base::TimeTicks::Now();

  // If it's a download, we don't want to poison the cache with it.
  request_->StopCaching();

  std::string content_disposition;
  request_->GetResponseHeaderByName("content-disposition",
                                    &content_disposition);
  set_content_disposition(content_disposition);
  set_content_length(response->content_length);

  const ResourceDispatcherHostRequestInfo* request_info =
    ResourceDispatcherHost::InfoForRequest(request_);

  // Deleted in DownloadManager.
  DownloadCreateInfo* info = new DownloadCreateInfo(FilePath(), GURL(),
      base::Time::Now(), 0, content_length_, DownloadItem::IN_PROGRESS,
      download_id_, request_info->has_user_gesture(),
      request_info->transition_type());
  info->url_chain = request_->url_chain();
  info->referrer_url = GURL(request_->referrer());
  info->start_time = base::Time::Now();
  info->received_bytes = 0;
  info->total_bytes = content_length_;
  info->state = DownloadItem::IN_PROGRESS;
  info->download_id = download_id_;
  info->has_user_gesture = request_info->has_user_gesture();
  info->content_disposition = content_disposition_;
  info->mime_type = response->mime_type;
  download_stats::RecordDownloadMimeType(info->mime_type);

  DownloadRequestHandle request_handle(rdh_, global_id_.child_id,
                                       render_view_id_, global_id_.request_id);

  // TODO(ahendrickson) -- Get the last modified time and etag, so we can
  // resume downloading.

  CallStartedCB(net::OK);

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
      save_as_ && save_info_.file_path.empty();
  info->referrer_charset = request_->context()->referrer_charset();
  info->save_info = save_info_;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadFileManager::StartDownload,
                 download_file_manager_, info, request_handle));

  // We can't start saving the data before we create the file on disk.
  // The request will be un-paused in DownloadFileManager::CreateDownloadFile.
  rdh_->PauseRequest(global_id_.child_id, global_id_.request_id, true);

  return true;
}

void DownloadResourceHandler::CallStartedCB(net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (started_cb_.is_null())
    return;
  started_cb_.Run(download_id_, error);
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
  net::Error error_code = net::OK;
  if (status.status() == net::URLRequestStatus::FAILED)
    error_code = static_cast<net::Error>(status.error());  // Normal case.
  // ERR_CONTENT_LENGTH_MISMATCH is allowed since a number of servers in the
  // wild advertise a larger Content-Length than the amount of bytes in the
  // message body, and then close the connection. Other browsers - IE8,
  // Firefox 4.0.1, and Safari 5.0.4 - treat the download as complete in this
  // case, so we follow their lead.
  if (error_code == net::ERR_CONTENT_LENGTH_MISMATCH)
    error_code = net::OK;
  InterruptReason reason =
      ConvertNetErrorToInterruptReason(error_code,
                                       DOWNLOAD_INTERRUPT_FROM_NETWORK);
  if ((status.status() == net::URLRequestStatus::CANCELED) &&
      (status.error() == net::ERR_ABORTED)) {
    // TODO(ahendrickson) -- Find a better set of codes to use here, as
    // CANCELED/ERR_ABORTED can occur for reasons other than user cancel.
    reason = DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;  // User canceled.
  }

  download_stats::RecordAcceptsRanges(accept_ranges_, bytes_read_);

  if (!download_id_.IsValid())
    CallStartedCB(error_code);
  // We transfer ownership to |DownloadFileManager| to delete |buffer_|,
  // so that any functions queued up on the FILE thread are executed
  // before deletion.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFileManager::OnResponseCompleted,
                 download_file_manager_, download_id_, reason, security_info));
  buffer_ = NULL;  // The buffer is longer needed by |DownloadResourceHandler|.
  read_buffer_ = NULL;
  return true;
}

void DownloadResourceHandler::OnRequestClosed() {
  UMA_HISTOGRAM_TIMES("SB2.DownloadDuration",
                      base::TimeTicks::Now() - download_start_time_);
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
    rdh_->PauseRequest(global_id_.child_id, global_id_.request_id,
                       should_pause);
    is_paused_ = should_pause;
  }
}

DownloadResourceHandler::~DownloadResourceHandler() {
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
                            request_->url().spec().c_str(),
                            download_id_.local(),
                            global_id_.child_id,
                            global_id_.request_id,
                            render_view_id_,
                            save_info_.file_path.value().c_str());
}
