// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/net/external_estimate_provider_android.h"

#include <stdint.h>

#include "jni/ExternalEstimateProviderAndroid_jni.h"

namespace chrome {
namespace android {

ExternalEstimateProviderAndroid::ExternalEstimateProviderAndroid()
    : delegate_(nullptr) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(j_external_estimate_provider_.is_null());
  j_external_estimate_provider_.Reset(
      Java_ExternalEstimateProviderAndroid_create(
          env, base::android::GetApplicationContext()));
  DCHECK(!j_external_estimate_provider_.is_null());
  no_value_ = Java_ExternalEstimateProviderAndroid_getNoValue(env);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

ExternalEstimateProviderAndroid::~ExternalEstimateProviderAndroid() {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void ExternalEstimateProviderAndroid::RequestUpdate() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExternalEstimateProviderAndroid_requestUpdate(
      env, j_external_estimate_provider_.obj());
}

bool ExternalEstimateProviderAndroid::GetRTT(base::TimeDelta* rtt) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t milliseconds =
      Java_ExternalEstimateProviderAndroid_getRTTMilliseconds(
          env, j_external_estimate_provider_.obj());
  DCHECK(milliseconds >= no_value_);
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
          env, j_external_estimate_provider_.obj());
  DCHECK(kbps >= no_value_);
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
      env, j_external_estimate_provider_.obj());
  DCHECK(kbps >= no_value_);
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
          env, j_external_estimate_provider_.obj());
  DCHECK(seconds >= no_value_);
  if (seconds == no_value_) {
    *time_since_last_update = base::TimeDelta::Max();
    return false;
  }
  *time_since_last_update = base::TimeDelta::FromMilliseconds(seconds);
  return true;
}

void ExternalEstimateProviderAndroid::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  RequestUpdate();
}

bool RegisterExternalEstimateProviderAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ExternalEstimateProviderAndroid::SetUpdatedEstimateDelegate(
    net::ExternalEstimateProvider::UpdatedEstimateDelegate* delegate) {
  delegate_ = delegate;
}

}  // namespace android
}  // namespace chrome
