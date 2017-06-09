// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PICTURE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PICTURE_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkPicture;

namespace android_webview {

class AwPicture {
 public:
  AwPicture(sk_sp<SkPicture> picture);
  ~AwPicture();

  // Methods called from Java.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jint GetWidth(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  jint GetHeight(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void Draw(JNIEnv* env,
            const base::android::JavaParamRef<jobject>& obj,
            const base::android::JavaParamRef<jobject>& canvas);

 private:
  sk_sp<SkPicture> picture_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AwPicture);
};

bool RegisterAwPicture(JNIEnv* env);

}  // android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_PICTURE_H_
