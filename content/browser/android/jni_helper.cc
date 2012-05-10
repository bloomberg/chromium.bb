// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/jni_helper.h"

#include <android/bitmap.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "jni/jni_helper_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::GetStaticMethodID;
using base::android::GetClass;
using base::android::GetMethodID;
using base::android::ScopedJavaLocalRef;

AutoLocalFrame::AutoLocalFrame(int capacity) {
  AttachCurrentThread()->PushLocalFrame(capacity);
}

AutoLocalFrame::~AutoLocalFrame() {
  AttachCurrentThread()->PopLocalFrame(NULL);
}

AutoLockJavaBitmap::AutoLockJavaBitmap(jobject bitmap) :
    bitmap_(bitmap),
    pixels_(NULL) {
  int err = AndroidBitmap_lockPixels(AttachCurrentThread(), bitmap_, &pixels_);
  DCHECK(!err);
  DCHECK(pixels_);
}

AutoLockJavaBitmap::~AutoLockJavaBitmap() {
  // TODO(aelias): Add AndroidBitmap_notifyPixelsChanged() call here
  //               once it's added to the NDK.  Using hardware bitmaps will
  //               be broken until this is fixed.
  int err = AndroidBitmap_unlockPixels(AttachCurrentThread(), bitmap_);
  DCHECK(!err);
}

gfx::Size AutoLockJavaBitmap::size() const {
  AndroidBitmapInfo info;
  int err = AndroidBitmap_getInfo(AttachCurrentThread(), bitmap_, &info);
  DCHECK(!err);
  return gfx::Size(info.width, info.height);
}

int AutoLockJavaBitmap::format() const {
  AndroidBitmapInfo info;
  int err = AndroidBitmap_getInfo(AttachCurrentThread(), bitmap_, &info);
  DCHECK(!err);
  return info.format;
}

uint32_t AutoLockJavaBitmap::stride() const {
  AndroidBitmapInfo info;
  int err = AndroidBitmap_getInfo(AttachCurrentThread(), bitmap_, &info);
  DCHECK(!err);
  return info.stride;
}

void PrintJavaStackTrace() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jclass> throwable_clazz =
      GetClass(env, "java/lang/Throwable");
  jmethodID throwable_constructor =
      GetMethodID(env, throwable_clazz, "<init>", "()V");

  ScopedJavaLocalRef<jobject> throwable_object(env,
      env->NewObject(throwable_clazz.obj(), throwable_constructor));
  DCHECK(!throwable_object.is_null());
  jmethodID printstacktrace =
      GetMethodID(env, throwable_clazz, "printStackTrace", "()V");

  env->CallVoidMethod(throwable_object.obj(), printstacktrace);
  CheckException(env);
}

void ConvertJavaArrayOfStringsToVectorOfStrings(
    JNIEnv* env,
    jobjectArray jstrings,
    std::vector<std::string>* vec) {
  vec->clear();
  jsize length = env->GetArrayLength(jstrings);
  for (jsize i = 0; i < length; ++i) {
    jstring item = static_cast<jstring>(
        env->GetObjectArrayElement(jstrings, i));
    vec->push_back(base::android::ConvertJavaStringToUTF8(env, item));
    env->DeleteLocalRef(item);
  }
}

ScopedJavaLocalRef<jobject> CreateJavaBitmap(const gfx::Size& size, bool keep) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> bitmap =
      Java_JNIHelper_createJavaBitmap(env, size.width(), size.height(), keep);
  CheckException(env);
  return bitmap;
}

void DeleteJavaBitmap(jobject bitmap) {
  JNIEnv* env = AttachCurrentThread();
  Java_JNIHelper_deleteJavaBitmap(env, bitmap);
  CheckException(env);
}

void PaintJavaBitmapToJavaBitmap(jobject source_bitmap,
                                 const gfx::Rect& source_rect,
                                 jobject dest_bitmap,
                                 const gfx::Rect& dest_rect) {
  TRACE_EVENT0("jni", "PaintJavaBitmapToJavaBitmap");
  JNIEnv* env = AttachCurrentThread();

  Java_JNIHelper_paintJavaBitmapToJavaBitmap(env,
                                             source_bitmap,
                                             source_rect.x(),
                                             source_rect.y(),
                                             source_rect.right(),
                                             source_rect.bottom(),
                                             dest_bitmap,
                                             dest_rect.x(),
                                             dest_rect.y(),
                                             dest_rect.right(),
                                             dest_rect.bottom());
  CheckException(env);
}

ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(const SkBitmap* skbitmap) {
  TRACE_EVENT0("jni", "ConvertToJavaBitmap");
  DCHECK(skbitmap);
  DCHECK_EQ(skbitmap->bytesPerPixel(), 4);

  // Create a temporary java bitmap.
  ScopedJavaLocalRef<jobject> jbitmap =
      CreateJavaBitmap(gfx::Size(skbitmap->width(), skbitmap->height()), false);

  // Lock and copy the pixels from the skbitmap.
  SkAutoLockPixels src_lock(*skbitmap);
  AutoLockJavaBitmap dst_lock(jbitmap.obj());
  void* src_pixels = skbitmap->getPixels();
  void* dst_pixels = dst_lock.pixels();
  memcpy(dst_pixels, src_pixels, skbitmap->getSize());

  return jbitmap;
}

bool RegisterJniHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
