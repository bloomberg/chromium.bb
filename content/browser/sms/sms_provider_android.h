// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_SMS_SMS_PROVIDER_ANDROID_H_

#include <utility>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/browser/sms/sms_provider.h"

namespace content {

class SmsProviderAndroid : public SmsProvider {
 public:
  SmsProviderAndroid();
  ~SmsProviderAndroid() override;

  void Retrieve() override;

  void OnReceive(JNIEnv*,
                 const base::android::JavaParamRef<jobject>&,
                 jstring message);
  void OnTimeout(JNIEnv* env, const base::android::JavaParamRef<jobject>&);

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_sms_receiver_;

  DISALLOW_COPY_AND_ASSIGN(SmsProviderAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PROVIDER_ANDROID_H_