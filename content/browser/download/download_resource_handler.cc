// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_resource_handler.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/stringprintf.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/byte_stream.h"
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

static const int kDownloadByteStreamSize = 100 * 1024;

void CallStartedCBOnUIThread(
    const DownloadResourceHandler::OnStartedCallback& started_cb,
    DownloadId id,
    net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (started_cb.is_null())
    return;
  started_cb.Run(id, error);
}

// Static function in order to prevent any accidental accesses to
// DownloadResourceHandler members from the UI thread.
static void StartOnUIThread(
    scoped_ptr<DownloadCreateInfo> info,
    scoped_ptr<content::ByteStreamReader> stream,
    const DownloadResourceHandler::OnStartedCallback& started_cb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadManager* download_manager = info->request_handle.GetDownloadManager();
  if (!download_manager) {
    // NULL in unittests or if the page closed right after starting the
    // download.
    if (!started_cb.is_null())
      started_cb.Run(DownloadId(), net::ERR_ACCESS_DENIED);
    return;
  }

  DownloadId download_id =
      download_manager->StartDownload(info.Pass(), stream.Pass());

  if (!started_cb.is_null())
    started_cb.Run(download_id, net::OK);
}

}  // namespace

DownloadResourceHandler::DownloadResourceHandler(
    net::URLRequest* request,
    const DownloadResourceHandler::OnStartedCallback& started_cb,
    const content::DownloadSaveInfo& save_info)
    : render_view_id_(0),               // Actually initialized below.
      content_length_(0),
      request_(request),
      started_cb_(started_cb),
      save_info_(save_info),
      last_buffer_size_(0),
      bytes_read_(0),
      pause_count_(0),
      was_deferred_(false),
      on_response_started_called_(false) {
  ResourceRequestInfoImpl* info(ResourceRequestInfoImpl::ForRequest(request));
  global_id_ = info->GetGlobalRequestID();
  render_view_id_ = info->GetRouteID();

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
    content::ResourceResponse* response,
    bool* defer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // There can be only one (call)
  DCHECK(!on_response_started_called_);
  on_response_started_called_ = true;

  VLOG(20) << __FUNCTION__ << "()" << DebugString()
           << " request_id = " << request_id;
  download_start_time_ = base::TimeTicks::Now();

  // If it's a download, we don't want to poison the cache with it.
  request_->StopCaching();

  std::string content_disposition;
  request_->GetResponseHeaderByName("content-disposition",
                                    &content_disposition);
  SetContentDisposition(content_disposition);
  SetContentLength(response->head.content_length);

  const ResourceRequestInfoImpl* request_info =
      ResourceRequestInfoImpl::ForRequest(request_);

  // Deleted in DownloadManager.
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo(
      base::Time::Now(), 0, content_length_, DownloadItem::IN_PROGRESS,
      request_->net_log(), request_info->HasUserGesture(),
      request_info->transition_type()));

  // Create the ByteStream for sending data to the download sink.
  scoped_ptr<content::ByteStreamReader> stream_reader;
  CreateByteStream(
      base::MessageLoopProxy::current(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      kDownloadByteStreamSize, &stream_writer_, &stream_reader);
  stream_writer_->RegisterCallback(
      base::Bind(&DownloadResourceHandler::ResumeRequest, AsWeakPtr()));

  info->url_chain = request_->url_chain();
  info->referrer_url = GURL(request_->referrer());
  info->start_time = base::Time::Now();
  info->received_bytes = save_info_.offset;
  info->total_bytes = content_length_;
  info->state = DownloadItem::IN_PROGRESS;
  info->has_user_gesture = request_info->HasUserGesture();
  info->content_disposition = content_disposition_;
  info->mime_type = response->head.mime_type;
  info->remote_address = request_->GetSocketAddress().host();
  download_stats::RecordDownloadMimeType(info->mime_type);

  info->request_handle =
      DownloadRequestHandle(AsWeakPtr(), global_id_.child_id,
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
  if (!response->head.headers ||
      !response->head.headers->GetMimeType(&content_type_header))
    content_type_header = "";
  info->original_mime_type = content_type_header;

  if (!response->head.headers ||
      !response->head.headers->EnumerateHeader(
          NULL, "Accept-Ranges", &accept_ranges_)) {
    accept_ranges_ = "";
  }

  info->prompt_user_for_save_location =
      save_info_.prompt_for_save_location && save_info_.file_path.empty();
  info->referrer_charset = request_->context()->referrer_charset();
  info->save_info = save_info_;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&StartOnUIThread,
                 base::Passed(info.Pass()),
                 base::Passed(stream_reader.Pass()),
                 // Pass to StartOnUIThread so that variable
                 // access is always on IO thread but function
                 // is called on UI thread.
                 started_cb_));
  // Guaranteed to be called in StartOnUIThread
  started_cb_.Reset();

  return true;
}

void DownloadResourceHandler::CallStartedCB(DownloadId id, net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
bool DownloadResourceHandler::OnReadCompleted(int request_id, int bytes_read,
                                              bool* defer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(read_buffer_);

  if (pause_count_ > 0) {
    *defer = was_deferred_ = true;
    return true;
  }

  base::TimeTicks now(base::TimeTicks::Now());
  if (!last_read_time_.is_null()) {
    double seconds_since_last_read = (now - last_read_time_).InSecondsF();
    if (now == last_read_time_)
      // Use 1/10 ms as a "very small number" so that we avoid
      // divide-by-zero error and still record a very high potential bandwidth.
      seconds_since_last_read = 0.00001;

    double actual_bandwidth = (bytes_read)/seconds_since_last_read;
    double potential_bandwidth = last_buffer_size_/seconds_since_last_read;
    download_stats::RecordBandwidth(actual_bandwidth, potential_bandwidth);
  }
  last_read_time_ = now;

  if (!bytes_read)
    return true;
  bytes_read_ += bytes_read;
  DCHECK(read_buffer_);

  // Take the data ship it down the stream.  If the stream is full, pause the
  // request; the stream callback will resume it.
  if (!stream_writer_->Write(read_buffer_, bytes_read)) {
    PauseRequest();
    *defer = was_deferred_ = true;
    last_stream_pause_time_ = now;
  }

  read_buffer_ = NULL;  // Drop our reference.

  return true;
}

bool DownloadResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  VLOG(20) << __FUNCTION__ << "()" << DebugString()
           << " request_id = " << request_id
           << " status.status() = " << status.status()
           << " status.error() = " << status.error();
  int response_code = status.is_success() ? request_->GetResponseCode() : 0;

  net::Error error_code = net::OK;
  if (status.status() == net::URLRequestStatus::FAILED)
    error_code = static_cast<net::Error>(status.error());  // Normal case.
  // ERR_CONTENT_LENGTH_MISMATCH and ERR_INCOMPLETE_CHUNKED_ENCODING are
  // allowed since a number of servers in the wild close the connection too
  // early by mistake. Other browsers - IE9, Firefox 11.0, and Safari 5.1.4 -
  // treat downloads as complete in both cases, so we follow their lead.
  if (error_code == net::ERR_CONTENT_LENGTH_MISMATCH ||
      error_code == net::ERR_INCOMPLETE_CHUNKED_ENCODING) {
    error_code = net::OK;
  }
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

  CallStartedCB(DownloadId(), error_code);

  // Send the info down the stream.  Conditional is in case we get
  // OnResponseCompleted without OnResponseStarted.
  if (stream_writer_.get())
    stream_writer_->Close(reason);

  stream_writer_.reset();  // We no longer need the stream.
  read_buffer_ = NULL;

  // Stats
  download_stats::RecordNetworkBandwidth(
      bytes_read_, base::TimeTicks::Now() - download_start_time_,
      total_pause_time_);

  return true;
}

// If the content-length header is not present (or contains something other
// than numbers), the incoming content_length is -1 (unknown size).
// Set the content length to 0 to indicate unknown size to DownloadManager.
void DownloadResourceHandler::SetContentLength(const int64& content_length) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  content_length_ = 0;
  if (content_length > 0)
    content_length_ = content_length;
}

void DownloadResourceHandler::SetContentDisposition(
    const std::string& content_disposition) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  content_disposition_ = content_disposition;
}

void DownloadResourceHandler::PauseRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ++pause_count_;
}

void DownloadResourceHandler::ResumeRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_LT(0, pause_count_);

  --pause_count_;

  if (!was_deferred_)
    return;
  if (pause_count_ > 0)
    return;

  was_deferred_ = false;
  if (!last_stream_pause_time_.is_null()) {
    total_pause_time_ += (base::TimeTicks::Now() - last_stream_pause_time_);
    last_stream_pause_time_ = base::TimeTicks();
  }

  controller()->Resume();
}

void DownloadResourceHandler::CancelRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ResourceDispatcherHostImpl::Get()->CancelRequest(
      global_id_.child_id,
      global_id_.request_id,
      false);
}

std::string DownloadResourceHandler::DebugString() const {
  return base::StringPrintf("{"
                            " url_ = " "\"%s\""
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
                            global_id_.child_id,
                            global_id_.request_id,
                            render_view_id_,
                            save_info_.file_path.value().c_str());
}

DownloadResourceHandler::~DownloadResourceHandler() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // This won't do anything if the callback was called before.
  // If it goes through, it will likely be because OnWillStart() returned
  // false somewhere in the chain of resource handlers.
  CallStartedCB(DownloadId(), net::ERR_ACCESS_DENIED);

  // Remove output stream callback if a stream exists.
  if (stream_writer_.get())
    stream_writer_->RegisterCallback(base::Closure());

  UMA_HISTOGRAM_TIMES("SB2.DownloadDuration",
                      base::TimeTicks::Now() - download_start_time_);
}
