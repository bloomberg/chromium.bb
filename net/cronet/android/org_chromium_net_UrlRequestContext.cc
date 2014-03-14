// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cronet/android/org_chromium_net_UrlRequestContext.h"

#include <stdio.h>

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "net/android/net_jni_registrar.h"
#include "net/cronet/android/org_chromium_net_UrlRequest.h"
#include "net/cronet/android/url_request_context_peer.h"
#include "net/cronet/android/url_request_peer.h"

// Version of this build of Chromium NET.
#define CHROMIUM_NET_VERSION "1"

namespace {

const char kVersion[] = CHROMIUM_VERSION "/" CHROMIUM_NET_VERSION;
const char kClassName[] = "org/chromium/net/UrlRequestContext";

jclass g_class;
jmethodID g_method_initNetworkThread;
jfieldID g_field_mRequestContext;

base::AtExitManager* g_at_exit_manager = NULL;

// Stores a reference to the peer in a java field.
void SetNativeObject(JNIEnv* env, jobject object, URLRequestContextPeer* peer) {
  env->SetLongField(
      object, g_field_mRequestContext, reinterpret_cast<jlong>(peer));
}

// Returns a reference to the peer, which is stored in a field of  the java
// object.
URLRequestContextPeer* GetNativeObject(JNIEnv* env, jobject object) {
  return reinterpret_cast<URLRequestContextPeer*>(
      env->GetLongField(object, g_field_mRequestContext));
}

// Delegate of URLRequestContextPeer that delivers callbacks to the Java layer.
class JniURLRequestContextPeerDelegate
    : public URLRequestContextPeer::URLRequestContextPeerDelegate {
 public:
  JniURLRequestContextPeerDelegate(JNIEnv* env, jobject owner)
      : owner_(env->NewGlobalRef(owner)) {
    env->GetJavaVM(&vm_);
  }

  virtual void OnContextInitialized(URLRequestContextPeer* context) OVERRIDE {
    JNIEnv* env = GetEnv(vm_);
    env->CallVoidMethod(owner_, g_method_initNetworkThread);
    if (env->ExceptionOccurred()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }

    // TODO(dplotnikov): figure out if we need to detach from the thread.
    // The documentation says we should detach just before the thread exits.
  }

 protected:
  virtual ~JniURLRequestContextPeerDelegate() {
    GetEnv(vm_)->DeleteGlobalRef(owner_);
  }

 private:
  jobject owner_;
  JavaVM* vm_;
};

}  // namespace

// Checks the available version of JNI. Also, caches Java reflection artifacts.
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }

  g_class = (jclass)env->NewGlobalRef(env->FindClass(kClassName));
  g_method_initNetworkThread =
      env->GetMethodID(g_class, "initNetworkThread", "()V");
  g_field_mRequestContext = env->GetFieldID(g_class, "mRequestContext", "J");
  if (!g_class || !g_method_initNetworkThread || !g_field_mRequestContext) {
    return -1;
  }

  base::android::InitVM(vm);

  if (!base::android::RegisterJni(env)) {
    return -1;
  }

  if (!UrlRequestRegisterJni(env)) {
    return -1;
  }

  if (!net::android::RegisterJni(env)) {
    return -1;
  }

  g_at_exit_manager = new base::AtExitManager();

  base::i18n::InitializeICU();

  return JNI_VERSION_1_6;
}

JNIEnv* GetEnv(JavaVM* vm) {
  // We need to make sure this native thread is attached to the JVM before
  // we can call any Java methods.
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) ==
      JNI_EDETACHED) {
    vm->AttachCurrentThread(&env, NULL);
  }
  return env;
}

URLRequestContextPeer* GetURLRequestContextPeer(JNIEnv* env,
                                                jobject request_context) {
  return GetNativeObject(env, request_context);
}

JNIEXPORT jstring JNICALL
Java_org_chromium_net_UrlRequestContext_getVersion(JNIEnv* env,
                                                   jobject object) {
  return env->NewStringUTF(kVersion);
}

// Sets global user-agent to be used for all subsequent requests.
JNIEXPORT void JNICALL
Java_org_chromium_net_UrlRequestContext_nativeInitialize(JNIEnv* env,
                                                         jobject object,
                                                         jobject context,
                                                         jstring user_agent,
                                                         jint log_level) {
  const char* user_agent_utf8 = env->GetStringUTFChars(user_agent, NULL);
  std::string user_agent_string(user_agent_utf8);
  env->ReleaseStringUTFChars(user_agent, user_agent_utf8);

  // Set application context.
  base::android::ScopedJavaLocalRef<jobject> scoped_context(env, context);
  base::android::InitApplicationContext(env, scoped_context);

  int logging_level = log_level;

  // TODO(dplotnikov): set application context.
  URLRequestContextPeer* peer = new URLRequestContextPeer(
      new JniURLRequestContextPeerDelegate(env, object),
      user_agent_string,
      logging_level,
      kVersion);
  peer->AddRef();  // Hold onto this ref-counted object.

  SetNativeObject(env, object, peer);

  peer->Initialize();
}

// Releases native objects.
JNIEXPORT void JNICALL
Java_org_chromium_net_UrlRequestContext_nativeFinalize(JNIEnv* env,
                                                       jobject object) {
  // URLRequestContextPeer is a ref-counted object, so we need to release it
  // instead of deleting outright.
  GetNativeObject(env, object)->Release();
  SetNativeObject(env, object, NULL);
}
