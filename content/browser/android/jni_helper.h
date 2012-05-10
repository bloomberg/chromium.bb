// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JNI_HELPER_H_
#define CONTENT_BROWSER_ANDROID_JNI_HELPER_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/string16.h"
#include "ui/gfx/rect.h"

class SkBitmap;
namespace gfx {
class Size;
}

// Auto creator/destructor for a JNI frame for local references.  This allows
// safely having more than 16 local references, and avoids calls to
// DeleteLocalRef.
// This should be created on the stack.
class AutoLocalFrame {
 public:
  AutoLocalFrame(int capacity);
  ~AutoLocalFrame();

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoLocalFrame);
};

// Create this object on the stack to obtain the pixels of a Java Bitmap and
// automatically release them on destruction.
// Similar to SkAutoLockPixels, except that it operates on a Java Bitmap
// object via the Android NDK.
class AutoLockJavaBitmap {
 public:
  AutoLockJavaBitmap(jobject bitmap);
  ~AutoLockJavaBitmap();

  void* pixels() const { return pixels_; }
  gfx::Size size() const;
  // Formats are in android/bitmap.h; e.g. ANDROID_BITMAP_FORMAT_RGBA_8888 */
  int format() const;
  uint32_t stride() const;

 private:
  jobject bitmap_;
  void* pixels_;

  DISALLOW_COPY_AND_ASSIGN(AutoLockJavaBitmap);
};

// Tell Java to write its current stack trace into Android logs.  Note that the
// trace will stop at the entry point into C++ code.
void PrintJavaStackTrace();

// Fills in the given vector<string> from a java String[].
void ConvertJavaArrayOfStringsToVectorOfStrings(
    JNIEnv* env,
    jobjectArray jstrings,
    std::vector<std::string>* vec);

// Helper method to create an Android Java Bitmap object.  You can use the
// AutoLockJavaBitmap class to manipulate its pixels, and pass it back up
// to Java code for drawing.  Returns a JNI local reference to the bitmap.
// If 'keep' is true, then a reference will be kept in a static Java data
// structure to prevent GC.  In that case, you must call DeleteJavaBitmap() to
// garbage collect it.
base::android::ScopedJavaLocalRef<jobject> CreateJavaBitmap(
    const gfx::Size& size, bool keep);
void DeleteJavaBitmap(jobject bitmap);

// Paint one java bitmap into another.  Scale if needed.
void PaintJavaBitmapToJavaBitmap(jobject sourceBitmap,
                                 const gfx::Rect& sourceRect,
                                 jobject destBitmap,
                                 const gfx::Rect& destRect);

// Copy the Chromium Skia bitmap into a new Java bitmap.  Useful for small UI
// bitmaps originating from WebKit that we want to manipulate in Java (such as
// favicons).  Due to the extra copy, should be avoided for large or frequently
// used bitmaps.  Returns a local reference to the new bitmap.
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(
    const SkBitmap* skbitmap);

// Registers our JNI methods.
bool RegisterJniHelper(JNIEnv* env);

#endif  // CONTENT_BROWSER_ANDROID_JNI_HELPER_H_
