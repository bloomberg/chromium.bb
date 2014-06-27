// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/org_chromium_net_UrlRequest.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "components/cronet/android/url_request_context_peer.h"
#include "components/cronet/android/url_request_peer.h"
#include "jni/UrlRequest_jni.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"

using base::android::ConvertUTF8ToJavaString;

namespace cronet {
namespace {

net::RequestPriority ConvertRequestPriority(jint request_priority) {
  switch (request_priority) {
    case REQUEST_PRIORITY_IDLE:
      return net::IDLE;
    case REQUEST_PRIORITY_LOWEST:
      return net::LOWEST;
    case REQUEST_PRIORITY_LOW:
      return net::LOW;
    case REQUEST_PRIORITY_MEDIUM:
      return net::MEDIUM;
    case REQUEST_PRIORITY_HIGHEST:
      return net::HIGHEST;
    default:
      return net::LOWEST;
  }
}

void SetPostContentType(JNIEnv* env,
                        URLRequestPeer* request,
                        jstring content_type) {
  DCHECK(request != NULL);

  std::string method_post("POST");
  request->SetMethod(method_post);

  std::string content_type_header("Content-Type");

  const char* content_type_utf8 = env->GetStringUTFChars(content_type, NULL);
  std::string content_type_string(content_type_utf8);
  env->ReleaseStringUTFChars(content_type, content_type_utf8);

  request->AddHeader(content_type_header, content_type_string);
}

// A delegate of URLRequestPeer that delivers callbacks to the Java layer.
class JniURLRequestPeerDelegate
    : public URLRequestPeer::URLRequestPeerDelegate {
 public:
  JniURLRequestPeerDelegate(JNIEnv* env, jobject owner) {
    owner_ = env->NewGlobalRef(owner);
  }

  virtual void OnResponseStarted(URLRequestPeer* request) OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_UrlRequest_onResponseStarted(env, owner_);
  }

  virtual void OnBytesRead(URLRequestPeer* request) OVERRIDE {
    int bytes_read = request->bytes_read();
    if (bytes_read != 0) {
      JNIEnv* env = base::android::AttachCurrentThread();
      jobject bytebuf = env->NewDirectByteBuffer(request->Data(), bytes_read);
      cronet::Java_UrlRequest_onBytesRead(env, owner_, bytebuf);
      env->DeleteLocalRef(bytebuf);
    }
  }

  virtual void OnRequestFinished(URLRequestPeer* request) OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_UrlRequest_finish(env, owner_);
  }

 protected:
  virtual ~JniURLRequestPeerDelegate() {
    JNIEnv* env = base::android::AttachCurrentThread();
    env->DeleteGlobalRef(owner_);
  }

 private:
  jobject owner_;

  DISALLOW_COPY_AND_ASSIGN(JniURLRequestPeerDelegate);
};

}  // namespace

// Explicitly register static JNI functions.
bool UrlRequestRegisterJni(JNIEnv* env) { return RegisterNativesImpl(env); }

static jlong CreateRequestPeer(JNIEnv* env,
                               jobject object,
                               jlong urlRequestContextPeer,
                               jstring url_string,
                               jint priority) {
  URLRequestContextPeer* context =
      reinterpret_cast<URLRequestContextPeer*>(urlRequestContextPeer);
  DCHECK(context != NULL);

  const char* url_utf8 = env->GetStringUTFChars(url_string, NULL);

  DVLOG(context->logging_level())
      << "New chromium network request. URL:" << url_utf8;

  GURL url(url_utf8);

  env->ReleaseStringUTFChars(url_string, url_utf8);

  URLRequestPeer* peer =
      new URLRequestPeer(context,
                         new JniURLRequestPeerDelegate(env, object),
                         url,
                         ConvertRequestPriority(priority));

  return reinterpret_cast<jlong>(peer);
}

// synchronized
static void AddHeader(JNIEnv* env,
                      jobject object,
                      jlong urlRequestPeer,
                      jstring name,
                      jstring value) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  DCHECK(request);

  std::string name_string(base::android::ConvertJavaStringToUTF8(env, name));
  std::string value_string(base::android::ConvertJavaStringToUTF8(env, value));

  request->AddHeader(name_string, value_string);
}

static void SetMethod(JNIEnv* env,
                      jobject object,
                      jlong urlRequestPeer,
                      jstring method) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  DCHECK(request);

  std::string method_string(
      base::android::ConvertJavaStringToUTF8(env, method));

  request->SetMethod(method_string);
}

static void SetUploadData(JNIEnv* env,
                          jobject object,
                          jlong urlRequestPeer,
                          jstring content_type,
                          jbyteArray content) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  SetPostContentType(env, request, content_type);

  if (content != NULL) {
    jsize size = env->GetArrayLength(content);
    if (size > 0) {
      jbyte* content_bytes = env->GetByteArrayElements(content, NULL);
      request->SetUploadContent(reinterpret_cast<const char*>(content_bytes),
                                size);
      env->ReleaseByteArrayElements(content, content_bytes, 0);
    }
  }
}

static void SetUploadChannel(JNIEnv* env,
                             jobject object,
                             jlong urlRequestPeer,
                             jstring content_type,
                             jobject content,
                             jlong content_length) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  SetPostContentType(env, request, content_type);

  request->SetUploadChannel(env, content, content_length);
}


/* synchronized */
static void Start(JNIEnv* env, jobject object, jlong urlRequestPeer) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  if (request != NULL) {
    request->Start();
  }
}

/* synchronized */
static void DestroyRequestPeer(JNIEnv* env,
                               jobject object,
                               jlong urlRequestPeer) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  if (request != NULL) {
    request->Destroy();
  }
}

/* synchronized */
static void Cancel(JNIEnv* env, jobject object, jlong urlRequestPeer) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  if (request != NULL) {
    request->Cancel();
  }
}

static jint GetErrorCode(JNIEnv* env, jobject object, jlong urlRequestPeer) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  int error_code = request->error_code();
  switch (error_code) {
    // TODO(mef): Investigate returning success on positive values, too, as
    // they technically indicate success.
    case net::OK:
      return REQUEST_ERROR_SUCCESS;

    // TODO(mef): Investigate this. The fact is that Chrome does not do this,
    // and this library is not just being used for downloads.

    // Comment from src/content/browser/download/download_resource_handler.cc:
    // ERR_CONTENT_LENGTH_MISMATCH and ERR_INCOMPLETE_CHUNKED_ENCODING are
    // allowed since a number of servers in the wild close the connection too
    // early by mistake. Other browsers - IE9, Firefox 11.0, and Safari 5.1.4 -
    // treat downloads as complete in both cases, so we follow their lead.
    case net::ERR_CONTENT_LENGTH_MISMATCH:
    case net::ERR_INCOMPLETE_CHUNKED_ENCODING:
      return REQUEST_ERROR_SUCCESS;

    case net::ERR_INVALID_URL:
    case net::ERR_DISALLOWED_URL_SCHEME:
    case net::ERR_UNKNOWN_URL_SCHEME:
      return REQUEST_ERROR_MALFORMED_URL;

    case net::ERR_CONNECTION_TIMED_OUT:
      return REQUEST_ERROR_CONNECTION_TIMED_OUT;

    case net::ERR_NAME_NOT_RESOLVED:
      return REQUEST_ERROR_UNKNOWN_HOST;
  }
  return REQUEST_ERROR_UNKNOWN;
}

static jstring GetErrorString(JNIEnv* env,
                              jobject object,
                              jlong urlRequestPeer) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  int error_code = request->error_code();
  char buffer[200];
  snprintf(buffer,
           sizeof(buffer),
           "System error: %s(%d)",
           net::ErrorToString(error_code),
           error_code);
  return ConvertUTF8ToJavaString(env, buffer).Release();
}

static jint GetHttpStatusCode(JNIEnv* env,
                              jobject object,
                              jlong urlRequestPeer) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  return request->http_status_code();
}

static jstring GetContentType(JNIEnv* env,
                              jobject object,
                              jlong urlRequestPeer) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  if (request == NULL) {
    return NULL;
  }
  std::string type = request->content_type();
  if (!type.empty()) {
    return ConvertUTF8ToJavaString(env, type.c_str()).Release();
  } else {
    return NULL;
  }
}

static jlong GetContentLength(JNIEnv* env,
                              jobject object,
                              jlong urlRequestPeer) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  if (request == NULL) {
    return 0;
  }
  return request->content_length();
}

static jstring GetHeader(
    JNIEnv* env, jobject object, jlong urlRequestPeer, jstring name) {
  URLRequestPeer* request = reinterpret_cast<URLRequestPeer*>(urlRequestPeer);
  if (request == NULL) {
    return NULL;
  }

  std::string name_string = base::android::ConvertJavaStringToUTF8(env, name);
  std::string value = request->GetHeader(name_string);
  if (!value.empty()) {
    return ConvertUTF8ToJavaString(env, value.c_str()).Release();
  } else {
    return NULL;
  }
}

}  // namespace cronet
