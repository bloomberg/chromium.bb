// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_

#include <jni.h>

#include <memory>
#include <queue>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
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

enum InputTarget {
  NONE = 0,
  CONTENT,
  UI
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

  void UpdateCompositorLayersOnUI(
      JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void DestroyOnUI(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);
  void SetDelegateOnUI(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       const base::android::JavaParamRef<jobject>& delegate);
  void GvrInitOnGL(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jlong native_gvr_api);
  void InitializeGlOnGL(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jint content_texture_handle,
                        jint ui_texture_handle);
  void DrawFrameOnGL(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  void OnTriggerEventOnUI(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnPauseOnUI(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);
  void OnResumeOnUI(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  void SetWebVrModeOnUI(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        bool enabled);

  void ContentWebContentsDestroyedOnUI();
  // Called when our WebContents have been hidden. Usually a sign that something
  // like another tab placed in front of it.
  void ContentWasHiddenOnUI();

  // html/js UI hooks.
  static base::WeakPtr<VrShell> GetWeakPtrOnUI(
      const content::WebContents* web_contents);
  // TODO(mthiesse): Clean up threading around Scene.
  UiScene* GetSceneOnGL();
  // TODO(mthiesse): Clean up threading around UiInterface.
  UiInterface* GetUiInterfaceOnGL();
  void OnDomContentsLoadedOnUI();

  // device::GvrDelegate implementation
  // TODO(mthiesse): Clean up threading around GVR API. These functions are
  // called on the UI thread, but use GL thread objects in a non-threadsafe way.
  void SetWebVRSecureOrigin(bool secure_origin) override;
  void SubmitWebVRFrame() override;
  void UpdateWebVRTextureBounds(const gvr::Rectf& left_bounds,
                                const gvr::Rectf& right_bounds) override;
  gvr::GvrApi* gvr_api() override;
  void SetGvrPoseForWebVr(const gvr::Mat4f& pose, uint32_t pose_num) override;
  void SetWebVRRenderSurfaceSize(int width, int height) override;
  gvr::Sizei GetWebVRCompositorSurfaceSize() override;

  void SurfacesChangedOnUI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jobject>& content_surface,
      const base::android::JavaParamRef<jobject>& ui_surface);

  void ContentBoundsChangedOnUI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint width, jint height, jfloat dpr);

  void UIBoundsChangedOnUI(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint width, jint height, jfloat dpr);

  // Called from non-render thread to queue a callback onto the render thread.
  // The render thread checks for callbacks and processes them between frames.
  void QueueTaskOnUI(base::Callback<void()>& callback);

  // Perform a UI action triggered by the javascript API.
  void DoUiActionOnUI(const UiAction action);

  void SetContentCssSizeOnUI(float width, float height, float dpr);
  void SetUiCssSizeOnUI(float width, float height, float dpr);

 private:
  ~VrShell() override;
  void LoadUIContentOnUI();
  void DrawVrShellOnGL(const gvr::Mat4f& head_pose, gvr::Frame &frame);
  void DrawUiViewOnGL(const gvr::Mat4f* head_pose,
                      const std::vector<const ContentRectangle*>& elements,
                      const gvr::Sizei& render_size, int viewport_offset);
  void DrawElementsOnGL(const gvr::Mat4f& render_matrix,
                        const std::vector<const ContentRectangle*>& elements);
  void DrawCursorOnGL(const gvr::Mat4f& render_matrix);
  void DrawWebVrOnGL();
  bool WebVrPoseByteIsValidOnGL(int pose_index_byte);

  void UpdateControllerOnGL(const gvr::Vec3f& forward_vector);
  void SendEventsToTargetOnGL(InputTarget input_target, int pixel_x,
                              int pixel_y);
  void SendGestureOnGL(InputTarget input_target,
                       std::unique_ptr<blink::WebInputEvent> event);

  void HandleQueuedTasksOnGL();

  // content::WebContentsObserver implementation. All called on UI thread.
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void MainFrameWasResized(bool width_changed) override;
  void WebContentsDestroyed() override;

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
  InputTarget current_input_target_ = NONE;
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
  std::unique_ptr<VrInputManager> content_input_manager_;
  base::WeakPtr<VrInputManager> weak_content_input_manager_;
  std::unique_ptr<VrInputManager> ui_input_manager_;
  base::WeakPtr<VrInputManager> weak_ui_input_manager_;
  scoped_refptr<VrMetricsHelper> metrics_helper_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  base::WeakPtrFactory<VrShell> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShell);
};

bool RegisterVrShell(JNIEnv* env);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
