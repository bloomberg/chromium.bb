// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/policy/policy_manager.h"

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/policy/core/browser/android/policy_converter.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_provider_android.h"
#include "components/policy/core/common/schema.h"
#include "jni/PolicyManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

PolicyManager::PolicyManager(JNIEnv* env, jobject obj)
    : weak_java_policy_manager_(env, obj) {
  policy_provider_ = static_cast<policy::PolicyProviderAndroid*>(
      g_browser_process->browser_policy_connector()->GetPlatformProvider());
  policy_provider_->SetDelegate(this);
}

PolicyManager::~PolicyManager() {}

ScopedJavaLocalRef<jobject> PolicyManager::CreatePolicyConverter(JNIEnv* env,
                                                                 jobject obj) {
  policy_converter_.reset(new policy::android::PolicyConverter(
      policy_provider_->GetChromeSchema()));
  return ScopedJavaLocalRef<jobject>(policy_converter_->GetJavaObject());
}

void PolicyManager::RefreshPolicies() {
  JNIEnv* env = AttachCurrentThread();
  Java_PolicyManager_refreshPolicies(
      env, weak_java_policy_manager_.get(env).obj());
}

void PolicyManager::PolicyProviderShutdown() {
  policy_provider_ = nullptr;
}

void PolicyManager::FlushPolicies(JNIEnv* env, jobject obj) {
  if (!policy_provider_)
    return;

  policy_provider_->SetPolicies(policy_converter_->GetPolicyBundle());
}

void PolicyManager::Destroy(JNIEnv* env, jobject obj) {
  if (policy_provider_)
    policy_provider_->SetDelegate(nullptr);

  delete this;
}

static jlong Init(JNIEnv* env, jobject obj) {
  return reinterpret_cast<intptr_t>(new PolicyManager(env, obj));
}

bool RegisterPolicyManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
