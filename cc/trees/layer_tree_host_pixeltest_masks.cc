// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_image_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/paint/drawing_display_item.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_recorder.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_pixel_resource_test.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/solid_color_content_layer_client.h"
#include "third_party/skia/include/core/SkImage.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

typedef ParameterizedPixelResourceTest LayerTreeHostMasksPixelTest;

INSTANTIATE_PIXEL_RESOURCE_TEST_CASE_P(LayerTreeHostMasksPixelTest);

class MaskContentLayerClient : public ContentLayerClient {
 public:
  explicit MaskContentLayerClient(const gfx::Size& bounds) : bounds_(bounds) {}
  ~MaskContentLayerClient() override {}

  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }

  gfx::Rect PaintableRegion() override { return gfx::Rect(bounds_); }

  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting picture_control) override {
    PaintRecorder recorder;
    PaintCanvas* canvas =
        recorder.beginRecording(gfx::RectToSkRect(PaintableRegion()));

    PaintFlags flags;
    flags.setStyle(PaintFlags::kStroke_Style);
    flags.setStrokeWidth(SkIntToScalar(2));
    flags.setColor(SK_ColorWHITE);

    canvas->clear(SK_ColorTRANSPARENT);
    gfx::Rect inset_rect(bounds_);
    while (!inset_rect.IsEmpty()) {
      inset_rect.Inset(3, 3, 2, 2);
      canvas->drawRect(
          SkRect::MakeXYWH(inset_rect.x(), inset_rect.y(), inset_rect.width(),
                           inset_rect.height()),
          flags);
      inset_rect.Inset(3, 3, 2, 2);
    }

    auto display_list = make_scoped_refptr(new DisplayItemList);
    display_list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
        PaintableRegion(), recorder.finishRecordingAsPicture(),
        gfx::RectToSkRect(PaintableRegion()));

    display_list->Finalize();
    return display_list;
  }

 private:
  gfx::Size bounds_;
};

TEST_P(LayerTreeHostMasksPixelTest, MaskOfLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(25, 25, 50, 50), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);

  gfx::Size mask_bounds(50, 50);
  MaskContentLayerClient client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  green->SetMaskLayer(mask.get());

  RunPixelResourceTest(background,
                       base::FilePath(FILE_PATH_LITERAL("mask_of_layer.png")));
}

TEST_P(LayerTreeHostMasksPixelTest, ImageMaskOfLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  gfx::Size mask_bounds(50, 50);

  scoped_refptr<PictureImageLayer> mask = PictureImageLayer::Create();
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  mask->SetBounds(mask_bounds);

  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(200, 200);
  SkCanvas* canvas = surface->getCanvas();
  canvas->scale(SkIntToScalar(4), SkIntToScalar(4));
  MaskContentLayerClient client(mask_bounds);
  scoped_refptr<DisplayItemList> mask_display_list =
      client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  mask_display_list->Raster(canvas);
  mask->SetImage(
      PaintImage(PaintImage::GetNextId(), surface->makeImageSnapshot()));

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(25, 25, 50, 50), kCSSGreen, 1, SK_ColorBLACK);
  green->SetMaskLayer(mask.get());
  background->AddChild(green);

  RunPixelResourceTest(
      background, base::FilePath(FILE_PATH_LITERAL("image_mask_of_layer.png")));
}

TEST_P(LayerTreeHostMasksPixelTest, MaskOfClippedLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  // Clip to the top half of the green layer.
  scoped_refptr<Layer> clip = Layer::Create();
  clip->SetPosition(gfx::PointF());
  clip->SetBounds(gfx::Size(100, 50));
  clip->SetMasksToBounds(true);
  background->AddChild(clip);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(25, 25, 50, 50), kCSSGreen, 1, SK_ColorBLACK);
  clip->AddChild(green);

  gfx::Size mask_bounds(50, 50);
  MaskContentLayerClient client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  green->SetMaskLayer(mask.get());

  RunPixelResourceTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("mask_of_clipped_layer.png")));
}

TEST_P(LayerTreeHostMasksPixelTest, MaskOfLargerLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(0, 0, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);

  gfx::Size mask_bounds(50, 50);
  MaskContentLayerClient client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  green->SetMaskLayer(mask.get());

  if (raster_buffer_provider_type_ == RASTER_BUFFER_PROVIDER_TYPE_BITMAP) {
    // Bitmap produces a sharper (but equivalent sized) mask.
    float percentage_pixels_large_error = 40.0f;
    float percentage_pixels_small_error = 0.0f;
    float average_error_allowed_in_bad_pixels = 65.0f;
    int large_error_allowed = 120;
    int small_error_allowed = 0;
    pixel_comparator_.reset(new FuzzyPixelComparator(
        true,  // discard_alpha
        percentage_pixels_large_error, percentage_pixels_small_error,
        average_error_allowed_in_bad_pixels, large_error_allowed,
        small_error_allowed));
  }

  RunPixelResourceTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("mask_of_larger_layer.png")));
}

TEST_P(LayerTreeHostMasksPixelTest, MaskOfLayerNonExactTextureSize) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(0, 0, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);

  gfx::Size mask_bounds(100, 100);
  MaskContentLayerClient client(mask_bounds);
  scoped_refptr<FakePictureLayer> mask = FakePictureLayer::Create(&client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  mask->set_fixed_tile_size(gfx::Size(173, 135));
  green->SetMaskLayer(mask.get());

  RunPixelResourceTest(background,
                       base::FilePath(FILE_PATH_LITERAL(
                           "mask_with_non_exact_texture_size.png")));
}

class CheckerContentLayerClient : public ContentLayerClient {
 public:
  CheckerContentLayerClient(const gfx::Size& bounds,
                           SkColor color,
                           bool vertical)
      : bounds_(bounds), color_(color), vertical_(vertical) {}
  ~CheckerContentLayerClient() override {}
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }
  gfx::Rect PaintableRegion() override { return gfx::Rect(bounds_); }
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting picture_control) override {
    PaintRecorder recorder;
    PaintCanvas* canvas =
        recorder.beginRecording(gfx::RectToSkRect(PaintableRegion()));

    PaintFlags flags;
    flags.setStyle(PaintFlags::kStroke_Style);
    flags.setStrokeWidth(SkIntToScalar(4));
    flags.setColor(color_);
    canvas->clear(SK_ColorTRANSPARENT);
    if (vertical_) {
      for (int i = 4; i < bounds_.width(); i += 16) {
        canvas->drawLine(i, 0, i, bounds_.height(), flags);
      }
    } else {
      for (int i = 4; i < bounds_.height(); i += 16) {
        canvas->drawLine(0, i, bounds_.width(), i, flags);
      }
    }

    auto display_list = make_scoped_refptr(new DisplayItemList);
    display_list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
        PaintableRegion(), recorder.finishRecordingAsPicture(),
        gfx::RectToSkRect(PaintableRegion()));

    display_list->Finalize();
    return display_list;
  }

 private:
  gfx::Size bounds_;
  SkColor color_;
  bool vertical_;
};

class CircleContentLayerClient : public ContentLayerClient {
 public:
  explicit CircleContentLayerClient(const gfx::Size& bounds)
      : bounds_(bounds) {}
  ~CircleContentLayerClient() override {}
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }
  gfx::Rect PaintableRegion() override { return gfx::Rect(bounds_); }
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting picture_control) override {
    PaintRecorder recorder;
    PaintCanvas* canvas =
        recorder.beginRecording(gfx::RectToSkRect(PaintableRegion()));

    PaintFlags flags;
    flags.setStyle(PaintFlags::kFill_Style);
    flags.setColor(SK_ColorWHITE);
    canvas->clear(SK_ColorTRANSPARENT);
    canvas->drawCircle(bounds_.width() / 2, bounds_.height() / 2,
                       bounds_.width() / 4, flags);

    auto display_list = make_scoped_refptr(new DisplayItemList);
    display_list->CreateAndAppendDrawingItem<DrawingDisplayItem>(
        PaintableRegion(), recorder.finishRecordingAsPicture(),
        gfx::RectToSkRect(PaintableRegion()));

    display_list->Finalize();
    return display_list;
  }

 private:
  gfx::Size bounds_;
};

using LayerTreeHostMasksForBackgroundFiltersPixelTest =
    ParameterizedPixelResourceTest;

INSTANTIATE_TEST_CASE_P(
    PixelResourceTest,
    LayerTreeHostMasksForBackgroundFiltersPixelTest,
    ::testing::Combine(
        ::testing::Values(SOFTWARE,
                          GL_GPU_RASTER_2D_DRAW,
                          GL_ONE_COPY_2D_STAGING_2D_DRAW,
                          GL_ONE_COPY_RECT_STAGING_2D_DRAW,
                          GL_ONE_COPY_EXTERNAL_STAGING_2D_DRAW,
                          GL_ZERO_COPY_2D_DRAW,
                          GL_ZERO_COPY_RECT_DRAW,
                          GL_ZERO_COPY_EXTERNAL_DRAW),
        ::testing::Values(Layer::LayerMaskType::SINGLE_TEXTURE_MASK,
                          Layer::LayerMaskType::MULTI_TEXTURE_MASK)));

TEST_P(LayerTreeHostMasksForBackgroundFiltersPixelTest,
       MaskOfLayerWithBackgroundFilter) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(100, 100), SK_ColorWHITE);

  gfx::Size picture_bounds(100, 100);
  CheckerContentLayerClient picture_client(picture_bounds, SK_ColorGREEN, true);
  scoped_refptr<PictureLayer> picture = PictureLayer::Create(&picture_client);
  picture->SetBounds(picture_bounds);
  picture->SetIsDrawable(true);

  scoped_refptr<SolidColorLayer> blur = CreateSolidColorLayer(
      gfx::Rect(100, 100), SK_ColorTRANSPARENT);
  background->AddChild(picture);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateGrayscaleFilter(1.0));
  blur->SetBackgroundFilters(filters);

  gfx::Size mask_bounds(100, 100);
  CircleContentLayerClient mask_client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&mask_client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  blur->SetMaskLayer(mask.get());

  float percentage_pixels_large_error = 2.5f;  // 2.5%, ~250px / (100*100)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 100.0f;
  int large_error_allowed = 256;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));

  RunPixelResourceTest(background,
                       base::FilePath(
                           FILE_PATH_LITERAL("mask_of_background_filter.png")));
}

TEST_P(LayerTreeHostMasksForBackgroundFiltersPixelTest,
       MaskOfLayerWithBlend) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(128, 128), SK_ColorWHITE);

  gfx::Size picture_bounds(128, 128);
  CheckerContentLayerClient picture_client_vertical(
      picture_bounds, SK_ColorGREEN, true);
  scoped_refptr<PictureLayer> picture_vertical =
      PictureLayer::Create(&picture_client_vertical);
  picture_vertical->SetBounds(picture_bounds);
  picture_vertical->SetIsDrawable(true);

  CheckerContentLayerClient picture_client_horizontal(
      picture_bounds, SK_ColorMAGENTA, false);
  scoped_refptr<PictureLayer> picture_horizontal =
      PictureLayer::Create(&picture_client_horizontal);
  picture_horizontal->SetBounds(picture_bounds);
  picture_horizontal->SetIsDrawable(true);
  picture_horizontal->SetContentsOpaque(false);
  picture_horizontal->SetBlendMode(SkBlendMode::kMultiply);

  background->AddChild(picture_vertical);
  background->AddChild(picture_horizontal);

  gfx::Size mask_bounds(128, 128);
  CircleContentLayerClient mask_client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&mask_client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  picture_horizontal->SetMaskLayer(mask.get());

  float percentage_pixels_large_error = 0.04f;  // 0.04%, ~6px / (128*128)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 256.0f;
  int large_error_allowed = 256;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));

  RunPixelResourceTest(background,
                       base::FilePath(
                           FILE_PATH_LITERAL("mask_of_layer_with_blend.png")));
}

}  // namespace
}  // namespace cc

#endif  // !defined(OS_ANDROID)
