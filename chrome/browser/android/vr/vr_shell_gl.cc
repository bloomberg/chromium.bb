// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_shell_gl.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>

#include "base/android/jni_android.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/containers/queue.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event_argument.h"
#include "chrome/browser/android/vr/gl_browser_interface.h"
#include "chrome/browser/android/vr/gvr_util.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "chrome/browser/android/vr/vr_controller.h"
#include "chrome/browser/android/vr/vr_metrics_util.h"
#include "chrome/browser/android/vr/vr_shell.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/metrics/session_metrics_helper.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/pose_util.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_gamepad_data_provider.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace vr {

namespace {
constexpr float kZNear = 0.1f;
constexpr float kZFar = 10000.0f;

// GVR buffer indices for use with viewport->SetSourceBufferIndex
// or frame.BindBuffer. We use one for world content (with reprojection)
// including main VrShell and WebVR content plus world-space UI.
constexpr int kFramePrimaryBuffer = 0;
constexpr int kFrameWebVrBrowserUiBuffer = 1;

// When display UI on top of WebVR, we use a seperate buffer. Normally, the
// buffer is set to recommended size to get best visual (i.e the buffer for
// rendering ChromeVR). We divide the recommended buffer size by this number to
// improve performance.
// We calculate a smaller FOV and UV per frame which includes all visible
// elements. This allows us rendering UI at the same quality with a smaller
// buffer.
// Use 2 for now, we can probably make the buffer even smaller.
constexpr float kWebVrBrowserUiSizeFactor = 2.f;

// The GVR viewport list has two entries (left eye and right eye) for each
// GVR buffer.
constexpr int kViewportListPrimaryOffset = 0;
constexpr int kViewportListWebVrBrowserUiOffset = 2;

// Buffer size large enough to handle the current backlog of poses which is
// 2-3 frames.
constexpr unsigned kPoseRingBufferSize = 8;

// Number of frames to use for sliding averages for pose timings,
// as used for estimating prediction times.
constexpr unsigned kWebVRSlidingAverageSize = 5;

// Criteria for considering holding the app button in combination with
// controller movement as a gesture.
constexpr float kMinAppButtonGestureAngleRad = 0.25;

// Exceeding pressing the appbutton for longer than this threshold will result
// in a long press.
constexpr base::TimeDelta kLongPressThreshold =
    base::TimeDelta::FromMilliseconds(900);

// Timeout for checking for the WebVR rendering GL fence. If the timeout is
// reached, yield to let other tasks execute before rechecking.
constexpr base::TimeDelta kWebVRFenceCheckTimeout =
    base::TimeDelta::FromMicroseconds(2000);

// Polling interval for checking for the WebVR rendering GL fence. Used as
// an alternative to kWebVRFenceCheckTimeout if the GPU workaround is active.
// The actual interval may be longer due to PostDelayedTask's resolution.
constexpr base::TimeDelta kWebVRFenceCheckPollInterval =
    base::TimeDelta::FromMicroseconds(500);

constexpr int kWebVrInitialFrameTimeoutSeconds = 5;
constexpr int kWebVrSpinnerTimeoutSeconds = 2;

// Heuristic time limit to detect overstuffed GVR buffers for a
// >60fps capable web app.
constexpr base::TimeDelta kWebVrSlowAcquireThreshold =
    base::TimeDelta::FromMilliseconds(2);

// If running too fast, allow dropping frames occasionally to let GVR catch up.
// Drop at most one frame in MaxDropRate.
constexpr int kWebVrUnstuffMaxDropRate = 11;

constexpr int kNumSamplesPerPixelBrowserUi = 2;
constexpr int kNumSamplesPerPixelWebVr = 1;

constexpr float kRedrawSceneAngleDeltaDegrees = 1.0;

gfx::Transform PerspectiveMatrixFromView(const gvr::Rectf& fov,
                                         float z_near,
                                         float z_far) {
  gfx::Transform result;
  const float x_left = -std::tan(gfx::DegToRad(fov.left)) * z_near;
  const float x_right = std::tan(gfx::DegToRad(fov.right)) * z_near;
  const float y_bottom = -std::tan(gfx::DegToRad(fov.bottom)) * z_near;
  const float y_top = std::tan(gfx::DegToRad(fov.top)) * z_near;

  DCHECK(x_left < x_right && y_bottom < y_top && z_near < z_far &&
         z_near > 0.0f && z_far > 0.0f);
  const float X = (2 * z_near) / (x_right - x_left);
  const float Y = (2 * z_near) / (y_top - y_bottom);
  const float A = (x_right + x_left) / (x_right - x_left);
  const float B = (y_top + y_bottom) / (y_top - y_bottom);
  const float C = (z_near + z_far) / (z_near - z_far);
  const float D = (2 * z_near * z_far) / (z_near - z_far);

  // The gfx::Transform default ctor initializes the transform to the identity,
  // so we must zero out a few values along the diagonal here.
  result.matrix().set(0, 0, X);
  result.matrix().set(0, 2, A);
  result.matrix().set(1, 1, Y);
  result.matrix().set(1, 2, B);
  result.matrix().set(2, 2, C);
  result.matrix().set(2, 3, D);
  result.matrix().set(3, 2, -1);
  result.matrix().set(3, 3, 0);

  return result;
}

gvr::Rectf UVFromGfxRect(gfx::RectF rect) {
  return {rect.x(), rect.x() + rect.width(), 1.0f - rect.bottom(),
          1.0f - rect.y()};
}

gfx::RectF GfxRectFromUV(gvr::Rectf rect) {
  return gfx::RectF(rect.left, 1.0 - rect.top, rect.right - rect.left,
                    rect.top - rect.bottom);
}

}  // namespace

VrShellGl::VrShellGl(GlBrowserInterface* browser_interface,
                     std::unique_ptr<Ui> ui,
                     gvr_context* gvr_api,
                     bool reprojected_rendering,
                     bool daydream_support,
                     bool start_in_web_vr_mode,
                     bool pause_content)
    : ui_(std::move(ui)),
      web_vr_mode_(start_in_web_vr_mode),
      surfaceless_rendering_(reprojected_rendering),
      daydream_support_(daydream_support),
      content_paused_(pause_content),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this),
      browser_(browser_interface),
      vr_ui_fps_meter_(),
      webvr_fps_meter_(),
      webvr_js_time_(kWebVRSlidingAverageSize),
      webvr_render_time_(kWebVRSlidingAverageSize),
      webvr_js_wait_time_(kWebVRSlidingAverageSize),
      webvr_acquire_time_(kWebVRSlidingAverageSize),
      webvr_submit_time_(kWebVRSlidingAverageSize),
      weak_ptr_factory_(this) {
  GvrInit(gvr_api);
}

VrShellGl::~VrShellGl() {
  ClosePresentationBindings();
}

void VrShellGl::Initialize() {
  if (surfaceless_rendering_) {
    // If we're rendering surfaceless, we'll never get a java surface to render
    // into, so we can initialize GL right away.
    InitializeGl(nullptr);
  }
}

void VrShellGl::InitializeGl(gfx::AcceleratedWidget window) {
  bool reinitializing = ready_to_draw_;

  // We should only ever re-initialize when our surface is destroyed, which
  // should only ever happen when drawing to a surface.
  CHECK(!reinitializing || !surfaceless_rendering_);
  if (gl::GetGLImplementation() == gl::kGLImplementationNone &&
      !gl::init::InitializeGLOneOff()) {
    LOG(ERROR) << "gl::init::InitializeGLOneOff failed";
    ForceExitVr();
    return;
  }
  if (window) {
    CHECK(!surfaceless_rendering_);
    surface_ = gl::init::CreateViewGLSurface(window);
  } else {
    CHECK(surfaceless_rendering_);
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  }
  if (!surface_.get()) {
    LOG(ERROR) << "gl::init::CreateOffscreenGLSurface failed";
    ForceExitVr();
    return;
  }

  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  if (!context_.get()) {
    LOG(ERROR) << "gl::init::CreateGLContext failed";
    ForceExitVr();
    return;
  }
  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    ForceExitVr();
    return;
  }

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  unsigned int textures[4];
  glGenTextures(4, textures);
  webvr_texture_id_ = textures[0];
  unsigned int content_texture_id = textures[1];
  unsigned int content_overlay_texture_id = textures[2];
  unsigned int ui_texture_id = textures[3];

  content_surface_texture_ = gl::SurfaceTexture::Create(content_texture_id);
  content_overlay_surface_texture_ =
      gl::SurfaceTexture::Create(content_overlay_texture_id);
  ui_surface_texture_ = gl::SurfaceTexture::Create(ui_texture_id);
  webvr_surface_texture_ = gl::SurfaceTexture::Create(webvr_texture_id_);

  content_surface_ =
      std::make_unique<gl::ScopedJavaSurface>(content_surface_texture_.get());
  browser_->ContentSurfaceCreated(content_surface_->j_surface().obj(),
                                  content_surface_texture_.get());
  content_overlay_surface_ = std::make_unique<gl::ScopedJavaSurface>(
      content_overlay_surface_texture_.get());
  browser_->ContentOverlaySurfaceCreated(
      content_overlay_surface_->j_surface().obj(),
      content_overlay_surface_texture_.get());

  ui_surface_ =
      std::make_unique<gl::ScopedJavaSurface>(ui_surface_texture_.get());
  browser_->DialogSurfaceCreated(ui_surface_->j_surface().obj(),
                                 ui_surface_texture_.get());

  content_surface_texture_->SetFrameAvailableCallback(base::BindRepeating(
      &VrShellGl::OnContentFrameAvailable, weak_ptr_factory_.GetWeakPtr()));
  content_overlay_surface_texture_->SetFrameAvailableCallback(
      base::BindRepeating(&VrShellGl::OnContentOverlayFrameAvailable,
                          weak_ptr_factory_.GetWeakPtr()));
  ui_surface_texture_->SetFrameAvailableCallback(base::BindRepeating(
      &VrShellGl::OnUiFrameAvailable, weak_ptr_factory_.GetWeakPtr()));
  webvr_surface_texture_->SetFrameAvailableCallback(base::BindRepeating(
      &VrShellGl::OnWebVRFrameAvailable, weak_ptr_factory_.GetWeakPtr()));

  content_surface_texture_->SetDefaultBufferSize(
      content_tex_buffer_size_.width(), content_tex_buffer_size_.height());
  content_overlay_surface_texture_->SetDefaultBufferSize(
      content_tex_buffer_size_.width(), content_tex_buffer_size_.height());
  ui_surface_texture_->SetDefaultBufferSize(content_tex_buffer_size_.width(),
                                            content_tex_buffer_size_.height());

  webvr_vsync_align_ = base::FeatureList::IsEnabled(features::kWebVrVsyncAlign);

  VrMetricsUtil::XRRenderPath render_path =
      VrMetricsUtil::XRRenderPath::kClientWait;

  std::string render_path_string = base::GetFieldTrialParamValueByFeature(
      features::kWebXrRenderPath, features::kWebXrRenderPathParamName);
  DVLOG(1) << __FUNCTION__ << ": WebXrRenderPath=" << render_path_string;
  if (render_path_string == features::kWebXrRenderPathParamValueClientWait) {
    // Use the baseline kClientWait.
  } else {
    // Default aka features::kWebXrRenderPathParamValueGpuFence.
    // Use kGpuFence if it is supported. If not, use baseline kClientWait.
    if (gl::GLFence::IsGpuFenceSupported()) {
      webvr_use_gpu_fence_ = true;
      render_path = VrMetricsUtil::XRRenderPath::kGpuFence;
    }
  }
  VrMetricsUtil::LogXrRenderPathUsed(render_path);

  // InitializeRenderer calls GvrDelegateReady which triggers actions such as
  // responding to RequestPresent. All member assignments or other
  // initialization actions which affect presentation setup, i.e. feature
  // checks, must happen before this point.
  if (!reinitializing)
    InitializeRenderer();

  ui_->OnGlInitialized(
      content_texture_id, UiElementRenderer::kTextureLocationExternal,
      content_overlay_texture_id, UiElementRenderer::kTextureLocationExternal,
      ui_texture_id, true);

  webvr_vsync_align_ = base::FeatureList::IsEnabled(features::kWebVrVsyncAlign);

  if (reinitializing && mailbox_bridge_) {
    mailbox_bridge_ = nullptr;
    mailbox_bridge_ready_ = false;
    CreateOrResizeWebVRSurface(webvr_surface_size_);
  }

  ready_to_draw_ = true;
  if (!paused_ && !reinitializing)
    OnVSync(base::TimeTicks::Now());
}

void VrShellGl::OnGpuProcessConnectionReady() {
  DVLOG(1) << __FUNCTION__;
  mailbox_bridge_ready_ = true;
}

void VrShellGl::CreateOrResizeWebVRSurface(const gfx::Size& size) {
  if (!webvr_surface_texture_) {
    DLOG(ERROR) << "No WebVR surface texture available";
    return;
  }

  // ContentPhysicalBoundsChanged is getting called twice with
  // identical sizes? Avoid thrashing the existing context.
  if (mailbox_bridge_ && (size == webvr_surface_size_)) {
    return;
  }

  if (!size.width() || !size.height()) {
    // Invalid size, defer until a new size arrives on a future bounds update.
    return;
  }

  webvr_surface_texture_->SetDefaultBufferSize(size.width(), size.height());
  webvr_surface_size_ = size;

  if (mailbox_bridge_) {
    mailbox_bridge_->ResizeSurface(size.width(), size.height());
  } else {
    mailbox_bridge_ready_ = false;
    mailbox_bridge_ = std::make_unique<MailboxToSurfaceBridge>(
        base::BindOnce(&VrShellGl::OnGpuProcessConnectionReady,
                       weak_ptr_factory_.GetWeakPtr()));

    mailbox_bridge_->CreateSurface(webvr_surface_texture_.get());
  }
}

void VrShellGl::SubmitFrame(int16_t frame_index,
                            const gpu::MailboxHolder& mailbox,
                            base::TimeDelta time_waited) {
  TRACE_EVENT0("gpu", "VrShellGl::SubmitWebVRFrame");

  // submit_client_ could be null when we exit presentation, if there were
  // pending SubmitFrame messages queued.  VRDisplayClient::OnExitPresent
  // will clean up state in blink, so it doesn't wait for
  // OnSubmitFrameTransferred or OnSubmitFrameRendered.
  if (!submit_client_.get())
    return;

  if (frame_index < 0 ||
      !webvr_frame_oustanding_[frame_index % kPoseRingBufferSize]) {
    mojo::ReportBadMessage("SubmitFrame called with an invalid frame_index");
    binding_.Close();
    return;
  }

  webvr_time_js_submit_[frame_index % kPoseRingBufferSize] =
      base::TimeTicks::Now();

  // The JavaScript wait time is supplied externally and not trustworthy. Clamp
  // to a reasonable range to avoid math errors.
  if (time_waited < base::TimeDelta())
    time_waited = base::TimeDelta();
  if (time_waited > base::TimeDelta::FromSeconds(1))
    time_waited = base::TimeDelta::FromSeconds(1);
  webvr_js_wait_time_.AddSample(time_waited);
  TRACE_COUNTER1("gpu", "WebVR JS wait (ms)",
                 webvr_js_wait_time_.GetAverage().InMilliseconds());

  // Swapping twice on a Surface without calling updateTexImage in
  // between can lose frames, so don't draw+swap if we already have
  // a pending frame we haven't consumed yet.
  bool swapped = false;
  if (pending_frames_.empty()) {
    swapped = mailbox_bridge_->CopyMailboxToSurfaceAndSwap(mailbox);
    if (swapped) {
      // Tell OnWebVRFrameAvailable to expect a new frame to arrive on
      // the SurfaceTexture, and save the associated frame index.
      pending_frames_.emplace(frame_index);
    }
  }
  // Always notify the client that we're done with the mailbox even
  // if we haven't drawn it, so that it's eligible for destruction.
  submit_client_->OnSubmitFrameTransferred(true);
  if (!swapped) {
    // We dropped without drawing, report this as completed rendering
    // now to unblock the client. We're not going to receive it in
    // OnWebVRFrameAvailable where we'd normally report that.
    submit_client_->OnSubmitFrameRendered();
  }
}

void VrShellGl::SubmitFrameWithTextureHandle(
    int16_t frame_index,
    mojo::ScopedHandle texture_handle) {
  NOTREACHED();
}

void VrShellGl::ConnectPresentingService(
    device::mojom::VRSubmitFrameClientPtrInfo submit_client_info,
    device::mojom::VRPresentationProviderRequest request,
    device::mojom::VRDisplayInfoPtr display_info,
    device::mojom::VRRequestPresentOptionsPtr present_options) {
  ClosePresentationBindings();
  submit_client_.Bind(std::move(submit_client_info));
  binding_.Bind(std::move(request));
  gfx::Size webvr_size(
      display_info->leftEye->renderWidth + display_info->rightEye->renderWidth,
      display_info->leftEye->renderHeight);
  DVLOG(1) << __FUNCTION__ << ": resize initial to " << webvr_size.width()
           << "x" << webvr_size.height();

  CreateOrResizeWebVRSurface(webvr_size);
  ScheduleOrCancelWebVrFrameTimeout();

  // TODO(https://crbug.com/795049): Add a metric to track how much the
  // permitted-but-not-recommended preserveDrawingBuffer=true mode is used by
  // WebVR 1.1 sites. Having this option set will disable the planned
  // direct-draw-to-shared-buffer optimization.
  DVLOG(1) << "preserveDrawingBuffer="
           << present_options->preserve_drawing_buffer;

  report_webxr_input_ = present_options->webxr_input;
}

void VrShellGl::OnSwapContents(int new_content_id) {
  ui_->OnSwapContents(new_content_id);
}

void VrShellGl::EnableAlertDialog(ContentInputForwarder* input_forwarder,
                                  int width,
                                  int height) {
  showing_vr_dialog_ = true;
  vr_dialog_.reset(new VrDialog(width, height));
  vr_dialog_->SetEventForwarder(input_forwarder);
  ui_->SetAlertDialogEnabled(true, vr_dialog_.get(), width, height);
  ScheduleOrCancelWebVrFrameTimeout();
}

void VrShellGl::DisableAlertDialog() {
  showing_vr_dialog_ = false;
  ui_->SetAlertDialogEnabled(false, nullptr, 0, 0);
  vr_dialog_ = nullptr;
  ScheduleOrCancelWebVrFrameTimeout();
}

void VrShellGl::SetAlertDialogSize(int width, int height) {
  if (vr_dialog_)
    vr_dialog_->SetSize(width, height);
  ui_->SetAlertDialogSize(width, height);
}

void VrShellGl::ResumeContentRendering() {
  if (!content_paused_)
    return;
  content_paused_ = false;

  // Note that we have to UpdateTexImage here because OnContentFrameAvailable
  // won't be fired again until we've updated.
  content_surface_texture_->UpdateTexImage();
}

void VrShellGl::OnContentFrameAvailable() {
  if (content_paused_)
    return;
  content_surface_texture_->UpdateTexImage();
  content_frame_available_ = true;
}

void VrShellGl::OnContentOverlayFrameAvailable() {
  content_overlay_surface_texture_->UpdateTexImage();
}

void VrShellGl::OnUiFrameAvailable() {
  ui_surface_texture_->UpdateTexImage();
}

void VrShellGl::OnWebVRFrameAvailable() {
  // This is called each time a frame that was drawn on the WebVR Surface
  // arrives on the SurfaceTexture.

  // A "while" loop here is a bad idea. It's legal to call
  // UpdateTexImage repeatedly even if no frames are available, but
  // that does *not* wait for a new frame, it just reuses the most
  // recent one. That would mess up the count.
  if (pending_frames_.empty()) {
    // We're expecting a frame, but it's not here yet. Retry in OnVsync.
    ++premature_received_frames_;
    return;
  }

  webvr_surface_texture_->UpdateTexImage();
  int frame_index = pending_frames_.front();
  TRACE_EVENT1("gpu", "VrShellGl::OnWebVRFrameAvailable", "frame", frame_index);
  pending_frames_.pop();

  OnNewWebVRFrame();
  DrawFrame(frame_index, base::TimeTicks::Now());
}

void VrShellGl::OnNewWebVRFrame() {
  ui_->OnWebVrFrameAvailable();

  if (web_vr_mode_) {
    ++webvr_frames_received_;

    webvr_fps_meter_.AddFrame(base::TimeTicks::Now());
    TRACE_COUNTER1("gpu", "WebVR FPS", webvr_fps_meter_.GetFPS());
  }

  ScheduleOrCancelWebVrFrameTimeout();
}

void VrShellGl::ScheduleOrCancelWebVrFrameTimeout() {
  // TODO(mthiesse): We should also timeout after the initial frame to prevent
  // bad experiences, but we have to be careful to handle things like splash
  // screens correctly. For now just ensure we receive a first frame.
  if (!web_vr_mode_ || webvr_frames_received_ > 0 || showing_vr_dialog_) {
    if (!webvr_frame_timeout_.IsCancelled())
      webvr_frame_timeout_.Cancel();
    if (!webvr_spinner_timeout_.IsCancelled())
      webvr_spinner_timeout_.Cancel();
    return;
  }
  if (ui_->CanSendWebVrVSync() && submit_client_) {
    webvr_spinner_timeout_.Reset(base::BindOnce(
        &VrShellGl::OnWebVrTimeoutImminent, base::Unretained(this)));
    task_runner_->PostDelayedTask(
        FROM_HERE, webvr_spinner_timeout_.callback(),
        base::TimeDelta::FromSeconds(kWebVrSpinnerTimeoutSeconds));
    webvr_frame_timeout_.Reset(base::BindOnce(&VrShellGl::OnWebVrFrameTimedOut,
                                              base::Unretained(this)));
    task_runner_->PostDelayedTask(
        FROM_HERE, webvr_frame_timeout_.callback(),
        base::TimeDelta::FromSeconds(kWebVrInitialFrameTimeoutSeconds));
  }
}

void VrShellGl::OnWebVrFrameTimedOut() {
  ui_->OnWebVrTimedOut();
}

void VrShellGl::OnWebVrTimeoutImminent() {
  ui_->OnWebVrTimeoutImminent();
}

void VrShellGl::GvrInit(gvr_context* gvr_api) {
  gvr_api_ = gvr::GvrApi::WrapNonOwned(gvr_api);
  controller_.reset(new VrController(gvr_api));
  ui_->OnPlatformControllerInitialized(controller_.get());

  VrMetricsUtil::LogVrViewerType(gvr_api_->GetViewerType());

  cardboard_ =
      (gvr_api_->GetViewerType() == gvr::ViewerType::GVR_VIEWER_TYPE_CARDBOARD);
  if (cardboard_ && web_vr_mode_) {
    browser_->ToggleCardboardGamepad(true);
  }
}

device::mojom::VRDisplayFrameTransportOptionsPtr
VrShellGl::GetWebVrFrameTransportOptions() {
  // All member assignments that affect render path selections must be complete
  // before this function executes. See InitializeRenderer comment in
  // InitializeGl.

  device::mojom::VRDisplayFrameTransportOptionsPtr transport_options =
      device::mojom::VRDisplayFrameTransportOptions::New();
  transport_options->transport_method =
      device::mojom::VRDisplayFrameTransportMethod::SUBMIT_AS_MAILBOX_HOLDER;
  // Only set boolean options that we need. Default is false, and we should be
  // able to safely ignore ones that our implementation doesn't care about.
  transport_options->wait_for_transfer_notification = true;
  if (webvr_use_gpu_fence_) {
    transport_options->wait_for_gpu_fence = true;
  } else {
    transport_options->wait_for_render_notification = true;
  }

  return transport_options;
}

void VrShellGl::InitializeRenderer() {
  gvr_api_->InitializeGl();
  gfx::Transform head_pose;
  device::GvrDelegate::GetGvrPoseWithNeckModel(gvr_api_.get(), &head_pose);
  webvr_head_pose_.assign(kPoseRingBufferSize, head_pose);
  webvr_time_pose_.assign(kPoseRingBufferSize, base::TimeTicks());
  webvr_frame_oustanding_.assign(kPoseRingBufferSize, false);
  webvr_time_js_submit_.assign(kPoseRingBufferSize, base::TimeTicks());
  webvr_time_copied_.assign(kPoseRingBufferSize, base::TimeTicks());

  // For kFramePrimaryBuffer (primary VrShell and WebVR content)
  specs_.push_back(gvr_api_->CreateBufferSpec());
  specs_.push_back(gvr_api_->CreateBufferSpec());

  gvr::Sizei render_size_default = specs_[kFramePrimaryBuffer].GetSize();
  render_size_default_ = {render_size_default.width,
                          render_size_default.height};

  specs_[kFramePrimaryBuffer].SetSamples(
      web_vr_mode_ ? kNumSamplesPerPixelWebVr : kNumSamplesPerPixelBrowserUi);
  specs_[kFramePrimaryBuffer].SetDepthStencilFormat(
      GVR_DEPTH_STENCIL_FORMAT_NONE);

  specs_[kFrameWebVrBrowserUiBuffer].SetSize(
      {render_size_default.width / kWebVrBrowserUiSizeFactor,
       render_size_default.height / kWebVrBrowserUiSizeFactor});
  specs_[kFrameWebVrBrowserUiBuffer].SetSamples(2);
  specs_[kFrameWebVrBrowserUiBuffer].SetDepthStencilFormat(
      GVR_DEPTH_STENCIL_FORMAT_NONE);
  render_size_webvr_ui_ = {
      render_size_default.width / kWebVrBrowserUiSizeFactor,
      render_size_default.height / kWebVrBrowserUiSizeFactor};

  swap_chain_ =
      std::make_unique<gvr::SwapChain>(gvr_api_->CreateSwapChain(specs_));

  // Allocate a buffer viewport for use in UI drawing. This isn't
  // initialized at this point, it'll be set from other viewport list
  // entries as needed.
  buffer_viewport_.reset(
      new gvr::BufferViewport(gvr_api_->CreateBufferViewport()));

  // Set up main content viewports. The list has two elements, 0=left
  // eye and 1=right eye.
  buffer_viewport_list_.reset(
      new gvr::BufferViewportList(gvr_api_->CreateEmptyBufferViewportList()));
  buffer_viewport_list_->SetToRecommendedBufferViewports();

  // Set up WebVR Browser UI viewports, these will be elements 2=left eye
  // and 3=right eye.
  webvr_browser_ui_left_viewport_.reset(
      new gvr::BufferViewport(gvr_api_->CreateBufferViewport()));
  buffer_viewport_list_->GetBufferViewport(
      GVR_LEFT_EYE, webvr_browser_ui_left_viewport_.get());
  webvr_browser_ui_left_viewport_->SetSourceBufferIndex(
      kFrameWebVrBrowserUiBuffer);

  webvr_browser_ui_right_viewport_.reset(
      new gvr::BufferViewport(gvr_api_->CreateBufferViewport()));
  buffer_viewport_list_->GetBufferViewport(
      GVR_RIGHT_EYE, webvr_browser_ui_right_viewport_.get());
  webvr_browser_ui_right_viewport_->SetSourceBufferIndex(
      kFrameWebVrBrowserUiBuffer);

  // Save copies of the first two viewport items for use by WebVR, it
  // sets its own UV bounds.
  webvr_left_viewport_.reset(
      new gvr::BufferViewport(gvr_api_->CreateBufferViewport()));
  buffer_viewport_list_->GetBufferViewport(GVR_LEFT_EYE,
                                           webvr_left_viewport_.get());
  webvr_left_viewport_->SetSourceBufferIndex(kFramePrimaryBuffer);

  webvr_right_viewport_.reset(
      new gvr::BufferViewport(gvr_api_->CreateBufferViewport()));
  buffer_viewport_list_->GetBufferViewport(GVR_RIGHT_EYE,
                                           webvr_right_viewport_.get());
  webvr_right_viewport_->SetSourceBufferIndex(kFramePrimaryBuffer);

  browser_->GvrDelegateReady(gvr_api_->GetViewerType(),
                             GetWebVrFrameTransportOptions());
}

void VrShellGl::UpdateController(const RenderInfo& render_info,
                                 base::TimeTicks current_time) {
  TRACE_EVENT0("gpu", "VrShellGl::UpdateController");
  gvr::Mat4f gvr_head_pose;
  TransformToGvrMat(render_info.head_pose, &gvr_head_pose);
  controller_->UpdateState(gvr_head_pose);
  gfx::Point3F laser_origin = controller_->GetPointerStart();

  device::GvrGamepadData controller_data = controller_->GetGamepadData();
  if (!ShouldDrawWebVr())
    controller_data.connected = false;
  browser_->UpdateGamepadData(controller_data);

  HandleControllerInput(laser_origin, render_info, current_time);
}

void VrShellGl::HandleControllerInput(const gfx::Point3F& laser_origin,
                                      const RenderInfo& render_info,
                                      base::TimeTicks current_time) {
  gfx::Vector3dF head_direction = GetForwardVector(render_info.head_pose);
  if (is_exiting_) {
    // When we're exiting, we don't show the reticle and the only input
    // processing we do is to handle immediate exits.
    SendImmediateExitRequestIfNecessary();
    return;
  }

  gfx::Vector3dF ergo_neutral_pose;
  if (!controller_->IsConnected()) {
    // No controller detected, set up a gaze cursor that tracks the
    // forward direction.
    ergo_neutral_pose = {0.0f, 0.0f, -1.0f};
    controller_quat_ =
        gfx::Quaternion(gfx::Vector3dF(0.0f, 0.0f, -1.0f), head_direction);
  } else {
    ergo_neutral_pose = {0.0f, -sin(kErgoAngleOffset), -cos(kErgoAngleOffset)};
    controller_quat_ = controller_->Orientation();
  }

  gfx::Transform mat(controller_quat_);
  gfx::Vector3dF controller_direction = ergo_neutral_pose;
  mat.TransformVector(&controller_direction);

  HandleControllerAppButtonActivity(controller_direction);

  if (ShouldDrawWebVr() && !ShouldSendGesturesToWebVr()) {
    return;
  }

  ControllerModel controller_model;
  controller_->GetTransform(&controller_model.transform);
  std::unique_ptr<GestureList> gesture_list_ptr = controller_->DetectGestures();
  GestureList& gesture_list = *gesture_list_ptr;
  controller_model.touchpad_button_state = UiInputManager::ButtonState::UP;
  DCHECK(!(controller_->ButtonUpHappened(gvr::kControllerButtonClick) &&
           controller_->ButtonDownHappened(gvr::kControllerButtonClick)))
      << "Cannot handle a button down and up event within one frame.";
  if (controller_->ButtonState(gvr::kControllerButtonClick)) {
    controller_model.touchpad_button_state = UiInputManager::ButtonState::DOWN;
  }
  controller_model.app_button_state =
      controller_->ButtonState(gvr::kControllerButtonApp)
          ? UiInputManager::ButtonState::DOWN
          : UiInputManager::ButtonState::UP;
  controller_model.home_button_state =
      controller_->ButtonState(gvr::kControllerButtonHome)
          ? UiInputManager::ButtonState::DOWN
          : UiInputManager::ButtonState::UP;
  controller_model.opacity = controller_->GetOpacity();
  controller_model.laser_direction = controller_direction;
  controller_model.laser_origin = laser_origin;
  controller_model.handedness = controller_->GetHandedness();
  controller_model.recentered = controller_->GetRecentered();
  controller_model.touching_touchpad = controller_->IsTouching();
  controller_model.touchpad_touch_position =
      gfx::PointF(controller_->TouchPosX(), controller_->TouchPosY());
  controller_model.app_button_long_pressed = app_button_long_pressed_;
  controller_model_ = controller_model;

  ReticleModel reticle_model;
  ui_->input_manager()->HandleInput(current_time, render_info, controller_model,
                                    &reticle_model, &gesture_list);
  ui_->OnControllerUpdated(controller_model, reticle_model);
}

void VrShellGl::SendImmediateExitRequestIfNecessary() {
  gvr::ControllerButton buttons[] = {
      gvr::kControllerButtonClick, gvr::kControllerButtonApp,
      gvr::kControllerButtonHome,
  };
  for (size_t i = 0; i < arraysize(buttons); ++i) {
    if (controller_->ButtonUpHappened(buttons[i]) ||
        controller_->ButtonDownHappened(buttons[i])) {
      browser_->ForceExitVr();
    }
  }
}

void VrShellGl::HandleControllerAppButtonActivity(
    const gfx::Vector3dF& controller_direction) {
  // Note that button up/down state is transient, so ButtonDownHappened only
  // returns true for a single frame (and we're guaranteed not to miss it).
  if (controller_->ButtonDownHappened(
          gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP)) {
    controller_start_direction_ = controller_direction;
    app_button_down_time_ = base::TimeTicks::Now();
    app_button_long_pressed_ = false;
  }

  if (controller_->ButtonUpHappened(
          gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP)) {
    // A gesture is a movement of the controller while holding the App button.
    // If the angle of the movement is within a threshold, the action is
    // considered a regular click
    // TODO(asimjour1): We need to refactor the gesture recognition outside of
    // VrShellGl.
    PlatformController::SwipeDirection direction =
        PlatformController::kSwipeDirectionNone;
    gfx::Vector3dF a = controller_start_direction_;
    gfx::Vector3dF b = controller_direction;
    a.set_y(0);
    b.set_y(0);
    if (a.LengthSquared() * b.LengthSquared() > 0.0) {
      float gesture_xz_angle =
          acos(gfx::DotProduct(a, b) / a.Length() / b.Length());
      if (fabs(gesture_xz_angle) > kMinAppButtonGestureAngleRad) {
        direction = gesture_xz_angle < 0
                        ? PlatformController::kSwipeDirectionLeft
                        : PlatformController::kSwipeDirectionRight;
        // Post a task, rather than calling the UI directly, so as not to modify
        // UI state in the midst of frame rendering.
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindRepeating(&Ui::OnAppButtonSwipePerformed,
                                base::Unretained(ui_.get()), direction));
      }
    }
    if (direction == PlatformController::kSwipeDirectionNone &&
        !app_button_long_pressed_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindRepeating(&Ui::OnAppButtonClicked,
                                         base::Unretained(ui_.get())));
    }
  }

  if (!app_button_long_pressed_ &&
      controller_->ButtonState(
          gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP) &&
      (base::TimeTicks::Now() - app_button_down_time_) > kLongPressThreshold) {
    app_button_long_pressed_ = true;
  }
}

bool VrShellGl::ResizeForWebVR(int16_t frame_index) {
  // Process all pending_bounds_ changes targeted for before this frame, being
  // careful of wrapping frame indices.
  static constexpr unsigned max =
      std::numeric_limits<decltype(frame_index_)>::max();
  static_assert(max > kPoseRingBufferSize * 2,
                "To detect wrapping, kPoseRingBufferSize must be smaller "
                "than half of frame_index_ range.");
  while (!pending_bounds_.empty()) {
    uint16_t index = pending_bounds_.front().first;
    // If index is less than the frame_index it's possible we've wrapped, so we
    // extend the range and 'un-wrap' to account for this.
    if (index < frame_index)
      index += max;
    // If the pending bounds change is for an upcoming frame within our buffer
    // size, wait to apply it. Otherwise, apply it immediately. This guarantees
    // that even if we miss many frames, the queue can't fill up with stale
    // bounds.
    if (index > frame_index && index <= frame_index + kPoseRingBufferSize)
      break;

    const WebVrBounds& bounds = pending_bounds_.front().second;
    webvr_left_viewport_->SetSourceUv(UVFromGfxRect(bounds.left_bounds));
    webvr_right_viewport_->SetSourceUv(UVFromGfxRect(bounds.right_bounds));
    DVLOG(1) << __FUNCTION__ << ": resize from pending_bounds to "
             << bounds.source_size.width() << "x"
             << bounds.source_size.height();
    CreateOrResizeWebVRSurface(bounds.source_size);
    pending_bounds_.pop();
  }
  if (render_info_primary_.surface_texture_size != webvr_surface_size_) {
    if (!webvr_surface_size_.width()) {
      // Don't try to resize to 0x0 pixels, drop frames until we get a valid
      // size.
      return false;
    }

    render_info_primary_.surface_texture_size = webvr_surface_size_;
    DVLOG(1) << __FUNCTION__ << ": resize GVR to "
             << webvr_surface_size_.width() << "x"
             << webvr_surface_size_.height();
    swap_chain_->ResizeBuffer(
        kFramePrimaryBuffer,
        {webvr_surface_size_.width(), webvr_surface_size_.height()});
  }
  return true;
}

void VrShellGl::UpdateSamples() {
  // It is illegal to call SetSamples on the swap change if we have an acquired
  // frame outstanding. Ensure that this is not the case here.
  CHECK(!acquired_frame_);
  int required_samples = ShouldDrawWebVr() ? kNumSamplesPerPixelWebVr
                                           : kNumSamplesPerPixelBrowserUi;
  if (specs_[kFramePrimaryBuffer].GetSamples() != required_samples) {
    specs_[kFramePrimaryBuffer].SetSamples(required_samples);
    swap_chain_ =
        std::make_unique<gvr::SwapChain>(gvr_api_->CreateSwapChain(specs_));
  }
}

void VrShellGl::UpdateEyeInfos(const gfx::Transform& head_pose,
                               int viewport_offset,
                               const gfx::Size& render_size,
                               RenderInfo* out_render_info) {
  for (auto eye : {GVR_LEFT_EYE, GVR_RIGHT_EYE}) {
    CameraModel& eye_info = (eye == GVR_LEFT_EYE)
                                ? out_render_info->left_eye_model
                                : out_render_info->right_eye_model;
    eye_info.eye_type = GVR_LEFT_EYE ? EyeType::kLeftEye : EyeType::kRightEye;

    buffer_viewport_list_->GetBufferViewport(eye + viewport_offset,
                                             buffer_viewport_.get());

    gfx::Transform eye_matrix;
    GvrMatToTransform(gvr_api_->GetEyeFromHeadMatrix(eye), &eye_matrix);
    eye_info.view_matrix = eye_matrix * head_pose;

    const gfx::RectF& rect = GfxRectFromUV(buffer_viewport_->GetSourceUv());
    eye_info.viewport = CalculatePixelSpaceRect(render_size, rect);

    eye_info.proj_matrix = PerspectiveMatrixFromView(
        buffer_viewport_->GetSourceFov(), kZNear, kZFar);
    eye_info.view_proj_matrix = eye_info.proj_matrix * eye_info.view_matrix;
  }
}

void VrShellGl::DrawFrame(int16_t frame_index, base::TimeTicks current_time) {
  TRACE_EVENT1("gpu", "VrShellGl::DrawFrame", "frame", frame_index);
  if (!webvr_delayed_frame_submit_.IsCancelled()) {
    webvr_delayed_frame_submit_.Cancel();
    DrawIntoAcquiredFrame(frame_index, current_time);
    return;
  }

  CHECK(!acquired_frame_);

  // Reset the viewport list to just the pair of viewports for the
  // primary buffer each frame. Head-locked viewports get added by
  // DrawVrShell if needed.
  buffer_viewport_list_->SetToRecommendedBufferViewports();

  // We may need to recreate the swap chain if we've switched between web vr and
  // browsing mode.
  UpdateSamples();

  // If needed, resize the primary buffer for use with WebVR. Resizing
  // needs to happen before acquiring a frame.
  if (ShouldDrawWebVr()) {
    if (!ResizeForWebVR(frame_index)) {
      // We don't have a valid size yet, can't draw.
      return;
    }
    buffer_viewport_list_->SetBufferViewport(GVR_LEFT_EYE,
                                             *webvr_left_viewport_);
    buffer_viewport_list_->SetBufferViewport(GVR_RIGHT_EYE,
                                             *webvr_right_viewport_);
  } else {
    if (render_info_primary_.surface_texture_size != render_size_default_) {
      render_info_primary_.surface_texture_size = render_size_default_;
      swap_chain_->ResizeBuffer(
          kFramePrimaryBuffer,
          {render_info_primary_.surface_texture_size.width(),
           render_info_primary_.surface_texture_size.height()});
    }
  }

  // When using async reprojection, we need to know which pose was
  // used in the WebVR app for drawing this frame and supply it when
  // submitting. Technically we don't need a pose if not reprojecting,
  // but keeping it uninitialized seems likely to cause problems down
  // the road. Copying it is cheaper than fetching a new one.
  if (ShouldDrawWebVr()) {
    static_assert(!((kPoseRingBufferSize - 1) & kPoseRingBufferSize),
                  "kPoseRingBufferSize must be a power of 2");
    // Copy into render info for overlay UI. WebVR doesn't use this.
    render_info_primary_.head_pose =
        webvr_head_pose_[frame_index % kPoseRingBufferSize];
    webvr_frame_oustanding_[frame_index % kPoseRingBufferSize] = false;
  } else {
    device::GvrDelegate::GetGvrPoseWithNeckModel(
        gvr_api_.get(), &render_info_primary_.head_pose);
  }

  // Update the render position of all UI elements (including desktop).
  bool scene_changed =
      ui_->scene()->OnBeginFrame(current_time, render_info_primary_.head_pose);

  // WebVR handles controller input in OnVsync.
  if (!ShouldDrawWebVr())
    UpdateController(render_info_primary_, current_time);

  bool textures_changed = ui_->scene()->UpdateTextures();

  // TODO(mthiesse): Determine if a visible controller is actually drawn in the
  // viewport.
  bool controller_dirty = ui_->IsControllerVisible();

  // TODO(mthiesse): Refine this notion of when we need to redraw. If only a
  // portion of the screen is dirtied, we can update just redraw that portion.
  bool redraw_needed = controller_dirty || scene_changed || textures_changed ||
                       content_frame_available_;

  bool head_moved = HeadMoveExceedsThreshold(last_used_head_pose_,
                                             render_info_primary_.head_pose,
                                             kRedrawSceneAngleDeltaDegrees);

  bool dirty = ShouldDrawWebVr() || head_moved || redraw_needed;

  if (!dirty && ui_->SkipsRedrawWhenNotDirty())
    return;

  TRACE_EVENT_BEGIN0("gpu", "VrShellGl::AcquireFrame");
  base::TimeTicks acquire_start = base::TimeTicks::Now();
  acquired_frame_ = swap_chain_->AcquireFrame();
  webvr_acquire_time_.AddSample(base::TimeTicks::Now() - acquire_start);
  TRACE_EVENT_END0("gpu", "VrShellGl::AcquireFrame");
  if (!acquired_frame_)
    return;

  DrawIntoAcquiredFrame(frame_index, current_time);
}

void VrShellGl::DrawIntoAcquiredFrame(int16_t frame_index,
                                      base::TimeTicks current_time) {
  TRACE_EVENT1("gpu", "VrShellGl::DrawIntoAcquiredFrame", "frame", frame_index);

  last_used_head_pose_ = render_info_primary_.head_pose;

  acquired_frame_.BindBuffer(kFramePrimaryBuffer);

  // We're redrawing over the entire viewport, but it's generally more
  // efficient on mobile tiling GPUs to clear anyway as a hint that
  // we're done with the old content. TODO(klausw, https://crbug.com/700389):
  // investigate using glDiscardFramebufferEXT here since that's more
  // efficient on desktop, but it would need a capability check since
  // it's not supported on older devices such as Nexus 5X.
  glClear(GL_COLOR_BUFFER_BIT);

  if (ShouldDrawWebVr())
    DrawWebVr();

  UpdateEyeInfos(render_info_primary_.head_pose, kViewportListPrimaryOffset,
                 render_info_primary_.surface_texture_size,
                 &render_info_primary_);

  // Measure projected content size and bubble up if delta exceeds threshold.
  ui_->OnProjMatrixChanged(render_info_primary_.left_eye_model.proj_matrix);

  // At this point, we draw non-WebVR content that could, potentially, fill the
  // viewport.  NB: this is not just 2d browsing stuff, we may have a splash
  // screen showing in WebVR mode that must also fill the screen. That said,
  // while the splash screen is up ShouldDrawWebVr() will return false.
  if (!ShouldDrawWebVr()) {
    ui_->ui_renderer()->Draw(render_info_primary_);
  }

  content_frame_available_ = false;
  acquired_frame_.Unbind();

  std::vector<const UiElement*> overlay_elements;
  if (ShouldDrawWebVr()) {
    overlay_elements = ui_->scene()->GetVisibleWebVrOverlayElementsToDraw();
  }

  if (!overlay_elements.empty() && ShouldDrawWebVr()) {
    // WebVR content may use an arbitray size buffer. We need to draw browser UI
    // on a different buffer to make sure that our UI has enough resolution.
    acquired_frame_.BindBuffer(kFrameWebVrBrowserUiBuffer);

    // Update recommended fov and uv per frame.
    buffer_viewport_list_->GetBufferViewport(GVR_LEFT_EYE,
                                             buffer_viewport_.get());
    const gvr::Rectf& fov_recommended_left = buffer_viewport_->GetSourceFov();
    buffer_viewport_list_->GetBufferViewport(GVR_RIGHT_EYE,
                                             buffer_viewport_.get());
    const gvr::Rectf& fov_recommended_right = buffer_viewport_->GetSourceFov();

    // Set render info to recommended setting. It will be used as our base for
    // optimization.
    RenderInfo render_info_webvr_browser_ui;
    render_info_webvr_browser_ui.head_pose = render_info_primary_.head_pose;
    webvr_browser_ui_left_viewport_->SetSourceFov(fov_recommended_left);
    webvr_browser_ui_right_viewport_->SetSourceFov(fov_recommended_right);
    buffer_viewport_list_->SetBufferViewport(
        kViewportListWebVrBrowserUiOffset + GVR_LEFT_EYE,
        *webvr_browser_ui_left_viewport_);
    buffer_viewport_list_->SetBufferViewport(
        kViewportListWebVrBrowserUiOffset + GVR_RIGHT_EYE,
        *webvr_browser_ui_right_viewport_);
    UpdateEyeInfos(render_info_webvr_browser_ui.head_pose,
                   kViewportListWebVrBrowserUiOffset, render_size_webvr_ui_,
                   &render_info_webvr_browser_ui);
    gvr::Rectf minimal_fov;
    GetMinimalFov(render_info_webvr_browser_ui.left_eye_model.view_matrix,
                  overlay_elements, fov_recommended_left, kZNear, &minimal_fov);
    webvr_browser_ui_left_viewport_->SetSourceFov(minimal_fov);

    GetMinimalFov(render_info_webvr_browser_ui.right_eye_model.view_matrix,
                  overlay_elements, fov_recommended_right, kZNear,
                  &minimal_fov);
    webvr_browser_ui_right_viewport_->SetSourceFov(minimal_fov);

    buffer_viewport_list_->SetBufferViewport(
        kViewportListWebVrBrowserUiOffset + GVR_LEFT_EYE,
        *webvr_browser_ui_left_viewport_);
    buffer_viewport_list_->SetBufferViewport(
        kViewportListWebVrBrowserUiOffset + GVR_RIGHT_EYE,
        *webvr_browser_ui_right_viewport_);
    UpdateEyeInfos(render_info_webvr_browser_ui.head_pose,
                   kViewportListWebVrBrowserUiOffset, render_size_webvr_ui_,
                   &render_info_webvr_browser_ui);

    ui_->ui_renderer()->DrawWebVrOverlayForeground(
        render_info_webvr_browser_ui);

    acquired_frame_.Unbind();
  }

  // GVR submit needs the exact head pose that was used for rendering.
  gfx::Transform submit_head_pose;
  if (ShouldDrawWebVr()) {
    // Don't use render_info_primary_.head_pose here, that may have been
    // overwritten by OnVSync's controller handling. We need the pose that was
    // sent to JS.
    submit_head_pose = webvr_head_pose_[frame_index % kPoseRingBufferSize];
  } else {
    submit_head_pose = render_info_primary_.head_pose;
  }
  std::unique_ptr<gl::GLFenceEGL> fence = nullptr;
  if (ShouldDrawWebVr() && surfaceless_rendering_) {
    webvr_time_copied_[frame_index % kPoseRingBufferSize] =
        base::TimeTicks::Now();
    if (webvr_use_gpu_fence_) {
      // Continue with submit once the previous frame's GL fence signals that
      // it is done rendering. This avoids blocking in GVR's Submit. Fence is
      // null for the first frame, in that case the fence wait is skipped.
      fence.reset(webvr_prev_frame_completion_fence_.release());
      if (fence && fence->HasCompleted()) {
        // The fence had already signaled, so we don't know how long ago
        // rendering had finished. We can submit immediately.
        AddWebVrRenderTimeEstimate(frame_index, false);
        fence = nullptr;
      }
    } else {
      // Continue with submit once a GL fence signals that current drawing
      // operations have completed.
      fence = gl::GLFenceEGL::Create();
    }
  }
  if (fence) {
    webvr_delayed_frame_submit_.Reset(base::BindRepeating(
        &VrShellGl::DrawFrameSubmitWhenReady, base::Unretained(this)));
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(webvr_delayed_frame_submit_.callback(), frame_index,
                       submit_head_pose, base::Passed(&fence)));
  } else {
    // Continue with submit immediately.
    DrawFrameSubmitNow(frame_index, submit_head_pose);
  }
}

void VrShellGl::DrawFrameSubmitWhenReady(
    int16_t frame_index,
    const gfx::Transform& head_pose,
    std::unique_ptr<gl::GLFenceEGL> fence) {
  bool use_polling = mailbox_bridge_ready_ &&
                     mailbox_bridge_->IsGpuWorkaroundEnabled(
                         gpu::DONT_USE_EGLCLIENTWAITSYNC_WITH_TIMEOUT);
  if (fence) {
    if (!use_polling) {
      // Use wait-with-timeout to find out as soon as possible when rendering
      // is complete.
      fence->ClientWaitWithTimeoutNanos(
          kWebVRFenceCheckTimeout.InMicroseconds() * 1000);
    }
    if (!fence->HasCompleted()) {
      webvr_delayed_frame_submit_.Reset(base::BindRepeating(
          &VrShellGl::DrawFrameSubmitWhenReady, base::Unretained(this)));
      if (use_polling) {
        // Poll the fence status at a short interval. This burns some CPU, but
        // avoids excessive waiting on devices which don't handle timeouts
        // correctly. Downside is that the completion status is only detected
        // with a delay of up to one polling interval.
        task_runner_->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(webvr_delayed_frame_submit_.callback(), frame_index,
                           head_pose, base::Passed(&fence)),
            kWebVRFenceCheckPollInterval);
      } else {
        task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(webvr_delayed_frame_submit_.callback(), frame_index,
                           head_pose, base::Passed(&fence)));
      }
      return;
    }
  }

  if (fence && webvr_use_gpu_fence_) {
    // We were waiting for the fence, so the time now is the actual
    // finish time for the previous frame's rendering.
    AddWebVrRenderTimeEstimate(frame_index, true);
  }

  webvr_delayed_frame_submit_.Cancel();
  DrawFrameSubmitNow(frame_index, head_pose);
}

void VrShellGl::AddWebVrRenderTimeEstimate(int16_t frame_index, bool did_wait) {
  int16_t prev_idx =
      (frame_index + kPoseRingBufferSize - 1) % kPoseRingBufferSize;
  base::TimeTicks prev_js_submit = webvr_time_js_submit_[prev_idx];
  if (webvr_use_gpu_fence_ && !prev_js_submit.is_null()) {
    // If we don't wait for rendering to complete, estimate render time for the
    // *previous* frame based on fence completion wait time.
    base::TimeDelta prev_render = base::TimeTicks::Now() - prev_js_submit;
    if (did_wait) {
      // Fence wasn't complete on first check. We've now waited for rendering to
      // finish, so the elapsed time is the actual render time.
      webvr_render_time_.AddSample(prev_render);
    } else {
      // Fence was already complete when we checked. It completed sometime
      // between the saved webvr_time_copied_ and now. Use the midpoint of
      // that as an estimate.
      base::TimeDelta lower_limit =
          webvr_time_copied_[frame_index % kPoseRingBufferSize] -
          prev_js_submit;
      base::TimeDelta midpoint = (lower_limit + prev_render) / 2;
      webvr_render_time_.AddSample(midpoint);
    }
  }
}

void VrShellGl::DrawFrameSubmitNow(int16_t frame_index,
                                   const gfx::Transform& head_pose) {
  TRACE_EVENT1("gpu", "VrShellGl::DrawFrameSubmitNow", "frame", frame_index);

  gvr::Mat4f mat;
  TransformToGvrMat(head_pose, &mat);
  {
    TRACE_EVENT0("gpu", "VrShellGl::SubmitToGvr");
    base::TimeTicks submit_start = base::TimeTicks::Now();
    acquired_frame_.Submit(*buffer_viewport_list_, mat);
    base::TimeTicks submit_done = base::TimeTicks::Now();
    webvr_submit_time_.AddSample(submit_done - submit_start);
    CHECK(!acquired_frame_);
  }

  // No need to swap buffers for surfaceless rendering.
  if (!surfaceless_rendering_) {
    // TODO(mthiesse): Support asynchronous SwapBuffers.
    TRACE_EVENT0("gpu", "VrShellGl::SwapBuffers");
    surface_->SwapBuffers(base::DoNothing());
  }

  // Report rendering completion to WebVR so that it's permitted to submit
  // a fresh frame. We could do this earlier, as soon as the frame got pulled
  // off the transfer surface, but that appears to result in overstuffed
  // buffers.
  if (submit_client_) {
    if (webvr_use_gpu_fence_) {
      // Save a fence for local completion checking.
      webvr_prev_frame_completion_fence_ = gl::GLFenceEGL::Create();

      // Make a GpuFence and pass it to the Renderer for sequencing frames.
      std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
      std::unique_ptr<gfx::GpuFence> gpu_fence = gl_fence->GetGpuFence();
      submit_client_->OnSubmitFrameGpuFence(
          gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle()));
    } else {
      // Renderer is waiting for the previous frame to render, unblock it now.
      submit_client_->OnSubmitFrameRendered();
    }
  }

  if (ShouldDrawWebVr()) {
    base::TimeTicks pose_time =
        webvr_time_pose_[frame_index % kPoseRingBufferSize];
    base::TimeTicks js_submit_time =
        webvr_time_js_submit_[frame_index % kPoseRingBufferSize];
    webvr_js_time_.AddSample(js_submit_time - pose_time);
    if (!webvr_use_gpu_fence_) {
      // Estimate render time from wallclock time, we waited for the pre-submit
      // render fence to signal.
      base::TimeTicks now = base::TimeTicks::Now();
      webvr_render_time_.AddSample(now - js_submit_time);
    }
  }

  // After saving the timestamp, fps will be available via GetFPS().
  // TODO(vollick): enable rendering of this framerate in a HUD.
  vr_ui_fps_meter_.AddFrame(base::TimeTicks::Now());
  DVLOG(1) << "fps: " << vr_ui_fps_meter_.GetFPS();
  TRACE_COUNTER1("gpu", "VR UI FPS", vr_ui_fps_meter_.GetFPS());
}

bool VrShellGl::ShouldDrawWebVr() {
  return web_vr_mode_ && ui_->ShouldRenderWebVr() && webvr_frames_received_ > 0;
}

bool VrShellGl::ShouldSendGesturesToWebVr() {
  return ui_->IsAppButtonLongPressed() != app_button_long_pressed_;
}

void VrShellGl::DrawWebVr() {
  TRACE_EVENT0("gpu", "VrShellGl::DrawWebVr");
  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_POLYGON_OFFSET_FILL);

  glViewport(0, 0, webvr_surface_size_.width(), webvr_surface_size_.height());
  ui_->ui_element_renderer()->DrawWebVr(webvr_texture_id_);
}

void VrShellGl::OnPause() {
  paused_ = true;
  vsync_helper_.CancelVSyncRequest();
  controller_->OnPause();
  gvr_api_->PauseTracking();
  webvr_frame_timeout_.Cancel();
  webvr_spinner_timeout_.Cancel();
}

void VrShellGl::OnResume() {
  paused_ = false;
  gvr_api_->RefreshViewerProfile();
  gvr_api_->ResumeTracking();
  controller_->OnResume();
  if (!ready_to_draw_)
    return;
  vsync_helper_.CancelVSyncRequest();
  OnVSync(base::TimeTicks::Now());
  if (web_vr_mode_)
    ScheduleOrCancelWebVrFrameTimeout();
}

void VrShellGl::OnExitPresent() {
  webvr_frame_timeout_.Cancel();
  webvr_spinner_timeout_.Cancel();
}

void VrShellGl::SetWebVrMode(bool enabled) {
  web_vr_mode_ = enabled;

  if (web_vr_mode_ && submit_client_) {
    ScheduleOrCancelWebVrFrameTimeout();
  } else {
    webvr_frame_timeout_.Cancel();
    webvr_frames_received_ = 0;
  }

  if (cardboard_)
    browser_->ToggleCardboardGamepad(enabled);

  if (!web_vr_mode_)
    ClosePresentationBindings();
}

void VrShellGl::ContentBoundsChanged(int width, int height) {
  TRACE_EVENT0("gpu", "VrShellGl::ContentBoundsChanged");
  ui_->OnContentBoundsChanged(width, height);
}

void VrShellGl::BufferBoundsChanged(const gfx::Size& content_buffer_size,
                                    const gfx::Size& overlay_buffer_size) {
  content_tex_buffer_size_ = content_buffer_size;
}

base::WeakPtr<VrShellGl> VrShellGl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<BrowserUiInterface> VrShellGl::GetBrowserUiWeakPtr() {
  return ui_->GetBrowserUiWeakPtr();
}

void VrShellGl::OnVSync(base::TimeTicks frame_time) {
  TRACE_EVENT0("gpu", "VrShellGl::OnVSync");
  // Create a synthetic VSync trace event for the reported last-VSync time. Use
  // this specific type since it appears to be the only one which supports
  // supplying a timestamp different from the current time, which is useful
  // since we seem to be >1ms behind the vsync time when we receive this call.
  //
  // See third_party/catapult/tracing/tracing/extras/vsync/vsync_auditor.html
  std::unique_ptr<base::trace_event::TracedValue> args =
      std::make_unique<base::trace_event::TracedValue>();
  args->SetDouble(
      "frame_time_us",
      static_cast<double>((frame_time - base::TimeTicks()).InMicroseconds()));
  TRACE_EVENT_INSTANT1("viz", "DisplayScheduler::BeginFrame",
                       TRACE_EVENT_SCOPE_THREAD, "args", std::move(args));

  while (premature_received_frames_ > 0) {
    TRACE_EVENT0("gpu", "VrShellGl::OnWebVRFrameAvailableRetry");
    --premature_received_frames_;
    OnWebVRFrameAvailable();
  }
  vsync_helper_.RequestVSync(
      base::BindRepeating(&VrShellGl::OnVSync, base::Unretained(this)));

  bool can_send_webvr_vsync = ui_->CanSendWebVrVSync();
  if (!last_should_send_webvr_vsync_ && can_send_webvr_vsync) {
    // We will start sending vsync to the WebVR page, so schedule the first
    // frame timeout.
    ScheduleOrCancelWebVrFrameTimeout();
  }
  last_should_send_webvr_vsync_ = can_send_webvr_vsync;

  // Process WebVR presenting VSync (VRDisplay rAF).
  if (!callback_.is_null() && can_send_webvr_vsync) {
    // A callback was stored by GetVSync. Use it now for sending a VSync.
    SendVSync(frame_time, base::ResetAndReturn(&callback_));
  } else {
    // We don't have a callback yet. Mark that there's a pending VSync
    // to indicate that the next GetVSync is allowed to call SendVSync
    // immediately.
    pending_vsync_ = true;
    pending_time_ = frame_time;
  }
  if (ShouldDrawWebVr()) {
    // When drawing WebVR, controller input doesn't need to be synchronized with
    // rendering as WebVR uses the gamepad api. To ensure we always handle input
    // like app button presses, update the controller here, but not in
    // DrawFrame.
    device::GvrDelegate::GetGvrPoseWithNeckModel(
        gvr_api_.get(), &render_info_primary_.head_pose);
    UpdateController(render_info_primary_, frame_time);
  } else {
    DrawFrame(-1, frame_time);
  }
}

void VrShellGl::GetVSync(GetVSyncCallback callback) {
  // In surfaceless (reprojecting) rendering, stay locked
  // to vsync intervals. Otherwise, for legacy Cardboard mode,
  // run requested animation frames now if it missed a vsync.
  if ((surfaceless_rendering_ && webvr_vsync_align_) || !pending_vsync_ ||
      !ui_->CanSendWebVrVSync()) {
    if (!callback_.is_null()) {
      mojo::ReportBadMessage(
          "Requested VSync before waiting for response to previous request.");
      ClosePresentationBindings();
      return;
    }
    callback_ = std::move(callback);
    return;
  }
  pending_vsync_ = false;
  SendVSync(pending_time_, std::move(callback));
}

void VrShellGl::ForceExitVr() {
  browser_->ForceExitVr();
}

namespace {
bool ValidateRect(const gfx::RectF& bounds) {
  // Bounds should be between 0 and 1, with positive width/height.
  // We simply clamp to [0,1], but still validate that the bounds are not NAN.
  return !std::isnan(bounds.width()) && !std::isnan(bounds.height()) &&
         !std::isnan(bounds.x()) && !std::isnan(bounds.y());
}

gfx::RectF ClampRect(gfx::RectF bounds) {
  bounds.AdjustToFit(gfx::RectF(0, 0, 1, 1));
  return bounds;
}

}  // namespace

void VrShellGl::UpdateLayerBounds(int16_t frame_index,
                                  const gfx::RectF& left_bounds,
                                  const gfx::RectF& right_bounds,
                                  const gfx::Size& source_size) {
  if (!ValidateRect(left_bounds) || !ValidateRect(right_bounds)) {
    mojo::ReportBadMessage("UpdateLayerBounds called with invalid bounds");
    binding_.Close();
    return;
  }

  if (frame_index >= 0 &&
      !webvr_frame_oustanding_[frame_index % kPoseRingBufferSize]) {
    mojo::ReportBadMessage("UpdateLayerBounds called with invalid frame_index");
    binding_.Close();
    return;
  }

  if (frame_index < 0) {
    webvr_left_viewport_->SetSourceUv(UVFromGfxRect(ClampRect(left_bounds)));
    webvr_right_viewport_->SetSourceUv(UVFromGfxRect(ClampRect(right_bounds)));
    CreateOrResizeWebVRSurface(source_size);

    // clear all pending bounds
    pending_bounds_ = base::queue<std::pair<uint8_t, WebVrBounds>>();
  } else {
    pending_bounds_.emplace(
        frame_index, WebVrBounds(left_bounds, right_bounds, source_size));
  }
}

base::TimeDelta VrShellGl::GetPredictedFrameTime() {
  base::TimeDelta frame_interval = vsync_helper_.DisplayVSyncInterval();
  // If we aim to submit at vsync, that frame will start scanning out
  // one vsync later. Add a half frame to split the difference between
  // left and right eye.
  base::TimeDelta js_time = webvr_js_time_.GetAverageOrDefault(frame_interval);
  base::TimeDelta render_time =
      webvr_render_time_.GetAverageOrDefault(frame_interval);
  base::TimeDelta overhead_time = frame_interval * 3 / 2;
  base::TimeDelta expected_frame_time = js_time + render_time + overhead_time;
  TRACE_COUNTER2("gpu", "WebVR frame time (ms)", "javascript",
                 js_time.InMilliseconds(), "rendering",
                 render_time.InMilliseconds());
  TRACE_COUNTER2("gpu", "GVR frame time (ms)", "acquire",
                 webvr_acquire_time_.GetAverage().InMilliseconds(), "submit",
                 webvr_submit_time_.GetAverage().InMilliseconds());
  TRACE_COUNTER1("gpu", "WebVR pose prediction (ms)",
                 expected_frame_time.InMilliseconds());
  return expected_frame_time;
}

bool VrShellGl::ShouldSkipVSync() {
  // Disable heuristic for traditional render path where we submit completed
  // frames.
  if (!webvr_use_gpu_fence_)
    return false;

  int16_t prev_idx =
      (frame_index_ + kPoseRingBufferSize - 1) % kPoseRingBufferSize;
  base::TimeTicks prev_js_submit = webvr_time_js_submit_[prev_idx];
  if (prev_js_submit.is_null())
    return false;

  base::TimeDelta frame_interval = vsync_helper_.DisplayVSyncInterval();
  base::TimeDelta mean_render_time =
      webvr_render_time_.GetAverageOrDefault(frame_interval);
  base::TimeDelta prev_render_time_left =
      mean_render_time - (base::TimeTicks::Now() - prev_js_submit);
  base::TimeDelta mean_js_time = webvr_js_time_.GetAverage();
  base::TimeDelta mean_js_wait = webvr_js_wait_time_.GetAverage();
  // We don't want the next frame to arrive too early. Estimated
  // time-to-new-frame is the net JavaScript time (not counting time spent
  // waiting) plus the net render time.
  //
  // Ideally we'd want the new frame to be ready one vsync interval after the
  // current frame finishes rendering, but allow being a half vsync early.
  if (mean_js_time - mean_js_wait + mean_render_time <
      prev_render_time_left + frame_interval / 2) {
    return true;
  }

  if (webvr_unstuff_ratelimit_frames_ > 0) {
    --webvr_unstuff_ratelimit_frames_;
  } else if (webvr_acquire_time_.GetAverage() >= kWebVrSlowAcquireThreshold &&
             mean_render_time < frame_interval) {
    webvr_unstuff_ratelimit_frames_ = kWebVrUnstuffMaxDropRate;
    return true;
  }
  return false;
}

void VrShellGl::SendVSync(base::TimeTicks time, GetVSyncCallback callback) {
  DVLOG(2) << __FUNCTION__;
  // There must not be a stored callback at this point, callers should use
  // ResetAndReturn to clear it before calling this method.
  DCHECK(!callback_);

  if (ShouldSkipVSync()) {
    callback_ = std::move(callback);
    return;
  }

  uint8_t frame_index = frame_index_++;

  TRACE_EVENT1("input", "VrShellGl::SendVSync", "frame", frame_index);

  int64_t prediction_nanos = GetPredictedFrameTime().InMicroseconds() * 1000;

  gfx::Transform head_mat;
  device::mojom::VRPosePtr pose =
      device::GvrDelegate::GetVRPosePtrWithNeckModel(gvr_api_.get(), &head_mat,
                                                     prediction_nanos);

  if (report_webxr_input_) {
    std::vector<device::mojom::XRInputSourceStatePtr> input_states;

    device::mojom::XRInputSourceStatePtr input_state =
        cardboard_ ? GetGazeInputSourceState()
                   : controller_->GetInputSourceState();

    input_states.push_back(std::move(input_state));
    pose->input_state = std::move(input_states);
  }

  webvr_head_pose_[frame_index % kPoseRingBufferSize] = head_mat;
  webvr_frame_oustanding_[frame_index % kPoseRingBufferSize] = true;
  webvr_time_pose_[frame_index % kPoseRingBufferSize] = base::TimeTicks::Now();

  std::move(callback).Run(
      std::move(pose), time - base::TimeTicks(), frame_index,
      device::mojom::VRPresentationProvider::VSyncStatus::SUCCESS);
}

void VrShellGl::ClosePresentationBindings() {
  webvr_frame_timeout_.Cancel();
  submit_client_.reset();
  if (!callback_.is_null()) {
    // When this Presentation provider is going away we have to respond to
    // pending callbacks, so instead of providing a VSync, tell the requester
    // the connection is closing.
    base::ResetAndReturn(&callback_)
        .Run(nullptr, base::TimeDelta(), -1,
             device::mojom::VRPresentationProvider::VSyncStatus::CLOSING);
  }
  binding_.Close();
}

device::mojom::XRInputSourceStatePtr VrShellGl::GetGazeInputSourceState() {
  device::mojom::XRInputSourceStatePtr state =
      device::mojom::XRInputSourceState::New();

  // Only one gaze input source to worry about, so it can have a static id.
  state->source_id = 1;

  // Report any trigger state changes made since the last call and reset the
  // state here.
  state->primary_input_pressed = cardboard_trigger_pressed_;
  state->primary_input_clicked = cardboard_trigger_clicked_;
  cardboard_trigger_clicked_ = false;

  state->description = device::mojom::XRInputSourceDescription::New();

  // It's a gaze-cursor-based device.
  state->description->pointer_origin = device::mojom::XRPointerOrigin::HEAD;
  state->description->emulated_position = true;

  // No implicit handedness
  state->description->handedness = device::mojom::XRHandedness::NONE;

  // Pointer and grip transforms are omitted since this is a gaze-based source.

  return state;
}

void VrShellGl::OnTriggerEvent(bool pressed) {
  if (pressed) {
    cardboard_trigger_pressed_ = true;
  } else if (cardboard_trigger_pressed_) {
    cardboard_trigger_pressed_ = false;
    cardboard_trigger_clicked_ = true;
  }
}

void VrShellGl::AcceptDoffPromptForTesting() {
  ui_->AcceptDoffPromptForTesting();
}

}  // namespace vr
