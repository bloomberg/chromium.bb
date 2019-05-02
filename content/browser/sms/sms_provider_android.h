// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_SMS_SMS_PROVIDER_ANDROID_H_

#include "sms_provider.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"

namespace content {

class SmsProviderAndroid : public SmsProvider {
 public:
  SmsProviderAndroid();
  ~SmsProviderAndroid() override;

  void Retrieve(base::TimeDelta timeout, SmsCallback callback) override;

 private:
  void OnReceive(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 jstring message);

  base::android::ScopedJavaGlobalRef<jobject> j_sms_receiver_;
  SmsCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SmsProviderAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PROVIDER_ANDROID_H_