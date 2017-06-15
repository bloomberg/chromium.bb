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
#include "chrome/browser/android/vr_shell/fps_meter.h"
#include "chrome/browser/android/vr_shell/gl_browser_interface.h"
#include "chrome/browser/android/vr_shell/mailbox_to_surface_bridge.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"
#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"
#include "chrome/browser/android/vr_shell/vr_controller.h"
#include "chrome/browser/android/vr_shell/vr_gl_util.h"
#include "chrome/browser/android/vr_shell/vr_metrics_util.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/android/vr_shell/vr_shell_renderer.h"
#include "chrome/browser/android/vr_shell/vr_usage_monitor.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_gamepad_data_provider.h"
#include "device/vr/vr_math.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
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

static constexpr float kReticleWidth = 0.025f;
static constexpr float kReticleHeight = 0.025f;

static constexpr float kLaserWidth = 0.01f;

static constexpr gfx::Point3F kOrigin = {0.0f, 0.0f, 0.0f};

// Fraction of the distance to the object the reticle is drawn at to avoid
// rounding errors drawing the reticle behind the object.
// TODO(mthiesse): Find a better approach for drawing the reticle on an object.
// Right now we have to wedge it very precisely between the content window and
// backplane to avoid rendering artifacts.
static constexpr float kReticleOffset = 0.999f;

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
static constexpr gvr::Rectf kHeadlockedBufferFov = {20.f, 20.f, 20.f, 20.f};

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

// Interval for checking for the WebVR rendering GL fence. Ideally we'd want to
// wait for completion with a short timeout, but GLFenceEGL doesn't currently
// support that, see TODO(klausw,crbug.com/726026).
static constexpr base::TimeDelta kWebVRFenceCheckInterval =
    base::TimeDelta::FromMicroseconds(250);

static constexpr gfx::PointF kInvalidTargetPoint =
    gfx::PointF(std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max());

static constexpr float kEpsilon = 1e-6f;

gfx::Point3F GetRayPoint(const gfx::Point3F& rayOrigin,
                         const gfx::Vector3dF& rayVector,
                         float scale) {
  return rayOrigin + gfx::ScaleVector3d(rayVector, scale);
}

// Provides the direction the head is looking towards as a 3x1 unit vector.
gfx::Vector3dF GetForwardVector(const gfx::Transform& head_pose) {
  // Same as multiplying the inverse of the rotation component of the matrix by
  // (0, 0, -1, 0).
  return gfx::Vector3dF(-head_pose.matrix().get(2, 0),
                        -head_pose.matrix().get(2, 1),
                        -head_pose.matrix().get(2, 2));
}

// Generate a quaternion representing the rotation from the negative Z axis
// (0, 0, -1) to a specified vector. This is an optimized version of a more
// general vector-to-vector calculation.
gfx::Quaternion GetRotationFromZAxis(gfx::Vector3dF vec) {
  vec.GetNormalized(&vec);
  gfx::Quaternion quat;
  quat.set_w(1.0f - vec.z());
  if (quat.w() < kEpsilon) {
    // Degenerate case: vectors are exactly opposite. Replace by an
    // arbitrary 180 degree rotation to avoid invalid normalization.
    return gfx::Quaternion(1, 0, 0, 0);
  }

  quat.set_x(vec.y());
  quat.set_y(-vec.x());
  quat.set_z(0.0);
  return quat.Normalized();
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

std::unique_ptr<blink::WebMouseEvent> MakeMouseEvent(
    blink::WebInputEvent::Type type,
    double timestamp,
    const gfx::Point& location,
    bool down) {
  blink::WebInputEvent::Modifiers modifiers =
      down ? blink::WebInputEvent::kLeftButtonDown
           : blink::WebInputEvent::kNoModifiers;
  std::unique_ptr<blink::WebMouseEvent> mouse_event(
      new blink::WebMouseEvent(type, modifiers, timestamp));
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event->button = blink::WebPointerProperties::Button::kLeft;
  mouse_event->SetPositionInWidget(location.x(), location.y());
  mouse_event->click_count = 1;

  return mouse_event;
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
  auto controller_model = VrControllerModel::LoadFromResources();
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
                     UiScene* scene)
    : web_vr_mode_(initially_web_vr),
      surfaceless_rendering_(reprojected_rendering),
      daydream_support_(daydream_support),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this),
      browser_(browser),
      scene_(scene),
      fps_meter_(new FPSMeter()),
      webvr_js_time_(new SlidingAverage(kWebVRSlidingAverageSize)),
      webvr_render_time_(new SlidingAverage(kWebVRSlidingAverageSize)),
      weak_ptr_factory_(this) {
  GvrInit(gvr_api);
}

VrShellGl::~VrShellGl() {
  vsync_task_.Cancel();
  // TODO(mthiesse): Can we omit the Close() here? Concern is that if
  // both ends of the connection ever live in the same process for
  // some reason, we could receive another VSync request in response
  // to the closing message in the destructor but fail to respond to
  // the callback.
  binding_.Close();
  if (!callback_.is_null()) {
    // When this VSync provider is going away we have to respond to pending
    // callbacks, so instead of providing a VSync, tell the requester to try
    // again. A VSyncProvider is guaranteed to exist, so the request in response
    // to this message will go through some other VSyncProvider.
    base::ResetAndReturn(&callback_)
        .Run(nullptr, base::TimeDelta(), -1,
             device::mojom::VRVSyncProvider::Status::CLOSING);
  }
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

  scene_->OnGLInitialized();

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

void VrShellGl::SubmitWebVRFrame(int16_t frame_index,
                                 const gpu::MailboxHolder& mailbox) {
  DCHECK(submit_client_.get());
  TRACE_EVENT0("gpu", "VrShellGl::SubmitWebVRFrame");

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

void VrShellGl::SetSubmitClient(
    device::mojom::VRSubmitFrameClientPtrInfo submit_client_info) {
  submit_client_.Bind(std::move(submit_client_info));
}

void VrShellGl::OnContentFrameAvailable() {
  content_surface_texture_->UpdateTexImage();
  received_frame_ = true;
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

  DrawFrame(frame_index);
}

void VrShellGl::GvrInit(gvr_context* gvr_api) {
  gvr_api_ = gvr::GvrApi::WrapNonOwned(gvr_api);
  controller_.reset(new VrController(gvr_api));

  VrMetricsUtil::LogVrViewerType(gvr_api);

  cardboard_ =
      (gvr_api_->GetViewerType() == gvr::ViewerType::GVR_VIEWER_TYPE_CARDBOARD);
  if (cardboard_ && web_vr_mode_) {
    browser_->ToggleCardboardGamepad(true);
  }
}

void VrShellGl::InitializeRenderer() {
  gvr_api_->InitializeGl();
  gfx::Transform head_pose;
  vr::Mat4f from_gvr;
  device::GvrDelegate::GetGvrPoseWithNeckModel(gvr_api_.get(), &from_gvr);
  head_pose = vr::ToTransform(from_gvr);
  webvr_head_pose_.assign(kPoseRingBufferSize, head_pose);
  webvr_time_pose_.assign(kPoseRingBufferSize, base::TimeTicks());
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

  vr_shell_renderer_ = base::MakeUnique<VrShellRenderer>();

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

  browser_->GvrDelegateReady();
}

void VrShellGl::UpdateController(const gfx::Vector3dF& head_direction) {
  controller_->UpdateState(head_direction);
  pointer_start_ = controller_->GetPointerStart();

  device::GvrGamepadData controller_data = controller_->GetGamepadData();
  browser_->UpdateGamepadData(controller_data);
}

void VrShellGl::HandleControllerInput(const gfx::Vector3dF& head_direction) {
  if (scene_->is_exiting()) {
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
    controller_quat_ = GetRotationFromZAxis(head_direction);
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
  gfx::PointF target_local_point(kInvalidTargetPoint);
  gfx::Vector3dF eye_to_target;
  reticle_render_target_ = nullptr;
  GetVisualTargetElement(controller_direction, eye_to_target, target_point_,
                         &reticle_render_target_, target_local_point);

  UiElement* target_element = nullptr;
  if (input_locked_element_) {
    gfx::Point3F plane_intersection_point;
    float distance_to_plane;
    if (!GetTargetLocalPoint(eye_to_target, *input_locked_element_,
                             2 * scene_->GetBackgroundDistance(),
                             target_local_point, plane_intersection_point,
                             distance_to_plane)) {
      target_local_point = kInvalidTargetPoint;
    }
    target_element = input_locked_element_;
  } else if (!in_scroll_ && !in_click_) {
    target_element = reticle_render_target_;
  }

  // Handle input targeting on the content quad, ignoring any other elements.
  // Content is treated specially to accomodate scrolling, flings, etc.
  gfx::Point local_point_pixels;
  if (target_element && (target_element->fill() == Fill::CONTENT)) {
    local_point_pixels.set_x(content_tex_css_width_ * target_local_point.x());
    local_point_pixels.set_y(content_tex_css_height_ * target_local_point.y());
  }
  std::unique_ptr<GestureList> gesture_list_ptr = controller_->DetectGestures();
  GestureList& gesture_list = *gesture_list_ptr;
  for (const std::unique_ptr<blink::WebGestureEvent>& gesture : gesture_list) {
    gesture->x = local_point_pixels.x();
    gesture->y = local_point_pixels.y();
  }
  SendFlingCancel(gesture_list);
  // For simplicity, don't allow scrolling while clicking until we need to.
  if (!in_click_) {
    SendScrollEnd(gesture_list);
    if (!SendScrollBegin(target_element, gesture_list)) {
      SendScrollUpdate(gesture_list);
    }
  }
  // If we're still scrolling, don't hover (and we can't be clicking, because
  // click ends scroll).
  if (in_scroll_)
    return;
  SendHoverLeave(target_element);
  if (!SendHoverEnter(target_element, target_local_point, local_point_pixels)) {
    SendHoverMove(target_local_point, local_point_pixels);
  }
  SendButtonDown(target_element, target_local_point, local_point_pixels);
  if (!SendButtonUp(target_element, target_local_point, local_point_pixels))
    SendTap(target_element, target_local_point, local_point_pixels);
}

void VrShellGl::HandleWebVrCompatibilityClick() {
  if (!ShouldDrawWebVr())
    return;

  // Process screen touch events for Cardboard button compatibility.
  // Also send tap events for controller "touchpad click" events.
  if (touch_pending_ ||
      controller_->ButtonUpHappened(gvr::kControllerButtonClick)) {
    touch_pending_ = false;
    std::unique_ptr<blink::WebGestureEvent> gesture(new blink::WebGestureEvent(
        blink::WebInputEvent::kGestureTapDown,
        blink::WebInputEvent::kNoModifiers, NowSeconds()));
    gesture->source_device = blink::kWebGestureDeviceTouchpad;
    gesture->x = 0;
    gesture->y = 0;
    SendGestureToContent(std::move(gesture));
    DVLOG(1) << __FUNCTION__ << ": sent CLICK gesture";
  }
}

void VrShellGl::SendFlingCancel(GestureList& gesture_list) {
  if (!fling_target_)
    return;
  if (gesture_list.empty() || (gesture_list.front()->GetType() !=
                               blink::WebInputEvent::kGestureFlingCancel))
    return;
  // Scrolling currently only supported on content window.
  DCHECK_EQ(fling_target_->fill(), Fill::CONTENT);
  SendGestureToContent(std::move(gesture_list.front()));
  gesture_list.erase(gesture_list.begin());
}

void VrShellGl::SendScrollEnd(GestureList& gesture_list) {
  if (!in_scroll_)
    return;
  DCHECK_NE(input_locked_element_, nullptr);
  if (controller_->ButtonDownHappened(gvr::kControllerButtonClick)) {
    DCHECK_GT(gesture_list.size(), 0LU);
    DCHECK_EQ(gesture_list.front()->GetType(),
              blink::WebInputEvent::kGestureScrollEnd);
  }
  // Scrolling currently only supported on content window.
  DCHECK_EQ(input_locked_element_->fill(), Fill::CONTENT);
  if (gesture_list.empty() || (gesture_list.front()->GetType() !=
                               blink::WebInputEvent::kGestureScrollEnd))
    return;
  DCHECK_LE(gesture_list.size(), 2LU);
  SendGestureToContent(std::move(gesture_list.front()));
  gesture_list.erase(gesture_list.begin());
  if (!gesture_list.empty()) {
    DCHECK_EQ(gesture_list.front()->GetType(),
              blink::WebInputEvent::kGestureFlingStart);
    SendGestureToContent(std::move(gesture_list.front()));
    fling_target_ = input_locked_element_;
    gesture_list.erase(gesture_list.begin());
  }
  input_locked_element_ = nullptr;
  in_scroll_ = false;
}

bool VrShellGl::SendScrollBegin(UiElement* target, GestureList& gesture_list) {
  if (in_scroll_ || !target)
    return false;
  // Scrolling currently only supported on content window.
  if (target->fill() != Fill::CONTENT)
    return false;
  if (gesture_list.empty() || (gesture_list.front()->GetType() !=
                               blink::WebInputEvent::kGestureScrollBegin))
    return false;
  input_locked_element_ = target;
  in_scroll_ = true;

  SendGestureToContent(std::move(gesture_list.front()));
  gesture_list.erase(gesture_list.begin());
  return true;
}

void VrShellGl::SendScrollUpdate(GestureList& gesture_list) {
  if (!in_scroll_)
    return;
  DCHECK(input_locked_element_);
  if (gesture_list.empty() || (gesture_list.front()->GetType() !=
                               blink::WebInputEvent::kGestureScrollUpdate))
    return;
  // Scrolling currently only supported on content window.
  DCHECK_EQ(input_locked_element_->fill(), Fill::CONTENT);
  SendGestureToContent(std::move(gesture_list.front()));
  gesture_list.erase(gesture_list.begin());
}

void VrShellGl::SendHoverLeave(UiElement* target) {
  if (!hover_target_ || (target == hover_target_))
    return;
  if (hover_target_->fill() == Fill::CONTENT) {
    SendGestureToContent(MakeMouseEvent(blink::WebInputEvent::kMouseLeave,
                                        NowSeconds(), gfx::Point(), in_click_));
  } else {
    hover_target_->OnHoverLeave();
  }
  hover_target_ = nullptr;
}

bool VrShellGl::SendHoverEnter(UiElement* target,
                               const gfx::PointF& target_point,
                               const gfx::Point& local_point_pixels) {
  if (!target || target == hover_target_)
    return false;
  if (target->fill() == Fill::CONTENT) {
    SendGestureToContent(MakeMouseEvent(blink::WebInputEvent::kMouseEnter,
                                        NowSeconds(), local_point_pixels,
                                        in_click_));
  } else {
    target->OnHoverEnter(target_point);
  }
  hover_target_ = target;
  return true;
}

void VrShellGl::SendHoverMove(const gfx::PointF& target_point,
                              const gfx::Point& local_point_pixels) {
  if (!hover_target_)
    return;
  if (hover_target_->fill() == Fill::CONTENT) {
    // TODO(mthiesse, vollick): Content is currently way too sensitive to mouse
    // moves for how noisy the controller is. It's almost impossible to click a
    // link without unintentionally starting a drag event. For this reason we
    // disable mouse moves, only delivering a down and up event.
    if (in_click_)
      return;
    SendGestureToContent(MakeMouseEvent(blink::WebInputEvent::kMouseMove,
                                        NowSeconds(), local_point_pixels,
                                        in_click_));
  } else {
    hover_target_->OnMove(target_point);
  }
}

void VrShellGl::SendButtonDown(UiElement* target,
                               const gfx::PointF& target_point,
                               const gfx::Point& local_point_pixels) {
  if (in_click_)
    return;
  if (!controller_->ButtonDownHappened(gvr::kControllerButtonClick))
    return;
  input_locked_element_ = target;
  in_click_ = true;
  if (!target)
    return;
  if (target->fill() == Fill::CONTENT) {
    SendGestureToContent(MakeMouseEvent(blink::WebInputEvent::kMouseDown,
                                        NowSeconds(), local_point_pixels,
                                        in_click_));
  } else {
    target->OnButtonDown(target_point);
  }
}

bool VrShellGl::SendButtonUp(UiElement* target,
                             const gfx::PointF& target_point,
                             const gfx::Point& local_point_pixels) {
  if (!in_click_)
    return false;
  if (!controller_->ButtonUpHappened(gvr::kControllerButtonClick) &&
      controller_->ButtonState(gvr::kControllerButtonClick))
    return false;
  in_click_ = false;
  if (!input_locked_element_)
    return true;
  DCHECK(input_locked_element_ == target);
  input_locked_element_ = nullptr;
  if (target->fill() == Fill::CONTENT) {
    SendGestureToContent(MakeMouseEvent(blink::WebInputEvent::kMouseUp,
                                        NowSeconds(), local_point_pixels,
                                        in_click_));
  } else {
    target->OnButtonUp(target_point);
  }
  return true;
}

void VrShellGl::SendTap(UiElement* target,
                        const gfx::PointF& target_point,
                        const gfx::Point& local_point_pixels) {
  if (!target)
    return;
  if (!touch_pending_)
    return;
  touch_pending_ = false;
  if (target->fill() == Fill::CONTENT) {
    auto gesture = base::MakeUnique<blink::WebGestureEvent>(
        blink::WebInputEvent::kGestureTapDown,
        blink::WebInputEvent::kNoModifiers, NowSeconds());
    gesture->source_device = blink::kWebGestureDeviceTouchpad;
    gesture->x = local_point_pixels.x();
    gesture->y = local_point_pixels.y();
    SendGestureToContent(std::move(gesture));
  } else {
    target->OnButtonDown(target_point);
    target->OnButtonUp(target_point);
  }
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

void VrShellGl::GetVisualTargetElement(
    const gfx::Vector3dF& controller_direction,
    gfx::Vector3dF& eye_to_target,
    gfx::Point3F& target_point,
    UiElement** target_element,
    gfx::PointF& target_local_point) const {
  // If we place the reticle based on elements intersecting the controller beam,
  // we can end up with the reticle hiding behind elements, or jumping laterally
  // in the field of view. This is physically correct, but hard to use. For
  // usability, do the following instead:
  //
  // - Project the controller laser onto a distance-limiting sphere.
  // - Create a vector between the eyes and the outer surface point.
  // - If any UI elements intersect this vector, and is within the bounding
  //   sphere, choose the closest to the eyes, and place the reticle at the
  //   intersection point.

  // Compute the distance from the eyes to the distance limiting sphere. Note
  // that the sphere is centered at the controller, rather than the eye, for
  // simplicity.
  float distance = scene_->GetBackgroundDistance();
  target_point = GetRayPoint(pointer_start_, controller_direction, distance);
  eye_to_target = target_point - kOrigin;
  eye_to_target.GetNormalized(&eye_to_target);

  // Determine which UI element (if any) intersects the line between the eyes
  // and the controller target position.
  float closest_element_distance = (target_point - kOrigin).Length();

  for (auto& element : scene_->GetUiElements()) {
    if (!element->IsHitTestable())
      continue;
    gfx::PointF local_point;
    gfx::Point3F plane_intersection_point;
    float distance_to_plane;
    if (!GetTargetLocalPoint(eye_to_target, *element.get(),
                             closest_element_distance, local_point,
                             plane_intersection_point, distance_to_plane))
      continue;
    if (!element->HitTest(local_point))
      continue;

    closest_element_distance = distance_to_plane;
    target_point = plane_intersection_point;
    *target_element = element.get();
    target_local_point = local_point;
  }
}

bool VrShellGl::GetTargetLocalPoint(const gfx::Vector3dF& eye_to_target,
                                    const UiElement& element,
                                    float max_distance_to_plane,
                                    gfx::PointF& target_local_point,
                                    gfx::Point3F& target_point,
                                    float& distance_to_plane) const {
  if (!element.GetRayDistance(kOrigin, eye_to_target, &distance_to_plane))
    return false;

  if (distance_to_plane < 0 || distance_to_plane >= max_distance_to_plane)
    return false;

  target_point = GetRayPoint(kOrigin, eye_to_target, distance_to_plane);
  gfx::PointF unit_xy_point = element.GetUnitRectangleCoordinates(target_point);

  target_local_point.set_x(0.5f + unit_xy_point.x());
  target_local_point.set_y(0.5f - unit_xy_point.y());
  return true;
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
    UiInterface::Direction direction = UiInterface::NONE;
    gfx::Vector3dF a = controller_start_direction_;
    gfx::Vector3dF b = controller_direction;
    a.set_y(0);
    b.set_y(0);
    if (a.LengthSquared() * b.LengthSquared() > 0.0) {
      float gesture_xz_angle =
          acos(gfx::DotProduct(a, b) / a.Length() / b.Length());
      if (fabs(gesture_xz_angle) > kMinAppButtonGestureAngleRad) {
        direction =
            gesture_xz_angle < 0 ? UiInterface::LEFT : UiInterface::RIGHT;
        browser_->AppButtonGesturePerformed(direction);
      }
    }
    if (direction == UiInterface::NONE)
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
    if (render_size_primary_ != webvr_surface_size_) {
      if (!webvr_surface_size_.width()) {
        // Don't try to resize to 0x0 pixels, drop frames until we get a
        // valid size.
        return;
      }

      render_size_primary_ = webvr_surface_size_;
      DVLOG(1) << __FUNCTION__ << ": resize GVR to "
               << render_size_primary_.width() << "x"
               << render_size_primary_.height();
      swap_chain_->ResizeBuffer(
          kFramePrimaryBuffer,
          {render_size_primary_.width(), render_size_primary_.height()});
    }
  } else {
    if (render_size_primary_ != render_size_vrshell_) {
      render_size_primary_ = render_size_vrshell_;
      swap_chain_->ResizeBuffer(
          kFramePrimaryBuffer,
          {render_size_primary_.width(), render_size_primary_.height()});
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

  gfx::Transform head_pose;

  // When using async reprojection, we need to know which pose was
  // used in the WebVR app for drawing this frame and supply it when
  // submitting. Technically we don't need a pose if not reprojecting,
  // but keeping it uninitialized seems likely to cause problems down
  // the road. Copying it is cheaper than fetching a new one.
  if (ShouldDrawWebVr()) {
    static_assert(!((kPoseRingBufferSize - 1) & kPoseRingBufferSize),
                  "kPoseRingBufferSize must be a power of 2");
    head_pose = webvr_head_pose_[frame_index % kPoseRingBufferSize];
  } else {
    vr::Mat4f from_gvr;
    device::GvrDelegate::GetGvrPoseWithNeckModel(gvr_api_.get(), &from_gvr);
    head_pose = vr::ToTransform(from_gvr);
  }

  // Update the render position of all UI elements (including desktop).
  scene_->OnBeginFrame(current_time);

  {
    // TODO(crbug.com/704690): Acquire controller state in a way that's timely
    // for both the gamepad API and UI input handling.
    TRACE_EVENT0("gpu", "VrShellGl::UpdateController");
    gfx::Vector3dF head_direction = GetForwardVector(head_pose);
    UpdateController(head_direction);
    HandleControllerInput(head_direction);
  }

  DrawWorldElements(head_pose);
  DrawOverlayElements(head_pose);

  frame.Unbind();

  // Draw head-locked elements to a separate, non-reprojected buffer.
  if (scene_->HasVisibleHeadLockedElements()) {
    frame.BindBuffer(kFrameHeadlockedBuffer);
    DrawHeadLockedElements();
    frame.Unbind();
  }

  if (ShouldDrawWebVr() && surfaceless_rendering_) {
    // Continue with submit once a GL fence signals that current drawing
    // operations have completed.
    std::unique_ptr<gl::GLFence> fence = base::MakeUnique<gl::GLFenceEGL>();
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&VrShellGl::DrawFrameSubmitWhenReady,
                   weak_ptr_factory_.GetWeakPtr(), frame_index, frame.release(),
                   head_pose, base::Passed(&fence)),
        kWebVRFenceCheckInterval);
  } else {
    // Continue with submit immediately.
    DrawFrameSubmitWhenReady(frame_index, frame.release(), head_pose, nullptr);
  }
}

void VrShellGl::DrawFrameSubmitWhenReady(int16_t frame_index,
                                         gvr_frame* frame_ptr,
                                         const gfx::Transform& head_pose,
                                         std::unique_ptr<gl::GLFence> fence) {
  if (fence && !fence->HasCompleted()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&VrShellGl::DrawFrameSubmitWhenReady,
                   weak_ptr_factory_.GetWeakPtr(), frame_index, frame_ptr,
                   head_pose, base::Passed(&fence)),
        kWebVRFenceCheckInterval);
    return;
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

void VrShellGl::DrawWorldElements(const gfx::Transform& head_pose) {
  TRACE_EVENT0("gpu", "VrShellGl::DrawWorldElements");

  if (ShouldDrawWebVr()) {
    // WebVR is incompatible with 3D world compositing since the
    // depth buffer was already populated with unknown scaling - the
    // WebVR app has full control over zNear/zFar. Just leave the
    // existing content in place in the primary buffer without
    // clearing. Currently, there aren't any world elements in WebVR
    // mode, this will need further testing if those get added
    // later.
  } else {
    // Non-WebVR mode, enable depth testing and clear the primary buffers.
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    const SkColor backgroundColor = scene_->GetWorldBackgroundColor();
    glClearColor(SkColorGetR(backgroundColor) / 255.0,
                 SkColorGetG(backgroundColor) / 255.0,
                 SkColorGetB(backgroundColor) / 255.0,
                 SkColorGetA(backgroundColor) / 255.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  std::vector<const UiElement*> elements = scene_->GetWorldElements();
  const bool draw_reticle = !(scene_->is_exiting() || ShouldDrawWebVr());
  DrawUiView(head_pose, elements, render_size_primary_,
             kViewportListPrimaryOffset, draw_reticle);
}

void VrShellGl::DrawOverlayElements(const gfx::Transform& head_pose) {
  std::vector<const UiElement*> elements = scene_->GetOverlayElements();
  if (elements.empty())
    return;

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  const bool draw_reticle = false;
  DrawUiView(head_pose, elements, render_size_primary_,
             kViewportListPrimaryOffset, draw_reticle);
}

void VrShellGl::DrawHeadLockedElements() {
  TRACE_EVENT0("gpu", "VrShellGl::DrawHeadLockedElements");
  std::vector<const UiElement*> elements = scene_->GetHeadLockedElements();

  // Add head-locked viewports. The list gets reset to just
  // the recommended viewports (for the primary buffer) each frame.
  buffer_viewport_list_->SetBufferViewport(
      kViewportListHeadlockedOffset + GVR_LEFT_EYE, *headlocked_left_viewport_);
  buffer_viewport_list_->SetBufferViewport(
      kViewportListHeadlockedOffset + GVR_RIGHT_EYE,
      *headlocked_right_viewport_);

  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gfx::Transform identity_matrix;
  DrawUiView(identity_matrix, elements, render_size_headlocked_,
             kViewportListHeadlockedOffset, false);
}

void VrShellGl::DrawUiView(const gfx::Transform& head_pose,
                           const std::vector<const UiElement*>& elements,
                           const gfx::Size& render_size,
                           int viewport_offset,
                           bool draw_reticle) {
  TRACE_EVENT0("gpu", "VrShellGl::DrawUiView");

  auto sorted_elements = GetElementsInDrawOrder(head_pose, elements);

  for (auto eye : {GVR_LEFT_EYE, GVR_RIGHT_EYE}) {
    buffer_viewport_list_->GetBufferViewport(eye + viewport_offset,
                                             buffer_viewport_.get());

    gfx::Transform eye_matrix;
    GvrMatToTransform(gvr_api_->GetEyeFromHeadMatrix(eye), &eye_matrix);
    gfx::Transform eye_view_matrix = eye_matrix * head_pose;

    const gfx::RectF& rect = GfxRectFromUV(buffer_viewport_->GetSourceUv());
    const gfx::Rect& pixel_rect = CalculatePixelSpaceRect(render_size, rect);
    glViewport(pixel_rect.x(), pixel_rect.y(), pixel_rect.width(),
               pixel_rect.height());

    gfx::Transform perspective_matrix = PerspectiveMatrixFromView(
        buffer_viewport_->GetSourceFov(), kZNear, kZFar);
    gfx::Transform view_proj_matrix = perspective_matrix * eye_view_matrix;

    DrawElements(view_proj_matrix, sorted_elements, draw_reticle);
    if (draw_reticle) {
      DrawController(view_proj_matrix);
      DrawLaser(view_proj_matrix);
    }
  }
}

void VrShellGl::DrawElements(const gfx::Transform& view_proj_matrix,
                             const std::vector<const UiElement*>& elements,
                             bool draw_reticle) {
  if (elements.empty())
    return;
  int initial_draw_phase = elements.front()->draw_phase();
  bool drawn_reticle = false;
  for (const auto* element : elements) {
    // If we have no element to draw the reticle on, draw it after the
    // background (the initial draw phase).
    if (!reticle_render_target_ && draw_reticle && !drawn_reticle &&
        element->draw_phase() > initial_draw_phase) {
      DrawReticle(view_proj_matrix);
      drawn_reticle = true;
    }

    DrawElement(view_proj_matrix, *element);

    if (draw_reticle && (reticle_render_target_ == element)) {
      DrawReticle(view_proj_matrix);
    }
  }
  vr_shell_renderer_->Flush();
}

void VrShellGl::DrawElement(const gfx::Transform& view_proj_matrix,
                            const UiElement& element) {
  gfx::Transform transform = view_proj_matrix * element.TransformMatrix();

  switch (element.fill()) {
    case Fill::OPAQUE_GRADIENT: {
      vr_shell_renderer_->GetGradientQuadRenderer()->Draw(
          transform, element.edge_color(), element.center_color(),
          element.computed_opacity());
      break;
    }
    case Fill::GRID_GRADIENT: {
      vr_shell_renderer_->GetGradientGridRenderer()->Draw(
          transform, element.edge_color(), element.center_color(),
          element.grid_color(), element.gridline_count(),
          element.computed_opacity());
      break;
    }
    case Fill::CONTENT: {
      gfx::RectF copy_rect(0, 0, 1, 1);
      vr_shell_renderer_->GetExternalTexturedQuadRenderer()->Draw(
          content_texture_id_, transform, copy_rect,
          element.computed_opacity());
      break;
    }
    case Fill::SELF: {
      element.Render(vr_shell_renderer_.get(), transform);
      break;
    }
    default:
      break;
  }
}

std::vector<const UiElement*> VrShellGl::GetElementsInDrawOrder(
    const gfx::Transform& view_matrix,
    const std::vector<const UiElement*>& elements) {
  std::vector<const UiElement*> sorted_elements = elements;

  // Sort elements primarily based on their draw phase (lower draw phase first)
  // and secondarily based on their z-axis distance (more distant first).
  // TODO(mthiesse, crbug.com/721356): This will not work well for elements not
  // directly in front of the user, but works well enough for our initial
  // release, and provides a consistent ordering that we can easily design
  // around.
  std::sort(sorted_elements.begin(), sorted_elements.end(),
            [](const UiElement* first, const UiElement* second) {
              if (first->draw_phase() != second->draw_phase()) {
                return first->draw_phase() < second->draw_phase();
              } else {
                return first->TransformMatrix().matrix().get(2, 3) <
                       second->TransformMatrix().matrix().get(2, 3);
              }
            });

  return sorted_elements;
}

void VrShellGl::DrawReticle(const gfx::Transform& render_matrix) {
  // Scale the reticle to have a fixed FOV size at any distance.
  const float eye_to_target =
      std::sqrt(target_point_.SquaredDistanceTo(kOrigin));

  gfx::Transform mat;
  mat.Scale3d(kReticleWidth * eye_to_target, kReticleHeight * eye_to_target, 1);

  gfx::Quaternion rotation;
  if (reticle_render_target_ != nullptr) {
    // Make the reticle planar to the element it's hitting.
    rotation = GetRotationFromZAxis(reticle_render_target_->GetNormal());
  } else {
    // Rotate the reticle to directly face the eyes.
    rotation = GetRotationFromZAxis(target_point_ - kOrigin);
  }
  gfx::Transform rotation_mat(rotation);
  mat = rotation_mat * mat;

  gfx::Point3F target_point = ScalePoint(target_point_, kReticleOffset);
  // Place the pointer slightly in front of the plane intersection point.
  mat.matrix().postTranslate(target_point.x(), target_point.y(),
                             target_point.z());

  gfx::Transform transform = render_matrix * mat;
  vr_shell_renderer_->GetReticleRenderer()->Draw(transform);
}

void VrShellGl::DrawLaser(const gfx::Transform& render_matrix) {
  gfx::Point3F target_point = ScalePoint(target_point_, kReticleOffset);
  // Find the length of the beam (from hand to target).
  const float laser_length =
      std::sqrt(pointer_start_.SquaredDistanceTo(target_point));

  // Build a beam, originating from the origin.
  gfx::Transform mat;

  // Move the beam half its height so that its end sits on the origin.
  mat.matrix().postTranslate(0.0f, 0.5f, 0.0f);
  mat.matrix().postScale(kLaserWidth, laser_length, 1);

  // Tip back 90 degrees to flat, pointing at the scene.
  const gfx::Quaternion quat(gfx::Vector3dF(1.0f, 0.0f, 0.0f), -M_PI / 2);
  gfx::Transform rotation_mat(quat);
  mat = rotation_mat * mat;

  const gfx::Vector3dF beam_direction = target_point_ - pointer_start_;

  gfx::Transform beam_direction_mat(GetRotationFromZAxis(beam_direction));

  float opacity = controller_->GetOpacity();
  // Render multiple faces to make the laser appear cylindrical.
  const int faces = 4;
  gfx::Transform face_transform;
  gfx::Transform transform;
  for (int i = 0; i < faces; i++) {
    // Rotate around Z.
    const float angle = M_PI * 2 * i / faces;
    const gfx::Quaternion rot({0.0f, 0.0f, 1.0f}, angle);
    face_transform = beam_direction_mat * gfx::Transform(rot) * mat;

    // Move the beam origin to the hand.
    face_transform.matrix().postTranslate(
        pointer_start_.x(), pointer_start_.y(), pointer_start_.z());
    transform = render_matrix * face_transform;
    vr_shell_renderer_->GetLaserRenderer()->Draw(opacity, transform);
  }
}

void VrShellGl::DrawController(const gfx::Transform& view_proj_matrix) {
  if (!vr_shell_renderer_->GetControllerRenderer()->IsSetUp())
    return;
  auto state = controller_->GetModelState();
  auto opacity = controller_->GetOpacity();
  gfx::Transform controller_transform;
  controller_->GetTransform(&controller_transform);
  gfx::Transform transform = view_proj_matrix * controller_transform;
  vr_shell_renderer_->GetControllerRenderer()->Draw(state, opacity, transform);
}

bool VrShellGl::ShouldDrawWebVr() {
  return web_vr_mode_ && scene_->GetWebVrRenderingEnabled();
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
    submit_client_.reset();
  }
}

void VrShellGl::UpdateWebVRTextureBounds(int16_t frame_index,
                                         const gfx::RectF& left_bounds,
                                         const gfx::RectF& right_bounds,
                                         const gfx::Size& source_size) {
  if (frame_index < 0) {
    webvr_left_viewport_->SetSourceUv(UVFromGfxRect(left_bounds));
    webvr_right_viewport_->SetSourceUv(UVFromGfxRect(right_bounds));
    CreateOrResizeWebVRSurface(source_size);
  } else {
    pending_bounds_.emplace(
        frame_index, WebVrBounds(left_bounds, right_bounds, source_size));
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

void VrShellGl::SetControllerModel(std::unique_ptr<VrControllerModel> model) {
  vr_shell_renderer_->GetControllerRenderer()->SetUp(std::move(model));
}

void VrShellGl::OnVSync() {
  while (premature_received_frames_ > 0) {
    TRACE_EVENT0("gpu", "VrShellGl::OnWebVRFrameAvailableRetry");
    --premature_received_frames_;
    OnWebVRFrameAvailable();
  }

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks target;

  // Don't send VSyncs until we have a timebase/interval.
  if (vsync_interval_.is_zero())
    return;
  target = now + vsync_interval_;
  int64_t intervals = (target - vsync_timebase_) / vsync_interval_;
  target = vsync_timebase_ + intervals * vsync_interval_;
  task_runner_->PostDelayedTask(FROM_HERE, vsync_task_.callback(),
                                target - now);

  base::TimeDelta time = intervals * vsync_interval_;
  if (!callback_.is_null()) {
    SendVSync(time, base::ResetAndReturn(&callback_));
  } else {
    pending_vsync_ = true;
    pending_time_ = time;
  }
  if (!ShouldDrawWebVr()) {
    DrawFrame(-1);
  }
}

void VrShellGl::OnRequest(device::mojom::VRVSyncProviderRequest request) {
  binding_.Close();
  binding_.Bind(std::move(request));
}

void VrShellGl::GetVSync(GetVSyncCallback callback) {
  // In surfaceless (reprojecting) rendering, stay locked
  // to vsync intervals. Otherwise, for legacy Cardboard mode,
  // run requested animation frames now if it missed a vsync.
  if (surfaceless_rendering_ || !pending_vsync_) {
    if (!callback_.is_null()) {
      mojo::ReportBadMessage(
          "Requested VSync before waiting for response to previous request.");
      binding_.Close();
      return;
    }
    callback_ = std::move(callback);
    return;
  }
  pending_vsync_ = false;
  SendVSync(pending_time_, std::move(callback));
}

void VrShellGl::UpdateVSyncInterval(int64_t timebase_nanos,
                                    double interval_seconds) {
  vsync_timebase_ = base::TimeTicks();
  vsync_timebase_ += base::TimeDelta::FromMicroseconds(timebase_nanos / 1000);
  vsync_interval_ = base::TimeDelta::FromSecondsD(interval_seconds);
  vsync_task_.Reset(base::Bind(&VrShellGl::OnVSync, base::Unretained(this)));
  OnVSync();
}

void VrShellGl::ForceExitVr() {
  browser_->ForceExitVr();
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
  vr::Mat4f from_gvr;
  device::mojom::VRPosePtr pose =
      device::GvrDelegate::GetVRPosePtrWithNeckModel(gvr_api_.get(), &from_gvr,
                                                     prediction_nanos);
  head_mat = vr::ToTransform(from_gvr);

  webvr_head_pose_[frame_index % kPoseRingBufferSize] = head_mat;
  webvr_time_pose_[frame_index % kPoseRingBufferSize] = base::TimeTicks::Now();

  std::move(callback).Run(std::move(pose), time, frame_index,
                          device::mojom::VRVSyncProvider::Status::SUCCESS);
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

}  // namespace vr_shell
