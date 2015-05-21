// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_POLICY_POLICY_MANAGER_H_
#define CHROME_BROWSER_ANDROID_POLICY_POLICY_MANAGER_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_provider_android_delegate.h"

namespace policy {

class PolicyProviderAndroid;
class Schema;

}  // namespace policy

class PolicyManager : public policy::PolicyProviderAndroidDelegate {
 public:
  PolicyManager(JNIEnv* env, jobject obj);

  // Called from Java:
  void SetPolicyBoolean(JNIEnv* env,
                        jobject obj,
                        jstring policyKey,
                        jboolean value);
  void SetPolicyInteger(JNIEnv* env,
                        jobject obj,
                        jstring policyKey,
                        jint value);
  void SetPolicyString(JNIEnv* env,
                       jobject obj,
                       jstring policyKey,
                       jstring value);
  void SetPolicyStringArray(JNIEnv* env,
                            jobject obj,
                            jstring policyKey,
                            jobjectArray value);
  void FlushPolicies(JNIEnv* env, jobject obj);

  void Destroy(JNIEnv* env, jobject obj);

  // Converts the passed in value to the type desired by the schema. If the
  // value is not convertible, it is returned unchanged, so the policy system
  // can report the error.
  // Note that this method will only look at the type of the schema, not at any
  // additional restrictions, or the schema for value's items or properties in
  // the case of a list or dictionary value.
  // This method takes ownership of the passed in value, and the caller takes
  // ownership of the return value.
  // Public for testing.
  static base::Value* ConvertValueToSchema(base::Value* raw_value,
                                           const policy::Schema& schema);

  // PolicyProviderAndroidDelegate:
  void RefreshPolicies() override;
  void PolicyProviderShutdown() override;

 private:
  ~PolicyManager() override;

  JavaObjectWeakGlobalRef weak_java_policy_manager_;

  scoped_ptr<policy::PolicyBundle> policy_bundle_;
  policy::PolicyProviderAndroid* policy_provider_;

  void SetPolicyValue(const std::string& key, base::Value* value);

  DISALLOW_COPY_AND_ASSIGN(PolicyManager);
};

// Register native methods
bool RegisterPolicyManager(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_POLICY_POLICY_MANAGER_H_
