// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_GVR_SCHEDULER_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_VR_GVR_SCHEDULER_DELEGATE_H_

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/android/vr/android_vsync_helper.h"
#include "chrome/browser/android/vr/gvr_graphics_delegate.h"
#include "chrome/browser/android/vr/web_xr_presentation_state.h"
#include "chrome/browser/vr/fps_meter.h"
#include "chrome/browser/vr/scheduler_delegate.h"
#include "chrome/browser/vr/sliding_average.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/gfx/transform.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gfx {
class GpuFence;
}

namespace gl {
class GLFenceAndroidNativeFenceSync;
class GLFenceEGL;
class SurfaceTexture;
}  // namespace gl

namespace gvr {
class GvrApi;
}

namespace vr {

class GlBrowserInterface;
class MailboxToSurfaceBridge;
class SchedulerUiInterface;
class ScopedGpuTrace;
class SlidingTimeDeltaAverage;

// Apart from scheduling, this class implements the XR providers and the
// transport logic.
class GvrSchedulerDelegate : public SchedulerDelegate,
                             device::mojom::XRPresentationProvider,
                             device::mojom::XRFrameDataProvider {
 public:
  GvrSchedulerDelegate(GlBrowserInterface* browser,
                       SchedulerUiInterface* ui,
                       gvr::GvrApi* gvr_api,
                       GvrGraphicsDelegate* graphics,
                       bool start_in_web_xr_mode,
                       bool cardboard_gamepad,
                       size_t sliding_time_size);
  ~GvrSchedulerDelegate() override;

  WebXrPresentationState* webxr() { return &webxr_; }

 private:
  // SchedulerDelegate overrides.
  gfx::Transform GetHeadPose() override;
  void AddInputSourceState(device::mojom::XRInputSourceStatePtr state) override;
  void OnPause() override;
  void OnResume() override;
  void OnExitPresent() override;
  void OnTriggerEvent(bool pressed) override;
  void SetWebXrMode(bool enabled) override;
  void SetShowingVrDialog(bool showing) override;
  void SetBrowserRenderer(
      SchedulerBrowserRendererInterface* browser_renderer) override;
  void ConnectPresentingService(
      device::mojom::VRDisplayInfoPtr display_info,
      device::mojom::XRRuntimeSessionOptionsPtr options) override;

  void GvrInit();
  void ScheduleOrCancelWebVrFrameTimeout();
  bool CanSendWebXrVSync() const;
  void OnWebXrTimeoutImminent();
  void OnWebXrFrameTimedOut();
  void OnVSync(base::TimeTicks frame_time);
  void DrawFrame(int16_t frame_index, base::TimeTicks current_time);
  void WebVrSendRenderNotification(bool was_rendered);
  void UpdatePendingBounds(int16_t frame_index);
  void SubmitDrawnFrame(bool is_webxr_frame);
  void DrawFrameSubmitWhenReady(bool is_webxr_frame,
                                const gfx::Transform& head_pose,
                                std::unique_ptr<gl::GLFenceEGL> fence);
  void DrawFrameSubmitNow(bool is_webxr_frame, const gfx::Transform& head_pose);

  // Used for discarding unwanted WebXR frames while UI is showing. We can't
  // safely cancel frames from processing start until they show up in
  // OnWebXrFrameAvailable, so only support cancelling them before or after
  // that lifecycle segment.
  void WebXrCancelAnimatingFrame();
  void WebXrCancelProcessingFrameAfterTransfer();

  // Sends a GetFrameData response to the presentation client.
  void SendVSync();
  void WebXrPrepareSharedBuffer();
  void WebXrCreateOrResizeSharedBufferImage(WebXrSharedBuffer* buffer,
                                            const gfx::Size& size);

  // Checks if we're in a valid state for starting animation of a new frame.
  // Invalid states include a previous animating frame that's not complete
  // yet (including deferred processing not having started yet), or timing
  // heuristics indicating that it should be retried later.
  bool WebVrCanAnimateFrame(bool is_from_onvsync);
  // Call this after state changes that could result in WebVrCanAnimateFrame
  // becoming true.
  void WebXrTryStartAnimatingFrame(bool is_from_onvsync);

  bool ShouldDrawWebVr();

  // Heuristics to avoid excessive backlogged frames.
  bool WebVrHasSlowRenderingFrame();
  bool WebVrHasOverstuffedBuffers();

  base::TimeDelta GetPredictedFrameTime();

  device::mojom::XRInputSourceStatePtr GetGazeInputSourceState();

  device::mojom::XRPresentationTransportOptionsPtr
  GetWebXrFrameTransportOptions(
      const device::mojom::XRRuntimeSessionOptionsPtr&);
  void CreateOrResizeWebXrSurface(const gfx::Size& size);
  void OnWebXrFrameAvailable();
  void CreateSurfaceBridge(gl::SurfaceTexture* surface_texture);
  void OnGpuProcessConnectionReady();

  void ClosePresentationBindings();

  // XRFrameDataProvider
  void GetFrameData(device::mojom::XRFrameDataProvider::GetFrameDataCallback
                        callback) override;

  // XRPresentationProvider
  void SubmitFrameMissing(int16_t frame_index, const gpu::SyncToken&) override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox,
                   base::TimeDelta time_waited) override;
  void SubmitFrameWithTextureHandle(int16_t frame_index,
                                    mojo::ScopedHandle texture_handle) override;
  void SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                   const gpu::SyncToken&,
                                   base::TimeDelta time_waited) override;
  void UpdateLayerBounds(int16_t frame_index,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;

  // Shared logic for SubmitFrame variants, including sanity checks.
  // Returns true if OK to proceed.
  bool SubmitFrameCommon(int16_t frame_index, base::TimeDelta time_waited);
  bool IsSubmitFrameExpected(int16_t frame_index);

  void OnNewWebXrFrame();
  void OnWebXrTokenSignaled(int16_t frame_index,
                            std::unique_ptr<gfx::GpuFence> gpu_fence);

  // Transition a frame from animating to processing.
  void ProcessWebVrFrameFromMailbox(int16_t frame_index,
                                    const gpu::MailboxHolder& mailbox);
  void ProcessWebVrFrameFromGMB(int16_t frame_index,
                                const gpu::SyncToken& sync_token);

  void AddWebVrRenderTimeEstimate(base::TimeTicks fence_complete_time);

  GlBrowserInterface* browser_;
  gvr::GvrApi* gvr_api_;

  SchedulerUiInterface* ui_ = nullptr;
  SchedulerBrowserRendererInterface* browser_renderer_ = nullptr;

  // Set from feature flags.
  const bool webvr_vsync_align_;

  WebXrPresentationState webxr_;
  bool web_xr_mode_ = false;
  bool showing_vr_dialog_ = false;
  bool cardboard_gamepad_ = false;
  // TODO(acondor): Move trigger data to controller delegate.
  bool cardboard_trigger_pressed_ = false;
  bool cardboard_trigger_clicked_ = false;

  // WebXR currently supports multiple render path choices, with runtime
  // selection based on underlying support being available and feature flags.
  // The webxr_use_* booleans choose among the implementations. Please don't
  // check WebXrRenderPath or other feature flags in individual code paths
  // directly to avoid inconsistent logic.
  bool webxr_use_gpu_fence_ = false;
  bool webxr_use_shared_buffer_draw_ = false;

  AndroidVSyncHelper vsync_helper_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::CancelableOnceClosure webvr_frame_timeout_;
  base::CancelableOnceClosure webvr_spinner_timeout_;

  gfx::Transform head_pose_;

  mojo::Binding<device::mojom::XRPresentationProvider> presentation_binding_;
  mojo::Binding<device::mojom::XRFrameDataProvider> frame_data_binding_;

  std::vector<device::mojom::XRInputSourceStatePtr> input_states_;
  device::mojom::XRPresentationClientPtr submit_client_;
  uint64_t webvr_frames_received_ = 0;
  base::queue<uint16_t> pending_frames_;

  base::queue<std::pair<WebXrPresentationState::FrameIndexType, WebVrBounds>>
      pending_bounds_;

  int webvr_unstuff_ratelimit_frames_ = 0;

  GvrGraphicsDelegate* graphics_;

  // Attributes tracking WebVR rAF/VSync animation loop state. Blink schedules
  // a callback using the GetFrameData mojo call which is stored in
  // get_frame_data_callback_. The callback is executed by SendVSync once
  // WebVrCanAnimateFrame returns true.
  //
  // pending_vsync_ is set to true in OnVSync and false in SendVSync. It
  // throttles animation to no faster than the VSync rate. The pending_time_ is
  // updated in OnVSync and used as the rAF animation timer in SendVSync.
  base::TimeTicks pending_time_;
  bool pending_vsync_ = false;
  device::mojom::XRFrameDataProvider::GetFrameDataCallback
      get_frame_data_callback_;

  // A fence used to avoid overstuffed GVR buffers in WebVR mode.
  std::unique_ptr<gl::GLFenceAndroidNativeFenceSync>
      webvr_prev_frame_completion_fence_;

  // WebXR defers submitting a frame to GVR by scheduling a closure
  // for later. If we exit WebVR before it is executed, we need to
  // cancel it to avoid inconsistent state.
  base::CancelableOnceCallback<
      void(bool, const gfx::Transform&, std::unique_ptr<gl::GLFenceEGL>)>
      webxr_delayed_gvr_submit_;

  std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge_;
  std::unique_ptr<ScopedGpuTrace> gpu_trace_;

  FPSMeter vr_ui_fps_meter_;
  FPSMeter webxr_fps_meter_;

  // Render time is from JS submitFrame to estimated render completion.
  // This is an estimate when submitting incomplete frames to GVR.
  // If submitFrame blocks, that means the previous frame wasn't done
  // rendering yet.
  SlidingTimeDeltaAverage webvr_render_time_;
  // JS time is from SendVSync (pose time) to incoming JS submitFrame.
  SlidingTimeDeltaAverage webvr_js_time_;
  // JS wait time is spent waiting for the previous frame to complete
  // rendering, as reported from the Renderer via mojo.
  SlidingTimeDeltaAverage webvr_js_wait_time_;

  base::WeakPtrFactory<GvrSchedulerDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GvrSchedulerDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_GVR_SCHEDULER_DELEGATE_H_
