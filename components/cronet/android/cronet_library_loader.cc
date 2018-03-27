// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_library_loader.h"

#include <jni.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/base_jni_onload.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/task_scheduler.h"
#include "components/cronet/android/cronet_jni_registration.h"
#include "components/cronet/cronet_global_state.h"
#include "components/cronet/version.h"
#include "jni/CronetLibraryLoader_jni.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "net/proxy_resolution/proxy_config_service_android.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "url/url_features.h"
#include "url/url_util.h"

#if !BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
#include "base/i18n/icu_util.h"  // nogncheck
#endif

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace cronet {
namespace {

// MessageLoop on the init thread, which is where objects that receive Java
// notifications generally live.
base::MessageLoop* g_init_message_loop = nullptr;

net::NetworkChangeNotifier* g_network_change_notifier = nullptr;

base::WaitableEvent g_init_thread_init_done(
    base::WaitableEvent::ResetPolicy::MANUAL,
    base::WaitableEvent::InitialState::NOT_SIGNALED);

void NativeInit() {
  if (!base::TaskScheduler::GetInstance())
    base::TaskScheduler::CreateAndStartWithDefaultParams("Cronet");
  url::Initialize();
}

}  // namespace

bool OnInitThread() {
  DCHECK(g_init_message_loop);
  return g_init_message_loop == base::MessageLoop::current();
}

// Checks the available version of JNI. Also, caches Java reflection artifacts.
jint CronetOnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!RegisterMainDexNatives(env) || !RegisterNonMainDexNatives(env)) {
    return -1;
  }
  if (!base::android::OnJNIOnLoadInit())
    return -1;
  NativeInit();
  return JNI_VERSION_1_6;
}

void CronetOnUnLoad(JavaVM* jvm, void* reserved) {
  if (base::TaskScheduler::GetInstance())
    base::TaskScheduler::GetInstance()->Shutdown();

  base::android::LibraryLoaderExitHook();
}

void JNI_CronetLibraryLoader_CronetInitOnInitThread(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
#if !BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
  base::i18n::InitializeICU();
#endif

  base::FeatureList::InitializeInstance(std::string(), std::string());
  DCHECK(!base::MessageLoop::current());
  DCHECK(!g_init_message_loop);
  g_init_message_loop =
      new base::MessageLoop(base::MessageLoop::Type::TYPE_JAVA);
  static_cast<base::MessageLoopForUI*>(g_init_message_loop)->Start();
  DCHECK(!g_network_change_notifier);

  if (!net::NetworkChangeNotifier::GetFactory()) {
    net::NetworkChangeNotifier::SetFactory(
        new net::NetworkChangeNotifierFactoryAndroid());
  }
  g_network_change_notifier = net::NetworkChangeNotifier::Create();
  g_init_thread_init_done.Signal();
}

ScopedJavaLocalRef<jstring> JNI_CronetLibraryLoader_GetCronetVersion(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
  return base::android::ConvertUTF8ToJavaString(env, CRONET_VERSION);
}

void PostTaskToInitThread(const base::Location& posted_from,
                          base::OnceClosure task) {
  g_init_thread_init_done.Wait();
  g_init_message_loop->task_runner()->PostTask(posted_from, std::move(task));
}

void EnsureInitialized() {
  if (g_init_message_loop) {
    // Ensure that init is done on the init thread.
    g_init_thread_init_done.Wait();
    return;
  }

  // The initialization can only be done once, so static |s_run_once| variable
  // is used to do it in the constructor.
  static class RunOnce {
   public:
    RunOnce() {
      NativeInit();
      JNIEnv* env = base::android::AttachCurrentThread();
      // Ensure initialized from Java side to properly create Init thread.
      cronet::Java_CronetLibraryLoader_ensureInitializedFromNative(env);
    }
  } s_run_once;
}

std::unique_ptr<net::ProxyConfigService> CreateProxyConfigService(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  std::unique_ptr<net::ProxyConfigService> service =
      net::ProxyResolutionService::CreateSystemProxyConfigService(io_task_runner);
  // If a PAC URL is present, ignore it and use the address and port of
  // Android system's local HTTP proxy server. See: crbug.com/432539.
  // TODO(csharrison) Architect the wrapper better so we don't need to cast for
  // android ProxyConfigServices.
  net::ProxyConfigServiceAndroid* android_proxy_config_service =
      static_cast<net::ProxyConfigServiceAndroid*>(service.get());
  android_proxy_config_service->set_exclude_pac_url(true);
  return service;
}

// Creates a proxy resolution service appropriate for this platform.
std::unique_ptr<net::ProxyResolutionService> CreateProxyResolutionService(
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    net::NetLog* net_log) {
  // Android provides a local HTTP proxy server that handles proxying when a PAC
  // URL is present. Create a proxy service without a resolver and rely on this
  // local HTTP proxy. See: crbug.com/432539.
  return net::ProxyResolutionService::CreateWithoutProxyResolver(
      std::move(proxy_config_service), net_log);
}

// Creates default User-Agent request value, combining optional
// |partial_user_agent| with system-dependent values.
std::string CreateDefaultUserAgent(const std::string& partial_user_agent) {
  // Cronet global state must be initialized to include application info
  // into default user agent
  cronet::EnsureInitialized();

  JNIEnv* env = base::android::AttachCurrentThread();
  std::string user_agent = base::android::ConvertJavaStringToUTF8(
      cronet::Java_CronetLibraryLoader_getDefaultUserAgent(env));
  if (!partial_user_agent.empty())
    user_agent.insert(user_agent.size() - 1, "; " + partial_user_agent);
  return user_agent;
}

}  // namespace cronet
