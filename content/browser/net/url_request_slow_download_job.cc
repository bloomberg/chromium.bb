// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/url_request_slow_download_job.h"

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
base::LazyInstance<
    URLRequestSlowDownloadJob::SlowJobsSet,
    base::LeakyLazyInstanceTraits<URLRequestSlowDownloadJob::SlowJobsSet> >
        URLRequestSlowDownloadJob::pending_requests_ =
            LAZY_INSTANCE_INITIALIZER;

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
      first_download_size_remaining_(kFirstDownloadSize),
      should_finish_download_(false),
      buffer_size_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

void URLRequestSlowDownloadJob::StartAsync() {
  if (LowerCaseEqualsASCII(kFinishDownloadUrl, request_->url().spec().c_str()))
    URLRequestSlowDownloadJob::FinishPendingRequests();

  NotifyHeadersComplete();
}

bool URLRequestSlowDownloadJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                            int *bytes_read) {
  if (LowerCaseEqualsASCII(kFinishDownloadUrl,
                           request_->url().spec().c_str())) {
    *bytes_read = 0;
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
  buffer_ = buf;
  buffer_size_ = buf_size;
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
    DCHECK(NULL != buffer_);
    // Send the rest of the data.
    DCHECK_GT(buffer_size_, kSecondDownloadSize);
    for (int i = 0; i < kSecondDownloadSize; ++i) {
      buffer_->data()[i] = '*';
    }
    SetStatus(net::URLRequestStatus());
    NotifyReadComplete(kSecondDownloadSize);  // Deletes this.
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
