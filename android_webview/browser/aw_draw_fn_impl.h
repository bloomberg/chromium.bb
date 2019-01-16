// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DRAW_FN_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DRAW_FN_IMPL_H_

#include <deque>

#include "android_webview/browser/compositor_frame_consumer.h"
#include "android_webview/browser/render_thread_manager.h"
#include "android_webview/public/browser/draw_fn.h"
#include "base/android/jni_weak_ref.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"
#include "third_party/vulkan/include/vulkan/vulkan.h"

class GrVkSecondaryCBDrawContext;

namespace gl {
class GLImageAHardwareBuffer;
}

namespace android_webview {
class GLNonOwnedCompatibilityContext;
class VulkanState;

class AwDrawFnImpl {
 public:
  AwDrawFnImpl();
  ~AwDrawFnImpl();

  void ReleaseHandle(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  jint GetFunctorHandle(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  jlong GetCompositorFrameConsumer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  int functor_handle() { return functor_handle_; }
  void OnSync(AwDrawFn_OnSyncParams* params);
  void OnContextDestroyed();
  void DrawGL(AwDrawFn_DrawGLParams* params);
  void InitVk(AwDrawFn_InitVkParams* params);
  void DrawVk(AwDrawFn_DrawVkParams* params);
  void PostDrawVk(AwDrawFn_PostDrawVkParams* params);

 private:
  // Struct which represents one in-flight draw for the Vk interop path.
  struct InFlightDraw {
    explicit InFlightDraw(VulkanState* vk_state);
    ~InFlightDraw();
    sk_sp<GrVkSecondaryCBDrawContext> draw_context;
    VkFence post_draw_fence = VK_NULL_HANDLE;
    VkSemaphore post_draw_semaphore = VK_NULL_HANDLE;
    base::ScopedFD sync_fd;
    scoped_refptr<gl::GLImageAHardwareBuffer> ahb_image;
    sk_sp<SkImage> ahb_skimage;
    uint32_t texture_id = 0;
    uint32_t framebuffer_id = 0;
    GrVkImageInfo image_info;

    // Used to clean up Vulkan objects.
    VulkanState* vk_state;
  };

  CompositorFrameConsumer* GetCompositorFrameConsumer() {
    return &render_thread_manager_;
  }

  std::unique_ptr<InFlightDraw> TakeInFlightDrawForReUse();

  int functor_handle_;
  RenderThreadManager render_thread_manager_;

  // State used for Vk rendering.
  scoped_refptr<VulkanState> vk_state_;

  // GL context used to draw via GL in Vk interop path.
  scoped_refptr<GLNonOwnedCompatibilityContext> gl_context_;

  // Queue of draw contexts pending cleanup.
  std::deque<std::unique_ptr<InFlightDraw>> in_flight_draws_;
  std::unique_ptr<InFlightDraw> pending_draw_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DRAW_FN_IMPL_H_
