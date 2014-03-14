// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CRONET_ANDROID_URLREQUESTCONTEXT_H_
#define NET_CRONET_ANDROID_URLREQUESTCONTEXT_H_

#include <jni.h>

#include "net/cronet/android/url_request_context_peer.h"

// Returns a JNIEnv attached to the current thread.
JNIEnv* GetEnv(JavaVM* vm);

extern "C" {

// TODO(mef): Replace following with generated jni/UrlRequestContext_jni.h

// Native method implementations of the org.chromium.netjni.UrlRequestContext

JNIEXPORT jstring JNICALL
    Java_org_chromium_net_UrlRequestContext_getVersion(JNIEnv* env,
                                                       jobject object);

JNIEXPORT void JNICALL
    Java_org_chromium_net_UrlRequestContext_nativeInitialize(JNIEnv* env,
                                                             jobject object,
                                                             jobject context,
                                                             jstring user_agent,
                                                             jint log_level);

JNIEXPORT void JNICALL
    Java_org_chromium_net_UrlRequestContext_nativeFinalize(JNIEnv* env,
                                                           jobject object);

URLRequestContextPeer* GetURLRequestContextPeer(JNIEnv* env,
                                                jobject request_context);

}  // extern "C"

#endif  // NET_CRONET_ANDROID_URLREQUESTCONTEXT_H_
