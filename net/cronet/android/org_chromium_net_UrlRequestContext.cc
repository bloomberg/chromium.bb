// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_string.h"
#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/metrics/statistics_recorder.h"
#include "jni/UrlRequestContext_jni.h"
#include "net/android/net_jni_registrar.h"
#include "net/cronet/android/org_chromium_net_UrlRequest.h"
#include "net/cronet/android/url_request_context_peer.h"
#include "net/cronet/android/url_request_peer.h"

// Version of this build of Chromium NET.
#define CHROMIUM_NET_VERSION "1"

namespace {

const char kVersion[] = CHROMIUM_VERSION "/" CHROMIUM_NET_VERSION;

const base::android::RegistrationMethod kCronetRegisteredMethods[] = {
  {"BaseAndroid", base::android::RegisterJni},
  {"NetAndroid", net::android::RegisterJni},
  {"UrlRequest", net::UrlRequestRegisterJni},
  {"UrlRequestContext", net::RegisterNativesImpl},
};

base::AtExitManager* g_at_exit_manager = NULL;

// Delegate of URLRequestContextPeer that delivers callbacks to the Java layer.
class JniURLRequestContextPeerDelegate
    : public URLRequestContextPeer::URLRequestContextPeerDelegate {
 public:
  JniURLRequestContextPeerDelegate(JNIEnv* env, jobject owner)
      : owner_(env->NewGlobalRef(owner)) {
  }

  virtual void OnContextInitialized(URLRequestContextPeer* context) OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    net::Java_UrlRequestContext_initNetworkThread(env, owner_);
    // TODO(dplotnikov): figure out if we need to detach from the thread.
    // The documentation says we should detach just before the thread exits.
  }

 protected:
  virtual ~JniURLRequestContextPeerDelegate() {
    JNIEnv* env = base::android::AttachCurrentThread();
    env->DeleteGlobalRef(owner_);
  }

 private:
  jobject owner_;
};

}  // namespace

// Checks the available version of JNI. Also, caches Java reflection artifacts.
extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }

  base::android::InitVM(vm);

  if (!base::android::RegisterNativeMethods(
          env, kCronetRegisteredMethods, arraysize(kCronetRegisteredMethods))) {
    return -1;
  }

  g_at_exit_manager = new base::AtExitManager();

  base::i18n::InitializeICU();

  return JNI_VERSION_1_6;
}

extern "C" void JNIEXPORT JNICALL JNI_OnUnLoad(JavaVM* jvm, void* reserved) {
  if (g_at_exit_manager) {
    delete g_at_exit_manager;
    g_at_exit_manager = NULL;
  }
}

namespace net {

static jstring GetVersion(JNIEnv* env, jclass unused) {
  return env->NewStringUTF(kVersion);
}

// Sets global user-agent to be used for all subsequent requests.
static jlong CreateRequestContextPeer(JNIEnv* env,
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
  peer->Initialize();
  return reinterpret_cast<jlong>(peer);
}

// Releases native objects.
static void ReleaseRequestContextPeer(JNIEnv* env,
                                      jobject object,
                                      jlong urlRequestContextPeer) {
  URLRequestContextPeer* peer =
      reinterpret_cast<URLRequestContextPeer*>(urlRequestContextPeer);
  // TODO(mef): Revisit this from thread safety point of view: Can we delete a
  // thread while running on that thread?
  // URLRequestContextPeer is a ref-counted object, and may have pending tasks,
  // so we need to release it instead of deleting here.
  peer->Release();
}

// Starts recording statistics.
static void InitializeStatistics(JNIEnv* env, jobject jcaller) {
  base::StatisticsRecorder::Initialize();
}

// Gets current statistics with |filter| as a substring as JSON text (an empty
// |filter| will include all registered histograms).
static jstring GetStatisticsJSON(JNIEnv* env, jobject jcaller, jstring filter) {
  std::string query = base::android::ConvertJavaStringToUTF8(env, filter);
  std::string json = base::StatisticsRecorder::ToJSON(query);
  return base::android::ConvertUTF8ToJavaString(env, json).Release();
}

// Starts recording NetLog into file with |fileName|.
static void StartNetLogToFile(JNIEnv* env,
                              jobject jcaller,
                              jlong urlRequestContextPeer,
                              jstring fileName) {
  URLRequestContextPeer* peer =
      reinterpret_cast<URLRequestContextPeer*>(urlRequestContextPeer);
  std::string file_name = base::android::ConvertJavaStringToUTF8(env, fileName);
  peer->StartNetLogToFile(file_name);
}

// Stops recording NetLog.
static void StopNetLog(JNIEnv* env,
                       jobject jcaller,
                       jlong urlRequestContextPeer) {
  URLRequestContextPeer* peer =
      reinterpret_cast<URLRequestContextPeer*>(urlRequestContextPeer);
  peer->StopNetLog();
}

}  // namespace net
