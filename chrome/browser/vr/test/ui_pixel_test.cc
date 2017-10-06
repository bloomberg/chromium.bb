// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/ui_pixel_test.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/skia/include/core/SkImageEncoder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/test/gl_image_test_template.h"

namespace vr {

namespace {

// Resolution of Pixel Phone for one eye.
static const gfx::Size kPixelHalfScreen(960, 1080);

}  // namespace

UiPixelTest::UiPixelTest() : frame_buffer_size_(kPixelHalfScreen) {}

UiPixelTest::~UiPixelTest() = default;

void UiPixelTest::SetUp() {
  gl_test_environment_ =
      base::MakeUnique<GlTestEnvironment>(frame_buffer_size_);

  // Make content texture.
  content_texture_ = gl::GLTestHelper::CreateTexture(GL_TEXTURE_2D);
  // TODO(tiborg): Make GL_TEXTURE_EXTERNAL_OES texture for content and fill it
  // with fake content.
  ASSERT_EQ(glGetError(), (GLenum)GL_NO_ERROR);

  browser_ = base::MakeUnique<MockBrowserInterface>();
  content_input_delegate_ = base::MakeUnique<MockContentInputDelegate>();
}

void UiPixelTest::TearDown() {
  ui_.reset();
  glDeleteTextures(1, &content_texture_);
  gl_test_environment_.reset();
}

void UiPixelTest::MakeUi(const UiInitialState& ui_initial_state,
                         const ToolbarState& toolbar_state) {
  ui_ = base::MakeUnique<Ui>(browser_.get(), content_input_delegate_.get(),
                             ui_initial_state);
  ui_->OnGlInitialized(content_texture_,
                       vr::UiElementRenderer::kTextureLocationLocal);
  ui_->GetBrowserUiWeakPtr()->SetToolbarState(toolbar_state);
}

void UiPixelTest::DrawUi(const gfx::Vector3dF& laser_direction,
                         const gfx::Point3F& laser_origin,
                         UiInputManager::ButtonState button_state,
                         float controller_opacity,
                         const gfx::Transform& controller_transform,
                         const gfx::Transform& view_matrix,
                         const gfx::Transform& proj_matrix) {
  ControllerInfo controller_info;
  controller_info.transform = controller_transform;
  controller_info.opacity = controller_opacity;
  controller_info.laser_origin = laser_origin;
  controller_info.touchpad_button_state = button_state;
  controller_info.app_button_state = UiInputManager::ButtonState::UP;
  controller_info.home_button_state = UiInputManager::ButtonState::UP;
  RenderInfo render_info;
  render_info.head_pose = view_matrix;
  render_info.left_eye_info.view_matrix = view_matrix;
  render_info.left_eye_info.proj_matrix = proj_matrix;
  render_info.left_eye_info.view_proj_matrix = proj_matrix * view_matrix;
  render_info.right_eye_info = render_info.left_eye_info;
  render_info.surface_texture_size = frame_buffer_size_;
  render_info.left_eye_info.viewport = {0, 0, frame_buffer_size_.width(),
                                        frame_buffer_size_.height()};
  render_info.right_eye_info.viewport = {0, 0, 0, 0};

  GestureList gesture_list;
  ui_->scene()->OnBeginFrame(
      base::TimeTicks(),
      gfx::Vector3dF(-render_info.head_pose.matrix().get(2, 0),
                     -render_info.head_pose.matrix().get(2, 1),
                     -render_info.head_pose.matrix().get(2, 2)));
  ui_->input_manager()->HandleInput(
      gfx::Vector3dF(0.0f, 0.0f, -1.0f), controller_info.laser_origin,
      controller_info.touchpad_button_state, &gesture_list,
      &controller_info.target_point, &controller_info.reticle_render_target);

  ui_->ui_renderer()->Draw(render_info, controller_info);

  // We produce GL errors while rendering. Clear them all so that we can check
  // for errors of subsequent calls.
  // TODO(768905): Fix producing errors while rendering.
  while (glGetError() != GL_NO_ERROR) {
  }
}

std::unique_ptr<SkBitmap> UiPixelTest::SaveCurrentFrameBufferToSkBitmap() {
  // Create buffer.
  std::unique_ptr<SkBitmap> bitmap = base::MakeUnique<SkBitmap>();
  if (!bitmap->tryAllocN32Pixels(frame_buffer_size_.width(),
                                 frame_buffer_size_.height(), false)) {
    return nullptr;
  }
  SkPixmap pixmap;
  if (!bitmap->peekPixels(&pixmap)) {
    return nullptr;
  }

  // Read pixels from frame buffer.
  glReadPixels(0, 0, frame_buffer_size_.width(), frame_buffer_size_.height(),
               GL_RGBA, GL_UNSIGNED_BYTE, pixmap.writable_addr());
  if (glGetError() != GL_NO_ERROR) {
    return nullptr;
  }

  // Flip image vertically since SkBitmap expects the pixels in a different
  // order.
  for (int col = 0; col < frame_buffer_size_.width(); col++) {
    for (int row = 0; row < frame_buffer_size_.height() / 2; row++) {
      std::swap(
          *pixmap.writable_addr32(col, row),
          *pixmap.writable_addr32(col, frame_buffer_size_.height() - row - 1));
    }
  }

  return bitmap;
}

bool UiPixelTest::SaveSkBitmapToPng(const SkBitmap& bitmap,
                                    const std::string& filename) {
  SkFILEWStream stream(filename.c_str());
  if (!stream.isValid()) {
    return false;
  }
  if (!SkEncodeImage(&stream, bitmap, SkEncodedImageFormat::kPNG, 100)) {
    return false;
  }
  return true;
}

}  // namespace vr
