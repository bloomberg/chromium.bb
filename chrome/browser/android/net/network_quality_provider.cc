// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/net/network_quality_provider.h"

#include <stdint.h>

#include "jni/NetworkQualityProvider_jni.h"
#include "net/base/network_quality.h"

NetworkQualityProvider::NetworkQualityProvider() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(j_network_quality_provider_.is_null());
  j_network_quality_provider_.Reset(Java_NetworkQualityProvider_create(
      env, base::android::GetApplicationContext()));
  DCHECK(!j_network_quality_provider_.is_null());
  no_value_ = Java_NetworkQualityProvider_getNoValue(env);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

NetworkQualityProvider::~NetworkQualityProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

bool NetworkQualityProvider::IsEstimateAvailable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_NetworkQualityProvider_isEstimateAvailable(
      env, j_network_quality_provider_.obj());
}

void NetworkQualityProvider::RequestUpdate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkQualityProvider_requestUpdate(env,
                                            j_network_quality_provider_.obj());
}

bool NetworkQualityProvider::GetRTT(base::TimeDelta* rtt) {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t milliseconds = Java_NetworkQualityProvider_getRTTMilliseconds(
      env, j_network_quality_provider_.obj());
  DCHECK(milliseconds >= no_value_);
  if (milliseconds == no_value_) {
    *rtt = net::NetworkQuality::InvalidRTT();
    return false;
  }
  *rtt = base::TimeDelta::FromMilliseconds(milliseconds);
  return true;
}

bool NetworkQualityProvider::GetDownstreamThroughputKbps(
    int32_t* downstream_throughput_kbps) {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t kbps = Java_NetworkQualityProvider_getDownstreamThroughputKbps(
      env, j_network_quality_provider_.obj());
  DCHECK(kbps >= no_value_);
  if (kbps == no_value_) {
    *downstream_throughput_kbps = net::NetworkQuality::kInvalidThroughput;
    return false;
  }
  *downstream_throughput_kbps = kbps;
  return true;
}

bool NetworkQualityProvider::GetUpstreamThroughputKbps(
    int32_t* upstream_throughput_kbps) {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t kbps = Java_NetworkQualityProvider_getUpstreamThroughputKbps(
      env, j_network_quality_provider_.obj());
  DCHECK(kbps >= no_value_);
  if (kbps == no_value_) {
    *upstream_throughput_kbps = net::NetworkQuality::kInvalidThroughput;
    return false;
  }
  *upstream_throughput_kbps = kbps;
  return true;
}

bool NetworkQualityProvider::GetTimeSinceLastUpdate(
    base::TimeDelta* time_since_last_update) {
  DCHECK(thread_checker_.CalledOnValidThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t seconds = Java_NetworkQualityProvider_getTimeSinceLastUpdateSeconds(
      env, j_network_quality_provider_.obj());
  DCHECK(seconds >= no_value_);
  if (seconds == no_value_) {
    *time_since_last_update = base::TimeDelta::Max();
    return false;
  }
  *time_since_last_update = base::TimeDelta::FromMilliseconds(seconds);
  return true;
}

void NetworkQualityProvider::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  RequestUpdate();
}

bool RegisterNetworkQualityProvider(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
