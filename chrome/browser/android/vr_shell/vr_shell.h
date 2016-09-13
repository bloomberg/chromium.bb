// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_

#include <jni.h>
#include <memory>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/vr_shell/ui_elements.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"


namespace vr_shell {

class VrShellRenderer;


class VrShell : public device::GvrDelegate {
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

  // GvrDelegate
  void RequestWebVRPresent() override;
  void ExitWebVRPresent() override;
  void SubmitWebVRFrame() override;
  void UpdateWebVRTextureBounds(
      int eye, float left, float top, float width, float height) override;
  gvr::GvrApi* gvr_api() override;

 private:
  virtual ~VrShell();
  void DrawVrShell();
  void DrawEye(const gvr::Mat4f& view_matrix,
               const gvr::BufferViewport& params);
  void DrawContentRect();
  void DrawWebVr();
  void DrawUI();

  void UpdateTransforms(float screen_width_meters,
                        float screen_height_meters,
                        float scree_tilt);

  // samplerExternalOES texture data for content area image.
  jint content_texture_id_ = 0;

  float desktop_screen_tilt_;
  float desktop_height_;

  ContentRectangle* desktop_plane_;
  gvr::Vec3f desktop_position_;

  std::vector<std::unique_ptr<ContentRectangle>> ui_rects_;

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

  bool webvr_mode_ = false;

  DISALLOW_COPY_AND_ASSIGN(VrShell);
};

bool RegisterVrShell(JNIEnv* env);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
