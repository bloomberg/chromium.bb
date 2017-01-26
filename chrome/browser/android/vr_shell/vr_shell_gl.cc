// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell_gl.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/vr_shell/ui_elements.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"
#include "chrome/browser/android/vr_shell/vr_controller.h"
#include "chrome/browser/android/vr_shell/vr_gl_util.h"
#include "chrome/browser/android/vr_shell/vr_math.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/android/vr_shell/vr_shell_renderer.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace vr_shell {

namespace {
// Constant taken from treasure_hunt demo.
static constexpr long kPredictionTimeWithoutVsyncNanos = 50000000;

static constexpr float kZNear = 0.1f;
static constexpr float kZFar = 1000.0f;

// Screen angle in degrees. 0 = vertical, positive = top closer.
static constexpr float kDesktopScreenTiltDefault = 0;

static constexpr float kReticleWidth = 0.025f;
static constexpr float kReticleHeight = 0.025f;

static constexpr float kLaserWidth = 0.01f;

// Angle (radians) the beam down from the controller axis, for wrist comfort.
static constexpr float kErgoAngleOffset = 0.26f;

static constexpr gvr::Vec3f kOrigin = {0.0f, 0.0f, 0.0f};

// In lieu of an elbow model, we assume a position for the user's hand.
// TODO(mthiesse): Handedness options.
static constexpr gvr::Vec3f kHandPosition = {0.2f, -0.5f, -0.2f};

// If there is no content quad, and the reticle isn't hitting another element,
// draw the reticle at this distance.
static constexpr float kDefaultReticleDistance = 2.0f;

// Fraction of the distance to the object the cursor is drawn at to avoid
// rounding errors drawing the cursor behind the object.
static constexpr float kReticleOffset = 0.99f;

// Limit the rendering distance of the reticle to the distance to a corner of
// the content quad, times this value. This lets the rendering distance
// adjust according to content quad placement.
static constexpr float kReticleDistanceMultiplier = 1.5f;

// GVR buffer indices for use with viewport->SetSourceBufferIndex
// or frame.BindBuffer. We use one for world content (with reprojection)
// including main VrShell and WebVR content plus world-space UI.
// The headlocked buffer is for UI that should not use reprojection.
static constexpr int kFramePrimaryBuffer = 0;
static constexpr int kFrameHeadlockedBuffer = 1;

// Pixel dimensions and field of view for the head-locked content. This
// is currently sized to fit the WebVR "insecure transport" warnings,
// adjust it as needed if there is additional content.
static constexpr gvr::Sizei kHeadlockedBufferDimensions = {1024, 1024};
static constexpr gvr::Rectf kHeadlockedBufferFov = {20.f, 20.f, 20.f, 20.f};

// The GVR viewport list has two entries (left eye and right eye) for each
// GVR buffer.
static constexpr int kViewportListPrimaryOffset = 0;
static constexpr int kViewportListHeadlockedOffset = 2;

// Magic numbers used to mark valid pose index values encoded in frame
// data. Must match the magic numbers used in blink's VRDisplay.cpp.
static constexpr std::array<uint8_t, 2> kWebVrPosePixelMagicNumbers{{42, 142}};

float Distance(const gvr::Vec3f& vec1, const gvr::Vec3f& vec2) {
  float xdiff = (vec1.x - vec2.x);
  float ydiff = (vec1.y - vec2.y);
  float zdiff = (vec1.z - vec2.z);
  float scale = xdiff * xdiff + ydiff * ydiff + zdiff * zdiff;
  return std::sqrt(scale);
}

// Generate a quaternion representing the rotation from the negative Z axis
// (0, 0, -1) to a specified vector. This is an optimized version of a more
// general vector-to-vector calculation.
gvr::Quatf GetRotationFromZAxis(gvr::Vec3f vec) {
  vr_shell::NormalizeVector(vec);
  gvr::Quatf quat;
  quat.qw = 1.0f - vec.z;
  if (quat.qw < 1e-6f) {
    // Degenerate case: vectors are exactly opposite. Replace by an
    // arbitrary 180 degree rotation to avoid invalid normalization.
    quat.qx = 1.0f;
    quat.qy = 0.0f;
    quat.qz = 0.0f;
    quat.qw = 0.0f;
  } else {
    quat.qx = vec.y;
    quat.qy = -vec.x;
    quat.qz = 0.0f;
    vr_shell::NormalizeQuat(quat);
  }
  return quat;
}

std::unique_ptr<blink::WebMouseEvent> MakeMouseEvent(WebInputEvent::Type type,
                                                     double timestamp,
                                                     float x,
                                                     float y) {
  std::unique_ptr<blink::WebMouseEvent> mouse_event(new blink::WebMouseEvent(
      type, blink::WebInputEvent::NoModifiers, timestamp));
  mouse_event->pointerType = blink::WebPointerProperties::PointerType::Mouse;
  mouse_event->x = x;
  mouse_event->y = y;
  mouse_event->windowX = x;
  mouse_event->windowY = y;
  mouse_event->clickCount = 1;

  return mouse_event;
}

enum class ViewerType {
  UNKNOWN_TYPE = 0,
  CARDBOARD = 1,
  DAYDREAM = 2,
  VIEWER_TYPE_MAX,
};

int GetPixelEncodedPoseIndexByte() {
  TRACE_EVENT0("gpu", "VrShellGl::GetPixelEncodedPoseIndex");
  // Read the pose index encoded in a bottom left pixel as color values.
  // See also third_party/WebKit/Source/modules/vr/VRDisplay.cpp which
  // encodes the pose index, and device/vr/android/gvr/gvr_device.cc
  // which tracks poses. Returns the low byte (0..255) if valid, or -1
  // if not valid due to bad magic number.
  uint8_t pixels[4];
  // Assume we're reading from the framebuffer we just wrote to.
  // That's true currently, we may need to use glReadBuffer(GL_BACK)
  // or equivalent if the rendering setup changes in the future.
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  // Check for the magic number written by VRDevice.cpp on submit.
  // This helps avoid glitches from garbage data in the render
  // buffer that can appear during initialization or resizing. These
  // often appear as flashes of all-black or all-white pixels.
  if (pixels[1] == kWebVrPosePixelMagicNumbers[0] &&
      pixels[2] == kWebVrPosePixelMagicNumbers[1]) {
    // Pose is good.
    return pixels[0];
  }
  VLOG(1) << "WebVR: reject decoded pose index " << (int)pixels[0] <<
      ", bad magic number " << (int)pixels[1] << ", " << (int)pixels[2];
  return -1;
}

int64_t TimeInMicroseconds() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

void WaitForSwapAck(const base::Closure& callback, gfx::SwapResult result) {
  callback.Run();
}

}  // namespace

VrShellGl::VrShellGl(
    const base::WeakPtr<VrShell>& weak_vr_shell,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gvr_context* gvr_api,
    bool initially_web_vr,
    bool reprojected_rendering)
      : web_vr_mode_(initially_web_vr),
        surfaceless_rendering_(reprojected_rendering),
        task_runner_(base::ThreadTaskRunnerHandle::Get()),
        weak_vr_shell_(weak_vr_shell),
        main_thread_task_runner_(std::move(main_thread_task_runner)),
        weak_ptr_factory_(this) {
  GvrInit(gvr_api);
}

VrShellGl::~VrShellGl() {
  draw_task_.Cancel();
}

void VrShellGl::Initialize() {
  gvr::Mat4f identity;
  SetIdentityM(identity);
  webvr_head_pose_.resize(kPoseRingBufferSize, identity);
  webvr_head_pose_valid_.resize(kPoseRingBufferSize, false);

  scene_.reset(new UiScene);

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

  // TODO(mthiesse): We don't appear to have a VSync provider ever here. This is
  // sort of okay, because the GVR swap chain will block if we render too fast,
  // but we should address this properly.
  if (surface_->GetVSyncProvider()) {
    surface_->GetVSyncProvider()->GetVSyncParameters(base::Bind(
        &VrShellGl::UpdateVSyncParameters, weak_ptr_factory_.GetWeakPtr()));
  } else {
    LOG(ERROR) << "No VSync Provider";
  }

  unsigned int textures[2];
  glGenTextures(2, textures);
  ui_texture_id_ = textures[0];
  content_texture_id_ = textures[1];
  ui_surface_texture_ = gl::SurfaceTexture::Create(ui_texture_id_);
  content_surface_texture_ = gl::SurfaceTexture::Create(content_texture_id_);
  ui_surface_.reset(new gl::ScopedJavaSurface(ui_surface_texture_.get()));
  content_surface_.reset(new gl::ScopedJavaSurface(
      content_surface_texture_.get()));
  ui_surface_texture_->SetFrameAvailableCallback(base::Bind(
        &VrShellGl::OnUIFrameAvailable, weak_ptr_factory_.GetWeakPtr()));
  content_surface_texture_->SetFrameAvailableCallback(base::Bind(
        &VrShellGl::OnContentFrameAvailable, weak_ptr_factory_.GetWeakPtr()));

  content_surface_texture_->SetDefaultBufferSize(
      content_tex_physical_size_.width, content_tex_physical_size_.height);
  ui_surface_texture_->SetDefaultBufferSize(ui_tex_physical_size_.width,
                                            ui_tex_physical_size_.height);

  main_thread_task_runner_->PostTask(FROM_HERE, base::Bind(
      &VrShell::SurfacesChanged, weak_vr_shell_,
      content_surface_->j_surface().obj(),
      ui_surface_->j_surface().obj()));

  InitializeRenderer();

  draw_task_.Reset(base::Bind(&VrShellGl::DrawFrame, base::Unretained(this)));
  ScheduleNextDrawFrame();

  ready_to_draw_ = true;
}

void VrShellGl::OnUIFrameAvailable() {
  ui_surface_texture_->UpdateTexImage();
}

void VrShellGl::OnContentFrameAvailable() {
  content_surface_texture_->UpdateTexImage();
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
  // While WebVR is going through the compositor path, it shares
  // the same texture ID. This will change once it gets its own
  // surface, but store it separately to avoid future confusion.
  // TODO(klausw,crbug.com/655722): remove this.
  webvr_texture_id_ = content_texture_id_;
  // Out of paranoia, explicitly reset the "pose valid" flags to false
  // from the GL thread. The constructor ran in the UI thread.
  // TODO(klausw,crbug.com/655722): remove this.
  webvr_head_pose_valid_.assign(kPoseRingBufferSize, false);

  gvr_api_->InitializeGl();
  std::vector<gvr::BufferSpec> specs;
  // For kFramePrimaryBuffer (primary VrShell and WebVR content)
  specs.push_back(gvr_api_->CreateBufferSpec());
  render_size_primary_ = specs[kFramePrimaryBuffer].GetSize();

  // For kFrameHeadlockedBuffer (for WebVR insecure content warning).
  // Set this up at fixed resolution, the (smaller) FOV gets set below.
  specs.push_back(gvr_api_->CreateBufferSpec());
  specs.back().SetSize(kHeadlockedBufferDimensions);
  render_size_headlocked_ = specs[kFrameHeadlockedBuffer].GetSize();

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

  main_thread_task_runner_->PostTask(FROM_HERE, base::Bind(
      &VrShell::GvrDelegateReady, weak_vr_shell_));
}

void VrShellGl::UpdateController(const gvr::Vec3f& forward_vector) {
  controller_->UpdateState();

  // Note that button up/down state is transient, so ButtonUpHappened only
  // returns true for a single frame (and we're guaranteed not to miss it).
  if (controller_->ButtonUpHappened(
          gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP)) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VrShell::AppButtonPressed, weak_vr_shell_));
  }

  if (web_vr_mode_) {
    // Process screen touch events for Cardboard button compatibility.
    // Also send tap events for controller "touchpad click" events.
    if (touch_pending_ || controller_->ButtonUpHappened(
            gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK)) {
      touch_pending_ = false;
      std::unique_ptr<WebGestureEvent> gesture(new WebGestureEvent(
          WebInputEvent::GestureTapDown, WebInputEvent::NoModifiers,
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF()));
      gesture->sourceDevice = blink::WebGestureDeviceTouchpad;
      gesture->x = 0;
      gesture->y = 0;
      SendGesture(InputTarget::CONTENT, std::move(gesture));
    }

    return;
  }

  gvr::Vec3f ergo_neutral_pose;
  if (!controller_->IsConnected()) {
    // No controller detected, set up a gaze cursor that tracks the
    // forward direction.
    ergo_neutral_pose = {0.0f, 0.0f, -1.0f};
    controller_quat_ = GetRotationFromZAxis(forward_vector);
  } else {
    ergo_neutral_pose = {0.0f, -sin(kErgoAngleOffset), -cos(kErgoAngleOffset)};
    controller_quat_ = controller_->Orientation();
  }

  gvr::Mat4f mat = QuatToMatrix(controller_quat_);
  gvr::Vec3f forward = MatrixVectorMul(mat, ergo_neutral_pose);
  gvr::Vec3f origin = kHandPosition;

  // If we place the reticle based on elements intersecting the controller beam,
  // we can end up with the reticle hiding behind elements, or jumping laterally
  // in the field of view. This is physically correct, but hard to use. For
  // usability, do the following instead:
  //
  // - Project the controller laser onto an outer surface, which is the
  //   closer of the desktop plane, or a distance-limiting sphere.
  // - Create a vector between the eyes and the outer surface point.
  // - If any UI elements intersect this vector, choose the closest to the eyes,
  //   and place the reticle at the intersection point.

  // Find distance to a corner of the content quad, and limit the cursor
  // distance to a multiple of that distance. This lets us keep the reticle on
  // the content plane near the content window, and on the surface of a sphere
  // in other directions. Note that this approach uses distance from controller,
  // rather than eye, for simplicity. This will make the sphere slightly
  // off-center.
  float distance = kDefaultReticleDistance;
  ContentRectangle* content_plane = scene_->GetContentQuad();
  if (content_plane) {
    distance = content_plane->GetRayDistance(origin, forward);
    gvr::Vec3f corner = {0.5f, 0.5f, 0.0f};
    corner = MatrixVectorMul(content_plane->transform.to_world, corner);
    float max_distance = Distance(origin, corner) * kReticleDistanceMultiplier;
    if (distance > max_distance || distance <= 0.0f) {
      distance = max_distance;
    }
  }

  target_point_ = GetRayPoint(origin, forward, distance);
  gvr::Vec3f eye_to_target = target_point_;
  NormalizeVector(eye_to_target);

  // Determine which UI element (if any) intersects the line between the eyes
  // and the controller target position.
  float closest_element_distance = std::numeric_limits<float>::infinity();
  int pixel_x = 0;
  int pixel_y = 0;
  target_element_ = nullptr;
  InputTarget input_target = InputTarget::NONE;

  for (const auto& plane : scene_->GetUiElements()) {
    if (!plane->IsHitTestable())
      continue;

    float distance_to_plane = plane->GetRayDistance(kOrigin, eye_to_target);
    gvr::Vec3f plane_intersection_point =
        GetRayPoint(kOrigin, eye_to_target, distance_to_plane);

    gvr::Vec3f rect_2d_point =
        MatrixVectorMul(plane->transform.from_world, plane_intersection_point);
    if (distance_to_plane > 0 && distance_to_plane < closest_element_distance) {
      float x = rect_2d_point.x + 0.5f;
      float y = 0.5f - rect_2d_point.y;
      bool is_inside = x >= 0.0f && x < 1.0f && y >= 0.0f && y < 1.0f;
      if (!is_inside)
        continue;

      closest_element_distance = distance_to_plane;
      Rectf pixel_rect;
      if (plane->content_quad) {
        pixel_rect = {0, 0, content_tex_css_width_, content_tex_css_height_};
      } else {
        pixel_rect = {plane->copy_rect.x, plane->copy_rect.y,
                      plane->copy_rect.width, plane->copy_rect.height};
      }
      pixel_x = pixel_rect.width * x + pixel_rect.x;
      pixel_y = pixel_rect.height * y + pixel_rect.y;

      target_point_ = plane_intersection_point;
      target_element_ = plane.get();
      input_target = plane->content_quad ? InputTarget::CONTENT
                                         : InputTarget::UI;
    }
  }
  SendEventsToTarget(input_target, pixel_x, pixel_y);
}

void VrShellGl::SendEventsToTarget(InputTarget input_target,
                                   int pixel_x,
                                   int pixel_y) {
  std::vector<std::unique_ptr<WebGestureEvent>> gesture_list =
      controller_->DetectGestures();
  double timestamp = gesture_list.front()->timeStampSeconds();

  if (touch_pending_) {
    touch_pending_ = false;
    std::unique_ptr<WebGestureEvent> event(new WebGestureEvent(
        WebInputEvent::GestureTapDown, WebInputEvent::NoModifiers, timestamp));
    event->sourceDevice = blink::WebGestureDeviceTouchpad;
    event->x = pixel_x;
    event->y = pixel_y;
    gesture_list.push_back(std::move(event));
  }

  for (const auto& gesture : gesture_list) {
    switch (gesture->type()) {
      case WebInputEvent::GestureScrollBegin:
      case WebInputEvent::GestureScrollUpdate:
      case WebInputEvent::GestureScrollEnd:
      case WebInputEvent::GestureFlingCancel:
      case WebInputEvent::GestureFlingStart:
        SendGesture(InputTarget::CONTENT,
                    base::WrapUnique(new WebGestureEvent(*gesture)));
        break;
      case WebInputEvent::GestureTapDown:
        gesture->x = pixel_x;
        gesture->y = pixel_y;
        if (input_target != InputTarget::NONE)
          SendGesture(input_target,
                      base::WrapUnique(new WebGestureEvent(*gesture)));
        break;
      case WebInputEvent::Undefined:
        break;
      default:
        NOTREACHED();
    }
  }

  // Hover support
  bool new_target = input_target != current_input_target_;
  if (new_target && current_input_target_ != InputTarget::NONE) {
    // Send a move event indicating that the pointer moved off of an element.
    SendGesture(current_input_target_,
                MakeMouseEvent(WebInputEvent::MouseLeave, timestamp, 0, 0));
  }
  current_input_target_ = input_target;
  if (current_input_target_ != InputTarget::NONE) {
    WebInputEvent::Type type =
        new_target ? WebInputEvent::MouseEnter : WebInputEvent::MouseMove;
    SendGesture(input_target,
                MakeMouseEvent(type, timestamp, pixel_x, pixel_y));
  }
}

void VrShellGl::SendGesture(InputTarget input_target,
                            std::unique_ptr<blink::WebInputEvent> event) {
  DCHECK(input_target != InputTarget::NONE);
  auto&& target = input_target == InputTarget::CONTENT
                      ? &VrShell::ProcessContentGesture
                      : &VrShell::ProcessUIGesture;
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(target, weak_vr_shell_, base::Passed(std::move(event))));
}

void VrShellGl::SetGvrPoseForWebVr(const gvr::Mat4f& pose, uint32_t pose_num) {
  webvr_head_pose_[pose_num % kPoseRingBufferSize] = pose;
  webvr_head_pose_valid_[pose_num % kPoseRingBufferSize] = true;
}

bool VrShellGl::WebVrPoseByteIsValid(int pose_index_byte) {
  if (pose_index_byte < 0) {
    return false;
  }
  if (!webvr_head_pose_valid_[pose_index_byte % kPoseRingBufferSize]) {
    VLOG(1) << "WebVR: reject decoded pose index " << pose_index_byte <<
        ", not a valid pose";
    return false;
  }
  return true;
}

void VrShellGl::DrawFrame() {
  TRACE_EVENT0("gpu", "VrShellGl::DrawFrame");
  // Reset the viewport list to just the pair of viewports for the
  // primary buffer each frame. Head-locked viewports get added by
  // DrawVrShell if needed.
  buffer_viewport_list_->SetToRecommendedBufferViewports();

  gvr::Frame frame = swap_chain_->AcquireFrame();
  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += kPredictionTimeWithoutVsyncNanos;

  gvr::Mat4f head_pose =
      gvr_api_->GetHeadSpaceFromStartSpaceRotation(target_time);

  gvr::Vec3f position = GetTranslation(head_pose);
  if (position.x == 0.0f && position.y == 0.0f && position.z == 0.0f) {
    // This appears to be a 3DOF pose without a neck model. Add one.
    // The head pose has redundant data. Assume we're only using the
    // object_from_reference_matrix, we're not updating position_external.
    // TODO: Not sure what object_from_reference_matrix is. The new api removed
    // it. For now, removing it seems working fine.
    gvr_api_->ApplyNeckModel(head_pose, 1.0f);
  }

  frame.BindBuffer(kFramePrimaryBuffer);

  // Update the render position of all UI elements (including desktop).
  const float screen_tilt = kDesktopScreenTiltDefault * M_PI / 180.0f;
  scene_->UpdateTransforms(screen_tilt, TimeInMicroseconds());

  UpdateController(GetForwardVector(head_pose));

  if (web_vr_mode_) {
    DrawWebVr();

    // When using async reprojection, we need to know which pose was used in
    // the WebVR app for drawing this frame. Due to unknown amounts of
    // buffering in the compositor and SurfaceTexture, we read the pose number
    // from a corner pixel. There's no point in doing this for legacy
    // distortion rendering since that doesn't need a pose, and reading back
    // pixels is an expensive operation. TODO(klausw,crbug.com/655722): stop
    // doing this once we have working no-compositor rendering for WebVR.
    if (gvr_api_->GetAsyncReprojectionEnabled()) {
      int pose_index_byte = GetPixelEncodedPoseIndexByte();
      if (WebVrPoseByteIsValid(pose_index_byte)) {
        // We have a valid pose, use it for reprojection.
        webvr_left_viewport_->SetReprojection(GVR_REPROJECTION_FULL);
        webvr_right_viewport_->SetReprojection(GVR_REPROJECTION_FULL);
        head_pose = webvr_head_pose_[pose_index_byte % kPoseRingBufferSize];
        // We can't mark the used pose as invalid since unfortunately
        // we have to reuse them. The compositor will re-submit stale
        // frames on vsync, and we can't tell that this has happened
        // until we've read the pose index from it, and at that point
        // it's too late to skip rendering.
      } else {
        // If we don't get a valid frame ID back we shouldn't attempt
        // to reproject by an invalid matrix, so turn off reprojection
        // instead. Invalid poses can permanently break reprojection
        // for this GVR instance: http://crbug.com/667327
        webvr_left_viewport_->SetReprojection(GVR_REPROJECTION_NONE);
        webvr_right_viewport_->SetReprojection(GVR_REPROJECTION_NONE);
      }
    }
  }

  DrawVrShell(head_pose, frame);

  frame.Unbind();
  frame.Submit(*buffer_viewport_list_, head_pose);

  // No need to swap buffers for surfaceless rendering.
  if (surfaceless_rendering_) {
    ScheduleNextDrawFrame();
    return;
  }

  if (surface_->SupportsAsyncSwap()) {
    surface_->SwapBuffersAsync(base::Bind(&WaitForSwapAck, base::Bind(
        &VrShellGl::ScheduleNextDrawFrame, weak_ptr_factory_.GetWeakPtr())));
  } else {
    surface_->SwapBuffers();
    ScheduleNextDrawFrame();
  }
}

void VrShellGl::DrawVrShell(const gvr::Mat4f& head_pose,
                            gvr::Frame &frame) {
  TRACE_EVENT0("gpu", "VrShellGl::DrawVrShell");
  std::vector<const ContentRectangle*> head_locked_elements;
  std::vector<const ContentRectangle*> world_elements;
  for (const auto& rect : scene_->GetUiElements()) {
    if (!rect->IsVisible())
      continue;
    if (rect->lock_to_fov) {
      head_locked_elements.push_back(rect.get());
    } else {
      world_elements.push_back(rect.get());
    }
  }

  if (web_vr_mode_) {
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

    glClearColor(BackgroundRenderer::kFogBrightness,
                 BackgroundRenderer::kFogBrightness,
                 BackgroundRenderer::kFogBrightness, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  if (!world_elements.empty()) {
    DrawUiView(&head_pose, world_elements, render_size_primary_,
               kViewportListPrimaryOffset);
  }

  if (!head_locked_elements.empty()) {
    // Add head-locked viewports. The list gets reset to just
    // the recommended viewports (for the primary buffer) each frame.
    buffer_viewport_list_->SetBufferViewport(
        kViewportListHeadlockedOffset + GVR_LEFT_EYE,
        *headlocked_left_viewport_);
    buffer_viewport_list_->SetBufferViewport(
        kViewportListHeadlockedOffset + GVR_RIGHT_EYE,
        *headlocked_right_viewport_);

    // Bind the headlocked framebuffer.
    // TODO(mthiesse): We don't unbind this? Maybe some cleanup is in order
    // here.
    frame.BindBuffer(kFrameHeadlockedBuffer);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    DrawUiView(nullptr, head_locked_elements, render_size_headlocked_,
               kViewportListHeadlockedOffset);
  }
}

gvr::Sizei VrShellGl::GetWebVRCompositorSurfaceSize() {
  // This is a stopgap while we're using the WebVR compositor rendering path.
  // TODO(klausw,crbug.com/655722): Remove this method and member once we're
  // using a separate WebVR render surface.
  return content_tex_physical_size_;
}

void VrShellGl::DrawUiView(const gvr::Mat4f* head_pose,
                           const std::vector<const ContentRectangle*>& elements,
                           const gvr::Sizei& render_size,
                           int viewport_offset) {
  TRACE_EVENT0("gpu", "VrShellGl::DrawUiView");
  for (auto eye : {GVR_LEFT_EYE, GVR_RIGHT_EYE}) {
    buffer_viewport_list_->GetBufferViewport(
        eye + viewport_offset, buffer_viewport_.get());

    gvr::Mat4f view_matrix = gvr_api_->GetEyeFromHeadMatrix(eye);
    if (head_pose != nullptr) {
      view_matrix = MatrixMul(view_matrix, *head_pose);
    }

    gvr::Recti pixel_rect =
        CalculatePixelSpaceRect(render_size, buffer_viewport_->GetSourceUv());
    glViewport(pixel_rect.left, pixel_rect.bottom,
               pixel_rect.right - pixel_rect.left,
               pixel_rect.top - pixel_rect.bottom);

    const gvr::Mat4f render_matrix = MatrixMul(
        PerspectiveMatrixFromView(
            buffer_viewport_->GetSourceFov(), kZNear, kZFar),
        view_matrix);

    if (!web_vr_mode_) {
      // TODO(tiborg): Enable through the UI API.
      // DrawBackground(render_matrix);
    }
    DrawElements(render_matrix, elements);
    if (head_pose != nullptr && !web_vr_mode_) {
      DrawCursor(render_matrix);
    }
  }
}

void VrShellGl::DrawElements(
    const gvr::Mat4f& render_matrix,
    const std::vector<const ContentRectangle*>& elements) {
  for (const auto& rect : elements) {
    Rectf copy_rect;
    jint texture_handle;
    if (rect->content_quad) {
      copy_rect = {0, 0, 1, 1};
      texture_handle = content_texture_id_;
    } else {
      copy_rect.x = static_cast<float>(rect->copy_rect.x) / ui_tex_css_width_;
      copy_rect.y = static_cast<float>(rect->copy_rect.y) / ui_tex_css_height_;
      copy_rect.width = static_cast<float>(rect->copy_rect.width) /
          ui_tex_css_width_;
      copy_rect.height = static_cast<float>(rect->copy_rect.height) /
          ui_tex_css_height_;
      texture_handle = ui_texture_id_;
    }
    gvr::Mat4f transform = MatrixMul(render_matrix, rect->transform.to_world);
    vr_shell_renderer_->GetTexturedQuadRenderer()->Draw(
        texture_handle, transform, copy_rect, rect->computed_opacity);
  }
}

void VrShellGl::DrawCursor(const gvr::Mat4f& render_matrix) {
  gvr::Mat4f mat;
  SetIdentityM(mat);

  // Draw the reticle.

  // Scale the pointer to have a fixed FOV size at any distance.
  const float eye_to_target = Distance(target_point_, kOrigin);
  ScaleM(mat, mat, kReticleWidth * eye_to_target,
         kReticleHeight * eye_to_target, 1.0f);

  gvr::Quatf rotation;
  if (target_element_ != nullptr) {
    // Make the reticle planar to the element it's hitting.
    rotation = GetRotationFromZAxis(target_element_->GetNormal());
  } else {
    // Rotate the cursor to directly face the eyes.
    rotation = GetRotationFromZAxis(target_point_);
  }
  mat = MatrixMul(QuatToMatrix(rotation), mat);

  // Place the pointer slightly in front of the plane intersection point.
  TranslateM(mat, mat, target_point_.x * kReticleOffset,
             target_point_.y * kReticleOffset,
             target_point_.z * kReticleOffset);

  gvr::Mat4f transform = MatrixMul(render_matrix, mat);
  vr_shell_renderer_->GetReticleRenderer()->Draw(transform);

  // Draw the laser.

  // Find the length of the beam (from hand to target).
  const float laser_length = Distance(kHandPosition, target_point_);

  // Build a beam, originating from the origin.
  SetIdentityM(mat);

  // Move the beam half its height so that its end sits on the origin.
  TranslateM(mat, mat, 0.0f, 0.5f, 0.0f);
  ScaleM(mat, mat, kLaserWidth, laser_length, 1);

  // Tip back 90 degrees to flat, pointing at the scene.
  const gvr::Quatf q = QuatFromAxisAngle({1.0f, 0.0f, 0.0f}, -M_PI / 2);
  mat = MatrixMul(QuatToMatrix(q), mat);

  const gvr::Vec3f beam_direction = {
    target_point_.x - kHandPosition.x,
    target_point_.y - kHandPosition.y,
    target_point_.z - kHandPosition.z
  };
  const gvr::Mat4f beam_direction_mat =
      QuatToMatrix(GetRotationFromZAxis(beam_direction));

  // Render multiple faces to make the laser appear cylindrical.
  const int faces = 4;
  for (int i = 0; i < faces; i++) {
    // Rotate around Z.
    const float angle = M_PI * 2 * i / faces;
    const gvr::Quatf rot = QuatFromAxisAngle({0.0f, 0.0f, 1.0f}, angle);
    gvr::Mat4f face_transform = MatrixMul(QuatToMatrix(rot), mat);

    // Orient according to target direction.
    face_transform = MatrixMul(beam_direction_mat, face_transform);

    // Move the beam origin to the hand.
    TranslateM(face_transform, face_transform, kHandPosition.x, kHandPosition.y,
               kHandPosition.z);

    transform = MatrixMul(render_matrix, face_transform);
    vr_shell_renderer_->GetLaserRenderer()->Draw(transform);
  }
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

  glViewport(0, 0, render_size_primary_.width, render_size_primary_.height);
  vr_shell_renderer_->GetWebVrRenderer()->Draw(webvr_texture_id_);

  buffer_viewport_list_->SetBufferViewport(GVR_LEFT_EYE,
                                           *webvr_left_viewport_);
  buffer_viewport_list_->SetBufferViewport(GVR_RIGHT_EYE,
                                           *webvr_right_viewport_);
}

void VrShellGl::DrawBackground(const gvr::Mat4f& render_matrix) {
  vr_shell_renderer_->GetBackgroundRenderer()->Draw(render_matrix);
}

void VrShellGl::OnTriggerEvent() {
  // Set a flag to handle this on the render thread at the next frame.
  touch_pending_ = true;
}

void VrShellGl::OnPause() {
  draw_task_.Cancel();
  controller_->OnPause();
  gvr_api_->PauseTracking();
}

void VrShellGl::OnResume() {
  gvr_api_->RefreshViewerProfile();
  gvr_api_->ResumeTracking();
  controller_->OnResume();
  if (ready_to_draw_) {
    draw_task_.Reset(base::Bind(&VrShellGl::DrawFrame, base::Unretained(this)));
    ScheduleNextDrawFrame();
  }
}

void VrShellGl::SetWebVrMode(bool enabled) {
  web_vr_mode_ = enabled;
}

void VrShellGl::UpdateWebVRTextureBounds(const gvr::Rectf& left_bounds,
                                         const gvr::Rectf& right_bounds) {
  webvr_left_viewport_->SetSourceUv(left_bounds);
  webvr_right_viewport_->SetSourceUv(right_bounds);
}

gvr::GvrApi* VrShellGl::gvr_api() {
  return gvr_api_.get();
}

void VrShellGl::ContentBoundsChanged(int width, int height) {
  TRACE_EVENT0("gpu", "VrShellGl::ContentBoundsChanged");
  content_tex_css_width_ = width;
  content_tex_css_height_ = height;
}

void VrShellGl::ContentPhysicalBoundsChanged(int width, int height) {
  if (content_surface_texture_.get())
    content_surface_texture_->SetDefaultBufferSize(width, height);
  content_tex_physical_size_.width = width;
  content_tex_physical_size_.height = height;
}

void VrShellGl::UIBoundsChanged(int width, int height) {
  ui_tex_css_width_ = width;
  ui_tex_css_height_ = height;
}

void VrShellGl::UIPhysicalBoundsChanged(int width, int height) {
  if (ui_surface_texture_.get())
    ui_surface_texture_->SetDefaultBufferSize(width, height);
  ui_tex_physical_size_.width = width;
  ui_tex_physical_size_.height = height;
}

base::WeakPtr<VrShellGl> VrShellGl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void VrShellGl::UpdateVSyncParameters(const base::TimeTicks timebase,
                                      const base::TimeDelta interval) {
  vsync_timebase_ = timebase;
  vsync_interval_ = interval;
}

void VrShellGl::ScheduleNextDrawFrame() {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks target;

  if (vsync_interval_.is_zero()) {
    target = now;
  } else {
    target = now + vsync_interval_;
    int64_t intervals = (target - vsync_timebase_) / vsync_interval_;
    target = vsync_timebase_ + intervals * vsync_interval_;
  }

  task_runner_->PostDelayedTask(FROM_HERE, draw_task_.callback(), target - now);
}

void VrShellGl::ForceExitVr() {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ForceExitVr, weak_vr_shell_));
}

void VrShellGl::UpdateScene(std::unique_ptr<base::ListValue> commands) {
  scene_->HandleCommands(std::move(commands), TimeInMicroseconds());
}

}  // namespace vr_shell
