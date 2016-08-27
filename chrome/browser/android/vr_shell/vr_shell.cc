// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell.h"

#include "chrome/browser/android/vr_shell/vr_shell_renderer.h"
#include "chrome/browser/android/vr_shell/vr_util.h"
#include "jni/VrShell_jni.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/init/gl_factory.h"

namespace vr_shell {

namespace {
// Constant taken from treasure_hunt demo.
const long kPredictionTimeWithoutVsyncNanos = 50000000;

const float kZNear = 0.1f;
const float kZFar = 1000.0f;

// Content rect in world coordinates. Height and width are currently supplied
// as DrawFrame arguments.
const gvr::Vec3f kContentRectPositionDefault = {0.0f, 0.0f, -1.0f};

}  // namespace

ContentRect::ContentRect() {
  SetIdentity();
}

ContentRect::~ContentRect() {}

void ContentRect::SetIdentity() {
  transfrom_to_world.m[0][0] = 1;
  transfrom_to_world.m[0][1] = 0;
  transfrom_to_world.m[0][2] = 0;
  transfrom_to_world.m[0][3] = 0;
  transfrom_to_world.m[1][0] = 0;
  transfrom_to_world.m[1][1] = 1;
  transfrom_to_world.m[1][2] = 0;
  transfrom_to_world.m[1][3] = 0;
  transfrom_to_world.m[2][0] = 0;
  transfrom_to_world.m[2][1] = 0;
  transfrom_to_world.m[2][2] = 1;
  transfrom_to_world.m[2][3] = 0;
  transfrom_to_world.m[3][0] = 0;
  transfrom_to_world.m[3][1] = 0;
  transfrom_to_world.m[3][2] = 0;
  transfrom_to_world.m[3][3] = 1;
}

void ContentRect::Translate(float x, float y, float z) {
  transfrom_to_world.m[0][3] += x;
  transfrom_to_world.m[1][3] += y;
  transfrom_to_world.m[2][3] += z;
}

VrShell::VrShell(JNIEnv* env, jobject obj) {
  j_vr_shell_.Reset(env, obj);
}

void VrShell::Destroy(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

bool RegisterVrShell(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

VrShell::~VrShell() {}

void VrShell::GvrInit(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jlong native_gvr_api) {
  gvr_api_ =
      gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(native_gvr_api));
}

void VrShell::InitializeGl(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jint texture_data_handle) {
  gl::init::InitializeGLOneOff();
  gvr_api_->InitializeGl();
  std::vector<gvr::BufferSpec> specs;
  specs.push_back(gvr_api_->CreateBufferSpec());
  render_size_ = specs[0].GetSize();
  swap_chain_.reset(new gvr::SwapChain(gvr_api_->CreateSwapchain(specs)));
  content_rect_.reset(new ContentRect());
  content_rect_->content_texture_handle =
      reinterpret_cast<int>(texture_data_handle);
  vr_shell_renderer_.reset(new VrShellRenderer());
  buffer_viewport_list_.reset(
      new gvr::BufferViewportList(gvr_api_->CreateEmptyBufferViewportList()));
  buffer_viewport_.reset(
      new gvr::BufferViewport(gvr_api_->CreateBufferViewport()));
}

void VrShell::DrawFrame(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj) {
  buffer_viewport_list_->SetToRecommendedBufferViewports();
  gvr::Frame frame = swap_chain_->AcquireFrame();
  gvr::ClockTimePoint target_time = gvr::GvrApi::GetTimePointNow();
  target_time.monotonic_system_time_nanos += kPredictionTimeWithoutVsyncNanos;
  head_pose_ = gvr_api_->GetHeadPoseInStartSpace(target_time);

  // Content area positioning.
  content_rect_->SetIdentity();
  content_rect_->Translate(kContentRectPositionDefault.x,
                           kContentRectPositionDefault.y,
                           kContentRectPositionDefault.z);

  gvr::Mat4f left_eye_view_matrix =
      MatrixMul(gvr_api_->GetEyeFromHeadMatrix(GVR_LEFT_EYE), head_pose_);
  gvr::Mat4f right_eye_view_matrix =
      MatrixMul(gvr_api_->GetEyeFromHeadMatrix(GVR_RIGHT_EYE), head_pose_);

  // Bind back to the default framebuffer.
  frame.BindBuffer(0);

  // Use culling to remove back faces.
  glEnable(GL_CULL_FACE);

  // Enable depth testing.
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_SCISSOR_TEST);

  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

  // Enable transparency.
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  buffer_viewport_list_->GetBufferViewport(GVR_LEFT_EYE,
                                           buffer_viewport_.get());
  DrawEye(left_eye_view_matrix, *buffer_viewport_);
  buffer_viewport_list_->GetBufferViewport(GVR_RIGHT_EYE,
                                           buffer_viewport_.get());
  DrawEye(right_eye_view_matrix, *buffer_viewport_);

  frame.Unbind();
  frame.Submit(*buffer_viewport_list_, head_pose_);
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
  DrawContentRect();
}

void VrShell::DrawContentRect() {
  gvr::Mat4f content_rect_combined_matrix =
      MatrixMul(view_matrix_, content_rect_->transfrom_to_world);
  content_rect_combined_matrix =
      MatrixMul(projection_matrix_, content_rect_combined_matrix);
  vr_shell_renderer_->GetTexturedQuadRenderer()->Draw(
      content_rect_->content_texture_handle, content_rect_combined_matrix);
}

void VrShell::OnPause(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj) {
  if (gvr_api_ == nullptr)
    return;
  gvr_api_->PauseTracking();
}

void VrShell::OnResume(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj) {
  if (gvr_api_ == nullptr)
    return;
  gvr_api_->RefreshViewerProfile();
  gvr_api_->ResumeTracking();
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
  VrShell* vrShell = new VrShell(env, obj);
  return reinterpret_cast<intptr_t>(vrShell);
}

}  // namespace vr_shell
