// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_ANDROID_STREAM_READER_URL_REQUEST_JOB_H_
#define ANDROID_WEBVIEW_NATIVE_ANDROID_STREAM_READER_URL_REQUEST_JOB_H_

#include "base/android/scoped_java_ref.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"

namespace android_webview {
class InputStream;
class InputStreamReader;
}

namespace base {
class TaskRunner;
}

namespace net {
class URLRequest;
}

class InputStreamReaderWrapper;

// A request job that reads data from a Java InputStream.
class AndroidStreamReaderURLRequestJob : public net::URLRequestJob {
 public:
  /*
   * We use a delegate so that we can share code for this job in slightly
   * different contexts.
   */
  class Delegate {
   public:
    virtual scoped_ptr<android_webview::InputStream> OpenInputStream(
        JNIEnv* env,
        net::URLRequest* request) = 0;

    virtual bool GetMimeType(
        JNIEnv* env,
        net::URLRequest* request,
        android_webview::InputStream* stream,
        std::string* mime_type) = 0;

    virtual bool GetCharset(
        JNIEnv* env,
        net::URLRequest* request,
        android_webview::InputStream* stream,
        std::string* charset) = 0;

    virtual ~Delegate() {}
  };

  AndroidStreamReaderURLRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      scoped_ptr<Delegate> delegate);

  // URLRequestJob:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;

 protected:
  virtual ~AndroidStreamReaderURLRequestJob();

  // Gets the TaskRunner for the worker thread.
  // Overridden in unittests.
  virtual base::TaskRunner* GetWorkerThreadRunner();

  // Creates an InputStreamReader instance.
  // Overridden in unittests to return a mock.
  virtual scoped_ptr<android_webview::InputStreamReader>
      CreateStreamReader(android_webview::InputStream* stream);

 private:
  void StartAsync();

  void OnReaderSeekCompleted(int content_size);
  void OnReaderReadCompleted(int bytes_read);

  net::HttpByteRange byte_range_;
  scoped_ptr<Delegate> delegate_;
  scoped_refptr<InputStreamReaderWrapper> input_stream_reader_wrapper_;
  base::WeakPtrFactory<AndroidStreamReaderURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidStreamReaderURLRequestJob);
};

#endif  // ANDROID_WEBVIEW_NATIVE_ANDROID_STREAM_READER_URL_REQUEST_JOB_H_
