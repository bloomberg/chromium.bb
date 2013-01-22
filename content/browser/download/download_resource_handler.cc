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
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/byte_stream.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context.h"

namespace content {
namespace {

void CallStartedCBOnUIThread(
    const DownloadResourceHandler::OnStartedCallback& started_cb,
    DownloadItem* item,
    net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (started_cb.is_null())
    return;
  started_cb.Run(item, error);
}

// Static function in order to prevent any accidental accesses to
// DownloadResourceHandler members from the UI thread.
static void StartOnUIThread(
    scoped_ptr<DownloadCreateInfo> info,
    scoped_ptr<ByteStreamReader> stream,
    const DownloadResourceHandler::OnStartedCallback& started_cb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadManager* download_manager = info->request_handle.GetDownloadManager();
  if (!download_manager) {
    // NULL in unittests or if the page closed right after starting the
    // download.
    if (!started_cb.is_null())
      started_cb.Run(NULL, net::ERR_ACCESS_DENIED);
    return;
  }

  DownloadItem* item = download_manager->StartDownload(
      info.Pass(), stream.Pass());

  if (!started_cb.is_null())
    started_cb.Run(item, net::OK);
}

}  // namespace

const int DownloadResourceHandler::kDownloadByteStreamSize = 100 * 1024;

DownloadResourceHandler::DownloadResourceHandler(
    DownloadId id,
    net::URLRequest* request,
    const DownloadResourceHandler::OnStartedCallback& started_cb,
    scoped_ptr<DownloadSaveInfo> save_info)
    : download_id_(id),
      render_view_id_(0),               // Actually initialized below.
      content_length_(0),
      request_(request),
      started_cb_(started_cb),
      save_info_(save_info.Pass()),
      last_buffer_size_(0),
      bytes_read_(0),
      pause_count_(0),
      was_deferred_(false),
      on_response_started_called_(false) {
  ResourceRequestInfoImpl* info(ResourceRequestInfoImpl::ForRequest(request));
  global_id_ = info->GetGlobalRequestID();
  render_view_id_ = info->GetRouteID();

  RecordDownloadCount(UNTHROTTLED_COUNT);
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
    ResourceResponse* response,
    bool* defer) {
  return true;
}

// Send the download creation information to the download thread.
bool DownloadResourceHandler::OnResponseStarted(
    int request_id,
    ResourceResponse* response,
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
      base::Time::Now(), content_length_,
      request_->net_log(), request_info->HasUserGesture(),
      request_info->GetPageTransition()));

  // Create the ByteStream for sending data to the download sink.
  scoped_ptr<ByteStreamReader> stream_reader;
  CreateByteStream(
      base::MessageLoopProxy::current(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      kDownloadByteStreamSize, &stream_writer_, &stream_reader);
  stream_writer_->RegisterCallback(
      base::Bind(&DownloadResourceHandler::ResumeRequest, AsWeakPtr()));

  info->download_id = download_id_;
  info->url_chain = request_->url_chain();
  info->referrer_url = GURL(request_->referrer());
  info->start_time = base::Time::Now();
  info->total_bytes = content_length_;
  info->has_user_gesture = request_info->HasUserGesture();
  info->content_disposition = content_disposition_;
  info->mime_type = response->head.mime_type;
  info->remote_address = request_->GetSocketAddress().host();
  RecordDownloadMimeType(info->mime_type);
  RecordDownloadContentDisposition(info->content_disposition);

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

    int status = headers->response_code();
    if (2 == status / 100  && status != net::HTTP_PARTIAL_CONTENT) {
      // Success & not range response; if we asked for a range, we didn't
      // get it--reset the file pointers to reflect that.
      save_info_->offset = 0;
      save_info_->hash_state = "";
    }
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

  info->save_info = save_info_.Pass();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&StartOnUIThread,
                 base::Passed(&info),
                 base::Passed(&stream_reader),
                 // Pass to StartOnUIThread so that variable
                 // access is always on IO thread but function
                 // is called on UI thread.
                 started_cb_));
  // Guaranteed to be called in StartOnUIThread
  started_cb_.Reset();

  return true;
}

void DownloadResourceHandler::CallStartedCB(
    DownloadItem* item, net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (started_cb_.is_null())
    return;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CallStartedCBOnUIThread, started_cb_, item, error));
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
  DCHECK(!read_buffer_);

  *buf_size = min_size < 0 ? kReadBufSize : min_size;
  last_buffer_size_ = *buf_size;
  read_buffer_ = new net::IOBuffer(*buf_size);
  *buf = read_buffer_.get();
  return true;
}

// Pass the buffer to the download file writer.
bool DownloadResourceHandler::OnReadCompleted(int request_id, int bytes_read,
                                              bool* defer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(read_buffer_);

  base::TimeTicks now(base::TimeTicks::Now());
  if (!last_read_time_.is_null()) {
    double seconds_since_last_read = (now - last_read_time_).InSecondsF();
    if (now == last_read_time_)
      // Use 1/10 ms as a "very small number" so that we avoid
      // divide-by-zero error and still record a very high potential bandwidth.
      seconds_since_last_read = 0.00001;

    double actual_bandwidth = (bytes_read)/seconds_since_last_read;
    double potential_bandwidth = last_buffer_size_/seconds_since_last_read;
    RecordBandwidth(actual_bandwidth, potential_bandwidth);
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

  if (pause_count_ > 0)
    *defer = was_deferred_ = true;

  return true;
}

bool DownloadResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& status,
    const std::string& security_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int response_code = status.is_success() ? request_->GetResponseCode() : 0;
  VLOG(20) << __FUNCTION__ << "()" << DebugString()
           << " request_id = " << request_id
           << " status.status() = " << status.status()
           << " status.error() = " << status.error()
           << " response_code = " << response_code;

  net::Error error_code = net::OK;
  if (status.status() == net::URLRequestStatus::FAILED ||
      // Note cancels as failures too.
      status.status() == net::URLRequestStatus::CANCELED) {
    error_code = static_cast<net::Error>(status.error());  // Normal case.
    // Make sure that at least the fact of failure comes through.
    if (error_code == net::OK)
      error_code = net::ERR_FAILED;
  }

  // ERR_CONTENT_LENGTH_MISMATCH and ERR_INCOMPLETE_CHUNKED_ENCODING are
  // allowed since a number of servers in the wild close the connection too
  // early by mistake. Other browsers - IE9, Firefox 11.0, and Safari 5.1.4 -
  // treat downloads as complete in both cases, so we follow their lead.
  if (error_code == net::ERR_CONTENT_LENGTH_MISMATCH ||
      error_code == net::ERR_INCOMPLETE_CHUNKED_ENCODING) {
    error_code = net::OK;
  }
  DownloadInterruptReason reason =
      ConvertNetErrorToInterruptReason(
        error_code, DOWNLOAD_INTERRUPT_FROM_NETWORK);

  if (status.status() == net::URLRequestStatus::CANCELED &&
      status.error() == net::ERR_ABORTED) {
    // CANCELED + ERR_ABORTED == something outside of the network
    // stack cancelled the request.  There aren't that many things that
    // could do this to a download request (whose lifetime is separated from
    // the tab from which it came).  We map this to USER_CANCELLED as the
    // case we know about (system suspend because of laptop close) corresponds
    // to a user action.
    // TODO(ahendrickson) -- Find a better set of codes to use here, as
    // CANCELED/ERR_ABORTED can occur for reasons other than user cancel.
    reason = DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
  }

  if (status.is_success() &&
      reason == DOWNLOAD_INTERRUPT_REASON_NONE &&
      request_->response_headers()) {
    // Handle server's response codes.
    switch(response_code) {
      case -1:                          // Non-HTTP request.
      case net::HTTP_OK:
      case net::HTTP_PARTIAL_CONTENT:
        // Expected successful codes.
        break;
      case net::HTTP_NO_CONTENT:
      case net::HTTP_NOT_FOUND:
        reason = DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
        break;
      case net::HTTP_PRECONDITION_FAILED:
        // Failed our 'If-Unmodified-Since' or 'If-Match'; see
        // download_manager_impl.cc BeginDownload()
        reason = DOWNLOAD_INTERRUPT_REASON_SERVER_PRECONDITION;
        break;
      case net::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE:
        // Retry by downloading from the start automatically:
        // If we haven't received data when we get this error, we won't.
        reason = DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;
        break;
      default:    // All other errors.
        // Redirection should have been handled earlier in the stack.
        DCHECK(3 != response_code / 100);

        // Informational codes should have been handled earlier in the
        // stack.
        DCHECK(1 != response_code / 100);
        reason = DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
        break;
    }
  }

  RecordAcceptsRanges(accept_ranges_, bytes_read_);
  RecordNetworkBlockage(
      base::TimeTicks::Now() - download_start_time_, total_pause_time_);

  CallStartedCB(NULL, error_code);

  // Send the info down the stream.  Conditional is in case we get
  // OnResponseCompleted without OnResponseStarted.
  if (stream_writer_.get())
    stream_writer_->Close(reason);

  // If the error mapped to something unknown, record it so that
  // we can drill down.
  if (reason == DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED) {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION("Download.MapErrorNetworkFailed",
                                     std::abs(status.error()),
                                     net::GetAllErrorCodesForUma());
  }

  stream_writer_.reset();  // We no longer need the stream.
  read_buffer_ = NULL;

  return true;
}

void DownloadResourceHandler::OnDataDownloaded(
    int request_id,
    int bytes_downloaded) {
  NOTREACHED();
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
                            " }",
                            request_ ?
                                request_->url().spec().c_str() :
                                "<NULL request>",
                            global_id_.child_id,
                            global_id_.request_id,
                            render_view_id_);
}

DownloadResourceHandler::~DownloadResourceHandler() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // This won't do anything if the callback was called before.
  // If it goes through, it will likely be because OnWillStart() returned
  // false somewhere in the chain of resource handlers.
  CallStartedCB(NULL, net::ERR_ACCESS_DENIED);

  // Remove output stream callback if a stream exists.
  if (stream_writer_.get())
    stream_writer_->RegisterCallback(base::Closure());

  UMA_HISTOGRAM_TIMES("SB2.DownloadDuration",
                      base::TimeTicks::Now() - download_start_time_);
}

}  // namespace content
