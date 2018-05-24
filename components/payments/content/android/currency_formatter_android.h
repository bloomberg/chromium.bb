// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_CURRENCY_FORMATTER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_CURRENCY_FORMATTER_ANDROID_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

namespace payments {

class CurrencyFormatter;

// Forwarding calls to payments::CurrencyFormatter.
class CurrencyFormatterAndroid {
 public:
  CurrencyFormatterAndroid(
      JNIEnv* env,
      jobject jcaller,
      const base::android::JavaParamRef<jstring>& currency_code,
      const base::android::JavaParamRef<jstring>& locale_name);
  ~CurrencyFormatterAndroid();

  // Message from Java to destroy this object.
  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jcaller);

  // Refer to CurrencyFormatter::Format documentation.
  base::android::ScopedJavaLocalRef<jstring> Format(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& amount);

  base::android::ScopedJavaLocalRef<jstring> GetFormattedCurrencyCode(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  std::unique_ptr<CurrencyFormatter> currency_formatter_;

  DISALLOW_COPY_AND_ASSIGN(CurrencyFormatterAndroid);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_CURRENCY_FORMATTER_ANDROID_H_
