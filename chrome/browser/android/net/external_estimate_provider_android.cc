// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/net/external_estimate_provider_android.h"

#include <stdint.h>

#include "base/android/context_utils.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "jni/ExternalEstimateProviderAndroid_jni.h"

using base::android::JavaParamRef;

namespace chrome {
namespace android {

ExternalEstimateProviderAndroid::ExternalEstimateProviderAndroid()
    : task_runner_(nullptr),
      delegate_(nullptr),
      weak_factory_(this) {
  if (base::ThreadTaskRunnerHandle::IsSet())
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
  JNIEnv* env = base::android::AttachCurrentThread();
  j_external_estimate_provider_.Reset(
      Java_ExternalEstimateProviderAndroid_create(
          env, base::android::GetApplicationContext(),
          reinterpret_cast<intptr_t>(this)));
  DCHECK(!j_external_estimate_provider_.is_null());
  no_value_ = Java_ExternalEstimateProviderAndroid_getNoValue(env);
}

ExternalEstimateProviderAndroid::~ExternalEstimateProviderAndroid() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Java_ExternalEstimateProviderAndroid_destroy(
      base::android::AttachCurrentThread(), j_external_estimate_provider_);
}

bool ExternalEstimateProviderAndroid::GetRTT(base::TimeDelta* rtt) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t milliseconds =
      Java_ExternalEstimateProviderAndroid_getRTTMilliseconds(
          env, j_external_estimate_provider_);
  DCHECK_GE(milliseconds, no_value_);
  if (milliseconds == no_value_)
    return false;
  *rtt = base::TimeDelta::FromMilliseconds(milliseconds);
  return true;
}

bool ExternalEstimateProviderAndroid::GetDownstreamThroughputKbps(
    int32_t* downstream_throughput_kbps) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t kbps =
      Java_ExternalEstimateProviderAndroid_getDownstreamThroughputKbps(
          env, j_external_estimate_provider_);
  DCHECK_GE(kbps, no_value_);
  if (kbps == no_value_)
    return false;
  *downstream_throughput_kbps = kbps;
  return true;
}

bool ExternalEstimateProviderAndroid::GetUpstreamThroughputKbps(
    int32_t* upstream_throughput_kbps) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t kbps = Java_ExternalEstimateProviderAndroid_getUpstreamThroughputKbps(
      env, j_external_estimate_provider_);
  DCHECK_GE(kbps, no_value_);
  if (kbps == no_value_)
    return false;
  *upstream_throughput_kbps = kbps;
  return true;
}

bool ExternalEstimateProviderAndroid::GetTimeSinceLastUpdate(
    base::TimeDelta* time_since_last_update) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t seconds =
      Java_ExternalEstimateProviderAndroid_getTimeSinceLastUpdateSeconds(
          env, j_external_estimate_provider_);
  DCHECK_GE(seconds, no_value_);
  if (seconds == no_value_) {
    *time_since_last_update = base::TimeDelta::Max();
    return false;
  }
  *time_since_last_update = base::TimeDelta::FromMilliseconds(seconds);
  return true;
}

void ExternalEstimateProviderAndroid::SetUpdatedEstimateDelegate(
    net::ExternalEstimateProvider::UpdatedEstimateDelegate* delegate) {
  delegate_ = delegate;
}

void ExternalEstimateProviderAndroid::Update() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExternalEstimateProviderAndroid_requestUpdate(
      env, j_external_estimate_provider_);
}

void ExternalEstimateProviderAndroid::
    NotifyExternalEstimateProviderAndroidUpdate(
        JNIEnv* env,
        const JavaParamRef<jobject>& obj) {
  if (!task_runner_)
    return;

  // Note that creating a weak pointer is safe even on a background thread,
  // because this method is called from the same critical section as the Java
  // destroy() method (so this object can't be destroyed while we're in this
  // method), and the factory itself isn't bound to a thread, just the weak
  // pointers are (but they are bound to the thread they're *dereferenced* on,
  // which is the task runner thread). Once we are outside of the critical
  // section, the weak pointer will be invalidated if the object is destroyed.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &ExternalEstimateProviderAndroid::NotifyUpdatedEstimateAvailable,
          weak_factory_.GetWeakPtr()));
}

void ExternalEstimateProviderAndroid::NotifyUpdatedEstimateAvailable() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::TimeDelta rtt;
  GetRTT(&rtt);

  int32_t downstream_throughput_kbps = -1;
  GetDownstreamThroughputKbps(&downstream_throughput_kbps);

  int32_t upstream_throughput_kbps = -1;
  GetUpstreamThroughputKbps(&upstream_throughput_kbps);

  if (delegate_) {
    delegate_->OnUpdatedEstimateAvailable(rtt, downstream_throughput_kbps,
                                          upstream_throughput_kbps);
  }
}

bool RegisterExternalEstimateProviderAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome
