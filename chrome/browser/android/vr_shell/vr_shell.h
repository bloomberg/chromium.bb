// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/android/vr_shell/vr_math.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/ndk/include/vr/gvr/capi/include/gvr_types.h"

using blink::WebInputEvent;

namespace content {
class WebContents;
}

namespace ui {
class WindowAndroid;
}

namespace vr_shell {

class UiInterface;
class UiScene;
class VrCompositor;
class VrController;
class VrInputManager;
class VrMetricsHelper;
class VrShellDelegate;
class VrShellRenderer;
class VrWebContentsObserver;
struct ContentRectangle;

enum UiAction {
  HISTORY_BACK = 0,
  HISTORY_FORWARD,
  RELOAD,
  ZOOM_OUT,
  ZOOM_IN,
  RELOAD_UI
};

class VrMetricsHelper;

class VrShell : public device::GvrDelegate, content::WebContentsObserver {
 public:
  VrShell(JNIEnv* env, jobject obj,
          content::WebContents* main_contents,
          ui::WindowAndroid* content_window,
          content::WebContents* ui_contents,
          ui::WindowAndroid* ui_window,
          bool for_web_vr);

  void UpdateCompositorLayers(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetDelegate(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& delegate);
  void GvrInit(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jlong native_gvr_api);
  void InitializeGl(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jint content_texture_handle,
                    jint ui_texture_handle);
  void DrawFrame(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnTriggerEvent(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnPause(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetWebVrMode(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    bool enabled);

  // html/js UI hooks.
  static base::WeakPtr<VrShell> GetWeakPtr(
      const content::WebContents* web_contents);
  UiScene* GetScene();
  UiInterface* GetUiInterface();
  void OnDomContentsLoaded();

  // device::GvrDelegate implementation
  void SetWebVRSecureOrigin(bool secure_origin) override;
  void SubmitWebVRFrame() override;
  void UpdateWebVRTextureBounds(const gvr::Rectf& left_bounds,
                                const gvr::Rectf& right_bounds) override;
  gvr::GvrApi* gvr_api() override;
  void SetGvrPoseForWebVr(const gvr::Mat4f& pose, uint32_t pose_num) override;
  void SetWebVRRenderSurfaceSize(int width, int height) override;
  gvr::Sizei GetWebVRCompositorSurfaceSize() override;

  void SurfacesChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& content_surface,
      const base::android::JavaParamRef<jobject>& ui_surface);

  void ContentBoundsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint width, jint height, jfloat dpr);

  void UIBoundsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint width, jint height, jfloat dpr);

  // Called from non-render thread to queue a callback onto the render thread.
  // The render thread checks for callbacks and processes them between frames.
  void QueueTask(base::Callback<void()>& callback);

  // Perform a UI action triggered by the javascript API.
  void DoUiAction(const UiAction action);

  void SetContentCssSize(float width, float height, float dpr);
  void SetUiCssSize(float width, float height, float dpr);

 private:
  ~VrShell() override;
  void LoadUIContent();
  void DrawVrShell(const gvr::Mat4f& head_pose, gvr::Frame &frame);
  void DrawUiView(const gvr::Mat4f* head_pose,
                  const std::vector<const ContentRectangle*>& elements,
                  const gvr::Sizei& render_size, int viewport_offset);
  void DrawElements(const gvr::Mat4f& render_matrix,
                    const std::vector<const ContentRectangle*>& elements);
  void DrawCursor(const gvr::Mat4f& render_matrix);
  void DrawWebVr();
  bool WebVrPoseByteIsValid(int pose_index_byte);

  void UpdateController(const gvr::Vec3f& forward_vector);
  void SendEventsToTarget(VrInputManager* input_target,
                          int pixel_x,
                          int pixel_y);

  void HandleQueuedTasks();

  // content::WebContentsObserver implementation.
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void MainFrameWasResized(bool width_changed) override;

  // samplerExternalOES texture data for UI content image.
  jint ui_texture_id_ = 0;
  // samplerExternalOES texture data for main content image.
  jint content_texture_id_ = 0;

  std::unique_ptr<UiScene> scene_;
  std::unique_ptr<UiInterface> html_interface_;

  std::unique_ptr<gvr::GvrApi> gvr_api_;
  std::unique_ptr<gvr::BufferViewportList> buffer_viewport_list_;
  std::unique_ptr<gvr::BufferViewport> buffer_viewport_;
  std::unique_ptr<gvr::BufferViewport> headlocked_left_viewport_;
  std::unique_ptr<gvr::BufferViewport> headlocked_right_viewport_;
  std::unique_ptr<gvr::BufferViewport> webvr_left_viewport_;
  std::unique_ptr<gvr::BufferViewport> webvr_right_viewport_;
  std::unique_ptr<gvr::SwapChain> swap_chain_;

  // Current sizes for the render buffers.
  gvr::Sizei render_size_primary_;
  gvr::Sizei render_size_headlocked_;

  // Intended size for the primary render buffer by UI mode.
  // For WebVR, a size of 0x0 is used to indicate "not yet ready"
  // to suppress rendering while still initializing.
  gvr::Sizei render_size_primary_webvr_ = device::kInvalidRenderTargetSize;
  gvr::Sizei render_size_primary_vrshell_;

  std::queue<base::Callback<void()>> task_queue_;
  base::Lock task_queue_lock_;
  base::Lock gvr_init_lock_;

  std::unique_ptr<VrCompositor> content_compositor_;
  content::WebContents* main_contents_;
  std::unique_ptr<VrCompositor> ui_compositor_;
  content::WebContents* ui_contents_;
  std::unique_ptr<VrWebContentsObserver> vr_web_contents_observer_;

  VrShellDelegate* delegate_ = nullptr;
  std::unique_ptr<VrShellRenderer> vr_shell_renderer_;
  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_;

  bool touch_pending_ = false;
  gvr::Quatf controller_quat_;

  gvr::Vec3f target_point_;
  const ContentRectangle* target_element_ = nullptr;
  VrInputManager* current_input_target_ = nullptr;
  int ui_tex_css_width_ = 0;
  int ui_tex_css_height_ = 0;
  int content_tex_css_width_ = 0;
  int content_tex_css_height_ = 0;
  gvr::Sizei content_tex_physical_size_ = {0, 0};

  // The pose ring buffer size must be a power of two to avoid glitches when
  // the pose index wraps around. It should be large enough to handle the
  // current backlog of poses which is 2-3 frames.
  static constexpr int kPoseRingBufferSize = 8;
  std::vector<gvr::Mat4f> webvr_head_pose_;
  std::vector<bool> webvr_head_pose_valid_;
  jint webvr_texture_id_ = 0;

  std::unique_ptr<VrController> controller_;
  scoped_refptr<VrInputManager> content_input_manager_;
  scoped_refptr<VrInputManager> ui_input_manager_;
  scoped_refptr<VrMetricsHelper> metrics_helper_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  base::WeakPtrFactory<VrShell> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShell);
};

bool RegisterVrShell(JNIEnv* env);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
