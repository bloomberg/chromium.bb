// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cronet/android/org_chromium_net_UrlRequest.h"

#include <stdio.h>

#include "base/macros.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/cronet/android/org_chromium_net_UrlRequestContext.h"
#include "net/cronet/android/url_request_context_peer.h"
#include "net/cronet/android/url_request_peer.h"

// TODO(mef): Replace following definitions with generated UrlRequest_jni.h
//#include "jni/UrlRequest_jni.h"
namespace {

jclass g_class;
jmethodID g_method_finish;
jmethodID g_method_onAppendChunkCompleted;
jmethodID g_method_onResponseStarted;
jmethodID g_method_onReadBytes;
jclass g_class_OutputStream;
jmethodID g_method_write;
jfieldID g_request_field;

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

// Stores a reference to the request in a java field.
void SetNativeObject(JNIEnv* env, jobject object, URLRequestPeer* request) {
  env->SetLongField(object, g_request_field, reinterpret_cast<jlong>(request));
}

// Returns a reference to the request, which is stored in a field of the Java
// object.
URLRequestPeer* GetNativeObject(JNIEnv* env, jobject object) {
  return reinterpret_cast<URLRequestPeer*>(
      env->GetLongField(object, g_request_field));
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

}  // namespace

// Find Java classes and retain them.
bool UrlRequestRegisterJni(JNIEnv* env) {
  g_class = reinterpret_cast<jclass>(
      env->NewGlobalRef(env->FindClass("org/chromium/net/UrlRequest")));
  g_method_finish = env->GetMethodID(g_class, "finish", "()V");
  g_method_onAppendChunkCompleted =
      env->GetMethodID(g_class, "onAppendChunkCompleted", "()V");
  g_method_onResponseStarted =
      env->GetMethodID(g_class, "onResponseStarted", "()V");
  g_method_onReadBytes =
      env->GetMethodID(g_class, "onBytesRead", "(Ljava/nio/ByteBuffer;)V");
  g_request_field = env->GetFieldID(g_class, "mRequest", "J");

  g_class_OutputStream = reinterpret_cast<jclass>(
      env->NewGlobalRef(env->FindClass("java/io/OutputStream")));
  g_method_write = env->GetMethodID(g_class_OutputStream, "write", "([BII)V");

  if (!g_class || !g_method_finish || !g_method_onAppendChunkCompleted ||
      !g_method_onResponseStarted || !g_method_onReadBytes ||
      !g_request_field || !g_class_OutputStream || !g_method_write) {
    return false;
  }
  return true;
}

// A delegate of URLRequestPeer that delivers callbacks to the Java layer.
class JniURLRequestPeerDelegate
    : public URLRequestPeer::URLRequestPeerDelegate {
 public:
  JniURLRequestPeerDelegate(JNIEnv* env, jobject owner) {
    owner_ = env->NewGlobalRef(owner);
    env->GetJavaVM(&vm_);
  }

  virtual void OnAppendChunkCompleted(URLRequestPeer* request) OVERRIDE {
    JNIEnv* env = GetEnv(vm_);
    env->CallVoidMethod(owner_, g_method_onAppendChunkCompleted);
    if (env->ExceptionOccurred()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
  }

  virtual void OnResponseStarted(URLRequestPeer* request) OVERRIDE {
    JNIEnv* env = GetEnv(vm_);
    env->CallVoidMethod(owner_, g_method_onResponseStarted);
    if (env->ExceptionOccurred()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
  }

  virtual void OnBytesRead(URLRequestPeer* request) OVERRIDE {
    int bytes_read = request->bytes_read();
    if (bytes_read != 0) {
      JNIEnv* env = GetEnv(vm_);
      jobject bytebuf = env->NewDirectByteBuffer(request->Data(), bytes_read);
      env->CallVoidMethod(owner_, g_method_onReadBytes, bytebuf);
      env->DeleteLocalRef(bytebuf);
      if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
      }
    }
  }

  virtual void OnRequestFinished(URLRequestPeer* request) OVERRIDE {
    JNIEnv* env = GetEnv(vm_);
    env->CallVoidMethod(owner_, g_method_finish);
    if (env->ExceptionOccurred()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
  }

 protected:
  virtual ~JniURLRequestPeerDelegate() { GetEnv(vm_)->DeleteGlobalRef(owner_); }

 private:
  jobject owner_;
  JavaVM* vm_;

  DISALLOW_COPY_AND_ASSIGN(JniURLRequestPeerDelegate);
};

JNIEXPORT void JNICALL
Java_org_chromium_net_UrlRequest_nativeInit(JNIEnv* env,
                                            jobject object,
                                            jobject request_context,
                                            jstring url_string,
                                            jint priority) {
  URLRequestContextPeer* context =
      GetURLRequestContextPeer(env, request_context);
  DCHECK(context != NULL);

  const char* url_utf8 = env->GetStringUTFChars(url_string, NULL);

  DVLOG(context->logging_level())
      << "New chromium network request. URL:" << url_utf8;

  GURL url(url_utf8);

  env->ReleaseStringUTFChars(url_string, url_utf8);

  URLRequestPeer* request =
      new URLRequestPeer(context,
                         new JniURLRequestPeerDelegate(env, object),
                         url,
                         ConvertRequestPriority(priority));

  SetNativeObject(env, object, request);
}

// synchronized
JNIEXPORT void JNICALL
Java_org_chromium_net_UrlRequest_nativeAddHeader(JNIEnv* env,
                                                 jobject object,
                                                 jstring name,
                                                 jstring value) {
  URLRequestPeer* request = GetNativeObject(env, object);
  DCHECK(request != NULL);

  const char* name_utf8 = env->GetStringUTFChars(name, NULL);
  std::string name_string(name_utf8);
  env->ReleaseStringUTFChars(name, name_utf8);

  const char* value_utf8 = env->GetStringUTFChars(value, NULL);
  std::string value_string(value_utf8);
  env->ReleaseStringUTFChars(value, value_utf8);

  request->AddHeader(name_string, value_string);
}

JNIEXPORT void JNICALL
Java_org_chromium_net_UrlRequest_nativeSetPostData(JNIEnv* env,
                                                   jobject object,
                                                   jstring content_type,
                                                   jbyteArray content) {
  URLRequestPeer* request = GetNativeObject(env, object);
  SetPostContentType(env, request, content_type);

  if (content != NULL) {
    jsize size = env->GetArrayLength(content);
    if (size > 0) {
      jbyte* content_bytes = env->GetByteArrayElements(content, NULL);
      request->SetPostContent(reinterpret_cast<const char*>(content_bytes),
                              size);
      env->ReleaseByteArrayElements(content, content_bytes, 0);
    }
  }
}

JNIEXPORT void JNICALL
Java_org_chromium_net_UrlRequest_nativeBeginChunkedUpload(
    JNIEnv* env,
    jobject object,
    jstring content_type) {
  URLRequestPeer* request = GetNativeObject(env, object);
  SetPostContentType(env, request, content_type);

  request->EnableStreamingUpload();
}

JNIEXPORT void JNICALL
Java_org_chromium_net_UrlRequest_nativeAppendChunk(JNIEnv* env,
                                                   jobject object,
                                                   jobject chunk_byte_buffer,
                                                   jint chunk_size,
                                                   jboolean is_last_chunk) {
  URLRequestPeer* request = GetNativeObject(env, object);
  CHECK(request != NULL);

  if (chunk_byte_buffer != NULL) {
    void* chunk = env->GetDirectBufferAddress(chunk_byte_buffer);
    request->AppendChunk(
        reinterpret_cast<const char*>(chunk), chunk_size, is_last_chunk);
  }
}

/* synchronized */
JNIEXPORT void JNICALL
Java_org_chromium_net_UrlRequest_nativeStart(JNIEnv* env, jobject object) {
  URLRequestPeer* request = GetNativeObject(env, object);
  if (request != NULL) {
    request->Start();
  }
}

/* synchronized */
JNIEXPORT void JNICALL
Java_org_chromium_net_UrlRequest_nativeRecycle(JNIEnv* env, jobject object) {
  URLRequestPeer* request = GetNativeObject(env, object);
  if (request != NULL) {
    request->Destroy();
  }

  SetNativeObject(env, object, NULL);
}

/* synchronized */
JNIEXPORT void JNICALL
Java_org_chromium_net_UrlRequest_nativeCancel(JNIEnv* env, jobject object) {
  URLRequestPeer* request = GetNativeObject(env, object);
  if (request != NULL) {
    request->Cancel();
  }
}

JNIEXPORT jint JNICALL
Java_org_chromium_net_UrlRequest_nativeGetErrorCode(JNIEnv* env,
                                                    jobject object) {
  URLRequestPeer* request = GetNativeObject(env, object);
  int error_code = request->error_code();
  switch (error_code) {
    // TODO(mef): Investigate returning success on positive values, too, as
    // they technically indicate success.
    case net::OK:
      return ERROR_SUCCESS;

    // TODO(mef): Investigate this. The fact is that Chrome does not do this,
    // and this library is not just being used for downloads.

    // Comment from src/content/browser/download/download_resource_handler.cc:
    // ERR_CONTENT_LENGTH_MISMATCH and ERR_INCOMPLETE_CHUNKED_ENCODING are
    // allowed since a number of servers in the wild close the connection too
    // early by mistake. Other browsers - IE9, Firefox 11.0, and Safari 5.1.4 -
    // treat downloads as complete in both cases, so we follow their lead.
    case net::ERR_CONTENT_LENGTH_MISMATCH:
    case net::ERR_INCOMPLETE_CHUNKED_ENCODING:
      return ERROR_SUCCESS;

    case net::ERR_INVALID_URL:
    case net::ERR_DISALLOWED_URL_SCHEME:
    case net::ERR_UNKNOWN_URL_SCHEME:
      return ERROR_MALFORMED_URL;

    case net::ERR_CONNECTION_TIMED_OUT:
      return ERROR_CONNECTION_TIMED_OUT;

    case net::ERR_NAME_NOT_RESOLVED:
      return ERROR_UNKNOWN_HOST;
  }
  return ERROR_UNKNOWN;
}

JNIEXPORT jstring JNICALL
Java_org_chromium_net_UrlRequest_nativeGetErrorString(JNIEnv* env,
                                                      jobject object) {
  int error_code = GetNativeObject(env, object)->error_code();
  char buffer[200];
  snprintf(buffer,
           sizeof(buffer),
           "System error: %s(%d)",
           net::ErrorToString(error_code),
           error_code);
  return env->NewStringUTF(buffer);
}

JNIEXPORT jint JNICALL
Java_org_chromium_net_UrlRequest_getHttpStatusCode(JNIEnv* env,
                                                   jobject object) {
  return GetNativeObject(env, object)->http_status_code();
}

JNIEXPORT jstring JNICALL
Java_org_chromium_net_UrlRequest_nativeGetContentType(JNIEnv* env,
                                                      jobject object) {
  URLRequestPeer* request = GetNativeObject(env, object);
  if (request == NULL) {
    return NULL;
  }
  std::string type = request->content_type();
  if (!type.empty()) {
    return env->NewStringUTF(type.c_str());
  } else {
    return NULL;
  }
}

JNIEXPORT jlong JNICALL
Java_org_chromium_net_UrlRequest_nativeGetContentLength(JNIEnv* env,
                                                        jobject object) {
  URLRequestPeer* request = GetNativeObject(env, object);
  if (request == NULL) {
    return 0;
  }
  return request->content_length();
}
