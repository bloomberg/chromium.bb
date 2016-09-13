// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell.h"

#include <thread>

#include "chrome/browser/android/vr_shell/vr_shell_renderer.h"
#include "chrome/browser/android/vr_shell/vr_util.h"
#include "jni/VrShell_jni.h"
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

static constexpr float kScreenHeightMeters = 2.0f;
static constexpr float kScreenWidthMeters = 2.0f;
}  // namespace

namespace vr_shell {

VrShell::VrShell(JNIEnv* env, jobject obj)
    : desktop_screen_tilt_(kDesktopScreenTiltDefault),
      desktop_height_(kDesktopHeightDefault),
      desktop_position_(kDesktopPositionDefault) {
  j_vr_shell_.Reset(env, obj);
  ui_rects_.emplace_back(new ContentRectangle());
  desktop_plane_ = ui_rects_.back().get();
  desktop_plane_->id = 0;
  desktop_plane_->copy_rect = {0.0f, 0.0f, 1.0f, 1.0f};
  // TODO(cjgrant): If we use the native path for content clicks, fix this.
  desktop_plane_->window_rect = {0, 0, 0, 0};
  desktop_plane_->translation = {0.0f, 0.0f, 0.0f};
  desktop_plane_->x_anchoring = XNONE;
  desktop_plane_->y_anchoring = YNONE;
  desktop_plane_->anchor_z = false;
  desktop_plane_->orientation_axis_angle = {{1.0f, 0.0f, 0.0f, 0.0f}};
  desktop_plane_->rotation_axis_angle = {{0.0f, 0.0f, 0.0f, 0.0f}};
}

void VrShell::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
  gl::init::ClearGLBindings();
}

bool RegisterVrShell(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

VrShell::~VrShell() {
  device::GvrDelegateManager::GetInstance()->Shutdown();
}

void VrShell::GvrInit(JNIEnv* env,
                      const JavaParamRef<jobject>& obj,
                      jlong native_gvr_api) {
  gvr_api_ =
      gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(native_gvr_api));

  device::GvrDelegateManager::GetInstance()->Initialize(this);
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

  desktop_plane_->content_texture_handle = content_texture_id_;

  vr_shell_renderer_.reset(new VrShellRenderer());
  buffer_viewport_list_.reset(
      new gvr::BufferViewportList(gvr_api_->CreateEmptyBufferViewportList()));
  buffer_viewport_.reset(
      new gvr::BufferViewport(gvr_api_->CreateBufferViewport()));
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
    DrawVrShell();
  }

  frame.Unbind();
  frame.Submit(*buffer_viewport_list_, head_pose_);
}

void VrShell::DrawVrShell() {
  float screen_width = kScreenWidthMeters * desktop_height_;
  float screen_height = kScreenHeightMeters * desktop_height_;

  float screen_tilt = desktop_screen_tilt_ * M_PI / 180.0f;

  gvr::Vec3f headPos = getTranslation(head_pose_);
  if (headPos.x == 0.0f && headPos.y == 0.0f && headPos.z == 0.0f) {
    // This appears to be a 3DOF pose without a neck model. Add one.
    // The head pose has redundant data. Assume we're only using the
    // object_from_reference_matrix, we're not updating position_external.
    // TODO: Not sure what object_from_reference_matrix is. The new api removed
    // it. For now, removing it seems working fine.
    ApplyNeckModel(head_pose_);
  }

  desktop_plane_->size = {screen_width, screen_height, 1.0f};
  desktop_plane_->translation.x = desktop_position_.x;
  desktop_plane_->translation.y = desktop_position_.y;
  desktop_plane_->translation.z = desktop_position_.z;

  // Update position of all UI elements (including desktop)
  UpdateTransforms(screen_width, screen_height, screen_tilt);

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
  DrawUI();
}

void VrShell::DrawUI() {
  for (std::size_t i = 0; i < ui_rects_.size(); ++i) {
    gvr::Mat4f combined_matrix =
        MatrixMul(view_matrix_, ui_rects_[i].get()->transform.to_world);
    combined_matrix = MatrixMul(projection_matrix_, combined_matrix);
    vr_shell_renderer_->GetTexturedQuadRenderer()->Draw(
        ui_rects_[i].get()->content_texture_handle, combined_matrix,
        ui_rects_[i].get()->copy_rect);
  }
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
  vr_shell_renderer_->GetWebVrRenderer()->Draw(
      reinterpret_cast<int>(desktop_plane_->content_texture_handle));
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

void VrShell::RequestWebVRPresent() {
  webvr_mode_ = true;
}

void VrShell::ExitWebVRPresent() {
  webvr_mode_ = false;
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

void VrShell::UpdateTransforms(float screen_width_meters,
                               float screen_height_meters,
                               float screen_tilt) {
  for (std::unique_ptr<ContentRectangle>& rect : ui_rects_) {
    rect->transform.MakeIdentity();
    rect->transform.Scale(rect->size.x, rect->size.y, rect->size.z);
    float x_anchor_translate;
    switch (rect->x_anchoring) {
      case XLEFT:
        x_anchor_translate = desktop_position_.x - screen_width_meters * 0.5;
        break;
      case XRIGHT:
        x_anchor_translate = desktop_position_.x + screen_width_meters * 0.5;
        break;
      case XCENTER:
        x_anchor_translate = desktop_position_.x;
        break;
      case XNONE:
        x_anchor_translate = 0;
        break;
    }
    float y_anchor_translate;
    switch (rect->y_anchoring) {
      case YTOP:
        y_anchor_translate = desktop_position_.y + screen_height_meters * 0.5;
        break;
      case YBOTTOM:
        y_anchor_translate = desktop_position_.y - screen_height_meters * 0.5;
        break;
      case YCENTER:
        y_anchor_translate = desktop_position_.y;
        break;
      case YNONE:
        y_anchor_translate = 0;
        break;
    }
    float z_anchor_translate = rect->anchor_z ? desktop_position_.z : 0;
    rect->transform.Translate(x_anchor_translate + rect->translation.x,
                              y_anchor_translate + rect->translation.y,
                              z_anchor_translate + rect->translation.z);
    // TODO(cjgrant): Establish which exact rotations we'll provide.
    // Adjust for screen tilt.
    rect->transform.Rotate(1.0f, 0.0f, 0.0f, screen_tilt);
  }
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return reinterpret_cast<intptr_t>(new VrShell(env, obj));
}

}  // namespace vr_shell
