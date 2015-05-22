// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/policy/policy_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_provider_android.h"
#include "jni/PolicyManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;

PolicyManager::PolicyManager(JNIEnv* env, jobject obj)
    : weak_java_policy_manager_(env, obj),
      policy_bundle_(new policy::PolicyBundle) {
  policy_provider_ = static_cast<policy::PolicyProviderAndroid*>(
      g_browser_process->browser_policy_connector()->GetPlatformProvider());
  policy_provider_->SetDelegate(this);
}

PolicyManager::~PolicyManager() {}

void PolicyManager::RefreshPolicies() {
  JNIEnv* env = AttachCurrentThread();
  Java_PolicyManager_refreshPolicies(
      env, weak_java_policy_manager_.get(env).obj());
}

void PolicyManager::PolicyProviderShutdown() {
  policy_provider_ = NULL;
}

void PolicyManager::SetPolicyBoolean(JNIEnv* env,
                                     jobject obj,
                                     jstring policyKey,
                                     jboolean value) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 new base::FundamentalValue(static_cast<bool>(value)));
}

void PolicyManager::SetPolicyInteger(JNIEnv* env,
                                     jobject obj,
                                     jstring policyKey,
                                     jint value) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 new base::FundamentalValue(static_cast<int>(value)));
}

void PolicyManager::SetPolicyString(JNIEnv* env,
                                    jobject obj,
                                    jstring policyKey,
                                    jstring value) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 new base::StringValue(ConvertJavaStringToUTF8(env, value)));
}

void PolicyManager::SetPolicyStringArray(JNIEnv* env,
                                         jobject obj,
                                         jstring policyKey,
                                         jobjectArray array) {
  std::vector<base::string16> string_vector;
  base::android::AppendJavaStringArrayToStringVector(env, array,
                                                     &string_vector);
  base::ListValue* list_values = new base::ListValue();
  list_values->AppendStrings(string_vector);
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey), list_values);
}

void PolicyManager::FlushPolicies(JNIEnv* env, jobject obj) {
  if (!policy_provider_)
    return;

  policy_provider_->SetPolicies(policy_bundle_.Pass());
  policy_bundle_.reset(new policy::PolicyBundle);
}

void PolicyManager::Destroy(JNIEnv* env, jobject obj) {
  if (policy_provider_)
    policy_provider_->SetDelegate(NULL);

  delete this;
}

// static
base::Value* PolicyManager::ConvertValueToSchema(base::Value* raw_value,
                                                 const policy::Schema& schema) {
  scoped_ptr<base::Value> value(raw_value);

  if (!schema.valid())
    return value.release();

  switch (schema.type()) {
    case base::Value::TYPE_NULL:
      return base::Value::CreateNullValue().release();

    case base::Value::TYPE_BOOLEAN: {
      std::string string_value;
      if (value->GetAsString(&string_value)) {
        if (string_value.compare("true") == 0)
          return new base::FundamentalValue(true);

        if (string_value.compare("false") == 0)
          return new base::FundamentalValue(false);

        return value.release();
      }
      int int_value = 0;
      if (value->GetAsInteger(&int_value))
        return new base::FundamentalValue(int_value != 0);

      return value.release();
    }

    case base::Value::TYPE_INTEGER: {
      std::string string_value;
      if (value->GetAsString(&string_value)) {
        int int_value = 0;
        if (base::StringToInt(string_value, &int_value))
          return new base::FundamentalValue(int_value);
      }
      return value.release();
    }

    case base::Value::TYPE_DOUBLE: {
      std::string string_value;
      if (value->GetAsString(&string_value)) {
        double double_value = 0;
        if (base::StringToDouble(string_value, &double_value))
          return new base::FundamentalValue(double_value);
      }
      return value.release();
    }

    // String can't be converted from other types.
    case base::Value::TYPE_STRING: {
      return value.release();
    }

    // Binary is not a valid schema type.
    case base::Value::TYPE_BINARY: {
      NOTREACHED();
      return NULL;
    }

    // Complex types have to be deserialized from JSON.
    case base::Value::TYPE_DICTIONARY:
    case base::Value::TYPE_LIST: {
      std::string string_value;
      if (value->GetAsString(&string_value)) {
        scoped_ptr<base::Value> decoded_value =
            base::JSONReader::Read(string_value);
        if (decoded_value)
          return decoded_value.release();
      }
      return value.release();
    }
  }

  NOTREACHED();
  return NULL;
}

void PolicyManager::SetPolicyValue(const std::string& key, base::Value* value) {
  if (!policy_provider_)
    return;

  policy::Schema schema =
      policy_provider_->GetChromeSchema()->GetKnownProperty(key);

  policy::PolicyNamespace ns(policy::POLICY_DOMAIN_CHROME, std::string());
  policy_bundle_->Get(ns).Set(key,
                              policy::POLICY_LEVEL_MANDATORY,
                              policy::POLICY_SCOPE_MACHINE,
                              ConvertValueToSchema(value, schema),
                              NULL);
}

static jlong Init(JNIEnv* env, jobject obj) {
  return reinterpret_cast<intptr_t>(new PolicyManager(env, obj));
}

bool RegisterPolicyManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
