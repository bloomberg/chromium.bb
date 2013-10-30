// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_ANDROID_MOJO_VIEW_H_
#define MOJO_SHELL_ANDROID_MOJO_VIEW_H_

#include "base/android/jni_helper.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "ui/gl/gl_surface_egl.h"

struct ANativeWindow;

namespace mojo {

class MojoView {
 public:
  static bool Register(JNIEnv* env);

  MojoView(JNIEnv* env, jobject obj);

  void Destroy(JNIEnv* env, jobject obj);
  void SurfaceCreated(JNIEnv* env, jobject obj, jobject jsurface);
  void SurfaceDestroyed(JNIEnv* env, jobject obj);
  void SurfaceSetSize(JNIEnv* env, jobject obj, jint width, jint height);

 private:
  ~MojoView();

  ANativeWindow* window_;
  scoped_refptr<gfx::GLSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(MojoView);
};

}  // namespace mojo

#endif  // MOJO_SHELL_ANDROID_MOJO_VIEW_H_
