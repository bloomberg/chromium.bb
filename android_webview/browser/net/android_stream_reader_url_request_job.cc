// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/android_stream_reader_url_request_job.h"

#include "android_webview/browser/input_stream.h"
#include "android_webview/browser/net/input_stream_reader.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_manager.h"

using android_webview::InputStream;
using android_webview::InputStreamReader;
using base::android::AttachCurrentThread;
using base::PostTaskAndReplyWithResult;
using content::BrowserThread;

AndroidStreamReaderURLRequestJob::AndroidStreamReaderURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    scoped_ptr<Delegate> delegate)
    : URLRequestJob(request, network_delegate),
      delegate_(delegate.Pass()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(delegate_.get());
}

AndroidStreamReaderURLRequestJob::~AndroidStreamReaderURLRequestJob() {
}

void AndroidStreamReaderURLRequestJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &AndroidStreamReaderURLRequestJob::StartAsync,
          weak_factory_.GetWeakPtr()));
}

void AndroidStreamReaderURLRequestJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  URLRequestJob::Kill();
}

scoped_refptr<InputStreamReader>
AndroidStreamReaderURLRequestJob::CreateStreamReader(InputStream* stream) {
  return make_scoped_refptr(new InputStreamReader(stream));
}

void AndroidStreamReaderURLRequestJob::StartAsync() {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // This could be done in the InputStreamReader but would force more
  // complex synchronization in the delegate.
  stream_ = delegate_->OpenInputStream(env, request());
  if (!stream_) {
    NotifyDone(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }

  DCHECK(!input_stream_reader_);
  input_stream_reader_ = CreateStreamReader(stream_.get());
  CHECK(input_stream_reader_);

  PostTaskAndReplyWithResult(
      GetWorkerThreadRunner(),
      FROM_HERE,
      base::Bind(&InputStreamReader::Seek, input_stream_reader_, byte_range_),
      base::Bind(&AndroidStreamReaderURLRequestJob::OnReaderSeekCompleted,
                 weak_factory_.GetWeakPtr()));
}

void AndroidStreamReaderURLRequestJob::OnReaderSeekCompleted(
    int result) {
  // Clear the IO_PENDING status set in Start().
  SetStatus(net::URLRequestStatus());
  if (result >= 0) {
    set_expected_content_size(result);
    NotifyHeadersComplete();
  } else {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED, result));
  }
}

void AndroidStreamReaderURLRequestJob::OnReaderReadCompleted(
    int result) {
  // The URLRequest API contract requires that:
  // * NotifyDone be called once, to set the status code, indicate the job is
  //   finished (there will be no further IO),
  // * NotifyReadComplete be called if false is returned from ReadRawData to
  //   indicate that the IOBuffer will not be used by the job anymore.
  // There might be multiple calls to ReadRawData (and thus multiple calls to
  // NotifyReadComplete), which is why NotifyDone is called only on errors
  // (result < 0) and end of data (result == 0).
  if (result == 0) {
    NotifyDone(net::URLRequestStatus());
  } else if (result < 0) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED, result));
  } else {
    // Clear the IO_PENDING status.
    SetStatus(net::URLRequestStatus());
  }
  NotifyReadComplete(result);
}

base::TaskRunner* AndroidStreamReaderURLRequestJob::GetWorkerThreadRunner() {
  return static_cast<base::TaskRunner*>(BrowserThread::GetBlockingPool());
}

bool AndroidStreamReaderURLRequestJob::ReadRawData(net::IOBuffer* dest,
                                                   int dest_size,
                                                   int* bytes_read) {
  DCHECK(input_stream_reader_);

  PostTaskAndReplyWithResult(
      GetWorkerThreadRunner(),
      FROM_HERE,
      base::Bind(&InputStreamReader::ReadRawData,
                 input_stream_reader_,
                 base::Unretained(dest),
                 dest_size),
      base::Bind(&AndroidStreamReaderURLRequestJob::OnReaderReadCompleted,
                 weak_factory_.GetWeakPtr()));

  SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  return false;
}

bool AndroidStreamReaderURLRequestJob::GetMimeType(
    std::string* mime_type) const {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  if (!stream_)
    return false;

  return delegate_->GetMimeType(env, request(), *stream_, mime_type);
}

bool AndroidStreamReaderURLRequestJob::GetCharset(
    std::string* charset) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  if (!stream_)
    return false;

  return delegate_->GetCharset(env, request(), *stream_, charset);
}

void AndroidStreamReaderURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // We only extract the "Range" header so that we know how many bytes in the
    // stream to skip and how many to read after that.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        byte_range_ = ranges[0];
      } else {
        // We don't support multiple range requests in one single URL request,
        // because we need to do multipart encoding here.
        NotifyDone(net::URLRequestStatus(
            net::URLRequestStatus::FAILED,
            net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
      }
    }
  }
}
