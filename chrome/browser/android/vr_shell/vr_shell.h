// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace base {
class ListValue;
class Thread;
}

namespace content {
class WebContents;
}

namespace ui {
class WindowAndroid;
}

namespace vr_shell {

class UiInterface;
class VrCompositor;
class VrInputManager;
class VrMetricsHelper;
class VrShellDelegate;
class VrWebContentsObserver;

enum UiAction {
  HISTORY_BACK = 0,
  HISTORY_FORWARD,
  RELOAD,
  ZOOM_OUT,
  ZOOM_IN,
  RELOAD_UI
};

class VrMetricsHelper;

// The native instance of the Java VrShell. This class is not threadsafe and
// must only be used on the UI thread.
class VrShell : public device::GvrDelegate, content::WebContentsObserver {
 public:
  VrShell(JNIEnv* env, jobject obj,
          content::WebContents* main_contents,
          ui::WindowAndroid* content_window,
          content::WebContents* ui_contents,
          ui::WindowAndroid* ui_window,
          bool for_web_vr,
          VrShellDelegate* delegate,
          gvr_context* gvr_api,
          bool reprojected_rendering);

  void LoadUIContent(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void OnTriggerEvent(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void OnPause(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj);
  void OnResume(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj);
  void SetSurface(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  const base::android::JavaParamRef<jobject>& surface);
  void SetWebVrMode(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    bool enabled);
  void OnLoadProgressChanged(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             double progress);

  void ContentWebContentsDestroyed();
  // Called when our WebContents have been hidden. Usually a sign that something
  // like another tab placed in front of it.
  void ContentWasHidden();

  // html/js UI hooks.
  static base::WeakPtr<VrShell> GetWeakPtr(
      const content::WebContents* web_contents);

  // TODO(mthiesse): Clean up threading around UiInterface.
  UiInterface* GetUiInterface();
  void OnDomContentsLoaded();

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

  void SurfacesChanged(jobject content_surface, jobject ui_surface);
  void GvrDelegateReady();
  void AppButtonPressed();

  void ContentPhysicalBoundsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint width, jint height, jfloat dpr);

  void UIPhysicalBoundsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      jint width, jint height, jfloat dpr);

  void UpdateScene(const base::ListValue* args);

  // Perform a UI action triggered by the javascript API.
  void DoUiAction(const UiAction action);

  void SetContentCssSize(float width, float height, float dpr);
  void SetUiCssSize(float width, float height, float dpr);

  void ContentFrameWasResized(bool width_changed);

  void ForceExitVr();

 private:
  ~VrShell() override;
  void SetShowingOverscrollGlow(bool showing_glow);
  void PostToGlThreadWhenReady(const base::Closure& task);

  // content::WebContentsObserver implementation. All called on UI thread.
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void MainFrameWasResized(bool width_changed) override;
  void WebContentsDestroyed() override;

  std::unique_ptr<UiInterface> html_interface_;

  content::WebContents* main_contents_;
  std::unique_ptr<VrCompositor> content_compositor_;
  content::WebContents* ui_contents_;
  std::unique_ptr<VrCompositor> ui_compositor_;

  std::unique_ptr<VrWebContentsObserver> vr_web_contents_observer_;

  VrShellDelegate* delegate_ = nullptr;
  base::android::ScopedJavaGlobalRef<jobject> j_vr_shell_;

  std::unique_ptr<VrInputManager> content_input_manager_;
  std::unique_ptr<VrInputManager> ui_input_manager_;
  std::unique_ptr<VrMetricsHelper> metrics_helper_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<base::Thread> gl_thread_;
  bool reprojected_rendering_;

  base::WeakPtrFactory<VrShell> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShell);
};

bool RegisterVrShell(JNIEnv* env);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_H_
