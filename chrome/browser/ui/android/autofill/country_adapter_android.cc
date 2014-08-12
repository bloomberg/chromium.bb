// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/country_adapter_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "jni/CountryAdapter_jni.h"

namespace autofill {

CountryAdapterAndroid::CountryAdapterAndroid(JNIEnv* env, jobject obj)
    : weak_java_obj_(env, obj) {
  country_combobox_model_.SetCountries(
      *PersonalDataManagerFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile()),
      base::Callback<bool(const std::string&)>());
}

CountryAdapterAndroid::~CountryAdapterAndroid() {}

ScopedJavaLocalRef<jobjectArray> CountryAdapterAndroid::GetItems(
    JNIEnv* env,
    jobject unused_obj) {
  std::vector<base::string16> country_names_and_codes;
  country_names_and_codes.reserve(
      2 * country_combobox_model_.countries().size());

  for (size_t i = 0; i < country_combobox_model_.countries().size(); ++i) {
    country_names_and_codes.push_back(
        country_combobox_model_.countries()[i]->name());
    country_names_and_codes.push_back(base::ASCIIToUTF16(
        country_combobox_model_.countries()[i]->country_code()));
  }

  return base::android::ToJavaArrayOfStrings(env, country_names_and_codes);
}

// static
bool CountryAdapterAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env, jobject obj) {
  CountryAdapterAndroid* country_adapter_android =
      new CountryAdapterAndroid(env, obj);
  return reinterpret_cast<intptr_t>(country_adapter_android);
}

}  // namespace autofill
