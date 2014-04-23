// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_PICTURE_H_
#define ANDROID_WEBVIEW_NATIVE_AW_PICTURE_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "skia/ext/refptr.h"

class SkPicture;

namespace android_webview {

class AwPicture {
 public:
  AwPicture(skia::RefPtr<SkPicture> picture);
  ~AwPicture();

  // Methods called from Java.
  void Destroy(JNIEnv* env, jobject obj);
  jint GetWidth(JNIEnv* env, jobject obj);
  jint GetHeight(JNIEnv* env, jobject obj);
  void Draw(JNIEnv* env, jobject obj, jobject canvas,
            jint left, jint top, jint right, jint bottom);

 private:
  skia::RefPtr<SkPicture> picture_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AwPicture);
};

bool RegisterAwPicture(JNIEnv* env);

}  // android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_PICTURE_
