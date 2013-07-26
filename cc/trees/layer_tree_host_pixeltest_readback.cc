// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/layers/content_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/paths.h"
#include "cc/test/solid_color_content_layer_client.h"
#include "cc/trees/layer_tree_impl.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostReadbackPixelTest : public LayerTreePixelTest {
 protected:
  virtual scoped_ptr<CopyOutputRequest> CreateCopyOutputRequest() OVERRIDE {
    scoped_ptr<CopyOutputRequest> request;

    switch (test_type_) {
      case GL_WITH_BITMAP:
      case SOFTWARE_WITH_BITMAP:
        request = CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&LayerTreeHostReadbackPixelTest::ReadbackResultAsBitmap,
                       base::Unretained(this)));
        break;
      case SOFTWARE_WITH_DEFAULT:
        request = CopyOutputRequest::CreateRequest(
            base::Bind(&LayerTreeHostReadbackPixelTest::ReadbackResultAsBitmap,
                       base::Unretained(this)));
        break;
      case GL_WITH_DEFAULT:
        request = CopyOutputRequest::CreateRequest(
            base::Bind(&LayerTreeHostReadbackPixelTest::ReadbackResultAsTexture,
                       base::Unretained(this)));
        break;
    }

    if (!copy_subrect_.IsEmpty())
      request->set_area(copy_subrect_);
    return request.Pass();
  }

  void ReadbackResultAsBitmap(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(proxy()->IsMainThread());
    EXPECT_TRUE(result->HasBitmap());
    result_bitmap_ = result->TakeBitmap().Pass();
    EndTest();
  }

  void ReadbackResultAsTexture(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(proxy()->IsMainThread());
    EXPECT_TRUE(result->HasTexture());

    scoped_ptr<TextureMailbox> texture_mailbox = result->TakeTexture().Pass();
    EXPECT_TRUE(texture_mailbox->IsValid());
    EXPECT_TRUE(texture_mailbox->IsTexture());

    scoped_ptr<SkBitmap> bitmap =
        CopyTextureMailboxToBitmap(result->size(), *texture_mailbox);
    texture_mailbox->RunReleaseCallback(0, false);

    ReadbackResultAsBitmap(CopyOutputResult::CreateBitmapResult(bitmap.Pass()));
  }

  gfx::Rect copy_subrect_;
};

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(SOFTWARE_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_Software_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(SOFTWARE_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(GL_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackRootLayerWithChild_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(SOFTWARE_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayerWithChild_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayerWithChild_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(GL_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(GL_WITH_BITMAP,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSmallNonRootLayer_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayer_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(GL_WITH_BITMAP,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayer_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSmallNonRootLayerWithChild_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSmallNonRootLayerWithChild_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(GL_WITH_BITMAP,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayerWithChild_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSubrect_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(50, 50, 100, 100);

  RunPixelTest(SOFTWARE_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSubrect_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(50, 50, 100, 100);

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSubrect_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(50, 50, 100, 100);

  RunPixelTest(GL_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayerSubrect_Software) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(25, 25, 150, 150), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(75, 75, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the green layer.
  copy_subrect_ = gfx::Rect(25, 25, 100, 100);

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayerSubrect_GL_Bitmap) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(25, 25, 150, 150), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(75, 75, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the green layer.
  copy_subrect_ = gfx::Rect(25, 25, 100, 100);

  RunPixelTestWithReadbackTarget(GL_WITH_BITMAP,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayerSubrect_GL) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(25, 25, 150, 150), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(75, 75, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the green layer.
  copy_subrect_ = gfx::Rect(25, 25, 100, 100);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

class LayerTreeHostReadbackDeviceScalePixelTest
    : public LayerTreeHostReadbackPixelTest {
 protected:
  LayerTreeHostReadbackDeviceScalePixelTest()
      : device_scale_factor_(1.f),
        white_client_(SK_ColorWHITE),
        green_client_(SK_ColorGREEN),
        blue_client_(SK_ColorBLUE) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    // Cause the device scale factor to be inherited by contents scales.
    settings->layer_transforms_should_scale_layer_contents = true;
  }

  virtual void SetupTree() OVERRIDE {
    layer_tree_host()->SetDeviceScaleFactor(device_scale_factor_);
    LayerTreePixelTest::SetupTree();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerImpl* root_impl = host_impl->active_tree()->root_layer();

    LayerImpl* background_impl = root_impl->children()[0];
    EXPECT_EQ(device_scale_factor_, background_impl->contents_scale_x());
    EXPECT_EQ(device_scale_factor_, background_impl->contents_scale_y());

    LayerImpl* green_impl = background_impl->children()[0];
    EXPECT_EQ(device_scale_factor_, green_impl->contents_scale_x());
    EXPECT_EQ(device_scale_factor_, green_impl->contents_scale_y());

    LayerImpl* blue_impl = green_impl->children()[0];
    EXPECT_EQ(device_scale_factor_, blue_impl->contents_scale_x());
    EXPECT_EQ(device_scale_factor_, blue_impl->contents_scale_y());
  }

  float device_scale_factor_;
  SolidColorContentLayerClient white_client_;
  SolidColorContentLayerClient green_client_;
  SolidColorContentLayerClient blue_client_;
};

TEST_F(LayerTreeHostReadbackDeviceScalePixelTest,
       ReadbackSubrect_Software) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetAnchorPoint(gfx::PointF());
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetAnchorPoint(gfx::PointF());
  green->SetBounds(gfx::Size(100, 100));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetAnchorPoint(gfx::PointF());
  blue->SetPosition(gfx::Point(50, 50));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(25, 25, 50, 50);
  device_scale_factor_ = 2.f;

  RunPixelTest(SOFTWARE_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackDeviceScalePixelTest,
       ReadbackSubrect_GL) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetAnchorPoint(gfx::PointF());
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetAnchorPoint(gfx::PointF());
  green->SetBounds(gfx::Size(100, 100));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetAnchorPoint(gfx::PointF());
  blue->SetPosition(gfx::Point(50, 50));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(25, 25, 50, 50);
  device_scale_factor_ = 2.f;

  RunPixelTest(GL_WITH_DEFAULT,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackDeviceScalePixelTest,
       ReadbackNonRootLayerSubrect_Software) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetAnchorPoint(gfx::PointF());
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetAnchorPoint(gfx::PointF());
  green->SetPosition(gfx::Point(10, 20));
  green->SetBounds(gfx::Size(90, 80));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetAnchorPoint(gfx::PointF());
  blue->SetPosition(gfx::Point(50, 50));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the green layer's content with blue in the bottom right.
  copy_subrect_ = gfx::Rect(25, 25, 50, 50);
  device_scale_factor_ = 2.f;

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackDeviceScalePixelTest,
       ReadbackNonRootLayerSubrect_GL) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetAnchorPoint(gfx::PointF());
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetAnchorPoint(gfx::PointF());
  green->SetPosition(gfx::Point(10, 20));
  green->SetBounds(gfx::Size(90, 80));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetAnchorPoint(gfx::PointF());
  blue->SetPosition(gfx::Point(50, 50));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the green layer's content with blue in the bottom right.
  copy_subrect_ = gfx::Rect(25, 25, 50, 50);
  device_scale_factor_ = 2.f;

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

class LayerTreeHostReadbackViaCompositeAndReadbackPixelTest
    : public LayerTreePixelTest {
 protected:
  LayerTreeHostReadbackViaCompositeAndReadbackPixelTest()
      : device_scale_factor_(1.f),
        white_client_(SK_ColorWHITE),
        green_client_(SK_ColorGREEN),
        blue_client_(SK_ColorBLUE) {}

  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    // Cause the device scale factor to be inherited by contents scales.
    settings->layer_transforms_should_scale_layer_contents = true;
  }

  virtual void SetupTree() OVERRIDE {
    layer_tree_host()->SetDeviceScaleFactor(device_scale_factor_);
    LayerTreePixelTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    EXPECT_EQ(device_scale_factor_, layer_tree_host()->device_scale_factor());
    if (TestEnded())
      return;

    gfx::Rect device_viewport_copy_rect(
        layer_tree_host()->device_viewport_size());
    if (!device_viewport_copy_subrect_.IsEmpty())
      device_viewport_copy_rect.Intersect(device_viewport_copy_subrect_);

    scoped_ptr<SkBitmap> bitmap(new SkBitmap);
    bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                      device_viewport_copy_rect.width(),
                      device_viewport_copy_rect.height());
    bitmap->allocPixels();
    {
      scoped_ptr<SkAutoLockPixels> lock(new SkAutoLockPixels(*bitmap));
      layer_tree_host()->CompositeAndReadback(bitmap->getPixels(),
                                              device_viewport_copy_rect);
    }

    result_bitmap_ = bitmap.Pass();
    EndTest();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    LayerImpl* root_impl = host_impl->active_tree()->root_layer();

    LayerImpl* background_impl = root_impl->children()[0];
    EXPECT_EQ(device_scale_factor_, background_impl->contents_scale_x());
    EXPECT_EQ(device_scale_factor_, background_impl->contents_scale_y());

    LayerImpl* green_impl = background_impl->children()[0];
    EXPECT_EQ(device_scale_factor_, green_impl->contents_scale_x());
    EXPECT_EQ(device_scale_factor_, green_impl->contents_scale_y());

    LayerImpl* blue_impl = green_impl->children()[0];
    EXPECT_EQ(device_scale_factor_, blue_impl->contents_scale_x());
    EXPECT_EQ(device_scale_factor_, blue_impl->contents_scale_y());
  }

  gfx::Rect device_viewport_copy_subrect_;
  float device_scale_factor_;
  SolidColorContentLayerClient white_client_;
  SolidColorContentLayerClient green_client_;
  SolidColorContentLayerClient blue_client_;
};

TEST_F(LayerTreeHostReadbackViaCompositeAndReadbackPixelTest,
       CompositeAndReadback_Software_1) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetAnchorPoint(gfx::PointF());
  background->SetBounds(gfx::Size(200, 200));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetAnchorPoint(gfx::PointF());
  green->SetBounds(gfx::Size(200, 200));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetAnchorPoint(gfx::PointF());
  blue->SetPosition(gfx::Point(100, 100));
  blue->SetBounds(gfx::Size(50, 50));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the middle of the device viewport.
  device_viewport_copy_subrect_ = gfx::Rect(50, 50, 100, 100);
  device_scale_factor_ = 1.f;

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackViaCompositeAndReadbackPixelTest,
       CompositeAndReadback_Software_2) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetAnchorPoint(gfx::PointF());
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetAnchorPoint(gfx::PointF());
  green->SetBounds(gfx::Size(100, 100));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetAnchorPoint(gfx::PointF());
  blue->SetPosition(gfx::Point(50, 50));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the middle of the device viewport.
  device_viewport_copy_subrect_ = gfx::Rect(50, 50, 100, 100);
  device_scale_factor_ = 2.f;

  RunPixelTestWithReadbackTarget(SOFTWARE_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackViaCompositeAndReadbackPixelTest,
       CompositeAndReadback_GL_1) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetAnchorPoint(gfx::PointF());
  background->SetBounds(gfx::Size(200, 200));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetAnchorPoint(gfx::PointF());
  green->SetBounds(gfx::Size(200, 200));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetAnchorPoint(gfx::PointF());
  blue->SetPosition(gfx::Point(100, 100));
  blue->SetBounds(gfx::Size(50, 50));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the middle of the device viewport.
  device_viewport_copy_subrect_ = gfx::Rect(50, 50, 100, 100);
  device_scale_factor_ = 1.f;

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackViaCompositeAndReadbackPixelTest,
       CompositeAndReadback_GL_2) {
  scoped_refptr<ContentLayer> background = ContentLayer::Create(&white_client_);
  background->SetAnchorPoint(gfx::PointF());
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<ContentLayer> green = ContentLayer::Create(&green_client_);
  green->SetAnchorPoint(gfx::PointF());
  green->SetBounds(gfx::Size(100, 100));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<ContentLayer> blue = ContentLayer::Create(&blue_client_);
  blue->SetAnchorPoint(gfx::PointF());
  blue->SetPosition(gfx::Point(50, 50));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the middle of the device viewport.
  device_viewport_copy_subrect_ = gfx::Rect(50, 50, 100, 100);
  device_scale_factor_ = 2.f;

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayerOutsideViewport) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  // Only the top left quarter of the layer is inside the viewport, so the
  // blue layer is entirely outside.
  green->SetPosition(gfx::Point(100, 100));
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_with_blue_corner.png")));
}

// TextureLayers are clipped differently than SolidColorLayers, verify they
// also can be copied when outside of the viewport.
TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackNonRootTextureLayerOutsideViewport) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 200, 200);
  bitmap.allocPixels();
  bitmap.eraseColor(SK_ColorGREEN);
  {
    SkDevice device(bitmap);
    skia::RefPtr<SkCanvas> canvas = skia::AdoptRef(new SkCanvas(&device));
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SK_ColorBLUE);
    canvas->drawRect(SkRect::MakeXYWH(150, 150, 50, 50), paint);
  }

  scoped_refptr<TextureLayer> texture = CreateTextureLayer(
      gfx::Rect(200, 200), bitmap);

  // Tests with solid color layers verify correctness when CanClipSelf is false.
  EXPECT_FALSE(background->CanClipSelf());
  // This test verifies correctness when CanClipSelf is true.
  EXPECT_TRUE(texture->CanClipSelf());

  // Only the top left quarter of the layer is inside the viewport, so the
  // blue corner is entirely outside.
  texture->SetPosition(gfx::Point(100, 100));
  background->AddChild(texture);

  RunPixelTestWithReadbackTarget(GL_WITH_DEFAULT,
                                 background,
                                 texture.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_with_blue_corner.png")));
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
