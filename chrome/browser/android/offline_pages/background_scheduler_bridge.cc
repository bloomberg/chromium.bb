// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/offline_pages/background_scheduler_bridge.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/offline_pages/background/device_conditions.h"
#include "components/offline_pages/background/request_coordinator.h"
#include "jni/BackgroundSchedulerBridge_jni.h"

using base::android::ScopedJavaGlobalRef;

namespace offline_pages {
namespace android {

namespace {

// C++ callback that delegates to Java callback.
void ProcessingDoneCallback(
    const ScopedJavaGlobalRef<jobject>& j_callback_obj, bool result) {
  base::android::RunCallbackAndroid(j_callback_obj, result);
}

}  // namespace

// JNI call to start request processing.
static jboolean StartProcessing(JNIEnv* env,
                                const JavaParamRef<jclass>& jcaller,
                                const jboolean j_power_connected,
                                const jint j_battery_percentage,
                                const jint j_net_connection_type,
                                const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  // Lookup/create RequestCoordinator KeyedService and call StartProcessing on
  // it with bound j_callback_obj.
  Profile* profile = ProfileManager::GetLastUsedProfile();
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetInstance()->
      GetForBrowserContext(profile);
  DVLOG(2) << "resource_coordinator: " << coordinator;
  DeviceConditions device_conditions(
      j_power_connected, j_battery_percentage,
      static_cast<net::NetworkChangeNotifier::ConnectionType>(
          j_net_connection_type));
  return coordinator->StartProcessing(
      device_conditions, base::Bind(&ProcessingDoneCallback, j_callback_ref));
}

void BackgroundSchedulerBridge::Schedule(
    const TriggerConditions& trigger_conditions) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_conditions =
      CreateTriggerConditions(env, trigger_conditions.require_power_connected,
                              trigger_conditions.minimum_battery_percentage,
                              trigger_conditions.require_unmetered_network);
  Java_BackgroundSchedulerBridge_schedule(env, j_conditions.obj());
}

void BackgroundSchedulerBridge::Unschedule() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BackgroundSchedulerBridge_unschedule(env);
}

ScopedJavaLocalRef<jobject> BackgroundSchedulerBridge::CreateTriggerConditions(
    JNIEnv* env,
    bool require_power_connected,
    int minimum_battery_percentage,
    bool require_unmetered_network) const {
  return Java_BackgroundSchedulerBridge_createTriggerConditions(
      env, require_power_connected, minimum_battery_percentage,
      require_unmetered_network);
}

bool RegisterBackgroundSchedulerBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace offline_pages
