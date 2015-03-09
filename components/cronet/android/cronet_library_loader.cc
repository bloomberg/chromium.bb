// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_library_loader.h"

#include <jni.h>

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_utils.h"
#include "base/at_exit.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "components/cronet/android/chromium_url_request.h"
#include "components/cronet/android/chromium_url_request_context.h"
#include "components/cronet/android/cronet_histogram_manager.h"
#include "components/cronet/android/cronet_upload_data_stream_delegate.h"
#include "components/cronet/android/cronet_url_request.h"
#include "components/cronet/android/cronet_url_request_context_adapter.h"
#include "jni/CronetLibraryLoader_jni.h"
#include "net/android/net_jni_registrar.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "url/android/url_jni_registrar.h"
#include "url/url_util.h"

#if !defined(USE_ICU_ALTERNATIVES_ON_ANDROID)
#include "base/i18n/icu_util.h"
#endif

namespace cronet {
namespace {

const base::android::RegistrationMethod kCronetRegisteredMethods[] = {
    {"BaseAndroid", base::android::RegisterJni},
    {"ChromiumUrlRequest", ChromiumUrlRequestRegisterJni},
    {"ChromiumUrlRequestContext", ChromiumUrlRequestContextRegisterJni},
    {"CronetHistogramManager", CronetHistogramManagerRegisterJni},
    {"CronetLibraryLoader", RegisterNativesImpl},
    {"CronetUploadDataStreamDelegate",
     CronetUploadDataStreamDelegateRegisterJni},
    {"CronetUrlRequest", CronetUrlRequestRegisterJni},
    {"CronetUrlRequestContextAdapter",
     CronetUrlRequestContextAdapterRegisterJni},
    {"NetAndroid", net::android::RegisterJni},
    {"UrlAndroid", url::android::RegisterJni},
};

base::AtExitManager* g_at_exit_manager = NULL;
// MessageLoop on the main thread, which is where objects that receive Java
// notifications generally live.
base::MessageLoop* g_main_message_loop = nullptr;

net::NetworkChangeNotifier* g_network_change_notifier = nullptr;

}  // namespace

// Checks the available version of JNI. Also, caches Java reflection artifacts.
jint CronetOnLoad(JavaVM* vm, void* reserved) {
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

  base::android::InitReplacementClassLoader(env,
                                            base::android::GetClassLoader(env));
  url::Initialize();

  return JNI_VERSION_1_6;
}

void CronetOnUnLoad(JavaVM* jvm, void* reserved) {
  if (g_at_exit_manager) {
    delete g_at_exit_manager;
    g_at_exit_manager = NULL;
  }
}

void CronetInitOnMainThread(JNIEnv* env, jclass jcaller, jobject japp_context) {
  // Set application context.
  base::android::ScopedJavaLocalRef<jobject> scoped_app_context(env,
                                                                japp_context);
  base::android::InitApplicationContext(env, scoped_app_context);

#if !defined(USE_ICU_ALTERNATIVES_ON_ANDROID)
  base::i18n::InitializeICU();
#endif

  DCHECK(!base::MessageLoop::current());
  DCHECK(!g_main_message_loop);
  g_main_message_loop = new base::MessageLoopForUI();
  base::MessageLoopForUI::current()->Start();
  DCHECK(!g_network_change_notifier);
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
  g_network_change_notifier = net::NetworkChangeNotifier::Create();
}

}  // namespace cronet
