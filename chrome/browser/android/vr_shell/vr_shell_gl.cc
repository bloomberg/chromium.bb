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
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/vr_shell/fps_meter.h"
#include "chrome/browser/android/vr_shell/mailbox_to_surface_bridge.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"
#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"
#include "chrome/browser/android/vr_shell/ui_scene_manager.h"
#include "chrome/browser/android/vr_shell/vr_controller.h"
#include "chrome/browser/android/vr_shell/vr_gl_thread.h"
#include "chrome/browser/android/vr_shell/vr_gl_util.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/android/vr_shell/vr_shell_renderer.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_gamepad_data_provider.h"
#include "device/vr/vr_math.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace vr_shell {

namespace {
static constexpr float kZNear = 0.1f;
static constexpr float kZFar = 1000.0f;

static constexpr float kReticleWidth = 0.025f;
static constexpr float kReticleHeight = 0.025f;

static constexpr float kLaserWidth = 0.01f;

static constexpr gfx::Point3F kOrigin = {0.0f, 0.0f, 0.0f};

// Fraction of the distance to the object the cursor is drawn at to avoid
// rounding errors drawing the cursor behind the object.
static constexpr float kReticleOffset = 0.99f;

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

// Criteria for considering holding the app button in combination with
// controller movement as a gesture.
static constexpr float kMinAppButtonGestureAngleRad = 0.25;

// Generate a quaternion representing the rotation from the negative Z axis
// (0, 0, -1) to a specified vector. This is an optimized version of a more
// general vector-to-vector calculation.
vr::Quatf GetRotationFromZAxis(gfx::Vector3dF vec) {
  vr::NormalizeVector(&vec);
  vr::Quatf quat;
  quat.qw = 1.0f - vec.z();
  if (quat.qw < 1e-6f) {
    // Degenerate case: vectors are exactly opposite. Replace by an
    // arbitrary 180 degree rotation to avoid invalid normalization.
    quat.qx = 1.0f;
    quat.qy = 0.0f;
    quat.qz = 0.0f;
    quat.qw = 0.0f;
  } else {
    quat.qx = vec.y();
    quat.qy = -vec.x();
    quat.qz = 0.0f;
    vr::NormalizeQuat(&quat);
  }
  return quat;
}

gvr::Mat4f PerspectiveMatrixFromView(const gvr::Rectf& fov,
                                     float z_near,
                                     float z_far) {
  gvr::Mat4f result;
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

  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.m[i][j] = 0.0f;
    }
  }
  result.m[0][0] = X;
  result.m[0][2] = A;
  result.m[1][1] = Y;
  result.m[1][2] = B;
  result.m[2][2] = C;
  result.m[2][3] = D;
  result.m[3][2] = -1;

  return result;
}

std::unique_ptr<blink::WebMouseEvent> MakeMouseEvent(WebInputEvent::Type type,
                                                     double timestamp,
                                                     float x,
                                                     float y) {
  std::unique_ptr<blink::WebMouseEvent> mouse_event(new blink::WebMouseEvent(
      type, blink::WebInputEvent::kNoModifiers, timestamp));
  mouse_event->pointer_type = blink::WebPointerProperties::PointerType::kMouse;
  mouse_event->SetPositionInWidget(x, y);
  mouse_event->click_count = 1;

  return mouse_event;
}

enum class ViewerType {
  UNKNOWN_TYPE = 0,
  CARDBOARD = 1,
  DAYDREAM = 2,
  VIEWER_TYPE_MAX,
};

void MatfToGvrMat(const vr::Mat4f& in, gvr::Mat4f* out) {
  // If our std::array implementation doesn't have any non-data members, we can
  // just cast the gvr matrix to an std::array.
  static_assert(sizeof(in) == sizeof(*out),
                "Cannot reinterpret gvr::Mat4f as vr::Matf");
  *out = *reinterpret_cast<gvr::Mat4f*>(const_cast<vr::Mat4f*>(&in));
}

void GvrMatToMatf(const gvr::Mat4f& in, vr::Mat4f* out) {
  // If our std::array implementation doesn't have any non-data members, we can
  // just cast the gvr matrix to an std::array.
  static_assert(sizeof(in) == sizeof(*out),
                "Cannot reinterpret gvr::Mat4f as vr::Matf");
  *out = *reinterpret_cast<vr::Mat4f*>(const_cast<gvr::Mat4f*>(&in));
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

VrShellGl::VrShellGl(VrBrowserInterface* browser,
                     gvr_context* gvr_api,
                     bool initially_web_vr,
                     bool reprojected_rendering,
                     UiScene* scene)
    : web_vr_mode_(initially_web_vr),
      surfaceless_rendering_(reprojected_rendering),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this),
      browser_(browser),
      scene_(scene),
#if DCHECK_IS_ON()
      fps_meter_(new FPSMeter()),
#endif
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

  TRACE_EVENT0("gpu", "VrShellGl::glFinish");
  // This is a load-bearing glFinish, please don't remove it without
  // before/after timing comparisons. Goal is to clear the GPU queue
  // of the native GL context to avoid stalls later in GVR frame
  // acquire/submit.
  glFinish();
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

  // It is legal for the WebVR client to submit a new frame now, since
  // we've consumed the image. TODO(klausw): would timing be better if
  // we move the "rendered" notification after draw, or suppress
  // the next vsync until that's done?

  if (submit_client_) {
    submit_client_->OnSubmitFrameRendered();
  }

  DrawFrame(frame_index);
}

void VrShellGl::GvrInit(gvr_context* gvr_api) {
  gvr_api_ = gvr::GvrApi::WrapNonOwned(gvr_api);
  controller_.reset(new VrController(gvr_api));

  ViewerType viewerType;
  switch (gvr_api_->GetViewerType()) {
    case gvr::ViewerType::GVR_VIEWER_TYPE_DAYDREAM:
      viewerType = ViewerType::DAYDREAM;
      break;
    case gvr::ViewerType::GVR_VIEWER_TYPE_CARDBOARD:
      viewerType = ViewerType::CARDBOARD;
      break;
    default:
      NOTREACHED();
      viewerType = ViewerType::UNKNOWN_TYPE;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("VRViewerType", static_cast<int>(viewerType),
                            static_cast<int>(ViewerType::VIEWER_TYPE_MAX));
}

void VrShellGl::InitializeRenderer() {
  gvr_api_->InitializeGl();
  vr::Mat4f head_pose;
  device::GvrDelegate::GetGvrPoseWithNeckModel(gvr_api_.get(), &head_pose);
  webvr_head_pose_.assign(kPoseRingBufferSize, head_pose);

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

  swap_chain_.reset(new gvr::SwapChain(gvr_api_->CreateSwapChain(specs)));

  vr_shell_renderer_.reset(new VrShellRenderer());

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

  browser_->UpdateGamepadData(controller_->GetGamepadData());
}

void VrShellGl::HandleControllerInput(const gfx::Vector3dF& head_direction) {
  if (ShouldDrawWebVr()) {
    // Process screen touch events for Cardboard button compatibility.
    // Also send tap events for controller "touchpad click" events.
    if (touch_pending_ ||
        controller_->ButtonUpHappened(
            gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK)) {
      touch_pending_ = false;
      std::unique_ptr<WebGestureEvent> gesture(new WebGestureEvent(
          WebInputEvent::kGestureTapDown, WebInputEvent::kNoModifiers,
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF()));
      gesture->source_device = blink::kWebGestureDeviceTouchpad;
      gesture->x = 0;
      gesture->y = 0;
      SendGestureToContent(std::move(gesture));
      DVLOG(1) << __FUNCTION__ << ": sent CLICK gesture";
    }
  }

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

  vr::Mat4f mat;
  QuatToMatrix(controller_quat_, &mat);
  gfx::Vector3dF controller_direction =
      vr::MatrixVectorMul(mat, ergo_neutral_pose);

  HandleControllerAppButtonActivity(controller_direction);

  if (ShouldDrawWebVr()) {
    return;
  }

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
  target_point_ =
      vr::GetRayPoint(pointer_start_, controller_direction, distance);
  gfx::Vector3dF eye_to_target = target_point_ - kOrigin;
  vr::NormalizeVector(&eye_to_target);

  // Determine which UI element (if any) intersects the line between the eyes
  // and the controller target position.
  float closest_element_distance = (target_point_ - kOrigin).Length();
  previous_target_element_ = target_element_;
  target_element_ = nullptr;
  float target_x;
  float target_y;

  for (auto& plane : scene_->GetUiElements()) {
    if (!plane->IsHitTestable())
      continue;

    float distance_to_plane;
    if (!plane->GetRayDistance(kOrigin, eye_to_target, &distance_to_plane))
      continue;

    if (distance_to_plane < 0 || distance_to_plane >= closest_element_distance)
      continue;

    gfx::Point3F plane_intersection_point =
        vr::GetRayPoint(kOrigin, eye_to_target, distance_to_plane);
    gfx::PointF unit_xy_point =
        plane->GetUnitRectangleCoordinates(plane_intersection_point);

    float x = 0.5f + unit_xy_point.x();
    float y = 0.5f - unit_xy_point.y();
    if (x < 0.0f || x >= 1.0f || y < 0.0f || y >= 1.0f)
      continue;

    closest_element_distance = distance_to_plane;
    target_point_ = plane_intersection_point;
    target_element_ = plane.get();
    target_x = x;
    target_y = y;
  }

  // Handle input targeting on the content quad, ignoring any other elements.
  // Content is treated specially to accomodate scrolling, flings, etc.
  InputTarget input_target = InputTarget::NONE;
  int pixel_x = 0;
  int pixel_y = 0;
  if (target_element_ != nullptr && target_element_->fill() == Fill::CONTENT) {
    input_target = InputTarget::CONTENT;
    gfx::RectF pixel_rect(0, 0, content_tex_css_width_,
                          content_tex_css_height_);
    pixel_x = pixel_rect.x() + pixel_rect.width() * target_x;
    pixel_y = pixel_rect.y() + pixel_rect.height() * target_y;
  }
  SendInputToContent(input_target, pixel_x, pixel_y);

  // Handle input targeting on all non-content elements.
  SendInputToUiElements(target_element_);
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
    float gesture_xz_angle;
    if (vr::XZAngle(controller_start_direction_, controller_direction,
                    &gesture_xz_angle)) {
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

void VrShellGl::SendInputToContent(InputTarget input_target,
                                   int pixel_x,
                                   int pixel_y) {
  std::vector<std::unique_ptr<WebGestureEvent>> gesture_list =
      controller_->DetectGestures();
  double timestamp = gesture_list.front()->TimeStampSeconds();

  if (touch_pending_) {
    touch_pending_ = false;
    std::unique_ptr<WebGestureEvent> event(
        new WebGestureEvent(WebInputEvent::kGestureTapDown,
                            WebInputEvent::kNoModifiers, timestamp));
    event->source_device = blink::kWebGestureDeviceTouchpad;
    event->x = pixel_x;
    event->y = pixel_y;
    gesture_list.push_back(std::move(event));
  }

  for (auto& gesture : gesture_list) {
    gesture->x = pixel_x;
    gesture->y = pixel_y;
    auto movableGesture = base::MakeUnique<WebGestureEvent>(*gesture);

    switch (gesture->GetType()) {
      // Once the user starts scrolling send all the scroll events to this
      // element until the scrolling stops.
      case WebInputEvent::kGestureScrollBegin:
        current_scroll_target_ = input_target;
        if (current_scroll_target_ != InputTarget::NONE) {
          SendGestureToContent(std::move(movableGesture));
        }
        break;
      case WebInputEvent::kGestureScrollEnd:
        if (current_scroll_target_ != InputTarget::NONE) {
          SendGestureToContent(std::move(movableGesture));
        }
        current_fling_target_ = current_scroll_target_;
        current_scroll_target_ = InputTarget::NONE;
        break;
      case WebInputEvent::kGestureScrollUpdate:
        if (current_scroll_target_ != InputTarget::NONE) {
          SendGestureToContent(std::move(movableGesture));
        }
        break;
      case WebInputEvent::kGestureFlingStart:
        if (current_fling_target_ != InputTarget::NONE) {
          SendGestureToContent(std::move(movableGesture));
        }
        current_fling_target_ = InputTarget::NONE;
        break;
      case WebInputEvent::kGestureFlingCancel:
        current_fling_target_ = InputTarget::NONE;
        if (input_target != InputTarget::NONE) {
          SendGestureToContent(std::move(movableGesture));
        }
        break;
      case WebInputEvent::kGestureTapDown:
        current_fling_target_ = InputTarget::NONE;
        if (input_target != InputTarget::NONE) {
          SendGestureToContent(std::move(movableGesture));
        }
        break;
      case WebInputEvent::kUndefined:
        break;
      default:
        NOTREACHED();
    }
  }

  // Hover support
  bool new_target = input_target != current_input_target_;
  if (new_target && current_input_target_ != InputTarget::NONE) {
    // Send a move event indicating that the pointer moved off of an element.
    SendGestureToContent(
        MakeMouseEvent(WebInputEvent::kMouseLeave, timestamp, 0, 0));
  }
  current_input_target_ = input_target;
  if (current_input_target_ != InputTarget::NONE) {
    WebInputEvent::Type type =
        new_target ? WebInputEvent::kMouseEnter : WebInputEvent::kMouseMove;
    SendGestureToContent(MakeMouseEvent(type, timestamp, pixel_x, pixel_y));
  }
}

void VrShellGl::SendInputToUiElements(UiElement* target_element) {
  if (target_element != previous_target_element_) {
    if (previous_target_element_ &&
        previous_target_element_->fill() != Fill::CONTENT) {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&UiElement::OnHoverLeave,
                                base::Unretained(previous_target_element_)));
    }
    if (target_element && target_element->fill() != Fill::CONTENT) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&UiElement::OnHoverEnter,
                                        base::Unretained(target_element)));
    }
    click_target_element_ = nullptr;
  }
  if (target_element && target_element->fill() != Fill::CONTENT) {
    if (controller_->ButtonDownHappened(
            gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK)) {
      task_runner_->PostTask(FROM_HERE,
                             base::Bind(&UiElement::OnButtonDown,
                                        base::Unretained(target_element)));
      click_target_element_ = target_element;
    }
    if (controller_->ButtonUpHappened(
            gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK)) {
      if (click_target_element_ == target_element) {
        task_runner_->PostTask(FROM_HERE,
                               base::Bind(&UiElement::OnButtonUp,
                                          base::Unretained(target_element)));
      }
      click_target_element_ = nullptr;
    }
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

  vr::Mat4f head_pose;

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
    device::GvrDelegate::GetGvrPoseWithNeckModel(gvr_api_.get(), &head_pose);
  }

  // Update the render position of all UI elements (including desktop).
  scene_->UpdateTransforms(current_time);

  {
    // TODO(crbug.com/704690): Acquire controller state in a way that's timely
    // for both the gamepad API and UI input handling.
    TRACE_EVENT0("gpu", "VrShellGl::UpdateController");
    auto head_direction = vr::GetForwardVector(head_pose);
    UpdateController(head_direction);
    HandleControllerInput(head_direction);
  }

  DrawWorldElements(head_pose);

  frame.Unbind();

  // Draw head-locked elements to a separate, non-reprojected buffer.
  if (scene_->HasVisibleHeadLockedElements()) {
    frame.BindBuffer(kFrameHeadlockedBuffer);
    DrawHeadLockedElements();
    frame.Unbind();
  }

  {
    TRACE_EVENT0("gpu", "VrShellGl::Submit");
    gvr::Mat4f mat;
    MatfToGvrMat(head_pose, &mat);
    frame.Submit(*buffer_viewport_list_, mat);
  }

  // No need to swap buffers for surfaceless rendering.
  if (!surfaceless_rendering_) {
    // TODO(mthiesse): Support asynchronous SwapBuffers.
    TRACE_EVENT0("gpu", "VrShellGl::SwapBuffers");
    surface_->SwapBuffers();
  }

#if DCHECK_IS_ON()
  // After saving the timestamp, fps will be available via GetFPS().
  // TODO(vollick): enable rendering of this framerate in a HUD.
  fps_meter_->AddFrame(current_time);
  DVLOG(1) << "fps: " << fps_meter_->GetFPS();
#endif
}

void VrShellGl::DrawWorldElements(const vr::Mat4f& head_pose) {
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

    const vr::Colorf& backgroundColor = scene_->GetBackgroundColor();
    glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b,
                 backgroundColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  std::vector<const UiElement*> elements = scene_->GetWorldElements();
  DrawUiView(head_pose, elements, render_size_primary_,
             kViewportListPrimaryOffset, !ShouldDrawWebVr());
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

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  vr::Mat4f identity_matrix;
  vr::SetIdentityM(&identity_matrix);
  DrawUiView(identity_matrix, elements, render_size_headlocked_,
             kViewportListHeadlockedOffset, false);
}

void VrShellGl::DrawUiView(const vr::Mat4f& head_pose,
                           const std::vector<const UiElement*>& elements,
                           const gfx::Size& render_size,
                           int viewport_offset,
                           bool draw_cursor) {
  TRACE_EVENT0("gpu", "VrShellGl::DrawUiView");

  auto elementsInDrawOrder = GetElementsInDrawOrder(head_pose, elements);

  for (auto eye : {GVR_LEFT_EYE, GVR_RIGHT_EYE}) {
    buffer_viewport_list_->GetBufferViewport(eye + viewport_offset,
                                             buffer_viewport_.get());

    vr::Mat4f eye_view_matrix;
    vr::Mat4f eye_matrix;
    GvrMatToMatf(gvr_api_->GetEyeFromHeadMatrix(eye), &eye_matrix);
    vr::MatrixMul(eye_matrix, head_pose, &eye_view_matrix);

    const gfx::RectF& rect = GfxRectFromUV(buffer_viewport_->GetSourceUv());
    const gfx::Rect& pixel_rect = CalculatePixelSpaceRect(render_size, rect);
    glViewport(pixel_rect.x(), pixel_rect.y(), pixel_rect.width(),
               pixel_rect.height());

    vr::Mat4f render_matrix;
    vr::Mat4f perspective_matrix;
    GvrMatToMatf(PerspectiveMatrixFromView(buffer_viewport_->GetSourceFov(),
                                           kZNear, kZFar),
                 &perspective_matrix);

    vr::MatrixMul(perspective_matrix, eye_view_matrix, &render_matrix);

    DrawElements(render_matrix, elementsInDrawOrder);
    if (draw_cursor) {
      DrawController(render_matrix);
      DrawCursor(render_matrix);
    }
  }
}

void VrShellGl::DrawElements(const vr::Mat4f& view_proj_matrix,
                             const std::vector<const UiElement*>& elements) {
  for (const auto* rect : elements) {
    vr::Mat4f transform;
    vr::MatrixMul(view_proj_matrix, rect->TransformMatrix(), &transform);

    switch (rect->fill()) {
      case Fill::OPAQUE_GRADIENT: {
        vr_shell_renderer_->GetGradientQuadRenderer()->Draw(
            transform, rect->edge_color(), rect->center_color(),
            rect->computed_opacity());
        break;
      }
      case Fill::GRID_GRADIENT: {
        vr_shell_renderer_->GetGradientGridRenderer()->Draw(
            transform, rect->edge_color(), rect->center_color(),
            rect->gridline_count(), rect->computed_opacity());
        break;
      }
      case Fill::CONTENT: {
        gfx::RectF copy_rect(0, 0, 1, 1);
        vr_shell_renderer_->GetExternalTexturedQuadRenderer()->Draw(
            content_texture_id_, transform, copy_rect,
            rect->computed_opacity());
        break;
      }
      case Fill::SELF: {
        rect->Render(vr_shell_renderer_.get(), transform);
        break;
      }
      default:
        break;
    }
  }
  vr_shell_renderer_->Flush();
}

std::vector<const UiElement*> VrShellGl::GetElementsInDrawOrder(
    const vr::Mat4f& view_matrix,
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
                return vr::GetTranslation(first->TransformMatrix()).z() <
                       vr::GetTranslation(second->TransformMatrix()).z();
              }
            });

  return sorted_elements;
}

void VrShellGl::DrawCursor(const vr::Mat4f& render_matrix) {
  vr::Mat4f mat;
  vr::SetIdentityM(&mat);

  // Draw the reticle.

  // Scale the pointer to have a fixed FOV size at any distance.
  const float eye_to_target =
      std::sqrt(target_point_.SquaredDistanceTo(kOrigin));
  vr::ScaleM(
      mat,
      {kReticleWidth * eye_to_target, kReticleHeight * eye_to_target, 1.0f},
      &mat);

  vr::Quatf rotation;
  if (target_element_ != nullptr) {
    // Make the reticle planar to the element it's hitting.
    rotation = GetRotationFromZAxis(target_element_->GetNormal());
  } else {
    // Rotate the cursor to directly face the eyes.
    rotation = GetRotationFromZAxis(target_point_ - kOrigin);
  }
  vr::Mat4f rotation_mat;
  vr::QuatToMatrix(rotation, &rotation_mat);
  vr::MatrixMul(rotation_mat, mat, &mat);

  gfx::Point3F target_point = ScalePoint(target_point_, kReticleOffset);
  // Place the pointer slightly in front of the plane intersection point.
  vr::TranslateM(mat, target_point - kOrigin, &mat);

  vr::Mat4f transform;
  vr::MatrixMul(render_matrix, mat, &transform);
  vr_shell_renderer_->GetReticleRenderer()->Draw(transform);

  // Draw the laser.

  // Find the length of the beam (from hand to target).
  const float laser_length =
      std::sqrt(pointer_start_.SquaredDistanceTo(target_point));

  // Build a beam, originating from the origin.
  vr::SetIdentityM(&mat);

  // Move the beam half its height so that its end sits on the origin.
  vr::TranslateM(mat, {0.0f, 0.5f, 0.0f}, &mat);
  vr::ScaleM(mat, {kLaserWidth, laser_length, 1}, &mat);

  // Tip back 90 degrees to flat, pointing at the scene.
  const vr::Quatf quat = vr::QuatFromAxisAngle({1.0f, 0.0f, 0.0f, -M_PI / 2});
  vr::QuatToMatrix(quat, &rotation_mat);
  vr::MatrixMul(rotation_mat, mat, &mat);

  const gfx::Vector3dF beam_direction = target_point_ - pointer_start_;

  vr::Mat4f beam_direction_mat;
  vr::QuatToMatrix(GetRotationFromZAxis(beam_direction), &beam_direction_mat);

  float opacity = controller_->GetOpacity();
  // Render multiple faces to make the laser appear cylindrical.
  const int faces = 4;
  for (int i = 0; i < faces; i++) {
    // Rotate around Z.
    const float angle = M_PI * 2 * i / faces;
    const vr::Quatf rot = vr::QuatFromAxisAngle({0.0f, 0.0f, 1.0f, angle});
    vr::Mat4f face_transform;
    vr::QuatToMatrix(rot, &face_transform);
    vr::MatrixMul(face_transform, mat, &face_transform);
    // Orient according to target direction.
    vr::MatrixMul(beam_direction_mat, face_transform, &face_transform);

    // Move the beam origin to the hand.
    vr::TranslateM(face_transform, pointer_start_ - kOrigin, &face_transform);

    vr::MatrixMul(render_matrix, face_transform, &transform);
    vr_shell_renderer_->GetLaserRenderer()->Draw(opacity, transform);
  }
}

void VrShellGl::DrawController(const vr::Mat4f& view_proj_matrix) {
  if (!vr_shell_renderer_->GetControllerRenderer()->IsSetUp())
    return;
  auto state = controller_->GetModelState();
  auto opacity = controller_->GetOpacity();
  vr::Mat4f controller_transform;
  controller_->GetTransform(&controller_transform);
  vr::Mat4f transform;
  vr::MatrixMul(view_proj_matrix, controller_transform, &transform);
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

void VrShellGl::GetVSync(const GetVSyncCallback& callback) {
  if (!pending_vsync_) {
    if (!callback_.is_null()) {
      mojo::ReportBadMessage(
          "Requested VSync before waiting for response to previous request.");
      binding_.Close();
      return;
    }
    callback_ = callback;
    return;
  }
  pending_vsync_ = false;
  SendVSync(pending_time_, callback);
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

void VrShellGl::SendVSync(base::TimeDelta time,
                          const GetVSyncCallback& callback) {
  uint8_t frame_index = frame_index_++;

  TRACE_EVENT1("input", "VrShellGl::SendVSync", "frame", frame_index);

  vr::Mat4f head_mat;
  device::mojom::VRPosePtr pose =
      device::GvrDelegate::GetVRPosePtrWithNeckModel(gvr_api_.get(), &head_mat);

  webvr_head_pose_[frame_index % kPoseRingBufferSize] = head_mat;

  callback.Run(std::move(pose), time, frame_index,
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
