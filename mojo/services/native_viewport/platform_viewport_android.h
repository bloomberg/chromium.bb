// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_ANDROID_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "mojo/services/native_viewport/platform_viewport.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/sequential_id_generator.h"
#include "ui/gfx/size.h"

namespace gpu {
class GLInProcessContext;
}

struct ANativeWindow;

namespace mojo {

class PlatformViewportAndroid : public PlatformViewport {
 public:
  static bool Register(JNIEnv* env);

  explicit PlatformViewportAndroid(Delegate* delegate);
  virtual ~PlatformViewportAndroid();

  void Destroy(JNIEnv* env, jobject obj);
  void SurfaceCreated(JNIEnv* env, jobject obj, jobject jsurface);
  void SurfaceDestroyed(JNIEnv* env, jobject obj);
  void SurfaceSetSize(JNIEnv* env, jobject obj, jint width, jint height);
  bool TouchEvent(JNIEnv* env, jobject obj, jint pointer_id, jint action,
                  jfloat x, jfloat y, jlong time_ms);

 private:
  // Overridden from PlatformViewport:
  virtual void Init(const gfx::Rect& bounds) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;

  void ReleaseWindow();

  Delegate* delegate_;
  ANativeWindow* window_;
  gfx::Rect bounds_;
  ui::SequentialIDGenerator id_generator_;

  base::WeakPtrFactory<PlatformViewportAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportAndroid);
};


}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_ANDROID_H_
