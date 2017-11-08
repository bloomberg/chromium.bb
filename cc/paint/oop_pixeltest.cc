// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/paint/display_item_list.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

class OopPixelTest : public testing::Test {
 public:
  void SetUp() override {
    bool is_offscreen = true;
    gpu::gles2::ContextCreationAttribHelper attribs;
    attribs.alpha_size = -1;
    attribs.depth_size = 24;
    attribs.stencil_size = 8;
    attribs.samples = 0;
    attribs.sample_buffers = 0;
    attribs.fail_if_major_perf_caveat = false;
    attribs.bind_generates_resource = false;
    // Enable OOP rasterization.
    attribs.enable_oop_rasterization = true;

    // Add an OOP rasterization command line flag so that we set
    // |chromium_raster_transport| features flag.
    // TODO(vmpstr): Is there a better way to do this?
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableOOPRasterization)) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableOOPRasterization);
    }

    context_ = gpu::GLInProcessContext::CreateWithoutInit();
    auto result = context_->Initialize(
        nullptr, nullptr, is_offscreen, gpu::kNullSurfaceHandle, nullptr,
        attribs, gpu::SharedMemoryLimits(), &gpu_memory_buffer_manager_,
        &image_factory_, base::ThreadTaskRunnerHandle::Get());

    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
    ASSERT_TRUE(context_->GetCapabilities().supports_oop_raster);
  }

  void TearDown() override { context_.reset(); }

  std::vector<SkColor> Raster(scoped_refptr<DisplayItemList> display_item_list,
                              const gfx::Size& playback_size) {
    gpu::gles2::GLES2Interface* gl = context_->GetImplementation();
    GLuint texture_id;
    GLuint fbo_id;
    int width = playback_size.width();
    int height = playback_size.height();

    // Create and allocate a texture.
    gl->GenTextures(1, &texture_id);
    gl->BindTexture(GL_TEXTURE_2D, texture_id);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, nullptr);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // Create an fbo and bind the texture to it.
    gl->GenFramebuffers(1, &fbo_id);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, texture_id, 0);

    EXPECT_EQ(gl->GetError(), static_cast<unsigned>(GL_NO_ERROR));

    // "Out of process" raster! \o/
    gl->BeginRasterCHROMIUM(texture_id,
                            SK_ColorBLACK,  // background_color
                            0,              // msaa_sample_count
                            false,          // use_lcd_text
                            false,          // use_distance_field_text
                            kRGBA_8888_GrPixelConfig);
    gl->RasterCHROMIUM(display_item_list.get(),
                       0,       // translate_x
                       0,       // translate_y
                       0,       // clip_x
                       0,       // clip_y
                       width,   // clip_width
                       height,  // clip_height
                       0.f,     // post_translate_x
                       0.f,     // post_translate_y
                       1.f);    // post_scale
    gl->EndRasterCHROMIUM();
    gl->Flush();

    EXPECT_EQ(gl->GetError(), static_cast<unsigned>(GL_NO_ERROR));

    // Read the data back.
    std::unique_ptr<unsigned char[]> data(
        new unsigned char[width * height * 4]);
    gl->ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.get());

    gl->DeleteTextures(1, &texture_id);
    gl->DeleteFramebuffers(1, &fbo_id);

    std::vector<SkColor> colors;
    colors.reserve(width * height);
    for (int h = 0; h < height; ++h) {
      for (int w = 0; w < width; ++w) {
        int i = (h * width + w) * 4;
        colors.push_back(
            SkColorSetARGB(data[i + 3], data[i + 0], data[i + 1], data[i + 2]));
      }
    }
    return colors;
  }

  void ExpectEquals(const gfx::Size& size,
                    std::vector<SkColor>* actual,
                    std::vector<SkColor>* expected) {
    size_t expected_size = size.width() * size.height();
    ASSERT_EQ(expected->size(), expected_size);
    if (*actual == *expected)
      return;

    ASSERT_EQ(actual->size(), expected_size);

    SkBitmap expected_bitmap;
    expected_bitmap.installPixels(
        SkImageInfo::MakeN32Premul(size.width(), size.height()),
        expected->data(), size.width() * sizeof(SkColor));

    SkBitmap actual_bitmap;
    actual_bitmap.installPixels(
        SkImageInfo::MakeN32Premul(size.width(), size.height()), actual->data(),
        size.width() * sizeof(SkColor));

    ADD_FAILURE() << "\nExpected: " << GetPNGDataUrl(expected_bitmap)
                  << "\nActual:   " << GetPNGDataUrl(actual_bitmap);
  }

 private:
  viz::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestImageFactory image_factory_;
  std::unique_ptr<gpu::GLInProcessContext> context_;
};

TEST_F(OopPixelTest, DrawColor) {
  gfx::Rect rect(10, 10);
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SK_ColorBLUE, SkBlendMode::kSrc);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(std::move(display_item_list), rect.size());
  std::vector<SkColor> expected(rect.width() * rect.height(),
                                SkColorSetARGB(255, 0, 0, 255));
  ExpectEquals(rect.size(), &actual, &expected);
}

TEST_F(OopPixelTest, DrawRect) {
  gfx::Rect rect(10, 10);
  auto color_paint = [](int r, int g, int b) {
    PaintFlags flags;
    flags.setColor(SkColorSetARGB(255, r, g, b));
    return flags;
  };
  std::vector<std::pair<SkRect, PaintFlags>> input = {
      {SkRect::MakeXYWH(0, 0, 5, 5), color_paint(0, 0, 255)},
      {SkRect::MakeXYWH(5, 0, 5, 5), color_paint(0, 255, 0)},
      {SkRect::MakeXYWH(0, 5, 5, 5), color_paint(0, 255, 255)},
      {SkRect::MakeXYWH(5, 5, 5, 5), color_paint(255, 0, 0)}};

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  for (auto& op : input) {
    display_item_list->StartPaint();
    display_item_list->push<DrawRectOp>(op.first, op.second);
    display_item_list->EndPaintOfUnpaired(
        gfx::ToEnclosingRect(gfx::SkRectToRectF(op.first)));
  }
  display_item_list->Finalize();

  auto actual = Raster(std::move(display_item_list), rect.size());

  // Expected colors are 5x5 rects of
  //  BLUE GREEN
  //  CYAN  RED
  std::vector<SkColor> expected(rect.width() * rect.height());
  for (int h = 0; h < rect.height(); ++h) {
    auto start = expected.begin() + h * rect.width();
    SkColor left_color =
        h < 5 ? input[0].second.getColor() : input[2].second.getColor();
    SkColor right_color =
        h < 5 ? input[1].second.getColor() : input[3].second.getColor();

    std::fill(start, start + 5, left_color);
    std::fill(start + 5, start + 10, right_color);
  }
  ExpectEquals(rect.size(), &actual, &expected);
}

}  // namespace
}  // namespace cc
