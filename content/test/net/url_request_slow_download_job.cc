// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/net/url_request_slow_download_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"

using content::BrowserThread;

const char URLRequestSlowDownloadJob::kUnknownSizeUrl[] =
  "http://url.handled.by.slow.download/download-unknown-size";
const char URLRequestSlowDownloadJob::kKnownSizeUrl[] =
  "http://url.handled.by.slow.download/download-known-size";
const char URLRequestSlowDownloadJob::kFinishDownloadUrl[] =
  "http://url.handled.by.slow.download/download-finish";

const int URLRequestSlowDownloadJob::kFirstDownloadSize = 1024 * 35;
const int URLRequestSlowDownloadJob::kSecondDownloadSize = 1024 * 10;

// static
base::LazyInstance<URLRequestSlowDownloadJob::SlowJobsSet>::Leaky
    URLRequestSlowDownloadJob::pending_requests_ = LAZY_INSTANCE_INITIALIZER;

void URLRequestSlowDownloadJob::Start() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestSlowDownloadJob::StartAsync,
                 weak_factory_.GetWeakPtr()));
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
}

// static
net::URLRequestJob* URLRequestSlowDownloadJob::Factory(
    net::URLRequest* request,
    const std::string& scheme) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  URLRequestSlowDownloadJob* job = new URLRequestSlowDownloadJob(request);
  if (request->url().spec() != kFinishDownloadUrl)
    pending_requests_.Get().insert(job);
  return job;
}

// static
size_t URLRequestSlowDownloadJob::NumberOutstandingRequests() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return pending_requests_.Get().size();
}

// static
void URLRequestSlowDownloadJob::FinishPendingRequests() {
  typedef std::set<URLRequestSlowDownloadJob*> JobList;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  for (JobList::iterator it = pending_requests_.Get().begin(); it !=
       pending_requests_.Get().end(); ++it) {
    (*it)->set_should_finish_download();
  }
}

URLRequestSlowDownloadJob::URLRequestSlowDownloadJob(net::URLRequest* request)
    : net::URLRequestJob(request),
      bytes_already_sent_(0),
      should_finish_download_(false),
      buffer_size_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

void URLRequestSlowDownloadJob::StartAsync() {
  if (LowerCaseEqualsASCII(kFinishDownloadUrl, request_->url().spec().c_str()))
    URLRequestSlowDownloadJob::FinishPendingRequests();

  NotifyHeadersComplete();
}

// ReadRawData and CheckDoneStatus together implement a state
// machine.  ReadRawData may be called arbitrarily by the network stack.
// It responds by:
//      * If there are bytes remaining in the first chunk, they are
//        returned.
//      [No bytes remaining in first chunk.   ]
//      * If should_finish_download_ is not set, it returns IO_PENDING,
//        and starts calling CheckDoneStatus on a regular timer.
//      [should_finish_download_ set.]
//      * If there are bytes remaining in the second chunk, they are filled.
//      * Otherwise, return *bytes_read = 0 to indicate end of request.
// CheckDoneStatus is called on a regular basis, in the specific
// case where we have transmitted all of the first chunk and none of the
// second.  If should_finish_download_ becomes set, it will "complete"
// the ReadRawData call that spawned off the CheckDoneStatus() repeated call.
//
// FillBufferHelper is a helper function that does the actual work of figuring
// out where in the state machine we are and how we should fill the buffer.
// It returns an enum indicating the state of the read.
URLRequestSlowDownloadJob::ReadStatus
URLRequestSlowDownloadJob::FillBufferHelper(
    net::IOBuffer* buf, int buf_size, int* bytes_written) {
  if (bytes_already_sent_ < kFirstDownloadSize) {
    int bytes_to_write = std::min(kFirstDownloadSize - bytes_already_sent_,
                                  buf_size);
    for (int i = 0; i < bytes_to_write; ++i) {
      buf->data()[i] = '*';
    }
    *bytes_written = bytes_to_write;
    bytes_already_sent_ += bytes_to_write;
    return BUFFER_FILLED;
  }

  if (!should_finish_download_)
    return REQUEST_BLOCKED;

  if (bytes_already_sent_ < kFirstDownloadSize + kSecondDownloadSize) {
    int bytes_to_write =
        std::min(kFirstDownloadSize + kSecondDownloadSize - bytes_already_sent_,
                 buf_size);
    for (int i = 0; i < bytes_to_write; ++i) {
      buf->data()[i] = '*';
    }
    *bytes_written = bytes_to_write;
    bytes_already_sent_ += bytes_to_write;
    return BUFFER_FILLED;
  }

  return REQUEST_COMPLETE;
}

bool URLRequestSlowDownloadJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                            int* bytes_read) {
  if (LowerCaseEqualsASCII(kFinishDownloadUrl,
                           request_->url().spec().c_str())) {
    VLOG(10) << __FUNCTION__ << " called w/ kFinishDownloadUrl.";
    *bytes_read = 0;
    return true;
  }

  VLOG(10) << __FUNCTION__ << " called at position "
           << bytes_already_sent_ << " in the stream.";
  ReadStatus status = FillBufferHelper(buf, buf_size, bytes_read);
  switch (status) {
    case BUFFER_FILLED:
      return true;
    case REQUEST_BLOCKED:
      buffer_ = buf;
      buffer_size_ = buf_size;
      SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&URLRequestSlowDownloadJob::CheckDoneStatus,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(100));
      return false;
    case REQUEST_COMPLETE:
      *bytes_read = 0;
      return true;
  }
  NOTREACHED();
  return true;
}

void URLRequestSlowDownloadJob::CheckDoneStatus() {
  if (should_finish_download_) {
    VLOG(10) << __FUNCTION__ << " called w/ should_finish_download_ set.";
    DCHECK(NULL != buffer_);
    int bytes_written = 0;
    ReadStatus status = FillBufferHelper(buffer_, buffer_size_, &bytes_written);
    DCHECK_EQ(BUFFER_FILLED, status);
    buffer_ = NULL;                     // Release the reference.
    SetStatus(net::URLRequestStatus());
    NotifyReadComplete(bytes_written);
  } else {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&URLRequestSlowDownloadJob::CheckDoneStatus,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(100));
  }
}

// Public virtual version.
void URLRequestSlowDownloadJob::GetResponseInfo(net::HttpResponseInfo* info) {
  // Forward to private const version.
  GetResponseInfoConst(info);
}

URLRequestSlowDownloadJob::~URLRequestSlowDownloadJob() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  pending_requests_.Get().erase(this);
}

// Private const version.
void URLRequestSlowDownloadJob::GetResponseInfoConst(
    net::HttpResponseInfo* info) const {
  // Send back mock headers.
  std::string raw_headers;
  if (LowerCaseEqualsASCII(kFinishDownloadUrl,
                           request_->url().spec().c_str())) {
    raw_headers.append(
      "HTTP/1.1 200 OK\n"
      "Content-type: text/plain\n");
  } else {
    raw_headers.append(
      "HTTP/1.1 200 OK\n"
      "Content-type: application/octet-stream\n"
      "Cache-Control: max-age=0\n");

    if (LowerCaseEqualsASCII(kKnownSizeUrl, request_->url().spec().c_str())) {
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
