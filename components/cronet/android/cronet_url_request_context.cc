// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_url_request_context.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/values.h"
#include "components/cronet/android/cronet_url_request.h"
#include "components/cronet/android/cronet_url_request_adapter.h"
#include "components/cronet/android/cronet_url_request_context_adapter.h"
#include "components/cronet/url_request_context_config.h"
#include "jni/CronetUrlRequestContext_jni.h"

namespace cronet {

// Init network thread on Java side.
void initJavaNetworkThread(
    const base::android::ScopedJavaGlobalRef<jobject>& jowner) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CronetUrlRequestContext_initNetworkThread(env, jowner.obj());
}

// Explicitly register static JNI functions.
bool CronetUrlRequestContextRegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// Sets global user-agent to be used for all subsequent requests.
static jlong CreateRequestContextAdapter(JNIEnv* env,
                                         jobject jcaller,
                                         jobject japp_context,
                                         jstring jconfig) {
  std::string config_string =
      base::android::ConvertJavaStringToUTF8(env, jconfig);
  scoped_ptr<URLRequestContextConfig> context_config(
      new URLRequestContextConfig());
  if (!context_config->LoadFromJSON(config_string))
    return 0;

  // Set application context.
  base::android::ScopedJavaLocalRef<jobject> scoped_context(env, japp_context);
  base::android::InitApplicationContext(env, scoped_context);

  base::android::ScopedJavaGlobalRef<jobject> jcaller_ref;
  jcaller_ref.Reset(env, jcaller);

  CronetURLRequestContextAdapter* context_adapter =
      new CronetURLRequestContextAdapter();
  base::Closure init_java_network_thread = base::Bind(&initJavaNetworkThread,
                                                      jcaller_ref);
  context_adapter->Initialize(context_config.Pass(), init_java_network_thread);

  return reinterpret_cast<jlong>(context_adapter);
}

// Destroys native objects.
static void DestroyRequestContextAdapter(JNIEnv* env,
                                         jobject jcaller,
                                         jlong jurl_request_context_adapter) {
  DCHECK(jurl_request_context_adapter);
  CronetURLRequestContextAdapter* context_adapter =
      reinterpret_cast<CronetURLRequestContextAdapter*>(
          jurl_request_context_adapter);
  context_adapter->Destroy();
}

// Starts recording NetLog into file with |fileName|.
static void StartNetLogToFile(JNIEnv* env,
                              jobject jcaller,
                              jlong jurl_request_context_adapter,
                              jstring jfile_name) {
  if (jurl_request_context_adapter == 0)
    return;
  CronetURLRequestContextAdapter* context_adapter =
      reinterpret_cast<CronetURLRequestContextAdapter*>(
          jurl_request_context_adapter);
  std::string file_name =
      base::android::ConvertJavaStringToUTF8(env, jfile_name);
  context_adapter->StartNetLogToFile(file_name);
}

// Stops recording NetLog.
static void StopNetLog(JNIEnv* env,
                       jobject jcaller,
                       jlong jurl_request_context_adapter) {
  if (jurl_request_context_adapter == 0)
    return;
  CronetURLRequestContextAdapter* context_adapter =
      reinterpret_cast<CronetURLRequestContextAdapter*>(
          jurl_request_context_adapter);
  context_adapter->StopNetLog();
}

static jint SetMinLogLevel(JNIEnv* env, jobject jcaller, jint jlog_level) {
  jint old_log_level = static_cast<jint>(logging::GetMinLogLevel());
  // MinLogLevel is global, shared by all URLRequestContexts.
  logging::SetMinLogLevel(static_cast<int>(jlog_level));
  return old_log_level;
}

}  // namespace cronet
