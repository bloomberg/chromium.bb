// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_GL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_GL_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr_shell/android_vsync_helper.h"
#include "chrome/browser/android/vr_shell/vr_controller.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/controller_mesh.h"
#include "chrome/browser/vr/fps_meter.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/sliding_average.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class Version;
}  // namespace base

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
class BrowserUiInterface;
class FPSMeter;
class SlidingTimeDeltaAverage;
class Ui;
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
class VrShellGl : public device::mojom::VRPresentationProvider {
 public:
  VrShellGl(GlBrowserInterface* browser_interface,
            std::unique_ptr<vr::Ui> ui,
            gvr_context* gvr_api,
            bool reprojected_rendering,
            bool daydream_support,
            bool start_in_web_vr_mode);
  ~VrShellGl() override;

  void Initialize();
  void InitializeGl(gfx::AcceleratedWidget window);

  void OnTriggerEvent();
  void OnPause();
  void OnResume();

  base::WeakPtr<vr::BrowserUiInterface> GetBrowserUiWeakPtr();

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

  void SetControllerMesh(std::unique_ptr<vr::ControllerMesh> mesh);

  void ConnectPresentingService(
      device::mojom::VRSubmitFrameClientPtrInfo submit_client_info,
      device::mojom::VRPresentationProviderRequest request,
      device::mojom::VRDisplayInfoPtr display_info);

  void set_is_exiting(bool exiting) { is_exiting_ = exiting; }

  void OnSwapContents(int new_content_id);

 private:
  void GvrInit(gvr_context* gvr_api);
  void InitializeRenderer();
  // Returns true if successfully resized.
  bool ResizeForWebVR(int16_t frame_index);
  void UpdateSamples();
  void UpdateEyeInfos(const gfx::Transform& head_pose,
                      int viewport_offset,
                      const gfx::Size& render_size,
                      vr::RenderInfo* out_render_info);
  void DrawFrame(int16_t frame_index, base::TimeTicks current_time);
  void DrawIntoAcquiredFrame(int16_t frame_index, base::TimeTicks current_time);
  void DrawFrameSubmitWhenReady(int16_t frame_index,
                                const gfx::Transform& head_pose,
                                std::unique_ptr<gl::GLFenceEGL> fence);
  void DrawFrameSubmitNow(int16_t frame_index, const gfx::Transform& head_pose);
  bool ShouldDrawWebVr();
  void DrawWebVr();
  bool WebVrPoseByteIsValid(int pose_index_byte);

  void UpdateController(const gfx::Transform& head_pose,
                        base::TimeTicks current_time);

  void SendImmediateExitRequestIfNecessary();
  void HandleControllerInput(const gfx::Point3F& laser_origin,
                             const gfx::Vector3dF& head_direction,
                             base::TimeTicks current_time);
  void HandleControllerAppButtonActivity(
      const gfx::Vector3dF& controller_direction);

  void OnContentFrameAvailable();
  void OnWebVRFrameAvailable();
  void ScheduleOrCancelWebVrFrameTimeout();
  void OnWebVrTimeoutImminent();
  void OnWebVrFrameTimedOut();

  base::TimeDelta GetPredictedFrameTime();

  void OnVSync(base::TimeTicks frame_time);

  // VRPresentationProvider
  void GetVSync(GetVSyncCallback callback) override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox,
                   base::TimeDelta time_waited) override;
  void SubmitFrameWithTextureHandle(int16_t frame_index,
                                    mojo::ScopedHandle texture_handle) override;
  void UpdateLayerBounds(int16_t frame_index,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;

  void ForceExitVr();

  void SendVSync(base::TimeTicks time, GetVSyncCallback callback);

  void ClosePresentationBindings();

  void OnAssetsLoaded(bool success,
                      std::string environment,
                      const base::Version& component_version);

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
  gvr::Frame acquired_frame_;
  base::queue<std::pair<uint8_t, WebVrBounds>> pending_bounds_;
  int premature_received_frames_ = 0;
  base::queue<uint16_t> pending_frames_;
  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;

  // The default size for the render buffers.
  gfx::Size render_size_default_;
  gfx::Size render_size_webvr_ui_;

  bool cardboard_ = false;
  gfx::Quaternion controller_quat_;

  gfx::Size content_tex_physical_size_ = {0, 0};
  gfx::Size webvr_surface_size_ = {0, 0};

  std::vector<base::TimeTicks> webvr_time_pose_;
  std::vector<base::TimeTicks> webvr_time_js_submit_;
  std::vector<bool> webvr_frame_oustanding_;
  std::vector<gfx::Transform> webvr_head_pose_;

  std::unique_ptr<vr::Ui> ui_;

  bool web_vr_mode_ = false;
  bool ready_to_draw_ = false;
  bool paused_ = true;
  const bool surfaceless_rendering_;
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

  uint8_t frame_index_ = 0;
  // Larger than frame_index_ so it can be initialized out-of-band.
  uint16_t last_frame_index_ = -1;

  uint64_t webvr_frames_received_ = 0;

  // Attributes for gesture detection while holding app button.
  gfx::Vector3dF controller_start_direction_;

  vr::FPSMeter fps_meter_;

  // JS time is from SendVSync (pose time) to incoming JS submitFrame.
  vr::SlidingTimeDeltaAverage webvr_js_time_;

  // Render time is from JS submitFrame to estimated render completion.
  // This is an estimate when submitting incomplete frames to GVR.
  // If submitFrame blocks, that means the previous frame wasn't done
  // rendering yet.
  vr::SlidingTimeDeltaAverage webvr_render_time_;

  // JS wait time is spent waiting for the previous frame to complete
  // rendering, as reported from the Renderer via mojo.
  vr::SlidingTimeDeltaAverage webvr_js_wait_time_;

  // GVR acquire/submit times for scheduling heuristics.
  vr::SlidingTimeDeltaAverage webvr_acquire_time_;
  vr::SlidingTimeDeltaAverage webvr_submit_time_;

  gfx::Point3F pointer_start_;

  vr::RenderInfo render_info_primary_;

  AndroidVSyncHelper vsync_helper_;

  base::CancelableCallback<void()> webvr_frame_timeout_;
  base::CancelableCallback<void()> webvr_spinner_timeout_;
  base::CancelableCallback<
      void(int16_t, const gfx::Transform&, std::unique_ptr<gl::GLFenceEGL>)>
      webvr_delayed_frame_submit_;

  std::vector<gvr::BufferSpec> specs_;

  bool content_frame_available_ = false;
  gfx::Transform last_used_head_pose_;

  vr::ControllerModel controller_model_;

  base::WeakPtrFactory<VrShellGl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShellGl);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_SHELL_GL_H_
