// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/physical_web/physical_web_data_source_android.h"

#include <jni.h>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "jni/UrlManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

PhysicalWebDataSourceAndroid::PhysicalWebDataSourceAndroid() {
  Initialize();
}

PhysicalWebDataSourceAndroid::~PhysicalWebDataSourceAndroid() {}

void PhysicalWebDataSourceAndroid::Initialize() {
  JNIEnv* env = AttachCurrentThread();

  // Cache a reference to the singleton instance of UrlManager.
  url_manager_.Reset(Java_UrlManager_getInstance(env));
  DCHECK(url_manager_.obj());
}

void PhysicalWebDataSourceAndroid::StartDiscovery(
    bool network_request_enabled) {
  // On Android, scanning is started and stopped through the Java layer.
  NOTREACHED();
}

void PhysicalWebDataSourceAndroid::StopDiscovery() {
  // On Android, scanning is started and stopped through the Java layer.
  NOTREACHED();
}

std::unique_ptr<base::ListValue> PhysicalWebDataSourceAndroid::GetMetadata() {
  // TODO(mattreynolds): get the metadata from the Java layer
  return base::MakeUnique<base::ListValue>();
}

bool PhysicalWebDataSourceAndroid::HasUnresolvedDiscoveries() {
  NOTIMPLEMENTED();
  return false;
}

// static
bool PhysicalWebDataSourceAndroid::RegisterPhysicalWebDataSource(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  PhysicalWebDataSource* data_source =
      g_browser_process->GetPhysicalWebDataSource();
  return reinterpret_cast<intptr_t>(data_source);
}
