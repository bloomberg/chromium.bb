// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_PUBLIC_BROWSER_DRAW_SW_H_
#define ANDROID_WEBVIEW_PUBLIC_BROWSER_DRAW_SW_H_

#include <jni.h>
#include <stddef.h>

#ifndef __cplusplus
#error "Can't mix C and C++ when using jni.h"
#endif

class SkPicture;

// Holds the information required to implement the SW draw to system canvas.
struct AwPixelInfo {
  int config;         // In SkBitmap::Config format.
  int width;          // In pixels.
  int height;         // In pixels.
  int row_bytes;      // Number of bytes from start of one line to next.
  void* pixels;       // The pixels, all (height * row_bytes) of them.
  float matrix[9];    // The matrix currently in effect on the canvas.
  void* clip_region;  // Flattened clip region.
  size_t clip_region_size;   // Number of bytes in |clip_region|.
};

// Function that can be called to fish out the underlying native pixel data
// from a Java canvas object, for optimized rendering path.
// Returns the pixel info on success, which must be freed via a call to
// AwReleasePixelsFunction, or NULL.
typedef AwPixelInfo* (AwAccessPixelsFunction)(JNIEnv* env, jobject canvas);

// Must be called to balance every *successful* call to AwAccessPixelsFunction
// (i.e. that returned true).
typedef void (AwReleasePixelsFunction)(AwPixelInfo* pixels);

// Called to create an Android Picture object encapsulating a native SkPicture.
typedef jobject (AwCreatePictureFunction)(JNIEnv* env, SkPicture* picture);

// Method that returns the current Skia function.
typedef void (SkiaVersionFunction)(int* major, int* minor, int* patch);

// Called to verify if the Skia versions are compatible.
typedef bool (AwIsSkiaVersionCompatibleFunction)(SkiaVersionFunction function);

// "vtable" for the functions declared in this file. An instance must be set via
// AwContents.setAwDrawSWFunctionTable
struct AwDrawSWFunctionTable {
  AwAccessPixelsFunction* access_pixels;
  AwReleasePixelsFunction* release_pixels;
  AwCreatePictureFunction* create_picture;
  AwIsSkiaVersionCompatibleFunction* is_skia_version_compatible;
};

#endif  // ANDROID_WEBVIEW_PUBLIC_BROWSER_DRAW_SW_H_
