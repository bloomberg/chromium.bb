// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell.h"

#include <thread>

#include "chrome/browser/android/vr_shell/ui_scene.h"
#include "chrome/browser/android/vr_shell/vr_compositor.h"
#include "chrome/browser/android/vr_shell/vr_gl_util.h"
#include "chrome/browser/android/vr_shell/vr_math.h"
#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"
#include "chrome/browser/android/vr_shell/vr_shell_renderer.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "jni/VrShell_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/init/gl_factory.h"

using base::android::JavaParamRef;

namespace {
// Constant taken from treasure_hunt demo.
static constexpr long kPredictionTimeWithoutVsyncNanos = 50000000;

static constexpr float kZNear = 0.1f;
static constexpr float kZFar = 1000.0f;

static constexpr gvr::Vec3f kDesktopPositionDefault = {0.0f, 0.0f, -2.0f};
static constexpr float kDesktopHeightDefault = 1.6f;

// Screen angle in degrees. 0 = vertical, positive = top closer.
static constexpr float kDesktopScreenTiltDefault = 0;

static constexpr float kScreenHeightRatio = 1.0f;
static constexpr float kScreenWidthRatio = 16.0f / 9.0f;

static constexpr float kReticleWidth = 0.05f;
static constexpr float kReticleHeight = 0.05f;

static constexpr float kLaserWidth = 0.01f;

// The neutral direction is fixed in world space, this is the
// reference angle pointing forward towards the horizon when the
// controller orientation is reset. This should match the yaw angle
// where the main screen is placed.
static constexpr gvr::Vec3f kNeutralPose = {0.0f, 0.0f, -1.0f};

// In lieu of an elbow model, we assume a position for the user's hand.
// TODO(mthiesse): Handedness options.
static constexpr gvr::Vec3f kHandPosition = {0.2f, -0.5f, -0.2f};

static constexpr float kReticleZOffset = 0.01f;

// UI element 0 is the browser content rectangle.
static constexpr int kBrowserUiElementId = 0;

}  // namespace

namespace vr_shell {

VrShell::VrShell(JNIEnv* env, jobject obj,
                 content::ContentViewCore* content_core,
                 ui::WindowAndroid* content_window)
    : desktop_screen_tilt_(kDesktopScreenTiltDefault),
      desktop_height_(kDesktopHeightDefault),
      desktop_position_(kDesktopPositionDefault),
      content_cvc_(content_core),
      delegate_(nullptr) {
  j_vr_shell_.Reset(env, obj);
  content_compositor_view_.reset(new VrCompositor(content_window));

  float screen_width = kScreenWidthRatio * desktop_height_;
  float screen_height = kScreenHeightRatio * desktop_height_;
  std::unique_ptr<ContentRectangle> rect(new ContentRectangle());
  rect->id = kBrowserUiElementId;
  rect->size = {screen_width, screen_height, 1.0f};
  scene_.AddUiElement(rect);

  desktop_plane_ = scene_.GetElementById(kBrowserUiElementId);

  content_cvc_->GetWebContents()->GetRenderWidgetHostView()
      ->GetRenderWidgetHost()->WasResized();
}

void VrShell::UpdateCompositorLayers(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  content_compositor_view_->SetLayer(content_cvc_);
}

void VrShell::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
  gl::init::ClearGLBindings();
}

bool RegisterVrShell(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

VrShell::~VrShell() {
}

void VrShell::SetDelegate(JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& delegate) {
  delegate_ = VrShellDelegate::getNativeDelegate(env, delegate);
}

void VrShell::GvrInit(JNIEnv* env,
                      const JavaParamRef<jobject>& obj,
                      jlong native_gvr_api) {
  gvr_api_ =
      gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(native_gvr_api));

  if (delegate_)
    delegate_->OnVrShellReady(this);
}

void VrShell::InitializeGl(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jint texture_data_handle) {
  CHECK(gl::GetGLImplementation() != gl::kGLImplementationNone ||
        gl::init::InitializeGLOneOff());

  content_texture_id_ = texture_data_handle;
  gvr_api_->InitializeGl();
  std::vector<gvr::BufferSpec> specs;
  specs.push_back(gvr_api_->CreateBufferSpec());
  render_size_ = specs[0].GetSize();
  swap_chain_.reset(new gvr::SwapChain(gvr_api_->CreateSwapchain(specs)));

  vr_shell_renderer_.reset(new VrShellRenderer());
  buffer_viewport_list_.reset(
      new gvr::BufferViewportList(gvr_api_->CreateEmptyBufferViewportList()));
  buffer_viewport_.reset(
      new gvr::BufferViewport(gvr_api_->CreateBufferViewport()));
}

void VrShell::UpdateController() {
  if (!controller_active_) {
    // No controller detected, set up a gaze cursor that tracks the
    // forward direction.
    //
    // Make a rotation quaternion that rotates forward_vector_ to (0, 0, -1).
    gvr::Quatf gaze_quat;
    gaze_quat.qw = 1 - forward_vector_.z;
    if (gaze_quat.qw < 1e-6) {
      // Degenerate case: vectors are exactly opposite. Replace by an
      // arbitrary 180 degree rotation to avoid invalid normalization.
      gaze_quat.qx = 1.0f;
      gaze_quat.qy = 0.0f;
      gaze_quat.qz = 0.0f;
      gaze_quat.qw = 0.0f;
    } else {
      gaze_quat.qx = forward_vector_.y;
      gaze_quat.qy = -forward_vector_.x;
      gaze_quat.qz = 0.0f;
      NormalizeQuat(gaze_quat);
    }
    controller_quat_ = gaze_quat;
  }

  gvr::Mat4f mat = QuatToMatrix(controller_quat_);
  gvr::Vec3f forward = MatrixVectorMul(mat, kNeutralPose);
  gvr::Vec3f translation = getTranslation(mat);

  // Use the eye midpoint as the origin for Cardboard mode, but apply an offset
  // for the controller.
  if (controller_active_) {
    translation.x += kHandPosition.x;
    translation.y += kHandPosition.y;
    translation.z += kHandPosition.z;
  }

  // We can only be intersecting the desktop plane at the moment.
  float distance_to_plane = desktop_plane_->GetRayDistance(translation,
                                                           forward);
  look_at_vector_ = GetRayPoint(translation, forward, distance_to_plane);
}

void ApplyNeckModel(gvr::Mat4f& mat_forward) {
  // This assumes that the input matrix is a pure rotation matrix. The
  // input object_from_reference matrix has the inverse rotation of
  // the head rotation. Invert it (this is just a transpose).
  gvr::Mat4f mat = MatrixTranspose(mat_forward);

  // Position of the point between the eyes, relative to the neck pivot:
  const float kNeckHorizontalOffset = -0.080f;  // meters in Z
  const float kNeckVerticalOffset = 0.075f;     // meters in Y

  std::array<float, 4> neckOffset = {
      {0.0f, kNeckVerticalOffset, kNeckHorizontalOffset, 1.0f}};

  // Rotate eyes around neck pivot point.
  auto offset = MatrixVectorMul(mat, neckOffset);

  // Measure new position relative to original center of head, because
  // applying a neck model should not elevate the camera.
  offset[1] -= kNeckVerticalOffset;

  // Right-multiply the inverse translation onto the
  // object_from_reference_matrix.
  TranslateMRight(mat_forward, mat_forward, -offset[0], -offset[1], -offset[2]);
}

void VrShell::DrawFrame(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  buffer_viewport_list_->SetToRecommendedBufferViewports();

  gvr::Frame frame = swap_chain_->AcquireFrame();
  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += kPredictionTimeWithoutVsyncNanos;
  head_pose_ = gvr_api_->GetHeadPoseInStartSpace(target_time);

  // Bind back to the default framebuffer.
  frame.BindBuffer(0);

  if (webvr_mode_) {
    DrawWebVr();
  } else {
    DrawVrShell(target_time.monotonic_system_time_nanos);
  }

  frame.Unbind();
  frame.Submit(*buffer_viewport_list_, head_pose_);
}

void VrShell::DrawVrShell(int64_t time) {
  float screen_tilt = desktop_screen_tilt_ * M_PI / 180.0f;

  forward_vector_ = getForwardVector(head_pose_);

  gvr::Vec3f headPos = getTranslation(head_pose_);
  if (headPos.x == 0.0f && headPos.y == 0.0f && headPos.z == 0.0f) {
    // This appears to be a 3DOF pose without a neck model. Add one.
    // The head pose has redundant data. Assume we're only using the
    // object_from_reference_matrix, we're not updating position_external.
    // TODO: Not sure what object_from_reference_matrix is. The new api removed
    // it. For now, removing it seems working fine.
    ApplyNeckModel(head_pose_);
  }

  desktop_plane_->translation.x = desktop_position_.x;
  desktop_plane_->translation.y = desktop_position_.y;
  desktop_plane_->translation.z = desktop_position_.z;

  // Update the render position of all UI elements (including desktop).
  scene_.UpdateTransforms(screen_tilt, time);

  UpdateController();

  // Everything should be positioned now, ready for drawing.
  gvr::Mat4f left_eye_view_matrix =
    MatrixMul(gvr_api_->GetEyeFromHeadMatrix(GVR_LEFT_EYE), head_pose_);
  gvr::Mat4f right_eye_view_matrix =
      MatrixMul(gvr_api_->GetEyeFromHeadMatrix(GVR_RIGHT_EYE), head_pose_);

  // Use culling to remove back faces.
  glEnable(GL_CULL_FACE);

  // Enable depth testing.
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_SCISSOR_TEST);

  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

  buffer_viewport_list_->GetBufferViewport(GVR_LEFT_EYE,
                                           buffer_viewport_.get());
  DrawEye(left_eye_view_matrix, *buffer_viewport_);
  buffer_viewport_list_->GetBufferViewport(GVR_RIGHT_EYE,
                                           buffer_viewport_.get());
  DrawEye(right_eye_view_matrix, *buffer_viewport_);
}

void VrShell::DrawEye(const gvr::Mat4f& view_matrix,
                      const gvr::BufferViewport& params) {
  gvr::Recti pixel_rect =
      CalculatePixelSpaceRect(render_size_, params.GetSourceUv());
  glViewport(pixel_rect.left, pixel_rect.bottom,
             pixel_rect.right - pixel_rect.left,
             pixel_rect.top - pixel_rect.bottom);
  glScissor(pixel_rect.left, pixel_rect.bottom,
            pixel_rect.right - pixel_rect.left,
            pixel_rect.top - pixel_rect.bottom);

  view_matrix_ = view_matrix;

  projection_matrix_ =
      PerspectiveMatrixFromView(params.GetSourceFov(), kZNear, kZFar);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // TODO(mthiesse): Draw order for transparency.
  DrawUI();
  DrawCursor();
}

void VrShell::DrawUI() {
  for (const auto& rect : scene_.GetUiElements()) {
    if (!rect->visible) {
      continue;
    }

    gvr::Mat4f combined_matrix = MatrixMul(view_matrix_,
                                           rect->transform.to_world);
    combined_matrix = MatrixMul(projection_matrix_, combined_matrix);

    Rectf copy_rect;
    jint texture_handle;
    if (rect->id == kBrowserUiElementId) {
      copy_rect = {0, 0, 1, 1};
      texture_handle = content_texture_id_;
    } else {
      // TODO(cjgrant): Populate UI texture and allow rendering.
      continue;
    }

    vr_shell_renderer_->GetTexturedQuadRenderer()->Draw(
        texture_handle, combined_matrix, copy_rect);
  }
}

void VrShell::DrawCursor() {
  gvr::Mat4f mat;
  SetIdentityM(mat);
  // Scale the pointer.
  ScaleM(mat, mat, kReticleWidth, kReticleHeight, 1.0f);
  // Place the pointer at the screen plane intersection point.
  TranslateM(mat, mat, look_at_vector_.x, look_at_vector_.y,
             look_at_vector_.z + kReticleZOffset);
  gvr::Mat4f mv = MatrixMul(view_matrix_, mat);
  gvr::Mat4f mvp = MatrixMul(projection_matrix_, mv);
  vr_shell_renderer_->GetReticleRenderer()->Draw(mvp);

  // Draw the laser only for controllers.
  if (!controller_active_) {
    return;
  }
  // Find the length of the beam (from hand to target).
  float xdiff = (kHandPosition.x - look_at_vector_.x);
  float ydiff = (kHandPosition.y - look_at_vector_.y);
  float zdiff = (kHandPosition.z - look_at_vector_.z);
  float scale = xdiff * xdiff + ydiff * ydiff + zdiff * zdiff;
  float laser_length = std::sqrt(scale);

  // Build a beam, originating from the origin.
  SetIdentityM(mat);

  // Move the beam half its height so that its end sits on the origin.
  TranslateM(mat, mat, 0, 0.5, 0);
  ScaleM(mat, mat, kLaserWidth, laser_length, 1);

  // Tip back 90 degrees to flat, pointing at the scene.
  auto q = QuatFromAxisAngle(1, 0, 0, -M_PI / 2);
  auto m = QuatToMatrix(q);
  mat = MatrixMul(m, mat);

  // Orient according to controller position.
  mat = MatrixMul(QuatToMatrix(controller_quat_), mat);

  // Move the beam origin to the hand.
  TranslateM(mat, mat, kHandPosition.x, kHandPosition.y, kHandPosition.z);

  mv = MatrixMul(view_matrix_, mat);
  mvp = MatrixMul(projection_matrix_, mv);
  vr_shell_renderer_->GetLaserRenderer()->Draw(mvp);
}

void VrShell::DrawWebVr() {
  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_POLYGON_OFFSET_FILL);

  // Don't need to clear, since we're drawing over the entire render target.

  glViewport(0, 0, render_size_.width, render_size_.height);
  vr_shell_renderer_->GetWebVrRenderer()->Draw(content_texture_id_);
}

void VrShell::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (gvr_api_ == nullptr)
    return;
  gvr_api_->PauseTracking();
}

void VrShell::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  if (gvr_api_ == nullptr)
    return;

  gvr_api_->RefreshViewerProfile();
  gvr_api_->ResumeTracking();
}

void VrShell::SetWebVrMode(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           bool enabled) {
  webvr_mode_ = enabled;
}

void VrShell::SubmitWebVRFrame() {
}

void VrShell::UpdateWebVRTextureBounds(
    int eye, float left, float top, float width, float height) {
  gvr::Rectf bounds = { left, top, width, height };
  vr_shell_renderer_->GetWebVrRenderer()->UpdateTextureBounds(eye, bounds);
}

gvr::GvrApi* VrShell::gvr_api() {
  return gvr_api_.get();
}

void VrShell::ContentSurfaceDestroyed(JNIEnv* env,
                                      const JavaParamRef<jobject>& object) {
  content_compositor_view_->SurfaceDestroyed();
}

void VrShell::ContentSurfaceChanged(JNIEnv* env,
                                    const JavaParamRef<jobject>& object,
                                    jint width,
                                    jint height,
                                    const JavaParamRef<jobject>& surface) {
  content_compositor_view_->SurfaceChanged((int)width, (int)height, surface);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& content_web_contents,
           jlong content_window_android) {
  content::ContentViewCore* c_core = content::ContentViewCore::FromWebContents(
      content::WebContents::FromJavaWebContents(content_web_contents));
  return reinterpret_cast<intptr_t>(new VrShell(
      env, obj, c_core,
      reinterpret_cast<ui::WindowAndroid*>(content_window_android)));
}

}  // namespace vr_shell
