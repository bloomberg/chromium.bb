// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell_gl.h"

#include <chrono>
#include <limits>
#include <utility>

#include "base/android/jni_android.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/vr_shell/gl_browser_interface.h"
#include "chrome/browser/android/vr_shell/mailbox_to_surface_bridge.h"
#include "chrome/browser/android/vr_shell/vr_controller.h"
#include "chrome/browser/android/vr_shell/vr_metrics_util.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/android/vr_shell/vr_usage_monitor.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/fps_meter.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "chrome/browser/vr/vr_shell_renderer.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_gamepad_data_provider.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace vr_shell {

namespace {
static constexpr float kZNear = 0.1f;
// This should be kept fairly small with current reticle rendering technique
// which requires fairly high precision to draw on top of elements correctly.
static constexpr float kZFar = 100.0f;

// GVR buffer indices for use with viewport->SetSourceBufferIndex
// or frame.BindBuffer. We use one for world content (with reprojection)
// including main VrShell and WebVR content plus world-space UI.
// The headlocked buffer is for UI that should not use reprojection.
static constexpr int kFramePrimaryBuffer = 0;
static constexpr int kFrameHeadlockedBuffer = 1;

// Pixel dimensions and field of view for the head-locked content. This
// is currently sized to fit the WebVR "insecure transport" warnings,
// adjust it as needed if there is additional content.
static constexpr gfx::Size kHeadlockedBufferDimensions = {1024, 1024};
// Represents the frame of view in degrees. 0 degrees is straight ahead, and the
// rect represents bottom/left/right/top alway from the center line.
static constexpr gvr::Rectf kHeadlockedBufferFov = {30.f, 20.f, 20.f, 20.f};

// The GVR viewport list has two entries (left eye and right eye) for each
// GVR buffer.
static constexpr int kViewportListPrimaryOffset = 0;
static constexpr int kViewportListHeadlockedOffset = 2;

// Buffer size large enough to handle the current backlog of poses which is
// 2-3 frames.
static constexpr unsigned kPoseRingBufferSize = 8;

// Number of frames to use for sliding averages for pose timings,
// as used for estimating prediction times.
static constexpr unsigned kWebVRSlidingAverageSize = 5;

// Criteria for considering holding the app button in combination with
// controller movement as a gesture.
static constexpr float kMinAppButtonGestureAngleRad = 0.25;

// Timeout for checking for the WebVR rendering GL fence. If the timeout is
// reached, yield to let other tasks execute before rechecking.
static constexpr base::TimeDelta kWebVRFenceCheckTimeout =
    base::TimeDelta::FromMicroseconds(2000);

// Provides the direction the head is looking towards as a 3x1 unit vector.
gfx::Vector3dF GetForwardVector(const gfx::Transform& head_pose) {
  // Same as multiplying the inverse of the rotation component of the matrix by
  // (0, 0, -1, 0).
  return gfx::Vector3dF(-head_pose.matrix().get(2, 0),
                        -head_pose.matrix().get(2, 1),
                        -head_pose.matrix().get(2, 2));
}

gfx::Transform PerspectiveMatrixFromView(const gvr::Rectf& fov,
                                         float z_near,
                                         float z_far) {
  gfx::Transform result;
  const float x_left = -std::tan(fov.left * M_PI / 180.0f) * z_near;
  const float x_right = std::tan(fov.right * M_PI / 180.0f) * z_near;
  const float y_bottom = -std::tan(fov.bottom * M_PI / 180.0f) * z_near;
  const float y_top = std::tan(fov.top * M_PI / 180.0f) * z_near;

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

void TransformToGvrMat(const gfx::Transform& in, gvr::Mat4f* out) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      out->m[i][j] = in.matrix().get(i, j);
    }
  }
}

void GvrMatToTransform(const gvr::Mat4f& in, gfx::Transform* out) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      out->matrix().set(i, j, in.m[i][j]);
    }
  }
}

gvr::Rectf UVFromGfxRect(gfx::RectF rect) {
  return {rect.x(), rect.x() + rect.width(), 1.0f - rect.bottom(),
          1.0f - rect.y()};
}

gfx::RectF GfxRectFromUV(gvr::Rectf rect) {
  return gfx::RectF(rect.left, 1.0 - rect.top, rect.right - rect.left,
                    rect.top - rect.bottom);
}

double NowSeconds() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
}

void LoadControllerModelTask(
    base::WeakPtr<VrShellGl> weak_vr_shell_gl,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto controller_model = vr::VrControllerModel::LoadFromResources();
  if (controller_model) {
    task_runner->PostTask(
        FROM_HERE, base::Bind(&VrShellGl::SetControllerModel, weak_vr_shell_gl,
                              base::Passed(&controller_model)));
  }
}

}  // namespace

VrShellGl::VrShellGl(GlBrowserInterface* browser,
                     gvr_context* gvr_api,
                     bool initially_web_vr,
                     bool reprojected_rendering,
                     bool daydream_support,
                     vr::UiScene* scene)
    : web_vr_mode_(initially_web_vr),
      surfaceless_rendering_(reprojected_rendering),
      daydream_support_(daydream_support),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this),
      browser_(browser),
      scene_(scene),
      fps_meter_(new vr::FPSMeter()),
      webvr_js_time_(new vr::SlidingAverage(kWebVRSlidingAverageSize)),
      webvr_render_time_(new vr::SlidingAverage(kWebVRSlidingAverageSize)),
      weak_ptr_factory_(this) {
  GvrInit(gvr_api);
}

VrShellGl::~VrShellGl() {
  vsync_task_.Cancel();
  closePresentationBindings();
}

void VrShellGl::Initialize() {
  if (surfaceless_rendering_) {
    // If we're rendering surfaceless, we'll never get a java surface to render
    // into, so we can initialize GL right away.
    InitializeGl(nullptr);
  }
}

void VrShellGl::InitializeGl(gfx::AcceleratedWidget window) {
  CHECK(!ready_to_draw_);
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

  unsigned int textures[2];
  glGenTextures(2, textures);
  content_texture_id_ = textures[0];
  webvr_texture_id_ = textures[1];
  content_surface_texture_ = gl::SurfaceTexture::Create(content_texture_id_);
  webvr_surface_texture_ = gl::SurfaceTexture::Create(webvr_texture_id_);
  CreateContentSurface();
  content_surface_texture_->SetFrameAvailableCallback(base::Bind(
      &VrShellGl::OnContentFrameAvailable, weak_ptr_factory_.GetWeakPtr()));
  webvr_surface_texture_->SetFrameAvailableCallback(base::Bind(
      &VrShellGl::OnWebVRFrameAvailable, weak_ptr_factory_.GetWeakPtr()));
  content_surface_texture_->SetDefaultBufferSize(
      content_tex_physical_size_.width(), content_tex_physical_size_.height());

  InitializeRenderer();

  browser_->OnGLInitialized();

  gfx::Size webvr_size =
      device::GvrDelegate::GetRecommendedWebVrSize(gvr_api_.get());
  DVLOG(1) << __FUNCTION__ << ": resize initial to " << webvr_size.width()
           << "x" << webvr_size.height();

  CreateOrResizeWebVRSurface(webvr_size);

  vsync_task_.Reset(base::Bind(&VrShellGl::OnVSync, base::Unretained(this)));
  OnVSync();

  ready_to_draw_ = true;

  if (daydream_support_) {
    base::PostTaskWithTraits(
        FROM_HERE, {base::TaskPriority::BACKGROUND},
        base::Bind(LoadControllerModelTask, weak_ptr_factory_.GetWeakPtr(),
                   task_runner_));
  }

  input_manager_ = base::MakeUnique<vr::UiInputManager>(scene_, this);
  ui_renderer_ = base::MakeUnique<vr::UiRenderer>(scene_, content_texture_id_,
                                                  vr_shell_renderer_.get());
}

void VrShellGl::CreateContentSurface() {
  content_surface_ =
      base::MakeUnique<gl::ScopedJavaSurface>(content_surface_texture_.get());
  browser_->ContentSurfaceChanged(content_surface_->j_surface().obj());
}

void VrShellGl::CreateOrResizeWebVRSurface(const gfx::Size& size) {
  if (!webvr_surface_texture_) {
    DLOG(ERROR) << "No WebVR surface texture available";
    return;
  }

  // ContentPhysicalBoundsChanged is getting called twice with
  // identical sizes? Avoid thrashing the existing context.
  if (size == webvr_surface_size_) {
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
    mailbox_bridge_ = base::MakeUnique<MailboxToSurfaceBridge>();
    mailbox_bridge_->CreateSurface(webvr_surface_texture_.get());
  }
}

void VrShellGl::SubmitFrame(int16_t frame_index,
                            const gpu::MailboxHolder& mailbox) {
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
  submit_client_->OnSubmitFrameTransferred();
  if (!swapped) {
    // We dropped without drawing, report this as completed rendering
    // now to unblock the client. We're not going to receive it in
    // OnWebVRFrameAvailable where we'd normally report that.
    submit_client_->OnSubmitFrameRendered();
  }
}

void VrShellGl::ConnectPresentingService(
    device::mojom::VRSubmitFrameClientPtrInfo submit_client_info,
    device::mojom::VRPresentationProviderRequest request) {
  closePresentationBindings();
  submit_client_.Bind(std::move(submit_client_info));
  binding_.Bind(std::move(request));
}

void VrShellGl::OnContentFrameAvailable() {
  content_surface_texture_->UpdateTexImage();
}

void VrShellGl::OnWebVRFrameAvailable() {
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

  browser_->OnWebVrFrameAvailable();

  DrawFrame(frame_index);
}

void VrShellGl::GvrInit(gvr_context* gvr_api) {
  gvr_api_ = gvr::GvrApi::WrapNonOwned(gvr_api);
  controller_.reset(new VrController(gvr_api));

  VrMetricsUtil::LogVrViewerType(gvr_api_->GetViewerType());

  cardboard_ =
      (gvr_api_->GetViewerType() == gvr::ViewerType::GVR_VIEWER_TYPE_CARDBOARD);
  if (cardboard_ && web_vr_mode_) {
    browser_->ToggleCardboardGamepad(true);
  }
}

void VrShellGl::InitializeRenderer() {
  gvr_api_->InitializeGl();
  gfx::Transform head_pose;
  device::GvrDelegate::GetGvrPoseWithNeckModel(gvr_api_.get(), &head_pose);
  webvr_head_pose_.assign(kPoseRingBufferSize, head_pose);
  webvr_time_pose_.assign(kPoseRingBufferSize, base::TimeTicks());
  webvr_frame_oustanding_.assign(kPoseRingBufferSize, false);
  webvr_time_js_submit_.assign(kPoseRingBufferSize, base::TimeTicks());

  std::vector<gvr::BufferSpec> specs;
  // For kFramePrimaryBuffer (primary VrShell and WebVR content)
  specs.push_back(gvr_api_->CreateBufferSpec());
  gvr::Sizei render_size_primary = specs[kFramePrimaryBuffer].GetSize();
  render_size_primary_ = {render_size_primary.width,
                          render_size_primary.height};
  render_size_vrshell_ = render_size_primary_;

  // For kFrameHeadlockedBuffer (for WebVR insecure content warning).
  // Set this up at fixed resolution, the (smaller) FOV gets set below.
  specs.push_back(gvr_api_->CreateBufferSpec());
  specs.back().SetSize({kHeadlockedBufferDimensions.width(),
                        kHeadlockedBufferDimensions.height()});
  gvr::Sizei render_size_headlocked = specs[kFrameHeadlockedBuffer].GetSize();
  render_size_headlocked_ = {render_size_headlocked.width,
                             render_size_headlocked.height};

  swap_chain_ =
      base::MakeUnique<gvr::SwapChain>(gvr_api_->CreateSwapChain(specs));

  vr_shell_renderer_ = base::MakeUnique<vr::VrShellRenderer>();

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

  // Set up head-locked UI viewports, these will be elements 2=left eye
  // and 3=right eye. For now, use a hardcoded 20-degree-from-center FOV
  // frustum to reduce rendering cost for this overlay. This fits the
  // current content, but will need to be adjusted once there's more dynamic
  // head-locked content that could be larger.
  headlocked_left_viewport_.reset(
      new gvr::BufferViewport(gvr_api_->CreateBufferViewport()));
  buffer_viewport_list_->GetBufferViewport(GVR_LEFT_EYE,
                                           headlocked_left_viewport_.get());
  headlocked_left_viewport_->SetSourceBufferIndex(kFrameHeadlockedBuffer);
  headlocked_left_viewport_->SetReprojection(GVR_REPROJECTION_NONE);
  headlocked_left_viewport_->SetSourceFov(kHeadlockedBufferFov);

  headlocked_right_viewport_.reset(
      new gvr::BufferViewport(gvr_api_->CreateBufferViewport()));
  buffer_viewport_list_->GetBufferViewport(GVR_RIGHT_EYE,
                                           headlocked_right_viewport_.get());
  headlocked_right_viewport_->SetSourceBufferIndex(kFrameHeadlockedBuffer);
  headlocked_right_viewport_->SetReprojection(GVR_REPROJECTION_NONE);
  headlocked_right_viewport_->SetSourceFov(kHeadlockedBufferFov);

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

  browser_->GvrDelegateReady(gvr_api_->GetViewerType());
}

void VrShellGl::UpdateController(const gfx::Vector3dF& head_direction) {
  controller_->UpdateState(head_direction);
  controller_info_.laser_origin = controller_->GetPointerStart();

  device::GvrGamepadData controller_data = controller_->GetGamepadData();
  browser_->UpdateGamepadData(controller_data);
}

void VrShellGl::HandleControllerInput(const gfx::Vector3dF& head_direction) {
  if (is_exiting_) {
    // When we're exiting, we don't show the reticle and the only input
    // processing we do is to handle immediate exits.
    SendImmediateExitRequestIfNecessary();
    return;
  }

  HandleWebVrCompatibilityClick();

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

  if (ShouldDrawWebVr())
    return;

  controller_->GetTransform(&controller_info_.transform);
  std::unique_ptr<GestureList> gesture_list_ptr = controller_->DetectGestures();
  GestureList& gesture_list = *gesture_list_ptr;
  controller_info_.touchpad_button_state = vr::UiInputManager::ButtonState::UP;
  DCHECK(!(controller_->ButtonUpHappened(gvr::kControllerButtonClick) &&
           controller_->ButtonDownHappened(gvr::kControllerButtonClick)))
      << "Cannot handle a button down and up event within one frame.";
  if (touch_pending_) {
    controller_info_.touchpad_button_state =
        vr::UiInputManager::ButtonState::CLICKED;
    touch_pending_ = false;
  } else if (controller_->ButtonState(gvr::kControllerButtonClick)) {
    controller_info_.touchpad_button_state =
        vr::UiInputManager::ButtonState::DOWN;
  }
  controller_info_.app_button_state =
      controller_->ButtonState(gvr::kControllerButtonApp)
          ? vr::UiInputManager::ButtonState::DOWN
          : vr::UiInputManager::ButtonState::UP;
  controller_info_.home_button_state =
      controller_->ButtonState(gvr::kControllerButtonHome)
          ? vr::UiInputManager::ButtonState::DOWN
          : vr::UiInputManager::ButtonState::UP;
  controller_info_.opacity = controller_->GetOpacity();
  input_manager_->HandleInput(
      controller_direction, controller_info_.laser_origin,
      controller_info_.touchpad_button_state, &gesture_list,
      &controller_info_.target_point, &controller_info_.reticle_render_target);
}

void VrShellGl::HandleWebVrCompatibilityClick() {
  if (!ShouldDrawWebVr())
    return;

  // Process screen touch events for Cardboard button compatibility.
  // Also send tap events for controller "touchpad click" events.
  if (touch_pending_ ||
      controller_->ButtonUpHappened(gvr::kControllerButtonClick)) {
    touch_pending_ = false;
    auto gesture = base::MakeUnique<blink::WebGestureEvent>(
        blink::WebInputEvent::kGestureTapDown,
        blink::WebInputEvent::kNoModifiers, NowSeconds());
    gesture->source_device = blink::kWebGestureDeviceTouchpad;
    gesture->x = 0;
    gesture->y = 0;
    SendGestureToContent(std::move(gesture));
    DVLOG(1) << __FUNCTION__ << ": sent CLICK gesture";
  }
}

std::unique_ptr<blink::WebMouseEvent> VrShellGl::MakeMouseEvent(
    blink::WebInputEvent::Type type,
    const gfx::PointF& normalized_web_content_location) {
  gfx::Point location(
      content_tex_css_width_ * normalized_web_content_location.x(),
      content_tex_css_height_ * normalized_web_content_location.y());
  blink::WebInputEvent::Modifiers modifiers =
      controller_->ButtonState(gvr::kControllerButtonClick)
          ? blink::WebInputEvent::kLeftButtonDown
          : blink::WebInputEvent::kNoModifiers;
  base::TimeTicks timestamp;
  switch (type) {
    case blink::WebInputEvent::kMouseUp:
    case blink::WebInputEvent::kMouseDown:
      timestamp = controller_->GetLastButtonTimestamp();
      break;
    case blink::WebInputEvent::kMouseMove:
    case blink::WebInputEvent::kMouseEnter:
    case blink::WebInputEvent::kMouseLeave:
      timestamp = controller_->GetLastOrientationTimestamp();
      break;
    default:
      NOTREACHED();
  }

  auto mouse_event = base::MakeUnique<blink::WebMouseEvent>(
      type, modifiers, (timestamp - base::TimeTicks()).InSecondsF());
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event->button = blink::WebPointerProperties::Button::kLeft;
  mouse_event->SetPositionInWidget(location.x(), location.y());
  // TODO(mthiesse): Should we support double-clicks for input? What should the
  // timeout be?
  mouse_event->click_count = 1;

  return mouse_event;
}

void VrShellGl::UpdateGesture(const gfx::PointF& normalized_content_hit_point,
                              blink::WebGestureEvent& gesture) {
  gesture.x = content_tex_css_width_ * normalized_content_hit_point.x();
  gesture.y = content_tex_css_height_ * normalized_content_hit_point.y();
}

void VrShellGl::OnContentEnter(const gfx::PointF& normalized_hit_point) {
  SendGestureToContent(
      MakeMouseEvent(blink::WebInputEvent::kMouseEnter, normalized_hit_point));
}

void VrShellGl::OnContentLeave() {
  SendGestureToContent(
      MakeMouseEvent(blink::WebInputEvent::kMouseLeave, gfx::PointF()));
}

void VrShellGl::OnContentMove(const gfx::PointF& normalized_hit_point) {
  SendGestureToContent(
      MakeMouseEvent(blink::WebInputEvent::kMouseMove, normalized_hit_point));
}

void VrShellGl::OnContentDown(const gfx::PointF& normalized_hit_point) {
  SendGestureToContent(
      MakeMouseEvent(blink::WebInputEvent::kMouseDown, normalized_hit_point));
}

void VrShellGl::OnContentUp(const gfx::PointF& normalized_hit_point) {
  SendGestureToContent(
      MakeMouseEvent(blink::WebInputEvent::kMouseUp, normalized_hit_point));
}

void VrShellGl::OnContentFlingBegin(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToContent(std::move(gesture));
}

void VrShellGl::OnContentFlingCancel(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToContent(std::move(gesture));
}

void VrShellGl::OnContentScrollBegin(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToContent(std::move(gesture));
}

void VrShellGl::OnContentScrollUpdate(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToContent(std::move(gesture));
}

void VrShellGl::OnContentScrollEnd(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, *gesture);
  SendGestureToContent(std::move(gesture));
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
  }
  if (controller_->ButtonUpHappened(
          gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP)) {
    // A gesture is a movement of the controller while holding the App button.
    // If the angle of the movement is within a threshold, the action is
    // considered a regular click
    // TODO(asimjour1): We need to refactor the gesture recognition outside of
    // VrShellGl.
    vr::UiInterface::Direction direction = vr::UiInterface::NONE;
    gfx::Vector3dF a = controller_start_direction_;
    gfx::Vector3dF b = controller_direction;
    a.set_y(0);
    b.set_y(0);
    if (a.LengthSquared() * b.LengthSquared() > 0.0) {
      float gesture_xz_angle =
          acos(gfx::DotProduct(a, b) / a.Length() / b.Length());
      if (fabs(gesture_xz_angle) > kMinAppButtonGestureAngleRad) {
        direction = gesture_xz_angle < 0 ? vr::UiInterface::LEFT
                                         : vr::UiInterface::RIGHT;
        browser_->AppButtonGesturePerformed(direction);
      }
    }
    if (direction == vr::UiInterface::NONE)
      browser_->AppButtonClicked();
  }
}

void VrShellGl::SendGestureToContent(
    std::unique_ptr<blink::WebInputEvent> event) {
  browser_->ProcessContentGesture(std::move(event));
}

void VrShellGl::DrawFrame(int16_t frame_index) {
  TRACE_EVENT1("gpu", "VrShellGl::DrawFrame", "frame", frame_index);

  base::TimeTicks current_time = base::TimeTicks::Now();

  // Reset the viewport list to just the pair of viewports for the
  // primary buffer each frame. Head-locked viewports get added by
  // DrawVrShell if needed.
  buffer_viewport_list_->SetToRecommendedBufferViewports();

  // If needed, resize the primary buffer for use with WebVR. Resizing
  // needs to happen before acquiring a frame.
  if (ShouldDrawWebVr()) {
    // Process all pending_bounds_ changes targeted for before this
    // frame, being careful of wrapping frame indices.
    static constexpr unsigned max =
        std::numeric_limits<decltype(frame_index_)>::max();
    static_assert(max > kPoseRingBufferSize * 2,
                  "To detect wrapping, kPoseRingBufferSize must be smaller "
                  "than half of frame_index_ range.");
    while (!pending_bounds_.empty()) {
      uint16_t index = pending_bounds_.front().first;
      // If index is less than the frame_index it's possible we've
      // wrapped, so we extend the range and 'un-wrap' to account
      // for this.
      if (index < frame_index)
        index += max;
      // If the pending bounds change is for an upcoming frame
      // within our buffer size, wait to apply it. Otherwise, apply
      // it immediately. This guarantees that even if we miss many
      // frames, the queue can't fill up with stale bounds.
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
    buffer_viewport_list_->SetBufferViewport(GVR_LEFT_EYE,
                                             *webvr_left_viewport_);
    buffer_viewport_list_->SetBufferViewport(GVR_RIGHT_EYE,
                                             *webvr_right_viewport_);
    if (render_info_primary_.content_texture_size != webvr_surface_size_) {
      if (!webvr_surface_size_.width()) {
        // Don't try to resize to 0x0 pixels, drop frames until we get a
        // valid size.
        return;
      }

      render_info_primary_.content_texture_size = webvr_surface_size_;
      DVLOG(1) << __FUNCTION__ << ": resize GVR to "
               << render_info_primary_.content_texture_size.width() << "x"
               << render_info_primary_.content_texture_size.height();
      swap_chain_->ResizeBuffer(
          kFramePrimaryBuffer,
          {render_info_primary_.content_texture_size.width(),
           render_info_primary_.content_texture_size.height()});
    }
  } else {
    if (render_info_primary_.content_texture_size != render_size_vrshell_) {
      render_info_primary_.content_texture_size = render_size_vrshell_;
      swap_chain_->ResizeBuffer(
          kFramePrimaryBuffer,
          {render_info_primary_.content_texture_size.width(),
           render_info_primary_.content_texture_size.height()});
    }
  }

  TRACE_EVENT_BEGIN0("gpu", "VrShellGl::AcquireFrame");
  gvr::Frame frame = swap_chain_->AcquireFrame();
  TRACE_EVENT_END0("gpu", "VrShellGl::AcquireFrame");
  if (!frame.is_valid()) {
    return;
  }
  frame.BindBuffer(kFramePrimaryBuffer);

  if (ShouldDrawWebVr()) {
    DrawWebVr();
  }

  // When using async reprojection, we need to know which pose was
  // used in the WebVR app for drawing this frame and supply it when
  // submitting. Technically we don't need a pose if not reprojecting,
  // but keeping it uninitialized seems likely to cause problems down
  // the road. Copying it is cheaper than fetching a new one.
  if (ShouldDrawWebVr()) {
    static_assert(!((kPoseRingBufferSize - 1) & kPoseRingBufferSize),
                  "kPoseRingBufferSize must be a power of 2");
    render_info_primary_.head_pose =
        webvr_head_pose_[frame_index % kPoseRingBufferSize];
    webvr_frame_oustanding_[frame_index % kPoseRingBufferSize] = false;
  } else {
    device::GvrDelegate::GetGvrPoseWithNeckModel(
        gvr_api_.get(), &render_info_primary_.head_pose);
  }

  // Update the render position of all UI elements (including desktop).
  scene_->OnBeginFrame(current_time);

  {
    // TODO(crbug.com/704690): Acquire controller state in a way that's timely
    // for both the gamepad API and UI input handling.
    TRACE_EVENT0("gpu", "VrShellGl::UpdateController");
    gfx::Vector3dF head_direction =
        GetForwardVector(render_info_primary_.head_pose);
    UpdateController(head_direction);
    HandleControllerInput(head_direction);
  }

  // Ensure that all elements are ready before drawing. Eg., elements may have
  // been dirtied due to animation on input processing and need to regenerate
  // textures.
  scene_->PrepareToDraw();

  UpdateEyeInfos(render_info_primary_.head_pose, kViewportListPrimaryOffset,
                 render_info_primary_.content_texture_size,
                 &render_info_primary_);
  ui_renderer_->Draw(render_info_primary_, controller_info_, ShouldDrawWebVr());

  frame.Unbind();

  // Draw head-locked elements to a separate, non-reprojected buffer.
  if (scene_->HasVisibleHeadLockedElements()) {
    frame.BindBuffer(kFrameHeadlockedBuffer);

    // Add head-locked viewports. The list gets reset to just
    // the recommended viewports (for the primary buffer) each frame.
    buffer_viewport_list_->SetBufferViewport(
        kViewportListHeadlockedOffset + GVR_LEFT_EYE,
        *headlocked_left_viewport_);
    buffer_viewport_list_->SetBufferViewport(
        kViewportListHeadlockedOffset + GVR_RIGHT_EYE,
        *headlocked_right_viewport_);

    UpdateEyeInfos(render_info_headlocked_.head_pose,
                   kViewportListHeadlockedOffset, render_size_headlocked_,
                   &render_info_headlocked_);
    ui_renderer_->DrawHeadLocked(render_info_headlocked_, controller_info_);
    frame.Unbind();
  }

  if (ShouldDrawWebVr() && surfaceless_rendering_) {
    // Continue with submit once a GL fence signals that current drawing
    // operations have completed.
    std::unique_ptr<gl::GLFenceEGL> fence = base::MakeUnique<gl::GLFenceEGL>();
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VrShellGl::DrawFrameSubmitWhenReady,
                   weak_ptr_factory_.GetWeakPtr(), frame_index, frame.release(),
                   render_info_primary_.head_pose, base::Passed(&fence)));
  } else {
    // Continue with submit immediately.
    DrawFrameSubmitWhenReady(frame_index, frame.release(),
                             render_info_primary_.head_pose, nullptr);
  }
}

void VrShellGl::UpdateEyeInfos(const gfx::Transform& head_pose,
                               int viewport_offset,
                               const gfx::Size& render_size,
                               vr::RenderInfo* out_render_info) {
  for (auto eye : {GVR_LEFT_EYE, GVR_RIGHT_EYE}) {
    vr::RenderInfo::EyeInfo& eye_info = (eye == GVR_LEFT_EYE)
                                            ? out_render_info->left_eye_info
                                            : out_render_info->right_eye_info;

    buffer_viewport_list_->GetBufferViewport(eye + viewport_offset,
                                             buffer_viewport_.get());

    gfx::Transform eye_matrix;
    GvrMatToTransform(gvr_api_->GetEyeFromHeadMatrix(eye), &eye_matrix);
    eye_info.view_matrix = eye_matrix * head_pose;

    const gfx::RectF& rect = GfxRectFromUV(buffer_viewport_->GetSourceUv());
    eye_info.viewport = vr::CalculatePixelSpaceRect(render_size, rect);

    eye_info.proj_matrix = PerspectiveMatrixFromView(
        buffer_viewport_->GetSourceFov(), kZNear, kZFar);
    eye_info.view_proj_matrix = eye_info.proj_matrix * eye_info.view_matrix;
  }
}

void VrShellGl::DrawFrameSubmitWhenReady(
    int16_t frame_index,
    gvr_frame* frame_ptr,
    const gfx::Transform& head_pose,
    std::unique_ptr<gl::GLFenceEGL> fence) {
  if (fence) {
    fence->ClientWaitWithTimeoutNanos(kWebVRFenceCheckTimeout.InMicroseconds() *
                                      1000);
    if (!fence->HasCompleted()) {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&VrShellGl::DrawFrameSubmitWhenReady,
                                weak_ptr_factory_.GetWeakPtr(), frame_index,
                                frame_ptr, head_pose, base::Passed(&fence)));
      return;
    }
  }

  TRACE_EVENT1("gpu", "VrShellGl::DrawFrameSubmitWhenReady", "frame",
               frame_index);

  gvr::Frame frame(frame_ptr);
  gvr::Mat4f mat;
  TransformToGvrMat(head_pose, &mat);
  frame.Submit(*buffer_viewport_list_, mat);

  // No need to swap buffers for surfaceless rendering.
  if (!surfaceless_rendering_) {
    // TODO(mthiesse): Support asynchronous SwapBuffers.
    TRACE_EVENT0("gpu", "VrShellGl::SwapBuffers");
    surface_->SwapBuffers();
  }

  // Report rendering completion to WebVR so that it's permitted to submit
  // a fresh frame. We could do this earlier, as soon as the frame got pulled
  // off the transfer surface, but that appears to result in overstuffed
  // buffers.
  if (submit_client_) {
    submit_client_->OnSubmitFrameRendered();
  }

  if (ShouldDrawWebVr()) {
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeTicks pose_time =
        webvr_time_pose_[frame_index % kPoseRingBufferSize];
    base::TimeTicks js_submit_time =
        webvr_time_js_submit_[frame_index % kPoseRingBufferSize];
    int64_t pose_to_js_submit_us =
        (js_submit_time - pose_time).InMicroseconds();
    webvr_js_time_->AddSample(pose_to_js_submit_us);
    int64_t js_submit_to_gvr_submit_us =
        (now - js_submit_time).InMicroseconds();
    webvr_render_time_->AddSample(js_submit_to_gvr_submit_us);
  }

  // After saving the timestamp, fps will be available via GetFPS().
  // TODO(vollick): enable rendering of this framerate in a HUD.
  fps_meter_->AddFrame(base::TimeTicks::Now());
  DVLOG(1) << "fps: " << fps_meter_->GetFPS();
  TRACE_COUNTER1("gpu", "WebVR FPS", fps_meter_->GetFPS());
}

bool VrShellGl::ShouldDrawWebVr() {
  return web_vr_mode_ && scene_->web_vr_rendering_enabled();
}

void VrShellGl::DrawWebVr() {
  TRACE_EVENT0("gpu", "VrShellGl::DrawWebVr");
  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_POLYGON_OFFSET_FILL);

  // We're redrawing over the entire viewport, but it's generally more
  // efficient on mobile tiling GPUs to clear anyway as a hint that
  // we're done with the old content. TODO(klausw,crbug.com/700389):
  // investigate using glDiscardFramebufferEXT here since that's more
  // efficient on desktop, but it would need a capability check since
  // it's not supported on older devices such as Nexus 5X.
  glClear(GL_COLOR_BUFFER_BIT);

  glViewport(0, 0, webvr_surface_size_.width(), webvr_surface_size_.height());
  vr_shell_renderer_->GetWebVrRenderer()->Draw(webvr_texture_id_);
}

void VrShellGl::OnTriggerEvent() {
  // Set a flag to handle this on the render thread at the next frame.
  touch_pending_ = true;
}

void VrShellGl::OnPause() {
  vsync_task_.Cancel();
  controller_->OnPause();
  gvr_api_->PauseTracking();
}

void VrShellGl::OnResume() {
  gvr_api_->RefreshViewerProfile();
  gvr_api_->ResumeTracking();
  controller_->OnResume();
  if (ready_to_draw_) {
    vsync_task_.Reset(base::Bind(&VrShellGl::OnVSync, base::Unretained(this)));
    OnVSync();
  }
}

void VrShellGl::SetWebVrMode(bool enabled) {
  web_vr_mode_ = enabled;

  if (cardboard_) {
    browser_->ToggleCardboardGamepad(enabled);
  }

  if (!enabled) {
    closePresentationBindings();
  }
}

void VrShellGl::ContentBoundsChanged(int width, int height) {
  TRACE_EVENT0("gpu", "VrShellGl::ContentBoundsChanged");
  content_tex_css_width_ = width;
  content_tex_css_height_ = height;
}

void VrShellGl::ContentPhysicalBoundsChanged(int width, int height) {
  if (content_surface_texture_.get())
    content_surface_texture_->SetDefaultBufferSize(width, height);
  content_tex_physical_size_.set_width(width);
  content_tex_physical_size_.set_height(height);
}

base::WeakPtr<VrShellGl> VrShellGl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void VrShellGl::SetControllerModel(
    std::unique_ptr<vr::VrControllerModel> model) {
  vr_shell_renderer_->GetControllerRenderer()->SetUp(std::move(model));
}

void VrShellGl::OnVSync() {
  while (premature_received_frames_ > 0) {
    TRACE_EVENT0("gpu", "VrShellGl::OnWebVRFrameAvailableRetry");
    --premature_received_frames_;
    OnWebVRFrameAvailable();
  }

  // Don't send VSyncs until we have a timebase/interval.
  if (vsync_interval_.is_zero())
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks target = now + vsync_interval_;
  double intervalsF =
      (target - vsync_timebase_).InSecondsF() / vsync_interval_.InSecondsF();
  uint64_t intervals = std::llround(intervalsF);
  target = vsync_timebase_ + intervals * vsync_interval_;
  task_runner_->PostDelayedTask(FROM_HERE, vsync_task_.callback(),
                                target - now);

  base::TimeDelta current_time = target - vsync_interval_ - base::TimeTicks();
  if (!callback_.is_null()) {
    SendVSync(current_time, base::ResetAndReturn(&callback_));
  } else {
    pending_vsync_ = true;
    pending_time_ = current_time;
  }
  if (!ShouldDrawWebVr()) {
    DrawFrame(-1);
  }
}

void VrShellGl::GetVSync(GetVSyncCallback callback) {
  // In surfaceless (reprojecting) rendering, stay locked
  // to vsync intervals. Otherwise, for legacy Cardboard mode,
  // run requested animation frames now if it missed a vsync.
  if (surfaceless_rendering_ || !pending_vsync_) {
    if (!callback_.is_null()) {
      mojo::ReportBadMessage(
          "Requested VSync before waiting for response to previous request.");
      closePresentationBindings();
      return;
    }
    callback_ = std::move(callback);
    return;
  }
  pending_vsync_ = false;
  SendVSync(pending_time_, std::move(callback));
}

void VrShellGl::UpdateVSyncInterval(base::TimeTicks vsync_timebase,
                                    base::TimeDelta vsync_interval) {
  bool needs_init = vsync_timebase_ == base::TimeTicks();
  vsync_timebase_ = vsync_timebase;
  vsync_interval_ = vsync_interval;
  if (needs_init) {
    vsync_task_.Reset(base::Bind(&VrShellGl::OnVSync, base::Unretained(this)));
    OnVSync();
  }
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
    pending_bounds_ = std::queue<std::pair<uint8_t, WebVrBounds>>();
  } else {
    pending_bounds_.emplace(
        frame_index, WebVrBounds(left_bounds, right_bounds, source_size));
  }
}

int64_t VrShellGl::GetPredictedFrameTimeNanos() {
  int64_t frame_time_micros = vsync_interval_.InMicroseconds();
  // If we aim to submit at vsync, that frame will start scanning out
  // one vsync later. Add a half frame to split the difference between
  // left and right eye.
  int64_t js_micros = webvr_js_time_->GetAverageOrDefault(frame_time_micros);
  int64_t render_micros =
      webvr_render_time_->GetAverageOrDefault(frame_time_micros);
  int64_t overhead_micros = frame_time_micros * 3 / 2;
  int64_t expected_frame_micros = js_micros + render_micros + overhead_micros;
  TRACE_COUNTER2("gpu", "WebVR frame time (ms)", "javascript",
                 js_micros / 1000.0, "rendering", render_micros / 1000.0);
  TRACE_COUNTER1("gpu", "WebVR pose prediction (ms)",
                 expected_frame_micros / 1000.0);
  return expected_frame_micros * 1000;
}

void VrShellGl::SendVSync(base::TimeDelta time, GetVSyncCallback callback) {
  uint8_t frame_index = frame_index_++;

  TRACE_EVENT1("input", "VrShellGl::SendVSync", "frame", frame_index);

  int64_t prediction_nanos = GetPredictedFrameTimeNanos();

  gfx::Transform head_mat;
  device::mojom::VRPosePtr pose =
      device::GvrDelegate::GetVRPosePtrWithNeckModel(gvr_api_.get(), &head_mat,
                                                     prediction_nanos);

  webvr_head_pose_[frame_index % kPoseRingBufferSize] = head_mat;
  webvr_frame_oustanding_[frame_index % kPoseRingBufferSize] = true;
  webvr_time_pose_[frame_index % kPoseRingBufferSize] = base::TimeTicks::Now();

  std::move(callback).Run(
      std::move(pose), time, frame_index,
      device::mojom::VRPresentationProvider::VSyncStatus::SUCCESS);
}

void VrShellGl::CreateVRDisplayInfo(
    const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
    uint32_t device_id) {
  // This assumes that the initial webvr_surface_size_ was set to the
  // appropriate recommended render resolution as the default size during
  // InitializeGl. Revisit if the initialization order changes.
  device::mojom::VRDisplayInfoPtr info =
      device::GvrDelegate::CreateVRDisplayInfo(gvr_api_.get(),
                                               webvr_surface_size_, device_id);
  browser_->RunVRDisplayInfoCallback(callback, &info);
}

void VrShellGl::closePresentationBindings() {
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

}  // namespace vr_shell
