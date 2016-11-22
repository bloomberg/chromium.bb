// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/android/vr_shell/ui_elements.h"
#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"
#include "chrome/browser/android/vr_shell/vr_compositor.h"
#include "chrome/browser/android/vr_shell/vr_controller.h"
#include "chrome/browser/android/vr_shell/vr_gl_util.h"
#include "chrome/browser/android/vr_shell/vr_input_manager.h"
#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"
#include "chrome/browser/android/vr_shell/vr_shell_renderer.h"
#include "chrome/browser/android/vr_shell/vr_usage_monitor.h"
#include "chrome/browser/android/vr_shell/vr_web_contents_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "jni/VrShellImpl_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/init/gl_factory.h"

using base::android::JavaParamRef;

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

vr_shell::VrShell* g_instance;

static const char kVrShellUIURL[] = "chrome://vr-shell-ui";

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

blink::WebMouseEvent MakeMouseEvent(WebInputEvent::Type type,
                                    double timestamp,
                                    float x,
                                    float y) {
  blink::WebMouseEvent mouse_event;
  mouse_event.type = type;
  mouse_event.pointerType = blink::WebPointerProperties::PointerType::Mouse;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.windowX = x;
  mouse_event.windowY = y;
  mouse_event.timeStampSeconds = timestamp;
  mouse_event.clickCount = 1;
  mouse_event.modifiers = 0;

  return mouse_event;
}

}  // namespace

namespace vr_shell {

VrShell::VrShell(JNIEnv* env,
                 jobject obj,
                 content::WebContents* main_contents,
                 ui::WindowAndroid* content_window,
                 content::WebContents* ui_contents,
                 ui::WindowAndroid* ui_window)
    : WebContentsObserver(ui_contents),
      main_contents_(main_contents),
      ui_contents_(ui_contents),
      metrics_helper_(new VrMetricsHelper(main_contents)),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  DCHECK(g_instance == nullptr);
  g_instance = this;
  j_vr_shell_.Reset(env, obj);
  scene_.reset(new UiScene);
  html_interface_.reset(new UiInterface);
  content_compositor_.reset(new VrCompositor(content_window, false));
  ui_compositor_.reset(new VrCompositor(ui_window, true));
  vr_web_contents_observer_.reset(
      new VrWebContentsObserver(main_contents, html_interface_.get()));

  LoadUIContent();

  gvr::Mat4f identity;
  SetIdentityM(identity);
  webvr_head_pose_.resize(kPoseRingBufferSize, identity);
}

void VrShell::UpdateCompositorLayers(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  content_compositor_->SetLayer(main_contents_);
  ui_compositor_->SetLayer(ui_contents_);
}

void VrShell::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void VrShell::LoadUIContent() {
  GURL url(kVrShellUIURL);
  ui_contents_->GetController().LoadURL(
      url, content::Referrer(),
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string(""));
}

bool RegisterVrShell(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

VrShell::~VrShell() {
  if (delegate_ && delegate_->GetDeviceProvider()) {
    delegate_->GetDeviceProvider()->OnGvrDelegateRemoved();
  }
  g_instance = nullptr;
  gl::init::ClearGLBindings();
}

void VrShell::SetDelegate(JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& delegate) {
  base::AutoLock lock(gvr_init_lock_);
  delegate_ = VrShellDelegate::GetNativeDelegate(env, delegate);
  if (gvr_api_) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&device::GvrDeviceProvider::OnGvrDelegateReady,
                              delegate_->GetDeviceProvider(),
                              weak_ptr_factory_.GetWeakPtr()));
  }
}

enum class ViewerType {
  UNKNOWN_TYPE = 0,
  CARDBOARD = 1,
  DAYDREAM = 2,
  VIEWER_TYPE_MAX,
};

void VrShell::GvrInit(JNIEnv* env,
                      const JavaParamRef<jobject>& obj,
                      jlong native_gvr_api) {
  base::AutoLock lock(gvr_init_lock_);

  // set the initial webvr state
  metrics_helper_->SetWebVREnabled(webvr_mode_);
  metrics_helper_->SetVRActive(true);

  gvr_api_ =
      gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(native_gvr_api));
  // TODO(klausw,crbug.com/655722): should report OnGvrDelegateReady here once
  // we switch to using a WebVR render surface. We currently need to wait for
  // the compositor window's size to be known first. See also
  // ContentSurfaceChanged.
  controller_.reset(
      new VrController(reinterpret_cast<gvr_context*>(native_gvr_api)));
  content_input_manager_ = new VrInputManager(main_contents_);
  ui_input_manager_ = new VrInputManager(ui_contents_);

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

void VrShell::InitializeGl(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jint content_texture_handle,
                           jint ui_texture_handle) {
  CHECK(gl::GetGLImplementation() != gl::kGLImplementationNone ||
        gl::init::InitializeGLOneOff());

  content_texture_id_ = content_texture_handle;
  ui_texture_id_ = ui_texture_handle;

  // While WebVR is going through the compositor path, it shares
  // the same texture ID. This will change once it gets its own
  // surface, but store it separately to avoid future confusion.
  // TODO(klausw,crbug.com/655722): remove this.
  webvr_texture_id_ = content_texture_id_;

  gvr_api_->InitializeGl();
  std::vector<gvr::BufferSpec> specs;
  // For kFramePrimaryBuffer (primary VrShell and WebVR content)
  specs.push_back(gvr_api_->CreateBufferSpec());
  render_size_primary_ = specs[kFramePrimaryBuffer].GetSize();
  render_size_primary_vrshell_ = render_size_primary_;

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
}

void VrShell::UpdateController(const gvr::Vec3f& forward_vector) {
  controller_->UpdateState();

#if defined(ENABLE_VR_SHELL)
  // Note that button up/down state is transient, so IsButtonUp only returns
  // true for a single frame (and we're guaranteed not to miss it).
  if (controller_->IsButtonUp(
      gvr::ControllerButton::GVR_CONTROLLER_BUTTON_APP)) {
    if (html_interface_->GetMode() == UiInterface::Mode::MENU) {
      // Temporary: Hit app button a second time to exit menu mode.
      if (webvr_mode_) {
        html_interface_->SetMode(UiInterface::Mode::WEB_VR);
        main_thread_task_runner_->PostTask(
            FROM_HERE, base::Bind(&device::GvrDeviceProvider::OnDisplayFocus,
                                  delegate_->GetDeviceProvider()));
      } else {
        html_interface_->SetMode(UiInterface::Mode::STANDARD);
      }
    } else {
      if (html_interface_->GetMode() == UiInterface::Mode::WEB_VR) {
        main_thread_task_runner_->PostTask(
            FROM_HERE, base::Bind(&device::GvrDeviceProvider::OnDisplayBlur,
                                  delegate_->GetDeviceProvider()));
      }
      html_interface_->SetMode(UiInterface::Mode::MENU);
      // TODO(mthiesse): The page is no longer visible here. We should unfocus
      // or otherwise let it know it's hidden.
    }
  }
#endif
  if (html_interface_->GetMode() == UiInterface::Mode::WEB_VR) {
    // Process screen touch events for Cardboard button compatibility.
    // Also send tap events for controller "touchpad click" events.
    if (touch_pending_ || controller_->IsButtonUp(
            gvr::ControllerButton::GVR_CONTROLLER_BUTTON_CLICK)) {
      touch_pending_ = false;
      std::unique_ptr<WebGestureEvent> gesture(new WebGestureEvent());
      gesture->sourceDevice = blink::WebGestureDeviceTouchpad;
      gesture->timeStampSeconds =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
      gesture->type = WebInputEvent::GestureTapDown;
      gesture->data.tapDown.width = 0;
      gesture->data.tapDown.height = 0;
      content_input_manager_->ProcessUpdatedGesture(*gesture.get());
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
  VrInputManager* input_target = nullptr;

  for (const auto& plane : scene_->GetUiElements()) {
    if (!plane->visible || !plane->hit_testable) {
      continue;
    }
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
        pixel_rect = {0, 0, content_tex_width_, content_tex_height_};
      } else {
        pixel_rect = {plane->copy_rect.x, plane->copy_rect.y,
                      plane->copy_rect.width, plane->copy_rect.height};
      }
      pixel_x = pixel_rect.width * x + pixel_rect.x;
      pixel_y = pixel_rect.height * y + pixel_rect.y;

      target_point_ = plane_intersection_point;
      target_element_ = plane.get();
      input_target = plane->content_quad ? content_input_manager_.get()
                                         : ui_input_manager_.get();
    }
  }
  SendEventsToTarget(input_target, pixel_x, pixel_y);
}

void VrShell::SendEventsToTarget(VrInputManager* input_target,
                                 int pixel_x,
                                 int pixel_y) {
  std::vector<std::unique_ptr<WebGestureEvent>> gesture_list =
      controller_->DetectGestures();
  std::unique_ptr<WebGestureEvent> gesture = std::move(gesture_list.front());

  // TODO(asimjour) for now, scroll is sent to the main content.
  if (gesture->type == WebInputEvent::GestureScrollBegin ||
      gesture->type == WebInputEvent::GestureScrollUpdate ||
      gesture->type == WebInputEvent::GestureScrollEnd ||
      gesture->type == WebInputEvent::GestureFlingCancel) {
    content_input_manager_->ProcessUpdatedGesture(*gesture.get());
  }

  if (gesture->type == WebInputEvent::GestureScrollEnd) {
    CHECK(gesture_list.size() == 2);
    std::unique_ptr<WebGestureEvent> fling_gesture =
        std::move(gesture_list.back());
    content_input_manager_->ProcessUpdatedGesture(*fling_gesture.get());
  }

  WebInputEvent::Type original_type = gesture->type;

  bool new_target = input_target != current_input_target_;
  if (new_target && current_input_target_ != nullptr) {
    // Send a move event indicating that the pointer moved off of an element.
    blink::WebMouseEvent mouse_event = MakeMouseEvent(
        WebInputEvent::MouseLeave, gesture->timeStampSeconds, 0, 0);
    current_input_target_->ProcessUpdatedGesture(mouse_event);
  }
  current_input_target_ = input_target;
  if (current_input_target_ == nullptr) {
    return;
  }
  WebInputEvent::Type type =
      new_target ? WebInputEvent::MouseEnter : WebInputEvent::MouseMove;
  blink::WebMouseEvent mouse_event =
      MakeMouseEvent(type, gesture->timeStampSeconds, pixel_x, pixel_y);
  current_input_target_->ProcessUpdatedGesture(mouse_event);

  if (original_type == WebInputEvent::GestureTapDown || touch_pending_) {
    if (touch_pending_) {
      touch_pending_ = false;
      gesture->sourceDevice = blink::WebGestureDeviceTouchpad;
      gesture->timeStampSeconds =
          (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    }
    gesture->type = WebInputEvent::GestureTapDown;
    gesture->data.tapDown.width = pixel_x;
    gesture->data.tapDown.height = pixel_y;
    current_input_target_->ProcessUpdatedGesture(*gesture.get());
  }
}

void VrShell::SetGvrPoseForWebVr(const gvr::Mat4f& pose, uint32_t pose_num) {
  webvr_head_pose_[pose_num % kPoseRingBufferSize] = pose;
}

uint32_t GetPixelEncodedPoseIndex() {
  TRACE_EVENT0("gpu", "VrShell::GetPixelEncodedPoseIndex");
  // Read the pose index encoded in a bottom left pixel as color values.
  // See also third_party/WebKit/Source/modules/vr/VRDisplay.cpp which
  // encodes the pose index, and device/vr/android/gvr/gvr_device.cc
  // which tracks poses.
  uint8_t pixels[4];
  // Assume we're reading from the framebuffer we just wrote to.
  // That's true currently, we may need to use glReadBuffer(GL_BACK)
  // or equivalent if the rendering setup changes in the future.
  glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  return pixels[0] | (pixels[1] << 8) | (pixels[2] << 16);
}

void VrShell::DrawFrame(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  TRACE_EVENT0("gpu", "VrShell::DrawFrame");
  // Reset the viewport list to just the pair of viewports for the
  // primary buffer each frame. Head-locked viewports get added by
  // DrawVrShell if needed.
  buffer_viewport_list_->SetToRecommendedBufferViewports();

  if (html_interface_->GetMode() == UiInterface::Mode::WEB_VR) {
    // If needed, resize the primary buffer for use with WebVR.
    if (render_size_primary_ != render_size_primary_webvr_) {
      render_size_primary_ = render_size_primary_webvr_;
      swap_chain_->ResizeBuffer(kFramePrimaryBuffer, render_size_primary_);
    }
  } else {
    if (render_size_primary_ != render_size_primary_vrshell_) {
      render_size_primary_ = render_size_primary_vrshell_;
      swap_chain_->ResizeBuffer(kFramePrimaryBuffer, render_size_primary_);
    }
  }

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

  // Bind the primary framebuffer.
  frame.BindBuffer(kFramePrimaryBuffer);

  HandleQueuedTasks();

  // Update the render position of all UI elements (including desktop).
  const float screen_tilt = kDesktopScreenTiltDefault * M_PI / 180.0f;
  scene_->UpdateTransforms(screen_tilt, UiScene::TimeInMicroseconds());

  UpdateController(GetForwardVector(head_pose));

  if (html_interface_->GetMode() == UiInterface::Mode::WEB_VR) {
    DrawWebVr();

    // When using async reprojection, we need to know which pose was used in
    // the WebVR app for drawing this frame. Due to unknown amounts of
    // buffering in the compositor and SurfaceTexture, we read the pose number
    // from a corner pixel. There's no point in doing this for legacy
    // distortion rendering since that doesn't need a pose, and reading back
    // pixels is an expensive operation. TODO(klausw,crbug.com/655722): stop
    // doing this once we have working no-compositor rendering for WebVR.
    if (gvr_api_->GetAsyncReprojectionEnabled()) {
      uint32_t webvr_pose_frame = GetPixelEncodedPoseIndex();
      // If we don't get a valid frame ID back we shouldn't attempt to reproject
      // by an invalid matrix, so turn of reprojection instead.
      if (webvr_pose_frame == 0) {
        webvr_left_viewport_->SetReprojection(GVR_REPROJECTION_NONE);
        webvr_right_viewport_->SetReprojection(GVR_REPROJECTION_NONE);
      } else {
        webvr_left_viewport_->SetReprojection(GVR_REPROJECTION_FULL);
        webvr_right_viewport_->SetReprojection(GVR_REPROJECTION_FULL);
        head_pose = webvr_head_pose_[webvr_pose_frame % kPoseRingBufferSize];
      }
    }
  }

  DrawVrShell(head_pose, frame);

  frame.Unbind();
  frame.Submit(*buffer_viewport_list_, head_pose);
}

void VrShell::DrawVrShell(const gvr::Mat4f& head_pose,
                          gvr::Frame &frame) {
  TRACE_EVENT0("gpu", "VrShell::DrawVrShell");
  std::vector<const ContentRectangle*> head_locked_elements;
  std::vector<const ContentRectangle*> world_elements;
  for (const auto& rect : scene_->GetUiElements()) {
    if (!rect->visible) {
      continue;
    }
    if (rect->lock_to_fov) {
      head_locked_elements.push_back(rect.get());
    } else {
      world_elements.push_back(rect.get());
    }
  }

  if (html_interface_->GetMode() == UiInterface::Mode::WEB_VR) {
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

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
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
    frame.BindBuffer(kFrameHeadlockedBuffer);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    DrawUiView(nullptr, head_locked_elements, render_size_headlocked_,
               kViewportListHeadlockedOffset);
  }
}

void VrShell::SetWebVRRenderSurfaceSize(int width, int height) {
  render_size_primary_webvr_.width = width;
  render_size_primary_webvr_.height = height;
  // TODO(klausw,crbug.com/655722): set the WebVR render surface size here once
  // we have that.
}

gvr::Sizei VrShell::GetWebVRCompositorSurfaceSize() {
  // This is a stopgap while we're using the WebVR compositor rendering path.
  // TODO(klausw,crbug.com/655722): Remove this method and member once we're
  // using a separate WebVR render surface.
  return content_tex_pixels_for_webvr_;
}


void VrShell::DrawUiView(const gvr::Mat4f* head_pose,
                         const std::vector<const ContentRectangle*>& elements,
                         const gvr::Sizei& render_size, int viewport_offset) {
  TRACE_EVENT0("gpu", "VrShell::DrawUiView");
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

    DrawElements(render_matrix, elements);
    if (head_pose != nullptr &&
        html_interface_->GetMode() != UiInterface::Mode::WEB_VR) {
      DrawCursor(render_matrix);
    }
  }
}

void VrShell::DrawElements(
    const gvr::Mat4f& render_matrix,
    const std::vector<const ContentRectangle*>& elements) {
  for (const auto& rect : elements) {
    Rectf copy_rect;
    jint texture_handle;
    if (rect->content_quad) {
      copy_rect = {0, 0, 1, 1};
      texture_handle = content_texture_id_;
    } else {
      copy_rect.x = static_cast<float>(rect->copy_rect.x) / ui_tex_width_;
      copy_rect.y = static_cast<float>(rect->copy_rect.y) / ui_tex_height_;
      copy_rect.width = static_cast<float>(rect->copy_rect.width) /
          ui_tex_width_;
      copy_rect.height = static_cast<float>(rect->copy_rect.height) /
          ui_tex_height_;
      texture_handle = ui_texture_id_;
    }
    gvr::Mat4f transform = MatrixMul(render_matrix, rect->transform.to_world);
    vr_shell_renderer_->GetTexturedQuadRenderer()->Draw(
        texture_handle, transform, copy_rect);
  }
}

void VrShell::DrawCursor(const gvr::Mat4f& render_matrix) {
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

void VrShell::DrawWebVr() {
  TRACE_EVENT0("gpu", "VrShell::DrawWebVr");
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

void VrShell::OnTriggerEvent(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  // Set a flag to handle this on the render thread at the next frame.
  touch_pending_ = true;
}

void VrShell::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (gvr_api_ == nullptr)
    return;
  controller_->OnPause();
  gvr_api_->PauseTracking();

  // exit vr session
  metrics_helper_->SetVRActive(false);
}

void VrShell::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (gvr_api_ == nullptr)
    return;

  gvr_api_->RefreshViewerProfile();
  gvr_api_->ResumeTracking();
  controller_->OnResume();

  // exit vr session
  metrics_helper_->SetVRActive(true);
}

base::WeakPtr<VrShell> VrShell::GetWeakPtr(
    const content::WebContents* web_contents) {
  // Ensure that the WebContents requesting the VrShell instance is the one
  // we created.
  if (g_instance != nullptr && g_instance->ui_contents_ == web_contents)
    return g_instance->weak_ptr_factory_.GetWeakPtr();
  return base::WeakPtr<VrShell>(nullptr);
}

void VrShell::OnDomContentsLoaded() {
  html_interface_->SetURL(main_contents_->GetVisibleURL());
  html_interface_->SetLoading(main_contents_->IsLoading());
  html_interface_->OnDomContentsLoaded();
}

void VrShell::SetWebVrMode(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           bool enabled) {
  webvr_mode_ = enabled;
  metrics_helper_->SetWebVREnabled(webvr_mode_);
  if (enabled) {
    html_interface_->SetMode(UiInterface::Mode::WEB_VR);
  } else {
    html_interface_->SetMode(UiInterface::Mode::STANDARD);
  }
}

void VrShell::SetWebVRSecureOrigin(bool secure_origin) {
  html_interface_->SetSecureOrigin(secure_origin);
}

void VrShell::SubmitWebVRFrame() {
}

void VrShell::UpdateWebVRTextureBounds(const gvr::Rectf& left_bounds,
                                       const gvr::Rectf& right_bounds) {
  webvr_left_viewport_->SetSourceUv(left_bounds);
  webvr_right_viewport_->SetSourceUv(right_bounds);
}

gvr::GvrApi* VrShell::gvr_api() {
  return gvr_api_.get();
}

void VrShell::ContentSurfaceChanged(JNIEnv* env,
                                    const JavaParamRef<jobject>& object,
                                    jint width,
                                    jint height,
                                    const JavaParamRef<jobject>& surface) {
  TRACE_EVENT0("gpu", "VrShell::ContentSurfaceChanged");
  // If we have a delegate, must trigger "ready" callback one time only.
  // Do so the first time we got a nonzero size. (This assumes it doesn't
  // change, but once we get resize ability we'll no longer need this hack.)
  // TODO(klausw,crbug.com/655722): remove when we have surface support.
  bool delegate_not_ready = delegate_ && !content_tex_pixels_for_webvr_.width;

  content_compositor_->SurfaceChanged((int)width, (int)height, surface);
  content_tex_pixels_for_webvr_.width = width;
  content_tex_pixels_for_webvr_.height = height;
  float scale_factor = display::Screen::GetScreen()
      ->GetPrimaryDisplay().device_scale_factor();
  content_tex_width_ = width / scale_factor;
  content_tex_height_ = height / scale_factor;

  // TODO(klausw,crbug.com/655722): move this back to GvrInit once we have
  // our own WebVR surface.
  if (delegate_ && delegate_not_ready) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&device::GvrDeviceProvider::OnGvrDelegateReady,
                              delegate_->GetDeviceProvider(),
                              weak_ptr_factory_.GetWeakPtr()));
  }
}

void VrShell::UiSurfaceChanged(JNIEnv* env,
                               const JavaParamRef<jobject>& object,
                               jint width,
                               jint height,
                               const JavaParamRef<jobject>& surface) {
  ui_compositor_->SurfaceChanged((int)width, (int)height, surface);
  float scale_factor = display::Screen::GetScreen()
      ->GetPrimaryDisplay().device_scale_factor();
  ui_tex_width_ = width / scale_factor;
  ui_tex_height_ = height / scale_factor;
}

UiScene* VrShell::GetScene() {
  return scene_.get();
}

UiInterface* VrShell::GetUiInterface() {
  return html_interface_.get();
}

void VrShell::QueueTask(base::Callback<void()>& callback) {
  base::AutoLock lock(task_queue_lock_);
  task_queue_.push(callback);
}

void VrShell::HandleQueuedTasks() {
  // To protect a stream of tasks from blocking rendering indefinitely,
  // process only the number of tasks present when first checked.
  std::vector<base::Callback<void()>> tasks;
  {
    base::AutoLock lock(task_queue_lock_);
    const size_t count = task_queue_.size();
    for (size_t i = 0; i < count; i++) {
      tasks.push_back(task_queue_.front());
      task_queue_.pop();
    }
  }
  for (auto &task : tasks) {
    task.Run();
  }
}

void VrShell::DoUiAction(const UiAction action) {
  content::NavigationController& controller = main_contents_->GetController();
  switch (action) {
    case HISTORY_BACK:
      if (controller.CanGoBack())
        controller.GoBack();
      break;
    case HISTORY_FORWARD:
      if (controller.CanGoForward())
        controller.GoForward();
      break;
    case RELOAD:
      controller.Reload(false);
      break;
#if defined(ENABLE_VR_SHELL_UI_DEV)
    case RELOAD_UI:
      ui_contents_->GetController().Reload(false);
      html_interface_.reset(new UiInterface);
      html_interface_->SetMode(UiInterface::Mode::STANDARD);
      vr_web_contents_observer_->SetUiInterface(html_interface_.get());
      break;
#endif
    case ZOOM_OUT:  // Not handled yet.
    case ZOOM_IN:  // Not handled yet.
      break;
    default:
      NOTREACHED();
  }
}

void VrShell::RenderViewHostChanged(content::RenderViewHost* old_host,
                                    content::RenderViewHost* new_host) {
  new_host->GetWidget()->GetView()->SetBackgroundColor(SK_ColorTRANSPARENT);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& content_web_contents,
           jlong content_window_android,
           const JavaParamRef<jobject>& ui_web_contents,
           jlong ui_window_android) {
  return reinterpret_cast<intptr_t>(new VrShell(
      env, obj, content::WebContents::FromJavaWebContents(content_web_contents),
      reinterpret_cast<ui::WindowAndroid*>(content_window_android),
      content::WebContents::FromJavaWebContents(ui_web_contents),
      reinterpret_cast<ui::WindowAndroid*>(ui_window_android)));
}

}  // namespace vr_shell
