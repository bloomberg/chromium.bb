// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/android/auxiliary_profile_loader_android.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "jni/PersonalAutofillPopulator_jni.h"

#define JAVA_METHOD(__jmethod__) Java_PersonalAutofillPopulator_##__jmethod__( \
    env_, \
    populator_.obj())

namespace {

string16 SafeJavaStringToUTF16(const ScopedJavaLocalRef<jstring>& jstring) {
  if (jstring.is_null())
    return string16();

  return ConvertJavaStringToUTF16(jstring);
}

void SafeJavaStringArrayToStringVector(
    const ScopedJavaLocalRef<jobjectArray>& jarray,
    JNIEnv* env,
    std::vector<string16>* string_vector) {
  if (!jarray.is_null()) {
    base::android::AppendJavaStringArrayToStringVector(env,
                                                       jarray.obj(),
                                                       string_vector);
  }
}

} // namespace

namespace autofill {

bool RegisterAuxiliaryProfileLoader(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

AuxiliaryProfileLoaderAndroid::AuxiliaryProfileLoaderAndroid() {}

AuxiliaryProfileLoaderAndroid::~AuxiliaryProfileLoaderAndroid() {}

void AuxiliaryProfileLoaderAndroid::Init(JNIEnv* env, const jobject& context) {
  env_ = env;
  populator_ = Java_PersonalAutofillPopulator_create(env_, context);
}

bool AuxiliaryProfileLoaderAndroid::GetHasPermissions() const {
  return (bool)JAVA_METHOD(getHasPermissions);
}

// Address info
string16 AuxiliaryProfileLoaderAndroid::GetStreet() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getStreet));
}

string16 AuxiliaryProfileLoaderAndroid::GetPostOfficeBox() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getPobox));
}

string16 AuxiliaryProfileLoaderAndroid::GetNeighborhood() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getNeighborhood));
}

string16 AuxiliaryProfileLoaderAndroid::GetRegion() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getRegion));
}

string16 AuxiliaryProfileLoaderAndroid::GetCity() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getCity));
}

string16 AuxiliaryProfileLoaderAndroid::GetPostalCode() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getPostalCode));
}

string16 AuxiliaryProfileLoaderAndroid::GetCountry() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getCountry));
}

// Name info
string16 AuxiliaryProfileLoaderAndroid::GetFirstName() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getFirstName));
}

string16 AuxiliaryProfileLoaderAndroid::GetMiddleName() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getMiddleName));
}

string16 AuxiliaryProfileLoaderAndroid::GetLastName() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getLastName));
}

string16 AuxiliaryProfileLoaderAndroid::GetSuffix() const {
  return SafeJavaStringToUTF16(JAVA_METHOD(getSuffix));
}

// Email info
void AuxiliaryProfileLoaderAndroid::GetEmailAddresses(
    std::vector<string16>* email_addresses) const {
  SafeJavaStringArrayToStringVector(JAVA_METHOD(getEmailAddresses),
                                    env_,
                                    email_addresses);
}

// Phone info
void AuxiliaryProfileLoaderAndroid::GetPhoneNumbers(
    std::vector<string16>* phone_numbers) const {
  SafeJavaStringArrayToStringVector(JAVA_METHOD(getPhoneNumbers),
                                    env_,
                                    phone_numbers);
}

} // namespace
