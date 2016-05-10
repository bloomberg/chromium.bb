// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/android/policy_converter.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "jni/PolicyConverter_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaRef;

namespace policy {
namespace android {

PolicyConverter::PolicyConverter(const Schema* policy_schema)
    : policy_schema_(policy_schema), policy_bundle_(new PolicyBundle) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_PolicyConverter_create(
                           env, reinterpret_cast<long>(this)).obj());
  DCHECK(!java_obj_.is_null());
}

PolicyConverter::~PolicyConverter() {
  Java_PolicyConverter_onNativeDestroyed(base::android::AttachCurrentThread(),
                                         java_obj_.obj());
}

std::unique_ptr<PolicyBundle> PolicyConverter::GetPolicyBundle() {
  std::unique_ptr<PolicyBundle> filled_bundle(std::move(policy_bundle_));
  policy_bundle_.reset(new PolicyBundle);
  return filled_bundle;
}

base::android::ScopedJavaLocalRef<jobject> PolicyConverter::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

void PolicyConverter::SetPolicyBoolean(JNIEnv* env,
                                       const JavaRef<jobject>& obj,
                                       const JavaRef<jstring>& policyKey,
                                       jboolean value) {
  SetPolicyValue(
      ConvertJavaStringToUTF8(env, policyKey),
      base::WrapUnique(new base::FundamentalValue(static_cast<bool>(value))));
}

void PolicyConverter::SetPolicyInteger(JNIEnv* env,
                                       const JavaRef<jobject>& obj,
                                       const JavaRef<jstring>& policyKey,
                                       jint value) {
  SetPolicyValue(
      ConvertJavaStringToUTF8(env, policyKey),
      base::WrapUnique(new base::FundamentalValue(static_cast<int>(value))));
}

void PolicyConverter::SetPolicyString(JNIEnv* env,
                                      const JavaRef<jobject>& obj,
                                      const JavaRef<jstring>& policyKey,
                                      const JavaRef<jstring>& value) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 base::WrapUnique(new base::StringValue(
                     ConvertJavaStringToUTF8(env, value))));
}

void PolicyConverter::SetPolicyStringArray(JNIEnv* env,
                                           const JavaRef<jobject>& obj,
                                           const JavaRef<jstring>& policyKey,
                                           const JavaRef<jobjectArray>& array) {
  SetPolicyValue(ConvertJavaStringToUTF8(env, policyKey),
                 ConvertJavaStringArrayToListValue(env, array));
}

// static
std::unique_ptr<base::ListValue>
PolicyConverter::ConvertJavaStringArrayToListValue(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array) {
  DCHECK(!array.is_null());
  int length = static_cast<int>(env->GetArrayLength(array.obj()));
  DCHECK_GE(length, 0) << "Invalid array length: " << length;

  std::unique_ptr<base::ListValue> list_value(new base::ListValue());
  for (int i = 0; i < length; ++i) {
    jstring str =
        static_cast<jstring>(env->GetObjectArrayElement(array.obj(), i));
    list_value->AppendString(ConvertJavaStringToUTF8(env, str));
  }

  return list_value;
}

// static
std::unique_ptr<base::Value> PolicyConverter::ConvertValueToSchema(
    std::unique_ptr<base::Value> value,
    const Schema& schema) {
  if (!schema.valid())
    return value;

  switch (schema.type()) {
    case base::Value::TYPE_NULL:
      return base::Value::CreateNullValue();

    case base::Value::TYPE_BOOLEAN: {
      std::string string_value;
      if (value->GetAsString(&string_value)) {
        if (string_value.compare("true") == 0)
          return base::WrapUnique(new base::FundamentalValue(true));

        if (string_value.compare("false") == 0)
          return base::WrapUnique(new base::FundamentalValue(false));

        return value;
      }
      int int_value = 0;
      if (value->GetAsInteger(&int_value))
        return base::WrapUnique(new base::FundamentalValue(int_value != 0));

      return value;
    }

    case base::Value::TYPE_INTEGER: {
      std::string string_value;
      if (value->GetAsString(&string_value)) {
        int int_value = 0;
        if (base::StringToInt(string_value, &int_value))
          return base::WrapUnique(new base::FundamentalValue(int_value));
      }
      return value;
    }

    case base::Value::TYPE_DOUBLE: {
      std::string string_value;
      if (value->GetAsString(&string_value)) {
        double double_value = 0;
        if (base::StringToDouble(string_value, &double_value))
          return base::WrapUnique(new base::FundamentalValue(double_value));
      }
      return value;
    }

    // String can't be converted from other types.
    case base::Value::TYPE_STRING: {
      return value;
    }

    // Binary is not a valid schema type.
    case base::Value::TYPE_BINARY: {
      NOTREACHED();
      return std::unique_ptr<base::Value>();
    }

    // Complex types have to be deserialized from JSON.
    case base::Value::TYPE_DICTIONARY:
    case base::Value::TYPE_LIST: {
      std::string string_value;
      if (value->GetAsString(&string_value)) {
        std::unique_ptr<base::Value> decoded_value =
            base::JSONReader::Read(string_value);
        if (decoded_value)
          return decoded_value;
      }
      return value;
    }
  }

  NOTREACHED();
  return std::unique_ptr<base::Value>();
}

// static
bool PolicyConverter::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void PolicyConverter::SetPolicyValue(const std::string& key,
                                     std::unique_ptr<base::Value> value) {
  const Schema schema = policy_schema_->GetKnownProperty(key);
  const PolicyNamespace ns(POLICY_DOMAIN_CHROME, std::string());
  policy_bundle_->Get(ns).Set(
      key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      ConvertValueToSchema(std::move(value), schema), nullptr);
}

}  // namespace android
}  // namespace policy
