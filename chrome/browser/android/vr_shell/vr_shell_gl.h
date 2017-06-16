// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_GL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_GL_H_

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr_shell/vr_controller.h"
#include "chrome/browser/android/vr_shell/vr_controller_model.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

namespace blink {
class WebMouseEvent;
}

namespace gl {
class GLContext;
class GLFence;
class GLSurface;
class ScopedJavaSurface;
class SurfaceTexture;
}

namespace gpu {
struct MailboxHolder;
}

namespace vr_shell {

class FPSMeter;
class MailboxToSurfaceBridge;
class SlidingAverage;
class UiElement;
class UiScene;
class GlBrowserInterface;
class VrController;
class VrShell;
class VrShellRenderer;

struct WebVrBounds {
  WebVrBounds(const gfx::RectF& left,
              const gfx::RectF& right,
              const gfx::Size& size)
      : left_bounds(left), right_bounds(right), source_size(size) {}
  gfx::RectF left_bounds;
  gfx::RectF right_bounds;
  gfx::Size source_size;
};

// This class manages all GLThread owned objects and GL rendering for VrShell.
// It is not threadsafe and must only be used on the GL thread.
class VrShellGl : public device::mojom::VRPresentationProvider {
 public:
  VrShellGl(GlBrowserInterface* browser,
            gvr_context* gvr_api,
            bool initially_web_vr,
            bool reprojected_rendering,
            bool daydream_support,
            UiScene* scene);
  ~VrShellGl() override;

  void Initialize();
  void InitializeGl(gfx::AcceleratedWidget window);

  void OnTriggerEvent();
  void OnPause();
  void OnResume();

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

  void SetWebVrMode(bool enabled);
  void CreateOrResizeWebVRSurface(const gfx::Size& size);
  void CreateContentSurface();
  void ContentBoundsChanged(int width, int height);
  void ContentPhysicalBoundsChanged(int width, int height);
  void UIBoundsChanged(int width, int height);
  void UIPhysicalBoundsChanged(int width, int height);
  base::WeakPtr<VrShellGl> GetWeakPtr();

  void SetControllerModel(std::unique_ptr<VrControllerModel> model);

  void UpdateVSyncInterval(int64_t timebase_nanos, double interval_seconds);

  void CreateVRDisplayInfo(
      const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
      uint32_t device_id);
  void ConnectPresentingService(
      device::mojom::VRSubmitFrameClientPtrInfo submit_client_info,
      device::mojom::VRPresentationProviderRequest request);

 private:
  void GvrInit(gvr_context* gvr_api);
  void InitializeRenderer();
  void DrawFrame(int16_t frame_index);
  void DrawFrameSubmitWhenReady(int16_t frame_index,
                                gvr_frame* frame_ptr,
                                const gfx::Transform& head_pose,
                                std::unique_ptr<gl::GLFence> fence);
  void DrawWorldElements(const gfx::Transform& head_pose);
  void DrawOverlayElements(const gfx::Transform& head_pose);
  void DrawHeadLockedElements();
  void DrawUiView(const gfx::Transform& head_pose,
                  const std::vector<const UiElement*>& elements,
                  const gfx::Size& render_size,
                  int viewport_offset,
                  bool draw_cursor);
  void DrawElements(const gfx::Transform& view_proj_matrix,
                    const std::vector<const UiElement*>& elements,
                    bool draw_cursor);
  void DrawElement(const gfx::Transform& view_proj_matrix,
                   const UiElement& element);
  std::vector<const UiElement*> GetElementsInDrawOrder(
      const gfx::Transform& view_matrix,
      const std::vector<const UiElement*>& elements);
  void DrawReticle(const gfx::Transform& view_proj_matrix);
  void DrawLaser(const gfx::Transform& view_proj_matrix);
  void DrawController(const gfx::Transform& view_proj_matrix);
  bool ShouldDrawWebVr();
  void DrawWebVr();
  bool WebVrPoseByteIsValid(int pose_index_byte);

  void UpdateController(const gfx::Vector3dF& head_direction);
  void HandleWebVrCompatibilityClick();
  void SendFlingCancel(GestureList& gesture_list);
  void SendScrollEnd(GestureList& gesture_list);
  bool SendScrollBegin(UiElement* target, GestureList& gesture_list);
  void SendScrollUpdate(GestureList& gesture_list);
  void SendHoverLeave(UiElement* target);
  bool SendHoverEnter(UiElement* target,
                      const gfx::PointF& target_point,
                      const gfx::Point& local_point_pixels);
  void SendHoverMove(const gfx::PointF& target_point,
                     const gfx::Point& local_point_pixels);
  void SendButtonDown(UiElement* target,
                      const gfx::PointF& target_point,
                      const gfx::Point& local_point_pixels);
  bool SendButtonUp(UiElement* target,
                    const gfx::PointF& target_point,
                    const gfx::Point& local_point_pixels);
  void SendTap(UiElement* target,
               const gfx::PointF& target_point,
               const gfx::Point& local_point_pixels);
  std::unique_ptr<blink::WebMouseEvent> MakeMouseEvent(
      blink::WebInputEvent::Type type,
      const gfx::Point& location);
  void SendImmediateExitRequestIfNecessary();
  void GetVisualTargetElement(const gfx::Vector3dF& controller_direction,
                              gfx::Vector3dF& eye_to_target,
                              gfx::Point3F& target_point,
                              UiElement** target_element,
                              gfx::PointF& target_local_point) const;
  bool GetTargetLocalPoint(const gfx::Vector3dF& eye_to_target,
                           const UiElement& element,
                           float max_distance_to_plane,
                           gfx::PointF& target_local_point,
                           gfx::Point3F& target_point,
                           float& distance_to_plane) const;
  void HandleControllerInput(const gfx::Vector3dF& head_direction);
  void HandleControllerAppButtonActivity(
      const gfx::Vector3dF& controller_direction);
  void SendGestureToContent(std::unique_ptr<blink::WebInputEvent> event);
  void CreateUiSurface();
  void OnContentFrameAvailable();
  void OnWebVRFrameAvailable();
  int64_t GetPredictedFrameTimeNanos();

  void OnVSync();

  // VRPresentationProvider
  void GetVSync(GetVSyncCallback callback) override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox) override;
  void UpdateLayerBounds(int16_t frame_index,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;

  void ForceExitVr();

  void SendVSync(base::TimeDelta time, GetVSyncCallback callback);

  // samplerExternalOES texture data for main content image.
  int content_texture_id_ = 0;
  // samplerExternalOES texture data for WebVR content image.
  int webvr_texture_id_ = 0;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::SurfaceTexture> content_surface_texture_;
  scoped_refptr<gl::SurfaceTexture> webvr_surface_texture_;

  std::unique_ptr<gl::ScopedJavaSurface> content_surface_;

  std::unique_ptr<gvr::GvrApi> gvr_api_;
  std::unique_ptr<gvr::BufferViewportList> buffer_viewport_list_;
  std::unique_ptr<gvr::BufferViewport> buffer_viewport_;
  std::unique_ptr<gvr::BufferViewport> headlocked_left_viewport_;
  std::unique_ptr<gvr::BufferViewport> headlocked_right_viewport_;
  std::unique_ptr<gvr::BufferViewport> webvr_left_viewport_;
  std::unique_ptr<gvr::BufferViewport> webvr_right_viewport_;
  std::unique_ptr<gvr::SwapChain> swap_chain_;
  std::queue<std::pair<uint8_t, WebVrBounds>> pending_bounds_;
  int premature_received_frames_ = 0;
  std::queue<uint16_t> pending_frames_;
  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;

  // Current sizes for the render buffers.
  gfx::Size render_size_primary_;
  gfx::Size render_size_headlocked_;

  // Intended render_size_primary_ for use by VrShell, so that it
  // can be restored after exiting WebVR mode.
  gfx::Size render_size_vrshell_;

  std::unique_ptr<VrShellRenderer> vr_shell_renderer_;

  bool cardboard_ = false;
  bool touch_pending_ = false;
  gfx::Quaternion controller_quat_;

  gfx::Point3F target_point_;

  // TODO(mthiesse): We need to handle elements being removed, and update this
  // state appropriately.
  UiElement* reticle_render_target_ = nullptr;
  UiElement* hover_target_ = nullptr;
  // TODO(mthiesse): We shouldn't have a fling target. Elements should fling
  // independently and we should only cancel flings on the relevant element
  // when we do cancel flings.
  UiElement* fling_target_ = nullptr;
  UiElement* input_locked_element_ = nullptr;
  bool in_scroll_ = false;
  bool in_click_ = false;

  int content_tex_css_width_ = 0;
  int content_tex_css_height_ = 0;
  gfx::Size content_tex_physical_size_ = {0, 0};
  gfx::Size webvr_surface_size_ = {0, 0};

  std::vector<base::TimeTicks> webvr_time_pose_;
  std::vector<base::TimeTicks> webvr_time_js_submit_;

  std::vector<gfx::Transform> webvr_head_pose_;

  bool web_vr_mode_;
  bool ready_to_draw_ = false;
  bool surfaceless_rendering_;
  bool daydream_support_;

  std::unique_ptr<VrController> controller_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::CancelableClosure vsync_task_;
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;

  base::TimeDelta pending_time_;
  bool pending_vsync_ = false;
  GetVSyncCallback callback_;
  bool received_frame_ = false;
  mojo::Binding<device::mojom::VRPresentationProvider> binding_;
  device::mojom::VRSubmitFrameClientPtr submit_client_;

  GlBrowserInterface* browser_;

  UiScene* scene_ = nullptr;

  uint8_t frame_index_ = 0;
  // Larger than frame_index_ so it can be initialized out-of-band.
  uint16_t last_frame_index_ = -1;

  // Attributes for gesture detection while holding app button.
  gfx::Vector3dF controller_start_direction_;

  std::unique_ptr<FPSMeter> fps_meter_;

  std::unique_ptr<SlidingAverage> webvr_js_time_;
  std::unique_ptr<SlidingAverage> webvr_render_time_;

  gfx::Point3F pointer_start_;

  base::WeakPtrFactory<VrShellGl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShellGl);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_GL_H_
