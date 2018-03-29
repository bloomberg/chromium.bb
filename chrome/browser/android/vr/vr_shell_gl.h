// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_SHELL_GL_H_
#define CHROME_BROWSER_ANDROID_VR_VR_SHELL_GL_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr/android_vsync_helper.h"
#include "chrome/browser/android/vr/vr_controller.h"
#include "chrome/browser/android/vr/vr_dialog.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/fps_meter.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/sliding_average.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"

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
class GlBrowserInterface;
class MailboxToSurfaceBridge;
class SlidingTimeDeltaAverage;
class Ui;
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
            std::unique_ptr<Ui> ui,
            gvr_context* gvr_api,
            bool reprojected_rendering,
            bool daydream_support,
            bool start_in_web_vr_mode,
            bool pause_content);
  ~VrShellGl() override;

  void Initialize();
  void InitializeGl(gfx::AcceleratedWidget window);

  void OnTriggerEvent(bool pressed);
  void OnPause();
  void OnResume();
  void OnExitPresent();

  base::WeakPtr<BrowserUiInterface> GetBrowserUiWeakPtr();

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return task_runner_;
  }

  void SetWebVrMode(bool enabled);
  void CreateOrResizeWebVRSurface(const gfx::Size& size);
  void ContentBoundsChanged(int width, int height);
  void BufferBoundsChanged(const gfx::Size& content_buffer_size,
                           const gfx::Size& overlay_buffer_size);
  void UIBoundsChanged(int width, int height);
  void ResumeContentRendering();

  base::WeakPtr<VrShellGl> GetWeakPtr();

  void ConnectPresentingService(
      device::mojom::VRSubmitFrameClientPtrInfo submit_client_info,
      device::mojom::VRPresentationProviderRequest request,
      device::mojom::VRDisplayInfoPtr display_info,
      device::mojom::VRRequestPresentOptionsPtr present_options);

  void set_is_exiting(bool exiting) { is_exiting_ = exiting; }

  void OnSwapContents(int new_content_id);

  void EnableAlertDialog(ContentInputForwarder* input_forwarder,
                         float width,
                         float height);
  void DisableAlertDialog();

  void SetAlertDialogSize(float width, float height);
  void SetDialogLocation(float x, float y);
  void SetDialogFloating();

  void AcceptDoffPromptForTesting();

 private:
  void GvrInit(gvr_context* gvr_api);
  device::mojom::VRDisplayFrameTransportOptionsPtr
  GetWebVrFrameTransportOptions();
  void InitializeRenderer();
  void OnGpuProcessConnectionReady();
  // Returns true if successfully resized.
  bool ResizeForWebVR(int16_t frame_index);
  void UpdateSamples();
  void UpdateEyeInfos(const gfx::Transform& head_pose,
                      int viewport_offset,
                      const gfx::Size& render_size,
                      RenderInfo* out_render_info);
  void DrawFrame(int16_t frame_index, base::TimeTicks current_time);
  void DrawIntoAcquiredFrame(int16_t frame_index, base::TimeTicks current_time);
  void DrawFrameSubmitWhenReady(int16_t frame_index,
                                const gfx::Transform& head_pose,
                                std::unique_ptr<gl::GLFenceEGL> fence);
  void DrawFrameSubmitNow(int16_t frame_index, const gfx::Transform& head_pose);
  bool ShouldDrawWebVr();
  void DrawWebVr();
  bool ShouldSendGesturesToWebVr();
  bool WebVrPoseByteIsValid(int pose_index_byte);

  void UpdateController(const RenderInfo& render_info,
                        base::TimeTicks current_time);

  void SendImmediateExitRequestIfNecessary();
  void HandleControllerInput(const gfx::Point3F& laser_origin,
                             const RenderInfo& render_info,
                             base::TimeTicks current_time);
  void HandleControllerAppButtonActivity(
      const gfx::Vector3dF& controller_direction);

  void OnContentFrameAvailable();
  void OnContentOverlayFrameAvailable();
  void OnUiFrameAvailable();
  void OnWebVRFrameAvailable();
  void OnNewWebVRFrame();
  void ScheduleOrCancelWebVrFrameTimeout();
  void OnWebVrTimeoutImminent();
  void OnWebVrFrameTimedOut();

  base::TimeDelta GetPredictedFrameTime();
  void AddWebVrRenderTimeEstimate(int16_t frame_index, bool did_wait);

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

  bool ShouldSkipVSync();
  void SendVSync(base::TimeTicks time, GetVSyncCallback callback);

  // Checks if we're in a valid state for submitting a frame. Invalid states
  // include mailbox_bridge_ready_ being false, or overlapping processing
  // frames.
  bool WebVrCanSubmitFrame();
  // Call this after state changes that could result in WebVrCanSubmitFrame
  // becoming true.
  void WebVrTryDeferredSubmit();

  void ClosePresentationBindings();

  device::mojom::XRInputSourceStatePtr GetGazeInputSourceState();

  // samplerExternalOES texture data for WebVR content image.
  int webvr_texture_id_ = 0;

  // Set from feature flags.
  bool webvr_vsync_align_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::SurfaceTexture> content_surface_texture_;
  scoped_refptr<gl::SurfaceTexture> content_overlay_surface_texture_;
  scoped_refptr<gl::SurfaceTexture> ui_surface_texture_;
  scoped_refptr<gl::SurfaceTexture> webvr_surface_texture_;
  std::unique_ptr<gl::ScopedJavaSurface> content_surface_;
  std::unique_ptr<gl::ScopedJavaSurface> ui_surface_;
  std::unique_ptr<gl::ScopedJavaSurface> content_overlay_surface_;

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
  base::queue<uint16_t> pending_frames_;
  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;
  bool mailbox_bridge_ready_ = false;

  // A fence used to avoid overstuffed GVR buffers in WebVR mode.
  std::unique_ptr<gl::GLFenceEGL> webvr_prev_frame_completion_fence_;

  // The default size for the render buffers.
  gfx::Size render_size_default_;
  gfx::Size render_size_webvr_ui_;

  // WebVR currently supports multiple render path choices, with runtime
  // selection based on underlying support being available and feature flags.
  // The webvr_use_* booleans choose among the implementations. Please don't
  // check WebXrRenderPath or other feature flags in individual code paths
  // directly to avoid inconsistent logic.
  bool webvr_use_gpu_fence_ = false;

  int webvr_unstuff_ratelimit_frames_ = 0;

  bool cardboard_ = false;
  gfx::Quaternion controller_quat_;

  gfx::Size content_tex_buffer_size_ = {0, 0};
  gfx::Size webvr_surface_size_ = {0, 0};

  std::vector<base::TimeTicks> webvr_time_pose_;
  std::vector<base::TimeTicks> webvr_time_js_submit_;
  std::vector<base::TimeTicks> webvr_time_copied_;
  std::vector<bool> webvr_frame_oustanding_;
  std::vector<gfx::Transform> webvr_head_pose_;

  std::unique_ptr<Ui> ui_;

  bool web_vr_mode_ = false;
  bool ready_to_draw_ = false;
  bool paused_ = true;
  const bool surfaceless_rendering_;
  bool daydream_support_;
  bool is_exiting_ = false;
  bool content_paused_;
  bool report_webxr_input_ = false;
  bool cardboard_trigger_pressed_ = false;
  bool cardboard_trigger_clicked_ = false;

  std::unique_ptr<VrController> controller_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Attributes tracking WebVR rAF/VSync animation loop state. Blink schedules
  // a callback using the GetVSync mojo call, and the callback is either passed
  // to SendVSync immediately, or deferred until the next OnVSync call.
  //
  // pending_vsync_ is set to true in OnVSync if there is no current
  // outstanding callback, and this means that a future GetVSync is permitted
  // to execute SendVSync immediately. If it is false, GetVSync must store the
  // pending callback in callback_ for later execution.
  base::TimeTicks pending_time_;
  bool pending_vsync_ = false;
  GetVSyncCallback callback_;

  mojo::Binding<device::mojom::VRPresentationProvider> binding_;
  device::mojom::VRSubmitFrameClientPtr submit_client_;

  GlBrowserInterface* browser_;

  // Index of the next WebXR frame, wrapping from 255 back to 0. Elsewhere we
  // use -1 to indicate a non-WebXR frame, so most internal APIs use int16_t to
  // store the -1..255 range.
  uint8_t next_frame_index_ = 0;

  uint64_t webvr_frames_received_ = 0;

  // Attributes for gesture detection while holding app button.
  gfx::Vector3dF controller_start_direction_;
  base::TimeTicks app_button_down_time_;
  bool app_button_long_pressed_ = false;

  FPSMeter vr_ui_fps_meter_;
  FPSMeter webvr_fps_meter_;

  // JS time is from SendVSync (pose time) to incoming JS submitFrame.
  SlidingTimeDeltaAverage webvr_js_time_;

  // Render time is from JS submitFrame to estimated render completion.
  // This is an estimate when submitting incomplete frames to GVR.
  // If submitFrame blocks, that means the previous frame wasn't done
  // rendering yet.
  SlidingTimeDeltaAverage webvr_render_time_;

  // JS wait time is spent waiting for the previous frame to complete
  // rendering, as reported from the Renderer via mojo.
  SlidingTimeDeltaAverage webvr_js_wait_time_;

  // GVR acquire/submit times for scheduling heuristics.
  SlidingTimeDeltaAverage webvr_acquire_time_;
  SlidingTimeDeltaAverage webvr_submit_time_;

  gfx::Point3F pointer_start_;

  RenderInfo render_info_primary_;

  AndroidVSyncHelper vsync_helper_;

  base::CancelableOnceCallback<void()> webvr_frame_timeout_;
  base::CancelableOnceCallback<void()> webvr_spinner_timeout_;

  // WebVR defers submitting a frame to GVR by scheduling a closure
  // for later. If we exit WebVR before it is executed, we need to
  // cancel it to avoid inconsistent state.
  base::CancelableCallback<
      void(int16_t, const gfx::Transform&, std::unique_ptr<gl::GLFenceEGL>)>
      webvr_delayed_gvr_submit_;

  // We only want one frame at a time in the lifecycle from
  // mojo SubmitFrame until we submit to GVR. This flag is true
  // for that timespan.
  bool webvr_frame_processing_ = false;

  // If we receive a new SubmitFrame when we're not ready, save it for
  // later execution.
  base::OnceClosure webvr_deferred_mojo_submit_;

  std::vector<gvr::BufferSpec> specs_;

  bool content_frame_available_ = false;
  gfx::Transform last_used_head_pose_;

  ControllerModel controller_model_;

  std::unique_ptr<VrDialog> vr_dialog_;
  bool showing_vr_dialog_ = false;

  bool last_should_send_webvr_vsync_ = false;

  base::WeakPtrFactory<VrShellGl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VrShellGl);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_SHELL_GL_H_
