// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/intercepted_request_data.h"

#include "android_webview/browser/input_stream.h"
#include "android_webview/browser/net/android_stream_reader_url_request_job.h"

namespace android_webview {

namespace {

class StreamReaderJobDelegateImpl
    : public AndroidStreamReaderURLRequestJob::Delegate {
 public:
  StreamReaderJobDelegateImpl(
      scoped_ptr<InterceptedRequestData> intercepted_request_data)
      : intercepted_request_data_(intercepted_request_data.Pass()) {
    DCHECK(intercepted_request_data_);
  }

  virtual scoped_ptr<InputStream> OpenInputStream(JNIEnv* env,
                                                  const GURL& url) OVERRIDE {
    return intercepted_request_data_->GetInputStream(env).Pass();
  }

  virtual void OnInputStreamOpenFailed(net::URLRequest* request,
                                       bool* restart) OVERRIDE {
    *restart = false;
  }

  virtual bool GetMimeType(JNIEnv* env,
                           net::URLRequest* request,
                           android_webview::InputStream* stream,
                           std::string* mime_type) OVERRIDE {
    return intercepted_request_data_->GetMimeType(env, mime_type);
  }

  virtual bool GetCharset(JNIEnv* env,
                          net::URLRequest* request,
                          android_webview::InputStream* stream,
                          std::string* charset) OVERRIDE {
    return intercepted_request_data_->GetCharset(env, charset);
  }

 private:
  scoped_ptr<InterceptedRequestData> intercepted_request_data_;
};

}  // namespace

// static
net::URLRequestJob* InterceptedRequestData::CreateJobFor(
    scoped_ptr<InterceptedRequestData> intercepted_request_data,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  DCHECK(intercepted_request_data);
  DCHECK(request);
  DCHECK(network_delegate);

  return new AndroidStreamReaderURLRequestJob(
      request,
      network_delegate,
      make_scoped_ptr(
          new StreamReaderJobDelegateImpl(intercepted_request_data.Pass()))
          .PassAs<AndroidStreamReaderURLRequestJob::Delegate>());
}

}  // namespace android_webview
