// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/url_request_slow_download_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_status.h"

const int kFirstDownloadSize = 1024 * 35;
const int kSecondDownloadSize = 1024 * 10;

const char URLRequestSlowDownloadJob::kUnknownSizeUrl[] =
  "http://url.handled.by.slow.download/download-unknown-size";
const char URLRequestSlowDownloadJob::kKnownSizeUrl[] =
  "http://url.handled.by.slow.download/download-known-size";
const char URLRequestSlowDownloadJob::kFinishDownloadUrl[] =
  "http://url.handled.by.slow.download/download-finish";
const char URLRequestSlowDownloadJob::kErrorFinishDownloadUrl[] =
  "http://url.handled.by.slow.download/download-error";

std::vector<URLRequestSlowDownloadJob*>
    URLRequestSlowDownloadJob::pending_requests_;

// Return whether this is the finish or error URL.
static bool IsCompletionUrl(const GURL& url) {
  if (url.spec() == URLRequestSlowDownloadJob::kFinishDownloadUrl)
    return true;
  return (url.spec() == URLRequestSlowDownloadJob::kErrorFinishDownloadUrl);
}

void URLRequestSlowDownloadJob::Start() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestSlowDownloadJob::StartAsync));
}

// static
void URLRequestSlowDownloadJob::AddUrlHandler() {
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddUrlHandler(GURL(kUnknownSizeUrl),
                        &URLRequestSlowDownloadJob::Factory);
  filter->AddUrlHandler(GURL(kKnownSizeUrl),
                        &URLRequestSlowDownloadJob::Factory);
  filter->AddUrlHandler(GURL(kFinishDownloadUrl),
                        &URLRequestSlowDownloadJob::Factory);
  filter->AddUrlHandler(GURL(kErrorFinishDownloadUrl),
                        &URLRequestSlowDownloadJob::Factory);
}

/*static */
net::URLRequestJob* URLRequestSlowDownloadJob::Factory(
    net::URLRequest* request,
    const std::string& scheme) {
  URLRequestSlowDownloadJob* job = new URLRequestSlowDownloadJob(request);
  if (!IsCompletionUrl(request->url()))
    URLRequestSlowDownloadJob::pending_requests_.push_back(job);
  return job;
}

/* static */
void URLRequestSlowDownloadJob::FinishPendingRequests(bool error) {
  typedef std::vector<URLRequestSlowDownloadJob*> JobList;
  for (JobList::iterator it = pending_requests_.begin(); it !=
       pending_requests_.end(); ++it) {
    if (error) {
      (*it)->set_should_error_download();
    } else {
      (*it)->set_should_finish_download();
    }
  }
  pending_requests_.clear();
}

URLRequestSlowDownloadJob::URLRequestSlowDownloadJob(net::URLRequest* request)
    : net::URLRequestJob(request),
      first_download_size_remaining_(kFirstDownloadSize),
      should_finish_download_(false),
      should_send_second_chunk_(false),
      should_error_download_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}

void URLRequestSlowDownloadJob::StartAsync() {
  if (IsCompletionUrl(request_->url())) {
    URLRequestSlowDownloadJob::FinishPendingRequests(
        request_->url().spec() == kErrorFinishDownloadUrl);
  }

  NotifyHeadersComplete();
}

bool URLRequestSlowDownloadJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                            int *bytes_read) {
  if (IsCompletionUrl(request_->url())) {
    *bytes_read = 0;
    return true;
  }

  if (should_send_second_chunk_) {
    DCHECK(buf_size > kSecondDownloadSize);
    for (int i = 0; i < kSecondDownloadSize; ++i) {
      buf->data()[i] = '*';
    }
    *bytes_read = kSecondDownloadSize;
    should_send_second_chunk_ = false;
    return true;
  }

  if (first_download_size_remaining_ > 0) {
    int send_size = std::min(first_download_size_remaining_, buf_size);
    for (int i = 0; i < send_size; ++i) {
      buf->data()[i] = '*';
    }
    *bytes_read = send_size;
    first_download_size_remaining_ -= send_size;

    DCHECK(!is_done());
    return true;
  }

  if (should_finish_download_) {
    *bytes_read = 0;
    return true;
  }

  // If we make it here, the first chunk has been sent and we need to wait
  // until a request is made for kFinishDownloadUrl.
  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestSlowDownloadJob::CheckDoneStatus),
      100);

  // Return false to signal there is pending data.
  return false;
}

void URLRequestSlowDownloadJob::CheckDoneStatus() {
  if (should_finish_download_) {
    should_send_second_chunk_ = true;
    SetStatus(net::URLRequestStatus());
    NotifyReadComplete(kSecondDownloadSize);
  } else if (should_error_download_) {
    NotifyDone(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));
  } else {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
            &URLRequestSlowDownloadJob::CheckDoneStatus),
        100);
  }
}

// Public virtual version.
void URLRequestSlowDownloadJob::GetResponseInfo(net::HttpResponseInfo* info) {
  // Forward to private const version.
  GetResponseInfoConst(info);
}

URLRequestSlowDownloadJob::~URLRequestSlowDownloadJob() {}

// Private const version.
void URLRequestSlowDownloadJob::GetResponseInfoConst(
    net::HttpResponseInfo* info) const {
  // Send back mock headers.
  std::string raw_headers;
  if (IsCompletionUrl(request_->url())) {
    raw_headers.append(
      "HTTP/1.1 200 OK\n"
      "Content-type: text/plain\n");
  } else {
    raw_headers.append(
      "HTTP/1.1 200 OK\n"
      "Content-type: application/octet-stream\n"
      "Cache-Control: max-age=0\n");

    if (request_->url().spec() == kKnownSizeUrl) {
      raw_headers.append(base::StringPrintf(
          "Content-Length: %d\n",
          kFirstDownloadSize + kSecondDownloadSize));
    }
  }

  // ParseRawHeaders expects \0 to end each header line.
  ReplaceSubstringsAfterOffset(&raw_headers, 0, "\n", std::string("\0", 1));
  info->headers = new net::HttpResponseHeaders(raw_headers);
}

bool URLRequestSlowDownloadJob::GetMimeType(std::string* mime_type) const {
  net::HttpResponseInfo info;
  GetResponseInfoConst(&info);
  return info.headers && info.headers->GetMimeType(mime_type);
}
