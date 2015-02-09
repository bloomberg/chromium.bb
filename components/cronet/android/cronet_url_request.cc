// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_url_request.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/macros.h"
#include "components/cronet/android/cronet_url_request_adapter.h"
#include "components/cronet/android/cronet_url_request_context_adapter.h"
#include "jni/CronetUrlRequest_jni.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

using base::android::ConvertUTF8ToJavaString;

// This file contains all the plumbing to handle the bidirectional communication
// between the Java CronetURLRequest and C++ CronetURLRequestAdapter.

namespace cronet {
namespace {

// A delegate of CronetURLRequestAdapter that delivers callbacks to the Java
// layer. Created on a Java thread, but always called and destroyed on the
// Network thread.
class JniCronetURLRequestAdapterDelegate
    : public CronetURLRequestAdapter::CronetURLRequestAdapterDelegate {
 public:
  JniCronetURLRequestAdapterDelegate(JNIEnv* env, jobject owner) {
    owner_.Reset(env, owner);
  }

  // CronetURLRequestAdapter::CronetURLRequestAdapterDelegate implementation.

  void OnRedirect(const GURL& new_location, int http_status_code) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_CronetUrlRequest_onRedirect(
        env,
        owner_.obj(),
        ConvertUTF8ToJavaString(env, new_location.spec()).obj(),
        http_status_code);
  }

  void OnResponseStarted(int http_status_code) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_CronetUrlRequest_onResponseStarted(env,
                                                    owner_.obj(),
                                                    http_status_code);
  }

  void OnBytesRead(unsigned char* bytes,
                   int bytes_read) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> java_buffer(
        env, env->NewDirectByteBuffer(bytes, bytes_read));
    cronet::Java_CronetUrlRequest_onDataReceived(
        env, owner_.obj(), java_buffer.obj());
  }

  void OnRequestFinished() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_CronetUrlRequest_onSucceeded(env, owner_.obj());
  }

  void OnError(int net_error) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_CronetUrlRequest_onError(
        env,
        owner_.obj(),
        net_error,
        ConvertUTF8ToJavaString(env, net::ErrorToString(net_error)).obj());
  }

 private:
  ~JniCronetURLRequestAdapterDelegate() override {
  }

  // Java object that owns the CronetURLRequestContextAdapter, which owns this
  // delegate.
  base::android::ScopedJavaGlobalRef<jobject> owner_;

  DISALLOW_COPY_AND_ASSIGN(JniCronetURLRequestAdapterDelegate);
};

}  // namespace

// Explicitly register static JNI functions.
bool CronetUrlRequestRegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong CreateRequestAdapter(JNIEnv* env,
                                  jobject jurl_request,
                                  jlong jurl_request_context_adapter,
                                  jstring jurl_string,
                                  jint jpriority) {
  CronetURLRequestContextAdapter* context_adapter =
      reinterpret_cast<CronetURLRequestContextAdapter*>(
          jurl_request_context_adapter);
  DCHECK(context_adapter);

  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl_string));

  VLOG(1) << "New chromium network request_adapter: "
          << url.possibly_invalid_spec();

  scoped_ptr<CronetURLRequestAdapter::CronetURLRequestAdapterDelegate> delegate(
      new JniCronetURLRequestAdapterDelegate(env, jurl_request));

  CronetURLRequestAdapter* adapter = new CronetURLRequestAdapter(
      context_adapter,
      delegate.Pass(),
      url,
      static_cast<net::RequestPriority>(jpriority));

  return reinterpret_cast<jlong>(adapter);
}

static jboolean SetHttpMethod(JNIEnv* env,
                              jobject jurl_request,
                              jlong jurl_request_adapter,
                              jstring jmethod) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(!request_adapter->IsOnNetworkThread());
  std::string method(base::android::ConvertJavaStringToUTF8(env, jmethod));
  // Http method is a token, just as header name.
  if (!net::HttpUtil::IsValidHeaderName(method))
    return JNI_FALSE;
  request_adapter->set_method(method);
  return JNI_TRUE;
}

static jboolean AddHeader(JNIEnv* env,
                          jobject jurl_request,
                          jlong jurl_request_adapter,
                          jstring jname,
                          jstring jvalue) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(!request_adapter->IsOnNetworkThread());
  std::string name(base::android::ConvertJavaStringToUTF8(env, jname));
  std::string value(base::android::ConvertJavaStringToUTF8(env, jvalue));
  if (!net::HttpUtil::IsValidHeaderName(name) ||
      !net::HttpUtil::IsValidHeaderValue(value)) {
    return JNI_FALSE;
  }
  request_adapter->AddRequestHeader(name, value);
  return JNI_TRUE;
}

static void Start(JNIEnv* env,
                  jobject jurl_request,
                  jlong jurl_request_adapter) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(!request_adapter->IsOnNetworkThread());
  request_adapter->PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetURLRequestAdapter::Start,
                 base::Unretained(request_adapter)));
}

static void DestroyRequestAdapter(JNIEnv* env,
                                  jobject jurl_request,
                                  jlong jurl_request_adapter) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(!request_adapter->IsOnNetworkThread());
  request_adapter->PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetURLRequestAdapter::Destroy,
                 base::Unretained(request_adapter)));
}

static void ReceiveData(JNIEnv* env,
                        jobject jcaller,
                        jlong jurl_request_adapter) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(!request_adapter->IsOnNetworkThread());
  request_adapter->PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetURLRequestAdapter::ReadData,
                 base::Unretained(request_adapter)));
}

static void FollowDeferredRedirect(JNIEnv* env,
                                   jobject jcaller,
                                   jlong jurl_request_adapter) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(!request_adapter->IsOnNetworkThread());
  request_adapter->PostTaskToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetURLRequestAdapter::FollowDeferredRedirect,
                 base::Unretained(request_adapter)));
}

static void DisableCache(JNIEnv* env,
                         jobject jurl_request,
                         jlong jurl_request_adapter) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(!request_adapter->IsOnNetworkThread());
  request_adapter->DisableCache();
}

static void PopulateResponseHeaders(JNIEnv* env,
                                    jobject jurl_request,
                                    jlong jurl_request_adapter,
                                    jobject jheaders_list) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(request_adapter->IsOnNetworkThread());

  const net::HttpResponseHeaders* headers =
      request_adapter->GetResponseHeaders();
  if (headers == nullptr)
    return;

  void* iter = nullptr;
  std::string header_name;
  std::string header_value;
  while (headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
    ScopedJavaLocalRef<jstring> name =
        ConvertUTF8ToJavaString(env, header_name);
    ScopedJavaLocalRef<jstring> value =
        ConvertUTF8ToJavaString(env, header_value);
    Java_CronetUrlRequest_onAppendResponseHeader(
        env, jurl_request, jheaders_list, name.obj(), value.obj());
  }
}

static jstring GetNegotiatedProtocol(JNIEnv* env,
                                     jobject jurl_request,
                                     jlong jurl_request_adapter) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(request_adapter->IsOnNetworkThread());
  return ConvertUTF8ToJavaString(
      env, request_adapter->GetNegotiatedProtocol()).Release();
}

static jboolean GetWasCached(JNIEnv* env,
                             jobject jurl_request,
                             jlong jurl_request_adapter) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(request_adapter->IsOnNetworkThread());
  return request_adapter->GetWasCached() ? JNI_TRUE : JNI_FALSE;
}

static jlong GetTotalReceivedBytes(JNIEnv* env,
                                   jobject jurl_request,
                                   jlong jurl_request_adapter) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(request_adapter->IsOnNetworkThread());
  return request_adapter->GetTotalReceivedBytes();
}

static jstring GetHttpStatusText(JNIEnv* env,
                                 jobject jurl_request,
                                 jlong jurl_request_adapter) {
  DCHECK(jurl_request_adapter);
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jurl_request_adapter);
  DCHECK(request_adapter->IsOnNetworkThread());
  const net::HttpResponseHeaders* headers =
      request_adapter->GetResponseHeaders();
  return ConvertUTF8ToJavaString(env, headers->GetStatusText()).Release();
}

}  // namespace cronet
