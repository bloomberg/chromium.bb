// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_POLICY_POLICY_MANAGER_H_
#define CHROME_BROWSER_ANDROID_POLICY_POLICY_MANAGER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/policy/core/common/policy_provider_android_delegate.h"

namespace policy {

class PolicyProviderAndroid;

namespace android {

class PolicyConverter;

}  // namespace android
}  // namespace policy

class PolicyManager : public policy::PolicyProviderAndroidDelegate {
 public:
  PolicyManager(JNIEnv* env, jobject obj);

  void FlushPolicies(JNIEnv* env, jobject obj);

  void Destroy(JNIEnv* env, jobject obj);

  // Creates the native and java |PolicyConverter|, returns the reference to
  // the java one.
  base::android::ScopedJavaLocalRef<jobject> CreatePolicyConverter(JNIEnv* env,
                                                                   jobject obj);

  // PolicyProviderAndroidDelegate:
  void RefreshPolicies() override;
  void PolicyProviderShutdown() override;

 private:
  ~PolicyManager() override;

  JavaObjectWeakGlobalRef weak_java_policy_manager_;

  scoped_ptr<policy::android::PolicyConverter> policy_converter_;
  policy::PolicyProviderAndroid* policy_provider_;

  DISALLOW_COPY_AND_ASSIGN(PolicyManager);
};

// Register native methods
bool RegisterPolicyManager(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_POLICY_POLICY_MANAGER_H_
