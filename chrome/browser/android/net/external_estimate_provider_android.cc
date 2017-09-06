// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/net/external_estimate_provider_android.h"

#include <stdint.h>

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
  DCHECK(j_external_estimate_provider_.is_null());
  DCHECK_GE(-1, no_value_);

  if (!base::ThreadTaskRunnerHandle::IsSet())
    return;

  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ExternalEstimateProviderAndroid::CreateJavaObject,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(10));
}

ExternalEstimateProviderAndroid::~ExternalEstimateProviderAndroid() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!j_external_estimate_provider_.is_null()) {
    Java_ExternalEstimateProviderAndroid_destroy(
        base::android::AttachCurrentThread(), j_external_estimate_provider_);
  }
}

void ExternalEstimateProviderAndroid::CreateJavaObject() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(j_external_estimate_provider_.is_null());

  JNIEnv* env = base::android::AttachCurrentThread();
  j_external_estimate_provider_.Reset(
      Java_ExternalEstimateProviderAndroid_create(
          env, reinterpret_cast<intptr_t>(this)));
  DCHECK(!j_external_estimate_provider_.is_null());
  DCHECK_EQ(no_value_, Java_ExternalEstimateProviderAndroid_getNoValue(env));
  Update();
}

base::TimeDelta ExternalEstimateProviderAndroid::GetRTT() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!j_external_estimate_provider_.is_null());

  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t milliseconds =
      Java_ExternalEstimateProviderAndroid_getRTTMilliseconds(
          env, j_external_estimate_provider_);
  DCHECK_GE(milliseconds, no_value_);
  return base::TimeDelta::FromMilliseconds(milliseconds);
}

int32_t ExternalEstimateProviderAndroid::GetDownstreamThroughputKbps() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!j_external_estimate_provider_.is_null());

  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t kbps =
      Java_ExternalEstimateProviderAndroid_getDownstreamThroughputKbps(
          env, j_external_estimate_provider_);
  DCHECK_GE(kbps, no_value_);
  return kbps;
}

void ExternalEstimateProviderAndroid::SetUpdatedEstimateDelegate(
    net::ExternalEstimateProvider::UpdatedEstimateDelegate* delegate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_ = delegate;
}

void ExternalEstimateProviderAndroid::Update() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (j_external_estimate_provider_.is_null())
    return;
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

  if (delegate_) {
    delegate_->OnUpdatedEstimateAvailable(GetRTT(),
                                          GetDownstreamThroughputKbps());
  }
}

}  // namespace android
}  // namespace chrome
