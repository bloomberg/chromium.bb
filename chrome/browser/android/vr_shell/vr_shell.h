// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_

#include <jni.h>
#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"

namespace vr_shell {

class VrShellRenderer;

class ContentRect {
 public:
  ContentRect();
  ~ContentRect();
  void SetIdentity();
  void Translate(float x, float y, float z);
  gvr::Mat4f transfrom_to_world;
  int content_texture_handle;
};

class VrShell {
 public:
  VrShell(JNIEnv* env, jobject obj);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void GvrInit(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jlong native_gvr_api);
  void InitializeGl(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jint texture_data_handle);
  void DrawFrame(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnPause(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  ~VrShell();

  void DrawEye(const gvr::Mat4f& view_matrix,
               const gvr::BufferViewport& params);
  void DrawContentRect();

  std::unique_ptr<ContentRect> content_rect_;
  std::unique_ptr<gvr::GvrApi> gvr_api_;
  std::unique_ptr<gvr::BufferViewportList> buffer_viewport_list_;
  std::unique_ptr<gvr::BufferViewport> buffer_viewport_;
  std::unique_ptr<gvr::SwapChain> swap_chain_;

  gvr::Mat4f view_matrix_;
  gvr::Mat4f projection_matrix_;

  gvr::Mat4f head_pose_;

  gvr::Sizei render_size_;

  std::unique_ptr<VrShellRenderer> vr_shell_renderer_;

  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_;

  DISALLOW_COPY_AND_ASSIGN(VrShell);
};

bool RegisterVrShell(JNIEnv* env);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
