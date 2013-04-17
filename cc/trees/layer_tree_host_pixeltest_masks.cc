// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/image_layer.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/pixel_comparator.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostMasksPixelTest : public LayerTreePixelTest {};

class MaskContentLayerClient : public cc::ContentLayerClient {
 public:
  MaskContentLayerClient() {}
  virtual ~MaskContentLayerClient() {}

  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}

  virtual void PaintContents(SkCanvas* canvas,
                             gfx::Rect rect,
                             gfx::RectF* opaque_rect) OVERRIDE {
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(SkIntToScalar(2));
    paint.setColor(SK_ColorWHITE);

    canvas->clear(SK_ColorTRANSPARENT);
    while (!rect.IsEmpty()) {
      rect.Inset(3, 3, 2, 2);
      canvas->drawRect(
          SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
          paint);
      rect.Inset(3, 3, 2, 2);
    }
  }
};

TEST_F(LayerTreeHostMasksPixelTest, MaskOfLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(50, 50, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);
  green->SetMaskLayer(mask);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "mask_of_layer.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, ImageMaskOfLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<ImageLayer> mask = ImageLayer::Create();
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);
  mask->SetBounds(gfx::Size(100, 100));

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 400, 400);
  bitmap.allocPixels();
  SkCanvas canvas(bitmap);
  canvas.scale(SkIntToScalar(4), SkIntToScalar(4));
  MaskContentLayerClient client;
  client.PaintContents(&canvas,
                       gfx::Rect(100, 100),
                       NULL);
  mask->SetBitmap(bitmap);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(50, 50, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  green->SetMaskLayer(mask);
  background->AddChild(green);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "image_mask_of_layer.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, MaskOfClippedLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  // Clip to the top half of the green layer.
  scoped_refptr<Layer> clip = Layer::Create();
  clip->SetAnchorPoint(gfx::PointF(0.f, 0.f));
  clip->SetPosition(gfx::Point(0, 0));
  clip->SetBounds(gfx::Size(200, 100));
  clip->SetMasksToBounds(true);
  background->AddChild(clip);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(50, 50, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  clip->AddChild(green);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);
  green->SetMaskLayer(mask);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "mask_of_clipped_layer.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, MaskWithReplica) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(0, 0, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);
  green->SetMaskLayer(mask);

  gfx::Transform replica_transform;
  replica_transform.Rotate(-90.0);

  scoped_refptr<Layer> replica = Layer::Create();
  replica->SetAnchorPoint(gfx::PointF(0.5f, 0.5f));
  replica->SetPosition(gfx::Point(100, 100));
  replica->SetTransform(replica_transform);
  green->SetReplicaLayer(replica);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "mask_with_replica.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, MaskWithReplicaOfClippedLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);

  // Clip to the bottom half of the green layer, and the left half of the
  // replica.
  scoped_refptr<Layer> clip = Layer::Create();
  clip->SetAnchorPoint(gfx::PointF(0.f, 0.f));
  clip->SetPosition(gfx::Point(0, 50));
  clip->SetBounds(gfx::Size(150, 150));
  clip->SetMasksToBounds(true);
  background->AddChild(clip);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(0, -50, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  clip->AddChild(green);
  green->SetMaskLayer(mask);

  gfx::Transform replica_transform;
  replica_transform.Rotate(-90.0);

  scoped_refptr<Layer> replica = Layer::Create();
  replica->SetAnchorPoint(gfx::PointF(0.5f, 0.5f));
  replica->SetPosition(gfx::Point(100, 100));
  replica->SetTransform(replica_transform);
  green->SetReplicaLayer(replica);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "mask_with_replica_of_clipped_layer.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, MaskOfReplica) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(50, 0, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> orange = CreateSolidColorLayer(
      gfx::Rect(-50, 50, 50, 50), kCSSOrange);
  green->AddChild(orange);

  gfx::Transform replica_transform;
  replica_transform.Rotate(180.0);
  replica_transform.Translate(100.0, 0.0);

  scoped_refptr<Layer> replica = Layer::Create();
  replica->SetAnchorPoint(gfx::PointF(1.f, 1.f));
  replica->SetPosition(gfx::Point());
  replica->SetTransform(replica_transform);
  replica->SetMaskLayer(mask);
  green->SetReplicaLayer(replica);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "mask_of_replica.png")));
}

TEST_F(LayerTreeHostMasksPixelTest, MaskOfReplicaOfClippedLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  MaskContentLayerClient client;
  scoped_refptr<ContentLayer> mask = ContentLayer::Create(&client);
  mask->SetBounds(gfx::Size(100, 100));
  mask->SetIsDrawable(true);
  mask->SetIsMask(true);

  // Clip to the bottom 3/4 of the green layer, and the top 3/4 of the replica.
  scoped_refptr<Layer> clip = Layer::Create();
  clip->SetAnchorPoint(gfx::PointF(0.f, 0.f));
  clip->SetPosition(gfx::Point(0, 25));
  clip->SetBounds(gfx::Size(200, 150));
  clip->SetMasksToBounds(true);
  background->AddChild(clip);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(50, -25, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  clip->AddChild(green);

  scoped_refptr<SolidColorLayer> orange = CreateSolidColorLayer(
      gfx::Rect(-50, 50, 50, 50), kCSSOrange);
  green->AddChild(orange);

  gfx::Transform replica_transform;
  replica_transform.Rotate(180.0);
  replica_transform.Translate(100.0, 0.0);

  scoped_refptr<Layer> replica = Layer::Create();
  replica->SetAnchorPoint(gfx::PointF(1.f, 1.f));
  replica->SetPosition(gfx::Point());
  replica->SetTransform(replica_transform);
  replica->SetMaskLayer(mask);
  green->SetReplicaLayer(replica);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "mask_of_replica_of_clipped_layer.png")));
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
