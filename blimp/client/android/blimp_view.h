// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_ANDROID_BLIMP_VIEW_H_
#define BLIMP_CLIENT_ANDROID_BLIMP_VIEW_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace gfx {
class Size;
}

namespace blimp {

class BlimpCompositorAndroid;

// The native component of org.chromium.blimp.BlimpView.  This builds and
// maintains a BlimpCompositorAndroid and handles notifying the compositor of
// SurfaceView surface changes (size, creation, destruction, etc.).
class BlimpView {
 public:
  static bool RegisterJni(JNIEnv* env);

  // |real_size| is the total display area including system decorations (see
  // android.view.Display.getRealSize()).  |size| is the total display
  // area not including system decorations (see android.view.Display.getSize()).
  // |dp_to_px| is the scale factor that is required to convert dp (device
  // pixels) to px.
  BlimpView(JNIEnv* env,
            const base::android::JavaParamRef<jobject>& jobj,
            const gfx::Size& real_size,
            const gfx::Size& size,
            float dp_to_px);

  // Methods called from Java via JNI.
  void Destroy(JNIEnv* env, jobject jobj);
  void SetNeedsComposite(JNIEnv* env, jobject jobj);
  void OnSurfaceChanged(JNIEnv* env,
                        jobject jobj,
                        jint format,
                        jint width,
                        jint height,
                        jobject jsurface);
  void OnSurfaceCreated(JNIEnv* env, jobject jobj);
  void OnSurfaceDestroyed(JNIEnv* env, jobject jobj);
  void SetVisibility(JNIEnv* env, jobject jobj, jboolean visible);

 private:
  virtual ~BlimpView();

  // Reference to the Java object which owns this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  scoped_ptr<BlimpCompositorAndroid> compositor_;

  // The format of the current surface owned by |compositor_|.  See
  // android.graphics.PixelFormat.java.
  int current_surface_format_;

  DISALLOW_COPY_AND_ASSIGN(BlimpView);
};

}  // namespace blimp

#endif  // BLIMP_CLIENT_ANDROID_BLIMP_VIEW_H_
