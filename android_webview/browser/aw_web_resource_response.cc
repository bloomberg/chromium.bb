// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_web_resource_response.h"

#include "android_webview/browser/input_stream.h"
#include "android_webview/browser/net/android_stream_reader_url_request_job.h"
#include "base/strings/string_number_conversions.h"
#include "net/http/http_response_headers.h"

namespace android_webview {

namespace {

class StreamReaderJobDelegateImpl
    : public AndroidStreamReaderURLRequestJob::Delegate {
 public:
  StreamReaderJobDelegateImpl(
      scoped_ptr<AwWebResourceResponse> aw_web_resource_response)
      : aw_web_resource_response_(aw_web_resource_response.Pass()) {
    DCHECK(aw_web_resource_response_);
  }

  scoped_ptr<InputStream> OpenInputStream(JNIEnv* env,
                                          const GURL& url) override {
    return aw_web_resource_response_->GetInputStream(env).Pass();
  }

  void OnInputStreamOpenFailed(net::URLRequest* request,
                               bool* restart) override {
    *restart = false;
  }

  bool GetMimeType(JNIEnv* env,
                   net::URLRequest* request,
                   android_webview::InputStream* stream,
                   std::string* mime_type) override {
    return aw_web_resource_response_->GetMimeType(env, mime_type);
  }

  bool GetCharset(JNIEnv* env,
                  net::URLRequest* request,
                  android_webview::InputStream* stream,
                  std::string* charset) override {
    return aw_web_resource_response_->GetCharset(env, charset);
  }

  void AppendResponseHeaders(JNIEnv* env,
                             net::HttpResponseHeaders* headers) override {
    int status_code;
    std::string reason_phrase;
    if (aw_web_resource_response_->GetStatusInfo(
            env, &status_code, &reason_phrase)) {
      std::string status_line("HTTP/1.1 ");
      status_line.append(base::IntToString(status_code));
      status_line.append(" ");
      status_line.append(reason_phrase);
      headers->ReplaceStatusLine(status_line);
    }
    aw_web_resource_response_->GetResponseHeaders(env, headers);
  }

 private:
  scoped_ptr<AwWebResourceResponse> aw_web_resource_response_;
};

}  // namespace

// static
net::URLRequestJob* AwWebResourceResponse::CreateJobFor(
    scoped_ptr<AwWebResourceResponse> aw_web_resource_response,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  DCHECK(aw_web_resource_response);
  DCHECK(request);
  DCHECK(network_delegate);

  return new AndroidStreamReaderURLRequestJob(
      request,
      network_delegate,
      make_scoped_ptr(
          new StreamReaderJobDelegateImpl(aw_web_resource_response.Pass())));
}

}  // namespace android_webview
