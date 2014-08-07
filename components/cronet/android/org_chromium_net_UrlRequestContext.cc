// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/org_chromium_net_UrlRequestContext.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/values.h"
#include "components/cronet/android/org_chromium_net_UrlRequest.h"
#include "components/cronet/android/url_request_context_peer.h"
#include "components/cronet/android/url_request_peer.h"
#include "components/cronet/url_request_context_config.h"
#include "jni/UrlRequestContext_jni.h"

namespace {

// Delegate of URLRequestContextPeer that delivers callbacks to the Java layer.
class JniURLRequestContextPeerDelegate
    : public cronet::URLRequestContextPeer::URLRequestContextPeerDelegate {
 public:
  JniURLRequestContextPeerDelegate(JNIEnv* env, jobject owner)
      : owner_(env->NewGlobalRef(owner)) {
  }

  virtual void OnContextInitialized(
      cronet::URLRequestContextPeer* context) OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_UrlRequestContext_initNetworkThread(env, owner_);
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

namespace cronet {

// Explicitly register static JNI functions.
bool UrlRequestContextRegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// Sets global user-agent to be used for all subsequent requests.
static jlong CreateRequestContextPeer(JNIEnv* env,
                                      jobject object,
                                      jobject context,
                                      jstring user_agent,
                                      jint log_level,
                                      jstring config) {
  std::string user_agent_string =
      base::android::ConvertJavaStringToUTF8(env, user_agent);

  std::string config_string =
      base::android::ConvertJavaStringToUTF8(env, config);

  scoped_ptr<base::Value> config_value(base::JSONReader::Read(config_string));
  if (!config_value || !config_value->IsType(base::Value::TYPE_DICTIONARY)) {
    DLOG(ERROR) << "Bad JSON: " << config_string;
    return 0;
  }

  scoped_ptr<URLRequestContextConfig> context_config(
      new URLRequestContextConfig());
  base::JSONValueConverter<URLRequestContextConfig> converter;
  if (!converter.Convert(*config_value, context_config.get())) {
    DLOG(ERROR) << "Bad Config: " << config_value;
    return 0;
  }

  // Set application context.
  base::android::ScopedJavaLocalRef<jobject> scoped_context(env, context);
  base::android::InitApplicationContext(env, scoped_context);

  // TODO(mef): MinLogLevel is global, shared by all URLRequestContexts.
  // Revisit this if each URLRequestContext would need an individual log level.
  logging::SetMinLogLevel(static_cast<int>(log_level));

  // TODO(dplotnikov): set application context.
  URLRequestContextPeer* peer = new URLRequestContextPeer(
      new JniURLRequestContextPeerDelegate(env, object), user_agent_string);
  peer->AddRef();  // Hold onto this ref-counted object.
  peer->Initialize(context_config.Pass());
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

}  // namespace cronet
