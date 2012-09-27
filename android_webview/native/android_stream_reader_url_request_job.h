// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_ANDROID_STREAM_READER_URL_REQUEST_JOB_H_
#define ANDROID_WEBVIEW_NATIVE_ANDROID_STREAM_READER_URL_REQUEST_JOB_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_job.h"

namespace net {
class URLRequest;
}

// A request job that reads data from a Java InputStream.
class AndroidStreamReaderURLRequestJob : public net::URLRequestJob {
 public:
  /*
   * We use a delegate so that we can share code for this job in slightly
   * different contexts.
   */
  class Delegate {
   public:
    virtual base::android::ScopedJavaLocalRef<jobject> OpenInputStream(
        JNIEnv* env,
        net::URLRequest* request) = 0;

    virtual bool GetMimeType(
        JNIEnv* env,
        net::URLRequest* request,
        jobject stream,
        std::string* mime_type) = 0;

    virtual bool GetCharset(
        JNIEnv* env,
        net::URLRequest* request,
        jobject stream,
        std::string* charset) = 0;

    virtual ~Delegate() {}
  };

  explicit AndroidStreamReaderURLRequestJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      scoped_ptr<Delegate> delegate);

  // URLRequestJob:
  virtual void Start() OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;
  virtual void SetExtraRequestHeaders(
      const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;

 protected:
  virtual ~AndroidStreamReaderURLRequestJob();

 private:
  // Verify the requested range against the stream size.
  bool VerifyRequestedRange(JNIEnv* env);

  // Skip to the first byte of the requested read range.
  bool SkipToRequestedRange(JNIEnv* env);

  net::HttpByteRange byte_range_;
  scoped_ptr<Delegate> delegate_;
  base::android::ScopedJavaGlobalRef<jobject> stream_;
  base::android::ScopedJavaGlobalRef<jbyteArray> buffer_;

  DISALLOW_COPY_AND_ASSIGN(AndroidStreamReaderURLRequestJob);
};

bool RegisterAndroidStreamReaderUrlRequestJob(JNIEnv* env);

#endif  // ANDROID_WEBVIEW_NATIVE_ANDROID_STREAM_READER_URL_REQUEST_JOB_H_
