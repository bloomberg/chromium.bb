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
#include "chrome/browser/android/vr_shell/android_vsync_helper.h"
#include "chrome/browser/android/vr_shell/vr_controller.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/vr_controller_model.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"

namespace blink {
class WebMouseEvent;
}  // namespace blink

namespace gl {
class GLContext;
class GLFenceEGL;
class GLSurface;
class ScopedJavaSurface;
class SurfaceTexture;
}  // namespace gl

namespace gpu {
struct MailboxHolder;
}  // namespace gpu

namespace vr {
class FPSMeter;
class SlidingAverage;
class UiScene;
class VrShellRenderer;
}  // namespace vr

namespace vr_shell {

class MailboxToSurfaceBridge;
class GlBrowserInterface;
class VrController;
class VrShell;

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
class VrShellGl : public device::mojom::VRPresentationProvider,
                  public vr::ContentInputDelegate {
 public:
  VrShellGl(GlBrowserInterface* browser,
            gvr_context* gvr_api,
            bool initially_web_vr,
            bool reprojected_rendering,
            bool daydream_support,
            vr::UiScene* scene);
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

  void SetControllerModel(std::unique_ptr<vr::VrControllerModel> model);

  void CreateVRDisplayInfo(
      const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
      uint32_t device_id);
  void ConnectPresentingService(
      device::mojom::VRSubmitFrameClientPtrInfo submit_client_info,
      device::mojom::VRPresentationProviderRequest request);

  void set_is_exiting(bool exiting) { is_exiting_ = exiting; }

 private:
  void GvrInit(gvr_context* gvr_api);
  void InitializeRenderer();
  void DrawFrame(int16_t frame_index);
  void DrawFrameSubmitWhenReady(int16_t frame_index,
                                gvr_frame* frame_ptr,
                                const gfx::Transform& head_pose,
                                std::unique_ptr<gl::GLFenceEGL> fence);
  bool ShouldDrawWebVr();
  void DrawWebVr();
  bool WebVrPoseByteIsValid(int pose_index_byte);

  void UpdateController(const gfx::Transform& head_pose);
  std::unique_ptr<blink::WebMouseEvent> MakeMouseEvent(
      blink::WebInputEvent::Type type,
      const gfx::PointF& normalized_web_content_location);
  void UpdateGesture(const gfx::PointF& normalized_content_hit_point,
                     blink::WebGestureEvent& gesture);

  // vr::ContentInputDelegate.
  void OnContentEnter(const gfx::PointF& normalized_hit_point) override;
  void OnContentLeave() override;
  void OnContentMove(const gfx::PointF& normalized_hit_point) override;
  void OnContentDown(const gfx::PointF& normalized_hit_point) override;
  void OnContentUp(const gfx::PointF& normalized_hit_point) override;
  void OnContentFlingStart(std::unique_ptr<blink::WebGestureEvent> gesture,
                           const gfx::PointF& normalized_hit_point) override;
  void OnContentFlingCancel(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& normalized_hit_point) override;
  void OnContentScrollBegin(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& normalized_hit_point) override;
  void OnContentScrollUpdate(std::unique_ptr<blink::WebGestureEvent> gesture,
                             const gfx::PointF& normalized_hit_point) override;
  void OnContentScrollEnd(std::unique_ptr<blink::WebGestureEvent> gesture,
                          const gfx::PointF& normalized_hit_point) override;

  void SendImmediateExitRequestIfNecessary();
  void HandleControllerInput(const gfx::Vector3dF& head_direction);
  void HandleControllerAppButtonActivity(
      const gfx::Vector3dF& controller_direction);
  void SendGestureToContent(std::unique_ptr<blink::WebInputEvent> event);
  void CreateUiSurface();

  void OnContentFrameAvailable();
  void OnWebVRFrameAvailable();
  void ScheduleWebVrFrameTimeout();
  void OnWebVrFrameTimedOut();

  int64_t GetPredictedFrameTimeNanos();

  void OnVSync(base::TimeTicks frame_time);

  void UpdateEyeInfos(const gfx::Transform& head_pose,
                      int viewport_offset,
                      const gfx::Size& render_size,
                      vr::RenderInfo* out_render_info);

  // VRPresentationProvider
  void GetVSync(GetVSyncCallback callback) override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox) override;
  void UpdateLayerBounds(int16_t frame_index,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;

  void ForceExitVr();

  void SendVSync(base::TimeTicks time, GetVSyncCallback callback);

  void closePresentationBindings();

  // samplerExternalOES texture data for main content image.
  int content_texture_id_ = 0;
  // samplerExternalOES texture data for WebVR content image.
  int webvr_texture_id_ = 0;

  // Set from feature flag.
  bool webvr_vsync_align_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::SurfaceTexture> content_surface_texture_;
  scoped_refptr<gl::SurfaceTexture> webvr_surface_texture_;

  std::unique_ptr<gl::ScopedJavaSurface> content_surface_;

  std::unique_ptr<gvr::GvrApi> gvr_api_;
  std::unique_ptr<gvr::BufferViewportList> buffer_viewport_list_;
  std::unique_ptr<gvr::BufferViewport> buffer_viewport_;
  std::unique_ptr<gvr::BufferViewport> webvr_browser_ui_left_viewport_;
  std::unique_ptr<gvr::BufferViewport> webvr_browser_ui_right_viewport_;
  std::unique_ptr<gvr::BufferViewport> webvr_left_viewport_;
  std::unique_ptr<gvr::BufferViewport> webvr_right_viewport_;
  std::unique_ptr<gvr::SwapChain> swap_chain_;
  std::queue<std::pair<uint8_t, WebVrBounds>> pending_bounds_;
  int premature_received_frames_ = 0;
  std::queue<uint16_t> pending_frames_;
  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;

  // The default size for the render buffers.
  gfx::Size render_size_default_;
  gfx::Size render_size_webvr_ui_;

  std::unique_ptr<vr::VrShellRenderer> vr_shell_renderer_;

  bool cardboard_ = false;
  gfx::Quaternion controller_quat_;

  int content_tex_css_width_ = 0;
  int content_tex_css_height_ = 0;
  gfx::Size content_tex_physical_size_ = {0, 0};
  gfx::Size webvr_surface_size_ = {0, 0};

  std::vector<base::TimeTicks> webvr_time_pose_;
  std::vector<base::TimeTicks> webvr_time_js_submit_;
  std::vector<bool> webvr_frame_oustanding_;
  std::vector<gfx::Transform> webvr_head_pose_;

  bool web_vr_mode_;
  bool ready_to_draw_ = false;
  bool paused_ = true;
  bool surfaceless_rendering_;
  bool daydream_support_;
  bool is_exiting_ = false;

  std::unique_ptr<VrController> controller_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::TimeTicks pending_time_;
  bool pending_vsync_ = false;
  GetVSyncCallback callback_;
  mojo::Binding<device::mojom::VRPresentationProvider> binding_;
  device::mojom::VRSubmitFrameClientPtr submit_client_;

  GlBrowserInterface* browser_;

  vr::UiScene* scene_ = nullptr;

  uint8_t frame_index_ = 0;
  // Larger than frame_index_ so it can be initialized out-of-band.
  uint16_t last_frame_index_ = -1;

  uint64_t webvr_frames_received_ = 0;

  // Attributes for gesture detection while holding app button.
  gfx::Vector3dF controller_start_direction_;

  std::unique_ptr<vr::FPSMeter> fps_meter_;

  std::unique_ptr<vr::SlidingAverage> webvr_js_time_;
  std::unique_ptr<vr::SlidingAverage> webvr_render_time_;

  gfx::Point3F pointer_start_;

  std::unique_ptr<vr::UiInputManager> input_manager_;
  std::unique_ptr<vr::UiRenderer> ui_renderer_;

  vr::ControllerInfo controller_info_;
  vr::RenderInfo render_info_primary_;
  vr::RenderInfo render_info_webvr_browser_ui_;

  AndroidVSyncHelper vsync_helper_;

  base::CancelableCallback<void()> webvr_frame_timeout_;

  base::WeakPtrFactory<VrShellGl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShellGl);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_GL_H_
