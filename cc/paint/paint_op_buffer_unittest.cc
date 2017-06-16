// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/display_item_list.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_skcanvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"

using testing::_;
using testing::Property;
using testing::Mock;

namespace cc {
namespace {
void Playback(PaintOpBuffer* buffer,
              SkCanvas* canvas,
              const std::vector<size_t>& indices) {
  buffer->Playback(canvas, nullptr, &indices);
}
}  // namespace

TEST(PaintOpBufferTest, Empty) {
  PaintOpBuffer buffer;
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);

  buffer.Reset();
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);

  PaintOpBuffer buffer2(std::move(buffer));
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);
  EXPECT_EQ(buffer2.size(), 0u);
  EXPECT_EQ(buffer2.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer2), false);
}

class PaintOpAppendTest : public ::testing::Test {
 public:
  PaintOpAppendTest() {
    rect_ = SkRect::MakeXYWH(2, 3, 4, 5);
    flags_.setColor(SK_ColorMAGENTA);
    flags_.setAlpha(100);
  }

  void PushOps(PaintOpBuffer* buffer) {
    buffer->push<SaveLayerOp>(&rect_, &flags_);
    buffer->push<SaveOp>();
    buffer->push<DrawColorOp>(draw_color_, blend_);
    buffer->push<RestoreOp>();
    EXPECT_EQ(buffer->size(), 4u);
  }

  void VerifyOps(PaintOpBuffer* buffer) {
    EXPECT_EQ(buffer->size(), 4u);

    PaintOpBuffer::Iterator iter(buffer);
    ASSERT_EQ(iter->GetType(), PaintOpType::SaveLayer);
    SaveLayerOp* save_op = static_cast<SaveLayerOp*>(*iter);
    EXPECT_EQ(save_op->bounds, rect_);
    EXPECT_TRUE(save_op->flags == flags_);
    ++iter;

    ASSERT_EQ(iter->GetType(), PaintOpType::Save);
    ++iter;

    ASSERT_EQ(iter->GetType(), PaintOpType::DrawColor);
    DrawColorOp* op = static_cast<DrawColorOp*>(*iter);
    EXPECT_EQ(op->color, draw_color_);
    EXPECT_EQ(op->mode, blend_);
    ++iter;

    ASSERT_EQ(iter->GetType(), PaintOpType::Restore);
    ++iter;

    EXPECT_FALSE(iter);
  }

 private:
  SkRect rect_;
  PaintFlags flags_;
  SkColor draw_color_ = SK_ColorRED;
  SkBlendMode blend_ = SkBlendMode::kSrc;
};

TEST_F(PaintOpAppendTest, SimpleAppend) {
  PaintOpBuffer buffer;
  PushOps(&buffer);
  VerifyOps(&buffer);

  buffer.Reset();
  PushOps(&buffer);
  VerifyOps(&buffer);
}

TEST_F(PaintOpAppendTest, MoveThenDestruct) {
  PaintOpBuffer original;
  PushOps(&original);
  VerifyOps(&original);

  PaintOpBuffer destination(std::move(original));
  VerifyOps(&destination);

  // Original should be empty, and safe to destruct.
  EXPECT_EQ(original.size(), 0u);
  EXPECT_EQ(original.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&original), false);
}

TEST_F(PaintOpAppendTest, MoveThenDestructOperatorEq) {
  PaintOpBuffer original;
  PushOps(&original);
  VerifyOps(&original);

  PaintOpBuffer destination;
  destination = std::move(original);
  VerifyOps(&destination);

  // Original should be empty, and safe to destruct.
  EXPECT_EQ(original.size(), 0u);
  EXPECT_EQ(original.bytes_used(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&original), false);
}

TEST_F(PaintOpAppendTest, MoveThenReappend) {
  PaintOpBuffer original;
  PushOps(&original);

  PaintOpBuffer destination(std::move(original));

  // Should be possible to reappend to the original and get the same result.
  PushOps(&original);
  VerifyOps(&original);
}

TEST_F(PaintOpAppendTest, MoveThenReappendOperatorEq) {
  PaintOpBuffer original;
  PushOps(&original);

  PaintOpBuffer destination;
  destination = std::move(original);

  // Should be possible to reappend to the original and get the same result.
  PushOps(&original);
  VerifyOps(&original);
}

// Verify that PaintOps with data are stored properly.
TEST(PaintOpBufferTest, PaintOpData) {
  PaintOpBuffer buffer;

  buffer.push<SaveOp>();
  PaintFlags flags;
  char text1[] = "asdfasdf";
  buffer.push_with_data<DrawTextOp>(text1, arraysize(text1), 0.f, 0.f, flags);

  char text2[] = "qwerty";
  buffer.push_with_data<DrawTextOp>(text2, arraysize(text2), 0.f, 0.f, flags);

  ASSERT_EQ(buffer.size(), 3u);

  // Verify iteration behavior and brief smoke test of op state.
  PaintOpBuffer::Iterator iter(&buffer);
  PaintOp* save_op = *iter;
  EXPECT_EQ(save_op->GetType(), PaintOpType::Save);
  ++iter;

  PaintOp* op1 = *iter;
  ASSERT_EQ(op1->GetType(), PaintOpType::DrawText);
  DrawTextOp* draw_text_op1 = static_cast<DrawTextOp*>(op1);
  EXPECT_EQ(draw_text_op1->bytes, arraysize(text1));
  const void* data1 = draw_text_op1->GetData();
  EXPECT_EQ(memcmp(data1, text1, arraysize(text1)), 0);
  ++iter;

  PaintOp* op2 = *iter;
  ASSERT_EQ(op2->GetType(), PaintOpType::DrawText);
  DrawTextOp* draw_text_op2 = static_cast<DrawTextOp*>(op2);
  EXPECT_EQ(draw_text_op2->bytes, arraysize(text2));
  const void* data2 = draw_text_op2->GetData();
  EXPECT_EQ(memcmp(data2, text2, arraysize(text2)), 0);
  ++iter;

  EXPECT_FALSE(iter);
}

// Verify that PaintOps with arrays are stored properly.
TEST(PaintOpBufferTest, PaintOpArray) {
  PaintOpBuffer buffer;
  buffer.push<SaveOp>();

  // arbitrary data
  std::string texts[] = {"xyz", "abcdefg", "thingerdoo"};
  SkPoint point1[] = {SkPoint::Make(1, 2), SkPoint::Make(2, 3),
                      SkPoint::Make(3, 4)};
  SkPoint point2[] = {SkPoint::Make(8, -12)};
  SkPoint point3[] = {SkPoint::Make(0, 0),   SkPoint::Make(5, 6),
                      SkPoint::Make(-1, -1), SkPoint::Make(9, 9),
                      SkPoint::Make(50, 50), SkPoint::Make(100, 100)};
  SkPoint* points[] = {point1, point2, point3};
  size_t counts[] = {arraysize(point1), arraysize(point2), arraysize(point3)};

  for (size_t i = 0; i < arraysize(texts); ++i) {
    PaintFlags flags;
    flags.setAlpha(i);
    buffer.push_with_array<DrawPosTextOp>(texts[i].c_str(), texts[i].length(),
                                          points[i], counts[i], flags);
  }

  PaintOpBuffer::Iterator iter(&buffer);
  PaintOp* save_op = *iter;
  EXPECT_EQ(save_op->GetType(), PaintOpType::Save);
  ++iter;

  for (size_t i = 0; i < arraysize(texts); ++i) {
    ASSERT_EQ(iter->GetType(), PaintOpType::DrawPosText);
    DrawPosTextOp* op = static_cast<DrawPosTextOp*>(*iter);

    EXPECT_EQ(op->flags.getAlpha(), i);

    EXPECT_EQ(op->bytes, texts[i].length());
    const void* data = op->GetData();
    EXPECT_EQ(memcmp(data, texts[i].c_str(), op->bytes), 0);

    EXPECT_EQ(op->count, counts[i]);
    const SkPoint* op_points = op->GetArray();
    for (size_t k = 0; k < op->count; ++k)
      EXPECT_EQ(op_points[k], points[i][k]);

    ++iter;
  }

  EXPECT_FALSE(iter);
}

// Verify that a SaveLayerAlpha / Draw / Restore can be optimized to just
// a draw with opacity.
TEST(PaintOpBufferTest, SaveDrawRestore) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(0, canvas.save_count_);
  EXPECT_EQ(0, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);

  // Expect the alpha from the draw and the save layer to be folded together.
  // Since alpha is stored in a uint8_t and gets rounded, so use tolerance.
  float expected_alpha = alpha * 50 / 255.f;
  EXPECT_LE(std::abs(expected_alpha - canvas.paint_.getAlpha()), 1.f);
}

// The same as SaveDrawRestore, but test that the optimization doesn't apply
// when the drawing op's flags are not compatible with being folded into the
// save layer with opacity.
TEST(PaintOpBufferTest, SaveDrawRestoreFail_BadFlags) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  draw_flags.setBlendMode(SkBlendMode::kSrc);
  EXPECT_FALSE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);
  EXPECT_EQ(draw_flags.getAlpha(), canvas.paint_.getAlpha());
}

// The same as SaveDrawRestore, but test that the optimization doesn't apply
// when there are more than one ops between the save and restore.
TEST(PaintOpBufferTest, SaveDrawRestoreFail_TooManyOps) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  draw_flags.setBlendMode(SkBlendMode::kSrcOver);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<NoopOp>();
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);
  EXPECT_EQ(draw_flags.getAlpha(), canvas.paint_.getAlpha());
}

// Verify that the save draw restore code works with a single op
// that's not a draw op, and the optimization does not kick in.
TEST(PaintOpBufferTest, SaveDrawRestore_SingleOpNotADrawOp) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);

  buffer.push<NoopOp>();
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
}

// Test that the save/draw/restore optimization applies if the single op
// is a DrawRecord that itself has a single draw op.
TEST(PaintOpBufferTest, SaveDrawRestore_SingleOpRecordWithSingleOp) {
  sk_sp<PaintRecord> record = sk_make_sp<PaintRecord>();

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  record->push<DrawRectOp>(rect, draw_flags);
  EXPECT_EQ(record->size(), 1u);

  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);
  buffer.push<DrawRecordOp>(std::move(record));
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(0, canvas.save_count_);
  EXPECT_EQ(0, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);

  float expected_alpha = alpha * 50 / 255.f;
  EXPECT_LE(std::abs(expected_alpha - canvas.paint_.getAlpha()), 1.f);
}

// The same as the above SingleOpRecord test, but the single op is not
// a draw op.  So, there's no way to fold in the save layer optimization.
// Verify that the optimization doesn't apply and that this doesn't crash.
// See: http://crbug.com/712093.
TEST(PaintOpBufferTest, SaveDrawRestore_SingleOpRecordWithSingleNonDrawOp) {
  sk_sp<PaintRecord> record = sk_make_sp<PaintRecord>();
  record->push<NoopOp>();
  EXPECT_EQ(record->size(), 1u);
  EXPECT_FALSE(record->GetFirstOp()->IsDrawOp());

  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, false);
  buffer.push<DrawRecordOp>(std::move(record));
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.Playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_EmptyBuffer) {
  PaintOpBuffer buffer;
  EXPECT_FALSE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_NoImageOp) {
  PaintOpBuffer buffer;
  PaintFlags flags;
  buffer.push<DrawRectOp>(SkRect::MakeWH(100, 100), flags);
  EXPECT_FALSE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_DrawImage) {
  PaintOpBuffer buffer;
  PaintImage image = PaintImage(PaintImage::GetNextId(),
                                CreateDiscardableImage(gfx::Size(100, 100)));
  buffer.push<DrawImageOp>(image, SkIntToScalar(0), SkIntToScalar(0), nullptr);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_DrawImageRect) {
  PaintOpBuffer buffer;
  PaintImage image = PaintImage(PaintImage::GetNextId(),
                                CreateDiscardableImage(gfx::Size(100, 100)));
  buffer.push<DrawImageRectOp>(
      image, SkRect::MakeWH(100, 100), SkRect::MakeWH(100, 100), nullptr,
      PaintCanvas::SrcRectConstraint::kFast_SrcRectConstraint);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, DiscardableImagesTracking_OpWithFlags) {
  PaintOpBuffer buffer;
  PaintFlags flags;
  sk_sp<SkImage> image = CreateDiscardableImage(gfx::Size(100, 100));
  flags.setShader(PaintShader::MakeImage(std::move(image),
                                         SkShader::kClamp_TileMode,
                                         SkShader::kClamp_TileMode, nullptr));
  buffer.push<DrawRectOp>(SkRect::MakeWH(100, 100), flags);
  EXPECT_TRUE(buffer.HasDiscardableImages());
}

TEST(PaintOpBufferTest, SlowPaths) {
  auto buffer = sk_make_sp<PaintOpBuffer>();
  EXPECT_EQ(buffer->numSlowPaths(), 0);

  // Op without slow paths
  PaintFlags noop_flags;
  SkRect rect = SkRect::MakeXYWH(2, 3, 4, 5);
  buffer->push<SaveLayerOp>(&rect, &noop_flags);

  // Line op with a slow path
  PaintFlags line_effect_slow;
  line_effect_slow.setStrokeWidth(1.f);
  line_effect_slow.setStyle(PaintFlags::kStroke_Style);
  line_effect_slow.setStrokeCap(PaintFlags::kRound_Cap);
  SkScalar intervals[] = {1.f, 1.f};
  line_effect_slow.setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));

  buffer->push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect_slow);
  EXPECT_EQ(buffer->numSlowPaths(), 1);

  // Line effect special case that Skia handles specially.
  PaintFlags line_effect = line_effect_slow;
  line_effect.setStrokeCap(PaintFlags::kButt_Cap);
  buffer->push<DrawLineOp>(1.f, 2.f, 3.f, 4.f, line_effect);
  EXPECT_EQ(buffer->numSlowPaths(), 1);

  // Antialiased convex path is not slow.
  SkPath path;
  path.addCircle(2, 2, 5);
  EXPECT_TRUE(path.isConvex());
  buffer->push<ClipPathOp>(path, SkClipOp::kIntersect, true);
  EXPECT_EQ(buffer->numSlowPaths(), 1);

  // Concave paths are slow only when antialiased.
  SkPath concave = path;
  concave.addCircle(3, 4, 2);
  EXPECT_FALSE(concave.isConvex());
  buffer->push<ClipPathOp>(concave, SkClipOp::kIntersect, true);
  EXPECT_EQ(buffer->numSlowPaths(), 2);
  buffer->push<ClipPathOp>(concave, SkClipOp::kIntersect, false);
  EXPECT_EQ(buffer->numSlowPaths(), 2);

  // Drawing a record with slow paths into another adds the same
  // number of slow paths as the record.
  auto buffer2 = sk_make_sp<PaintOpBuffer>();
  EXPECT_EQ(0, buffer2->numSlowPaths());
  buffer2->push<DrawRecordOp>(buffer);
  EXPECT_EQ(2, buffer2->numSlowPaths());
  buffer2->push<DrawRecordOp>(buffer);
  EXPECT_EQ(4, buffer2->numSlowPaths());
}

TEST(PaintOpBufferTest, ContiguousIndices) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  buffer.push<DrawColorOp>(0u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(1u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(2u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(3u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays all items.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&buffer, &canvas, {0, 1, 2, 3, 4});
}

TEST(PaintOpBufferTest, NonContiguousIndices) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  buffer.push<DrawColorOp>(0u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(1u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(2u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(3u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays 0, 1, 3, 4 indices.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&buffer, &canvas, {0, 1, 3, 4});
}

TEST(PaintOpBufferTest, FirstTwoIndices) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  buffer.push<DrawColorOp>(0u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(1u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(2u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(3u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays first two indices.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  Playback(&buffer, &canvas, {0, 1});
}

TEST(PaintOpBufferTest, MiddleIndex) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  buffer.push<DrawColorOp>(0u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(1u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(2u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(3u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays index 2.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
  Playback(&buffer, &canvas, {2});
}

TEST(PaintOpBufferTest, LastTwoElements) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  buffer.push<DrawColorOp>(0u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(1u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(2u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(3u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(4u, SkBlendMode::kClear);

  // Plays last two elements.
  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&buffer, &canvas, {3, 4});
}

TEST(PaintOpBufferTest, ContiguousIndicesWithSaveLayerAlphaRestore) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  buffer.push<DrawColorOp>(0u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(1u, SkBlendMode::kClear);
  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, true);
  buffer.push<RestoreOp>();
  buffer.push<DrawColorOp>(2u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(3u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(4u, SkBlendMode::kClear);

  // Items are {0, 1, save, restore, 2, 3, 4}.

  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
  // The empty SaveLayerAlpha/Restore is dropped.
  EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
  Playback(&buffer, &canvas, {0, 1, 2, 3, 4, 5, 6});
  Mock::VerifyAndClearExpectations(&canvas);
}

TEST(PaintOpBufferTest, NonContiguousIndicesWithSaveLayerAlphaRestore) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  buffer.push<DrawColorOp>(0u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(1u, SkBlendMode::kClear);
  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, true);
  buffer.push<DrawColorOp>(2u, SkBlendMode::kClear);
  buffer.push<DrawColorOp>(3u, SkBlendMode::kClear);
  buffer.push<RestoreOp>();
  buffer.push<DrawColorOp>(4u, SkBlendMode::kClear);

  // Items are {0, 1, save, 2, 3, restore, 4}.

  // Plays back all indices.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
    // The SaveLayerAlpha/Restore is not dropped if we draw the middle
    // range, as we need them to represent the two draws inside the layer
    // correctly.
    EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(2u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(3u)).InSequence(s);
    EXPECT_CALL(canvas, willRestore()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
    Playback(&buffer, &canvas, {0, 1, 2, 3, 4, 5, 6});
  }
  Mock::VerifyAndClearExpectations(&canvas);

  // Skips the middle indices.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawPaintWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawPaintWithColor(1u)).InSequence(s);
    // The now-empty SaveLayerAlpha/Restore is dropped
    EXPECT_CALL(canvas, OnDrawPaintWithColor(4u)).InSequence(s);
    Playback(&buffer, &canvas, {0, 1, 2, 5, 6});
  }
  Mock::VerifyAndClearExpectations(&canvas);
}

TEST(PaintOpBufferTest, ContiguousIndicesWithSaveLayerAlphaDrawRestore) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  auto add_draw_rect = [](PaintOpBuffer* buffer, SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    buffer->push<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  add_draw_rect(&buffer, 0u);
  add_draw_rect(&buffer, 1u);
  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, true);
  add_draw_rect(&buffer, 2u);
  buffer.push<RestoreOp>();
  add_draw_rect(&buffer, 3u);
  add_draw_rect(&buffer, 4u);

  // Items are {0, 1, save, 2, restore, 3, 4}.

  testing::Sequence s;
  EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
  // The empty SaveLayerAlpha/Restore is dropped, the containing
  // operation can be drawn with alpha.
  EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(3u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(4u)).InSequence(s);
  Playback(&buffer, &canvas, {0, 1, 2, 3, 4, 5, 6});
  Mock::VerifyAndClearExpectations(&canvas);
}

TEST(PaintOpBufferTest, NonContiguousIndicesWithSaveLayerAlphaDrawRestore) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  auto add_draw_rect = [](PaintOpBuffer* buffer, SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    buffer->push<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  add_draw_rect(&buffer, 0u);
  add_draw_rect(&buffer, 1u);
  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, true);
  add_draw_rect(&buffer, 2u);
  add_draw_rect(&buffer, 3u);
  add_draw_rect(&buffer, 4u);
  buffer.push<RestoreOp>();

  // Items are are {0, 1, save, 2, 3, 4, restore}.

  // If the middle range is played, then the SaveLayerAlpha/Restore
  // can't be dropped.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(3u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(4u)).InSequence(s);
    EXPECT_CALL(canvas, willRestore()).InSequence(s);
    Playback(&buffer, &canvas, {0, 1, 2, 3, 4, 5, 6});
  }
  Mock::VerifyAndClearExpectations(&canvas);

  // If the middle range is not played, then the SaveLayerAlpha/Restore
  // can be dropped.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(4u)).InSequence(s);
    Playback(&buffer, &canvas, {0, 1, 2, 5, 6});
  }
  Mock::VerifyAndClearExpectations(&canvas);

  // If the middle range is not played, then the SaveLayerAlpha/Restore
  // can be dropped.
  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
    Playback(&buffer, &canvas, {0, 1, 2, 3, 6});
  }
}

TEST(PaintOpBufferTest, SaveLayerAlphaDrawRestoreWithBadBlendMode) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  auto add_draw_rect = [](PaintOpBuffer* buffer, SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    // This blend mode prevents the optimization.
    flags.setBlendMode(SkBlendMode::kSrc);
    buffer->push<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  add_draw_rect(&buffer, 0u);
  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, true);
  add_draw_rect(&buffer, 1u);
  buffer.push<RestoreOp>();
  add_draw_rect(&buffer, 2u);

  {
    testing::Sequence s;
    EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
    EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
    EXPECT_CALL(canvas, willRestore()).InSequence(s);
    EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
    buffer.Playback(&canvas);
  }
}

TEST(PaintOpBufferTest, UnmatchedSaveRestoreNoSideEffects) {
  PaintOpBuffer buffer;
  MockCanvas canvas;

  auto add_draw_rect = [](PaintOpBuffer* buffer, SkColor c) {
    PaintFlags flags;
    flags.setColor(c);
    buffer->push<DrawRectOp>(SkRect::MakeWH(1, 1), flags);
  };

  // Push 2 saves.

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, true);
  add_draw_rect(&buffer, 0u);
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha, true);
  add_draw_rect(&buffer, 1u);
  add_draw_rect(&buffer, 2u);
  // But only 1 restore.
  buffer.push<RestoreOp>();

  testing::Sequence s;
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(0u)).InSequence(s);
  EXPECT_CALL(canvas, OnSaveLayer()).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(1u)).InSequence(s);
  EXPECT_CALL(canvas, OnDrawRectWithColor(2u)).InSequence(s);
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  // We will restore back to the original save count regardless with 2 restores.
  EXPECT_CALL(canvas, willRestore()).InSequence(s);
  buffer.Playback(&canvas);
}

}  // namespace cc
