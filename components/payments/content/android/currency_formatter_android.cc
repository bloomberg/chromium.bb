// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/currency_formatter_android.h"

#include "base/android/jni_string.h"
#include "base/strings/string16.h"
#include "components/payments/core/currency_formatter.h"
#include "jni/CurrencyFormatter_jni.h"

namespace payments {
namespace {

using ::base::android::JavaParamRef;
using ::base::android::ConvertJavaStringToUTF8;

}  // namespace

CurrencyFormatterAndroid::CurrencyFormatterAndroid(
    JNIEnv* env,
    jobject jcaller,
    const JavaParamRef<jstring>& currency_code,
    const JavaParamRef<jstring>& currency_system,
    const JavaParamRef<jstring>& locale_name) {
  std::string currency_system_str =
      ConvertJavaStringToUTF8(env, currency_system);

  currency_formatter_.reset(new CurrencyFormatter(
      ConvertJavaStringToUTF8(env, currency_code), currency_system_str,
      ConvertJavaStringToUTF8(env, locale_name)));
}

CurrencyFormatterAndroid::~CurrencyFormatterAndroid() {}

void CurrencyFormatterAndroid::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& jcaller) {
  delete this;
}

base::android::ScopedJavaLocalRef<jstring> CurrencyFormatterAndroid::Format(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& amount) {
  base::string16 result =
      currency_formatter_->Format(ConvertJavaStringToUTF8(env, amount));
  return base::android::ConvertUTF16ToJavaString(env, result);
}

base::android::ScopedJavaLocalRef<jstring>
CurrencyFormatterAndroid::GetFormattedCurrencyCode(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj) {
  return base::android::ConvertUTF8ToJavaString(
      env, currency_formatter_->formatted_currency_code());
}

static jlong InitCurrencyFormatterAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& currency_code,
    const JavaParamRef<jstring>& currency_system,
    const JavaParamRef<jstring>& locale_name) {
  CurrencyFormatterAndroid* currency_formatter_android =
      new CurrencyFormatterAndroid(env, obj, currency_code, currency_system,
                                   locale_name);
  return reinterpret_cast<intptr_t>(currency_formatter_android);
}

}  // namespace payments
