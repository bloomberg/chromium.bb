// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_ANDROID_H_
#define COMPONENTS_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/native_viewport/platform_viewport.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/sequential_id_generator.h"

namespace gpu {
class GLInProcessContext;
}

struct ANativeWindow;

namespace native_viewport {

class PlatformViewportAndroid : public PlatformViewport {
 public:
  static bool Register(JNIEnv* env);

  explicit PlatformViewportAndroid(Delegate* delegate);
  ~PlatformViewportAndroid() override;

  void Destroy(JNIEnv* env, jobject obj);
  void SurfaceCreated(JNIEnv* env,
                      jobject obj,
                      jobject jsurface,
                      float device_pixel_ratio);
  void SurfaceDestroyed(JNIEnv* env, jobject obj);
  void SurfaceSetSize(JNIEnv* env,
                      jobject obj,
                      jint width,
                      jint height,
                      jfloat density);
  bool TouchEvent(JNIEnv* env,
                  jobject obj,
                  jlong time_ms,
                  jint masked_action,
                  jint pointer_id,
                  jfloat x,
                  jfloat y,
                  jfloat pressure,
                  jfloat touch_major,
                  jfloat touch_minor,
                  jfloat orientation,
                  jfloat h_wheel,
                  jfloat v_wheel);
  bool KeyEvent(JNIEnv* env,
                jobject obj,
                bool pressed,
                jint key_code,
                jint unicode_character);

 private:
  // Overridden from PlatformViewport:
  void Init(const gfx::Rect& bounds) override;
  void Show() override;
  void Hide() override;
  void Close() override;
  gfx::Size GetSize() override;
  void SetBounds(const gfx::Rect& bounds) override;

  void ReleaseWindow();

  Delegate* const delegate_;
  JavaObjectWeakGlobalRef java_platform_viewport_android_;
  ANativeWindow* window_;
  mojo::ViewportMetricsPtr metrics_;
  ui::SequentialIDGenerator id_generator_;

  base::WeakPtrFactory<PlatformViewportAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformViewportAndroid);
};

}  // namespace native_viewport

#endif  // COMPONENTS_NATIVE_VIEWPORT_PLATFORM_VIEWPORT_ANDROID_H_
