// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/chromium_url_request_context.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/values.h"
#include "components/cronet/android/chromium_url_request.h"
#include "components/cronet/android/url_request_adapter.h"
#include "components/cronet/android/url_request_context_adapter.h"
#include "components/cronet/url_request_context_config.h"
#include "jni/ChromiumUrlRequestContext_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;

namespace {

// Delegate of URLRequestContextAdapter that delivers callbacks to the Java
// layer.
class JniURLRequestContextAdapterDelegate
    : public cronet::URLRequestContextAdapter::
          URLRequestContextAdapterDelegate {
 public:
  JniURLRequestContextAdapterDelegate(JNIEnv* env, jobject owner)
      : owner_(env->NewGlobalRef(owner)) {}

  void OnContextInitialized(
      cronet::URLRequestContextAdapter* context_adapter) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    cronet::Java_ChromiumUrlRequestContext_initNetworkThread(env, owner_);
    // TODO(dplotnikov): figure out if we need to detach from the thread.
    // The documentation says we should detach just before the thread exits.
  }

 protected:
  ~JniURLRequestContextAdapterDelegate() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    env->DeleteGlobalRef(owner_);
  }

 private:
  jobject owner_;
};

}  // namespace

namespace cronet {

// Explicitly register static JNI functions.
bool ChromiumUrlRequestContextRegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// Sets global user-agent to be used for all subsequent requests.
static jlong CreateRequestContextAdapter(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& juser_agent,
    jint jlog_level,
    const JavaParamRef<jstring>& jconfig) {
  std::string user_agent = ConvertJavaStringToUTF8(env, juser_agent);

  std::string config = ConvertJavaStringToUTF8(env, jconfig);

  scoped_ptr<base::Value> config_value = base::JSONReader::Read(config);
  if (!config_value || !config_value->IsType(base::Value::TYPE_DICTIONARY)) {
    DLOG(ERROR) << "Bad JSON: " << config;
    return 0;
  }

  scoped_ptr<URLRequestContextConfig> context_config(
      new URLRequestContextConfig());
  base::JSONValueConverter<URLRequestContextConfig> converter;
  if (!converter.Convert(*config_value, context_config.get())) {
    DLOG(ERROR) << "Bad Config: " << config_value;
    return 0;
  }

  // TODO(mef): MinLogLevel is global, shared by all URLRequestContexts.
  // Revisit this if each URLRequestContext would need an individual log level.
  logging::SetMinLogLevel(static_cast<int>(jlog_level));

  // TODO(dplotnikov): set application context.
  URLRequestContextAdapter* context_adapter = new URLRequestContextAdapter(
      new JniURLRequestContextAdapterDelegate(env, jcaller), user_agent);
  context_adapter->AddRef();  // Hold onto this ref-counted object.
  context_adapter->Initialize(context_config.Pass());
  return reinterpret_cast<jlong>(context_adapter);
}

// Releases native objects.
static void ReleaseRequestContextAdapter(JNIEnv* env,
                                         const JavaParamRef<jobject>& jcaller,
                                         jlong jurl_request_context_adapter) {
  URLRequestContextAdapter* context_adapter =
      reinterpret_cast<URLRequestContextAdapter*>(jurl_request_context_adapter);
  // TODO(mef): Revisit this from thread safety point of view: Can we delete a
  // thread while running on that thread?
  // URLRequestContextAdapter is a ref-counted object, and may have pending
  // tasks,
  // so we need to release it instead of deleting here.
  context_adapter->Release();
}

// Starts recording statistics.
static void InitializeStatistics(JNIEnv* env,
                                 const JavaParamRef<jobject>& jcaller) {
  base::StatisticsRecorder::Initialize();
}

// Gets current statistics with |jfilter| as a substring as JSON text (an empty
// |jfilter| will include all registered histograms).
static ScopedJavaLocalRef<jstring> GetStatisticsJSON(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& jfilter) {
  std::string query = ConvertJavaStringToUTF8(env, jfilter);
  std::string json = base::StatisticsRecorder::ToJSON(query);
  return ConvertUTF8ToJavaString(env, json);
}

// Starts recording NetLog into file with |jfilename|.
static void StartNetLogToFile(JNIEnv* env,
                              const JavaParamRef<jobject>& jcaller,
                              jlong jurl_request_context_adapter,
                              const JavaParamRef<jstring>& jfilename,
                              jboolean jlog_all) {
  URLRequestContextAdapter* context_adapter =
      reinterpret_cast<URLRequestContextAdapter*>(jurl_request_context_adapter);
  std::string filename = ConvertJavaStringToUTF8(env, jfilename);
  context_adapter->StartNetLogToFile(filename, jlog_all);
}

// Stops recording NetLog.
static void StopNetLog(JNIEnv* env,
                       const JavaParamRef<jobject>& jcaller,
                       jlong jurl_request_context_adapter) {
  URLRequestContextAdapter* context_adapter =
      reinterpret_cast<URLRequestContextAdapter*>(jurl_request_context_adapter);
  context_adapter->StopNetLog();
}

// Called on application's main Java thread.
static void InitRequestContextOnMainThread(JNIEnv* env,
                                           const JavaParamRef<jobject>& jcaller,
                                           jlong jurl_request_context_adapter) {
  URLRequestContextAdapter* context_adapter =
      reinterpret_cast<URLRequestContextAdapter*>(jurl_request_context_adapter);
  context_adapter->InitRequestContextOnMainThread();
}

}  // namespace cronet
