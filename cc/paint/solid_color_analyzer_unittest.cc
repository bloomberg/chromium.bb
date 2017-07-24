// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/solid_color_analyzer.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/record_paint_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkOffsetImageFilter.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

class SolidColorAnalyzerTest : public testing::Test {
 public:
  void SetUp() override {
    display_item_list_ = base::MakeRefCounted<DisplayItemList>(
        DisplayItemList::kToBeReleasedAsPaintOpBuffer);
    display_item_list_->StartPaint();
  }

  void TearDown() override {
    Finalize();
    canvas_.reset();
    display_item_list_ = nullptr;
    buffer_ = nullptr;
  }

  void Initialize(const gfx::Rect& rect = gfx::Rect(0, 0, 100, 100)) {
    canvas_.emplace(display_item_list_.get(), gfx::RectToSkRect(rect));
    rect_ = rect;
  }
  RecordPaintCanvas* canvas() { return &*canvas_; }

  bool IsSolidColor() {
    Finalize();
    auto color = SolidColorAnalyzer::DetermineIfSolidColor(buffer_.get(), rect_,
                                                           1, nullptr);
    return !!color;
  }

  SkColor GetColor() {
    Finalize();
    auto color = SolidColorAnalyzer::DetermineIfSolidColor(buffer_.get(), rect_,
                                                           1, nullptr);
    EXPECT_TRUE(color);
    return color ? *color : SK_ColorTRANSPARENT;
  }

 private:
  void Finalize() {
    if (buffer_)
      return;
    display_item_list_->EndPaintOfUnpaired(gfx::Rect());
    display_item_list_->Finalize();
    buffer_ = display_item_list_->ReleaseAsRecord();
  }

  gfx::Rect rect_;
  scoped_refptr<DisplayItemList> display_item_list_;
  sk_sp<PaintOpBuffer> buffer_;
  base::Optional<RecordPaintCanvas> canvas_;
  base::Optional<SolidColorAnalyzer> analyzer_;
};

TEST_F(SolidColorAnalyzerTest, Empty) {
  Initialize();
  EXPECT_EQ(SK_ColorTRANSPARENT, GetColor());
}

TEST_F(SolidColorAnalyzerTest, ClearTransparent) {
  Initialize();
  SkColor color = SkColorSetARGB(0, 12, 34, 56);
  canvas()->clear(color);
  EXPECT_EQ(SK_ColorTRANSPARENT, GetColor());
}

TEST_F(SolidColorAnalyzerTest, ClearSolid) {
  Initialize();
  SkColor color = SkColorSetARGB(255, 65, 43, 21);
  canvas()->clear(color);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, ClearTranslucent) {
  Initialize();
  SkColor color = SkColorSetARGB(128, 11, 22, 33);
  canvas()->clear(color);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawColor) {
  Initialize();
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  canvas()->drawColor(color);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawOval) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  canvas()->drawOval(SkRect::MakeWH(100, 100), flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawBitmap) {
  Initialize();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  canvas()->drawBitmap(bitmap, 0, 0, nullptr);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRect) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->clipRect(rect, SkClipOp::kIntersect, false);
  canvas()->drawRect(rect, flags);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectClipped) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->clipRect(SkRect::MakeWH(50, 50), SkClipOp::kIntersect, false);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectWithTranslateNotSolid) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 100);
  canvas()->translate(1, 1);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectWithTranslateSolid) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(101, 101);
  canvas()->translate(1, 1);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, TwoOpsNotSolid) {
  Initialize();
  SkColor color = SkColorSetARGB(255, 65, 43, 21);
  canvas()->clear(color);
  canvas()->clear(color);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectBlendModeClear) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  flags.setBlendMode(SkBlendMode::kClear);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->drawRect(rect, flags);
  EXPECT_EQ(SK_ColorTRANSPARENT, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectBlendModeSrcOver) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  flags.setBlendMode(SkBlendMode::kSrcOver);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->drawRect(rect, flags);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectRotated) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->rotate(50);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectScaledNotSolid) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->scale(0.1f, 0.1f);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectScaledSolid) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(10, 10);
  canvas()->scale(10, 10);
  canvas()->drawRect(rect, flags);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectFilterPaint) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);
  flags.setImageFilter(SkOffsetImageFilter::Make(10, 10, nullptr));
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectClipPath) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);

  SkPath path;
  path.moveTo(0, 0);
  path.lineTo(128, 50);
  path.lineTo(255, 0);
  path.lineTo(255, 255);
  path.lineTo(0, 255);

  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->clipPath(path, SkClipOp::kIntersect);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, SaveLayer) {
  Initialize();
  PaintFlags flags;
  SkColor color = SkColorSetARGB(255, 11, 22, 33);
  flags.setColor(color);

  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->saveLayer(&rect, &flags);
  EXPECT_FALSE(IsSolidColor());
}

}  // namespace
}  // namespace cc
