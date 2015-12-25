// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_request_core.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/power_save_blocker.h"
#include "content/public/browser/web_contents.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context.h"

namespace content {

const int DownloadRequestCore::kDownloadByteStreamSize = 100 * 1024;

DownloadRequestCore::DownloadRequestCore(
    net::URLRequest* request,
    scoped_ptr<DownloadSaveInfo> save_info,
    const base::Closure& on_ready_to_read_callback)
    : on_ready_to_read_callback_(on_ready_to_read_callback),
      request_(request),
      save_info_(std::move(save_info)),
      last_buffer_size_(0),
      bytes_read_(0),
      pause_count_(0),
      was_deferred_(false) {
  DCHECK(request_);
  DCHECK(save_info_);
  DCHECK(!on_ready_to_read_callback_.is_null());
  RecordDownloadCount(UNTHROTTLED_COUNT);
  power_save_blocker_ = PowerSaveBlocker::Create(
      PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
      PowerSaveBlocker::kReasonOther, "Download in progress");
}

DownloadRequestCore::~DownloadRequestCore() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Remove output stream callback if a stream exists.
  if (stream_writer_)
    stream_writer_->RegisterCallback(base::Closure());

  UMA_HISTOGRAM_TIMES("SB2.DownloadDuration",
                      base::TimeTicks::Now() - download_start_time_);
}

// Send the download creation information to the download thread.
void DownloadRequestCore::OnResponseStarted(
    scoped_ptr<DownloadCreateInfo>* create_info,
    scoped_ptr<ByteStreamReader>* stream_reader) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(save_info_);
  DVLOG(20) << __FUNCTION__ << "()" << DebugString();
  download_start_time_ = base::TimeTicks::Now();

  // If it's a download, we don't want to poison the cache with it.
  request()->StopCaching();

  // Lower priority as well, so downloads don't contend for resources
  // with main frames.
  request()->SetPriority(net::IDLE);

  // If the content-length header is not present (or contains something other
  // than numbers), the incoming content_length is -1 (unknown size).
  // Set the content length to 0 to indicate unknown size to DownloadManager.
  int64_t content_length = request()->GetExpectedContentSize() > 0
                               ? request()->GetExpectedContentSize()
                               : 0;

  // Deleted in DownloadManager.
  scoped_ptr<DownloadCreateInfo> info(
      new DownloadCreateInfo(base::Time::Now(), content_length,
                             request()->net_log(), std::move(save_info_)));

  // Create the ByteStream for sending data to the download sink.
  CreateByteStream(
      base::ThreadTaskRunnerHandle::Get(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      kDownloadByteStreamSize, &stream_writer_, stream_reader);
  stream_writer_->RegisterCallback(
      base::Bind(&DownloadRequestCore::ResumeRequest, AsWeakPtr()));

  info->url_chain = request()->url_chain();
  info->referrer_url = GURL(request()->referrer());
  string mime_type;
  request()->GetMimeType(&mime_type);
  info->mime_type = mime_type;
  info->remote_address = request()->GetSocketAddress().host();
  if (request()->response_headers()) {
    // Grab the first content-disposition header.  There may be more than one,
    // though as of this writing, the network stack ensures if there are, they
    // are all duplicates.
    request()->response_headers()->EnumerateHeader(
        nullptr, "Content-Disposition", &info->content_disposition);
  }
  RecordDownloadMimeType(info->mime_type);
  RecordDownloadContentDisposition(info->content_disposition);

  // Get the last modified time and etag.
  const net::HttpResponseHeaders* headers = request()->response_headers();
  if (headers) {
    if (headers->HasStrongValidators()) {
      // If we don't have strong validators as per RFC 2616 section 13.3.3, then
      // we neither store nor use them for range requests.
      if (!headers->EnumerateHeader(nullptr, "Last-Modified",
                                    &info->last_modified))
        info->last_modified.clear();
      if (!headers->EnumerateHeader(nullptr, "ETag", &info->etag))
        info->etag.clear();
    }

    int status = headers->response_code();
    if (2 == status / 100 && status != net::HTTP_PARTIAL_CONTENT) {
      // Success & not range response; if we asked for a range, we didn't
      // get it--reset the file pointers to reflect that.
      info->save_info->offset = 0;
      info->save_info->hash_state = "";
    }

    if (!headers->GetMimeType(&info->original_mime_type))
      info->original_mime_type.clear();
  }

  // Blink verifies that the requester of this download is allowed to set a
  // suggested name for the security origin of the downlaod URL. However, this
  // assumption doesn't hold if there were cross origin redirects. Therefore,
  // clear the suggested_name for such requests.
  if (info->url_chain.size() > 1 &&
      info->url_chain.front().GetOrigin() != info->url_chain.back().GetOrigin())
    info->save_info->suggested_name.clear();

  info.swap(*create_info);
}

// Create a new buffer, which will be handed to the download thread for file
// writing and deletion.
bool DownloadRequestCore::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                     int* buf_size,
                                     int min_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(buf && buf_size);
  DCHECK(!read_buffer_.get());

  *buf_size = min_size < 0 ? kReadBufSize : min_size;
  last_buffer_size_ = *buf_size;
  read_buffer_ = new net::IOBuffer(*buf_size);
  *buf = read_buffer_.get();
  return true;
}

// Pass the buffer to the download file writer.
bool DownloadRequestCore::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(read_buffer_.get());

  base::TimeTicks now(base::TimeTicks::Now());
  if (!last_read_time_.is_null()) {
    double seconds_since_last_read = (now - last_read_time_).InSecondsF();
    if (now == last_read_time_)
      // Use 1/10 ms as a "very small number" so that we avoid
      // divide-by-zero error and still record a very high potential bandwidth.
      seconds_since_last_read = 0.00001;

    double actual_bandwidth = (bytes_read) / seconds_since_last_read;
    double potential_bandwidth = last_buffer_size_ / seconds_since_last_read;
    RecordBandwidth(actual_bandwidth, potential_bandwidth);
  }
  last_read_time_ = now;

  if (!bytes_read)
    return true;
  bytes_read_ += bytes_read;
  DCHECK(read_buffer_.get());

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

DownloadInterruptReason DownloadRequestCore::OnResponseCompleted(
    const net::URLRequestStatus& status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int response_code = status.is_success() ? request()->GetResponseCode() : 0;
  DVLOG(20) << __FUNCTION__ << "()" << DebugString()
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
  DownloadInterruptReason reason = ConvertNetErrorToInterruptReason(
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
    if (net::IsCertStatusError(request()->ssl_info().cert_status))
      reason = DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM;
    else
      reason = DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
  }

  if (status.is_success() && reason == DOWNLOAD_INTERRUPT_REASON_NONE &&
      request()->response_headers()) {
    // Handle server's response codes.
    switch (response_code) {
      case -1:  // Non-HTTP request.
      case net::HTTP_OK:
      case net::HTTP_CREATED:
      case net::HTTP_ACCEPTED:
      case net::HTTP_NON_AUTHORITATIVE_INFORMATION:
      case net::HTTP_RESET_CONTENT:
      case net::HTTP_PARTIAL_CONTENT:
        // Expected successful codes.
        break;
      case net::HTTP_NO_CONTENT:
      case net::HTTP_NOT_FOUND:
        reason = DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
        break;
      case net::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE:
        // Retry by downloading from the start automatically:
        // If we haven't received data when we get this error, we won't.
        reason = DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;
        break;
      case net::HTTP_UNAUTHORIZED:
        // Server didn't authorize this request.
        reason = DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED;
        break;
      case net::HTTP_FORBIDDEN:
        // Server forbids access to this resource.
        reason = DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN;
        break;
      default:  // All other errors.
        // Redirection and informational codes should have been handled earlier
        // in the stack.
        DCHECK_NE(3, response_code / 100);
        DCHECK_NE(1, response_code / 100);
        reason = DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
        break;
    }
  }

  std::string accept_ranges;
  bool has_strong_validators = false;
  if (request()->response_headers()) {
    request()->response_headers()->EnumerateHeader(nullptr, "Accept-Ranges",
                                                   &accept_ranges);
    has_strong_validators =
        request()->response_headers()->HasStrongValidators();
  }
  RecordAcceptsRanges(accept_ranges, bytes_read_, has_strong_validators);
  RecordNetworkBlockage(base::TimeTicks::Now() - download_start_time_,
                        total_pause_time_);

  // Send the info down the stream.  Conditional is in case we get
  // OnResponseCompleted without OnResponseStarted.
  if (stream_writer_)
    stream_writer_->Close(reason);

  // If the error mapped to something unknown, record it so that
  // we can drill down.
  if (reason == DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Download.MapErrorNetworkFailed",
                                std::abs(status.error()));
  }

  stream_writer_.reset();  // We no longer need the stream.
  read_buffer_ = nullptr;

  return reason;
}

void DownloadRequestCore::PauseRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ++pause_count_;
}

void DownloadRequestCore::ResumeRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

  on_ready_to_read_callback_.Run();
}

std::string DownloadRequestCore::DebugString() const {
  return base::StringPrintf(
      "{"
      " url_ = "
      "\"%s\""
      " }",
      request() ? request()->url().spec().c_str() : "<NULL request>");
}

}  // namespace content
