// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BLIMP_CHROME_BLIMP_CLIENT_CONTEXT_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_BLIMP_CHROME_BLIMP_CLIENT_CONTEXT_DELEGATE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/blimp/chrome_blimp_client_context_delegate.h"

// Android implementation of ChromeBlimpClientContextDelegate which is partly
// backed by Java.
class ChromeBlimpClientContextDelegateAndroid
    : public ChromeBlimpClientContextDelegate {
 public:
  static bool RegisterJni(JNIEnv* env);

  ChromeBlimpClientContextDelegateAndroid(JNIEnv* env,
                                          jobject jobj,
                                          Profile* profile);
  ~ChromeBlimpClientContextDelegateAndroid() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBlimpClientContextDelegateAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_BLIMP_CHROME_BLIMP_CLIENT_CONTEXT_DELEGATE_ANDROID_H_
