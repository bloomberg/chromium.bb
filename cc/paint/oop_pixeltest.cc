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
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
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

scoped_refptr<DisplayItemList> MakeNoopDisplayItemList() {
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  display_item_list->push<SaveOp>();
  display_item_list->push<RestoreOp>();
  display_item_list->EndPaintOfUnpaired(gfx::Rect(10000, 10000));
  display_item_list->Finalize();
  return display_item_list;
}

class NoOpImageProvider : public ImageProvider {
 public:
  ~NoOpImageProvider() override = default;

  ScopedDecodedDrawImage GetDecodedDrawImage(
      const DrawImage& draw_image) override {
    SkBitmap bitmap;
    bitmap.allocPixelsFlags(SkImageInfo::MakeN32Premul(10, 10),
                            SkBitmap::kZeroPixels_AllocFlag);
    sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);
    return ScopedDecodedDrawImage(DecodedDrawImage(image, SkSize::Make(10, 10),
                                                   SkSize::Make(1, 1),
                                                   kLow_SkFilterQuality, true));
  }
};

class OopPixelTest : public testing::Test {
 public:
  void SetUp() override {
    // Add an OOP rasterization command line flag so that we set
    // |chromium_raster_transport| features flag.
    // TODO(vmpstr): Is there a better way to do this?
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableOOPRasterization)) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableOOPRasterization);
    }

    // Setup a GL context for reading back pixels.
    bool is_offscreen = true;
    gpu::ContextCreationAttribs attribs;
    attribs.alpha_size = -1;
    attribs.depth_size = 24;
    attribs.stencil_size = 8;
    attribs.samples = 0;
    attribs.sample_buffers = 0;
    attribs.fail_if_major_perf_caveat = false;
    attribs.bind_generates_resource = false;

    context_ = gpu::GLInProcessContext::CreateWithoutInit();
    auto result = context_->Initialize(
        nullptr, nullptr, is_offscreen, gpu::kNullSurfaceHandle, nullptr,
        attribs, gpu::SharedMemoryLimits(), &gpu_memory_buffer_manager_,
        &image_factory_, nullptr, base::ThreadTaskRunnerHandle::Get());

    ASSERT_EQ(result, gpu::ContextResult::kSuccess);

    // Setup a second context with OOP rasterization enabled and a
    // RasterInterface on top of it.
    attribs.enable_oop_rasterization = true;

    raster_context_ = gpu::GLInProcessContext::CreateWithoutInit();
    result = raster_context_->Initialize(
        nullptr, nullptr, is_offscreen, gpu::kNullSurfaceHandle, nullptr,
        attribs, gpu::SharedMemoryLimits(), &gpu_memory_buffer_manager_,
        &image_factory_, nullptr, base::ThreadTaskRunnerHandle::Get());
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
    ASSERT_TRUE(raster_context_->GetCapabilities().supports_oop_raster);
    raster_implementation_ =
        std::make_unique<gpu::raster::RasterImplementationGLES>(
            raster_context_->GetImplementation(),
            raster_context_->GetImplementation(),
            raster_context_->GetCapabilities());
  }

  void TearDown() override {
    raster_implementation_.reset();
    raster_context_.reset();
    context_.reset();
  }

  struct RasterOptions {
    SkColor background_color = SK_ColorBLACK;
    int msaa_sample_count = 0;
    bool use_lcd_text = false;
    bool use_distance_field_text = false;
    SkColorType color_type = kRGBA_8888_SkColorType;
    gfx::Size resource_size;
    gfx::Size content_size;
    gfx::Rect full_raster_rect;
    gfx::Rect playback_rect;
    gfx::Vector2dF post_translate = {0.f, 0.f};
    float post_scale = 1.f;
    gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
    bool requires_clear = false;
    bool preclear = false;
    SkColor preclear_color;
  };

  SkBitmap Raster(scoped_refptr<DisplayItemList> display_item_list,
                  const gfx::Size& playback_size) {
    RasterOptions options;
    options.resource_size = playback_size;
    options.content_size = options.resource_size;
    options.full_raster_rect = gfx::Rect(playback_size);
    options.playback_rect = gfx::Rect(playback_size);
    return Raster(display_item_list, options);
  }

  SkBitmap Raster(scoped_refptr<DisplayItemList> display_item_list,
                  const RasterOptions& options) {
    gpu::gles2::GLES2Interface* gl = context_->GetImplementation();
    int width = options.resource_size.width();
    int height = options.resource_size.height();

    // Create and allocate a texture on the raster interface.
    GLuint raster_texture_id;
    raster_implementation_->GenTextures(1, &raster_texture_id);
    raster_implementation_->BindTexture(GL_TEXTURE_2D, raster_texture_id);
    raster_implementation_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                                       0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    raster_implementation_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                          GL_LINEAR);

    EXPECT_EQ(raster_implementation_->GetError(),
              static_cast<unsigned>(GL_NO_ERROR));

    RasterColorSpace color_space(options.color_space, ++color_space_id_);

    if (options.preclear) {
      raster_implementation_->BeginRasterCHROMIUM(
          raster_texture_id, options.preclear_color, options.msaa_sample_count,
          options.use_lcd_text, options.use_distance_field_text,
          options.color_type, color_space);
      raster_implementation_->EndRasterCHROMIUM();
    }

    // "Out of process" raster! \o/

    raster_implementation_->BeginRasterCHROMIUM(
        raster_texture_id, options.background_color, options.msaa_sample_count,
        options.use_lcd_text, options.use_distance_field_text,
        options.color_type, color_space);
    raster_implementation_->RasterCHROMIUM(
        display_item_list.get(), &image_provider_, options.content_size,
        options.full_raster_rect, options.playback_rect, options.post_translate,
        options.post_scale, options.requires_clear);
    raster_implementation_->EndRasterCHROMIUM();

    // Produce a mailbox and insert an ordering barrier (assumes the raster
    // interface and gl are on the same scheduling group).
    gpu::Mailbox mailbox;
    raster_implementation_->GenMailboxCHROMIUM(mailbox.name);
    raster_implementation_->ProduceTextureDirectCHROMIUM(raster_texture_id,
                                                         mailbox.name);
    raster_implementation_->OrderingBarrierCHROMIUM();

    EXPECT_EQ(raster_implementation_->GetError(),
              static_cast<unsigned>(GL_NO_ERROR));

    // Import the texture in gl, create an fbo and bind the texture to it.
    GLuint gl_texture_id = gl->CreateAndConsumeTextureCHROMIUM(mailbox.name);
    GLuint fbo_id;
    gl->GenFramebuffers(1, &fbo_id);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, gl_texture_id, 0);

    // Read the data back.
    std::unique_ptr<unsigned char[]> data(
        new unsigned char[width * height * 4]);
    gl->ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.get());

    gl->DeleteTextures(1, &gl_texture_id);
    gl->DeleteFramebuffers(1, &fbo_id);

    gl->OrderingBarrierCHROMIUM();
    raster_implementation_->DeleteTextures(1, &raster_texture_id);

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
    SkPixmap pixmap(SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                               options.resource_size.height()),
                    colors.data(),
                    options.resource_size.width() * sizeof(SkColor));
    bitmap.writePixels(pixmap);
    return bitmap;
  }

  SkBitmap RasterExpectedBitmap(
      scoped_refptr<DisplayItemList> display_item_list,
      const gfx::Size& playback_size) {
    RasterOptions options;
    options.resource_size = playback_size;
    options.full_raster_rect = gfx::Rect(playback_size);
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
    recording.UpdateDisplayItemList(display_item_list, 0u, 1.f);
    recording.SetBackgroundColor(options.background_color);
    Region fake_invalidation;
    gfx::Rect layer_rect(gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom()));
    recording.UpdateAndExpandInvalidation(&fake_invalidation, layer_rect.size(),
                                          layer_rect);
    recording.SetRequiresClear(options.requires_clear);

    auto raster_source = recording.CreateRasterSource();
    RasterSource::PlaybackSettings settings;
    settings.use_lcd_text = options.use_lcd_text;
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
    size_t max_resource_cache_bytes;
    size_t max_glyph_cache_texture_bytes;
    skia_bindings::GrContextForGLES2Interface::DefaultCacheLimitsForTests(
        &max_resource_cache_bytes, &max_glyph_cache_texture_bytes);
    skia_bindings::GrContextForGLES2Interface scoped_grcontext(
        gl, capabilities, max_resource_cache_bytes,
        max_glyph_cache_texture_bytes);
    SkImageInfo image_info = SkImageInfo::MakeN32Premul(
        options.resource_size.width(), options.resource_size.height());
    auto surface = SkSurface::MakeRenderTarget(scoped_grcontext.get(),
                                               SkBudgeted::kYes, image_info);
    SkCanvas* canvas = surface->getCanvas();
    if (options.preclear)
      canvas->drawColor(options.preclear_color);
    else
      canvas->drawColor(options.background_color);

    gfx::AxisTransform2d raster_transform(options.post_scale,
                                          options.post_translate);
    gfx::ColorSpace target_color_space;
    raster_source->PlaybackToCanvas(
        canvas, options.color_space, options.content_size,
        options.full_raster_rect, options.playback_rect, raster_transform,
        settings);
    surface->prepareForExternalIO();
    EXPECT_EQ(gl->GetError(), static_cast<unsigned>(GL_NO_ERROR));

    SkBitmap bitmap;
    SkImageInfo info = SkImageInfo::Make(
        options.resource_size.width(), options.resource_size.height(),
        SkColorType::kBGRA_8888_SkColorType, SkAlphaType::kPremul_SkAlphaType);
    bitmap.allocPixels(info, options.resource_size.width() * 4);
    bool success = surface->readPixels(bitmap, 0, 0);
    CHECK(success);
    EXPECT_EQ(gl->GetError(), static_cast<unsigned>(GL_NO_ERROR));
    return bitmap;
  }

  void ExpectEquals(SkBitmap actual,
                    SkBitmap expected,
                    const char* label = nullptr) {
    EXPECT_EQ(actual.dimensions(), expected.dimensions());
    auto expected_url = GetPNGDataUrl(expected);
    auto actual_url = GetPNGDataUrl(actual);
    if (actual_url == expected_url)
      return;
    if (label) {
      ADD_FAILURE() << "\nCase: " << label << "\nExpected: " << expected_url
                    << "\nActual:   " << actual_url;
    } else {
      ADD_FAILURE() << "\nExpected: " << expected_url
                    << "\nActual:   " << actual_url;
    }
  }

 private:
  viz::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestImageFactory image_factory_;
  std::unique_ptr<gpu::GLInProcessContext> context_;
  std::unique_ptr<gpu::GLInProcessContext> raster_context_;
  std::unique_ptr<gpu::raster::RasterImplementationGLES> raster_implementation_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
  NoOpImageProvider image_provider_;
  int color_space_id_ = 0;
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

TEST_F(OopPixelTest, Preclear) {
  gfx::Rect rect(10, 10);
  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->Finalize();

  RasterOptions options;
  options.resource_size = rect.size();
  options.full_raster_rect = rect;
  options.playback_rect = rect;
  options.background_color = SK_ColorMAGENTA;
  options.preclear = true;
  options.preclear_color = SK_ColorGREEN;

  auto actual = Raster(display_item_list, options);

  options.preclear = false;
  options.background_color = SK_ColorGREEN;
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, ClearingOpaqueCorner) {
  // Verify that clears work properly for both the right and bottom sides
  // of an opaque corner tile.

  RasterOptions options;
  gfx::Point arbitrary_offset(10, 20);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorGREEN;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect a two pixel border from texels 7-9 on the column and 6-8 on row.
  SkCanvas canvas(bitmap);
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  canvas.drawRect(SkRect::MakeXYWH(7, 0, 2, 8), green);
  canvas.drawRect(SkRect::MakeXYWH(0, 6, 9, 2), green);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingOpaqueCornerExactEdge) {
  // Verify that clears work properly for both the right and bottom sides
  // of an opaque corner tile whose content rect exactly lines up with
  // the edge of the resource.

  RasterOptions options;
  gfx::Point arbitrary_offset(10, 20);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, options.resource_size);
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorGREEN;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect a one pixel border on the bottom/right edge.
  SkCanvas canvas(bitmap);
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  canvas.drawRect(SkRect::MakeXYWH(9, 0, 1, 10), green);
  canvas.drawRect(SkRect::MakeXYWH(0, 9, 10, 1), green);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingOpaqueCornerPartialRaster) {
  // Verify that clears do nothing on an opaque corner tile whose
  // partial raster rect doesn't intersect the edge of the content.

  RasterOptions options;
  options.resource_size = gfx::Size(10, 10);
  gfx::Point arbitrary_offset(30, 12);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect =
      gfx::Rect(arbitrary_offset.x() + 5, arbitrary_offset.y() + 3, 2, 4);
  options.background_color = SK_ColorGREEN;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect no clearing here because the playback rect is internal.
  SkCanvas canvas(bitmap);
  canvas.drawColor(options.preclear_color);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingOpaqueRightEdge) {
  // Verify that a tile that intersects the right edge of content
  // but not the bottom only clears the right pixels.

  RasterOptions options;
  gfx::Point arbitrary_offset(30, 40);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(3, 10));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom() + 1000);
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorGREEN;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect a two pixel column border from texels 2-4.
  SkCanvas canvas(bitmap);
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  canvas.drawRect(SkRect::MakeXYWH(2, 0, 2, 10), green);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingOpaqueBottomEdge) {
  // Verify that a tile that intersects the bottom edge of content
  // but not the right only clears the bottom pixels.

  RasterOptions options;
  gfx::Point arbitrary_offset(10, 20);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(10, 5));
  options.content_size = gfx::Size(options.full_raster_rect.right() + 1000,
                                   options.full_raster_rect.bottom());
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorGREEN;
  float arbitrary_scale = 0.25f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect a two pixel border from texels 4-6 on the row
  SkCanvas canvas(bitmap);
  canvas.drawColor(options.preclear_color);
  SkPaint green;
  green.setColor(options.background_color);
  canvas.drawRect(SkRect::MakeXYWH(0, 4, 10, 2), green);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingOpaqueInternal) {
  // Verify that an internal opaque tile does no clearing.

  RasterOptions options;
  gfx::Point arbitrary_offset(35, 12);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, options.resource_size);
  // Very large content rect to make this an internal tile.
  options.content_size = gfx::Size(1000, 1000);
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorGREEN;
  float arbitrary_scale = 1.2345f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = false;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Expect no clears here, as this tile does not intersect the edge of the
  // tile.
  SkCanvas canvas(bitmap);
  canvas.drawColor(options.preclear_color);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingTransparentCorner) {
  RasterOptions options;
  gfx::Point arbitrary_offset(5, 8);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorTRANSPARENT;
  float arbitrary_scale = 3.7f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = true;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  // Because this is rastering the entire tile, clear the entire thing
  // even if the full raster rect doesn't cover the whole resource.
  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap);
  canvas.drawColor(SK_ColorTRANSPARENT);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingTransparentInternalTile) {
  // Content rect much larger than full raster rect or playback rect.
  // This should still clear the tile.
  RasterOptions options;
  gfx::Point arbitrary_offset(100, 200);
  options.resource_size = gfx::Size(10, 10);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, options.resource_size);
  options.content_size = gfx::Size(1000, 1000);
  options.playback_rect = options.full_raster_rect;
  options.background_color = SK_ColorTRANSPARENT;
  float arbitrary_scale = 3.7f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = true;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  // Because this is rastering the entire tile, clear the entire thing
  // even if the full raster rect doesn't cover the whole resource.
  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  SkCanvas canvas(bitmap);
  canvas.drawColor(SK_ColorTRANSPARENT);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
}

TEST_F(OopPixelTest, ClearingTransparentCornerPartialRaster) {
  RasterOptions options;
  options.resource_size = gfx::Size(10, 10);
  gfx::Point arbitrary_offset(30, 12);
  options.full_raster_rect = gfx::Rect(arbitrary_offset, gfx::Size(8, 7));
  options.content_size = gfx::Size(options.full_raster_rect.right(),
                                   options.full_raster_rect.bottom());
  options.playback_rect =
      gfx::Rect(arbitrary_offset.x() + 5, arbitrary_offset.y() + 3, 2, 4);
  options.background_color = SK_ColorTRANSPARENT;
  float arbitrary_scale = 0.23f;
  options.post_scale = arbitrary_scale;
  options.requires_clear = true;
  options.preclear = true;
  options.preclear_color = SK_ColorRED;

  // Make a non-empty but noop display list to avoid early outs.
  auto display_item_list = MakeNoopDisplayItemList();

  auto oop_result = Raster(display_item_list, options);
  auto gpu_result = RasterExpectedBitmap(display_item_list, options);

  SkBitmap bitmap;
  bitmap.allocPixelsFlags(
      SkImageInfo::MakeN32Premul(options.resource_size.width(),
                                 options.resource_size.height()),
      SkBitmap::kZeroPixels_AllocFlag);

  // Result should be a red background with a cleared hole where the
  // playback_rect is.
  SkCanvas canvas(bitmap);
  canvas.drawColor(options.preclear_color);
  canvas.translate(-arbitrary_offset.x(), -arbitrary_offset.y());
  canvas.clipRect(gfx::RectToSkRect(options.playback_rect));
  canvas.drawColor(SK_ColorTRANSPARENT, SkBlendMode::kSrc);

  ExpectEquals(oop_result, bitmap, "oop");
  ExpectEquals(gpu_result, bitmap, "gpu");
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
      {{5, 5, 15, 10}, {5, 5, 10, 10}}};

  for (size_t i = 0; i < input.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Case %zd", i));

    RasterOptions options;
    options.resource_size = input[i].first.size(),
    options.full_raster_rect = input[i].first;
    options.content_size = gfx::Size(options.full_raster_rect.right(),
                                     options.full_raster_rect.bottom());
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
  options.resource_size = {20, 20};
  options.content_size = {25, 25};
  options.full_raster_rect = {5, 5, 20, 20};
  options.playback_rect = {5, 5, 13, 9};
  options.background_color = SK_ColorCYAN;
  options.post_translate = {0.5f, 0.25f};
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
  options.resource_size = {10, 10};
  options.content_size = {20, 20};
  options.full_raster_rect = {0, 10, 1, 10};
  options.playback_rect = {0, 10, 1, 10};
  options.background_color = SK_ColorGRAY;
  options.post_translate = {0.f, 0.f};
  options.post_scale = 2.f;

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

TEST_F(OopPixelTest, DrawRectColorSpace) {
  RasterOptions options;
  options.resource_size = gfx::Size(100, 100);
  options.content_size = options.resource_size;
  options.full_raster_rect = gfx::Rect(options.content_size);
  options.playback_rect = options.full_raster_rect;
  options.color_space = gfx::ColorSpace::CreateDisplayP3D65();

  auto display_item_list = base::MakeRefCounted<DisplayItemList>();
  display_item_list->StartPaint();
  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(SK_ColorGREEN);
  display_item_list->push<DrawRectOp>(
      gfx::RectToSkRect(gfx::Rect(options.resource_size)), flags);
  display_item_list->EndPaintOfUnpaired(options.full_raster_rect);
  display_item_list->Finalize();

  auto actual = Raster(display_item_list, options);
  auto expected = RasterExpectedBitmap(display_item_list, options);
  ExpectEquals(actual, expected);
}

}  // namespace
}  // namespace cc
