// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/paths.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostReadbackPixelTest : public LayerTreePixelTest {
 protected:
  LayerTreeHostReadbackPixelTest() : force_readback_as_bitmap_(false) {}

  virtual scoped_ptr<CopyOutputRequest> CreateCopyOutputRequest() OVERRIDE {
    if (force_readback_as_bitmap_) {
      return CopyOutputRequest::CreateBitmapRequest(
          base::Bind(&LayerTreeHostReadbackPixelTest::ReadbackResultAsBitmap,
                     base::Unretained(this)));
    }

    if (!use_gl_) {
      return CopyOutputRequest::CreateRequest(
          base::Bind(&LayerTreeHostReadbackPixelTest::ReadbackResultAsBitmap,
                     base::Unretained(this)));
    }

    return CopyOutputRequest::CreateRequest(
        base::Bind(&LayerTreeHostReadbackPixelTest::ReadbackResultAsTexture,
                   base::Unretained(this)));
  }

  void ReadbackResultAsBitmap(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(result->HasBitmap());
    result_bitmap_ = result->TakeBitmap().Pass();
    EndTest();
  }

  void ReadbackResultAsTexture(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(result->HasTexture());

    scoped_ptr<TextureMailbox> texture_mailbox = result->TakeTexture().Pass();
    EXPECT_TRUE(texture_mailbox->IsValid());
    EXPECT_TRUE(texture_mailbox->IsTexture());

    scoped_ptr<SkBitmap> bitmap =
        CopyTextureMailboxToBitmap(result->size(), *texture_mailbox);
    ReadbackResultAsBitmap(CopyOutputResult::CreateBitmapResult(bitmap.Pass()));

    texture_mailbox->RunReleaseCallback(0, false);
  }

  bool force_readback_as_bitmap_;
};

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_Software) {
  use_gl_ = false;
  force_readback_as_bitmap_ = false;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_Software_Bitmap) {
  use_gl_ = false;
  force_readback_as_bitmap_ = true;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_GL_Bitmap) {
  use_gl_ = true;
  force_readback_as_bitmap_ = true;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer_GL) {
  use_gl_ = true;
  force_readback_as_bitmap_ = false;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackRootLayerWithChild_Software) {
  use_gl_ = false;
  force_readback_as_bitmap_ = false;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayerWithChild_GL_Bitmap) {
  use_gl_ = true;
  force_readback_as_bitmap_ = true;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayerWithChild_GL) {
  use_gl_ = true;
  force_readback_as_bitmap_ = false;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer_Software) {
  use_gl_ = false;
  force_readback_as_bitmap_ = false;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer_GL_Bitmap) {
  use_gl_ = true;
  force_readback_as_bitmap_ = true;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer_GL) {
  use_gl_ = true;
  force_readback_as_bitmap_ = false;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSmallNonRootLayer_Software) {
  use_gl_ = false;
  force_readback_as_bitmap_ = false;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayer_GL_Bitmap) {
  use_gl_ = true;
  force_readback_as_bitmap_ = true;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayer_GL) {
  use_gl_ = true;
  force_readback_as_bitmap_ = false;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSmallNonRootLayerWithChild_Software) {
  use_gl_ = false;
  force_readback_as_bitmap_ = false;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest,
       ReadbackSmallNonRootLayerWithChild_GL_Bitmap) {
  use_gl_ = true;
  force_readback_as_bitmap_ = true;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayerWithChild_GL) {
  use_gl_ = true;
  force_readback_as_bitmap_ = false;

  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
