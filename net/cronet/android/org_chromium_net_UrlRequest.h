// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CRONET_ANDROID_URLREQUEST_H_
#define NET_CRONET_ANDROID_URLREQUEST_H_

#include <jni.h>

extern "C" {

// Native method implementations of the org.chromium.netjni.UrlRequest class.

// Request priorities. Also declared in UrlRequest.java
const jint REQUEST_PRIORITY_IDLE = 0;
const jint REQUEST_PRIORITY_LOWEST = 1;
const jint REQUEST_PRIORITY_LOW = 2;
const jint REQUEST_PRIORITY_MEDIUM = 3;
const jint REQUEST_PRIORITY_HIGHEST = 4;

// Error codes.  Also declared in UrlRequest.java
const jint ERROR_SUCCESS = 0;
const jint ERROR_UNKNOWN = 1;
const jint ERROR_MALFORMED_URL = 2;
const jint ERROR_CONNECTION_TIMED_OUT = 3;
const jint ERROR_UNKNOWN_HOST = 4;

// TODO(mef): Replace following definitions with generated jni/UrlRequest_jni.h

bool UrlRequestRegisterJni(JNIEnv* env);

JNIEXPORT void JNICALL
    Java_org_chromium_net_UrlRequest_nativeInit(JNIEnv* env,
                                                jobject object,
                                                jobject request_context,
                                                jstring url,
                                                jint priority);

JNIEXPORT void JNICALL
    Java_org_chromium_net_UrlRequest_nativeAddHeader(JNIEnv* env,
                                                     jobject object,
                                                     jstring name,
                                                     jstring value);

JNIEXPORT void JNICALL
    Java_org_chromium_net_UrlRequest_nativeSetPostData(JNIEnv* env,
                                                       jobject object,
                                                       jstring content_type,
                                                       jbyteArray content);

JNIEXPORT void JNICALL
    Java_org_chromium_net_UrlRequest_nativeBeginChunkedUpload(
        JNIEnv* env,
        jobject object,
        jstring content_type);

JNIEXPORT void JNICALL Java_org_chromium_net_UrlRequest_nativeAppendChunk(
    JNIEnv* env,
    jobject object,
    jobject chunk_byte_buffer,
    jint chunk_size,
    jboolean is_last_chunk);

JNIEXPORT void JNICALL
    Java_org_chromium_net_UrlRequest_nativeStart(JNIEnv* env, jobject object);

JNIEXPORT void JNICALL
    Java_org_chromium_net_UrlRequest_nativeCancel(JNIEnv* env, jobject object);

JNIEXPORT void JNICALL
    Java_org_chromium_net_UrlRequest_nativeRecycle(JNIEnv* env, jobject object);

JNIEXPORT jint JNICALL
    Java_org_chromium_net_UrlRequest_nativeGetErrorCode(JNIEnv* env,
                                                        jobject object);

JNIEXPORT jstring JNICALL
    Java_org_chromium_net_UrlRequest_nativeGetErrorString(JNIEnv* env,
                                                          jobject object);

JNIEXPORT jint JNICALL
    Java_org_chromium_net_UrlRequest_getHttpStatusCode(JNIEnv* env,
                                                       jobject object);

JNIEXPORT jstring JNICALL
    Java_org_chromium_net_UrlRequest_nativeGetContentType(JNIEnv* env,
                                                          jobject object);

JNIEXPORT jlong JNICALL
    Java_org_chromium_net_UrlRequest_nativeGetContentLength(JNIEnv* env,
                                                            jobject object);

}  // extern "C"

#endif  // NET_CRONET_ANDROID_URLREQUEST_H_
