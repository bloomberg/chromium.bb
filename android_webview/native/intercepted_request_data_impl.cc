// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/intercepted_request_data_impl.h"

#include "android_webview/browser/net/android_stream_reader_url_request_job.h"
#include "android_webview/native/input_stream_impl.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/InterceptedRequestData_jni.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

using base::android::ScopedJavaLocalRef;

namespace android_webview {

namespace {

class StreamReaderJobDelegateImpl :
    public AndroidStreamReaderURLRequestJob::Delegate {
 public:
    StreamReaderJobDelegateImpl(
        scoped_ptr<InterceptedRequestDataImpl> intercepted_request_data)
        : intercepted_request_data_impl_(intercepted_request_data.Pass()) {
      DCHECK(intercepted_request_data_impl_);
    }

    virtual scoped_ptr<InputStream> OpenInputStream(
        JNIEnv* env,
        const GURL& url) OVERRIDE {
      return intercepted_request_data_impl_->GetInputStream(env).Pass();
    }

    virtual void OnInputStreamOpenFailed(net::URLRequest* request,
                                         bool* restart) OVERRIDE {
      *restart = false;
    }

    virtual bool GetMimeType(JNIEnv* env,
                             net::URLRequest* request,
                             android_webview::InputStream* stream,
                             std::string* mime_type) OVERRIDE {
      return intercepted_request_data_impl_->GetMimeType(env, mime_type);
    }

    virtual bool GetCharset(JNIEnv* env,
                            net::URLRequest* request,
                            android_webview::InputStream* stream,
                            std::string* charset) OVERRIDE {
      return intercepted_request_data_impl_->GetCharset(env, charset);
    }

 private:
    scoped_ptr<InterceptedRequestDataImpl> intercepted_request_data_impl_;
};

} // namespace

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
      scoped_ptr<AndroidStreamReaderURLRequestJob::Delegate>(
          new StreamReaderJobDelegateImpl(
              // PassAs rightfully doesn't support downcasts.
              scoped_ptr<InterceptedRequestDataImpl>(
                  static_cast<InterceptedRequestDataImpl*>(
                      intercepted_request_data.release())))));
}

InterceptedRequestDataImpl::InterceptedRequestDataImpl(
    const base::android::JavaRef<jobject>& obj)
  : java_object_(obj) {
}

InterceptedRequestDataImpl::~InterceptedRequestDataImpl() {
}

scoped_ptr<InputStream>
InterceptedRequestDataImpl::GetInputStream(JNIEnv* env) const {
  ScopedJavaLocalRef<jobject> jstream =
      Java_InterceptedRequestData_getData(env, java_object_.obj());
  if (jstream.is_null())
    return scoped_ptr<InputStream>();
  return make_scoped_ptr<InputStream>(new InputStreamImpl(jstream));
}

bool InterceptedRequestDataImpl::GetMimeType(JNIEnv* env,
                                             std::string* mime_type) const {
  ScopedJavaLocalRef<jstring> jstring_mime_type =
      Java_InterceptedRequestData_getMimeType(env, java_object_.obj());
  if (jstring_mime_type.is_null())
    return false;
  *mime_type = ConvertJavaStringToUTF8(jstring_mime_type);
  return true;
}

bool InterceptedRequestDataImpl::GetCharset(
    JNIEnv* env, std::string* charset) const {
  ScopedJavaLocalRef<jstring> jstring_charset =
      Java_InterceptedRequestData_getCharset(env, java_object_.obj());
  if (jstring_charset.is_null())
    return false;
  *charset = ConvertJavaStringToUTF8(jstring_charset);
  return true;
}

bool RegisterInterceptedRequestData(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} // namespace android_webview
