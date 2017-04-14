// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"
#include "cc/test/test_skcanvas.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

template <typename T>
void CheckRefCnt(const T& obj, int32_t count) {
// Skia doesn't define getRefCnt in all builds.
#ifdef SK_DEBUG
  EXPECT_EQ(obj->getRefCnt(), count);
#endif
}

}  // namespace

namespace cc {

TEST(PaintOpBufferTest, Empty) {
  PaintOpBuffer buffer;
  EXPECT_EQ(buffer.approximateOpCount(), 0);
  EXPECT_EQ(buffer.approximateBytesUsed(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);

  buffer.Reset();
  EXPECT_EQ(buffer.approximateOpCount(), 0);
  EXPECT_EQ(buffer.approximateBytesUsed(), sizeof(PaintOpBuffer));
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);
}

TEST(PaintOpBufferTest, SimpleAppend) {
  SkRect rect = SkRect::MakeXYWH(2, 3, 4, 5);
  PaintFlags flags;
  flags.setColor(SK_ColorMAGENTA);
  flags.setAlpha(100);
  SkColor draw_color = SK_ColorRED;
  SkBlendMode blend = SkBlendMode::kSrc;

  PaintOpBuffer buffer;
  buffer.push<SaveLayerOp>(&rect, &flags);
  buffer.push<SaveOp>();
  buffer.push<DrawColorOp>(draw_color, blend);
  buffer.push<RestoreOp>();

  EXPECT_EQ(buffer.approximateOpCount(), 4);

  PaintOpBuffer::Iterator iter(&buffer);
  ASSERT_EQ(iter->GetType(), PaintOpType::SaveLayer);
  SaveLayerOp* save_op = static_cast<SaveLayerOp*>(*iter);
  EXPECT_EQ(save_op->bounds, rect);
  EXPECT_TRUE(save_op->flags == flags);
  ++iter;

  ASSERT_EQ(iter->GetType(), PaintOpType::Save);
  ++iter;

  ASSERT_EQ(iter->GetType(), PaintOpType::DrawColor);
  DrawColorOp* op = static_cast<DrawColorOp*>(*iter);
  EXPECT_EQ(op->color, draw_color);
  EXPECT_EQ(op->mode, blend);
  ++iter;

  ASSERT_EQ(iter->GetType(), PaintOpType::Restore);
  ++iter;

  EXPECT_FALSE(iter);
}

// PaintOpBuffer has a special case for first ops stored locally, so
// make sure that appending different kind of ops as a first op works
// properly, as well as resetting and reusing the first local op.
TEST(PaintOpBufferTest, FirstOpWithAndWithoutData) {
  PaintOpBuffer buffer;
  char text[] = "asdf";

  // Use a color filter and its ref count to verify that the destructor
  // is called on ops after reset.
  PaintFlags flags;
  sk_sp<SkColorFilter> filter =
      SkColorFilter::MakeModeFilter(SK_ColorMAGENTA, SkBlendMode::kSrcOver);
  flags.setColorFilter(filter);
  CheckRefCnt(filter, 2);

  buffer.push_with_data<DrawTextOp>(text, arraysize(text), 0.f, 0.f, flags);
  CheckRefCnt(filter, 3);

  // Verify that when the first op has data, which may not fit in the
  // PaintRecord internal buffer, that it adds a noop as the first op
  // and then appends the "op with data" into the heap buffer.
  ASSERT_EQ(buffer.approximateOpCount(), 2);
  EXPECT_EQ(buffer.GetFirstOp()->GetType(), PaintOpType::Noop);

  // Verify iteration behavior and brief smoke test of op state.
  {
    PaintOpBuffer::Iterator iter(&buffer);
    PaintOp* noop = *iter;
    EXPECT_EQ(buffer.GetFirstOp(), noop);
    ++iter;

    PaintOp* op = *iter;
    ASSERT_EQ(op->GetType(), PaintOpType::DrawText);
    DrawTextOp* draw_text_op = static_cast<DrawTextOp*>(op);
    EXPECT_EQ(draw_text_op->bytes, arraysize(text));

    void* data = paint_op_data(draw_text_op);
    EXPECT_EQ(memcmp(data, text, arraysize(text)), 0);

    ++iter;
    EXPECT_FALSE(iter);
  }

  // Reset, verify state, and append an op that will fit in the first slot.
  buffer.Reset();
  CheckRefCnt(filter, 2);

  ASSERT_EQ(buffer.approximateOpCount(), 0);
  EXPECT_EQ(PaintOpBuffer::Iterator(&buffer), false);

  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, flags);
  CheckRefCnt(filter, 3);

  ASSERT_EQ(buffer.approximateOpCount(), 1);
  EXPECT_EQ(buffer.GetFirstOp()->GetType(), PaintOpType::DrawRect);

  PaintOpBuffer::Iterator iter(&buffer);
  ASSERT_EQ(iter->GetType(), PaintOpType::DrawRect);
  DrawRectOp* draw_rect_op = static_cast<DrawRectOp*>(*iter);
  EXPECT_EQ(draw_rect_op->rect, rect);

  ++iter;
  EXPECT_FALSE(iter);

  buffer.Reset();
  ASSERT_EQ(buffer.approximateOpCount(), 0);
  CheckRefCnt(filter, 2);
}

TEST(PaintOpBufferTest, Peek) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);
  PaintFlags draw_flags;
  buffer.push<DrawRectOp>(SkRect::MakeXYWH(1, 2, 3, 4), draw_flags);
  buffer.push<RestoreOp>();
  buffer.push<SaveOp>();
  buffer.push<NoopOp>();
  buffer.push<RestoreOp>();

  PaintOpBuffer::Iterator init_iter(&buffer);
  PaintOp* peek[2] = {*init_iter, init_iter.peek1()};

  // Expect that while iterating that next = current.peek1() and that
  // next.peek1() == current.peek2().
  for (PaintOpBuffer::Iterator iter(&buffer); iter; ++iter) {
    EXPECT_EQ(*iter, peek[0]) << iter.op_idx();
    EXPECT_EQ(iter.peek1(), peek[1]) << iter.op_idx();

    peek[0] = iter.peek1();
    peek[1] = iter.peek2();
  }
}

TEST(PaintOpBufferTest, PeekEmpty) {
  PaintOpBuffer empty;
  PaintOpBuffer::Iterator empty_iter(&empty);
  EXPECT_EQ(nullptr, empty_iter.peek1());
  EXPECT_EQ(nullptr, empty_iter.peek2());
}

// Verify that a SaveLayerAlpha / Draw / Restore can be optimized to just
// a draw with opacity.
TEST(PaintOpBufferTest, SaveDrawRestore) {
  PaintOpBuffer buffer;

  uint8_t alpha = 100;
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  EXPECT_TRUE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.playback(&canvas);

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
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

  PaintFlags draw_flags;
  draw_flags.setColor(SK_ColorMAGENTA);
  draw_flags.setAlpha(50);
  draw_flags.setBlendMode(SkBlendMode::kSrc);
  EXPECT_FALSE(draw_flags.SupportsFoldingAlpha());
  SkRect rect = SkRect::MakeXYWH(1, 2, 3, 4);
  buffer.push<DrawRectOp>(rect, draw_flags);
  buffer.push<RestoreOp>();

  SaveCountingCanvas canvas;
  buffer.playback(&canvas);

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
  buffer.push<SaveLayerAlphaOp>(nullptr, alpha);

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
  buffer.playback(&canvas);

  EXPECT_EQ(1, canvas.save_count_);
  EXPECT_EQ(1, canvas.restore_count_);
  EXPECT_EQ(rect, canvas.draw_rect_);
  EXPECT_EQ(draw_flags.getAlpha(), canvas.paint_.getAlpha());
}

}  // namespace cc
