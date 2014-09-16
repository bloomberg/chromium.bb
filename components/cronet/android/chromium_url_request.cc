// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/chromium_url_request.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "components/cronet/android/url_request_adapter.h"
#include "components/cronet/android/url_request_context_adapter.h"
#include "jni/ChromiumUrlRequest_jni.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"

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
                        URLRequestAdapter* request,
                        jstring content_type) {
  DCHECK(request != NULL);

  std::string method_post("POST");
  request->SetMethod(method_post);

  std::string content_type_header("Content-Type");
  std::string content_type_string(
      base::android::ConvertJavaStringToUTF8(env, content_type));

  request->AddHeader(content_type_header, content_type_string);
}

// A delegate of URLRequestAdapter that delivers callbacks to the Java layer.
class JniURLRequestAdapterDelegate
    : public URLRequestAdapter::URLRequestAdapterDelegate {
 public:
  JniURLRequestAdapterDelegate(JNIEnv* env, jobject owner) {
    owner_ = env->NewGlobalRef(owner);
  }

  virtual void OnResponseStarted(URLRequestAdapter* request) OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_ChromiumUrlRequest_onResponseStarted(env, owner_);
  }

  virtual void OnBytesRead(URLRequestAdapter* request) OVERRIDE {
    int bytes_read = request->bytes_read();
    if (bytes_read != 0) {
      JNIEnv* env = base::android::AttachCurrentThread();
      base::android::ScopedJavaLocalRef<jobject> java_buffer(
          env, env->NewDirectByteBuffer(request->Data(), bytes_read));
      cronet::Java_ChromiumUrlRequest_onBytesRead(
          env, owner_, java_buffer.obj());
    }
  }

  virtual void OnRequestFinished(URLRequestAdapter* request) OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_ChromiumUrlRequest_finish(env, owner_);
  }

  virtual int ReadFromUploadChannel(net::IOBuffer* buf,
                                    int buf_length) OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> java_buffer(
        env, env->NewDirectByteBuffer(buf->data(), buf_length));
    jint bytes_read = cronet::Java_ChromiumUrlRequest_readFromUploadChannel(
        env, owner_, java_buffer.obj());
    return bytes_read;
  }

 protected:
  virtual ~JniURLRequestAdapterDelegate() {
    JNIEnv* env = base::android::AttachCurrentThread();
    env->DeleteGlobalRef(owner_);
  }

 private:
  jobject owner_;

  DISALLOW_COPY_AND_ASSIGN(JniURLRequestAdapterDelegate);
};

}  // namespace

// Explicitly register static JNI functions.
bool ChromiumUrlRequestRegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong CreateRequestAdapter(JNIEnv* env,
                                  jobject object,
                                  jlong urlRequestContextAdapter,
                                  jstring url_string,
                                  jint priority) {
  URLRequestContextAdapter* context =
      reinterpret_cast<URLRequestContextAdapter*>(urlRequestContextAdapter);
  DCHECK(context != NULL);

  GURL url(base::android::ConvertJavaStringToUTF8(env, url_string));

  VLOG(1) << "New chromium network request: " << url.possibly_invalid_spec();

  URLRequestAdapter* adapter =
      new URLRequestAdapter(context,
                            new JniURLRequestAdapterDelegate(env, object),
                            url,
                            ConvertRequestPriority(priority));

  return reinterpret_cast<jlong>(adapter);
}

// synchronized
static void AddHeader(JNIEnv* env,
                      jobject object,
                      jlong urlRequestAdapter,
                      jstring name,
                      jstring value) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  DCHECK(request);

  std::string name_string(base::android::ConvertJavaStringToUTF8(env, name));
  std::string value_string(base::android::ConvertJavaStringToUTF8(env, value));

  request->AddHeader(name_string, value_string);
}

static void SetMethod(JNIEnv* env,
                      jobject object,
                      jlong urlRequestAdapter,
                      jstring method) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  DCHECK(request);

  std::string method_string(
      base::android::ConvertJavaStringToUTF8(env, method));

  request->SetMethod(method_string);
}

static void SetUploadData(JNIEnv* env,
                          jobject object,
                          jlong urlRequestAdapter,
                          jstring content_type,
                          jbyteArray content) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
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
                             jlong urlRequestAdapter,
                             jstring content_type,
                             jlong content_length) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  SetPostContentType(env, request, content_type);

  request->SetUploadChannel(env, content_length);
}

static void EnableChunkedUpload(JNIEnv* env,
                               jobject object,
                               jlong urlRequestAdapter,
                               jstring content_type) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  SetPostContentType(env, request, content_type);

  request->EnableChunkedUpload();
}

static void AppendChunk(JNIEnv* env,
                        jobject object,
                        jlong urlRequestAdapter,
                        jobject chunk_byte_buffer,
                        jint chunk_size,
                        jboolean is_last_chunk) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  DCHECK(chunk_byte_buffer != NULL);

  void* chunk = env->GetDirectBufferAddress(chunk_byte_buffer);
  request->AppendChunk(
      reinterpret_cast<const char*>(chunk), chunk_size, is_last_chunk);
}

/* synchronized */
static void Start(JNIEnv* env, jobject object, jlong urlRequestAdapter) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  if (request != NULL) {
    request->Start();
  }
}

/* synchronized */
static void DestroyRequestAdapter(JNIEnv* env,
                                  jobject object,
                                  jlong urlRequestAdapter) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  if (request != NULL) {
    request->Destroy();
  }
}

/* synchronized */
static void Cancel(JNIEnv* env, jobject object, jlong urlRequestAdapter) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  if (request != NULL) {
    request->Cancel();
  }
}

static jint GetErrorCode(JNIEnv* env, jobject object, jlong urlRequestAdapter) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
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
                              jlong urlRequestAdapter) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  int error_code = request->error_code();
  char buffer[200];
  std::string error_string = net::ErrorToString(error_code);
  snprintf(buffer,
           sizeof(buffer),
           "System error: %s(%d)",
           error_string.c_str(),
           error_code);
  return ConvertUTF8ToJavaString(env, buffer).Release();
}

static jint GetHttpStatusCode(JNIEnv* env,
                              jobject object,
                              jlong urlRequestAdapter) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  return request->http_status_code();
}

static jstring GetContentType(JNIEnv* env,
                              jobject object,
                              jlong urlRequestAdapter) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
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
                              jlong urlRequestAdapter) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  if (request == NULL) {
    return 0;
  }
  return request->content_length();
}

static jstring GetHeader(JNIEnv* env,
                         jobject object,
                         jlong urlRequestAdapter,
                         jstring name) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
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

static void GetAllHeaders(JNIEnv* env,
                          jobject object,
                          jlong urlRequestAdapter,
                          jobject headersMap) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  if (request == NULL)
    return;

  net::HttpResponseHeaders* headers = request->GetResponseHeaders();
  if (headers == NULL)
    return;

  void* iter = NULL;
  std::string header_name;
  std::string header_value;
  while (headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
    ScopedJavaLocalRef<jstring> name =
        ConvertUTF8ToJavaString(env, header_name);
    ScopedJavaLocalRef<jstring> value =
        ConvertUTF8ToJavaString(env, header_value);
    Java_ChromiumUrlRequest_onAppendResponseHeader(
        env, object, headersMap, name.Release(), value.Release());
  }

  // Some implementations (notably HttpURLConnection) include a mapping for the
  // null key; in HTTP's case, this maps to the HTTP status line.
  ScopedJavaLocalRef<jstring> status_line =
      ConvertUTF8ToJavaString(env, headers->GetStatusLine());
  Java_ChromiumUrlRequest_onAppendResponseHeader(
      env, object, headersMap, NULL, status_line.Release());
}

static jstring GetNegotiatedProtocol(JNIEnv* env,
                                     jobject object,
                                     jlong urlRequestAdapter) {
  URLRequestAdapter* request =
      reinterpret_cast<URLRequestAdapter*>(urlRequestAdapter);
  if (request == NULL)
    return ConvertUTF8ToJavaString(env, "").Release();

  std::string negotiated_protocol = request->GetNegotiatedProtocol();
  return ConvertUTF8ToJavaString(env, negotiated_protocol.c_str()).Release();
}

}  // namespace cronet
