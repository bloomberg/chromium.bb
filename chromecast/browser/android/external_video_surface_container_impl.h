// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ANDROID_EXTERNAL_VIDEO_SURFACE_CONTAINER_IMPL_H_
#define CHROMECAST_BROWSER_ANDROID_EXTERNAL_VIDEO_SURFACE_CONTAINER_IMPL_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "content/public/browser/android/external_video_surface_container.h"

namespace chromecast {
namespace shell {

class ExternalVideoSurfaceContainerImpl
    : public content::ExternalVideoSurfaceContainer {
 public:
  typedef base::Callback<void(int, jobject)> SurfaceCreatedCB;
  typedef base::Callback<void(int)> SurfaceDestroyedCB;

  ExternalVideoSurfaceContainerImpl(content::WebContents* contents);

  // ExternalVideoSurfaceContainer implementation.
  void RequestExternalVideoSurface(
      int player_id,
      const SurfaceCreatedCB& surface_created_cb,
      const SurfaceDestroyedCB& surface_destroyed_cb) override;
  int GetCurrentPlayerId() override;
  void ReleaseExternalVideoSurface(int player_id) override;
  void OnFrameInfoUpdated() override;
  void OnExternalVideoSurfacePositionChanged(
      int player_id, const gfx::RectF& rect) override;

  // Methods called from Java.
  void SurfaceCreated(
      JNIEnv* env, jobject obj, jint player_id, jobject jsurface);
  void SurfaceDestroyed(JNIEnv* env, jobject obj, jint player_id);

 private:
  ~ExternalVideoSurfaceContainerImpl() override;

  base::android::ScopedJavaGlobalRef<jobject> jobject_;

  SurfaceCreatedCB surface_created_cb_;
  SurfaceDestroyedCB surface_destroyed_cb_;

 DISALLOW_COPY_AND_ASSIGN(ExternalVideoSurfaceContainerImpl);
};

bool RegisterExternalVideoSurfaceContainer(JNIEnv* env);

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ANDROID_EXTERNAL_VIDEO_SURFACE_CONTAINER_IMPL_H_
