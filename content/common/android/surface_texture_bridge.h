// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_BRIDGE_H_
#define CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_BRIDGE_H_
#pragma once

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"

namespace content {

// This class serves as a bridge for native code to call java functions inside
// android SurfaceTexture class.
class SurfaceTextureBridge {
 public:
  explicit SurfaceTextureBridge(int texture_id);
  ~SurfaceTextureBridge();

  // Set the listener callback, which will be invoked on the same thread that
  // is being called from here for registration.
  // Note: Since callbacks come in from Java objects that might outlive objects
  // being referenced from the callback, the only robust way here is to create
  // the callback from a weak pointer to your object.
  void SetFrameAvailableCallback(const base::Closure& callback);

  // Update the texture image to the most recent frame from the image stream.
  void UpdateTexImage();

  // Retrieve the 4x4 texture coordinate transform matrix associated with the
  // texture image set by the most recent call to updateTexImage.
  void GetTransformMatrix(float mtx[16]);

  // Set the default size of the image buffers.
  void SetDefaultBufferSize(int width, int height);

  int texture_id() const {
    return texture_id_;
  }

  const base::android::JavaRef<jobject>& j_surface_texture() const {
    return j_surface_texture_;
  }

 private:
  const int texture_id_;

  // Java SurfaceTexture class and instance.
  base::android::ScopedJavaGlobalRef<jclass> j_class_;
  base::android::ScopedJavaGlobalRef<jobject> j_surface_texture_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTextureBridge);
};

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_SURFACE_TEXTURE_BRIDGE_H_
