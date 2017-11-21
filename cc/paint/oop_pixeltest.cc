// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/region.h"
#include "cc/layers/recording_source.h"
#include "cc/paint/display_item_list.h"
#include "cc/raster/raster_source.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_implementation.h"

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

  struct RasterOptions {
    SkColor background_color = SK_ColorBLACK;
    int msaa_sample_count = 0;
    bool use_lcd_text = false;
    bool use_distance_field_text = false;
    GrPixelConfig pixel_config = kRGBA_8888_GrPixelConfig;
    gfx::Rect bitmap_rect;
    gfx::Rect playback_rect;
    float post_translate_x = 0.f;
    float post_translate_y = 0.f;
    float post_scale = 1.f;
  };

  SkBitmap Raster(scoped_refptr<DisplayItemList> display_item_list,
                  const gfx::Size& playback_size) {
    RasterOptions options;
    options.bitmap_rect = gfx::Rect(playback_size);
    options.playback_rect = gfx::Rect(playback_size);
    return Raster(display_item_list, options);
  }

  SkBitmap Raster(scoped_refptr<DisplayItemList> display_item_list,
                  const RasterOptions& options) {
    gpu::gles2::GLES2Interface* gl = context_->GetImplementation();
    GLuint texture_id;
    GLuint fbo_id;
    int width = options.bitmap_rect.width();
    int height = options.bitmap_rect.height();

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
    gl->BeginRasterCHROMIUM(texture_id, options.background_color,
                            options.msaa_sample_count, options.use_lcd_text,
                            options.use_distance_field_text,
                            options.pixel_config);
    gl->RasterCHROMIUM(display_item_list.get(), options.bitmap_rect.x(),
                       options.bitmap_rect.y(), options.playback_rect.x(),
                       options.playback_rect.y(), options.playback_rect.width(),
                       options.playback_rect.height(), options.post_translate_x,
                       options.post_translate_y, options.post_scale);
    gl->EndRasterCHROMIUM();
    gl->Flush();

    EXPECT_EQ(gl->GetError(), static_cast<unsigned>(GL_NO_ERROR));

    // Read the data back.
    std::unique_ptr<unsigned char[]> data(
        new unsigned char[width * height * 4]);
    gl->ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.get());

    gl->DeleteTextures(1, &texture_id);
    gl->DeleteFramebuffers(1, &fbo_id);

    // Swizzle rgba->bgra if needed.
    std::vector<SkPMColor> colors;
    colors.reserve(width * height);
    for (int h = 0; h < height; ++h) {
      for (int w = 0; w < width; ++w) {
        int i = (h * width + w) * 4;
        colors.push_back(SkPreMultiplyARGB(data[i + 3], data[i + 0],
                                           data[i + 1], data[i + 2]));
      }
    }

    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    SkPixmap pixmap(SkImageInfo::MakeN32Premul(options.bitmap_rect.width(),
                                               options.bitmap_rect.height()),
                    colors.data(),
                    options.bitmap_rect.width() * sizeof(SkColor));
    bitmap.writePixels(pixmap);
    return bitmap;
  }

  SkBitmap RasterExpectedBitmap(
      scoped_refptr<DisplayItemList> display_item_list,
      const gfx::Size& playback_size) {
    RasterOptions options;
    options.bitmap_rect = gfx::Rect(playback_size);
    options.playback_rect = gfx::Rect(playback_size);
    return RasterExpectedBitmap(display_item_list, options);
  }

  SkBitmap RasterExpectedBitmap(
      scoped_refptr<DisplayItemList> display_item_list,
      const RasterOptions& options) {
    // Generate bitmap via the "in process" raster path.  This verifies
    // that the preamble setup in RasterSource::PlaybackToCanvas matches
    // the same setup done in GLES2Implementation::RasterCHROMIUM.
    RecordingSource recording;
    recording.UpdateDisplayItemList(display_item_list, 0u);
    recording.SetBackgroundColor(options.background_color);
    Region fake_invalidation;
    gfx::Rect layer_rect(
        gfx::Size(options.bitmap_rect.right(), options.bitmap_rect.bottom()));
    recording.UpdateAndExpandInvalidation(&fake_invalidation, layer_rect.size(),
                                          layer_rect);

    auto raster_source = recording.CreateRasterSource();
    RasterSource::PlaybackSettings settings;
    settings.use_lcd_text = options.use_lcd_text;
    // OOP raster does not support the complicated debug color clearing from
    // RasterSource::ClearCanvasForPlayback, so disable it for consistency.
    settings.clear_canvas_before_raster = false;
    // TODO(enne): add a fake image provider here.

    uint32_t flags = options.use_distance_field_text
                         ? SkSurfaceProps::kUseDistanceFieldFonts_Flag
                         : 0;
    SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
    if (options.use_lcd_text) {
      surface_props =
          SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
    }
    gpu::Capabilities capabilities;
    gpu::gles2::GLES2Interface* gl = context_->GetImplementation();
    skia_bindings::GrContextForGLES2Interface scoped_grcontext(gl,
                                                               capabilities);
    SkImageInfo image_info = SkImageInfo::MakeN32Premul(
        options.bitmap_rect.width(), options.bitmap_rect.height());
    auto surface = SkSurface::MakeRenderTarget(scoped_grcontext.get(),
                                               SkBudgeted::kYes, image_info);
    SkCanvas* canvas = surface->getCanvas();
    canvas->drawColor(options.background_color);

    gfx::AxisTransform2d raster_transform(
        options.post_scale,
        gfx::Vector2dF(options.post_translate_x, options.post_translate_y));
    // TODO(enne): add a target colorspace to BeginRasterCHROMIUM etc.
    gfx::ColorSpace target_color_space;
    raster_source->PlaybackToCanvas(canvas, target_color_space,
                                    options.bitmap_rect, options.playback_rect,
                                    raster_transform, settings);
    surface->prepareForExternalIO();
    EXPECT_EQ(gl->GetError(), static_cast<unsigned>(GL_NO_ERROR));

    SkBitmap bitmap;
    SkImageInfo info = SkImageInfo::Make(
        options.bitmap_rect.width(), options.bitmap_rect.height(),
        SkColorType::kBGRA_8888_SkColorType, SkAlphaType::kPremul_SkAlphaType);
    bitmap.allocPixels(info, options.bitmap_rect.width() * 4);
    bool success = surface->readPixels(bitmap, 0, 0);
    CHECK(success);
    EXPECT_EQ(gl->GetError(), static_cast<unsigned>(GL_NO_ERROR));
    return bitmap;
  }

  void ExpectEquals(SkBitmap actual, SkBitmap expected) {
    EXPECT_EQ(actual.dimensions(), expected.dimensions());
    auto expected_url = GetPNGDataUrl(expected);
    auto actual_url = GetPNGDataUrl(actual);
    if (actual_url == expected_url)
      return;
    ADD_FAILURE() << "\nExpected: " << expected_url
                  << "\nActual:   " << actual_url;
  }

 private:
  viz::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestImageFactory image_factory_;
  std::unique_ptr<gpu::GLInProcessContext> context_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
};

TEST_F(OopPixelTest, DrawColor) {
  gfx::Rect rect(10, 10);
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawColorOp>(SK_ColorBLUE, SkBlendMode::kSrc);
  display_item_list->EndPaintOfUnpaired(rect);
  display_item_list->Finalize();

  auto actual = Raster(std::move(display_item_list), rect.size());
  std::vector<SkPMColor> expected_pixels(rect.width() * rect.height(),
                                         SkPreMultiplyARGB(255, 0, 0, 255));
  SkBitmap expected;
  expected.installPixels(
      SkImageInfo::MakeN32Premul(rect.width(), rect.height()),
      expected_pixels.data(), rect.width() * sizeof(SkColor));
  ExpectEquals(actual, expected);
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
  std::vector<SkPMColor> expected_pixels(rect.width() * rect.height());
  for (int h = 0; h < rect.height(); ++h) {
    auto start = expected_pixels.begin() + h * rect.width();
    SkPMColor left_color = SkPreMultiplyColor(
        h < 5 ? input[0].second.getColor() : input[2].second.getColor());
    SkPMColor right_color = SkPreMultiplyColor(
        h < 5 ? input[1].second.getColor() : input[3].second.getColor());

    std::fill(start, start + 5, left_color);
    std::fill(start + 5, start + 10, right_color);
  }
  SkBitmap expected;
  expected.installPixels(
      SkImageInfo::MakeN32Premul(rect.width(), rect.height()),
      expected_pixels.data(), rect.width() * sizeof(SkPMColor));
  ExpectEquals(actual, expected);
}

// Test various bitmap and playback rects in the raster options, to verify
// that in process (RasterSource) and out of process (GLES2Implementation)
// raster behave identically.
TEST_F(OopPixelTest, DrawRectBasicRasterOptions) {
  PaintFlags flags;
  flags.setColor(SkColorSetARGB(255, 250, 10, 20));
  gfx::Rect draw_rect(3, 1, 8, 9);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
  display_item_list->EndPaintOfUnpaired(draw_rect);
  display_item_list->Finalize();

  std::vector<std::pair<gfx::Rect, gfx::Rect>> input = {
      {{0, 0, 10, 10}, {0, 0, 10, 10}},
      {{1, 2, 10, 10}, {4, 2, 5, 6}},
      {{5, 5, 15, 10}, {0, 0, 10, 10}}};

  for (size_t i = 0; i < input.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Case %zd", i));

    RasterOptions options;
    options.bitmap_rect = input[i].first;
    options.playback_rect = input[i].second;
    options.background_color = SK_ColorMAGENTA;

    auto actual = Raster(display_item_list, options);
    auto expected = RasterExpectedBitmap(display_item_list, options);
    ExpectEquals(actual, expected);
  }
}

TEST_F(OopPixelTest, DrawRectScaleTransformOptions) {
  PaintFlags flags;
  // Use powers of two here to make floating point blending consistent.
  flags.setColor(SkColorSetARGB(128, 64, 128, 32));
  flags.setAntiAlias(true);
  gfx::Rect draw_rect(3, 4, 8, 9);

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
  display_item_list->EndPaintOfUnpaired(draw_rect);
  display_item_list->Finalize();

  // Draw a greenish transparent box, partially offset and clipped in the
  // bottom right.  It should appear near the upper left of a cyan background,
  // with the left and top sides of the greenish box partially blended due to
  // the post translate.
  RasterOptions options;
  options.bitmap_rect = {5, 5, 20, 20};
  options.playback_rect = {3, 2, 15, 12};
  options.background_color = SK_ColorCYAN;
  options.post_translate_x = 0.5f;
  options.post_translate_y = 0.25f;
  options.post_scale = 2.f;

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawRectQueryMiddleOfDisplayList) {
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  std::vector<SkColor> colors = {
      SkColorSetARGB(255, 0, 0, 255),    SkColorSetARGB(255, 0, 255, 0),
      SkColorSetARGB(255, 0, 255, 255),  SkColorSetARGB(255, 255, 0, 0),
      SkColorSetARGB(255, 255, 0, 255),  SkColorSetARGB(255, 255, 255, 0),
      SkColorSetARGB(255, 255, 255, 255)};

  for (int i = 0; i < 20; ++i) {
    gfx::Rect draw_rect(0, i, 1, 1);
    PaintFlags flags;
    flags.setColor(colors[i % colors.size()]);
    display_item_list->StartPaint();
    display_item_list->push<DrawRectOp>(gfx::RectToSkRect(draw_rect), flags);
    display_item_list->EndPaintOfUnpaired(draw_rect);
  }
  display_item_list->Finalize();

  // Draw a "tile" in the middle of the display list with a post scale.
  RasterOptions options;
  options.bitmap_rect = {0, 10, 1, 10};
  options.playback_rect = {0, 10, 1, 10};
  options.background_color = SK_ColorGRAY;
  options.post_translate_x = 0.f;
  options.post_translate_y = 0.f;
  options.post_scale = 2.f;

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

}  // namespace
}  // namespace cc
